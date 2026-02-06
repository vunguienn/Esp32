/**
 * MQTT Handler Implementation
 */

#include <WiFi.h>
#include <SPIFFS.h>
#include <new>  // For std::nothrow
#include "mqtt_handler.h"

// Global instance
MQTTHandler mqttHandler;

// Static callback wrapper
static MQTTHandler* _instance = nullptr;

void MQTTHandler::staticCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        _instance->handleMessage(topic, payload, length);
    }
}

MQTTHandler::MQTTHandler() {
    _mqttClient = nullptr;
    _wifiClient = nullptr;
    _lastReconnectAttempt = 0;
    _relayCallback = nullptr;
    _verifyCallback = nullptr;
    _automationCallback = nullptr;
    
    _mqttPort = DEFAULT_MQTT_PORT;
    _mqttServer = DEFAULT_MQTT_SERVER;
    _instance = this;
}

void MQTTHandler::begin(WiFiClient& wifiClient) {
    _wifiClient = &wifiClient;
    _mqttClient = new PubSubClient(*_wifiClient);
    
    loadConfig();
    
    _mqttClient->setServer(_mqttServer.c_str(), _mqttPort);
    _mqttClient->setCallback(staticCallback);
    _mqttClient->setBufferSize(16384);  // 16KB buffer for large automation payload (~10.5KB)
    _mqttClient->setKeepAlive(MQTT_KEEPALIVE);
    
    Serial.printf("[MQTT] 🔧 Configured server: %s:%d\n", _mqttServer.c_str(), _mqttPort);
    Serial.printf("[MQTT] 🔧 Buffer size: 16384 bytes\n");
    Serial.printf("[MQTT] 🆔 Device ID: %s\n", _deviceId.c_str());
}

void MQTTHandler::loadConfig() {
    _prefs.begin(PREF_NAMESPACE, true);
    _mqttServer = _prefs.getString(KEY_MQTT_SERVER, DEFAULT_MQTT_SERVER);
    _mqttPort = _prefs.getInt(KEY_MQTT_PORT, DEFAULT_MQTT_PORT);
    _mqttUser = _prefs.getString(KEY_MQTT_USER, "");
    _mqttPass = _prefs.getString(KEY_MQTT_PASS, "");
    _deviceId = _prefs.getString(KEY_DEVICE_ID, "");
    _deviceToken = _prefs.getString(KEY_DEVICE_TOKEN, DEFAULT_DEVICE_TOKEN);
    _prefs.end();
    
    updateTopics();
}

void MQTTHandler::saveConfig() {
    _prefs.begin(PREF_NAMESPACE, false);
    _prefs.putString(KEY_MQTT_SERVER, _mqttServer);
    _prefs.putInt(KEY_MQTT_PORT, _mqttPort);
    _prefs.putString(KEY_MQTT_USER, _mqttUser);
    _prefs.putString(KEY_MQTT_PASS, _mqttPass);
    _prefs.putString(KEY_DEVICE_ID, _deviceId);
    _prefs.putString(KEY_DEVICE_TOKEN, _deviceToken);
    _prefs.end();
    
    Serial.println("[MQTT] Config saved");
}

void MQTTHandler::setDeviceCredentials(const char* deviceId, const char* token) {
    _deviceId = String(deviceId);
    _deviceToken = String(token);
    
    updateTopics();
    saveConfig();
    
    Serial.printf("[MQTT] ✅ Device credentials set: %s\n", _deviceId.c_str());
}

String MQTTHandler::getDeviceId() {
    return _deviceId;
}

