/**
 * ESP32 Web Monitor Implementation
 */

#include "web_monitor.h"
#include "mqtt_handler.h"
#include "system_watchdog.h"
#include "dashboard_html.h"
#include <LittleFS.h>

// Use LittleFS alias
#ifndef SPIFFS
#define SPIFFS LittleFS
#endif

// Global instance
WebMonitor webMonitor;

// Static log buffer
LogEntry WebMonitor::_logBuffer[MAX_LOG_ENTRIES];
int WebMonitor::_logIndex = 0;
int WebMonitor::_logCount = 0;

WebMonitor::WebMonitor() {
    _server = nullptr;
    _running = false;
    _sensors = nullptr;
    _relays = nullptr;
    memset(&_cachedReadings, 0, sizeof(_cachedReadings));
}

void WebMonitor::begin(uint16_t port) {
    if (_server != nullptr) {
        delete _server;
    }
    
    _server = new WebServer(port);
    
    // Setup routes
    _server->on("/", HTTP_GET, [this]() { handleRoot(); });
    _server->on("/api/status", HTTP_GET, [this]() { handleApiStatus(); });
    _server->on("/api/sensors", HTTP_GET, [this]() { handleApiSensors(); });
    _server->on("/api/relays", HTTP_GET, [this]() { handleApiRelays(); });
    _server->on("/api/relay/toggle", HTTP_POST, [this]() { handleApiRelayToggle(); });
    _server->on("/api/inputs", HTTP_GET, [this]() { handleApiInputs(); });
    _server->on("/api/automation", HTTP_GET, [this]() { handleApiAutomation(); });
    _server->on("/api/automation/full", HTTP_GET, [this]() { handleApiAutomationFull(); });
    _server->on("/api/automation/check", HTTP_GET, [this]() { handleApiAutomationCheck(); });
    _server->on("/api/system", HTTP_GET, [this]() { handleApiSystem(); });
    _server->on("/api/watchdog", HTTP_GET, [this]() { handleApiWatchdog(); });
    _server->on("/api/watchdog/reset", HTTP_POST, [this]() { handleApiWatchdogReset(); });
    _server->on("/api/logs", HTTP_GET, [this]() { handleApiLogs(); });
    _server->onNotFound([this]() { handleNotFound(); });
    
    // Enable CORS
    _server->enableCORS(true);
    
    _server->begin();
    _running = true;
    
    Serial.printf("[WebMonitor] ðŸŒ Server started on port %d\n", port);
    Serial.printf("[WebMonitor] ðŸ“Š Dashboard: http://%s/\n", WiFi.localIP().toString().c_str());
}

void WebMonitor::stop() {
    if (_server != nullptr) {
        _server->stop();
        delete _server;
        _server = nullptr;
    }
    _running = false;
    Serial.println("[WebMonitor] Server stopped");
}

void WebMonitor::handleClient() {
    if (_running && _server != nullptr) {
        _server->handleClient();
    }
}

void WebMonitor::setSensorManager(SensorManager* sensors) {
    _sensors = sensors;
}

void WebMonitor::setRelayController(RelayController* relays) {
    _relays = relays;
}

void WebMonitor::updateSensorData(const SensorReadings& readings) {
    _cachedReadings = readings;
}

void WebMonitor::addLog(const char* level, const char* category, const char* message) {
    // Add to circular buffer
    LogEntry& entry = _logBuffer[_logIndex];
    entry.timestamp = millis();
    strncpy(entry.level, level, sizeof(entry.level) - 1);
    strncpy(entry.category, category, sizeof(entry.category) - 1);
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.level[sizeof(entry.level) - 1] = '\0';
    entry.category[sizeof(entry.category) - 1] = '\0';
    entry.message[sizeof(entry.message) - 1] = '\0';
    
    _logIndex = (_logIndex + 1) % MAX_LOG_ENTRIES;
    if (_logCount < MAX_LOG_ENTRIES) {
        _logCount++;
    }
}

String WebMonitor::getJsonContentType() {
    return "application/json";
}

// ============================================================================
// API Handlers
// ============================================================================

