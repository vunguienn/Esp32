/**
 * ESP32 Automation Sync Implementation
 * 
 * Receives automation data from Cloud, stores in SPIFFS,
 * and executes automation rules offline.
 */

#include "automation_sync.h"
#include <MD5Builder.h>
#include <time.h>
#include <Preferences.h>

// Global instance
AutomationSync automationSync;

// Lifecycle override storage
static const char* LIFECYCLE_NS = "lifecycle";
static const char* KEY_LIFECYCLE_ENABLED = "enabled";
static const char* KEY_LIFECYCLE_BASE_TS = "baseTs";
static const char* KEY_LIFECYCLE_BASE_DAY = "baseDay";
static const char* KEY_LIFECYCLE_BASE_WEEK = "baseWeek";
static const char* KEY_LIFECYCLE_BASE_DOW = "baseDow";
static const char* KEY_LIFECYCLE_PHASE = "phase";

static const char* LICENSE_NS = "license";
static const char* KEY_LICENSE_ACTIVE = "active";
static const char* KEY_LICENSE_EXPIRES = "expires";
static const char* KEY_LICENSE_LAST_SYNC = "lastSync";
static const char* KEY_LICENSE_GRACE = "grace";

// ============================================
// CONSTRUCTOR
// ============================================

AutomationSync::AutomationSync() {
  _loaded = false;
  _version = 0;
  _ruleCount = 0;
  _irrigationCount = 0;
  _weeklyPlanCount = 0;
  _phaseCount = 0;
  _relayCallback = nullptr;
  
  // Timezone (default Vietnam UTC+7)
  _timezoneOffset = 25200;  // 7 * 3600 seconds
  memset(_timezoneName, 0, sizeof(_timezoneName));
  strcpy(_timezoneName, "Asia/Ho_Chi_Minh");
  
  // Plant lifecycle
  _plantStartTimestamp = 0;
  _totalWeeks = 0;
  _currentWeek = 1;
  _currentWeekInPhase = 1;  // Week within current phase
  _currentDay = 1;
  _currentProjectDay = 1;
  memset(_currentPhase, 0, sizeof(_currentPhase));
  strcpy(_currentPhase, "SEEDING");

  _lifecycleOverrideEnabled = false;
  _lifecycleBaseTimestamp = 0;
  _lifecycleBaseProjectDay = 1;
  _lifecycleBaseWeek = 1;
  _lifecycleBaseDayInWeek = 1;
  memset(_lifecyclePhaseOverride, 0, sizeof(_lifecyclePhaseOverride));

  _licenseActive = false;
  _licenseExpiresAt = 0;
  _lastCloudSync = 0;
  _offlineGraceDays = 180;

  _sensorOverrideEnabled = false;
  _sensorOverrideTemp = 0.0f;
  _sensorOverrideHumi = 0.0f;
  _sensorOverrideCo2 = 0.0f;
  _sensorOverrideVpd = 0.0f;
  
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

  loadLifecycleOverride();
  loadLicenseState();
  
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
  if (_sensorOverrideEnabled) {
    _currentTemp = _sensorOverrideTemp;
    _currentHumi = _sensorOverrideHumi;
    _currentCo2 = _sensorOverrideCo2;
    _currentVpd = _sensorOverrideVpd;
    return;
  }

  _currentTemp = temp;
  _currentHumi = humi;
  _currentCo2 = co2;
  _currentVpd = vpd;
}

void AutomationSync::setSensorOverride(float temp, float humi, float co2, float vpd) {
  _sensorOverrideEnabled = true;
  _sensorOverrideTemp = temp;
  _sensorOverrideHumi = humi;
  _sensorOverrideCo2 = co2;
  _sensorOverrideVpd = vpd;

  _currentTemp = temp;
  _currentHumi = humi;
  _currentCo2 = co2;
  _currentVpd = vpd;
}

void AutomationSync::clearSensorOverride() {
  _sensorOverrideEnabled = false;
}