void MQTTHandler::updateTopics() {
    if (_deviceId.length() > 0) {
        snprintf(_topicSensors, sizeof(_topicSensors), TOPIC_SENSORS, _deviceId.c_str());
        snprintf(_topicControl, sizeof(_topicControl), TOPIC_CONTROL, _deviceId.c_str());
        snprintf(_topicStatus, sizeof(_topicStatus), TOPIC_STATUS, _deviceId.c_str());
        snprintf(_topicAck, sizeof(_topicAck), TOPIC_ACK, _deviceId.c_str());
        snprintf(_topicVerify, sizeof(_topicVerify), TOPIC_DEVICE_VERIFY, _deviceId.c_str());
        snprintf(_topicConfirm, sizeof(_topicConfirm), TOPIC_DEVICE_CONFIRM, _deviceId.c_str());
        snprintf(_topicAutomationUpdate, sizeof(_topicAutomationUpdate), "device/%s/automation/update", _deviceId.c_str());
        snprintf(_topicAutomationAck, sizeof(_topicAutomationAck), "device/%s/automation/ack", _deviceId.c_str());
    }
}

String MQTTHandler::generateClientId() {
    return "grow-" + _deviceId + "-" + String(random(1000, 9999));
}

bool MQTTHandler::connect() {
    if (_mqttClient == nullptr || _wifiClient == nullptr) {
        Serial.println("[MQTT] ❌ Not initialized!");
        return false;
    }
    
    if (_mqttClient->connected()) {
        Serial.println("[MQTT] ✅ Already connected");
        return true;
    }
    
    unsigned long now = millis();
    if (now - _lastReconnectAttempt < MQTT_RECONNECT_DELAY) {
        return false;
    }
    _lastReconnectAttempt = now;
    
    String clientId = generateClientId();
    Serial.printf("[MQTT] 🔌 Connecting to %s:%d as %s...\n", 
                  _mqttServer.c_str(), _mqttPort, clientId.c_str());
    
    bool connected = false;
    
    // Build LWT (Last Will and Testament) for offline detection
    String lwtTopic = String("device/") + _deviceId + "/status";
    String lwtMessage = "{\"online\":false}";
    
    if (_mqttUser.length() > 0) {
        connected = _mqttClient->connect(
            clientId.c_str(),
            _mqttUser.c_str(),
            _mqttPass.c_str(),
            lwtTopic.c_str(),
            1,      // QoS
            true,   // Retain
            lwtMessage.c_str()
        );
    } else {
        connected = _mqttClient->connect(
            clientId.c_str(),
            lwtTopic.c_str(),
            1,
            true,
            lwtMessage.c_str()
        );
    }
    
    if (connected) {
        Serial.println("[MQTT] ✅ Connected to broker!");
        subscribeToTopics();
        
        // Publish online status
        String onlineMsg = "{\"online\":true,\"deviceId\":\"" + _deviceId + "\"}";
        _mqttClient->publish(lwtTopic.c_str(), onlineMsg.c_str(), true);
        Serial.println("[MQTT] 📤 Published online status");
        
        return true;
    } else {
        Serial.printf("[MQTT] ❌ Failed, state=%d\n", _mqttClient->state());
        return false;
    }
}

void MQTTHandler::disconnect() {
    if (_mqttClient && _mqttClient->connected()) {
        _mqttClient->disconnect();
        Serial.println("[MQTT] 🔌 Disconnected");
    }
}

bool MQTTHandler::isConnected() {
    return _mqttClient && _mqttClient->connected();
}

void MQTTHandler::loop() {
    if (_mqttClient) {
        _mqttClient->loop();
    }
}

void MQTTHandler::subscribeToTopics() {
    if (!_mqttClient || !_mqttClient->connected()) return;
    
    Serial.println("[MQTT] 📡 Subscribing to topics...");
    
    if (_deviceId.length() > 0) {
        // Subscribe to device verification topic
        _mqttClient->subscribe(_topicVerify, 0);
        Serial.printf("[MQTT] 📥 SUB: %s (QoS 0)\n", _topicVerify);
        
        // Subscribe to control topic
        _mqttClient->subscribe(_topicControl, 0);
        Serial.printf("[MQTT] 📥 SUB: %s (QoS 0)\n", _topicControl);
        
        // Subscribe to automation update topic
        _mqttClient->subscribe(_topicAutomationUpdate, 0);
        Serial.println("[MQTT] ========================================");
        Serial.printf("[MQTT] 📥 SUBSCRIBED: %s (QoS 0)\n", _topicAutomationUpdate);
        Serial.println("[MQTT] ⚠️  CLOUD PHẢI GỬI CHÍNH XÁC TOPIC NÀY!");
        Serial.println("[MQTT] ========================================");
    }
    
    Serial.println("[MQTT] ✅ Subscription complete");
}

