/**
 * Relay Controller for ESP32 Cloud Grow Gateway
 * Handles 8-channel relay control
 */

#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

// Relay names for clarity
enum RelayChannel {
    RELAY_LIGHT = 1,        // Main grow light
    RELAY_FAN_CIRC = 2,     // Circulation fan
    RELAY_FAN_EXHAUST = 3,  // Exhaust fan
    RELAY_PUMP_1 = 4,       // Irrigation pump 1
    RELAY_PUMP_2 = 5,       // Irrigation pump 2
    RELAY_PUMP_3 = 6,       // Irrigation pump 3
    RELAY_CO2_VALVE = 7,    // CO2 solenoid valve
    RELAY_AC_TRIGGER = 8    // AC control trigger
};

struct RelayStates {
    bool relay1;  // Light
    bool relay2;  // Fan circulation
    bool relay3;  // Fan exhaust
    bool relay4;  // Pump 1
    bool relay5;  // Pump 2
    bool relay6;  // Pump 3
    bool relay7;  // CO2
    bool relay8;  // AC
};

class RelayController {
public:
    RelayController();
    
    // Initialize relay pins
    void begin();
    
    // Control individual relay
    void setRelay(uint8_t channel, bool state);
    bool getRelay(uint8_t channel);
    void toggleRelay(uint8_t channel);
    
    // Control by name
    void setLight(bool state);
    void setFanCirculation(bool state);
    void setFanExhaust(bool state);
    void setPump(uint8_t pumpNum, bool state);
    void setCO2Valve(bool state);
    void setACTrigger(bool state);
    
    // Bulk control
    void setAllRelays(bool state);
    void setRelayStates(RelayStates states);
    RelayStates getRelayStates();
    
    // Safety features
    void emergencyStop();
    void setMaxOnTime(uint8_t channel, unsigned long maxMs);
    bool isOvertime(uint8_t channel);
    
    // Status LED
    void updateStatusLED();
    
private:
    bool _states[8];
    unsigned long _onTimes[8];
    unsigned long _maxOnTimes[8];
    
    uint8_t getPin(uint8_t channel);
    void applyState(uint8_t channel, bool state);
};

#endif // RELAY_CONTROLLER_H
