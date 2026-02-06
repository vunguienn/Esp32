/**
 * ESP32 Automation Sync Implementation
 * 
 * Receives automation data from Cloud, stores in SPIFFS,
 * and executes automation rules offline.
 */

#include "automation_sync.h"
#include <MD5Builder.h>
#include <time.h>

// Global instance
AutomationSync automationSync;

// ============================================
// CONSTRUCTOR
// ============================================

AutomationSync::AutomationSync() {
  _loaded = false;
  _version = 0;
  _ruleCount = 0;
  _irrigationCount = 0;
  _weeklyPlanCount = 0;
  _relayCallback = nullptr;
  
  // Timezone (default Vietnam UTC+7)
  _timezoneOffset = 25200;  // 7 * 3600 seconds
  memset(_timezoneName, 0, sizeof(_timezoneName));
  strcpy(_timezoneName, "Asia/Ho_Chi_Minh");
  
  // Plant lifecycle
  _plantStartTimestamp = 0;
  _totalWeeks = 0;
  _currentWeek = 1;
  _currentDay = 1;
  memset(_currentPhase, 0, sizeof(_currentPhase));
  strcpy(_currentPhase, "Seedling");
  
  memset(_gatewayId, 0, sizeof(_gatewayId));
  memset(_roomId, 0, sizeof(_roomId));
  memset(_checksum, 0, sizeof(_checksum));
  
  // Default lighting
  strcpy(_lighting.lightsOn, "06:00");
  strcpy(_lighting.lightsOff, "18:00");
  
  // Default targets
  _targets.tempTargetDay = 26.0;
  _targets.humiHighDay = 70;
  _targets.humiLowDay = 60;
  _targets.co2StartDay = 1000;
  _targets.co2StopDay = 1400;
  _targets.tempTargetNight = 20.0;
  _targets.humiHighNight = 65;
  _targets.humiLowNight = 55;
  _targets.co2StartNight = 450;
  _targets.co2StopNight = 600;
  _targets.vpdMin = 0.8;
  _targets.vpdMax = 1.2;
  
  // Init relay states
  for (int i = 0; i < 8; i++) {
    _relayStates[i] = false;
    _relayAutoMode[i] = true;
  }
  
  // Init sensor values
  _currentTemp = 25.0;
  _currentHumi = 60.0;
  _currentCo2 = 800.0;
  _currentVpd = 1.0;
  
  // Init deferred actions queue
  for (int i = 0; i < MAX_DEFERRED_ACTIONS; i++) {
    _deferredActions[i].pending = false;
  }
  
  // Daily reset tracking
  _lastDayReset = -1;
}

// ============================================
// INITIALIZATION
// ============================================

bool AutomationSync::begin() {
  Serial.println("[AutoSync] 🔧 Mounting filesystem...");
  if (!SPIFFS.begin(true)) {
    Serial.println("[AutoSync] ⚠️  SPIFFS mount failed with format=true");
    Serial.println("[AutoSync] 🔄 Attempting mount without format...");
    if (!SPIFFS.begin(false)) {
      Serial.println("[AutoSync] ❌ SPIFFS mount completely failed");
      return false;
    }
  }
  Serial.println("[AutoSync] ✅ SPIFFS mounted successfully");
  Serial.printf("[AutoSync] 📊 Total bytes: %d, Used: %d\n", SPIFFS.totalBytes(), SPIFFS.usedBytes());
  
  // Load saved automation
  String saved = loadFromSPIFFS();
  if (saved.length() > 0) {
    if (parsePayload(saved.c_str())) {
      Serial.printf("[AutoSync] ✅ Loaded automation v%d from storage\n", _version);
      _loaded = true;
    }
  }
  
  return true;
}

// ============================================
// MQTT UPDATE HANDLING
// ============================================

bool AutomationSync::handleUpdate(const char* payload, unsigned int length) {
  Serial.println("\n[AutoSync] ========================================");
  Serial.println("[AutoSync] 🔍 Starting automation update process...");
  Serial.printf("[AutoSync] 📄 Payload length: %d bytes\n", length);
  
  // Parse and validate
  if (!parsePayload(payload)) {
    Serial.println("[AutoSync] ❌ Failed to parse payload");
    Serial.println("[AutoSync] ========================================");
    return false;
  }
  
  Serial.println("[AutoSync] ✅ Parse successful!");
  
  // Save to SPIFFS
  if (!saveToSPIFFS(payload)) {
    Serial.println("[AutoSync] ❌ Failed to save to SPIFFS");
    Serial.println("[AutoSync] ========================================");
    return false;
  }
  
  _loaded = true;
  Serial.printf("[AutoSync] ✅ Automation v%d synced and saved!\n", _version);
  Serial.println("[AutoSync] ========================================\n");
  return true;
}

SyncAckPayload AutomationSync::getAckPayload(bool success, const char* errorCode, const char* errorMsg) {
  SyncAckPayload ack;
  memset(&ack, 0, sizeof(ack));
  
  strncpy(ack.gatewayId, _gatewayId, sizeof(ack.gatewayId) - 1);
  ack.version = _version;
  strcpy(ack.status, success ? "OK" : "ERROR");
  ack.timestamp = millis();
  ack.usedBytes = SPIFFS.usedBytes();
  ack.freeBytes = SPIFFS.totalBytes() - SPIFFS.usedBytes();
  
  if (!success && errorCode) {
    strncpy(ack.errorCode, errorCode, sizeof(ack.errorCode) - 1);
  }
  if (!success && errorMsg) {
    strncpy(ack.errorMessage, errorMsg, sizeof(ack.errorMessage) - 1);
  }
  
  return ack;
}

