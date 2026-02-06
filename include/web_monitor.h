/**
 * ESP32 Web Monitor
 * 
 * Provides a web-based dashboard to view:
 * - Sensor readings
 * - Relay states
 * - Automation status
 * - System info
 */

#ifndef WEB_MONITOR_H
#define WEB_MONITOR_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "sensor_manager.h"
#include "relay_controller.h"
#include "automation_sync.h"

// Log entry structure
#define MAX_LOG_ENTRIES 100
struct LogEntry {
    unsigned long timestamp;  // millis()
    char level[8];            // INFO, WARN, ERROR
    char category[16];        // MQTT, RELAY, SENSOR, AUTO, SYSTEM
    char message[128];
};

class WebMonitor {
public:
    WebMonitor();
    
    /**
     * Initialize web server on specified port
     * @param port HTTP port (default 80)
     */
    void begin(uint16_t port = 80);
    
    /**
     * Stop web server
     */
    void stop();
    
    /**
     * Handle incoming requests (call in loop)
     */
    void handleClient();
    
    /**
     * Set references to other modules for data access
     */
    void setSensorManager(SensorManager* sensors);
    void setRelayController(RelayController* relays);
    
    /**
     * Update cached sensor readings
     */
    void updateSensorData(const SensorReadings& readings);
    
    /**
     * Add log entry (static for global access)
     */
    static void addLog(const char* level, const char* category, const char* message);
    
    /**
     * Check if server is running
     */
    bool isRunning() const { return _running; }
    
private:
    WebServer* _server;
    bool _running;
    
    SensorManager* _sensors;
    RelayController* _relays;
    SensorReadings _cachedReadings;
    
    // Log buffer (circular buffer)
    static LogEntry _logBuffer[MAX_LOG_ENTRIES];
    static int _logIndex;
    static int _logCount;
    
    // Request handlers
    void handleRoot();
    void handleApiStatus();
    void handleApiSensors();
    void handleApiRelays();
    void handleApiRelayToggle();   // Toggle relay from dashboard
    void handleApiInputs();        // Read digital inputs
    void handleApiAutomation();
    void handleApiAutomationFull();
    void handleApiAutomationCheck();
    void handleApiSystem();
    void handleApiWatchdog();
    void handleApiWatchdogReset();
    void handleApiLogs();
    void handleNotFound();
    
    // HTML generation
    String getMonitorPage();
    String getJsonContentType();
};

// Global instance
extern WebMonitor webMonitor;

#endif // WEB_MONITOR_H