void WebMonitor::handleApiStatus() {
    JsonDocument doc;
    
    // System
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["wifiRSSI"] = WiFi.RSSI();
    doc["wifiIP"] = WiFi.localIP().toString();
    doc["mqttConnected"] = mqttHandler.isConnected();
    
    // Timezone
    doc["timezone"] = automationSync.getTimezoneName();
    doc["timezoneOffset"] = automationSync.getTimezoneOffset();
    char localTime[6];
    automationSync.getLocalTime(localTime);
    doc["localTime"] = localTime;
    doc["localHour"] = automationSync.getLocalHour();
    doc["localMinute"] = automationSync.getLocalMinute();
    
    // Automation
    doc["automationLoaded"] = automationSync.isLoaded();
    doc["automationVersion"] = automationSync.getVersion();
    doc["currentWeek"] = automationSync.getCurrentWeek();
    doc["currentPhase"] = automationSync.getCurrentPhase();
    doc["isDaytime"] = automationSync.isDaytime();
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleApiSensors() {
    JsonDocument doc;
    
    doc["temperature"] = _cachedReadings.temperature;
    doc["humidity"] = _cachedReadings.humidity;
    doc["co2"] = _cachedReadings.co2;
    doc["vpd"] = _cachedReadings.vpd;
    doc["waterTemp"] = _cachedReadings.waterTemp;
    doc["ec"] = _cachedReadings.ec;
    doc["ph"] = _cachedReadings.ph;
    doc["waterLevel"] = _cachedReadings.waterLevel;
    doc["valid"] = _cachedReadings.valid;
    doc["timestamp"] = _cachedReadings.timestamp;
    
    // Targets
    JsonObject targets = doc["targets"].to<JsonObject>();
    targets["tempTarget"] = automationSync.getCurrentTempTarget();
    targets["humiHigh"] = automationSync.getCurrentHumiHigh();
    targets["humiLow"] = automationSync.getCurrentHumiLow();
    targets["co2Start"] = automationSync.getCurrentCo2Start();
    targets["co2Stop"] = automationSync.getCurrentCo2Stop();
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleApiRelays() {
    JsonDocument doc;
    
    if (_relays != nullptr) {
        RelayStates states = _relays->getRelayStates();
        
        JsonArray relays = doc["relays"].to<JsonArray>();
        
        const char* names[] = {"Light", "FanCirc", "FanExh", "Pump1", "Pump2", "Pump3", "CO2", "AC"};
        bool stateArr[] = {states.relay1, states.relay2, states.relay3, states.relay4, 
                           states.relay5, states.relay6, states.relay7, states.relay8};
        
        for (int i = 0; i < 8; i++) {
            JsonObject relay = relays.add<JsonObject>();
            relay["channel"] = i + 1;
            relay["name"] = names[i];
            relay["state"] = stateArr[i];
        }
    }
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleApiRelayToggle() {
    if (!_server->hasArg("ch")) {
        _server->send(400, getJsonContentType(), "{\"error\":\"Missing ch parameter\"}");
        return;
    }
    
    int ch = _server->arg("ch").toInt();
    if (ch < 1 || ch > 8) {
        _server->send(400, getJsonContentType(), "{\"error\":\"Channel must be 1-8\"}");
        return;
    }
    
    if (_relays != nullptr) {
        _relays->toggleRelay(ch);
        bool newState = _relays->getRelay(ch);
        
        char logMsg[64];
        snprintf(logMsg, sizeof(logMsg), "Relay CH%d toggled to %s (manual)", ch, newState ? "ON" : "OFF");
        addLog("INFO", "RELAY", logMsg);
        Serial.printf("[WebMonitor] âš¡ %s\n", logMsg);
        
        JsonDocument doc;
        doc["success"] = true;
        doc["channel"] = ch;
        doc["state"] = newState;
        String output;
        serializeJson(doc, output);
        _server->send(200, getJsonContentType(), output);
    } else {
        _server->send(500, getJsonContentType(), "{\"error\":\"Relay controller not available\"}");
    }
}

void WebMonitor::handleApiInputs() {
    JsonDocument doc;
    
    const uint8_t inputPins[] = {DI_PIN_1, DI_PIN_2, DI_PIN_3, DI_PIN_4, DI_PIN_5, DI_PIN_6, DI_PIN_7};
    const char* inputNames[] = {"Input 1", "Input 2", "Input 3", "Input 4", "Input 5", "Input 6", "Input 7"};
    
    JsonArray inputs = doc["inputs"].to<JsonArray>();
    for (int i = 0; i < 7; i++) {
        JsonObject input = inputs.add<JsonObject>();
        input["channel"] = i + 1;
        input["name"] = inputNames[i];
        input["pin"] = inputPins[i];
        input["state"] = digitalRead(inputPins[i]);
    }
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleApiAutomation() {
    JsonDocument doc;
    
    doc["loaded"] = automationSync.isLoaded();
    doc["version"] = automationSync.getVersion();
    doc["gatewayId"] = automationSync.getGatewayId();
    doc["roomId"] = automationSync.getRoomId();
    doc["ruleCount"] = automationSync.getRuleCount();
    
    // Current week/phase
    doc["currentWeek"] = automationSync.getCurrentWeek();
    doc["currentPhase"] = automationSync.getCurrentPhase();
    doc["isDaytime"] = automationSync.isDaytime();
    
    // Lighting schedule
    const LightingSchedule& light = automationSync.getLighting();
    JsonObject lighting = doc["lighting"].to<JsonObject>();
    lighting["lightsOn"] = light.lightsOn;
    lighting["lightsOff"] = light.lightsOff;
    
    // Targets
    const EnvironmentTargets& t = automationSync.getTargets();
    JsonObject targets = doc["targets"].to<JsonObject>();
    targets["tempDay"] = t.tempTargetDay;
    targets["tempNight"] = t.tempTargetNight;
    targets["humiHighDay"] = t.humiHighDay;
    targets["humiLowDay"] = t.humiLowDay;
    targets["humiHighNight"] = t.humiHighNight;
    targets["humiLowNight"] = t.humiLowNight;
    targets["co2StartDay"] = t.co2StartDay;
    targets["co2StopDay"] = t.co2StopDay;
    targets["vpdMin"] = t.vpdMin;
    targets["vpdMax"] = t.vpdMax;
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleApiAutomationFull() {
    JsonDocument doc;
    
    doc["loaded"] = automationSync.isLoaded();
    doc["version"] = automationSync.getVersion();
    doc["gatewayId"] = automationSync.getGatewayId();
    doc["roomId"] = automationSync.getRoomId();
    doc["ruleCount"] = automationSync.getRuleCount();
    doc["currentWeek"] = automationSync.getCurrentWeek();
    doc["currentPhase"] = automationSync.getCurrentPhase();
    doc["totalWeeks"] = automationSync.getTotalWeeks();
    doc["plantStartTimestamp"] = automationSync.getPlantStartTimestamp();
    doc["timezoneOffset"] = automationSync.getTimezoneOffset();
    doc["timezoneName"] = automationSync.getTimezoneName();
    
    // Get all weekly plans
    JsonArray weeksArray = doc["weeklyPlans"].to<JsonArray>();
    const WeeklyPlan* plans = automationSync.getWeeklyPlans();
    int planCount = automationSync.getWeeklyPlanCount();
    
    Serial.printf("[WebMonitor] ðŸ“… API /automation/full - Weekly Plans Count: %d\n", planCount);
    
    for (int i = 0; i < planCount; i++) {
        JsonObject weekObj = weeksArray.add<JsonObject>();
        weekObj["week"] = plans[i].week;
        weekObj["phase"] = plans[i].phase;
        
        Serial.printf("[WebMonitor] ðŸ“… Week %d: %s\n", plans[i].week, plans[i].phase);
        
        // Lighting
        JsonObject lighting = weekObj["lighting"].to<JsonObject>();
        lighting["lightsOn"] = plans[i].lighting.lightsOn;
        lighting["lightsOff"] = plans[i].lighting.lightsOff;
        
        // Targets
        JsonObject targets = weekObj["targets"].to<JsonObject>();
        targets["tempTargetDay"] = plans[i].targets.tempTargetDay;
        targets["humiHighDay"] = plans[i].targets.humiHighDay;
        targets["humiLowDay"] = plans[i].targets.humiLowDay;
        targets["co2StartDay"] = plans[i].targets.co2StartDay;
        targets["co2StopDay"] = plans[i].targets.co2StopDay;
        targets["tempTargetNight"] = plans[i].targets.tempTargetNight;
        targets["humiHighNight"] = plans[i].targets.humiHighNight;
        targets["humiLowNight"] = plans[i].targets.humiLowNight;
        targets["co2StartNight"] = plans[i].targets.co2StartNight;
        targets["co2StopNight"] = plans[i].targets.co2StopNight;
        targets["vpdMin"] = plans[i].targets.vpdMin;
        targets["vpdMax"] = plans[i].targets.vpdMax;
        
        // Equipment
        JsonObject equipment = weekObj["equipment"].to<JsonObject>();
        equipment["fanCircMode"] = plans[i].equipment.fanCircMode;
        equipment["fanCircOnTime"] = plans[i].equipment.fanCircOnTime;
        equipment["fanCircOffTime"] = plans[i].equipment.fanCircOffTime;
        equipment["fanCircTriggerTemp"] = plans[i].equipment.fanCircTriggerTemp;
        equipment["fanExhMode"] = plans[i].equipment.fanExhMode;
        equipment["fanExhOnTime"] = plans[i].equipment.fanExhOnTime;
        equipment["fanExhOffTime"] = plans[i].equipment.fanExhOffTime;
        equipment["fanExhTriggerHumi"] = plans[i].equipment.fanExhTriggerHumi;
        equipment["fanExhTriggerVpd"] = plans[i].equipment.fanExhTriggerVpd;
        equipment["acMode"] = plans[i].equipment.acMode;
        equipment["acTargetTemp"] = plans[i].equipment.acTargetTemp;
        equipment["acFanSpeed"] = plans[i].equipment.acFanSpeed;
    }
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleApiSystem() {
    JsonDocument doc;
    
    // ESP32 info
    doc["chipModel"] = ESP.getChipModel();
    doc["chipCores"] = ESP.getChipCores();
    doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["minFreeHeap"] = ESP.getMinFreeHeap();
    doc["heapSize"] = ESP.getHeapSize();
    doc["flashSize"] = ESP.getFlashChipSize();
    doc["sketchSize"] = ESP.getSketchSize();
    doc["freeSketchSpace"] = ESP.getFreeSketchSpace();
    
    // Uptime
    unsigned long uptime = millis() / 1000;
    doc["uptimeSeconds"] = uptime;
    doc["uptimeFormatted"] = String(uptime / 86400) + "d " + 
                             String((uptime % 86400) / 3600) + "h " +
                             String((uptime % 3600) / 60) + "m";
    
    // WiFi
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = WiFi.SSID();
    wifi["rssi"] = WiFi.RSSI();
    wifi["ip"] = WiFi.localIP().toString();
    wifi["mac"] = WiFi.macAddress();
    wifi["channel"] = WiFi.channel();
    
    // LittleFS
    JsonObject storage = doc["storage"].to<JsonObject>();
    storage["totalBytes"] = LittleFS.totalBytes();
    storage["usedBytes"] = LittleFS.usedBytes();
    storage["freeBytes"] = LittleFS.totalBytes() - LittleFS.usedBytes();
    
    // Watchdog
    SystemStatus ws = systemWatchdog.getStatus();
    JsonObject watchdog = doc["watchdog"].to<JsonObject>();
    watchdog["enabled"] = ws.wdtEnabled;
    watchdog["safeMode"] = ws.safeMode;
    watchdog["crashCount"] = ws.crashCount;
    watchdog["maxCrashCount"] = MAX_CRASH_COUNT;
    watchdog["lastReset"] = systemWatchdog.getLastResetReason();
    watchdog["health"] = ws.health == HEALTH_OK ? "OK" :
                         ws.health == HEALTH_WARNING ? "WARNING" :
                         ws.health == HEALTH_CRITICAL ? "CRITICAL" : "RECOVERY";
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleNotFound() {
    _server->send(404, "text/plain", "Not Found");
}

void WebMonitor::handleApiWatchdog() {
    JsonDocument doc;
    
    SystemStatus ws = systemWatchdog.getStatus();
    
    doc["enabled"] = ws.wdtEnabled;
    doc["safeMode"] = ws.safeMode;
    doc["crashCount"] = ws.crashCount;
    doc["maxCrashCount"] = MAX_CRASH_COUNT;
    doc["freeHeap"] = ws.freeHeap;
    doc["minFreeHeap"] = ws.minFreeHeap;
    doc["heapWarning"] = HEAP_WARNING_THRESHOLD;
    doc["heapCritical"] = HEAP_CRITICAL_THRESHOLD;
    doc["uptime"] = ws.uptime;
    doc["lastError"] = ws.lastError;
    doc["lastReset"] = systemWatchdog.getLastResetReason();
    doc["health"] = ws.health == HEALTH_OK ? "OK" :
                    ws.health == HEALTH_WARNING ? "WARNING" :
                    ws.health == HEALTH_CRITICAL ? "CRITICAL" : "RECOVERY";
    
    // Health color
    doc["healthColor"] = ws.health == HEALTH_OK ? "#4CAF50" :
                         ws.health == HEALTH_WARNING ? "#FF9800" :
                         ws.health == HEALTH_CRITICAL ? "#F44336" : "#9C27B0";
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleApiWatchdogReset() {
    systemWatchdog.resetCrashCount();
    
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "Crash counter reset, safe mode exited";
    doc["crashCount"] = 0;
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleApiLogs() {
    JsonDocument doc;
    JsonArray logs = doc["logs"].to<JsonArray>();
    
    // Get logs in chronological order (oldest first)
    int count = min(_logCount, MAX_LOG_ENTRIES);
    int startIdx = (_logIndex - count + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
    
    for (int i = 0; i < count; i++) {
        int idx = (startIdx + i) % MAX_LOG_ENTRIES;
        LogEntry& entry = _logBuffer[idx];
        
        JsonObject log = logs.add<JsonObject>();
        log["timestamp"] = entry.timestamp;
        log["level"] = entry.level;
        log["category"] = entry.category;
        log["message"] = entry.message;
    }
    
    doc["count"] = count;
    doc["total"] = _logCount;
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleApiAutomationCheck() {
    JsonDocument doc;
    
    // SPIFFS should already be mounted by AutomationSync::begin()
    // If not mounted, try to mount it
    if (!SPIFFS.begin(false)) {
        // Try with format=true if first attempt fails
        if (!SPIFFS.begin(true)) {
            doc["spiffs_ok"] = false;
            doc["error"] = "SPIFFS mount failed";
            String output;
            serializeJson(doc, output);
            _server->send(200, getJsonContentType(), output);
            return;
        }
    }
    
    doc["spiffs_ok"] = true;
    doc["total_bytes"] = SPIFFS.totalBytes();
    doc["used_bytes"] = SPIFFS.usedBytes();
    doc["free_bytes"] = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    
    // Check automation.json file
    if (SPIFFS.exists("/automation.json")) {
        File file = SPIFFS.open("/automation.json", "r");
        if (file) {
            size_t fileSize = file.size();
            doc["file_exists"] = true;
            doc["file_path"] = "/automation.json";
            doc["file_size"] = fileSize;
            
            // Try to read first 200 bytes to validate it's valid JSON
            char buffer[201];
            size_t bytesRead = file.readBytes(buffer, 200);
            buffer[bytesRead] = '\0';
            file.close();
            
            doc["file_preview"] = buffer;
            
            // Parse to check validity
            JsonDocument tempDoc;
            DeserializationError error = deserializeJson(tempDoc, buffer);
            doc["json_valid"] = !error;
            if (error) {
                doc["json_error"] = error.c_str();
            }
        } else {
            doc["file_exists"] = true;
            doc["file_open_error"] = "Cannot open file";
        }
    } else {
        doc["file_exists"] = false;
        doc["file_path"] = "/automation.json";
    }
    
    // Check backup file
    if (SPIFFS.exists("/automation.backup")) {
        File bfile = SPIFFS.open("/automation.backup", "r");
        if (bfile) {
            doc["backup_exists"] = true;
            doc["backup_size"] = bfile.size();
            bfile.close();
        }
    } else {
        doc["backup_exists"] = false;
    }
    
    // Automation state
    doc["automation_loaded"] = automationSync.isLoaded();
    doc["automation_version"] = automationSync.getVersion();
    doc["automation_rules"] = automationSync.getRuleCount();
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

// ============================================================================
// HTML Dashboard
// ============================================================================

void WebMonitor::handleRoot() {
    _server->send_P(200, "text/html", DASHBOARD_PAGE);
}

String WebMonitor::getMonitorPage() {
    return String(FPSTR(DASHBOARD_PAGE));
}

