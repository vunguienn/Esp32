/**
 * ESP32-S3 IoT Gateway Main Firmware
 * Cloud Grow System
 * 
 * Features:
 * - WiFi AP mode for provisioning (if not configured)
 * - MQTT communication with Cloud server
 * - Sensor data collection (RS485 or simulation)
 * - 8-channel relay control
 * - Device verification/pairing with Cloud
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#include "config.h"
#include "wifi_manager.h"
#include "mqtt_handler.h"
#include "sensor_manager.h"
#include "relay_controller.h"
#include "automation_sync.h"
#include "web_monitor.h"
#include "system_watchdog.h"

// ============================================================================
// Global Objects
// ============================================================================
GrowWiFiManager wifiManager;
SensorManager sensors;
RelayController relays;
WiFiClient wifiClient;
Preferences prefs;

// ============================================================================
// State Variables
// ============================================================================
bool systemReady = false;
unsigned long lastSensorRead = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastStatusUpdate = 0;
unsigned long lastWifiCheck = 0;
unsigned long lastAutomationCheck = 0;

// Cached sensor readings (avoid double read in same loop)
SensorReadings cachedReadings;
bool sensorReadThisLoop = false;

// ============================================================================
// Callbacks
// ============================================================================

void onRelayCommand(uint8_t relay, bool state) {
    Serial.printf("[Main] Relay command: CH%d -> %s\n", relay, state ? "ON" : "OFF");
    relays.setRelay(relay, state);
    
    // Log to web monitor
    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "Relay CH%d → %s (from Cloud)", relay, state ? "ON" : "OFF");
    WebMonitor::addLog("INFO", "RELAY", logMsg);
    
    // Blink LED to indicate command received
    digitalWrite(LED_MQTT, HIGH);
    delay(50);
    digitalWrite(LED_MQTT, LOW);
}

// Callback from automation sync to control relays
void onAutomationRelay(uint8_t relay, bool state, const char* source) {
    Serial.printf("[Main] Automation relay: CH%d -> %s [%s]\n", relay + 1, state ? "ON" : "OFF", source);
    relays.setRelay(relay + 1, state);  // relay index is 0-based, setRelay expects 1-based
    
    // Log to web monitor
    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "Relay CH%d → %s [%s]", relay + 1, state ? "ON" : "OFF", source);
    WebMonitor::addLog("INFO", "AUTO", logMsg);
}

// Callback when automation update received via MQTT
void onAutomationUpdate(const char* payload, unsigned int length) {
    Serial.println("\n[Main] ========================================");
    Serial.println("[Main] 📬 Processing automation update...");
    Serial.println("[Main] ========================================");
    WebMonitor::addLog("INFO", "MQTT", "Automation update received from Cloud");
    
    bool success = automationSync.handleUpdate(payload, length);
    
    Serial.printf("[Main] ✅ Parse result: %s\n", success ? "SUCCESS" : "FAILED");
    
    // Send acknowledgement back to Cloud
    SyncAckPayload ack = automationSync.getAckPayload(
        success, 
        success ? nullptr : "PARSE_ERROR",
        success ? nullptr : "Failed to parse automation payload"
    );
    
    mqttHandler.publishAutomationAck(
        ack.version,
        success,
        success ? nullptr : ack.errorCode,
        success ? nullptr : ack.errorMessage
    );
    
    if (success) {
        Serial.printf("[Main] ✅ Automation v%d synced successfully!\n", automationSync.getVersion());
        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "Automation v%d synced (%d rules)", 
                 automationSync.getVersion(), automationSync.getRuleCount());
        WebMonitor::addLog("INFO", "AUTO", logMsg);
    } else {
        Serial.println("[Main] ❌ Automation sync failed!");
        WebMonitor::addLog("ERROR", "AUTO", "Automation sync failed");
    }
}

void onVerifyRequest(const char* token) {
    Serial.printf("[Main] Verify request with token: %s\n", token);
    
    // Quick blink to indicate verification (reduced from 1.2s to 0.4s)
    for (int i = 0; i < 2; i++) {
        digitalWrite(LED_STATUS, HIGH);
        digitalWrite(LED_WIFI, HIGH);
        digitalWrite(LED_MQTT, HIGH);
        delay(100);
        digitalWrite(LED_STATUS, LOW);
        digitalWrite(LED_WIFI, LOW);
        digitalWrite(LED_MQTT, LOW);
        delay(100);
    }
}

void onAPStart() {
    Serial.println("[Main] AP mode started - waiting for configuration");
    // Quick blink to indicate AP mode (reduced from 1s to 0.4s)
    for (int i = 0; i < 2; i++) {
        digitalWrite(LED_WIFI, HIGH);
        delay(100);
        digitalWrite(LED_WIFI, LOW);
        delay(100);
    }
}

void onWiFiConnected() {
    Serial.printf("[Main] ✅ WiFi connected! IP: %s (RSSI: %d dBm)\n",
                  wifiManager.getIP().c_str(), wifiManager.getRSSI());
    digitalWrite(LED_WIFI, HIGH);
    
    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "WiFi connected: %s (RSSI: %d dBm)", 
             wifiManager.getIP().c_str(), wifiManager.getRSSI());
    WebMonitor::addLog("INFO", "SYSTEM", logMsg);
    
    // Setup NTP time sync (UTC+7 for Vietnam)
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    Serial.println("[Main] ⏰ NTP time sync started (UTC+7)");
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    // Initialize serial
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("=====================================");
    Serial.println("  🌱 GROW GATEWAY - ESP32-S3");
    Serial.printf("  Model: %s\n", DEVICE_MODEL);
    Serial.printf("  Firmware: %s\n", FIRMWARE_VERSION);
    Serial.println("=====================================");
    Serial.println();
    
    // Initialize LEDs
    pinMode(LED_STATUS, OUTPUT);
    pinMode(LED_WIFI, OUTPUT);
    pinMode(LED_MQTT, OUTPUT);
    digitalWrite(LED_STATUS, HIGH);  // Status on during boot
    
    // Initialize System Watchdog FIRST (for protection)
    systemWatchdog.begin();
    
    // Initialize relay controller
    Serial.println("[Init] Relay controller...");
    relays.begin();
    
    // Initialize sensor manager
    Serial.println("[Init] Sensor manager...");
    sensors.begin();
    
    // Initialize automation sync (SPIFFS)
    Serial.println("[Init] Automation sync...");
    if (automationSync.begin()) {
        automationSync.setRelayCallback(onAutomationRelay);
        Serial.printf("[Init] Automation loaded: v%d with %d rules\n", 
                      automationSync.getVersion(), 
                      automationSync.getRuleCount());
    }
    
    // Initialize WiFi manager
    Serial.println("[Init] WiFi manager...");
    wifiManager.begin();
    wifiManager.setAPStartCallback(onAPStart);
    wifiManager.setConnectedCallback(onWiFiConnected);
    
    // Generate/load device ID
    String deviceId = wifiManager.getDeviceId();
    Serial.printf("[Init] Device ID: %s\n", deviceId.c_str());
    
    // Load device token from preferences
    prefs.begin(PREF_NAMESPACE, true);
    String deviceToken = prefs.getString(KEY_DEVICE_TOKEN, DEFAULT_DEVICE_TOKEN);
    prefs.end();
    
    // Try to connect to WiFi
    if (wifiManager.autoConnect()) {
        // WiFi connected - initialize MQTT
        Serial.println("\n[Init] 🌐 WiFi connected! Initializing MQTT...");
        mqttHandler.begin(wifiClient);
        mqttHandler.setDeviceCredentials(deviceId.c_str(), deviceToken.c_str());
        
        // Set callbacks
        mqttHandler.setRelayCommandCallback(onRelayCommand);
        mqttHandler.setVerifyRequestCallback(onVerifyRequest);
        mqttHandler.setAutomationUpdateCallback(onAutomationUpdate);
        
        // Start Web Monitor on port 8080 (port 80 used by WiFi manager)
        webMonitor.setSensorManager(&sensors);
        webMonitor.setRelayController(&relays);
        webMonitor.begin(8080);
        
        systemReady = true;
    } else {
        // AP mode is active - wait for configuration
        Serial.println("[Init] 📡 AP mode active - Waiting for WiFi configuration...");
    }
    
    digitalWrite(LED_STATUS, LOW);
    Serial.println("[Init] Setup complete!");
    Serial.println();
    
    // Add startup log
    WebMonitor::addLog("INFO", "SYSTEM", "ESP32 Gateway started successfully");
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    unsigned long now = millis();
    static unsigned long lastStatusLog = 0;
    static unsigned long lastApLog = 0;
    
    // ========================================
    // WATCHDOG - Feed at start of loop
    // ========================================
    systemWatchdog.feed();
    
    // Reset sensor read flag each loop iteration
    sensorReadThisLoop = false;
    
    // Handle WiFi manager (AP mode web server)
    wifiManager.handleClient();
    
    // Handle Web Monitor requests
    webMonitor.handleClient();
    
    // If AP mode is active, keep serving but do not block main logic
    if (wifiManager.isAPModeActive()) {
        // Blink LED to indicate AP mode
        static unsigned long lastBlink = 0;
        if (now - lastBlink > 500) {
            lastBlink = now;
            digitalWrite(LED_WIFI, !digitalRead(LED_WIFI));
        }
        if (now - lastApLog > 10000) {
            lastApLog = now;
            Serial.printf("[WiFi] 📡 AP active: SSID=%s IP=%s\n",
                          wifiManager.getAPSSID().c_str(),
                          WiFi.softAPIP().toString().c_str());
        }
    }
    
    // Check WiFi connection periodically (NON-BLOCKING)
    static bool wifiReconnecting = false;
    static unsigned long wifiReconnectStart = 0;
    
    if (wifiReconnecting) {
        // Check if reconnected (non-blocking)
        if (wifiManager.isConnected()) {
            wifiReconnecting = false;
            Serial.println("[WiFi] ✅ Reconnected!");
            digitalWrite(LED_WIFI, HIGH);
        } else if (now - wifiReconnectStart > 5000) {
            // Timeout after 5 seconds, will retry on next interval
            wifiReconnecting = false;
            Serial.println("[WiFi] ⚠️ Reconnect timeout, will retry later");
        }
    } else if (now - lastWifiCheck > WIFI_CHECK_INTERVAL) {
        lastWifiCheck = now;
        
        if (!wifiManager.isConnected()) {
            Serial.println("[WiFi] Connection lost! Reconnecting (non-blocking)...");
            digitalWrite(LED_WIFI, LOW);
            WiFi.reconnect();
            wifiReconnecting = true;
            wifiReconnectStart = now;
        }
    }
    
    // MQTT connection management
    if (systemReady && wifiManager.isConnected()) {
        if (!mqttHandler.isConnected()) {
            digitalWrite(LED_MQTT, LOW);
            
            if (mqttHandler.connect()) {
                digitalWrite(LED_MQTT, HIGH);
                
                Serial.println("[Main] 🎉 MQTT Connected! Publishing online status...");
                WebMonitor::addLog("INFO", "MQTT", "Connected to broker");
                
                // Publish online status
                mqttHandler.publishStatus(
                    true,
                    wifiManager.getIP().c_str(),
                    wifiManager.getRSSI(),
                    millis() / 1000,
                    ESP.getFreeHeap()
                );
            }
        }
        
        // Process MQTT messages
        mqttHandler.loop();
    }

    // Periodic status log
    if (now - lastStatusLog > 10000) {
        lastStatusLog = now;
        Serial.printf("[Status] WiFi=%s SSID=%s IP=%s RSSI=%d dBm | AP=%s | MQTT=%s\n",
                      wifiManager.isConnected() ? "CONNECTED" : "DISCONNECTED",
                      wifiManager.getSavedSSID().c_str(),
                      wifiManager.getIP().c_str(),
                      wifiManager.getRSSI(),
                      wifiManager.isAPModeActive() ? "ON" : "OFF",
                      mqttHandler.isConnected() ? "CONNECTED" : "DISCONNECTED");
    }
    
    // Read sensors periodically (cache for reuse in this loop)
    if (now - lastSensorRead > SENSOR_READ_INTERVAL) {
        lastSensorRead = now;
        
        cachedReadings = sensors.read();
        sensorReadThisLoop = true;
        
        if (cachedReadings.valid) {
            Serial.printf("[Main] 📡 Sensor read: T=%.1f°C H=%.1f%% CO2=%d ppm VPD=%.2f\n",
                         cachedReadings.temperature,
                         cachedReadings.humidity,
                         cachedReadings.co2,
                         cachedReadings.vpd);
            
            // Log significant changes
            static float lastTemp = 0;
            static float lastHumi = 0;
            if (abs(cachedReadings.temperature - lastTemp) > 2.0 || 
                abs(cachedReadings.humidity - lastHumi) > 5.0) {
                char logMsg[128];
                snprintf(logMsg, sizeof(logMsg), "Sensor: %.1f°C, %.1f%%, %dppm CO2", 
                         cachedReadings.temperature, cachedReadings.humidity, cachedReadings.co2);
                WebMonitor::addLog("INFO", "SENSOR", logMsg);
                lastTemp = cachedReadings.temperature;
                lastHumi = cachedReadings.humidity;
            }
            
            // Update automation sync with current sensor values
            automationSync.updateSensorValues(
                cachedReadings.temperature,
                cachedReadings.humidity,
                cachedReadings.co2,
                cachedReadings.vpd
            );
            
            // Update web monitor with sensor data
            webMonitor.updateSensorData(cachedReadings);
        }
    }
    
    // Run automation rules every second
    if (now - lastAutomationCheck > 1000) {
        lastAutomationCheck = now;
        
        // Watchdog health check (mỗi 5 giây bên trong)
        systemWatchdog.checkHealth();
        
        // Skip automation if in Safe Mode
        if (systemWatchdog.isSafeMode()) {
            static unsigned long lastSafeModeLog = 0;
            if (now - lastSafeModeLog > 10000) {
                lastSafeModeLog = now;
                Serial.println("[Main] ⚠️ SAFE MODE - Automation disabled");
            }
        } else if (automationSync.isLoaded()) {
            // Check lighting schedule
            automationSync.checkLightingSchedule();
            
            // Check equipment schedules
            automationSync.checkEquipmentSchedules();
            
            // Run custom automation rules
            automationSync.runRules();

            // Publish lifecycle status when day/week changes
            static int lastProjectDay = -1;
            static int lastDayInWeek = -1;
            static int lastWeek = -1;
            static int lastWeekInPhase = -1;
            static char lastPhase[20] = "";
            
            int projectDay = automationSync.getProjectDay();
            int dayInWeek = automationSync.getCurrentDayInWeek();
            int currentWeek = automationSync.getCurrentWeek();
            int currentWeekInPhase = automationSync.getCurrentWeekInPhase();
            const char* currentPhase = automationSync.getCurrentPhase();
            
            if (mqttHandler.isConnected()) {
                bool changed = projectDay != lastProjectDay ||
                               dayInWeek != lastDayInWeek ||
                               currentWeek != lastWeek ||
                               currentWeekInPhase != lastWeekInPhase ||
                               strcmp(currentPhase, lastPhase) != 0;
                
                if (changed) {
                    bool published = mqttHandler.publishLifecycleStatus(
                        true,
                        wifiManager.getIP().c_str(),
                        wifiManager.getRSSI(),
                        millis() / 1000,
                        ESP.getFreeHeap(),
                        projectDay,
                        dayInWeek,
                        currentWeek,
                        currentWeekInPhase,
                        currentPhase
                    );
                    
                    if (published) {
                        lastProjectDay = projectDay;
                        lastDayInWeek = dayInWeek;
                        lastWeek = currentWeek;
                        lastWeekInPhase = currentWeekInPhase;
                        strlcpy(lastPhase, currentPhase, sizeof(lastPhase));
                    }
                }
            }
            
            // Check irrigation EVERY SECOND (for short pump durations like 10s)
            automationSync.checkIrrigationSchedules();
        }
    }
    
    // Publish sensor data to MQTT
    if (now - lastMqttPublish > MQTT_PUBLISH_INTERVAL) {
        lastMqttPublish = now;
        
        if (mqttHandler.isConnected()) {
            // Use cached readings if available, otherwise read fresh
            if (!sensorReadThisLoop) {
                cachedReadings = sensors.read();
                sensorReadThisLoop = true;
            }
            
            if (cachedReadings.valid) {
                Serial.printf("[Main] 📤 Sending sensor data to MQTT...\n");
                bool published = mqttHandler.publishSensorData(
                    cachedReadings.temperature,
                    cachedReadings.humidity,
                    cachedReadings.co2,
                    cachedReadings.waterTemp,
                    cachedReadings.ec,
                    cachedReadings.ph,
                    cachedReadings.waterLevel,
                    cachedReadings.vpd
                );
                
                if (published) {
                    // Quick blink to indicate data sent (non-blocking would be better)
                    digitalWrite(LED_MQTT, LOW);
                    delay(50);
                    digitalWrite(LED_MQTT, HIGH);
                }
            }
        }
    }
    
    // Publish status update
    if (now - lastStatusUpdate > STATUS_UPDATE_INTERVAL) {
        lastStatusUpdate = now;
        
        if (mqttHandler.isConnected()) {
            Serial.printf("[Main] 📤 Sending status update...\n");
            mqttHandler.publishStatus(
                true,
                wifiManager.getIP().c_str(),
                wifiManager.getRSSI(),
                millis() / 1000,
                ESP.getFreeHeap()
            );
        }
    }
    
    // Update relay status LED
    relays.updateStatusLED();
    
    // Check for overtime relays (safety)
    for (int i = 1; i <= 8; i++) {
        if (relays.isOvertime(i)) {
            Serial.printf("[Safety] Relay %d overtime - turning OFF\n", i);
            relays.setRelay(i, false);
        }
    }
    
    // Small delay to prevent watchdog issues
    delay(10);
}

// ============================================================================
// Serial Command Handler (for debugging)
// ============================================================================

void serialEvent() {
    while (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd.startsWith("relay")) {
            // Format: relay1=on, relay2=off
            int ch = cmd.charAt(5) - '0';
            bool state = cmd.indexOf("on") > 0;
            relays.setRelay(ch, state);
        }
        else if (cmd == "status") {
            Serial.println("=== SYSTEM STATUS ===");
            Serial.printf("WiFi: %s (%s, %d dBm)\n", 
                         wifiManager.isConnected() ? "Connected" : "Disconnected",
                         wifiManager.getIP().c_str(),
                         wifiManager.getRSSI());
            Serial.printf("MQTT: %s\n", mqttHandler.isConnected() ? "Connected" : "Disconnected");
            Serial.printf("Device ID: %s\n", mqttHandler.getDeviceId().c_str());
            Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
            Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
            Serial.printf("Min Heap: %d bytes\n", ESP.getMinFreeHeap());
            Serial.printf("Simulation: %s\n", sensors.isSimulationMode() ? "ON" : "OFF");
            Serial.printf("Safe Mode: %s\n", systemWatchdog.isSafeMode() ? "YES" : "NO");
            Serial.printf("Crash Count: %d/%d\n", systemWatchdog.getStatus().crashCount, MAX_CRASH_COUNT);
            Serial.printf("Last Reset: %s\n", systemWatchdog.getLastResetReason().c_str());
        }
        else if (cmd == "sensors") {
            SensorReadings r = sensors.read();
            Serial.printf("Temp: %.1f°C\n", r.temperature);
            Serial.printf("Humidity: %.1f%%\n", r.humidity);
            Serial.printf("CO2: %d ppm\n", r.co2);
            Serial.printf("VPD: %.2f kPa\n", r.vpd);
            Serial.printf("Water Temp: %.1f°C\n", r.waterTemp);
            Serial.printf("EC: %.2f mS/cm\n", r.ec);
            Serial.printf("pH: %.1f\n", r.ph);
            Serial.printf("Water Level: %d%%\n", r.waterLevel);
        }
        else if (cmd == "relays") {
            RelayStates states = relays.getRelayStates();
            Serial.println("=== RELAY STATES ===");
            Serial.printf("1 (Light):      %s\n", states.relay1 ? "ON" : "OFF");
            Serial.printf("2 (Fan Circ):   %s\n", states.relay2 ? "ON" : "OFF");
            Serial.printf("3 (Fan Exh):    %s\n", states.relay3 ? "ON" : "OFF");
            Serial.printf("4 (Pump 1):     %s\n", states.relay4 ? "ON" : "OFF");
            Serial.printf("5 (Pump 2):     %s\n", states.relay5 ? "ON" : "OFF");
            Serial.printf("6 (Pump 3):     %s\n", states.relay6 ? "ON" : "OFF");
            Serial.printf("7 (CO2):        %s\n", states.relay7 ? "ON" : "OFF");
            Serial.printf("8 (AC):         %s\n", states.relay8 ? "ON" : "OFF");
        }
        else if (cmd == "reset") {
            Serial.println("Resetting to factory defaults...");
            wifiManager.clearCredentials();
            prefs.begin(PREF_NAMESPACE, false);
            prefs.clear();
            prefs.end();
            delay(1000);
            ESP.restart();
        }
        else if (cmd == "reboot") {
            Serial.println("Rebooting...");
            delay(500);
            ESP.restart();
        }
        else if (cmd == "wdt_reset") {
            Serial.println("Resetting crash counter...");
            systemWatchdog.resetCrashCount();
        }
        else if (cmd == "wdt_status") {
            SystemStatus ws = systemWatchdog.getStatus();
            Serial.println("=== WATCHDOG STATUS ===");
            Serial.printf("WDT Enabled: %s\n", ws.wdtEnabled ? "YES" : "NO");
            Serial.printf("Safe Mode: %s\n", ws.safeMode ? "YES" : "NO");
            Serial.printf("Health: %s\n", 
                         ws.health == HEALTH_OK ? "OK" :
                         ws.health == HEALTH_WARNING ? "WARNING" :
                         ws.health == HEALTH_CRITICAL ? "CRITICAL" : "RECOVERY");
            Serial.printf("Crash Count: %d/%d\n", ws.crashCount, MAX_CRASH_COUNT);
            Serial.printf("Free Heap: %d bytes\n", ws.freeHeap);
            Serial.printf("Min Free Heap: %d bytes\n", ws.minFreeHeap);
            Serial.printf("Uptime: %lu seconds\n", ws.uptime);
            Serial.printf("Last Error: %s\n", strlen(ws.lastError) > 0 ? ws.lastError : "(none)");
            Serial.printf("Last Reset: %s\n", systemWatchdog.getLastResetReason().c_str());
        }
        else if (cmd == "help") {
            Serial.println("=== COMMANDS ===");
            Serial.println("status     - Show system status");
            Serial.println("sensors    - Read all sensors");
            Serial.println("relays     - Show relay states");
            Serial.println("relay1=on/off - Control relay");
            Serial.println("reset      - Factory reset");
            Serial.println("reboot     - Reboot device");
            Serial.println("wdt_status - Show watchdog status");
            Serial.println("wdt_reset  - Reset crash counter & exit safe mode");
        }
    }
}
