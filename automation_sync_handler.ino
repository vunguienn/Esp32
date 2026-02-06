/**
 * ESP32 Automation Sync Handler
 * 
 * This code handles receiving automation data from Cloud via MQTT
 * and stores it in SPIFFS for offline execution.
 * 
 * Features:
 * - Receives automation JSON via MQTT
 * - Validates checksum for data integrity
 * - Stores in SPIFFS with backup
 * - Parses and executes automation rules offline
 * - Sends acknowledgement back to Cloud
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <time.h>
#include <MD5Builder.h>

// ============================================
// CONFIGURATION
// ============================================
#define AUTOMATION_FILE "/automation.json"
#define AUTOMATION_BACKUP "/automation.bak"
#define EXECUTION_LOG "/execution.log"
#define MAX_RULES 20
#define MAX_ACTIONS 5

// Relay GPIO pins
const uint8_t RELAY_PINS[8] = {16, 17, 18, 19, 21, 22, 23, 25};
const char* RELAY_NAMES[8] = {"Light", "FanCirc", "FanExh", "Pump1", "Pump2", "Pump3", "CO2", "AC"};

// ============================================
// DATA STRUCTURES
// ============================================
struct EnvironmentTargets {
  float tempTargetDay;
  int humiHighDay;
  int humiLowDay;
  int co2StartDay;
  int co2StopDay;
  float tempTargetNight;
  int humiHighNight;
  int humiLowNight;
  int co2StartNight;
  int co2StopNight;
  float vpdMin;
  float vpdMax;
};

struct LightingSchedule {
  char lightsOn[6];   // "HH:mm"
  char lightsOff[6];
};

struct AutomationTrigger {
  char type[20];       // TIME, SENSOR_THRESHOLD, SCHEDULE
  char time[6];        // For TIME trigger
  char sensor[20];     // For SENSOR_THRESHOLD
  char op[3];          // >, <, >=, <=, ==
  float value;
  float hysteresis;
};

struct AutomationAction {
  char type[15];       // RELAY_ON, RELAY_OFF, RELAY_TOGGLE
  char relayId[10];    // relay1, relay2, etc.
  int delayMs;
};

struct AutomationRule {
  char id[30];
  char name[50];
  bool enabled;
  int priority;
  AutomationTrigger triggers[3];
  int triggerCount;
  AutomationAction actions[MAX_ACTIONS];
  int actionCount;
  unsigned long lastExecuted;
  int executionsToday;
};

struct IrrigationConfig {
  char id[40];
  char name[50];
  bool enabled;
  char cycleStart[6];
  char cycleEnd[6];
  int pumpDurationSec;
  int restDurationSec;
  char activeDays[7][3];
  char pumpRelays[3][10];
  int pumpCount;
};

// ============================================
// GLOBAL STATE
// ============================================
int automationVersion = 0;
char gatewayId[20] = "";
char roomId[40] = "";
EnvironmentTargets targets;
LightingSchedule lighting;
AutomationRule rules[MAX_RULES];
int ruleCount = 0;
IrrigationConfig irrigation[5];
int irrigationCount = 0;
bool relayStates[8] = {false};
bool relayAutoMode[8] = {true, true, true, true, true, true, true, true};

// Current sensor values (updated from sensors)
float currentTemp = 25.0;
float currentHumi = 60.0;
float currentCo2 = 800.0;
float currentVpd = 1.0;

// MQTT client
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ============================================
// UTILITY FUNCTIONS
// ============================================

/**
 * Calculate MD5 checksum
 */
String calculateMD5(const char* data) {
  MD5Builder md5;
  md5.begin();
  md5.add(data);
  md5.calculate();
  return md5.toString();
}

/**
 * Get current time as HH:mm string
 */
void getCurrentTime(char* buffer) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    strcpy(buffer, "00:00");
    return;
  }
  sprintf(buffer, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
}

/**
 * Compare two time strings
 */