// ============================================
// AUTOMATION EXECUTION
// ============================================

void AutomationSync::runRules() {
  if (!_loaded || !isAutomationAllowed()) return;
  
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
  if (!_loaded || !isAutomationAllowed()) return;
  
  static bool lastLightState = false;
  bool shouldBeOn = isDaytime();
  
  if (shouldBeOn != lastLightState) {
    setRelay(0, shouldBeOn, "SCHEDULE");  // Relay 1 = Light
    lastLightState = shouldBeOn;
  }
}

void AutomationSync::checkEquipmentSchedules() {
  if (!_loaded || !isAutomationAllowed()) return;
  
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
  if (!_loaded || !isAutomationAllowed()) return;
  
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

static unsigned long parseIsoTimestamp(const char* iso) {
  if (!iso || strlen(iso) < 10) return 0;
  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  if (sscanf(iso, "%d-%d-%dT%d:%d:%d",
             &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
             &tm.tm_hour, &tm.tm_min, &tm.tm_sec) < 3) {
    return 0;
  }
  tm.tm_year -= 1900;
  tm.tm_mon -= 1;
  time_t ts = mktime(&tm);
  if (ts < 0) return 0;
  return (unsigned long)ts;
}

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

  // Parse license info (from Cloud)
  JsonObject licenseObj = doc["license"];
  if (!licenseObj.isNull()) {
    bool wasActive = _licenseActive;
    _licenseActive = licenseObj["active"] | true;
    const char* expiresAt = licenseObj["expiresAt"] | "";
    unsigned long expiresTs = parseIsoTimestamp(expiresAt);
    if (expiresTs > 0) {
      _licenseExpiresAt = expiresTs;
    }
    _offlineGraceDays = licenseObj["offlineGraceDays"] | _offlineGraceDays;
    if (_offlineGraceDays < 0) _offlineGraceDays = 0;

    time_t now = time(nullptr);
    if (now >= 1577836800) {
      _lastCloudSync = (unsigned long)now;
    }
    saveLicenseState();

    if (!wasActive && _licenseActive) {
      resetLifecycleForNewLicense();
    }
  }
  
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
  
  _totalWeeks = doc["totalWeeks"] | 16;
  _currentWeek = doc["currentWeek"] | 1;
  
  // Parse nested weekly plans structure (SEEDING/VEG/FLOWER/HARVEST -> weeks)
  if (doc.containsKey("weeklyPlans")) {
    if (!parseNestedWeeklyPlans(doc["weeklyPlans"])) {
      Serial.println("[AutoSync] ❌ Failed to parse nested weeklyPlans");
      return false;
    }
  }
  
  // Calculate current phase and week info based on currentWeek
  updateCurrentWeekInfo();
  
  Serial.printf("[AutoSync] ✅ Parsed v%d: %d rules, %d irrigation, current week: %d, phase: %s\n", 
                _version, _ruleCount, _irrigationCount, _currentWeek, _currentPhase);
  
  return true;
}

// ============================================
// WEEK CALCULATION
// ============================================

