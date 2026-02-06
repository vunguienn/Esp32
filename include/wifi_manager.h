/**
 * WiFi Manager for ESP32 Cloud Grow Gateway
 * Handles WiFi connection and AP mode for provisioning
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "config.h"

class GrowWiFiManager {
public:
    GrowWiFiManager();
    
    // Initialize WiFi manager
    void begin();
    
    // Try to connect to saved WiFi, start AP if failed
    bool autoConnect();
    
    // Start AP mode for configuration
    void startAPMode();
    
    // Stop AP mode
    void stopAPMode();
    
    // Check if connected to WiFi
    bool isConnected();
    
    // Get current IP address
    String getIP();
    
    // Get WiFi RSSI
    int getRSSI();
    
    // Get saved credentials
    String getSavedSSID();
    
    // Set WiFi credentials programmatically
    void setCredentials(const char* ssid, const char* password);
    
    // Clear saved credentials
    void clearCredentials();
    
    // Handle web server requests (call in loop)
    void handleClient();
    
    // Check if AP mode is active
    bool isAPModeActive();
    
    // Get device ID (MAC-based)
    String getDeviceId();
    
    // Get AP SSID
    String getAPSSID();
    
    // Callbacks
    void setAPStartCallback(void (*callback)());
    void setConnectedCallback(void (*callback)());
    
private:
    WebServer* _server;
    Preferences _prefs;
    bool _apModeActive;
    String _savedSSID;
    String _savedPass;
    String _deviceId;
    
    void (*_apStartCallback)();
    void (*_connectedCallback)();
    
    // Web server handlers
    void handleRoot();
    void handleSave();
    void handleScan();
    void handleReset();
    void handleStatus();
    
    // HTML pages
    String getConfigPage();
    String getScanResultsJSON();
    
    // Helper functions
    void loadCredentials();
    void saveCredentials();
    bool connectToWiFi(int timeoutSeconds);
    void generateDeviceId();
};

#endif // WIFI_MANAGER_H