// ============================================
// SENSOR VALUE UPDATES
// ============================================

void AutomationSync::updateSensorValues(float temp, float humi, float co2, float vpd) {
  _currentTemp = temp;
  _currentHumi = humi;
  _currentCo2 = co2;
  _currentVpd = vpd;
}

// ============================================
// AUTOMATION EXECUTION
// ============================================

void AutomationSync::runRules() {
  if (!_loaded) return;
  
  // Process any pending deferred actions first (non-blocking)
  processDeferredActions();
  
  // Reset daily counters at midnight
  resetDailyCounters();
  
  // Re-calculate current week periodically (every hour)
  static unsigned long lastWeekCheck = 0;
  if (millis() - lastWeekCheck > 3600000 || lastWeekCheck == 0) {  // Every hour
    lastWeekCheck = millis();
    int oldWeek = _currentWeek;
    calculateCurrentWeek();
    if (_currentWeek != oldWeek) {
      selectWeeklyPlan(_currentWeek);
      Serial.printf("[AutoSync] 📅 Week changed: %d → %d\n", oldWeek, _currentWeek);
    }
  }
  
  for (int i = 0; i < _ruleCount; i++) {
    AutomationRule& rule = _rules[i];
    if (!rule.enabled) continue;
    
    // Check max executions per day
    if (rule.maxExecutionsPerDay > 0 && rule.executionsToday >= rule.maxExecutionsPerDay) {
      continue;
    }
    
    // Check cooldown
    if (rule.cooldownMs > 0 && (millis() - rule.lastExecuted) < rule.cooldownMs) {
      continue;
    }
    
    // Check if any trigger matches
    bool triggered = false;
    for (int t = 0; t < rule.triggerCount; t++) {
      if (checkTrigger(rule.triggers[t])) {
        triggered = true;
        break;
      }
    }
    
    if (triggered) {
      // Prevent rapid re-triggering (default 5 second cooldown)
      if (rule.cooldownMs == 0 && (millis() - rule.lastExecuted) < 5000) {
        continue;
      }
      
      Serial.printf("[AutoSync] ⚡ Rule triggered: %s\n", rule.name);
      
      // Execute all actions
      for (int a = 0; a < rule.actionCount; a++) {
        executeAction(rule.actions[a]);
      }
      
      rule.lastExecuted = millis();
      rule.executionsToday++;
    }
  }
}

void AutomationSync::checkLightingSchedule() {
  if (!_loaded) return;
  
  static bool lastLightState = false;
  bool shouldBeOn = isDaytime();
  
  if (shouldBeOn != lastLightState) {
    setRelay(0, shouldBeOn, "SCHEDULE");  // Relay 1 = Light
    lastLightState = shouldBeOn;
  }
}

void AutomationSync::checkEquipmentSchedules() {
  if (!_loaded) return;
  
  // Fan circulation
  if (strcmp(_equipment.fanCircMode, "24H") == 0) {
    setRelay(1, true, "24H");
  }
  else if (strcmp(_equipment.fanCircMode, "OFF") == 0) {
    setRelay(1, false, "OFF");
  }
  else if (strcmp(_equipment.fanCircMode, "TRIGGERED") == 0) {
    bool shouldRun = _currentTemp > _equipment.fanCircTriggerTemp;
    setRelay(1, shouldRun, "TRIGGER");
  }
  
  // Fan exhaust
  if (strcmp(_equipment.fanExhMode, "24H") == 0) {
    setRelay(2, true, "24H");
  }
  else if (strcmp(_equipment.fanExhMode, "OFF") == 0) {
    setRelay(2, false, "OFF");
  }
  else if (strcmp(_equipment.fanExhMode, "TRIGGERED") == 0) {
    bool shouldRun = _currentHumi > _equipment.fanExhTriggerHumi || 
                     _currentVpd > _equipment.fanExhTriggerVpd;
    setRelay(2, shouldRun, "TRIGGER");
  }
}