void MQTTHandler::handleMessage(char* topic, byte* payload, unsigned int length) {
    Serial.println("[MQTT] ════════════════════════════════════════");
    Serial.printf("[MQTT] 📨 RAW CALLBACK - TOPIC: %s\n", topic);
    Serial.printf("[MQTT] 📦 RAW CALLBACK - LENGTH: %d bytes\n", length);
    Serial.printf("[MQTT] 📦 RAW CALLBACK - PAYLOAD PTR: %p\n", payload);
    
    // Allocate buffer for message
    constexpr size_t STACK_BUFFER_SIZE = 512;
    char stackBuffer[STACK_BUFFER_SIZE];
    char* message = nullptr;
    bool useHeap = (length + 1 > STACK_BUFFER_SIZE);
    
    if (useHeap) {
        message = new (std::nothrow) char[length + 1];
        if (!message) {
            Serial.println("[MQTT] ❌ Memory allocation failed!");
            return;
        }
    } else {
        message = stackBuffer;
    }
    
    if (length > 0 && payload) {
        memcpy(message, payload, length);
    }
    message[length] = '\0';
    
    Serial.printf("[MQTT] 📦 SIZE (after copy): %d bytes\n", length);
    
    // Route message based on topic FIRST (before JSON parsing)
    String topicStr = String(topic);
    
    if (topicStr == String(_topicAutomationUpdate)) {
        // For automation, pass raw message (no JSON parse - it's large!)
        Serial.println("[MQTT] 🤖 Routing to: AUTOMATION HANDLER");
        handleAutomationUpdate(message, length);
    } else {
        // For other topics, parse JSON
        Serial.printf("[MQTT] 📄 Payload (first 256 chars): %.256s\n", message);
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        
        if (error) {
            Serial.printf("[MQTT] ❌ JSON parse error: %s\n", error.c_str());
            if (useHeap) delete[] message;
            return;
        }
        
        // Route other messages
        if (topicStr == String(_topicControl)) {
            Serial.println("[MQTT] 🎮 Routing to: CONTROL HANDLER");
            handleControlMessage(doc);
        } else if (topicStr == String(_topicVerify)) {
            Serial.println("[MQTT] ✔️ Routing to: VERIFY HANDLER");
            handleVerifyMessage(doc);
        } else {
            Serial.printf("[MQTT] ⚠️  Unknown topic: %s\n", topic);
        }
    }
    
    Serial.println("[MQTT] ════════════════════════════════════════\n");
    
    if (useHeap) delete[] message;
}

void MQTTHandler::handleControlMessage(JsonDocument& doc) {
    Serial.println("[MQTT] 🎮 Processing relay command...");
    
    // Extract command ID for acknowledgement
    const char* commandId = doc["commandId"] | "";
    
    // Process each relay
    for (int i = 1; i <= 8; i++) {
        char key[8];
        sprintf(key, "relay%d", i);
        
        if (doc.containsKey(key)) {
            bool state = doc[key].as<bool>();
            Serial.printf("[MQTT] 🔄 Relay %d -> %s\n", i, state ? "ON ✅" : "OFF ❌");
            
            if (_relayCallback) {
                _relayCallback(i, state);
            }
        }
    }
    
    // Send acknowledgement
    if (strlen(commandId) > 0) {
        publishCommandAck(commandId, true, "Command executed");
    }
}

