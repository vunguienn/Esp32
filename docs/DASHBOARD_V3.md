# 📊 Dashboard v3 - Hướng Dẫn Chi Tiết

## 🎯 Giới Thiệu

Dashboard v3 được thiết kế hoàn toàn mới với giao diện Tiếng Việt, tối ưu cho việc quản lý hệ thống trồng cây tự động dựa trên cấu trúc JSON v3. 

Dashboard hiển thị thông tin theo từng **giai đoạn (Phase)** và **tuần (Week)** của vòng đời cây trồng.

---

## 📱 Giao Diện Dashboard

### 1. **Header (Phần Header Trên Cùng)**

```
🌱 Hệ Thống Trồng Cây Tự Động

[✅ WiFi: 192.168.100.71]  [🔴 MQTT: Chưa kết nối]  [🕐 14:35:22]
```

**Thông Tin:**
- ✅/🔴: Trạng thái kết nối
- WiFi: IP địa chỉ của ESP32
- MQTT: Trạng thái kết nối MQTT
- 🕐: Thời gian hiện tại

---

### 2. **Tabs (Thanh Điều Hướng)**

```
📊 Tổng Quan | 📅 Lịch Hoạt Động | ⚡ Relay & I/O | ⚙️ Thiết Bị | 🔧 Hệ Thống
```

**Các Tab:**

#### 🔴 **TAB 1: Tổng Quan (Overview)**

Hiển thị tất cả thông tin quan trọng:

```
┌─────────────────────────────────────────────┐
│ 🌱 Giai Đoạn Hiện Tại                      │
│                                             │
│ VEG (Rau Xanh)                              │
│ [===========50%===========]                 │
│ Tuần 1 / 2  |  Tuần Toàn Bộ 1 / 16         │
└─────────────────────────────────────────────┘

┌─────────────┬─────────────┬─────────────┬─────────────┐
│ Nhiệt độ    │ Nhiệt độ    │ Độ Ẩm Ngày  │ Độ Ẩm Đêm   │
│ Ngày        │ Đêm         │             │             │
│             │             │             │             │
│ 26°C        │ 20°C        │ 70%         │ 65%         │
└─────────────┴─────────────┴─────────────┴─────────────┘

┌─────────────┬─────────────┬─────────────┬─────────────┐
│ CO2 Ngày    │ CO2 Đêm     │ VPD Min     │ VPD Max     │
│             │             │             │             │
│ 1200 ppm    │ 500 ppm     │ 0.8         │ 1.2         │
└─────────────┴─────────────┴─────────────┴─────────────┘

┌──────────────────────────────────────────────────────────┐
│ 🌡️ Cảm Biến Hiện Tại                                     │
│                                                          │
│ Nhiệt Độ: 25.6°C  │  Độ Ẩm: 68%                         │
│ CO2: 1150 ppm    │  VPD: 0.95 kPa                      │
└──────────────────────────────────────────────────────────┘
```

**Giải Thích:**
- **Giai Đoạn:** VEG (Rau Xanh) - Tuần 1 của 2 tuần
- **Thanh Tiến Trình:** Hiển thị bao nhiêu % đã hoàn thành
- **Mục Tiêu:** Nhiệt độ, độ ẩm, CO2 mục tiêu cho tuần này
- **Cảm Biến:** Dữ liệu thực tế từ các sensor

---

#### 🟢 **TAB 2: Lịch Hoạt Động (Schedule)**

Hiển thị lịch bật/tắt thiết bị:

```
┌─────────────────────────────────────────────────────┐
│ 💡 Lịch Sáng & Điều Chỉnh Độ Sáng (PWM)             │
│                                                     │
│ Giờ On/Off: 06:00 - 18:00                           │
│                                                     │
│ ┌─────────┬──────────┬──────┬──────┬───────┐        │
│ │ Thời Gian│ Độ Sáng% │⚪White│🟡Yellow│🔴Red│        │
│ ├─────────┼──────────┼──────┼──────┼───────┤        │
│ │ 06:00   │ 30%      │  0%  │ 30%  │ 10%   │        │
│ │ 09:00   │ 60%      │ 30%  │ 40%  │ 20%   │        │
│ │ 12:00   │ 100%     │100%  │ 60%  │ 30%   │        │
│ │ 15:00   │ 60%      │ 30%  │ 40%  │ 20%   │        │
│ │ 18:00   │ 0%       │  0%  │  0%  │  0%   │        │
│ └─────────┴──────────┴──────┴──────┴───────┘        │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ 💧 Lịch Tưới Nước                                    │
│                                                     │
│ ┌─────────────┬──────────────┬────────┬──────────┐  │
│ │ Thời Gian   │ Thời Lượng    │ Lưu L.│ Trạng Thái│  │
│ ├─────────────┼──────────────┼────────┼──────────┤  │
│ │ 08:00       │ 15 phút       │ 2.5/ph│ Chủ Động │  │
│ │ 16:00       │ 20 phút       │ 2.5/ph│ Chủ Động │  │
│ └─────────────┴──────────────┴────────┴──────────┘  │
└─────────────────────────────────────────────────────┘
```