void AutomationSync::checkIrrigationSchedules() {
  if (!_loaded) return;
  
  char now[6];
  getCurrentTime(now);
  
  for (int i = 0; i < _irrigationCount; i++) {
    IrrigationConfig& irr = _irrigation[i];
    if (!irr.enabled) continue;
    
    // STATE MACHINE: Check if pump is currently running
    if (irr.pumpRunning) {
      unsigned long elapsed = millis() - irr.pumpStartTime;
      if (elapsed >= (unsigned long)(irr.pumpDurationSec * 1000UL)) {
        // Time to turn pump OFF
        for (int p = 0; p < irr.pumpCount; p++) {
          int relayIdx = getRelayIndex(irr.pumpRelays[p]);
          if (relayIdx >= 0) {
            setRelay(relayIdx, false, "IRRIGATION");
          }
        }
        irr.pumpRunning = false;
        irr.lastRun = millis();
        Serial.printf("[AutoSync] 💧 Irrigation cycle complete: %s\n", irr.name);
      }
      continue; // Skip rest of checks while pump running
    }
    
    // Check if today is an active day
    if (!isActiveDayOfWeek(irr.activeDays, irr.activeDaysCount)) {
      continue;
    }
    
    // Check if within cycle time
    bool inCycle = false;
    if (compareTime(irr.cycleStart, irr.cycleEnd) < 0) {
      // Normal range (e.g., 08:00 - 18:00)
      inCycle = compareTime(now, irr.cycleStart) >= 0 && 
                compareTime(now, irr.cycleEnd) < 0;
    } else {
      // Overnight range
      inCycle = compareTime(now, irr.cycleStart) >= 0 || 
                compareTime(now, irr.cycleEnd) < 0;
    }
    
    if (!inCycle) continue;
    
    // Check pump cycle timing (pump duration + rest duration)
    unsigned long cycleDuration = (irr.pumpDurationSec + irr.restDurationSec) * 1000UL;
    unsigned long timeSinceLastRun = millis() - irr.lastRun;
    
    if (timeSinceLastRun >= cycleDuration || irr.lastRun == 0) {
      // Time to START pump (non-blocking)
      for (int p = 0; p < irr.pumpCount; p++) {
        int relayIdx = getRelayIndex(irr.pumpRelays[p]);
        if (relayIdx >= 0) {
          setRelay(relayIdx, true, "IRRIGATION");
        }
      }
      irr.pumpRunning = true;
      irr.pumpStartTime = millis();
      Serial.printf("[AutoSync] 💧 Irrigation started: %s (duration: %ds)\n", 
                    irr.name, irr.pumpDurationSec);
    }
  }
}

// ============================================
// RELAY CONTROL
// ============================================

void AutomationSync::setRelayCallback(void (*callback)(uint8_t, bool, const char*)) {
  _relayCallback = callback;
}

void AutomationSync::setRelayManualMode(uint8_t relay, bool manual) {
  if (relay < 8) {
    _relayAutoMode[relay] = !manual;
    Serial.printf("[AutoSync] Relay %d %s mode\n", relay + 1, manual ? "MANUAL" : "AUTO");
  }
}

void AutomationSync::setRelay(int index, bool state, const char* source) {
  if (index < 0 || index >= 8) return;
  
  // Check if in auto mode (unless manual override)
  if (!_relayAutoMode[index] && strcmp(source, "MANUAL") != 0) {
    return;
  }
  
  // Only trigger if state changed
  if (_relayStates[index] == state) return;
  
  _relayStates[index] = state;
  
  if (_relayCallback) {
    _relayCallback(index, state, source);
  }
  
  Serial.printf("[AutoSync] 🔌 Relay %d = %s [%s]\n", index + 1, state ? "ON" : "OFF", source);
}

// ============================================
// HELPERS
// ============================================

bool AutomationSync::isDaytime() const {
  char now[6];
  const_cast<AutomationSync*>(this)->getCurrentTime(now);
  
  // Normal day (e.g., 06:00 - 18:00)
  if (compareTime(_lighting.lightsOn, _lighting.lightsOff) < 0) {
    return compareTime(now, _lighting.lightsOn) >= 0 && 
           compareTime(now, _lighting.lightsOff) < 0;
  }
  // Overnight (e.g., 18:00 - 06:00)
  return compareTime(now, _lighting.lightsOn) >= 0 || 
         compareTime(now, _lighting.lightsOff) < 0;
}

float AutomationSync::getCurrentTempTarget() const {
  return isDaytime() ? _targets.tempTargetDay : _targets.tempTargetNight;
}

int AutomationSync::getCurrentHumiHigh() const {
  return isDaytime() ? _targets.humiHighDay : _targets.humiHighNight;
}

int AutomationSync::getCurrentHumiLow() const {
  return isDaytime() ? _targets.humiLowDay : _targets.humiLowNight;
}

int AutomationSync::getCurrentCo2Start() const {
  return isDaytime() ? _targets.co2StartDay : _targets.co2StartNight;
}

int AutomationSync::getCurrentCo2Stop() const {
  return isDaytime() ? _targets.co2StopDay : _targets.co2StopNight;
}

