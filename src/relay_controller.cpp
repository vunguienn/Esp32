/**
 * Relay Controller Implementation
 */

#include "relay_controller.h"

RelayController::RelayController() {
    for (int i = 0; i < 8; i++) {
        _states[i] = false;
        _onTimes[i] = 0;
        _maxOnTimes[i] = 0;  // 0 = no limit
    }
}

void RelayController::begin() {
    // Initialize all relay pins as outputs
    pinMode(DO_PIN_1, OUTPUT);
    pinMode(DO_PIN_2, OUTPUT);
    pinMode(DO_PIN_3, OUTPUT);
    pinMode(DO_PIN_4, OUTPUT);
    pinMode(DO_PIN_5, OUTPUT);
    pinMode(DO_PIN_6, OUTPUT);
    pinMode(DO_PIN_7, OUTPUT);
    pinMode(DO_PIN_8, OUTPUT);
    
    // Initialize status LEDs
    pinMode(LED_STATUS, OUTPUT);
    pinMode(LED_WIFI, OUTPUT);
    pinMode(LED_MQTT, OUTPUT);
    
    // Set all relays to OFF initially
    setAllRelays(false);
    
    Serial.println("[Relay] Controller initialized - all relays OFF");
}

uint8_t RelayController::getPin(uint8_t channel) {
    switch (channel) {
        case 1: return DO_PIN_1;
        case 2: return DO_PIN_2;
        case 3: return DO_PIN_3;
        case 4: return DO_PIN_4;
        case 5: return DO_PIN_5;
        case 6: return DO_PIN_6;
        case 7: return DO_PIN_7;
        case 8: return DO_PIN_8;
        default: return DO_PIN_1;
    }
}

void RelayController::applyState(uint8_t channel, bool state) {
    if (channel < 1 || channel > 8) return;
    
    uint8_t pin = getPin(channel);
    bool outputState = RELAY_ACTIVE_HIGH ? state : !state;
    
    digitalWrite(pin, outputState);
    _states[channel - 1] = state;
    
    // Track on-time
    if (state) {
        if (_onTimes[channel - 1] == 0) {
            _onTimes[channel - 1] = millis();
        }
    } else {
        _onTimes[channel - 1] = 0;
    }
    
    Serial.printf("[Relay] CH%d -> %s (pin %d = %s)\n", 
                  channel, state ? "ON" : "OFF",
                  pin, outputState ? "HIGH" : "LOW");
}

void RelayController::setRelay(uint8_t channel, bool state) {
    if (channel < 1 || channel > 8) {
        Serial.printf("[Relay] Invalid channel: %d\n", channel);
        return;
    }
    
    applyState(channel, state);
}

bool RelayController::getRelay(uint8_t channel) {
    if (channel < 1 || channel > 8) return false;
    return _states[channel - 1];
}

void RelayController::toggleRelay(uint8_t channel) {
    if (channel < 1 || channel > 8) return;
    setRelay(channel, !_states[channel - 1]);
}

// Named relay control
void RelayController::setLight(bool state) {
    setRelay(RELAY_LIGHT, state);
}

void RelayController::setHumidifier(bool state) {
    setRelay(RELAY_HUMIDIFIER, state);
}

void RelayController::setDehumidifier(bool state) {
    setRelay(RELAY_DEHUMIDIFIER, state);
}

void RelayController::setCO2Valve(bool state) {
    setRelay(RELAY_CO2_VALVE, state);
}

void RelayController::setPump(bool state) {
    setRelay(RELAY_PUMP, state);
}

void RelayController::setFanCirculation(bool state) {
    setRelay(RELAY_FAN_CIRC, state);
}

void RelayController::setFanExhaust(bool state) {
    setRelay(RELAY_FAN_EXHAUST, state);
}

void RelayController::setOption(bool state) {
    setRelay(RELAY_OPTION, state);
}

// Bulk control
void RelayController::setAllRelays(bool state) {
    for (int i = 1; i <= 8; i++) {
        applyState(i, state);
    }
}

void RelayController::setRelayStates(RelayStates states) {
    applyState(1, states.relay1);
    applyState(2, states.relay2);
    applyState(3, states.relay3);
    applyState(4, states.relay4);
    applyState(5, states.relay5);
    applyState(6, states.relay6);
    applyState(7, states.relay7);
    applyState(8, states.relay8);
}

RelayStates RelayController::getRelayStates() {
    RelayStates states;
    states.relay1 = _states[0];
    states.relay2 = _states[1];
    states.relay3 = _states[2];
    states.relay4 = _states[3];
    states.relay5 = _states[4];
    states.relay6 = _states[5];
    states.relay7 = _states[6];
    states.relay8 = _states[7];
    return states;
}

// Safety
void RelayController::emergencyStop() {
    Serial.println("[Relay] !!! EMERGENCY STOP !!!");
    setAllRelays(false);
    
    // Flash status LED
    for (int i = 0; i < 10; i++) {
        digitalWrite(LED_STATUS, HIGH);
        delay(100);
        digitalWrite(LED_STATUS, LOW);
        delay(100);
    }
}

void RelayController::setMaxOnTime(uint8_t channel, unsigned long maxMs) {
    if (channel >= 1 && channel <= 8) {
        _maxOnTimes[channel - 1] = maxMs;
    }
}

bool RelayController::isOvertime(uint8_t channel) {
    if (channel < 1 || channel > 8) return false;
    
    uint8_t idx = channel - 1;
    if (_maxOnTimes[idx] == 0) return false;  // No limit
    if (_onTimes[idx] == 0) return false;     // Not on
    
    return (millis() - _onTimes[idx]) > _maxOnTimes[idx];
}

void RelayController::updateStatusLED() {
    // Count active relays
    int activeCount = 0;
    for (int i = 0; i < 8; i++) {
        if (_states[i]) activeCount++;
    }
    
    // Blink pattern based on active relays
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    
    unsigned long interval = (activeCount == 0) ? 1000 : (500 / activeCount);
    
    if (millis() - lastBlink > interval) {
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(LED_STATUS, ledState);
    }
}