**PWM Lighting (Điều Chỉnh Độ Sáng):**
- 3 kênh: **White (Trắng)**, **Yellow (Vàng)**, **Red (Đỏ)**
- Mỗi kênh từ 0-100%
- Có thể cấu hình tối đa 10 điểm thời gian trong ngày

**Giải Thích PWM:**
```
06:00 (Sáng) ────────> 30% Yellow, 10% Red
12:00 (Trưa) ────────> 100% White, 60% Yellow, 30% Red  
18:00 (Tối) ────────> 0% tất cả (Đèn Tắt)
```

---

#### 🔵 **TAB 3: Relay & I/O**

```
┌─────────────────────────────────────────────────────┐
│ ⚡ Điều Khiển Relay                                  │
│                                                     │
│ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │
│ │   🟢    │ │   ⚫    │ │   🟢    │ │   ⚫    │ │
│ │  Đèn    │ │ Phun Ẩm │ │ Hút Ẩm │ │  CO2   │ │
│ │   ON    │ │  OFF    │ │   ON    │ │  OFF   │ │
│ └──────────┘ └──────────┘ └──────────┘ └──────────┘ │
│                                                     │
│ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │
│ │   🟢    │ │   ⚫    │ │   🟢    │ │   ⚫    │ │
│ │  Bơm    │ │Quạt Thổi│ │Quạt Hút │ │  Tùy Ch.│ │
│ │   ON    │ │  OFF    │ │   ON    │ │  OFF   │ │
│ └──────────┘ └──────────┘ └──────────┘ └──────────┘ │
│                                                     │
│ ← Click để bật/tắt từng Relay                       │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ 🔌 Trạng Thái Đầu Vào Digital                        │
│                                                     │
│ Input 1: 🟢 HIGH     Input 2: ⚫ LOW                │
│ Input 3: 🟢 HIGH     Input 4: ⚫ LOW                │
└─────────────────────────────────────────────────────┘
```

**8 Relay được điều khiển:**
1. **CH1:** Đèn (Lighting)
2. **CH2:** Phun Ẩm (Humidifier)
3. **CH3:** Hút Ẩm (Dehumidifier)
4. **CH4:** CO2 (CO2 Pump)
5. **CH5:** Bơm Nước (Water Pump)
6. **CH6:** Quạt Thổi (Supply Fan)
7. **CH7:** Quạt Hút (Exhaust Fan)
8. **CH8:** Tùy Chọn (Option)

---

#### 🟠 **TAB 4: Thiết Bị (Equipment)**

```
┌──────────────────────────────────────────┐
│ ⚙️ Cấu Hình Thiết Bị Tuần Hiện Tại        │
│                                          │
│ 🌬️ Quạt Tuần Hoàn                         │
│    Mode: [24H]                           │
│    Nhiệt độ kích hoạt: 28°C              │
│                                          │
│ 🌀 Quạt Hút                               │
│    Mode: [24H]                           │
│    Độ ẩm kích hoạt: 75%                  │
│    VPD: 1.5 kPa                          │
│                                          │
│ ❄️ Máy Lạnh                               │
│    Mode: [OFF]                           │
│    Nhiệt độ mục tiêu: 26°C               │
│    Tốc độ quạt: AUTO                     │
│                                          │
│ 💧 Chế Độ Độ Ẩm                           │
│    Chế độ: [DEHUMIDIFY]                  │
│    Hút ẩm khi độ ẩm > 70%                │
└──────────────────────────────────────────┘
```

**Các Mode:**
- **24H:** Luôn bật
- **TIMER:** Bật theo lịch cụ thể
- **HUMIDIFY:** Phun ẩm khi độ ẩm thấp
- **DEHUMIDIFY:** Hút ẩm khi độ ẩm cao

