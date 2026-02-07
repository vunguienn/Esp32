/**
 * MQTT Handler for ESP32 Cloud Grow Gateway
 * Handles all MQTT communication with Cloud server
 */

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "config.h"

// Callback types
typedef void (*RelayCommandCallback)(uint8_t relay, bool state);
typedef void (*VerifyRequestCallback)(const char* token);
typedef void (*AutomationUpdateCallback)(const char* payload, unsigned int length);

class MQTTHandler {
public:
    MQTTHandler();
    
    // Initialize MQTT
    void begin(WiFiClient& wifiClient);
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected();
    void loop();
    
    // Set device credentials
    void setDeviceCredentials(const char* deviceId, const char* token);
    
    // Get device info
    String getDeviceId();
    
    // Publishing
    bool publishSensorData(float temp, float humidity, int co2, 
                          float waterTemp = 0, float ec = 0, float ph = 0, 
                          int waterLevel = 0, float vpd = 0);
    bool publishStatus(bool online, const char* ip, int rssi, 
                      unsigned long uptime, uint32_t freeHeap);
    bool publishLifecycleStatus(bool online, const char* ip, int rssi,
                                unsigned long uptime, uint32_t freeHeap,
                                int projectDay, int dayInWeek,
                                int currentWeek, int currentWeekInPhase,
                                const char* currentPhase);
    bool publishCommandAck(const char* commandId, bool success, const char* message = nullptr);
    
    // Device verification
    bool publishDeviceRegister();
    bool publishVerifyConfirm(bool valid, const char* message = nullptr);
    
    // Automation sync acknowledgement
    bool publishAutomationAck(int version, bool success, const char* errorCode = nullptr, const char* errorMsg = nullptr);
    
    // Callbacks
    void setRelayCommandCallback(RelayCommandCallback callback);
    void setVerifyRequestCallback(VerifyRequestCallback callback);
    void setAutomationUpdateCallback(AutomationUpdateCallback callback);
    
    // Load/save config
    void loadConfig();
    void saveConfig();
    
private:
    PubSubClient* _mqttClient;
    WiFiClient* _wifiClient;
    Preferences _prefs;
    
    String _mqttServer;
    int _mqttPort;
    String _mqttUser;
    String _mqttPass;
    String _deviceId;
    String _deviceToken;
    
    unsigned long _lastReconnectAttempt;
    
    RelayCommandCallback _relayCallback;
    VerifyRequestCallback _verifyCallback;
    AutomationUpdateCallback _automationCallback;
    
    // Topic buffers
    char _topicSensors[64];
    char _topicControl[64];
    char _topicStatus[64];
    char _topicAck[64];
    char _topicVerify[64];
    char _topicConfirm[64];
    char _topicAutomationUpdate[80];
    char _topicAutomationAck[80];
    
    // Internal methods
    void updateTopics();
    void subscribeToTopics();
    static void staticCallback(char* topic, byte* payload, unsigned int length);
    void handleMessage(char* topic, byte* payload, unsigned int length);
    void handleControlMessage(JsonDocument& doc);
    void handleVerifyMessage(JsonDocument& doc);
    void handleAutomationUpdate(const char* payload, unsigned int length);
    
    String generateClientId();
};

// Global instance
extern MQTTHandler mqttHandler;

#endif // MQTT_HANDLER_H
