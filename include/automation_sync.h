/**
 * ESP32 Automation Sync Module
 * 
 * Handles receiving automation data from Cloud via MQTT
 * and stores in LittleFS for offline execution.
 * Note: Using LittleFS macro aliased as SPIFFS for compatibility.
 */

#ifndef AUTOMATION_SYNC_H
#define AUTOMATION_SYNC_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Use LittleFS instead of SPIFFS for ESP32-S3
#define SPIFFS LittleFS

// ============================================
// CONFIGURATION
// ============================================
#define AUTOMATION_FILE "/automation.json"
#define AUTOMATION_BACKUP "/automation.bak"
#define MAX_RULES 20
#define MAX_ACTIONS 5
#define MAX_TRIGGERS 3
#define MAX_WEEKLY_PLANS 16  // Support up to 16 weeks grow cycle
#define MAX_DEFERRED_ACTIONS 10  // Max pending delayed actions

// ============================================
// DATA STRUCTURES
// ============================================

// Deferred action for non-blocking delays
struct DeferredAction {
  unsigned long executeAt;  // millis() when to execute
  int relayIndex;           // Relay to control (0-7)
  bool state;               // ON or OFF
  bool pending;             // Is this slot in use?
};

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
  char humidityMode[12];  // HUMIDIFY, DEHUMIDIFY
};

// PWM Lighting schedule point (JSON v8)
struct PWMLightingPoint {
  char time[6];           // "HH:mm"
  int brightness;         // 0-100%
  int ch1;                // White channel (0-100%)
  int ch2;                // Yellow channel (0-100%)
  int ch3;                // Red channel (0-100%)
};

struct LightingSchedule {
  char lightsOn[6];        // "HH:mm"
  char lightsOff[6];
  PWMLightingPoint schedule[10];  // Max 10 dimming points per day
  int scheduleCount;
};

struct EquipmentConfig {
  // Fan circulation
  char fanCircMode[12];   // OFF, 24H, TIMER, SCHEDULE, TRIGGERED
  char fanCircOnTime[6];
  char fanCircOffTime[6];
  float fanCircTriggerTemp;
  
  // Fan exhaust  
  char fanExhMode[12];
  char fanExhOnTime[6];
  char fanExhOffTime[6];
  float fanExhTriggerHumi;
  float fanExhTriggerVpd;
  
  // AC
  char acMode[6];         // OFF, COOL, HEAT, AUTO
  float acTargetTemp;
  char acFanSpeed[8];     // AUTO, LOW, MEDIUM, HIGH
};

// Weekly plan structure (for multi-week support)
struct WeeklyPlan {
  int week;                       // Week number within phase (1-8)
  int globalWeek;                 // Global week number (1-16)
  char phase[20];                 // SEEDING, VEG, FLOWER, HARVEST
  EnvironmentTargets targets;
  LightingSchedule lighting;
  EquipmentConfig equipment;
};

// Phase-based weekly plans (JSON v8 structure)
struct PhaseWeeks {
  char phaseName[20];             // "SEEDING", "VEG", "FLOWER", "HARVEST"
  WeeklyPlan weeks[10];           // Each phase can have up to 10 weeks
  int weekCount;                  // Number of weeks in this phase
  int startGlobalWeek;            // Starting global week for this phase
  int endGlobalWeek;              // Ending global week for this phase
};

struct AutomationTrigger {
  char type[20];          // TIME, SENSOR_THRESHOLD, SCHEDULE, INTERVAL
  char time[6];           // For TIME trigger (HH:mm)
  char sensor[15];        // For SENSOR_THRESHOLD: temp, humidity, co2, vpd
  char op[3];             // Operator: >, <, >=, <=, ==
  float value;
  float hysteresis;
};

struct AutomationAction {
  char type[15];          // RELAY_ON, RELAY_OFF, RELAY_TOGGLE
  char relayId[10];       // relay1, relay2, etc.
  int delayMs;
};

