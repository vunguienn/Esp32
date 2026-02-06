/**
 * Sensor Manager Implementation
 */

#include "sensor_manager.h"
#include <Wire.h>

SensorManager::SensorManager() {
    _adsInitialized = false;
    _simulationMode = ENABLE_SENSOR_SIMULATION;
    
    // Initialize simulation values with mid-range
    _simTemp = (SIM_TEMP_MIN + SIM_TEMP_MAX) / 2;
    _simHumidity = (SIM_HUMIDITY_MIN + SIM_HUMIDITY_MAX) / 2;
    _simCO2 = (SIM_CO2_MIN + SIM_CO2_MAX) / 2;
    _simWaterTemp = (SIM_WATER_TEMP_MIN + SIM_WATER_TEMP_MAX) / 2;
    _simEC = (SIM_EC_MIN + SIM_EC_MAX) / 2;
    _simPH = (SIM_PH_MIN + SIM_PH_MAX) / 2;
    _simWaterLevel = (SIM_WATER_LEVEL_MIN + SIM_WATER_LEVEL_MAX) / 2;
}

void SensorManager::begin() {
    // Initialize I2C for ADS1115
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // Initialize ADS1115
    if (_ads.begin(ADS1115_ADDR)) {
        _adsInitialized = true;
        _ads.setGain(GAIN_ONE);  // ±4.096V range
        Serial.println("[Sensor] ✅ ADS1115 initialized (I2C address: 0x48)");
    } else {
        Serial.println("[Sensor] ⚠️  ADS1115 not found, using SIMULATION MODE");
        _simulationMode = true;
    }
    
    // Initialize RS485
    initRS485();
    
    Serial.printf("[Sensor] 📊 Mode: %s\n", _simulationMode ? "SIMULATION 📈" : "REAL SENSORS 📡");
}

void SensorManager::initRS485() {
    // TODO: RS485 initialization causing hang - disabled for now
    // Serial1.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
    // pinMode(RS485_DE_RE, OUTPUT);
    // digitalWrite(RS485_DE_RE, LOW);
    
    Serial.println("[Sensor] ⚠️ RS485 disabled (was causing hang)");
}

void SensorManager::setSimulationMode(bool enabled) {
    _simulationMode = enabled;
    Serial.printf("[Sensor] Simulation mode: %s\n", enabled ? "ON" : "OFF");
}

bool SensorManager::isSimulationMode() {
    return _simulationMode;
}

void SensorManager::setSimulatedValues(float temp, float humidity, int co2) {
    _simTemp = temp;
    _simHumidity = humidity;
    _simCO2 = co2;
}

SensorReadings SensorManager::read() {
    SensorReadings readings;
    readings.timestamp = millis();
    
    if (_simulationMode) {
        // Simulated values with realistic drift
        readings.temperature = addNoise(_simTemp, 2.0);
        readings.humidity = addNoise(_simHumidity, 3.0);
        readings.co2 = addNoise(_simCO2, 5.0);
        readings.waterTemp = addNoise(_simWaterTemp, 1.0);
        readings.ec = addNoise(_simEC, 2.0);
        readings.ph = addNoise(_simPH, 1.0);
        readings.waterLevel = addNoise(_simWaterLevel, 5.0);
        
        // Slowly drift simulation values for realism
        _simTemp += random(-10, 11) / 100.0;
        _simHumidity += random(-20, 21) / 100.0;
        _simCO2 += random(-20, 21);
        
        // Clamp to ranges
        _simTemp = constrain(_simTemp, SIM_TEMP_MIN, SIM_TEMP_MAX);
        _simHumidity = constrain(_simHumidity, SIM_HUMIDITY_MIN, SIM_HUMIDITY_MAX);
        _simCO2 = constrain(_simCO2, SIM_CO2_MIN, SIM_CO2_MAX);
        
        readings.valid = true;
    } else {
        // Read from real sensors
        readings.temperature = readTemperature();
        readings.humidity = readHumidity();
        readings.co2 = readCO2();
        readings.waterTemp = readWaterTemp();
        readings.ec = readEC();
        readings.ph = readPH();
        readings.waterLevel = readWaterLevel();
        
        readings.valid = (readings.temperature > -40 && readings.temperature < 100);
    }
    
    // Calculate VPD
    readings.vpd = calculateVPD(readings.temperature, readings.humidity);
    
    return readings;
}

float SensorManager::readTemperature() {
    // TODO: Implement RS485 Modbus read for temperature sensor
    // Example: Read holding register from address 0x01
    return _simulationMode ? addNoise(_simTemp, 2.0) : 25.0;
}