void AutomationSync::getCurrentTime(char* buffer) const {
  struct tm timeinfo;
  if (!::getLocalTime(&timeinfo)) {
    strcpy(buffer, "00:00");
    return;
  }
  sprintf(buffer, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
}

void AutomationSync::getLocalTime(char* buffer) const {
  // Same as getCurrentTime - both use configured timezone
  getCurrentTime(buffer);
}

int AutomationSync::getLocalHour() const {
  struct tm timeinfo;
  if (!::getLocalTime(&timeinfo)) {
    return 0;
  }
  return timeinfo.tm_hour;
}

int AutomationSync::getLocalMinute() const {
  struct tm timeinfo;
  if (!::getLocalTime(&timeinfo)) {
    return 0;
  }
  return timeinfo.tm_min;
}

int AutomationSync::compareTime(const char* t1, const char* t2) const {
  int h1, m1, h2, m2;
  sscanf(t1, "%d:%d", &h1, &m1);
  sscanf(t2, "%d:%d", &h2, &m2);
  return (h1 * 60 + m1) - (h2 * 60 + m2);
}

int AutomationSync::getRelayIndex(const char* relayId) const {
  if (strncmp(relayId, "relay", 5) == 0) {
    int idx = atoi(relayId + 5) - 1;
    if (idx >= 0 && idx < 8) return idx;
  }
  return -1;
}

int AutomationSync::parseDayOfWeek(const char* day) const {
  if (strcmp(day, "SU") == 0) return 0;
  if (strcmp(day, "MO") == 0) return 1;
  if (strcmp(day, "TU") == 0) return 2;
  if (strcmp(day, "WE") == 0) return 3;
  if (strcmp(day, "TH") == 0) return 4;
  if (strcmp(day, "FR") == 0) return 5;
  if (strcmp(day, "SA") == 0) return 6;
  return -1;
}

bool AutomationSync::isActiveDayOfWeek(const char days[][3], int count) const {
  struct tm timeinfo;
  if (!::getLocalTime(&timeinfo)) return true; // Default to active
  
  int today = timeinfo.tm_wday; // 0 = Sunday
  for (int i = 0; i < count; i++) {
    if (parseDayOfWeek(days[i]) == today) return true;
  }
  return false;
}

bool AutomationSync::checkTrigger(const AutomationTrigger& trigger) {
  if (strcmp(trigger.type, "TIME") == 0) {
    char now[6];
    getCurrentTime(now);
    return strcmp(now, trigger.time) == 0;
  }
  
  if (strcmp(trigger.type, "SENSOR_THRESHOLD") == 0) {
    float sensorValue = 0;
    
    if (strcmp(trigger.sensor, "temp") == 0) sensorValue = _currentTemp;
    else if (strcmp(trigger.sensor, "humidity") == 0) sensorValue = _currentHumi;
    else if (strcmp(trigger.sensor, "co2") == 0) sensorValue = _currentCo2;
    else if (strcmp(trigger.sensor, "vpd") == 0) sensorValue = _currentVpd;
    
    if (strcmp(trigger.op, ">") == 0) return sensorValue > trigger.value;
    if (strcmp(trigger.op, "<") == 0) return sensorValue < trigger.value;
    if (strcmp(trigger.op, ">=") == 0) return sensorValue >= trigger.value;
    if (strcmp(trigger.op, "<=") == 0) return sensorValue <= trigger.value;
    if (strcmp(trigger.op, "==") == 0) return abs(sensorValue - trigger.value) < trigger.hysteresis;
  }
  
  return false;
}

void AutomationSync::executeAction(const AutomationAction& action) {
  int relayIdx = getRelayIndex(action.relayId);
  if (relayIdx < 0) return;
  
  bool state = false;
  if (strcmp(action.type, "RELAY_ON") == 0) {
    state = true;
  } else if (strcmp(action.type, "RELAY_OFF") == 0) {
    state = false;
  } else if (strcmp(action.type, "RELAY_TOGGLE") == 0) {
    state = !_relayStates[relayIdx];
  } else {
    return;  // Unknown action type
  }
  
  // Use deferred queue for delayed actions (non-blocking)
  if (action.delayMs > 0) {
    queueDeferredAction(relayIdx, state, action.delayMs);
    Serial.printf("[AutoSync] ⏳ Queued relay %d -> %s in %dms\n", 
                  relayIdx + 1, state ? "ON" : "OFF", action.delayMs);
  } else {
    setRelay(relayIdx, state, "RULE");
  }
}

// ============================================
// DEFERRED ACTIONS (Non-blocking delays)
// ============================================

void AutomationSync::queueDeferredAction(int relayIndex, bool state, unsigned long delayMs) {
  // Find empty slot
  for (int i = 0; i < MAX_DEFERRED_ACTIONS; i++) {
    if (!_deferredActions[i].pending) {
      _deferredActions[i].executeAt = millis() + delayMs;
      _deferredActions[i].relayIndex = relayIndex;
      _deferredActions[i].state = state;
      _deferredActions[i].pending = true;
      return;
    }
  }
  Serial.println("[AutoSync] ⚠️ Deferred action queue full!");
}

void AutomationSync::processDeferredActions() {
  unsigned long now = millis();
  
  for (int i = 0; i < MAX_DEFERRED_ACTIONS; i++) {
    if (_deferredActions[i].pending && now >= _deferredActions[i].executeAt) {
      // Execute the deferred action
      setRelay(_deferredActions[i].relayIndex, _deferredActions[i].state, "DEFERRED");
      _deferredActions[i].pending = false;
      Serial.printf("[AutoSync] ✅ Executed deferred: relay %d -> %s\n",
                    _deferredActions[i].relayIndex + 1, 
                    _deferredActions[i].state ? "ON" : "OFF");
    }
  }
}

// ============================================
// DAILY RESET
// ============================================

void AutomationSync::resetDailyCounters() {
  struct tm timeinfo;
  if (!::getLocalTime(&timeinfo)) return;
  
  int today = timeinfo.tm_mday;
  
  if (_lastDayReset != today) {
    // New day - reset all execution counters
    for (int i = 0; i < _ruleCount; i++) {
      _rules[i].executionsToday = 0;
    }
    _lastDayReset = today;
    Serial.printf("[AutoSync] 🌅 New day (%d) - reset execution counters\n", today);
  }
}

// ============================================
// SPIFFS STORAGE
// ============================================

bool AutomationSync::saveToSPIFFS(const char* jsonData) {
  // Check if filesystem is mounted - try to remount if not
  Serial.println("[AutoSync] 🔍 Checking filesystem status...");
  
  // Try to get filesystem info to verify it's mounted
  size_t totalBytes = SPIFFS.totalBytes();
  if (totalBytes == 0) {
    Serial.println("[AutoSync] ⚠️ Filesystem not mounted! Attempting remount...");
    if (!SPIFFS.begin(true)) {
      Serial.println("[AutoSync] ❌ Failed to remount filesystem!");
      return false;
    }
    Serial.println("[AutoSync] ✅ Filesystem remounted successfully");
  }
  
  Serial.printf("[AutoSync] 📝 Attempting to save %d bytes to SPIFFS\n", strlen(jsonData));
  Serial.printf("[AutoSync] 📊 Storage: %d/%d bytes used\n", SPIFFS.usedBytes(), SPIFFS.totalBytes());
  
  // Backup existing file
  if (SPIFFS.exists(AUTOMATION_FILE)) {
    SPIFFS.remove(AUTOMATION_BACKUP);
    SPIFFS.rename(AUTOMATION_FILE, AUTOMATION_BACKUP);
    Serial.println("[AutoSync] 📁 Backed up existing file");
  }
  
  // Write new file
  Serial.printf("[AutoSync] 📂 Opening file: %s\n", AUTOMATION_FILE);
  File file = SPIFFS.open(AUTOMATION_FILE, FILE_WRITE);
  if (!file) {
    Serial.printf("[AutoSync] ❌ Failed to open file for writing: %s\n", AUTOMATION_FILE);
    Serial.printf("[AutoSync] 📊 Free space: %d bytes\n", totalBytes - SPIFFS.usedBytes());
    return false;
  }
  
  size_t written = file.print(jsonData);
  file.close();
  
  if (written > 0) {
    Serial.printf("[AutoSync] ✅ Saved to SPIFFS (%d bytes)\n", written);
    return true;
  }
  
  // Restore backup on failure
  if (SPIFFS.exists(AUTOMATION_BACKUP)) {
    SPIFFS.remove(AUTOMATION_FILE);
    SPIFFS.rename(AUTOMATION_BACKUP, AUTOMATION_FILE);
    Serial.println("[AutoSync] ⚠️ Restored backup after failure");
  }
  
  return false;
}

String AutomationSync::loadFromSPIFFS() {
  if (!SPIFFS.exists(AUTOMATION_FILE)) {
    Serial.println("[AutoSync] ⚠️ No automation file found");
    return "";
  }
  
  File file = SPIFFS.open(AUTOMATION_FILE, FILE_READ);
  if (!file) {
    Serial.println("[AutoSync] ❌ Failed to open file");
    return "";
  }
  
  String content = file.readString();
  file.close();
  
  Serial.printf("[AutoSync] 📖 Loaded from SPIFFS (%d bytes)\n", content.length());
  return content;
}

String AutomationSync::calculateMD5(const char* data) {
  MD5Builder md5;
  md5.begin();
  md5.add(data);
  md5.calculate();
  return md5.toString();
}

// ============================================
// PAYLOAD PARSING
// ============================================

bool AutomationSync::parsePayload(const char* jsonData) {
  // Allocate JSON document (ArduinoJson 7.x auto-sizes)
  JsonDocument doc;
  
  Serial.printf("[AutoSync] 🔍 Attempting to parse JSON, length: %d bytes\n", strlen(jsonData));
  Serial.printf("[AutoSync] 📝 First 100 chars: %.100s...\n", jsonData);
  
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
    Serial.printf("[AutoSync] ❌ JSON parse error: %s\n", error.c_str());
    Serial.printf("[AutoSync] 📊 Error code: %d\n", (int)error.code());
    return false;
  }
  
  Serial.println("[AutoSync] ✅ JSON parsed successfully!");
  
  // Verify checksum
  String receivedChecksum = doc["checksum"].as<String>();
  Serial.printf("[AutoSync] 🔐 Received checksum: %s\n", receivedChecksum.c_str());
  
  // Calculate checksum without the checksum field
  JsonDocument docCopy;
  docCopy.set(doc);
  docCopy.remove("checksum");
  
  String dataWithoutChecksum;
  serializeJson(docCopy, dataWithoutChecksum);
  String calculatedChecksum = calculateMD5(dataWithoutChecksum.c_str());
  
  Serial.printf("[AutoSync] 🔐 Calculated checksum: %s\n", calculatedChecksum.c_str());
  
  if (receivedChecksum != calculatedChecksum) {
    Serial.println("[AutoSync] ❌ Checksum mismatch!");
    Serial.printf("  Received: %s\n", receivedChecksum.c_str());
    Serial.printf("  Calculated: %s\n", calculatedChecksum.c_str());
    Serial.printf("  Data without checksum length: %d bytes\n", dataWithoutChecksum.length());
    return false;
  }
  
  Serial.println("[AutoSync] ✅ Checksum verified");
  
  // Parse basic info
  _version = doc["version"] | 0;
  strlcpy(_gatewayId, doc["gatewayId"] | "", sizeof(_gatewayId));
  strlcpy(_roomId, doc["roomId"] | "", sizeof(_roomId));
  strlcpy(_checksum, receivedChecksum.c_str(), sizeof(_checksum));
  
  // Parse timezone (from Cloud)
  _timezoneOffset = doc["timezoneOffset"] | 25200;  // Default UTC+7 (Vietnam)
  strlcpy(_timezoneName, doc["timezone"] | "Asia/Ho_Chi_Minh", sizeof(_timezoneName));
  
  // Configure system timezone
  configTime(_timezoneOffset, 0, "pool.ntp.org", "time.nist.gov");
  Serial.printf("[AutoSync] 🕐 Timezone: %s (UTC%+ld)\n", 
                _timezoneName, _timezoneOffset / 3600);
  
  // Parse lighting schedule
  strlcpy(_lighting.lightsOn, doc["lighting"]["lightsOn"] | "06:00", sizeof(_lighting.lightsOn));
  strlcpy(_lighting.lightsOff, doc["lighting"]["lightsOff"] | "18:00", sizeof(_lighting.lightsOff));
  
  // Parse targets
  JsonObject targetsObj = doc["targets"];
  _targets.tempTargetDay = targetsObj["tempTargetDay"] | 26.0;
  _targets.humiHighDay = targetsObj["humiHighDay"] | 70;
  _targets.humiLowDay = targetsObj["humiLowDay"] | 60;
  _targets.co2StartDay = targetsObj["co2StartDay"] | 1000;
  _targets.co2StopDay = targetsObj["co2StopDay"] | 1400;
  _targets.tempTargetNight = targetsObj["tempTargetNight"] | 20.0;
  _targets.humiHighNight = targetsObj["humiHighNight"] | 65;
  _targets.humiLowNight = targetsObj["humiLowNight"] | 55;
  _targets.co2StartNight = targetsObj["co2StartNight"] | 450;
  _targets.co2StopNight = targetsObj["co2StopNight"] | 600;
  _targets.vpdMin = targetsObj["vpdMin"] | 0.8;
  _targets.vpdMax = targetsObj["vpdMax"] | 1.2;
  
  // Parse equipment config
  JsonObject equipObj = doc["equipment"];
  strlcpy(_equipment.fanCircMode, equipObj["fanCirculation"]["mode"] | "24H", sizeof(_equipment.fanCircMode));
  strlcpy(_equipment.fanCircOnTime, equipObj["fanCirculation"]["onTime"] | "", sizeof(_equipment.fanCircOnTime));
  strlcpy(_equipment.fanCircOffTime, equipObj["fanCirculation"]["offTime"] | "", sizeof(_equipment.fanCircOffTime));
  _equipment.fanCircTriggerTemp = equipObj["fanCirculation"]["triggerTemp"] | 28.0;
  
  strlcpy(_equipment.fanExhMode, equipObj["fanExhaust"]["mode"] | "24H", sizeof(_equipment.fanExhMode));
  strlcpy(_equipment.fanExhOnTime, equipObj["fanExhaust"]["onTime"] | "", sizeof(_equipment.fanExhOnTime));
  strlcpy(_equipment.fanExhOffTime, equipObj["fanExhaust"]["offTime"] | "", sizeof(_equipment.fanExhOffTime));
  _equipment.fanExhTriggerHumi = equipObj["fanExhaust"]["triggerHumidity"] | 75.0;
  _equipment.fanExhTriggerVpd = equipObj["fanExhaust"]["triggerVpd"] | 1.5;
  
  strlcpy(_equipment.acMode, equipObj["ac"]["mode"] | "OFF", sizeof(_equipment.acMode));
  _equipment.acTargetTemp = equipObj["ac"]["targetTemp"] | 26.0;
  strlcpy(_equipment.acFanSpeed, equipObj["ac"]["fanSpeed"] | "AUTO", sizeof(_equipment.acFanSpeed));
  
  // Parse rules
  JsonArray rulesArray = doc["rules"];
  _ruleCount = 0;
  
  for (JsonObject ruleObj : rulesArray) {
    if (_ruleCount >= MAX_RULES) break;
    
    AutomationRule& rule = _rules[_ruleCount];
    strlcpy(rule.id, ruleObj["id"] | "", sizeof(rule.id));
    strlcpy(rule.name, ruleObj["name"] | "", sizeof(rule.name));
    rule.enabled = ruleObj["enabled"] | true;
    rule.priority = ruleObj["priority"] | 50;
    rule.lastExecuted = 0;
    rule.executionsToday = 0;
    rule.maxExecutionsPerDay = ruleObj["maxExecutionsPerDay"] | 0;
    rule.cooldownMs = ruleObj["cooldownMs"] | 5000;
    
    // Parse triggers
    JsonArray triggersArray = ruleObj["triggers"];
    rule.triggerCount = 0;
    
    for (JsonObject triggerObj : triggersArray) {
      if (rule.triggerCount >= MAX_TRIGGERS) break;
      AutomationTrigger& trigger = rule.triggers[rule.triggerCount];
      strlcpy(trigger.type, triggerObj["type"] | "", sizeof(trigger.type));
      strlcpy(trigger.time, triggerObj["time"] | "", sizeof(trigger.time));
      strlcpy(trigger.sensor, triggerObj["sensor"] | "", sizeof(trigger.sensor));
      strlcpy(trigger.op, triggerObj["operator"] | ">", sizeof(trigger.op));
      trigger.value = triggerObj["value"] | 0.0;
      trigger.hysteresis = triggerObj["hysteresis"] | 0.5;
      rule.triggerCount++;
    }
    
    // Parse actions
    JsonArray actionsArray = ruleObj["actions"];
    rule.actionCount = 0;
    
    for (JsonObject actionObj : actionsArray) {
      if (rule.actionCount >= MAX_ACTIONS) break;
      AutomationAction& action = rule.actions[rule.actionCount];
      strlcpy(action.type, actionObj["type"] | "", sizeof(action.type));
      strlcpy(action.relayId, actionObj["relayId"] | "", sizeof(action.relayId));
      action.delayMs = actionObj["delayMs"] | 0;
      rule.actionCount++;
    }
    
    _ruleCount++;
  }
  
  // Parse irrigation schedules
  JsonArray irrArray = doc["irrigation"];
  _irrigationCount = 0;
  
  for (JsonObject irrObj : irrArray) {
    if (_irrigationCount >= 5) break;
    
    IrrigationConfig& irr = _irrigation[_irrigationCount];
    strlcpy(irr.id, irrObj["id"] | "", sizeof(irr.id));
    strlcpy(irr.name, irrObj["name"] | "", sizeof(irr.name));
    irr.enabled = irrObj["enabled"] | true;
    strlcpy(irr.cycleStart, irrObj["cycleStart"] | "08:00", sizeof(irr.cycleStart));
    strlcpy(irr.cycleEnd, irrObj["cycleEnd"] | "18:00", sizeof(irr.cycleEnd));
    irr.pumpDurationSec = irrObj["pumpDurationSec"] | 30;
    irr.restDurationSec = irrObj["restDurationSec"] | 300;
    irr.lastRun = 0;
    irr.pumpRunning = false;     // State machine init
    irr.pumpStartTime = 0;       // State machine init
    
    // Parse active days
    JsonArray daysArray = irrObj["activeDays"];
    irr.activeDaysCount = 0;
    for (const char* day : daysArray) {
      if (irr.activeDaysCount >= 7) break;
      strlcpy(irr.activeDays[irr.activeDaysCount], day, 3);
      irr.activeDaysCount++;
    }
    
    // Parse pump relays
    JsonArray pumpsArray = irrObj["pumpRelays"];
    irr.pumpCount = 0;
    for (const char* pump : pumpsArray) {
      if (irr.pumpCount >= 3) break;
      strlcpy(irr.pumpRelays[irr.pumpCount], pump, 10);
      irr.pumpCount++;
    }
    
    _irrigationCount++;
  }
  
  // Parse plant lifecycle (for multi-week support)
  const char* plantStartStr = doc["plantStartDate"] | "";
  if (strlen(plantStartStr) > 0) {
    // Parse ISO date string to timestamp
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    // Parse format: "2026-01-15T00:00:00Z" or "2026-01-15T00:00:00.000Z"
    if (sscanf(plantStartStr, "%d-%d-%dT%d:%d:%d", 
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday, 
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) >= 3) {
      tm.tm_year -= 1900;  // Years since 1900
      tm.tm_mon -= 1;      // Months 0-11
      _plantStartTimestamp = mktime(&tm);
      Serial.printf("[AutoSync] 📅 Plant start: %s (ts=%lu)\n", plantStartStr, _plantStartTimestamp);
    }
  }
  
  _totalWeeks = doc["totalWeeks"] | 12;
  
  // Parse weekly plans array
  JsonArray weeklyPlansArray = doc["weeklyPlans"];
  _weeklyPlanCount = 0;
  
  for (JsonObject planObj : weeklyPlansArray) {
    if (_weeklyPlanCount >= MAX_WEEKLY_PLANS) break;
    
    WeeklyPlan& plan = _weeklyPlans[_weeklyPlanCount];
    plan.week = planObj["week"] | 1;
    strlcpy(plan.phase, planObj["phase"] | "Unknown", sizeof(plan.phase));
    
    // Parse targets for this week
    JsonObject targetsObj = planObj["targets"];
    plan.targets.tempTargetDay = targetsObj["tempTargetDay"] | 26.0;
    plan.targets.humiHighDay = targetsObj["humiHighDay"] | 70;
    plan.targets.humiLowDay = targetsObj["humiLowDay"] | 60;
    plan.targets.co2StartDay = targetsObj["co2StartDay"] | 1000;
    plan.targets.co2StopDay = targetsObj["co2StopDay"] | 1400;
    plan.targets.tempTargetNight = targetsObj["tempTargetNight"] | 20.0;
    plan.targets.humiHighNight = targetsObj["humiHighNight"] | 65;
    plan.targets.humiLowNight = targetsObj["humiLowNight"] | 55;
    plan.targets.co2StartNight = targetsObj["co2StartNight"] | 450;
    plan.targets.co2StopNight = targetsObj["co2StopNight"] | 600;
    plan.targets.vpdMin = targetsObj["vpdMin"] | 0.8;
    plan.targets.vpdMax = targetsObj["vpdMax"] | 1.2;
    
    // Parse lighting for this week
    strlcpy(plan.lighting.lightsOn, planObj["lighting"]["lightsOn"] | "06:00", sizeof(plan.lighting.lightsOn));
    strlcpy(plan.lighting.lightsOff, planObj["lighting"]["lightsOff"] | "18:00", sizeof(plan.lighting.lightsOff));
    
    // Parse equipment for this week
    JsonObject equipObj = planObj["equipment"];
    strlcpy(plan.equipment.fanCircMode, equipObj["fanCirculation"]["mode"] | "24H", sizeof(plan.equipment.fanCircMode));
    strlcpy(plan.equipment.fanCircOnTime, equipObj["fanCirculation"]["onTime"] | "", sizeof(plan.equipment.fanCircOnTime));
    strlcpy(plan.equipment.fanCircOffTime, equipObj["fanCirculation"]["offTime"] | "", sizeof(plan.equipment.fanCircOffTime));
    plan.equipment.fanCircTriggerTemp = equipObj["fanCirculation"]["triggerTemp"] | 28.0;
    
    strlcpy(plan.equipment.fanExhMode, equipObj["fanExhaust"]["mode"] | "24H", sizeof(plan.equipment.fanExhMode));
    strlcpy(plan.equipment.fanExhOnTime, equipObj["fanExhaust"]["onTime"] | "", sizeof(plan.equipment.fanExhOnTime));
    strlcpy(plan.equipment.fanExhOffTime, equipObj["fanExhaust"]["offTime"] | "", sizeof(plan.equipment.fanExhOffTime));
    plan.equipment.fanExhTriggerHumi = equipObj["fanExhaust"]["triggerHumidity"] | 75.0;
    plan.equipment.fanExhTriggerVpd = equipObj["fanExhaust"]["triggerVpd"] | 1.5;
    
    strlcpy(plan.equipment.acMode, equipObj["ac"]["mode"] | "OFF", sizeof(plan.equipment.acMode));
    plan.equipment.acTargetTemp = equipObj["ac"]["targetTemp"] | 26.0;
    strlcpy(plan.equipment.acFanSpeed, equipObj["ac"]["fanSpeed"] | "AUTO", sizeof(plan.equipment.acFanSpeed));
    
    _weeklyPlanCount++;
  }
  
  Serial.printf("[AutoSync] ✅ Parsed v%d: %d rules, %d irrigation, %d weekly plans\n", 
                _version, _ruleCount, _irrigationCount, _weeklyPlanCount);
  
  // Calculate current week and select appropriate plan
  if (_plantStartTimestamp > 0) {
    calculateCurrentWeek();
    selectWeeklyPlan(_currentWeek);
  }
  
  return true;
}