void AutomationSync::calculateCurrentWeek() {
  if (_lifecycleOverrideEnabled) {
    applyLifecycleOverride();
    return;
  }
  if (_plantStartTimestamp == 0) {
    _currentWeek = 1;
    _currentDay = 1;
    _currentProjectDay = 1;
    return;
  }
  
  // Get current time
  time_t now = time(nullptr);
  
  // If time not synced yet (before 2020), use default
  if (now < 1577836800) {  // Jan 1, 2020
    Serial.println("[AutoSync] ⚠️ Time not synced, using week 1");
    _currentWeek = 1;
    _currentDay = 1;
    _currentProjectDay = 1;
    return;
  }
  
  // Calculate days since plant start
  long elapsedSeconds = now - _plantStartTimestamp;
  int daysSinceStart = elapsedSeconds / 86400;  // 24*60*60
  
  if (daysSinceStart < 0) {
    // Plant hasn't started yet
    _currentWeek = 1;
    _currentDay = 1;
    _currentProjectDay = 1;
    Serial.println("[AutoSync] 📅 Plant hasn't started yet");
    return;
  }
  
  // Calculate week (1-based) and day within week (1-7)
  _currentWeek = (daysSinceStart / 7) + 1;
  _currentDay = (daysSinceStart % 7) + 1;
  _currentProjectDay = daysSinceStart + 1;
  
  // Cap at total weeks
  if (_currentWeek > _totalWeeks && _totalWeeks > 0) {
    _currentWeek = _totalWeeks;
  }
  
  Serial.printf("[AutoSync] 📅 Current: Week %d, Day %d (days since start: %d)\n", 
                _currentWeek, _currentDay, daysSinceStart);
}

bool AutomationSync::isAutomationAllowed() const {
  if (!_licenseActive) return false;

  time_t now = time(nullptr);
  if (now >= 1577836800) {
    if (_licenseExpiresAt > 0 && now > (time_t)_licenseExpiresAt) {
      return false;
    }

    if (_lastCloudSync > 0 && _offlineGraceDays > 0) {
      long daysOffline = (now - (time_t)_lastCloudSync) / 86400;
      if (daysOffline > _offlineGraceDays) {
        return false;
      }
    }
  }

  return true;
}

void AutomationSync::loadLicenseState() {
  Preferences prefs;
  prefs.begin(LICENSE_NS, true);
  _licenseActive = prefs.getBool(KEY_LICENSE_ACTIVE, _licenseActive);
  _licenseExpiresAt = prefs.getULong(KEY_LICENSE_EXPIRES, _licenseExpiresAt);
  _lastCloudSync = prefs.getULong(KEY_LICENSE_LAST_SYNC, _lastCloudSync);
  _offlineGraceDays = prefs.getInt(KEY_LICENSE_GRACE, _offlineGraceDays);
  prefs.end();
}

void AutomationSync::saveLicenseState() {
  Preferences prefs;
  prefs.begin(LICENSE_NS, false);
  prefs.putBool(KEY_LICENSE_ACTIVE, _licenseActive);
  prefs.putULong(KEY_LICENSE_EXPIRES, _licenseExpiresAt);
  prefs.putULong(KEY_LICENSE_LAST_SYNC, _lastCloudSync);
  prefs.putInt(KEY_LICENSE_GRACE, _offlineGraceDays);
  prefs.end();
}

bool AutomationSync::setLifecycleOverride(int projectDay, int currentWeek, const char* phaseName) {
  if (projectDay < 1) return false;

  int dayInWeek = ((projectDay - 1) % 7) + 1;
  int weekFromDay = ((projectDay - 1) / 7) + 1;

  if (currentWeek <= 0) {
    currentWeek = weekFromDay;
  }

  if (phaseName && strlen(phaseName) > 0 && _phaseCount > 0) {
    for (int i = 0; i < _phaseCount; i++) {
      if (strcmp(_phaseWeeks[i].phaseName, phaseName) == 0) {
        if (currentWeek < _phaseWeeks[i].startGlobalWeek) {
          currentWeek = _phaseWeeks[i].startGlobalWeek;
        } else if (currentWeek > _phaseWeeks[i].endGlobalWeek) {
          currentWeek = _phaseWeeks[i].endGlobalWeek;
        }
        break;
      }
    }
  }

  projectDay = (currentWeek - 1) * 7 + dayInWeek;

  time_t now = time(nullptr);
  if (now < 1577836800) {
    now = 0;
  }

  _lifecycleOverrideEnabled = true;
  _lifecycleBaseTimestamp = (unsigned long)now;
  _lifecycleBaseProjectDay = projectDay;
  _lifecycleBaseWeek = currentWeek;
  _lifecycleBaseDayInWeek = dayInWeek;
  memset(_lifecyclePhaseOverride, 0, sizeof(_lifecyclePhaseOverride));
  if (phaseName && strlen(phaseName) > 0) {
    strlcpy(_lifecyclePhaseOverride, phaseName, sizeof(_lifecyclePhaseOverride));
  }

  saveLifecycleOverride();
  calculateCurrentWeek();
  selectWeeklyPlan(_currentWeek);
  return true;
}

