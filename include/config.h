/**
 * ESP32-S3 IoT Gateway Configuration
 * Hardware Pin Definitions for Cloud Grow System
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// DEVICE IDENTIFICATION
// ============================================================================
// Unique device token - Generated from Cloud when creating device
#define DEFAULT_DEVICE_TOKEN    "ZXCZXC"
#define DEVICE_MODEL            "ESP32-S3-GROW-V1"
#define FIRMWARE_VERSION        "1.0.0"

// ============================================================================
// WIFI CONFIGURATION
// ============================================================================
#define AP_SSID_PREFIX          "GrowGateway_"    // AP mode SSID prefix
#define AP_PASSWORD             "12345678"       // Default AP password
#define WIFI_CONNECT_TIMEOUT    30                // Seconds to wait for WiFi
#define AP_TIMEOUT              180               // Seconds before AP mode timeout

// ============================================================================
// MQTT CONFIGURATION (Default - User can change via AP Portal)
// ============================================================================
#define DEFAULT_MQTT_SERVER     "192.168.1.100"   // Default Cloud server IP
#define DEFAULT_MQTT_PORT       1883
#define DEFAULT_MQTT_USER       ""
#define DEFAULT_MQTT_PASS       ""
#define MQTT_RECONNECT_DELAY    5000              // ms between reconnect attempts
#define MQTT_KEEPALIVE          60                // Keepalive interval in seconds

// ============================================================================
// MQTT TOPICS (Based on Cloud MQTT Service)
// ============================================================================
// Format: device/{deviceId}/...
// Note: Mỗi thiết bị là 1 phòng trồng (không có Room ID riêng)
#define TOPIC_SENSORS           "device/%s/sensors"    // ESP32 -> Cloud: Sensor data
#define TOPIC_CONTROL           "device/%s/control"    // Cloud -> ESP32: Relay commands
#define TOPIC_STATUS            "device/%s/status"     // ESP32 -> Cloud: Online status
#define TOPIC_ACK               "device/%s/ack"        // ESP32 -> Cloud: Command ack

// Device Registration/Verification Topics
#define TOPIC_DEVICE_REGISTER   "device/register"      // ESP32 -> Cloud: Register new device
#define TOPIC_DEVICE_VERIFY     "device/%s/verify"     // Cloud -> ESP32: Verify request (deviceId)
#define TOPIC_DEVICE_CONFIRM    "device/%s/confirm"    // ESP32 -> Cloud: Verify response (deviceId)

// ============================================================================
// HARDWARE PIN DEFINITIONS (ESP32-S3)
// ============================================================================

// Digital Inputs (7 channels - Optocoupler isolated)
// Note: DI_PIN_8 not available - IO8 is used for I2C SCL
#define DI_PIN_1    1     // Input 1 - IN1
#define DI_PIN_2    2     // Input 2 - IN2
#define DI_PIN_3    3     // Input 3 - IN3
#define DI_PIN_4    4     // Input 4 - IN4
#define DI_PIN_5    5     // Input 5 - IN5
#define DI_PIN_6    6     // Input 6 - IN6
#define DI_PIN_7    7     // Input 7 - IN7

// Digital Outputs / Relays (8 channels) - From PCB Schematic ESP32-S3-1U-N4
// OUT1-OUT8 mapping according to hardware design
#define DO_PIN_1    20    // Relay 1 - Light (Đèn) - OUT1
#define DO_PIN_2    19    // Relay 2 - Humidifier (Phun ẩm) - OUT2
#define DO_PIN_3    21    // Relay 3 - Dehumidifier (Hút ẩm) - OUT3
#define DO_PIN_4    42    // Relay 4 - CO2 valve - OUT4
#define DO_PIN_5    41    // Relay 5 - Pump (Bơm tưới) - OUT5
#define DO_PIN_6    40    // Relay 6 - Fan Circulation (Quạt thổi) - OUT6
#define DO_PIN_7    39    // Relay 7 - Fan Exhaust (Quạt hút) - OUT7
#define DO_PIN_8    38    // Relay 8 - Option (Tùy chọn) - OUT8

// Relay active state (HIGH or LOW depending on relay module)
// Low-trigger relay boards should use false
#define RELAY_ACTIVE_HIGH   false

// PWM Dimming for LED Lighting (3 channels)
// NOTE: Relay CH1 (GPIO 20) controls main light ON/OFF
// These PWM pins control brightness & color mixing 
#define PWM_FREQ            5000      // 5 KHz
#define PWM_RESOLUTION      8         // 8-bit resolution (0-255)
#define PWM_CHANNEL_1       0         // LEDC channel for White
#define PWM_CHANNEL_2       1         // LEDC channel for Yellow
#define PWM_CHANNEL_3       2         // LEDC channel for Red
// ⚠️ PWM GPIO pins depend on your LED driver hardware
// If using relay module only, PWM dimming will be disabled
#define PWM_CH1_PIN         20        // PWM for White LED
#define PWM_CH2_PIN         34        // PWM for Yellow LED
#define PWM_CH3_PIN         35        // PWM for Red LED

// ADS1115 ADC (4 channels via I2C) - From PCB Schematic
#define I2C_SDA         9     // IO9
#define I2C_SCL         8     // IO8 (Changed from IO10)
#define ADS1115_ADDR    0x48

// RS485 Interface (GPIO 17,18 - tránh conflict với DI_PIN_7,8)
#define RS485_TX        17
#define RS485_RX        18
#define RS485_DE_RE     46    // Driver Enable / Receiver Enable

// Ethernet W5500 (SPI)
#define ETH_CS          10
#define ETH_MOSI        11
#define ETH_MISO        13
#define ETH_SCK         12
#define ETH_RST         14

// Status LED
#define LED_STATUS      2
#define LED_WIFI        47
#define LED_MQTT        45   // Avoid buzzer on GPIO48

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================
#define SENSOR_READ_INTERVAL    5000      // Read sensors every 5 seconds
#define MQTT_PUBLISH_INTERVAL   10000     // Publish to Cloud every 10 seconds
#define STATUS_UPDATE_INTERVAL  60000     // Status update every 60 seconds
#define WIFI_CHECK_INTERVAL     30000     // Check WiFi connection every 30s
#define RELAY_DEBOUNCE_MS       100       // Relay switching debounce

// ============================================================================
// SENSOR SIMULATION (For testing without RS485 sensors)
// ============================================================================
#define ENABLE_SENSOR_SIMULATION   true   // Set to false when using real RS485 sensors

// Simulated sensor ranges
#define SIM_TEMP_MIN        22.0
#define SIM_TEMP_MAX        28.0
#define SIM_HUMIDITY_MIN    55.0
#define SIM_HUMIDITY_MAX    75.0
#define SIM_CO2_MIN         600
#define SIM_CO2_MAX         1400
#define SIM_WATER_TEMP_MIN  20.0
#define SIM_WATER_TEMP_MAX  25.0
#define SIM_EC_MIN          1.0
#define SIM_EC_MAX          2.5
#define SIM_PH_MIN          5.5
#define SIM_PH_MAX          6.5
#define SIM_WATER_LEVEL_MIN 30
#define SIM_WATER_LEVEL_MAX 95

// ============================================================================
// PREFERENCES KEYS (For NVS Storage)
// ============================================================================
#define PREF_NAMESPACE      "grow_config"
#define KEY_WIFI_SSID       "wifi_ssid"
#define KEY_WIFI_PASS       "wifi_pass"
#define KEY_MQTT_SERVER     "mqtt_server"
#define KEY_MQTT_PORT       "mqtt_port"
#define KEY_MQTT_USER       "mqtt_user"
#define KEY_MQTT_PASS       "mqtt_pass"
#define KEY_DEVICE_ID       "device_id"
#define KEY_DEVICE_TOKEN    "device_token"

#endif // CONFIG_H
