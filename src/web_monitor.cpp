/**
 * ESP32 Web Monitor Implementation
 */

#include "web_monitor.h"
#include "mqtt_handler.h"
#include "system_watchdog.h"
#include "dashboard_html_v3.h"
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
    _server->on("/api/sensors/mock", HTTP_POST, [this]() { handleApiSensorMock(); });
    _server->on("/api/relays", HTTP_GET, [this]() { handleApiRelays(); });
    _server->on("/api/relay/toggle", HTTP_POST, [this]() { handleApiRelayToggle(); });
    _server->on("/api/inputs", HTTP_GET, [this]() { handleApiInputs(); });
    _server->on("/api/automation", HTTP_GET, [this]() { handleApiAutomation(); });
    _server->on("/api/automation/full", HTTP_GET, [this]() { handleApiAutomationFull(); });
    _server->on("/api/automation/check", HTTP_GET, [this]() { handleApiAutomationCheck(); });
    _server->on("/api/lifecycle", HTTP_POST, [this]() { handleApiLifecycleSet(); });
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
    if (automationSync.isSensorOverrideEnabled()) {
        return;
    }
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
    doc["mockEnabled"] = automationSync.isSensorOverrideEnabled();
    
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

void WebMonitor::handleApiSensorMock() {
    JsonDocument doc;
    JsonDocument resp;

    String body = _server->arg("plain");
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        resp["ok"] = false;
        resp["message"] = "Invalid JSON";
        String output;
        serializeJson(resp, output);
        _server->send(400, getJsonContentType(), output);
        return;
    }

    bool enabled = doc["enabled"] | false;
    if (enabled) {
        float temp = doc["temperature"] | _cachedReadings.temperature;
        float humi = doc["humidity"] | _cachedReadings.humidity;
        float co2 = doc["co2"] | _cachedReadings.co2;
        float vpd = doc["vpd"] | _cachedReadings.vpd;

        automationSync.setSensorOverride(temp, humi, co2, vpd);
        _cachedReadings.temperature = temp;
        _cachedReadings.humidity = humi;
        _cachedReadings.co2 = (int)co2;
        _cachedReadings.vpd = vpd;
        _cachedReadings.valid = true;
        _cachedReadings.timestamp = millis();
    } else {
        automationSync.clearSensorOverride();
    }

    resp["ok"] = true;
    resp["mockEnabled"] = automationSync.isSensorOverrideEnabled();
    resp["temperature"] = _cachedReadings.temperature;
    resp["humidity"] = _cachedReadings.humidity;
    resp["co2"] = _cachedReadings.co2;
    resp["vpd"] = _cachedReadings.vpd;

    String output;
    serializeJson(resp, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleApiRelays() {
    JsonDocument doc;
    
    if (_relays != nullptr) {
        RelayStates states = _relays->getRelayStates();
        
        JsonArray relays = doc["relays"].to<JsonArray>();
        
        const char* names[] = {"Light", "PhunAm", "HutAm", "CO2", "Pump", "QuatThoi", "QuatHut", "Option"};
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
    
    // Basic info
    doc["loaded"] = automationSync.isLoaded();
    doc["version"] = automationSync.getVersion();
    doc["gatewayId"] = automationSync.getGatewayId();
    doc["roomId"] = automationSync.getRoomId();
    doc["ruleCount"] = automationSync.getRuleCount();
    
    // Phase & Week info (JSON v3)
    doc["currentWeek"] = automationSync.getCurrentWeek();
    doc["currentPhase"] = automationSync.getCurrentPhase();
    doc["currentWeekInPhase"] = automationSync.getCurrentWeekInPhase();
    doc["phaseWeekCount"] = automationSync.getPhaseWeekCount();
    doc["totalWeeks"] = automationSync.getTotalWeeks();
    doc["projectDay"] = automationSync.getProjectDay();
    doc["currentDayInWeek"] = automationSync.getCurrentDayInWeek();
    doc["lifecycleOverride"] = automationSync.isLifecycleOverrideEnabled();
    
    // Timezone
    doc["timezoneOffset"] = automationSync.getTimezoneOffset();
    doc["timezoneName"] = automationSync.getTimezoneName();
    doc["plantStartTimestamp"] = automationSync.getPlantStartTimestamp();
    
    // Current week targets (for quick access)
    const EnvironmentTargets* targets = automationSync.getCurrentTargets();
    if (targets) {
      JsonObject tgt = doc["targets"].to<JsonObject>();
      tgt["tempTargetDay"] = targets->tempTargetDay;
      tgt["tempTargetNight"] = targets->tempTargetNight;
      tgt["humiHighDay"] = targets->humiHighDay;
      tgt["humiLowDay"] = targets->humiLowDay;
      tgt["humiHighNight"] = targets->humiHighNight;
      tgt["humiLowNight"] = targets->humiLowNight;
      tgt["co2StartDay"] = targets->co2StartDay;
      tgt["co2StopDay"] = targets->co2StopDay;
      tgt["co2StartNight"] = targets->co2StartNight;
      tgt["co2StopNight"] = targets->co2StopNight;
      tgt["vpdMin"] = targets->vpdMin;
      tgt["vpdMax"] = targets->vpdMax;
      tgt["humidityMode"] = targets->humidityMode;
    }
    
    // Current week lighting (with PWM schedule)
    const LightingSchedule* lighting = automationSync.getCurrentLightingSchedule();
    if (lighting) {
      JsonObject light = doc["lighting"].to<JsonObject>();
      light["lightsOn"] = lighting->lightsOn;
      light["lightsOff"] = lighting->lightsOff;
      
      // PWM dimming points
      JsonArray schedule = light["schedule"].to<JsonArray>();
      for (int i = 0; i < lighting->scheduleCount && i < 10; i++) {
        JsonObject point = schedule.add<JsonObject>();
        point["time"] = lighting->schedule[i].time;
        point["brightness"] = lighting->schedule[i].brightness;
        
        JsonObject channels = point["channels"].to<JsonObject>();
        channels["ch1"] = lighting->schedule[i].ch1;  // White
        channels["ch2"] = lighting->schedule[i].ch2;  // Yellow
        channels["ch3"] = lighting->schedule[i].ch3;  // Red
      }
    }
    
    // Current week equipment config
    const EquipmentConfig* equipment = automationSync.getCurrentEquipmentConfig();
    if (equipment) {
      JsonObject equip = doc["equipment"].to<JsonObject>();
      
      JsonObject fanCirc = equip["fanCirculation"].to<JsonObject>();
      fanCirc["mode"] = equipment->fanCircMode;
      fanCirc["triggerTemp"] = equipment->fanCircTriggerTemp;
      
      JsonObject fanExh = equip["fanExhaust"].to<JsonObject>();
      fanExh["mode"] = equipment->fanExhMode;
      fanExh["triggerHumidity"] = equipment->fanExhTriggerHumi;
      fanExh["triggerVpd"] = equipment->fanExhTriggerVpd;
      
      JsonObject ac = equip["ac"].to<JsonObject>();
      ac["mode"] = equipment->acMode;
      ac["targetTemp"] = equipment->acTargetTemp;
      ac["fanSpeed"] = equipment->acFanSpeed;
    }
    
    // Irrigation schedules (for current week)
    JsonArray irrigArray = doc["irrigation"].to<JsonArray>();
    const IrrigationConfig* irrigs = automationSync.getIrrigations();
    int irrigCount = automationSync.getIrrigationCount();
    
    for (int i = 0; i < irrigCount; i++) {
      JsonObject irrigObj = irrigArray.add<JsonObject>();
      irrigObj["startTime"] = irrigs[i].cycleStart;
      irrigObj["endTime"] = irrigs[i].cycleEnd;
      irrigObj["durationMinutes"] = irrigs[i].pumpDurationSec / 60;
      irrigObj["flowRate"] = 2.5;  // Default 2.5 L/min
      irrigObj["enabled"] = irrigs[i].enabled;
    }
    
    // Relay states (current status)
    if (_relays) {
      JsonArray relayStates = doc["relayStates"].to<JsonArray>();
      for (int i = 0; i < 8; i++) {
        relayStates.add(_relays->getRelay(i));
      }
    }
    
    // Sensor readings (current readings)
    if (_sensors) {
      JsonObject readings = doc["sensorReadings"].to<JsonObject>();
      readings["temperature"] = _cachedReadings.temperature;
      readings["humidity"] = _cachedReadings.humidity;
      readings["co2"] = _cachedReadings.co2;
      readings["vpd"] = _cachedReadings.vpd;
    }

        doc["sensorMockEnabled"] = automationSync.isSensorOverrideEnabled();
    
    // Automation status
    doc["irrigationCount"] = automationSync.getIrrigationCount();
    doc["automationRunning"] = true;
    
    String output;
    serializeJson(doc, output);
    _server->send(200, getJsonContentType(), output);
}

void WebMonitor::handleApiLifecycleSet() {
    JsonDocument doc;
    JsonDocument resp;

    String body = _server->arg("plain");
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        resp["ok"] = false;
        resp["message"] = "Invalid JSON";
        String output;
        serializeJson(resp, output);
        _server->send(400, getJsonContentType(), output);
        return;
    }

    int projectDay = doc["projectDay"] | 0;
    int currentWeek = 0;
    const char* phase = doc["currentPhase"] | "";

    if (projectDay < 1) {
        resp["ok"] = false;
        resp["message"] = "projectDay must be >= 1";
        String output;
        serializeJson(resp, output);
        _server->send(400, getJsonContentType(), output);
        return;
    }

    bool ok = automationSync.setLifecycleOverride(projectDay, currentWeek, phase);
    resp["ok"] = ok;
    resp["message"] = ok ? "Saved" : "Failed to save";
    resp["projectDay"] = automationSync.getProjectDay();
    resp["currentWeek"] = automationSync.getCurrentWeek();
    resp["currentWeekInPhase"] = automationSync.getCurrentWeekInPhase();
    resp["currentPhase"] = automationSync.getCurrentPhase();

    if (ok && mqttHandler.isConnected()) {
        mqttHandler.publishLifecycleStatus(
            true,
            WiFi.localIP().toString().c_str(),
            WiFi.RSSI(),
            millis() / 1000,
            ESP.getFreeHeap(),
            automationSync.getProjectDay(),
            automationSync.getCurrentDayInWeek(),
            automationSync.getCurrentWeek(),
            automationSync.getCurrentWeekInPhase(),
            automationSync.getCurrentPhase()
        );
    }

    String output;
    serializeJson(resp, output);
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
    _server->send_P(200, "text/html", DASHBOARD_HTML);
}

String WebMonitor::getMonitorPage() {
    return String(FPSTR(DASHBOARD_HTML));
}

