# MQTT Protocol Specification - Cloud Grow

## 📡 Tổng quan

Tài liệu này mô tả chi tiết các MQTT topics và message formats được sử dụng trong hệ thống Cloud Grow.

---

## 🔑 Topic Structure

### Room Topics (sau khi paired)

```
grow/{roomId}/sensors    # ESP32 → Cloud: Dữ liệu cảm biến
grow/{roomId}/control    # Cloud → ESP32: Lệnh điều khiển
grow/{roomId}/status     # ESP32 → Cloud: Trạng thái gateway
grow/{roomId}/ack        # ESP32 → Cloud: Xác nhận lệnh
```

### Device Topics (trước khi paired)

```
device/register              # ESP32 → Cloud: Đăng ký device mới
device/{deviceId}/verify     # Cloud → ESP32: Yêu cầu xác minh
device/{deviceId}/confirm    # ESP32 → Cloud: Xác nhận verify
device/{deviceId}/pair       # Cloud → ESP32: Gán roomId
device/{deviceId}/status     # ESP32 → Cloud: Online/Offline (LWT)
```

---

## 📤 ESP32 → Cloud Messages

### 1. Sensor Data

**Topic:** `grow/{roomId}/sensors`

**Payload:**
```json
{
  "temp": 25.5,          // Nhiệt độ (°C)
  "humidity": 65.2,      // Độ ẩm (%)
  "co2": 850,            // CO2 (ppm)
  "vpd": 1.12,           // Vapor Pressure Deficit (kPa)
  "waterTemp": 22.0,     // Nhiệt độ nước (°C) - optional
  "ec": 1.8,             // Electrical Conductivity (mS/cm) - optional
  "ph": 6.2,             // pH - optional
  "waterLevel": 75,      // Mực nước (%) - optional
  "timestamp": 1234567890
}
```

**Publish Interval:** 30 giây (configurable)

**QoS:** 0 (At most once)

**Retained:** false

---

### 2. Gateway Status

**Topic:** `grow/{roomId}/status`

**Payload:**
```json
{
  "online": true,
  "ip": "192.168.1.105",
  "rssi": -65,           // WiFi signal (dBm)
  "uptime": 3600,        // Seconds since boot
  "freeHeap": 180000,    // Free memory (bytes)
  "firmware": "1.0.0"
}
```

**Publish:** On connect + every 60 seconds

**QoS:** 1

**Retained:** true

---

### 3. Command Acknowledgement

**Topic:** `grow/{roomId}/ack`

**Payload:**
```json
{
  "commandId": "cmd-12345",
  "success": true,
  "timestamp": 1234567890,
  "message": "Command executed"
}
```

**Publish:** After processing control command

**QoS:** 1

**Retained:** false

---

### 4. Device Registration

**Topic:** `device/register`

**Payload:**
```json
{
  "deviceId": "ESP32-001",
  "model": "ESP32-S3-GROW",
  "firmware": "1.0.0",
  "timestamp": 1234567890
}
```

**Publish:** On first connection (không có roomId)

**QoS:** 1

**Retained:** false

---

### 5. Verify Confirmation

**Topic:** `device/{deviceId}/confirm`

**Payload (success):**
```json
{
  "valid": true,
  "requestId": "req-001",
  "deviceId": "ESP32-001",
  "model": "ESP32-S3-GROW",
  "firmware": "1.0.0",
  "message": "Device verified successfully"
}
```

**Payload (failed):**
```json
{
  "valid": false,
  "requestId": "req-001",
  "deviceId": "ESP32-001",
  "message": "Invalid token"
}
```

**QoS:** 1

**Retained:** false

---

### 6. Last Will Testament (LWT)

**Topic:** `device/{deviceId}/status`

**Payload (online - published manually):**
```json
{
  "online": true,
  "deviceId": "ESP32-001"
}
```

**Payload (offline - set as LWT):**
```json
{
  "online": false
}
```

**QoS:** 1

**Retained:** true

---

## 📥 Cloud → ESP32 Messages

### 1. Relay Control

**Topic:** `grow/{roomId}/control`

**Payload:**
```json
{
  "commandId": "cmd-12345",
  "relay1": true,        // Bật relay 1
  "relay2": false,       // Tắt relay 2
  "relay3": true         // Bật relay 3
  // relay4-8 không có = giữ nguyên
}
```

**Relay Mapping:**
| Key | Relay | GPIO | Mục đích thường dùng |
|-----|-------|------|---------------------|
| relay1 | 1 | 4 | Quạt thông gió |
| relay2 | 2 | 5 | Quạt làm mát |
| relay3 | 3 | 6 | Bơm tưới 1 |
| relay4 | 4 | 7 | Bơm tưới 2 |
| relay5 | 5 | 15 | Đèn grow |
| relay6 | 6 | 16 | Máy tạo ẩm |
| relay7 | 7 | 17 | CO2 valve |
| relay8 | 8 | 18 | Spare |

**QoS:** 1

---

### 2. Verify Request

**Topic:** `device/{deviceId}/verify`

**Payload:**
```json
{
  "token": "abc123xyz",
  "requestId": "req-001",
  "timestamp": 1234567890
}
```

**QoS:** 1

---

### 3. Pair Assignment

**Topic:** `device/{deviceId}/pair`

**Payload:**
```json
{
  "roomId": "room-001",
  "roomName": "Grow Room 1",
  "timestamp": 1234567890
}
```

**QoS:** 1

---

## 🔄 Message Flow Diagrams

### Device Pairing Flow