float SensorManager::readHumidity() {
    // TODO: Implement RS485 Modbus read for humidity sensor
    return _simulationMode ? addNoise(_simHumidity, 3.0) : 60.0;
}

int SensorManager::readCO2() {
    // TODO: Implement RS485 Modbus read for CO2 sensor
    return _simulationMode ? addNoise(_simCO2, 5.0) : 800;
}

float SensorManager::readWaterTemp() {
    if (_adsInitialized) {
        // Read from ADC channel 0 (PT100 or NTC thermistor)
        float voltage = readVoltage(0);
        // Convert voltage to temperature (depends on sensor)
        // Example: 10mV/°C sensor
        return voltage * 100;
    }
    return _simulationMode ? addNoise(_simWaterTemp, 1.0) : 22.0;
}

float SensorManager::readEC() {
    if (_adsInitialized) {
        // Read from ADC channel 1 (4-20mA EC sensor)
        float mA = read4to20mA(1);
        // Convert to EC (example: 4mA = 0, 20mA = 5.0 mS/cm)
        return (mA - 4.0) / 16.0 * 5.0;
    }
    return _simulationMode ? addNoise(_simEC, 2.0) : 1.5;
}

float SensorManager::readPH() {
    if (_adsInitialized) {
        // Read from ADC channel 2 (4-20mA pH sensor)
        float mA = read4to20mA(2);
        // Convert to pH (example: 4mA = pH 0, 20mA = pH 14)
        return (mA - 4.0) / 16.0 * 14.0;
    }
    return _simulationMode ? addNoise(_simPH, 1.0) : 6.0;
}

int SensorManager::readWaterLevel() {
    if (_adsInitialized) {
        // Read from ADC channel 3 (4-20mA level sensor)
        float mA = read4to20mA(3);
        // Convert to percentage (4mA = 0%, 20mA = 100%)
        return (int)((mA - 4.0) / 16.0 * 100);
    }
    return _simulationMode ? addNoise(_simWaterLevel, 5.0) : 50;
}

float SensorManager::readADC(uint8_t channel) {
    if (!_adsInitialized || channel > 3) return 0;
    return _ads.readADC_SingleEnded(channel);
}

float SensorManager::readVoltage(uint8_t channel) {
    if (!_adsInitialized || channel > 3) return 0;
    // GAIN_ONE: ±4.096V, resolution = 4.096/32768 = 0.125mV
    return _ads.readADC_SingleEnded(channel) * 0.000125;
}

float SensorManager::read4to20mA(uint8_t channel) {
    // Assuming 250 ohm shunt resistor: 4mA = 1V, 20mA = 5V
    float voltage = readVoltage(channel);
    return voltage / 0.25;  // Convert voltage to mA
}

bool SensorManager::sendRS485Command(const uint8_t* command, size_t len) {
    // Switch to transmit mode
    digitalWrite(RS485_DE_RE, HIGH);
    delayMicroseconds(100);
    
    // Send command
    Serial1.write(command, len);
    Serial1.flush();
    
    // Switch back to receive mode
    delayMicroseconds(100);
    digitalWrite(RS485_DE_RE, LOW);
    
    return true;
}

bool SensorManager::readRS485Response(uint8_t* buffer, size_t maxLen, unsigned long timeout) {
    unsigned long startTime = millis();
    size_t index = 0;
    
    while (millis() - startTime < timeout && index < maxLen) {
        if (Serial1.available()) {
            buffer[index++] = Serial1.read();
        }
    }
    
    return index > 0;
}

float SensorManager::addNoise(float value, float noisePercent) {
    float noise = value * noisePercent / 100.0 * (random(-100, 101) / 100.0);
    return value + noise;
}

int SensorManager::addNoise(int value, float noisePercent) {
    int noise = (int)(value * noisePercent / 100.0 * (random(-100, 101) / 100.0));
    return value + noise;
}

float SensorManager::calculateVPD(float temp, float humidity) {
    // Leaf temperature assumed 2°C below air temp
    float leafTemp = temp - 2.0;
    
    // Saturation vapor pressure at leaf temp
    float svpLeaf = 0.6108 * exp((17.27 * leafTemp) / (leafTemp + 237.3));
    
    // Saturation vapor pressure at air temp
    float svpAir = 0.6108 * exp((17.27 * temp) / (temp + 237.3));
    
    // Actual vapor pressure
    float avp = svpAir * (humidity / 100.0);
    
    // VPD = SVP at leaf - AVP
    float vpd = svpLeaf - avp;
    
    return round(vpd * 100) / 100.0;
}
