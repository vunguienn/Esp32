# ESP32 Cloud Grow - Quick Reference Card

## 🚀 Quick Commands

```bash
# Build
cd /path/to/Cloud/esp32 && pio run

# Upload
pio run --target upload

# Monitor (115200 baud)
pio device monitor

# All-in-one
pio run -t upload && pio device monitor
```

## 📡 MQTT Topics Cheat Sheet

| Direction | Topic | Purpose |
|-----------|-------|---------|
| ESP→Cloud | `grow/{roomId}/sensors` | Sensor data |
| ESP→Cloud | `grow/{roomId}/status` | Online status |
| Cloud→ESP | `grow/{roomId}/control` | Relay commands |
| ESP→Cloud | `device/{id}/confirm` | Verify response |
| Cloud→ESP | `device/{id}/verify` | Verify request |
| Cloud→ESP | `device/{id}/pair` | Room assignment |

## 🔧 Test Commands

```bash
# Subscribe all
mosquitto_sub -h localhost -t '#' -v

# Test relay control
mosquitto_pub -h localhost -t 'grow/room-001/control' \
  -m '{"relay1":true,"commandId":"test1"}'

# Test verify
mosquitto_pub -h localhost -t 'device/ESP32-001/verify' \
  -m '{"token":"abc123","requestId":"test"}'
```

## 📌 GPIO Mapping

```
Relays: GPIO 4,5,6,7,15,16,17,18
Inputs: GPIO 35,36,37,38,39,40,41,42
I2C:    SDA=8, SCL=9
RS485:  TX=43, RX=44, DE=45
LED:    GPIO 2
```

## 🔑 Key Files

```
include/config.h      ← Pin definitions, MQTT settings
src/main.cpp          ← Main loop
src/wifi_manager.cpp  ← AP mode, web portal
src/mqtt_handler.cpp  ← MQTT pub/sub
src/sensor_manager.cpp ← Sensor reading
src/relay_controller.cpp ← Relay control
```

## ⚡ Default Settings

| Setting | Value |
|---------|-------|
| AP SSID | Cloud-Grow-XXXX |
| AP Password | 12345678 |
| AP Portal IP | 192.168.4.1 |
| MQTT Port | 1883 |
| Sensor Interval | 30 sec |
| Serial Baud | 115200 |

## 🐛 Troubleshooting

```bash
# Reset WiFi config
# In Serial Monitor, type: RESET

# Check MQTT broker
nc -zv localhost 1883

# View ESP32 logs
pio device monitor --filter esp32_exception_decoder
```

## 📦 Dependencies

```ini
# platformio.ini
lib_deps =
  knolleary/PubSubClient
  bblanchon/ArduinoJson
  adafruit/Adafruit ADS1X15
  tzapu/WiFiManager
```

---
*v1.0.0 | Cloud Grow ESP32 Gateway*