struct AutomationRule {
  char id[30];
  char name[50];
  bool enabled;
  int priority;
  AutomationTrigger triggers[MAX_TRIGGERS];
  int triggerCount;
  AutomationAction actions[MAX_ACTIONS];
  int actionCount;
  unsigned long lastExecuted;
  int executionsToday;
  int maxExecutionsPerDay;
  unsigned long cooldownMs;
};

struct IrrigationConfig {
  char id[40];
  char name[50];
  bool enabled;
  char cycleStart[6];
  char cycleEnd[6];
  int pumpDurationSec;
  int restDurationSec;
  char activeDays[7][3];  // MO, TU, WE, TH, FR, SA, SU
  int activeDaysCount;
  char pumpRelays[3][10];
  int pumpCount;
  unsigned long lastRun;
  // State machine for non-blocking pump control
  bool pumpRunning;          // Is pump currently ON?
  unsigned long pumpStartTime; // When pump was turned ON (millis)
};

struct SyncAckPayload {
  char gatewayId[20];
  int version;
  char status[10];        // OK, ERROR, PARTIAL
  unsigned long timestamp;
  size_t usedBytes;
  size_t freeBytes;
  char errorCode[20];
  char errorMessage[100];
};

// ============================================
// AUTOMATION SYNC CLASS
// ============================================

class AutomationSync {
public:
  AutomationSync();
  
  /**
   * Initialize SPIFFS and load saved automation
   */
  bool begin();
  
  /**
   * Handle incoming automation update from MQTT
   * @param payload JSON string from MQTT
   * @param length Length of payload in bytes
   * @return true if successfully parsed and saved
   */
  bool handleUpdate(const char* payload, unsigned int length);
  
  /**
   * Get acknowledgement payload to send back to Cloud
   */
  SyncAckPayload getAckPayload(bool success, const char* errorCode = nullptr, const char* errorMsg = nullptr);
  
  /**
   * Update sensor values for rule evaluation
   */
  void updateSensorValues(float temp, float humi, float co2, float vpd);
  
  /**
   * Run automation rules based on current time and sensor values
   * Call this in loop() every second
   */
  void runRules();
  
  /**
   * Check lighting schedule and control light relay
   */
  void checkLightingSchedule();
  
  /**
   * Check equipment schedules (fans, AC)
   */
  void checkEquipmentSchedules();
  
  /**
   * Check irrigation schedules
   */
  void checkIrrigationSchedules();
  
  /**
   * Set relay state callback
   */
  void setRelayCallback(void (*callback)(uint8_t relay, bool state, const char* source));
  
  /**
   * Manual relay override (bypasses automation)
   */
  void setRelayManualMode(uint8_t relay, bool manual);
  
  /**
   * Get current automation version
   */
  int getVersion() const { return _version; }
  
  /**
   * Get gateway ID
   */
  const char* getGatewayId() const { return _gatewayId; }
  
  /**
   * Get room ID
   */
  const char* getRoomId() const { return _roomId; }
  
  /**
   * Get lighting schedule
   */
  const LightingSchedule& getLighting() const { return _lighting; }
  
  /**
   * Get environment targets
   */
  const EnvironmentTargets& getTargets() const { return _targets; }
  
  /**
   * Check if automation is loaded
   */
  bool isLoaded() const { return _loaded; }

  /**
   * Set sensor override values for testing
   */
  void setSensorOverride(float temp, float humi, float co2, float vpd);

  /**
   * Clear sensor override and use real sensor values
   */
  void clearSensorOverride();

  /**
   * Check if sensor override is enabled
   */
  bool isSensorOverrideEnabled() const { return _sensorOverrideEnabled; }

  /**
   * Check if automation is allowed by license
   */
  bool isAutomationAllowed() const;

  /**
   * Get license status
   */
  bool isLicenseActive() const { return _licenseActive; }
  unsigned long getLicenseExpiresAt() const { return _licenseExpiresAt; }
  int getOfflineGraceDays() const { return _offlineGraceDays; }
  unsigned long getLastCloudSync() const { return _lastCloudSync; }
  
  /**
   * Get rule count
   */
  int getRuleCount() const { return _ruleCount; }
  
  /**
   * Check if it's daytime based on lighting schedule
   */
  bool isDaytime() const;
  