int compareTime(const char* t1, const char* t2) {
  int h1, m1, h2, m2;
  sscanf(t1, "%d:%d", &h1, &m1);
  sscanf(t2, "%d:%d", &h2, &m2);
  int mins1 = h1 * 60 + m1;
  int mins2 = h2 * 60 + m2;
  return mins1 - mins2;
}

/**
 * Check if it's daytime based on lighting schedule
 */
bool isDaytime() {
  char now[6];
  getCurrentTime(now);
  
  // Normal day (e.g., 06:00 - 18:00)
  if (compareTime(lighting.lightsOn, lighting.lightsOff) < 0) {
    return compareTime(now, lighting.lightsOn) >= 0 && 
           compareTime(now, lighting.lightsOff) < 0;
  }
  // Overnight (e.g., 18:00 - 06:00)
  return compareTime(now, lighting.lightsOn) >= 0 || 
         compareTime(now, lighting.lightsOff) < 0;
}

/**
 * Get relay index from relay ID string
 */
int getRelayIndex(const char* relayId) {
  if (strncmp(relayId, "relay", 5) == 0) {
    int idx = atoi(relayId + 5) - 1;
    if (idx >= 0 && idx < 8) return idx;
  }
  return -1;
}

// ============================================
// RELAY CONTROL
// ============================================

void setRelay(int index, bool state, const char* source) {
  if (index < 0 || index >= 8) return;
  if (!relayAutoMode[index] && strcmp(source, "MANUAL") != 0) {
    Serial.printf("⚠️ Relay %d in manual mode, ignoring %s command\n", index + 1, source);
    return;
  }
  
  digitalWrite(RELAY_PINS[index], state ? HIGH : LOW);
  relayStates[index] = state;
  Serial.printf("🔌 Relay %d (%s) = %s [%s]\n", 
    index + 1, RELAY_NAMES[index], state ? "ON" : "OFF", source);
}

void initRelays() {
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }
  Serial.println("✅ Relays initialized");
}

// ============================================
// SPIFFS STORAGE
// ============================================

bool saveAutomationToSPIFFS(const char* jsonData) {
  // First, backup existing file
  if (SPIFFS.exists(AUTOMATION_FILE)) {
    SPIFFS.remove(AUTOMATION_BACKUP);
    SPIFFS.rename(AUTOMATION_FILE, AUTOMATION_BACKUP);
    Serial.println("📁 Backed up existing automation file");
  }
  
  // Write new file
  File file = SPIFFS.open(AUTOMATION_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("❌ Failed to open automation file for writing");
    return false;
  }
  
  size_t written = file.print(jsonData);
  file.close();
  
  if (written > 0) {
    Serial.printf("✅ Saved automation to SPIFFS (%d bytes)\n", written);
    return true;
  }
  
  // Restore backup on failure
  if (SPIFFS.exists(AUTOMATION_BACKUP)) {
    SPIFFS.remove(AUTOMATION_FILE);
    SPIFFS.rename(AUTOMATION_BACKUP, AUTOMATION_FILE);
    Serial.println("⚠️ Restored backup after write failure");
  }
  
  return false;
}

String loadAutomationFromSPIFFS() {
  if (!SPIFFS.exists(AUTOMATION_FILE)) {
    Serial.println("⚠️ No automation file found");
    return "";
  }
  
  File file = SPIFFS.open(AUTOMATION_FILE, FILE_READ);
  if (!file) {
    Serial.println("❌ Failed to open automation file");
    return "";
  }
  
  String content = file.readString();
  file.close();
  
  Serial.printf("📖 Loaded automation from SPIFFS (%d bytes)\n", content.length());
  return content;
}

// ============================================
// AUTOMATION PARSING
// ============================================

