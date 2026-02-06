# ESP32-S3 Cloud Grow Gateway - Hướng Dẫn Cài Đặt Chi Tiết

## 📋 Mục Lục

1. [Yêu cầu phần cứng](#1-yêu-cầu-phần-cứng)
2. [Cài đặt môi trường phát triển](#2-cài-đặt-môi-trường-phát-triển)
3. [Sơ đồ nối dây](#3-sơ-đồ-nối-dây)
4. [Build và nạp firmware](#4-build-và-nạp-firmware)
5. [Cấu hình thiết bị](#5-cấu-hình-thiết-bị)
6. [Ghép nối với Cloud](#6-ghép-nối-với-cloud)
7. [Test MQTT](#7-test-mqtt)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. Yêu cầu phần cứng

### Board chính
- **ESP32-S3 1U-N4** (Dual-core Xtensa LX7 @ 240MHz)
- Flash: 4MB
- PSRAM: 2MB (optional)
- WiFi 2.4GHz + BLE 5.0

### Ngoại vi
| Thành phần | Model | Số lượng | Ghi chú |
|------------|-------|----------|---------|
| Digital Input | Optocoupler PC817 | 8 kênh | 24V DC input |
| Digital Output | Relay 5V | 8 kênh | 10A 250VAC |
| ADC | ADS1115 | 1 | 16-bit, 4 kênh |
| RS485 | ADUM 1201 | 1 | Isolated transceiver |
| Ethernet | W5500 | 1 | SPI interface (optional) |

---

## 2. Cài đặt môi trường phát triển

### Option A: VS Code + PlatformIO (Khuyến nghị)

#### Bước 1: Cài đặt VS Code
```bash
# Ubuntu/Debian
sudo snap install code --classic

# Hoặc download từ https://code.visualstudio.com/
```

#### Bước 2: Cài đặt PlatformIO Extension
1. Mở VS Code
2. Vào Extensions (Ctrl+Shift+X)
3. Tìm "PlatformIO IDE"
4. Click Install
5. Restart VS Code

#### Bước 3: Mở project
```bash
# Mở folder esp32 trong VS Code
code /path/to/Cloud/esp32
```

### Option B: Command Line

#### Cài đặt PlatformIO Core
```bash
# Cài pip nếu chưa có
sudo apt install python3-pip

# Cài PlatformIO
pip3 install platformio

# Thêm vào PATH
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Verify
pio --version
```

---

## 3. Sơ đồ nối dây

### GPIO Mapping

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3 GPIO Layout                     │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────┐           ┌─────────────────────────┐  │
│  │   RELAY OUTPUT  │           │      OPTOCOUPLER IN     │  │
│  │   (8 channels)  │           │      (8 channels)       │  │
│  ├─────────────────┤           ├─────────────────────────┤  │
│  │ GPIO 4  → Relay1│           │ GPIO 35 ← Input 1       │  │
│  │ GPIO 5  → Relay2│           │ GPIO 36 ← Input 2       │  │
│  │ GPIO 6  → Relay3│           │ GPIO 37 ← Input 3       │  │
│  │ GPIO 7  → Relay4│           │ GPIO 38 ← Input 4       │  │
│  │ GPIO 15 → Relay5│           │ GPIO 39 ← Input 5       │  │
│  │ GPIO 16 → Relay6│           │ GPIO 40 ← Input 6       │  │
│  │ GPIO 17 → Relay7│           │ GPIO 41 ← Input 7       │  │
│  │ GPIO 18 → Relay8│           │ GPIO 42 ← Input 8       │  │
│  └─────────────────┘           └─────────────────────────┘  │
│                                                             │
│  ┌─────────────────┐           ┌─────────────────────────┐  │
│  │   I2C (ADS1115) │           │       RS485 (UART)      │  │
│  ├─────────────────┤           ├─────────────────────────┤  │
│  │ GPIO 8  → SDA   │           │ GPIO 43 → TX            │  │
│  │ GPIO 9  → SCL   │           │ GPIO 44 ← RX            │  │
│  │                 │           │ GPIO 45 → DE/RE         │  │
│  └─────────────────┘           └─────────────────────────┘  │
│                                                             │
│  ┌─────────────────┐           ┌─────────────────────────┐  │
│  │   LED Status    │           │      W5500 Ethernet     │  │
│  ├─────────────────┤           ├─────────────────────────┤  │
│  │ GPIO 2  → LED   │           │ GPIO 10 → CS            │  │
│  │                 │           │ GPIO 11 → MOSI          │  │
│  │                 │           │ GPIO 12 → MISO          │  │
│  │                 │           │ GPIO 13 → SCK           │  │
│  │                 │           │ GPIO 14 → RST           │  │
│  └─────────────────┘           └─────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### ADS1115 ADC Wiring

```
    ADS1115              ESP32-S3
    ┌─────┐             ┌────────┐
    │ VDD ├─────────────┤ 3.3V   │
    │ GND ├─────────────┤ GND    │
    │ SCL ├─────────────┤ GPIO 9 │
    │ SDA ├─────────────┤ GPIO 8 │
    │ADDR ├─────────────┤ GND    │  (Address 0x48)
    │ALRT │             │        │  (Optional)
    │ A0  ├──── Sensor 1 (0-5V)
    │ A1  ├──── Sensor 2 (0-5V)
    │ A2  ├──── Sensor 3 (0-5V)
    │ A3  ├──── Sensor 4 (0-5V)
    └─────┘
```

### RS485 Wiring (ADUM1201)

```
    ADUM1201             ESP32-S3         RS485 Device
    ┌─────┐             ┌────────┐        ┌──────────┐
    │VDD1 ├─────────────┤ 3.3V   │        │          │
    │GND1 ├─────────────┤ GND    │        │          │
    │VIA  ├─────────────┤GPIO 43 │ TX     │          │
    │VIB  ├─────────────┤GPIO 44 │ RX     │          │
    │VOA  ├─────────────────────────────── A+ (Data+)│
    │VOB  ├─────────────────────────────── B- (Data-)│
    │VDD2 ├─────────────┤ 5V     │        │          │
    │GND2 ├─────────────┤ GND    │        │          │
    └─────┘             └────────┘        └──────────┘

    ┌──────────────────────────────────────────────────┐
    │ DE/RE Control: GPIO 45                           │
    │ - HIGH = Transmit mode                          │
    │ - LOW  = Receive mode                           │
    └──────────────────────────────────────────────────┘
```

---

## 4. Build và nạp firmware

### Từ VS Code (PlatformIO)

1. **Build**
   - Click biểu tượng ✓ trên thanh status bar
   - Hoặc: `Ctrl+Alt+B`

2. **Upload**
   - Kết nối ESP32-S3 qua USB
   - Click biểu tượng → trên status bar
   - Hoặc: `Ctrl+Alt+U`

3. **Monitor Serial**
   - Click biểu tượng 🔌 trên status bar
   - Hoặc: `Ctrl+Alt+S`

### Từ Command Line

```bash
cd /path/to/Cloud/esp32

# Build
pio run

# Upload (kết nối ESP32 qua USB)
pio run --target upload

# Monitor serial
pio device monitor --baud 115200

# Build + Upload + Monitor
pio run --target upload && pio device monitor
```

### Build environments

```bash
# Build cho production
pio run -e esp32-s3

# Clean build
pio run --target clean
```

---

## 5. Cấu hình thiết bị

### Lần đầu khởi động

1. **Kết nối nguồn** cho ESP32
2. **Đợi LED nhấp nháy nhanh** - thiết bị đang vào chế độ AP
3. **Kết nối WiFi** từ điện thoại/laptop:
   - SSID: `Cloud-Grow-XXXX` (XXXX = 4 số cuối MAC address)
   - Password: `12345678`

### Cấu hình qua Web Portal

1. **Truy cập** `http://192.168.4.1` trên trình duyệt
2. **Nhập thông tin WiFi:**
   - SSID: Tên WiFi nhà/công ty
   - Password: Mật khẩu WiFi

3. **Nhập thông tin MQTT:**
   - Server: IP của MQTT broker (ví dụ: `192.168.1.100`)
   - Port: `1883` (mặc định)
   - Username: (để trống nếu không có)
   - Password: (để trống nếu không có)

4. **Nhập Device Credentials:**
   - Device ID: ID duy nhất (từ Cloud)
   - Device Token: Token xác thực (từ Cloud)

5. **Click "Save"** - thiết bị sẽ restart và kết nối WiFi

---

## 6. Ghép nối với Cloud

### Lấy Device ID và Token từ Cloud

```bash
# 1. Tạo token mới cho device
curl -X POST http://localhost:4000/api/devices/generate-token \
  -H "Content-Type: application/json" \
  -d '{"deviceId": "ESP32-001"}'

# Response:
# {
#   "success": true,
#   "deviceId": "ESP32-001",
#   "token": "abc123xyz"
# }
```

### Ghép nối device với Room

```bash
# 2. Verify device (sau khi ESP32 đã kết nối MQTT)
curl -X POST http://localhost:4000/api/devices/verify \
  -H "Content-Type: application/json" \
  -d '{
    "deviceId": "ESP32-001",
    "token": "abc123xyz"
  }'

# 3. Ghép với room
curl -X POST http://localhost:4000/api/devices/pair \
  -H "Content-Type: application/json" \
  -d '{
    "deviceId": "ESP32-001",
    "roomName": "Grow Room 1"
  }'
```

### Hoặc dùng Frontend

1. Mở trang **Devices** hoặc **Rooms**
2. Click **"Thêm thiết bị mới"**
3. Nhập Device ID hiển thị trên Serial Monitor
4. Click **"Xác minh"**
5. Nếu thành công, chọn tên Room và click **"Ghép nối"**

---

## 7. Test MQTT

### Cài đặt Mosquitto client

```bash
sudo apt install mosquitto-clients
```

### Subscribe để xem messages

```bash
# Terminal 1: Xem tất cả messages
mosquitto_sub -h localhost -p 1883 -t '#' -v

# Terminal 2: Chỉ xem sensor data
mosquitto_sub -h localhost -p 1883 -t 'grow/+/sensors' -v

# Terminal 3: Xem device status
mosquitto_sub -h localhost -p 1883 -t 'device/+/status' -v
```

### Gởi lệnh điều khiển relay

```bash
# Bật relay 1
mosquitto_pub -h localhost -p 1883 -t 'grow/room-001/control' \
  -m '{"relay1": true, "commandId": "cmd-001"}'

# Tắt relay 1
mosquitto_pub -h localhost -p 1883 -t 'grow/room-001/control' \
  -m '{"relay1": false, "commandId": "cmd-002"}'

# Bật nhiều relay
mosquitto_pub -h localhost -p 1883 -t 'grow/room-001/control' \
  -m '{"relay1": true, "relay2": true, "relay5": true, "commandId": "cmd-003"}'
```

### Test verify flow

```bash
# Gởi verify request
mosquitto_pub -h localhost -p 1883 -t 'device/ESP32-001/verify' \
  -m '{"token": "abc123xyz", "requestId": "req-001"}'

# Xem confirm response
mosquitto_sub -h localhost -p 1883 -t 'device/ESP32-001/confirm'
```

---

## 8. Troubleshooting

### ❌ Không thấy WiFi AP "Cloud-Grow-XXXX"

**Nguyên nhân:** ESP32 đã có WiFi config hoặc chưa vào AP mode

**Giải pháp:**
```cpp
// Reset WiFi config qua Serial:
// Gởi lệnh "RESET" qua Serial Monitor
```

Hoặc flash lại với `FORCE_AP_MODE = true`:
```cpp
// config.h
#define FORCE_AP_MODE       true
```

### ❌ Không kết nối được WiFi

**Kiểm tra:**
1. SSID và password đúng chưa?
2. WiFi 2.4GHz (không phải 5GHz)?
3. Signal strength đủ mạnh?

**Debug qua Serial:**
```
[WiFi] Connecting to YourSSID...
[WiFi] Failed! Reason: 2  ← Check error code
```

Error codes:
- 1: SSID not found
- 2: Wrong password
- 4: Connection failed
- 5: Lost connection

### ❌ Không kết nối được MQTT

**Kiểm tra:**
```bash
# Test MQTT broker có chạy không
nc -zv localhost 1883

# Check MQTT logs
sudo journalctl -u mosquitto -f
```

**Firewall:**
```bash
# Mở port 1883
sudo ufw allow 1883
```

### ❌ Không nhận được sensor data

**Kiểm tra:**
1. Device đã paired chưa? (`isPaired()` = true)
2. Topic đúng format: `grow/{roomId}/sensors`
3. Simulation mode có bật không?

**Enable simulation:**
```cpp
// config.h
#define SIMULATION_MODE     true
#define SIMULATION_INTERVAL 5000  // 5 seconds
```

### ❌ Relay không hoạt động

**Kiểm tra:**
1. Subscribe đúng topic `grow/{roomId}/control`
2. JSON format đúng: `{"relay1": true}`
3. Relay callback đã được set

**Test manual qua Serial:**
```
// Gởi qua Serial Monitor:
RELAY:1:ON
RELAY:2:OFF
```

### 📊 Debug Info

**Serial Output khi hoạt động bình thường:**
```
[BOOT] Cloud Grow Gateway v1.0.0
[BOOT] Device ID: ESP32-001
[WiFi] Connected to MyWiFi
[WiFi] IP: 192.168.1.105
[MQTT] Connecting as grow-ESP32-001-1234...
[MQTT] Connected!
[MQTT] Subscribed to: device/ESP32-001/verify
[MQTT] Subscribed to: device/ESP32-001/pair
[MQTT] Subscribed to: grow/room-001/control
[SENSOR] Temp: 25.3°C, Humidity: 65.2%, CO2: 850ppm
[MQTT] Published sensors: T=25.3 H=65.2 CO2=850
```

### 🔄 Factory Reset

Để reset hoàn toàn về mặc định:

```cpp
// Thêm vào setup() hoặc gởi lệnh qua Serial
Preferences prefs;
prefs.begin("cloud-grow", false);
prefs.clear();
prefs.end();
ESP.restart();
```

---

## 📚 Tài liệu tham khảo

- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [PubSubClient Library](https://github.com/knolleary/pubsubclient)
- [ArduinoJson](https://arduinojson.org/)
- [Adafruit ADS1X15](https://github.com/adafruit/Adafruit_ADS1X15)

---

*Tài liệu này được tạo cho dự án Cloud Grow Gateway*
*Phiên bản: 1.0.0*
*Cập nhật: 2024*