  /**
   * Get current day/night targets
   */
  float getCurrentTempTarget() const;
  int getCurrentHumiHigh() const;
  int getCurrentHumiLow() const;
  int getCurrentCo2Start() const;
  int getCurrentCo2Stop() const;
  
  /**
   * Get calculated current week (from plantStartDate)
   */
  int getCurrentWeek() const { return _currentWeek; }

  /**
   * Get current day within week (1-7)
   */
  int getCurrentDayInWeek() const { return _currentDay; }

  /**
   * Get current project day (1..N) since plant start
   */
  int getProjectDay() const { return _currentProjectDay; }

  /**
   * Set lifecycle override (project day, week, phase)
   */
  bool setLifecycleOverride(int projectDay, int currentWeek, const char* phaseName);

  /**
   * Clear lifecycle override and return to auto calculation
   */
  bool clearLifecycleOverride();

  /**
   * Check if lifecycle override is enabled
   */
  bool isLifecycleOverrideEnabled() const { return _lifecycleOverrideEnabled; }
  
  /**
   * Get current week within phase (e.g., Week 2 of FLOWER)
   */
  int getCurrentWeekInPhase() const { return _currentWeekInPhase; }
  
  /**
   * Get current phase name (SEEDING, VEG, FLOWER, HARVEST)
   */
  const char* getCurrentPhase() const { return _currentPhase; }
  
  /**
   * Get all phase-based weekly plans (JSON v8 structure)
   */
  const PhaseWeeks* getPhaseWeeks() const { return _phaseWeeks; }
  
  /**
   * Get number of phases
   */
  int getPhaseCount() const { return _phaseCount; }
  
  /**
   * Get all weekly plans (flattened array for compatibility)
   */
  const WeeklyPlan* getWeeklyPlans() const { return _weeklyPlans; }
  int getWeeklyPlanCount() const { return _weeklyPlanCount; }
  
  /**
   * Get total weeks
   */
  int getTotalWeeks() const { return _totalWeeks; }
  
  /**
   * Get plant start timestamp
   */
  unsigned long getPlantStartTimestamp() const { return _plantStartTimestamp; }
  
  /**
   * Get timezone offset in seconds from UTC
   */
  long getTimezoneOffset() const { return _timezoneOffset; }
  
  /**
   * Get timezone name
   */
  const char* getTimezoneName() const { return _timezoneName; }
  
  /**
   * Get current week's plan (with targets, lighting, equipment)
   */
  const WeeklyPlan* getCurrentWeekPlan() const;
  
  /**
   * Get current week's environment targets
   */
  const EnvironmentTargets* getCurrentTargets() const;
  
  /**
   * Get current week's lighting schedule
   */
  const LightingSchedule* getCurrentLightingSchedule() const;
  
  /**
   * Get current week's equipment config
   */
  const EquipmentConfig* getCurrentEquipmentConfig() const;
  
  /**
   * Get current phase's week count
   */
  int getPhaseWeekCount() const {
    for (int i = 0; i < _phaseCount; i++) {
      if (strcmp(_phaseWeeks[i].phaseName, _currentPhase) == 0) {
        return _phaseWeeks[i].weekCount;
      }
    }
    return 0;
  }
  
  /**
   * Get irrigation schedules array
   */
  const IrrigationConfig* getIrrigations() const { return _irrigation; }
  
  /**
   * Get irrigation schedules count
   */
  int getIrrigationCount() const { return _irrigationCount; }
  
  /**
   * Get local time (UTC + timezone offset)
   * @param buffer Output buffer for "HH:mm" format
   */
  void getLocalTime(char* buffer) const;
  
  /**
   * Get local hour (0-23)
   */
  int getLocalHour() const;
  
  /**
   * Get local minute (0-59)
   */
  int getLocalMinute() const;

private:
  // State
  bool _loaded;
  int _version;
  char _gatewayId[20];
  char _roomId[40];
  char _checksum[33];
  