bool parseAutomationPayload(const char* jsonData) {
  // Allocate JSON document (adjust size as needed)
  DynamicJsonDocument doc(16384);
  
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
    Serial.printf("❌ JSON parse error: %s\n", error.c_str());
    return false;
  }
  
  // Verify checksum
  String receivedChecksum = doc["checksum"].as<String>();
  doc.remove("checksum");
  
  String dataWithoutChecksum;
  serializeJson(doc, dataWithoutChecksum);
  String calculatedChecksum = calculateMD5(dataWithoutChecksum.c_str());
  
  // Restore checksum for storage
  doc["checksum"] = receivedChecksum;
  
  if (receivedChecksum != calculatedChecksum) {
    Serial.println("❌ Checksum mismatch! Data may be corrupted.");
    Serial.printf("  Received: %s\n", receivedChecksum.c_str());
    Serial.printf("  Calculated: %s\n", calculatedChecksum.c_str());
    return false;
  }
  
  Serial.println("✅ Checksum verified");
  
  // Parse basic info
  automationVersion = doc["version"] | 0;
  strlcpy(gatewayId, doc["gatewayId"] | "", sizeof(gatewayId));
  strlcpy(roomId, doc["roomId"] | "", sizeof(roomId));
  
  // Parse lighting schedule
  strlcpy(lighting.lightsOn, doc["lighting"]["lightsOn"] | "06:00", sizeof(lighting.lightsOn));
  strlcpy(lighting.lightsOff, doc["lighting"]["lightsOff"] | "18:00", sizeof(lighting.lightsOff));
  
  // Parse targets
  JsonObject targetsObj = doc["targets"];
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
  
  // Parse rules
  JsonArray rulesArray = doc["rules"];
  ruleCount = 0;
  
  for (JsonObject ruleObj : rulesArray) {
    if (ruleCount >= MAX_RULES) break;
    
    AutomationRule& rule = rules[ruleCount];
    strlcpy(rule.id, ruleObj["id"] | "", sizeof(rule.id));
    strlcpy(rule.name, ruleObj["name"] | "", sizeof(rule.name));
    rule.enabled = ruleObj["enabled"] | true;
    rule.priority = ruleObj["priority"] | 50;
    rule.lastExecuted = 0;
    rule.executionsToday = 0;
    
    // Parse triggers
    JsonArray triggersArray = ruleObj["triggers"];
    rule.triggerCount = 0;
    
    for (JsonObject triggerObj : triggersArray) {
      if (rule.triggerCount >= 3) break;
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
    
    ruleCount++;
  }
  
  Serial.printf("✅ Parsed automation v%d: %d rules loaded\n", automationVersion, ruleCount);
  return true;
}

// ============================================
// AUTOMATION EXECUTION
// ============================================

bool checkTrigger(const AutomationTrigger& trigger) {
  if (strcmp(trigger.type, "TIME") == 0) {
    char now[6];
    getCurrentTime(now);
    return strcmp(now, trigger.time) == 0;
  }
  
  if (strcmp(trigger.type, "SENSOR_THRESHOLD") == 0) {
    float sensorValue = 0;
    
    if (strcmp(trigger.sensor, "temp") == 0) sensorValue = currentTemp;
    else if (strcmp(trigger.sensor, "humidity") == 0) sensorValue = currentHumi;
    else if (strcmp(trigger.sensor, "co2") == 0) sensorValue = currentCo2;
    else if (strcmp(trigger.sensor, "vpd") == 0) sensorValue = currentVpd;
    
    if (strcmp(trigger.op, ">") == 0) return sensorValue > trigger.value;
    if (strcmp(trigger.op, "<") == 0) return sensorValue < trigger.value;
    if (strcmp(trigger.op, ">=") == 0) return sensorValue >= trigger.value;
    if (strcmp(trigger.op, "<=") == 0) return sensorValue <= trigger.value;
    if (strcmp(trigger.op, "==") == 0) return abs(sensorValue - trigger.value) < trigger.hysteresis;
  }
  
  return false;
}

void executeAction(const AutomationAction& action) {
  if (action.delayMs > 0) {
    delay(action.delayMs);
  }
  
  int relayIdx = getRelayIndex(action.relayId);
  if (relayIdx < 0) return;
  
  if (strcmp(action.type, "RELAY_ON") == 0) {
    setRelay(relayIdx, true, "RULE");
  }
  else if (strcmp(action.type, "RELAY_OFF") == 0) {
    setRelay(relayIdx, false, "RULE");
  }
  else if (strcmp(action.type, "RELAY_TOGGLE") == 0) {
    setRelay(relayIdx, !relayStates[relayIdx], "RULE");
  }
}