// ============================================
// WEEK CALCULATION
// ============================================

void AutomationSync::calculateCurrentWeek() {
  if (_plantStartTimestamp == 0) {
    _currentWeek = 1;
    _currentDay = 1;
    return;
  }
  
  // Get current time
  time_t now = time(nullptr);
  
  // If time not synced yet (before 2020), use default
  if (now < 1577836800) {  // Jan 1, 2020
    Serial.println("[AutoSync] ⚠️ Time not synced, using week 1");
    _currentWeek = 1;
    _currentDay = 1;
    return;
  }
  
  // Calculate days since plant start
  long elapsedSeconds = now - _plantStartTimestamp;
  int daysSinceStart = elapsedSeconds / 86400;  // 24*60*60
  
  if (daysSinceStart < 0) {
    // Plant hasn't started yet
    _currentWeek = 1;
    _currentDay = 1;
    Serial.println("[AutoSync] 📅 Plant hasn't started yet");
    return;
  }
  
  // Calculate week (1-based) and day within week (1-7)
  _currentWeek = (daysSinceStart / 7) + 1;
  _currentDay = (daysSinceStart % 7) + 1;
  
  // Cap at total weeks
  if (_currentWeek > _totalWeeks && _totalWeeks > 0) {
    _currentWeek = _totalWeeks;
  }
  
  Serial.printf("[AutoSync] 📅 Current: Week %d, Day %d (days since start: %d)\n", 
                _currentWeek, _currentDay, daysSinceStart);
}

