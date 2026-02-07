# JSON Structure Version 8 - Cloud Automation

## 📋 Overview

Version 8 của automation JSON từ Cloud có những thay đổi lớn về structure và features.

## 🔄 Major Changes from Previous Versions

### 1. **Weekly Plans - Nested Structure**

**Old Structure (v1-7):**
```json
"weeklyPlans": [
  {"week": 1, "phase": "VEG", "targets": {...}},
  {"week": 2, "phase": "VEG", "targets": {...}}
]
```

**New Structure (v8):**
```json
"weeklyPlans": {
  "VEG": {
    "1": {"targets": {...}, "lighting": {...}, "equipment": {...}},
    "2": {"targets": {...}, "lighting": {...}, "equipment": {...}}
  },
  "FLOWER": {
    "1": {...},
    "2": {...}
  }
}
```

### 2. **Lighting Schedule with PWM Dimming**

```json
"lighting": {
  "lightsOn": "06:00",
  "lightsOff": "18:00",
  "schedule": [
    {
      "time": "06:00",
      "brightness": 100,
      "channels": {
        "ch1": 30,  // White channel (0-100%)
        "ch2": 0,   // Yellow channel
        "ch3": 0    // Red channel
      }
    },
    {
      "time": "06:15",
      "brightness": 100,
      "channels": {"ch1": 40, "ch2": 0, "ch3": 0}
    }
  ]
}
```

**Features:**
- ✅ Timeline-based brightness control
- ✅ 3-channel RGB/White LED dimming
- ✅ Sunrise/sunset simulation

### 3. **Humidity Control Mode**

```json
"targets": {
  "humidityMode": "DEHUMIDIFY"  // or "HUMIDIFY"
}
```

- **DEHUMIDIFY**: Activate relay2 (dehumidifier) when humidity > humiHighDay
- **HUMIDIFY**: Activate relay3 (humidifier) when humidity < humiLowDay

### 4. **Equipment Modes**

```json
"equipment": {
  "fanCirculation": {
    "mode": "TIMER"  // or "24H"
  },
  "fanExhaust": {
    "mode": "24H"
  },
  "ac": {
    "mode": "COOL",
    "targetTemp": 26,
    "fanSpeed": "AUTO"
  }
}
```

**Modes:**
- `TIMER`: Bật/tắt theo conditions (nhiệt độ, độ ẩm)
- `24H`: Chạy liên tục 24/7
- `OFF`: Tắt hoàn toàn

### 5. **Relay Configuration**

```json
"relays": [
  {
    "relayId": "relay1",
    "name": "Main Light",
    "type": "LIGHT",
    "gpio": 16,  // ⚠️ IGNORED - Sử dụng GPIO từ config.h
    "inverted": false,
    "defaultState": false,
    "autoMode": true
  }
]
```

**⚠️ GPIO Mapping:**
- JSON chứa GPIO pins: 16,17,18,19,21,22,23,25
- **NHỮ chúng ta KHÔNG dùng GPIO từ JSON**
- **Sử dụng GPIO từ sơ đồ PCB ESP32-S3-1U-N4:** 20,19,21,42,41,40,39,38

**Relay Type Mapping:**
| relayId | Type | Name | GPIO PCB | Hardware |
|---------|------|------|----------|----------|
| relay1 | LIGHT | Main Light | GPIO 20 | Đèn chính |
| relay2 | FAN_CIRC | Circulation Fan | GPIO 19 | Phun ẩm |
| relay3 | FAN_EXH | Exhaust Fan | GPIO 21 | Hút ẩm |
| relay4 | PUMP1 | Pump 1 | GPIO 42 | CO2 |
| relay5 | PUMP2 | Pump 2 | GPIO 41 | Pump |
| relay6 | PUMP3 | Pump 3 | GPIO 40 | Quạt thổi |
| relay7 | CO2 | CO2 Valve | GPIO 39 | Quạt Hút |
| relay8 | AC | AC Trigger | GPIO 38 | Option |

### 6. **Rules with Conditions**

```json
"rules": [
  {
    "id": "auto_co2_inject",
    "name": "CO2 Injection",
    "enabled": true,
    "priority": 10,
    "triggers": [
      {
        "type": "SENSOR_THRESHOLD",
        "sensor": "co2",
        "operator": "<",
        "value": 800,
        "hysteresis": 50
      }
    ],
    "actions": [
      {
        "type": "RELAY_ON",
        "relayId": "relay7"
      }
    ],
    "conditions": [
      {
        "sensor": "co2",
        "operator": "<",
        "value": 1000
      }
    ]
  }
]
```

**Rule Types:**
- `TIME`: Trigger tại thời gian cụ thể
- `SENSOR_THRESHOLD`: Trigger khi sensor vượt ngưỡng

### 7. **Irrigation Cycles**

```json
"irrigation": [
  {
    "id": "80ab6bef-8901-47bd-a028-f2cda78006e5",
    "name": "Irrigation Cycle",
    "enabled": true,
    "cycleStart": "06:00",
    "cycleEnd": "18:00",
    "pumpDurationSec": 300,      // Bơm 5 phút
    "restDurationSec": 3600,     // Nghỉ 1 giờ
    "activeDays": ["MO","TU","WE","TH","FR","SA","SU"],
    "pumpRelays": ["relay4"]
  }
]
```

**Logic:**
- Từ 06:00 đến 18:00 mỗi ngày
- Bơm 5 phút → Nghỉ 1 giờ → Lặp lại
- Chỉ hoạt động vào các ngày được chọn

## 📊 Phase Lifecycle

```
SEEDING (1 week)
  ↓
VEG (2 weeks)
  ↓
FLOWER (8 weeks)
  ↓
HARVEST (2 weeks)
```

**currentWeek**: 1-16 (global week counter)
**totalWeeks**: 16

## 🎨 Implementation Plan

### Phase 1: Data Structures ✅
- [ ] Update AutomationSync structures
- [ ] Add PWM dimming support
- [ ] Add humidity mode field
- [ ] Add equipment mode parsing

### Phase 2: Parsing Logic ✅
- [ ] Parse nested weeklyPlans structure
- [ ] Parse lighting schedule array
- [ ] Parse relay configurations
- [ ] Parse rules with conditions

### Phase 3: Execution Logic ✅
- [ ] Implement PWM control for 3 channels
- [ ] Implement lighting schedule timeline
- [ ] Implement equipment mode logic
- [ ] Implement humidity mode logic

### Phase 4: Dashboard Updates ✅
- [ ] Display current phase & week
- [ ] Display lighting schedule timeline
- [ ] Display active irrigation cycles
- [ ] Display equipment status

## 🔧 Technical Notes

### PWM Channels Setup

```cpp
// LED PWM setup
#define PWM_FREQ 5000      // 5 KHz
#define PWM_RESOLUTION 8   // 8-bit (0-255)
#define PWM_CH1_PIN 20     // White LED
#define PWM_CH2_PIN 34     // Yellow LED
#define PWM_CH3_PIN 35     // Red LED
```

### Lighting Schedule Interpolation

```cpp
// Tìm 2 schedule points gần nhất
// Interpolate brightness giữa chúng
// Cập nhật PWM duty cycle mỗi phút
```

### Equipment Mode Logic

```cpp
if (mode == "24H") {
  // Always ON
  setRelay(relay, true);
} else if (mode == "TIMER") {
  // Control based on sensors
  if (temp > threshold) setRelay(relay, true);
}
```