void MQTTHandler::handleVerifyMessage(JsonDocument& doc) {
    Serial.println("[MQTT] Processing verify request...");
    
    const char* token = doc["token"] | "";
    const char* requestId = doc["requestId"] | "";
    
    // Check if token matches
    bool valid = (String(token) == _deviceToken);
    
    Serial.printf("[MQTT] Token check: received=%s, stored=%s, valid=%s\n", 
                  token, _deviceToken.c_str(), valid ? "YES" : "NO");
    
    // Callback for additional processing
    if (_verifyCallback) {
        _verifyCallback(token);
    }
    
    // Send confirmation
    JsonDocument response;
    response["valid"] = valid;
    response["requestId"] = requestId;
    response["deviceId"] = _deviceId;
    response["model"] = DEVICE_MODEL;
    response["firmware"] = FIRMWARE_VERSION;
    
    if (valid) {
        response["message"] = "Device verified successfully";
    } else {
        response["message"] = "Invalid token";
    }
    
    String output;
    serializeJson(response, output);
    
    _mqttClient->publish(_topicConfirm, output.c_str(), false);
    Serial.printf("[MQTT] Published verify confirm to %s\n", _topicConfirm);
}

// ============================================================================
// Publishing Methods
// ============================================================================

bool MQTTHandler::publishSensorData(float temp, float humidity, int co2,
                                    float waterTemp, float ec, float ph,
                                    int waterLevel, float vpd) {
    if (!isConnected()) {
        return false;
    }
    
    JsonDocument doc;
    doc["temp"] = round(temp * 10) / 10.0;
    doc["humidity"] = round(humidity * 10) / 10.0;
    doc["co2"] = co2;
    doc["timestamp"] = millis();
    
    if (waterTemp > 0) doc["waterTemp"] = round(waterTemp * 10) / 10.0;
    if (ec > 0) doc["ec"] = round(ec * 100) / 100.0;
    if (ph > 0) doc["ph"] = round(ph * 10) / 10.0;
    if (waterLevel > 0) doc["waterLevel"] = waterLevel;
    if (vpd > 0) doc["vpd"] = round(vpd * 100) / 100.0;
    
    String output;
    serializeJson(doc, output);
    
    bool success = _mqttClient->publish(_topicSensors, output.c_str(), false);
    
    if (success) {
        Serial.printf("[MQTT] 📤 PUBLISH to %s\n", _topicSensors);
        Serial.printf("[MQTT] 📊 Data: T=%.1f°C H=%.1f%% CO2=%dppm VPD=%.2f\n", 
                     temp, humidity, co2, vpd);
    }
    
    return success;
}

bool MQTTHandler::publishStatus(bool online, const char* ip, int rssi,
                               unsigned long uptime, uint32_t freeHeap) {
    if (!isConnected()) {
        return false;
    }
    
    JsonDocument doc;
    doc["online"] = online;
    doc["ip"] = ip;
    doc["rssi"] = rssi;
    doc["uptime"] = uptime;
    doc["freeHeap"] = freeHeap;
    doc["firmware"] = FIRMWARE_VERSION;
    
    String output;
    serializeJson(doc, output);
    
    bool success = _mqttClient->publish(_topicStatus, output.c_str(), true);
    
    if (success) {
        Serial.printf("[MQTT] 📤 PUBLISH to %s\n", _topicStatus);
        Serial.printf("[MQTT] 📍 Status: Online=%s IP=%s RSSI=%d dBm RAM=%d bytes\n", 
                     online ? "YES" : "NO", ip, rssi, freeHeap);
    }
    
    return success;
}

bool MQTTHandler::publishCommandAck(const char* commandId, bool success, const char* message) {
    if (!isConnected()) {
        return false;
    }
    
    JsonDocument doc;
    doc["commandId"] = commandId;
    doc["success"] = success;
    doc["timestamp"] = millis();
    if (message) {
        doc["message"] = message;
    }
    
    String output;
    serializeJson(doc, output);
    
    return _mqttClient->publish(_topicAck, output.c_str(), false);
}