void runAutomationRules() {
  for (int i = 0; i < ruleCount; i++) {
    AutomationRule& rule = rules[i];
    if (!rule.enabled) continue;
    
    // Check if any trigger matches
    bool triggered = false;
    for (int t = 0; t < rule.triggerCount; t++) {
      if (checkTrigger(rule.triggers[t])) {
        triggered = true;
        break;
      }
    }
    
    if (triggered) {
      // Prevent rapid re-triggering (5 second cooldown)
      if (millis() - rule.lastExecuted < 5000) continue;
      
      Serial.printf("⚡ Rule triggered: %s\n", rule.name);
      
      // Execute all actions
      for (int a = 0; a < rule.actionCount; a++) {
        executeAction(rule.actions[a]);
      }
      
      rule.lastExecuted = millis();
      rule.executionsToday++;
    }
  }
}

// ============================================
// MQTT SYNC HANDLING
// ============================================

void sendSyncAck(bool success, const char* errorMsg = nullptr) {
  DynamicJsonDocument doc(512);
  doc["gatewayId"] = gatewayId;
  doc["version"] = automationVersion;
  doc["status"] = success ? "OK" : "ERROR";
  doc["timestamp"] = millis();
  
  if (!success && errorMsg) {
    JsonArray errors = doc.createNestedArray("errors");
    JsonObject err = errors.createNestedObject();
    err["code"] = "PARSE_ERROR";
    err["message"] = errorMsg;
  }
  
  // Add storage info
  JsonObject storage = doc.createNestedObject("storage");
  storage["usedBytes"] = SPIFFS.usedBytes();
  storage["freeBytes"] = SPIFFS.totalBytes() - SPIFFS.usedBytes();
  storage["fileName"] = AUTOMATION_FILE;
  
  String payload;
  serializeJson(doc, payload);
  
  char topic[80];
  snprintf(topic, sizeof(topic), "device/%s/automation/ack", gatewayId);
  mqtt.publish(topic, payload.c_str());
  
  Serial.printf("📤 Sent sync ACK: %s\n", success ? "OK" : "ERROR");
}

void handleAutomationUpdate(const char* payload) {
  Serial.println("📥 Received automation update");
  
  // Parse and validate
  if (!parseAutomationPayload(payload)) {
    sendSyncAck(false, "Failed to parse automation payload");
    return;
  }
  
  // Save to SPIFFS
  if (!saveAutomationToSPIFFS(payload)) {
    sendSyncAck(false, "Failed to save to SPIFFS");
    return;
  }
  
  // Send success acknowledgement
  sendSyncAck(true);
  
  Serial.printf("✅ Automation v%d synced and saved!\n", automationVersion);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  // Check if this is automation update
  if (topicStr.endsWith("/automation/update")) {
    handleAutomationUpdate(message.c_str());
  }
}

// ============================================
// SETUP & LOOP
// ============================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n🌱 ESP32 Automation Controller Starting...");
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS mount failed");
    return;
  }
  Serial.println("✅ SPIFFS mounted");
  
  // Initialize relays
  initRelays();
  
  // Load saved automation from SPIFFS
  String savedAutomation = loadAutomationFromSPIFFS();
  if (savedAutomation.length() > 0) {
    if (parseAutomationPayload(savedAutomation.c_str())) {
      Serial.printf("✅ Loaded automation v%d from storage\n", automationVersion);
    }
  }
  
  // TODO: Connect WiFi and MQTT here
  // mqtt.setCallback(mqttCallback);
  // mqtt.subscribe("device/YOUR_MAC/automation/update");
  
  Serial.println("🚀 Automation controller ready!");
}

void loop() {
  // Handle MQTT
  if (mqtt.connected()) {
    mqtt.loop();
  }
  
  // Run automation rules every second
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 1000) {
    lastCheck = millis();
    runAutomationRules();
  }
  
  // TODO: Read sensors and update currentTemp, currentHumi, etc.
}