```
┌─────────┐          ┌─────────────┐          ┌───────────┐
│  ESP32  │          │ MQTT Broker │          │   Cloud   │
└────┬────┘          └──────┬──────┘          └─────┬─────┘
     │                      │                       │
     │ 1. CONNECT           │                       │
     │─────────────────────>│                       │
     │                      │                       │
     │ 2. SUBSCRIBE         │                       │
     │  device/ESP32-001/*  │                       │
     │─────────────────────>│                       │
     │                      │                       │
     │                      │ 3. PUBLISH verify     │
     │                      │<──────────────────────│
     │ 4. MESSAGE           │                       │
     │<─────────────────────│                       │
     │                      │                       │
     │ 5. PUBLISH confirm   │                       │
     │─────────────────────>│                       │
     │                      │ 6. MESSAGE            │
     │                      │──────────────────────>│
     │                      │                       │
     │                      │ 7. PUBLISH pair       │
     │                      │<──────────────────────│
     │ 8. MESSAGE           │                       │
     │<─────────────────────│                       │
     │                      │                       │
     │ 9. SUBSCRIBE         │                       │
     │  grow/room-001/*     │                       │
     │─────────────────────>│                       │
     │                      │                       │
     │ 10. PUBLISH status   │                       │
     │─────────────────────>│                       │
     │                      │                       │
```

### Relay Control Flow

```
┌─────────┐          ┌─────────────┐          ┌───────────┐
│  ESP32  │          │ MQTT Broker │          │   Cloud   │
└────┬────┘          └──────┬──────┘          └─────┬─────┘
     │                      │                       │
     │                      │ 1. PUBLISH control    │
     │                      │  {"relay1": true}     │
     │                      │<──────────────────────│
     │ 2. MESSAGE           │                       │
     │<─────────────────────│                       │
     │                      │                       │
     │ [Toggle Relay 1]     │                       │
     │                      │                       │
     │ 3. PUBLISH ack       │                       │
     │  {"success": true}   │                       │
     │─────────────────────>│                       │
     │                      │ 4. MESSAGE            │
     │                      │──────────────────────>│
     │                      │                       │
```

### Sensor Publishing Flow

```
┌─────────┐          ┌─────────────┐          ┌───────────┐
│  ESP32  │          │ MQTT Broker │          │   Cloud   │
└────┬────┘          └──────┬──────┘          └─────┬─────┘
     │                      │                       │
     │ [Read Sensors]       │                       │
     │                      │                       │
     │ 1. PUBLISH sensors   │                       │
     │  {"temp":25.5,...}   │                       │
     │─────────────────────>│                       │
     │                      │ 2. MESSAGE            │
     │                      │──────────────────────>│
     │                      │                       │
     │        [Every 30 seconds]                    │
     │                      │                       │
```

---

## ⚙️ Configuration Constants

### ESP32 Side (`config.h`)

```cpp
#define DEFAULT_MQTT_SERVER     "localhost"
#define DEFAULT_MQTT_PORT       1883
#define MQTT_KEEPALIVE          60      // seconds
#define MQTT_RECONNECT_DELAY    5000    // ms
#define SENSOR_PUBLISH_INTERVAL 30000   // ms
#define STATUS_PUBLISH_INTERVAL 60000   // ms
```

### Cloud Side (`.env`)

```env
MQTT_BROKER_HOST=localhost
MQTT_BROKER_PORT=1883
MQTT_USERNAME=
MQTT_PASSWORD=
```

---

## 🔐 Security Considerations

### Current Implementation
- Device Token authentication
- No TLS (for local network)

### Production Recommendations
1. **Enable TLS:**
   ```cpp
   _mqttClient->setSecure();
   ```

2. **Use MQTT username/password:**
   ```cpp
   _mqttClient->connect(clientId, username, password);
   ```

3. **Implement ACL:**
   - ESP32 chỉ được publish to: `grow/{ownRoomId}/*`, `device/{ownId}/*`
   - ESP32 chỉ được subscribe to: `grow/{ownRoomId}/control`, `device/{ownId}/*`

---

## 📊 Error Codes

### MQTT Connection Errors

| Code | Description | Solution |
|------|-------------|----------|
| -4 | MQTT_CONNECTION_TIMEOUT | Kiểm tra network |
| -3 | MQTT_CONNECTION_LOST | Reconnect |
| -2 | MQTT_CONNECT_FAILED | Kiểm tra broker |
| -1 | MQTT_DISCONNECTED | Reconnect |
| 1 | MQTT_CONNECT_BAD_PROTOCOL | Update client |
| 2 | MQTT_CONNECT_BAD_CLIENT_ID | Change client ID |
| 3 | MQTT_CONNECT_UNAVAILABLE | Broker overloaded |
| 4 | MQTT_CONNECT_BAD_CREDENTIALS | Check user/pass |
| 5 | MQTT_CONNECT_UNAUTHORIZED | Check ACL |

---

## 📝 Example Code

### Subscribe and Handle Control

```cpp
void handleControlMessage(JsonDocument& doc) {
    for (int i = 1; i <= 8; i++) {
        char key[8];
        sprintf(key, "relay%d", i);
        
        if (doc.containsKey(key)) {
            bool state = doc[key].as<bool>();
            relayController.setRelay(i, state);
        }
    }
}
```

### Publish Sensor Data

```cpp
void publishSensors() {
    JsonDocument doc;
    doc["temp"] = sensorManager.getTemperature();
    doc["humidity"] = sensorManager.getHumidity();
    doc["co2"] = sensorManager.getCO2();
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    
    mqttClient.publish("grow/room-001/sensors", output.c_str());
}
```

---

*MQTT Protocol v1.0 - Cloud Grow System*