bool AutomationSync::clearLifecycleOverride() {
  _lifecycleOverrideEnabled = false;
  _lifecycleBaseTimestamp = 0;
  _lifecycleBaseProjectDay = 1;
  _lifecycleBaseWeek = 1;
  _lifecycleBaseDayInWeek = 1;
  memset(_lifecyclePhaseOverride, 0, sizeof(_lifecyclePhaseOverride));

  saveLifecycleOverride();
  calculateCurrentWeek();
  selectWeeklyPlan(_currentWeek);
  return true;
}

void AutomationSync::loadLifecycleOverride() {
  Preferences prefs;
  prefs.begin(LIFECYCLE_NS, true);
  _lifecycleOverrideEnabled = prefs.getBool(KEY_LIFECYCLE_ENABLED, false);
  _lifecycleBaseTimestamp = prefs.getULong(KEY_LIFECYCLE_BASE_TS, 0);
  _lifecycleBaseProjectDay = prefs.getInt(KEY_LIFECYCLE_BASE_DAY, 1);
  _lifecycleBaseWeek = prefs.getInt(KEY_LIFECYCLE_BASE_WEEK, 1);
  _lifecycleBaseDayInWeek = prefs.getInt(KEY_LIFECYCLE_BASE_DOW, 1);
  String phase = prefs.getString(KEY_LIFECYCLE_PHASE, "");
  prefs.end();

  memset(_lifecyclePhaseOverride, 0, sizeof(_lifecyclePhaseOverride));
  if (phase.length() > 0) {
    strlcpy(_lifecyclePhaseOverride, phase.c_str(), sizeof(_lifecyclePhaseOverride));
  }
}

void AutomationSync::saveLifecycleOverride() {
  Preferences prefs;
  prefs.begin(LIFECYCLE_NS, false);
  prefs.putBool(KEY_LIFECYCLE_ENABLED, _lifecycleOverrideEnabled);
  prefs.putULong(KEY_LIFECYCLE_BASE_TS, _lifecycleBaseTimestamp);
  prefs.putInt(KEY_LIFECYCLE_BASE_DAY, _lifecycleBaseProjectDay);
  prefs.putInt(KEY_LIFECYCLE_BASE_WEEK, _lifecycleBaseWeek);
  prefs.putInt(KEY_LIFECYCLE_BASE_DOW, _lifecycleBaseDayInWeek);
  prefs.putString(KEY_LIFECYCLE_PHASE, _lifecyclePhaseOverride);
  prefs.end();
}

void AutomationSync::applyLifecycleOverride() {
  time_t now = time(nullptr);
  int deltaDays = 0;
  if (_lifecycleBaseTimestamp > 0 && now >= 1577836800) {
    deltaDays = (int)((now - _lifecycleBaseTimestamp) / 86400);
  }

  int baseOffset = _lifecycleBaseDayInWeek - 1 + deltaDays;
  _currentDay = (baseOffset % 7) + 1;
  _currentWeek = _lifecycleBaseWeek + (baseOffset / 7);
  _currentProjectDay = _lifecycleBaseProjectDay + deltaDays;

  updateCurrentWeekInfo();

  if (strlen(_lifecyclePhaseOverride) > 0) {
    for (int i = 0; i < _phaseCount; i++) {
      if (strcmp(_phaseWeeks[i].phaseName, _lifecyclePhaseOverride) == 0) {
        strlcpy(_currentPhase, _lifecyclePhaseOverride, sizeof(_currentPhase));
        int weekInPhase = _currentWeek - _phaseWeeks[i].startGlobalWeek + 1;
        if (weekInPhase < 1) weekInPhase = 1;
        if (weekInPhase > _phaseWeeks[i].weekCount) weekInPhase = _phaseWeeks[i].weekCount;
        _currentWeekInPhase = weekInPhase;
        return;
      }
    }

    strlcpy(_currentPhase, _lifecyclePhaseOverride, sizeof(_currentPhase));
    _currentWeekInPhase = 1;
  }
}