---

#### 🟣 **TAB 5: Hệ Thống (System)**

```
┌──────────────────────────────────────────┐
│ 📱 Thông Tin Thiết Bị                     │
│                                          │
│ Gateway ID: 9C139EFB13A4                 │
│ Room ID: ROOM_GIALAI_001                 │
│ Phiên Bản JSON: v3                       │
│ Dung Lượng HDD: 512 KB / 960 KB (53%)    │
│                                          │
│ 🔄 Tự Động Hóa                            │
│ ✅ Hoạt Động                              │
│                                          │
│ Số quy tắc: 12 | Lịch tưới: 2            │
└──────────────────────────────────────────┘
```

---

## 🔄 Logic Hoạt Động

### **Vòng Đời Cây Trồng (16 Tuần)**

```
┌─────────────────────────────────────────────────────────────────────┐
│                                                                     │
│  SEEDING (1 tuần)  │  VEG (2 tuần)  │  FLOWER (8 tuần)  │ HARVEST │
│  Tuần 1            │ Tuần 2-3        │ Tuần 4-11          │ Tuần... │
│                    │                 │                   │         │
│  [=]               │ [========]      │ [================] │ [...] │
│                                                                     │
│  Giai đoạn ────────> Tuần ────────> Targets ────────> Relay Control
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

### **Quy Trình Hoạt Động Mỗi Tuần**

```
1. Cloud gửi JSON v3 xuống
   ↓
2. ESP32 nhận và phân tích
   ├─ Xác định giai đoạn (SEEDING/VEG/FLOWER/HARVEST)
   ├─ Xác định tuần hiện tại (currentWeek từ Cloud)
   └─ Tính toán tuần trong phase
   ↓
3. Lấy config từ tuần hiện tại
   ├─ Targets (Nhiệt độ, CO2, Độ ẩm)
   ├─ Lighting (PWM 3 kênh)
   ├─ Equipment (Fan, AC, Humidify/Dehumidify)
   └─ Irrigation (Tưới nước theo lịch)
   ↓
4. Điều khiển Relay
   ├─ Bật/tắt đèn theo lịch PWM
   ├─ Bật/tắt quạt theo nhiệt độ/độ ẩm
   ├─ Bật/tắt máy lạnh
   └─ Bật/tắt bơm tưới nước
   ↓
5. Hiển thị trên Dashboard
   └─ Cập nhật real-time mỗi 3 giây