bool MQTTHandler::publishDeviceRegister() {
    if (!isConnected()) {
        return false;
    }
    
    JsonDocument doc;
    doc["deviceId"] = _deviceId;
    doc["model"] = DEVICE_MODEL;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    
    bool success = _mqttClient->publish(TOPIC_DEVICE_REGISTER, output.c_str(), false);
    
    if (success) {
        Serial.printf("[MQTT] 📤 PUBLISH to %s\n", TOPIC_DEVICE_REGISTER);
        Serial.printf("[MQTT] 🔑 Device: %s | Model: %s | FW: %s\n", 
                     _deviceId.c_str(), DEVICE_MODEL, FIRMWARE_VERSION);
    }
    
    return success;
}

bool MQTTHandler::publishVerifyConfirm(bool valid, const char* message) {
    if (!isConnected()) {
        return false;
    }
    
    JsonDocument doc;
    doc["deviceId"] = _deviceId;
    doc["valid"] = valid;
    doc["timestamp"] = millis();
    if (message) {
        doc["message"] = message;
    }
    
    String output;
    serializeJson(doc, output);
    
    return _mqttClient->publish(_topicConfirm, output.c_str(), false);
}

// ============================================================================
// Callbacks
// ============================================================================

void MQTTHandler::setRelayCommandCallback(RelayCommandCallback callback) {
    _relayCallback = callback;
}

void MQTTHandler::setVerifyRequestCallback(VerifyRequestCallback callback) {
    _verifyCallback = callback;
}

void MQTTHandler::setAutomationUpdateCallback(AutomationUpdateCallback callback) {
    _automationCallback = callback;
}

// ============================================================================
// Automation Sync
// ============================================================================

void MQTTHandler::handleAutomationUpdate(const char* payload, unsigned int length) {
    Serial.println("\n╔═══════════════════════════════════════════════╗");
    Serial.println("║   🔥 AUTOMATION UPDATE RECEIVED!          ║");
    Serial.println("╚═══════════════════════════════════════════════╝");
    Serial.printf("[MQTT] 📥 Topic: %s\n", _topicAutomationUpdate);
    Serial.printf("[MQTT] 📦 Payload size: %d bytes\n", length);
    Serial.printf("[MQTT] 📄 First 200 chars: %.200s...\n", payload);
    
    if (_automationCallback) {
        _automationCallback(payload, length);
    } else {
        Serial.println("[MQTT] ⚠️ No automation callback set");
    }
}

bool MQTTHandler::publishAutomationAck(int version, bool success, const char* errorCode, const char* errorMsg) {
    if (!isConnected()) {
        Serial.println("[MQTT] ❌ Cannot publish ACK - not connected");
        return false;
    }
    
    JsonDocument doc;
    doc["gatewayId"] = _deviceId;
    doc["version"] = version;
    doc["status"] = success ? "OK" : "ERROR";
    doc["timestamp"] = millis();
    
    if (!success && errorCode) {
        JsonArray errors = doc["errors"].to<JsonArray>();
        JsonObject err = errors.add<JsonObject>();
        err["code"] = errorCode;
        err["message"] = errorMsg ? errorMsg : "Unknown error";
    }
    
    // Add storage info
    JsonObject storage = doc["storage"].to<JsonObject>();
    storage["usedBytes"] = SPIFFS.usedBytes();
    storage["freeBytes"] = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    storage["fileName"] = "/automation.json";
    
    String output;
    serializeJson(doc, output);
    
    Serial.printf("[MQTT] 📤 Publishing ACK to: %s\n", _topicAutomationAck);
    Serial.printf("[MQTT] 📤 ACK Payload: %s\n", output.c_str());
    bool result = _mqttClient->publish(_topicAutomationAck, output.c_str(), false);
    
    if (result) {
        Serial.printf("[MQTT] 📤 Automation ACK sent: v%d %s\n", version, success ? "OK" : "ERROR");
    } else {
        Serial.println("[MQTT] ❌ Failed to send automation ACK");
    }
    
    return result;
}