void AutomationSync::selectWeeklyPlan(int week) {
  // Find the plan for this week
  WeeklyPlan* selectedPlan = nullptr;
  
  for (int i = 0; i < _weeklyPlanCount; i++) {
    if (_weeklyPlans[i].week == week) {
      selectedPlan = &_weeklyPlans[i];
      break;
    }
  }
  
  // If no exact match, find the closest plan (same or earlier week)
  if (!selectedPlan && _weeklyPlanCount > 0) {
    int closestWeek = 0;
    for (int i = 0; i < _weeklyPlanCount; i++) {
      if (_weeklyPlans[i].week <= week && _weeklyPlans[i].week > closestWeek) {
        closestWeek = _weeklyPlans[i].week;
        selectedPlan = &_weeklyPlans[i];
      }
    }
  }
  
  // Fallback to first plan
  if (!selectedPlan && _weeklyPlanCount > 0) {
    selectedPlan = &_weeklyPlans[0];
  }
  
  if (selectedPlan) {
    // Copy settings from selected plan to active settings
    _lighting = selectedPlan->lighting;
    _targets = selectedPlan->targets;
    _equipment = selectedPlan->equipment;
    strlcpy(_currentPhase, selectedPlan->phase, sizeof(_currentPhase));
    
    Serial.printf("[AutoSync] 🎯 Selected plan for week %d: %s\n", 
                  selectedPlan->week, selectedPlan->phase);
    Serial.printf("  Lights: %s - %s\n", _lighting.lightsOn, _lighting.lightsOff);
    Serial.printf("  Temp target day: %.1f°C\n", _targets.tempTargetDay);
  } else {
    Serial.println("[AutoSync] ⚠️ No weekly plan found, using defaults");
  }
}