```

---

### **Ví Dụ: Tuần VEG thứ 2**

**JSON dari Cloud:**
```json
{
  "currentWeek": 3,
  "totalWeeks": 16,
  "weeklyPlans": {
    "VEG": {
      "2": {
        "targets": {
          "tempTargetDay": 25,
          "humiHighDay": 70,
          "co2StartDay": 1200,
          "humidityMode": "DEHUMIDIFY"
        },
        "lighting": {
          "lightsOn": "06:00",
          "lightsOff": "18:00",
          "schedule": [
            {"time": "06:00", "brightness": 30, "channels": {"ch1": 30, "ch2": 10, "ch3": 0}},
            {"time": "12:00", "brightness": 100, "channels": {"ch1": 60, "ch2": 30, "ch3": 100}},
            {"time": "18:00", "brightness": 0, "channels": {"ch1": 0, "ch2": 0, "ch3": 0}}
          ]
        },
        "equipment": {
          "fanCirculation": {"mode": "24H", "triggerTemp": 28},
          "fanExhaust": {"mode": "24H", "triggerHumidity": 75},
          "ac": {"mode": "OFF", "targetTemp": 25}
        },
        "irrigation": [
          {"startTime": "08:00", "durationMinutes": 15},
          {"startTime": "16:00", "durationMinutes": 20}
        ]
      }
    }
  }
}
```

**ESP32 sẽ:**
1. ✅ Nhận currentWeek = 3
2. ✅ Xác định Phase = VEG, Week trong Phase = 2
3. ✅ Set Target: 25°C, 70% độ ẩm, 1200 ppm CO2
4. ✅ Set PWM Lighting: 30% (6h), 100% (12h), 0% (18h)
5. ✅ Set Fan: 24H ON
6. ✅ Set AC: OFF
7. ✅ Set Tưới: 8:00 (15 phút) + 16:00 (20 phút)

**Dashboard hiển thị:**
```
🌱 Giai Đoạn Hiện Tại
VEG (Rau Xanh)
[==============18.75%=========] ← 3/16 tuần
Tuần 2 / 2  |  Tuần Toàn Bộ 3 / 16
```

---

## 📊 Dữ Liệu Từ API

**Điểm cuối:** `/api/automation/full`

**Phản hồi:**
```json
{
  "loaded": true,
  "version": 3,
  "gatewayId": "9C139EFB13A4",
  "currentPhase": "VEG",
  "currentWeek": 3,
  "currentWeekInPhase": 2,
  "phaseWeekCount": 2,
  "totalWeeks": 16,
  "targets": {
    "tempTargetDay": 25,
    "humiHighDay": 70,
    "co2StartDay": 1200,
    "humidityMode": "DEHUMIDIFY"
  },
  "lighting": {
    "lightsOn": "06:00",
    "lightsOff": "18:00",
    "schedule": [
      {"time": "06:00", "brightness": 30, "channels": {"ch1": 30, "ch2": 10, "ch3": 0}},
      {"time": "12:00", "brightness": 100, "channels": {"ch1": 60, "ch2": 30, "ch3": 100}},
      {"time": "18:00", "brightness": 0, "channels": {"ch1": 0, "ch2": 0, "ch3": 0}}
    ]
  },
  "equipment": {
    "fanCirculation": {"mode": "24H", "triggerTemp": 28},
    "fanExhaust": {"mode": "24H", "triggerHumidity": 75},
    "ac": {"mode": "OFF", "targetTemp": 25, "fanSpeed": "AUTO"}
  },
  "irrigation": [
    {"startTime": "08:00", "durationMinutes": 15, "flowRate": 2.5},
    {"startTime": "16:00", "durationMinutes": 20, "flowRate": 2.5}
  ],
  "relayStates": [1, 0, 1, 0, 1, 1, 0, 0],
  "sensorReadings": {
    "temperature": 25.6,
    "humidity": 68,
    "co2": 1150,
    "vpd": 0.95
  },
  "automationRunning": true
}
```

---

## 🎛️ Cách Điều Khiển

### **1. Bật/Tắt Relay**
- Click vào nút Relay trong tab "⚡ Relay & I/O"
- Sẽ gửi request POST đến `/api/relay/{index}/toggle`

### **2. Xem Mực Độ Ẩm**
- Nhìn vào "Cảm Biến Hiện Tại" ở tab "📊 Tổng Quan"
- So sánh với "Mục Tiêu Môi Trường"

### **3. Điều Chỉnh PWM Màu Sáng**
- Gửi JSON lịch mới từ Cloud
- ESP32 sẽ tự động cập nhật PWM
- Dashboard sẽ hiển thị lịch mới

---

## 📈 Cập Nhật Dữ Liệu

Dashboard **tự động cập nhật mỗi 3 giây** từ API.

```javascript
// Trong dashboard_html_v3.h
setInterval(loadDashboard, 3000); // 3000ms = 3 giây
```

---

## 🔧 Troubleshooting

| Vấn đề | Nguyên Nhân | Giải Pháp |
|--------|-----------|----------|
| Dashboard không load | WiFi không kết nối | Kiểm tra IP ESP32 |
| Relay không bật/tắt | MQTT chưa kết nối | Kiểm tra MQTT server |
| PWM không đúng | JSON cấu trúc sai | Kiểm tra file v3 json |
| Dữ liệu cảm biến = 0 | Sensor không kết nối | Kiểm tra RS485 |

---

## 📝 Ghi Chú

- ✅ Dashboard sử dụng **CSS Grid** - responsive trên desktop + mobile
- ✅ Cập nhật **real-time** mỗi 3 giây
- ✅ Hiển thị **8 Relay** tương ứng với 8 kênh GPIO
- ✅ PWM **3 kênh** (White, Yellow, Red) cho điều khiển độ sáng
- ✅ Tất cả **Tiếng Việt** - dễ hiểu

---

## 📞 Support

Nếu có vấn đề, kiểm tra:
1. Serial monitor (115200 baud)
2. `/api/automation/full` response
3. MQTT messages
4. ESP32 logs

---

**Created:** February 2026  
**Version:** 3.0 - JSON v3 Structure  
**Language:** Tiếng Việt (Vietnamese)