  // Timezone (from Cloud)
  long _timezoneOffset;           // Seconds from UTC (e.g., +25200 for UTC+7)
  char _timezoneName[40];         // e.g., "Asia/Ho_Chi_Minh"
  
  // Plant lifecycle
  unsigned long _plantStartTimestamp;  // Unix timestamp of plant start
  int _totalWeeks;
  int _currentWeek;                    // Global week number (1-16)
  int _currentWeekInPhase;             // Week within current phase (1-8)
  int _currentDay;                     // Day within current week (1-7)
  int _currentProjectDay;              // Day since plant start (1..N)
  char _currentPhase[20];              // SEEDING, VEG, FLOWER, HARVEST

  // Lifecycle override (manual edit)
  bool _lifecycleOverrideEnabled;
  unsigned long _lifecycleBaseTimestamp;
  int _lifecycleBaseProjectDay;
  int _lifecycleBaseWeek;
  int _lifecycleBaseDayInWeek;
  char _lifecyclePhaseOverride[20];

  // Sensor override (testing)
  bool _sensorOverrideEnabled;
  float _sensorOverrideTemp;
  float _sensorOverrideHumi;
  float _sensorOverrideCo2;
  float _sensorOverrideVpd;

  // License state (from Cloud)
  bool _licenseActive;
  unsigned long _licenseExpiresAt;
  unsigned long _lastCloudSync;
  int _offlineGraceDays;
  
  // Phase-based weekly plans (JSON v8 nested structure)
  PhaseWeeks _phaseWeeks[4];           // Max 4 phases: SEEDING, VEG, FLOWER, HARVEST
  int _phaseCount;
  
  // Weekly plans (flattened array for compatibility)
  WeeklyPlan _weeklyPlans[MAX_WEEKLY_PLANS];
  int _weeklyPlanCount;
  
  // Current week's active settings (selected from weekly plans)
  LightingSchedule _lighting;
  EnvironmentTargets _targets;
  EquipmentConfig _equipment;
  
  // Rules
  AutomationRule _rules[MAX_RULES];
  int _ruleCount;
  
  // Irrigation
  IrrigationConfig _irrigation[5];
  int _irrigationCount;
  
  // Relay states
  bool _relayStates[8];
  bool _relayAutoMode[8];
  
  // Current sensor values
  float _currentTemp;
  float _currentHumi;
  float _currentCo2;
  float _currentVpd;
  
  // Deferred actions queue (non-blocking delays)
  DeferredAction _deferredActions[MAX_DEFERRED_ACTIONS];
  
  // Daily reset tracking
  int _lastDayReset;  // Day of month when last reset executionsToday
  
  // Callback
  void (*_relayCallback)(uint8_t relay, bool state, const char* source);
  
  // Private methods
  bool parsePayload(const char* jsonData);
  bool parseNestedWeeklyPlans(JsonObject weeklyPlansObj);
  void updateCurrentWeekInfo();
  
  bool saveToSPIFFS(const char* jsonData);
  String loadFromSPIFFS();
  String calculateMD5(const char* data);
  
  void getCurrentTime(char* buffer) const;
  int compareTime(const char* t1, const char* t2) const;
  int getRelayIndex(const char* relayId) const;
  
  bool checkTrigger(const AutomationTrigger& trigger);
  void executeAction(const AutomationAction& action);
  void setRelay(int index, bool state, const char* source);
  
  // Deferred action queue management
  void queueDeferredAction(int relayIndex, bool state, unsigned long delayMs);
  void processDeferredActions();
  
  // Daily reset
  void resetDailyCounters();
  
  int parseDayOfWeek(const char* day) const;
  bool isActiveDayOfWeek(const char days[][3], int count) const;
  
  // Week calculation
  void calculateCurrentWeek();
  void selectWeeklyPlan(int week);
  void loadLifecycleOverride();
  void saveLifecycleOverride();
  void applyLifecycleOverride();
  void loadLicenseState();
  void saveLicenseState();
  void resetLifecycleForNewLicense();
  bool parseWeeklyPlans(JsonArray& plans);
};

// Global instance
extern AutomationSync automationSync;

#endif // AUTOMATION_SYNC_H
