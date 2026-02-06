/**
 * Sensor Manager for ESP32 Cloud Grow Gateway
 * Handles RS485 sensors and simulation mode
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include "config.h"

struct SensorReadings {
    float temperature;
    float humidity;
    int co2;
    float waterTemp;
    float ec;
    float ph;
    int waterLevel;
    float vpd;
    bool valid;
    unsigned long timestamp;
};

class SensorManager {
public:
    SensorManager();
    
    // Initialize sensors
    void begin();
    
    // Read all sensors
    SensorReadings read();
    
    // Read individual sensors
    float readTemperature();
    float readHumidity();
    int readCO2();
    float readWaterTemp();
    float readEC();
    float readPH();
    int readWaterLevel();
    
    // Simulation mode
    void setSimulationMode(bool enabled);
    bool isSimulationMode();
    
    // Set simulation values (for testing)
    void setSimulatedValues(float temp, float humidity, int co2);
    
    // ADC readings (ADS1115)
    float readADC(uint8_t channel);
    float readVoltage(uint8_t channel);
    float read4to20mA(uint8_t channel);
    
    // RS485 communication
    bool sendRS485Command(const uint8_t* command, size_t len);
    bool readRS485Response(uint8_t* buffer, size_t maxLen, unsigned long timeout);
    
private:
    Adafruit_ADS1115 _ads;
    bool _adsInitialized;
    bool _simulationMode;
    
    // Simulation state
    float _simTemp;
    float _simHumidity;
    int _simCO2;
    float _simWaterTemp;
    float _simEC;
    float _simPH;
    int _simWaterLevel;
    
    // Noise for realistic simulation
    float addNoise(float value, float noisePercent);
    int addNoise(int value, float noisePercent);
    
    // Calculate VPD
    float calculateVPD(float temp, float humidity);
    
    // Initialize RS485
    void initRS485();
};

#endif // SENSOR_MANAGER_H
