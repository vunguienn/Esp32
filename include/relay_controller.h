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
    RELAY_HUMIDIFIER = 2,   // Humidifier (Phun ẩm)
    RELAY_DEHUMIDIFIER = 3, // Dehumidifier (Hút ẩm)
    RELAY_CO2_VALVE = 4,    // CO2 solenoid valve
    RELAY_PUMP = 5,         // Irrigation pump
    RELAY_FAN_CIRC = 6,     // Circulation fan (Quạt thổi)
    RELAY_FAN_EXHAUST = 7,  // Exhaust fan (Quạt Hút)
    RELAY_OPTION = 8        // Optional device
};

struct RelayStates {
    bool relay1;  // Light
    bool relay2;  // Humidifier (Phun ẩm)
    bool relay3;  // Dehumidifier (Hút ẩm)
    bool relay4;  // CO2
    bool relay5;  // Pump
    bool relay6;  // Fan Circulation (Quạt thổi)
    bool relay7;  // Fan Exhaust (Quạt Hút)
    bool relay8;  // Option
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
    void setHumidifier(bool state);
    void setDehumidifier(bool state);
    void setCO2Valve(bool state);
    void setPump(bool state);
    void setFanCirculation(bool state);
    void setFanExhaust(bool state);
    void setOption(bool state);
    
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
