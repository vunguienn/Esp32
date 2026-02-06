# ESP32-S3 IoT Gateway for Cloud Grow

Firmware cho mạch ESP32-S3 để điều khiển hệ thống trồng trọt thông minh.

## 🔧 Thông số phần cứng

| Thành phần | Chi tiết |
|------------|----------|
| MCU | ESP32-S3 1U-N4 (Dual-core Xtensa LX7 @240MHz) |
| WiFi/BLE | WiFi/BLE 5.0 |
| Digital Input | 8 kênh (optocoupler cách ly, 12-24VDC) |
| Digital Output | 8 kênh Relay |
| ADC | ADS1115, 4 kênh 16-bit (0-10V hoặc 4-20mA) |
| RS485 | ADUM 1201 cách ly, 25Mbps |
| Ethernet | W5500 (SPI) |
| Nguồn | 24VDC - 2A |

## 📁 Cấu trúc project

```
esp32/
├── platformio.ini          # PlatformIO configuration
├── include/
│   ├── config.h            # Hardware pin definitions & settings
│   ├── wifi_manager.h      # WiFi AP provisioning
│   ├── mqtt_handler.h      # MQTT communication
│   ├── sensor_manager.h    # RS485 sensors
│   └── relay_controller.h  # 8-channel relay control
├── src/
│   ├── main.cpp            # Main firmware
│   ├── wifi_manager.cpp
│   ├── mqtt_handler.cpp
│   ├── sensor_manager.cpp
│   └── relay_controller.cpp
└── README.md
```

## 🚀 Cài đặt

### 1. Cài đặt PlatformIO

```bash
# VS Code
# Cài đặt extension "PlatformIO IDE"

# Hoặc CLI
pip install platformio
```

### 2. Build & Upload

```bash
cd esp32

# Build
pio run

# Upload
pio run --target upload

# Monitor
pio device monitor
```

## 📡 MQTT Topics

### ESP32 → Cloud (Publish)

| Topic | Mô tả | Payload |
|-------|-------|---------|
| `grow/{roomId}/sensors` | Dữ liệu cảm biến | `{"temp":25.5,"humidity":65,"co2":800,"vpd":1.2,...}` |
| `grow/{roomId}/status` | Trạng thái gateway | `{"online":true,"ip":"192.168.1.100","rssi":-45,...}` |
| `grow/{roomId}/ack` | Xác nhận lệnh | `{"commandId":"...","success":true}` |
| `device/{deviceId}/confirm` | Xác nhận verification | `{"valid":true,"requestId":"..."}` |

### Cloud → ESP32 (Subscribe)

| Topic | Mô tả | Payload |
|-------|-------|---------|
| `grow/{roomId}/control` | Điều khiển relay | `{"relay1":true,"relay2":false,...}` |
| `device/{deviceId}/verify` | Yêu cầu xác thực | `{"requestId":"...","token":"..."}` |
| `device/{deviceId}/pair` | Thông tin ghép nối | `{"roomId":"..."}` |

## 🔌 Relay Mapping

| Relay | Chức năng | Pin |
|-------|-----------|-----|
| relay1 | Đèn chính | GPIO 35 |
| relay2 | Quạt tuần hoàn | GPIO 36 |
| relay3 | Quạt hút | GPIO 37 |
| relay4 | Bơm 1 (tưới) | GPIO 38 |
| relay5 | Bơm 2 | GPIO 39 |
| relay6 | Bơm 3 | GPIO 40 |
| relay7 | Van CO2 | GPIO 41 |
| relay8 | Trigger điều hòa | GPIO 42 |

## 🌐 WiFi Provisioning (AP Mode)

Khi chưa cấu hình WiFi hoặc kết nối thất bại:

1. ESP32 tạo AP với SSID: `GrowGateway_XXXX` (4 số cuối MAC)
2. Mật khẩu mặc định: `grow12345`
3. Kết nối và truy cập: `http://192.168.4.1`
4. Nhập thông tin WiFi và MQTT server
5. Thiết bị tự động restart và kết nối

## 🔐 Ghép nối thiết bị với Cloud

### Quy trình:

1. **Lấy Device ID**: Mỗi ESP32 có Device ID dựa trên MAC address
2. **Tạo Token**: Cloud tạo token ngẫu nhiên
3. **Nạp Token vào ESP32**: Qua AP mode hoặc serial
4. **Test kết nối**: Cloud gửi verify request, ESP32 phản hồi
5. **Tạo phòng**: Nếu xác thực thành công, Cloud tạo Room và gửi Room ID cho ESP32

### Từ Cloud Dashboard:

```
1. Vào "Thêm phòng mới"
2. Chọn "MCU Gateway"
3. Nhập Device ID (từ màn hình ESP32 hoặc sticker)
4. Nhập Token (đã nạp vào ESP32)
5. Nhấn "Test kết nối"
6. Nếu thành công → Nhập tên phòng → Tạo
```

## 📊 Mô phỏng cảm biến

Khi chưa có cảm biến RS485 thực, firmware sẽ tự động mô phỏng:

```cpp
// config.h
#define ENABLE_SENSOR_SIMULATION   true
```

Giá trị mô phỏng:
- Nhiệt độ: 22-28°C
- Độ ẩm: 55-75%
- CO2: 600-1400 ppm
- Nhiệt độ nước: 20-25°C
- EC: 1.0-2.5 mS/cm
- pH: 5.5-6.5
- Mực nước: 30-95%

## 🖥️ Serial Commands (Debug)

| Lệnh | Mô tả |
|------|-------|
| `status` | Hiển thị trạng thái hệ thống |
| `sensors` | Đọc tất cả cảm biến |
| `relays` | Hiển thị trạng thái relay |
| `relay1=on` | Bật relay 1 |
| `relay1=off` | Tắt relay 1 |
| `reset` | Factory reset |
| `reboot` | Khởi động lại |
| `help` | Hiển thị danh sách lệnh |

## ⚡ LED Indicators

| LED | Trạng thái | Ý nghĩa |
|-----|-----------|---------|
| STATUS | Blink nhanh | AP mode đang chờ cấu hình |
| STATUS | Blink chậm | Đang hoạt động bình thường |
| WIFI | Sáng | Đã kết nối WiFi |
| WIFI | Tắt | Mất kết nối WiFi |
| MQTT | Sáng | Đã kết nối MQTT |
| MQTT | Blink | Đang gửi/nhận dữ liệu |

## 🔧 Cấu hình MQTT mặc định

```cpp
// config.h
#define DEFAULT_MQTT_SERVER     "192.168.1.100"  // IP Cloud server
#define DEFAULT_MQTT_PORT       1883
```

## 📋 TODO

- [ ] Tích hợp Modbus RTU cho cảm biến RS485 thực
- [ ] OTA update
- [ ] Ethernet fallback khi WiFi mất
- [ ] Watchdog timer
- [ ] HTTPS/MQTTS support
- [ ] Local data logging (SD card hoặc LittleFS)

## 📞 Liên hệ

Xem thêm tài liệu tại thư mục gốc của dự án Cloud Grow.