void AutomationSync::resetLifecycleForNewLicense() {
  setLifecycleOverride(1, 1, "SEEDING");
  Serial.println("[AutoSync] 🔁 License re-activated: lifecycle reset to Day 1 / SEEDING");
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
// ============================================
// PARSE NESTED WEEKLY PLANS (JSON v3)
// ============================================

bool AutomationSync::parseNestedWeeklyPlans(JsonObject weeklyPlansObj) {
  Serial.println("\n[AutoSync] 📋 Parsing nested weeklyPlans (JSON v3)...");
  
  _phaseCount = 0;
  _weeklyPlanCount = 0;
  int globalWeek = 1;  // Running counter for global week
  
  // Phase order (as they appear in lifecycle)
  const char* phases[] = {"SEEDING", "VEG", "FLOWER", "HARVEST"};
  
  // ========================================
  // Parse each phase
  // ========================================
  
  for (int p = 0; p < 4; p++) {
    const char* phaseName = phases[p];
    
    // Skip if phase not in JSON
    if (!weeklyPlansObj.containsKey(phaseName)) {
      Serial.printf("[AutoSync] ⏭️  Phase '%s' not found\n", phaseName);
      continue;
    }
    
    JsonObject phaseObj = weeklyPlansObj[phaseName];
    PhaseWeeks& phase = _phaseWeeks[_phaseCount];
    
    // Initialize phase structure
    strlcpy(phase.phaseName, phaseName, sizeof(phase.phaseName));
    phase.weekCount = 0;
    phase.startGlobalWeek = globalWeek;
    
    Serial.printf("[AutoSync] 📦 Phase: %s\n", phaseName);
    
    // ========================================
    // Parse each week in this phase
    // ========================================
    
    for (JsonPair weekPair : phaseObj) {
      if (phase.weekCount >= 10) {
        Serial.printf("[AutoSync] ⚠️  Phase %s has >10 weeks, skipping rest\n", phaseName);
        break;
      }
      
      const char* weekKey = weekPair.key().c_str();  // "1", "2", "3"...
      int weekInPhase = atoi(weekKey);
      JsonObject weekObj = weekPair.value();
      
      WeeklyPlan& plan = phase.weeks[phase.weekCount];
      
      // Set week identifiers
      plan.week = weekInPhase;           // Within phase: 1-8
      plan.globalWeek = globalWeek;      // Global: 1-16
      strlcpy(plan.phase, phaseName, sizeof(plan.phase));
      
      Serial.printf("[AutoSync]   📅 Week %d/%d (Phase: %s, Global: %d)\n",
                    weekInPhase, 
                    weekKey[1] ? 100 : weekInPhase,  // Crude multi-week detect
                    phaseName, globalWeek);
      
      // ========================================
      // Parse TARGETS for this week
      // ========================================
      
      JsonObject targetsObj = weekObj["targets"];
      EnvironmentTargets& targets = plan.targets;
      
      targets.tempTargetDay = targetsObj["tempTargetDay"] | 26.0;
      targets.humiHighDay = targetsObj["humiHighDay"] | 70;
      targets.humiLowDay = targetsObj["humiLowDay"] | 60;
      targets.co2StartDay = targetsObj["co2StartDay"] | 1000;
      targets.co2StopDay = targetsObj["co2StopDay"] | 1400;
      targets.tempTargetNight = targetsObj["tempTargetNight"] | 20.0;
      targets.humiHighNight = targetsObj["humiHighNight"] | 65;
      targets.humiLowNight = targetsObj["humiLowNight"] | 55;
      targets.co2StartNight = targetsObj["co2StartNight"] | 450;
      targets.co2StopNight = targetsObj["co2StopNight"] | 600;
      targets.vpdMin = targetsObj["vpdMin"] | 0.8;
      targets.vpdMax = targetsObj["vpdMax"] | 1.2;
      strlcpy(targets.humidityMode, 
              targetsObj["humidityMode"] | "DEHUMIDIFY", 
              sizeof(targets.humidityMode));
      
      Serial.printf("[AutoSync]     🌡️ Targets: %d°C day, %d°C night, Mode: %s\n",
                    (int)targets.tempTargetDay, (int)targets.tempTargetNight,
                    targets.humidityMode);
      
      // ========================================
      // Parse LIGHTING for this week
      // ========================================
      
      JsonObject lightingObj = weekObj["lighting"];
      LightingSchedule& lighting = plan.lighting;
      
      strlcpy(lighting.lightsOn, 
              lightingObj["lightsOn"] | "06:00", 
              sizeof(lighting.lightsOn));
      strlcpy(lighting.lightsOff, 
              lightingObj["lightsOff"] | "18:00", 
              sizeof(lighting.lightsOff));
      
      // Parse PWM dimming schedule (0-10 points per day)
      JsonArray scheduleArray = lightingObj["schedule"];
      lighting.scheduleCount = 0;
      
      for (JsonObject schedPoint : scheduleArray) {
        if (lighting.scheduleCount >= 10) break;
        
        PWMLightingPoint& point = lighting.schedule[lighting.scheduleCount];
        
        strlcpy(point.time, schedPoint["time"] | "06:00", sizeof(point.time));
        point.brightness = schedPoint["brightness"] | 100;
        
        JsonObject channels = schedPoint["channels"];
        point.ch1 = channels["ch1"] | 0;  // White (0-100%)
        point.ch2 = channels["ch2"] | 0;  // Yellow (0-100%)
        point.ch3 = channels["ch3"] | 0;  // Red (0-100%)
        
        lighting.scheduleCount++;
      }
      
      if (lighting.scheduleCount > 0) {
        Serial.printf("[AutoSync]     💡 Lighting: %s-%s, %d PWM points\n",
                      lighting.lightsOn, lighting.lightsOff, 
                      lighting.scheduleCount);
      }
      
      // ========================================
      // Parse EQUIPMENT for this week
      // ========================================
      
      JsonObject equipObj = weekObj["equipment"];
      EquipmentConfig& equipment = plan.equipment;
      
      strlcpy(equipment.fanCircMode, 
              equipObj["fanCirculation"]["mode"] | "24H", 
              sizeof(equipment.fanCircMode));
      strlcpy(equipment.fanExhMode, 
              equipObj["fanExhaust"]["mode"] | "24H", 
              sizeof(equipment.fanExhMode));
      strlcpy(equipment.acMode, 
              equipObj["ac"]["mode"] | "OFF", 
              sizeof(equipment.acMode));
      equipment.acTargetTemp = equipObj["ac"]["targetTemp"] | 26.0;
      strlcpy(equipment.acFanSpeed, 
              equipObj["ac"]["fanSpeed"] | "AUTO", 
              sizeof(equipment.acFanSpeed));
      
      Serial.printf("[AutoSync]     ⚙️  Equipment: FanCirc=%s, FanExh=%s, AC=%s\n",
                    equipment.fanCircMode, equipment.fanExhMode, equipment.acMode);
      
      // ========================================
      // Parse IRRIGATION for this week
      // ========================================
      
      JsonArray irrArray = weekObj["irrigation"];
      
      // Store irrigation count for this week (optional, for UI display)
      int weekIrrigationCount = 0;
      for (JsonObject irrObj : irrArray) {
        weekIrrigationCount++;
      }
      
      if (weekIrrigationCount > 0) {
        Serial.printf("[AutoSync]     💧 Irrigation: %d schedules\n", weekIrrigationCount);
        
        // Parse first irrigation of this week (if multiple, Cloud should consolidate)
        if (irrArray.size() > 0) {
          JsonObject firstIrr = irrArray[0];
          // Store for this week's use
          // (Full irrigation parsing happens at root level or per-week)
        }
      }
      
      // ========================================
      // Add to flattened array (for compatibility)
      // ========================================
      
      if (_weeklyPlanCount < MAX_WEEKLY_PLANS) {
        _weeklyPlans[_weeklyPlanCount] = plan;
        _weeklyPlanCount++;
      }
      
      phase.weekCount++;
      globalWeek++;
    }
    
    phase.endGlobalWeek = globalWeek - 1;
    
    Serial.printf("[AutoSync] ✅ Phase %s: %d weeks (global weeks %d-%d)\n",
                  phaseName, phase.weekCount, 
                  phase.startGlobalWeek, phase.endGlobalWeek);
    
    _phaseCount++;
  }
  
  Serial.printf("[AutoSync] ✅ Parsed %d phases, %d total weeks\n\n", 
                _phaseCount, _weeklyPlanCount);
  
  return true;
}

// ============================================
// UPDATE CURRENT WEEK INFO
// ============================================

void AutomationSync::updateCurrentWeekInfo() {
  // Use currentWeek from JSON (Trust Cloud)
  _currentWeek = _currentWeek;  // Already set by Cloud in parsePayload()
  _currentWeekInPhase = 1;       // Will calculate below
  
  // Find which phase contains current week
  for (int p = 0; p < _phaseCount; p++) {
    const PhaseWeeks& phase = _phaseWeeks[p];
    
    if (_currentWeek >= phase.startGlobalWeek && 
        _currentWeek <= phase.endGlobalWeek) {
      
      // Found the phase!
      _currentWeekInPhase = _currentWeek - phase.startGlobalWeek + 1;
      strlcpy(_currentPhase, phase.phaseName, sizeof(_currentPhase));
      
      Serial.printf("[AutoSync] 📍 Current Week: %s Week %d / %d (Global Week %d / %d)\n",
                    phase.phaseName, _currentWeekInPhase, phase.weekCount,
                    _currentWeek, _totalWeeks);
      
      return;
    }
  }
  
  // Fallback (shouldn't happen)
  Serial.printf("[AutoSync] ⚠️  Current week %d out of range [1-%d]\n", 
                _currentWeek, _totalWeeks);
}

// ============================================
// GET CURRENT WEEK PLAN
// ============================================

const WeeklyPlan* AutomationSync::getCurrentWeekPlan() const {
  // Return plan for current global week
  if (_currentWeek >= 1 && _currentWeek <= _weeklyPlanCount) {
    return &_weeklyPlans[_currentWeek - 1];
  }
  return nullptr;
}

// ============================================
// GET CURRENT TARGETS (for sensor evaluation)
// ============================================

const EnvironmentTargets* AutomationSync::getCurrentTargets() const {
  const WeeklyPlan* plan = getCurrentWeekPlan();
  return plan ? &plan->targets : nullptr;
}

// ============================================
// GET CURRENT LIGHTING SCHEDULE
// ============================================

const LightingSchedule* AutomationSync::getCurrentLightingSchedule() const {
  const WeeklyPlan* plan = getCurrentWeekPlan();
  return plan ? &plan->lighting : nullptr;
}

// ============================================
// GET CURRENT EQUIPMENT CONFIG
// ============================================

const EquipmentConfig* AutomationSync::getCurrentEquipmentConfig() const {
  const WeeklyPlan* plan = getCurrentWeekPlan();
  return plan ? &plan->equipment : nullptr;
}