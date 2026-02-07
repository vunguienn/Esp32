/**
 * Dashboard HTML v3 - Tiếng Việt
 * Hiển thị thông tin Phase, Targets, PWM Lighting, Equipment, Irrigation
 */

#ifndef DASHBOARD_HTML_V3_H
#define DASHBOARD_HTML_V3_H

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Hệ Thống Trồng Cây Tự Động</title>
<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: 'Segoe UI', 'Arial Unicode MS', sans-serif;
  background: linear-gradient(135deg, #0f172a 0%, #1a1f35 100%);
  color: #e2e8f0;
  min-height: 100vh;
  padding: 10px;
}

.container {
  max-width: 1400px;
  margin: 0 auto;
}

/* ===== HEADER ===== */
.header {
  background: linear-gradient(135deg, #1e293b, #334155);
  padding: 15px 20px;
  border-radius: 10px 10px 0 0;
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 0;
  border-bottom: 2px solid #4ade80;
}

.header h1 {
  font-size: 18px;
  color: #4ade80;
  display: flex;
  align-items: center;
  gap: 8px;
}

.header-info {
  display: flex;
  gap: 15px;
  font-size: 12px;
  align-items: center;
}

.status-dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  display: inline-block;
  margin-right: 5px;
}

.dot-ok {
  background: #4ade80;
  box-shadow: 0 0 8px #4ade80;
}

.dot-err {
  background: #ef4444;
  box-shadow: 0 0 8px #ef4444;
}

/* ===== TABS ===== */
.tabs {
  display: flex;
  background: #1e293b;
  border-bottom: 2px solid #334155;
  gap: 0;
  overflow-x: auto;
  margin: 0;
}

.tab-btn {
  padding: 12px 18px;
  cursor: pointer;
  border: none;
  background: transparent;
  color: #94a3b8;
  font-size: 13px;
  font-weight: 600;
  transition: all 0.3s;
  border-bottom: 3px solid transparent;
  white-space: nowrap;
}

.tab-btn:hover {
  color: #e2e8f0;
  background: rgba(255, 255, 255, 0.05);
}

.tab-btn.active {
  color: #4ade80;
  background: rgba(74, 222, 128, 0.1);
  border-bottom-color: #4ade80;
}

.tab-content {
  display: none;
  padding: 20px;
  background: #0f172a;
  border-radius: 0 0 10px 10px;
}

.tab-content.active {
  display: block;
}

/* ===== CARDS ===== */
.card {
  background: #1e293b;
  border: 1px solid #334155;
  border-radius: 8px;
  padding: 16px;
  margin-bottom: 15px;
  transition: all 0.3s;
}

.card:hover {
  border-color: #4ade80;
  box-shadow: 0 0 10px rgba(74, 222, 128, 0.2);
}

.card-title {
  font-size: 11px;
  text-transform: uppercase;
  color: #64748b;
  margin-bottom: 12px;
  letter-spacing: 1px;
  font-weight: 700;
}

.card-title-large {
  font-size: 14px;
  color: #4ade80;
  display: flex;
  align-items: center;
  gap: 8px;
}

/* ===== GRID ===== */
.grid-2 {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 15px;
}

.grid-3 {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 15px;
}

.grid-full {
  grid-column: 1 / -1;
}

/* ===== STAT BOX ===== */
.stat-item {
  background: #0f172a;
  padding: 12px;
  border-radius: 6px;
  border-left: 3px solid #4ade80;
  margin-bottom: 10px;
}

.stat-label {
  color: #64748b;
  font-size: 11px;
  text-transform: uppercase;
}

.stat-value {
  font-size: 18px;
  font-weight: 700;
  color: #4ade80;
  margin-top: 4px;
  font-family: 'Courier New', monospace;
}

.stat-unit {
  font-size: 12px;
  color: #94a3b8;
  margin-left: 4px;
}

/* ===== PHASE INFO ===== */
.phase-box {
  background: linear-gradient(135deg, #1e3a5f, #1e293b);
  border: 2px solid #3b82f6;
  border-radius: 8px;
  padding: 16px;
  margin-bottom: 15px;
}

.phase-label {
  font-size: 10px;
  color: #64748b;
  text-transform: uppercase;
  margin-bottom: 4px;
}

.phase-name {
  font-size: 20px;
  font-weight: 700;
  color: #3b82f6;
  margin-bottom: 8px;
}

.phase-progress {
  display: flex;
  gap: 8px;
  align-items: center;
  margin-bottom: 8px;
}

.progress-bar {
  flex: 1;
  height: 8px;
  background: #0f172a;
  border-radius: 4px;
  overflow: hidden;
}

.progress-fill {
  height: 100%;
  background: linear-gradient(90deg, #3b82f6, #4ade80);
  transition: width 0.3s;
}

.phase-week {
  font-size: 11px;
  color: #cbd5e1;
}

/* ===== TARGETS TABLE ===== */
.targets-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px;
}

.target-item {
  background: #0f172a;
  padding: 10px;
  border-radius: 6px;
  border-left: 3px solid #10b981;
}

.target-label {
  font-size: 10px;
  color: #64748b;
  text-transform: uppercase;
}

.target-value {
  font-size: 16px;
  font-weight: 700;
  color: #10b981;
  margin-top: 4px;
}

/* ===== LIGHTING SCHEDULE ===== */
.schedule-table {
  width: 100%;
  background: #0f172a;
  border-collapse: collapse;
  border-radius: 6px;
  overflow: hidden;
  margin-top: 10px;
}

.schedule-table thead {
  background: #1e293b;
}

.schedule-table th {
  padding: 8px;
  text-align: left;
  font-size: 10px;
  color: #64748b;
  text-transform: uppercase;
  border-bottom: 2px solid #334155;
  font-weight: 700;
}

.schedule-table td {
  padding: 8px;
  border-bottom: 1px solid #334155;
  font-size: 11px;
  color: #cbd5e1;
}

.schedule-table tr:hover {
  background: rgba(255, 255, 255, 0.02);
}

.time-cell {
  font-weight: 600;
  color: #4ade80;
}

.pwm-cell {
  font-family: 'Courier New', monospace;
  display: flex;
  gap: 4px;
}

.pwm-ch {
  padding: 2px 6px;
  border-radius: 3px;
  font-size: 9px;
  font-weight: 700;
}

.pwm-w { background: rgba(209, 213, 219, 0.3); color: #e5e7eb; }
.pwm-y { background: rgba(234, 179, 8, 0.3); color: #facc15; }
.pwm-r { background: rgba(239, 68, 68, 0.3); color: #fca5a5; }

/* ===== RELAY GRID ===== */
.relay-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 10px;
}

.relay-btn {
  padding: 12px 8px;
  border-radius: 6px;
  border: 2px solid #334155;
  background: transparent;
  color: #94a3b8;
  cursor: pointer;
  transition: all 0.3s;
  text-align: center;
  font-size: 12px;
  font-weight: 600;
}

.relay-btn:hover {
  transform: translateY(-2px);
}

.relay-btn.on {
  background: #166534;
  border-color: #22c55e;
  color: #4ade80;
  box-shadow: 0 0 10px rgba(74, 222, 128, 0.3);
}

.relay-btn.off {
  background: #1e293b;
  border-color: #475569;
  color: #64748b;
}

.relay-name {
  display: block;
  font-size: 10px;
  margin-top: 4px;
  text-transform: uppercase;
}

/* ===== EQUIPMENT CONFIG ===== */
.equip-box {
  background: #0f172a;
  border-left: 3px solid #f59e0b;
  padding: 12px;
  border-radius: 4px;
  margin-bottom: 10px;
}

.equip-name {
  font-weight: 700;
  color: #f59e0b;
  font-size: 12px;
  margin-bottom: 4px;
  text-transform: uppercase;
}

.equip-mode {
  font-size: 11px;
  padding: 4px 8px;
  background: rgba(245, 158, 11, 0.2);
  border-radius: 3px;
  display: inline-block;
  color: #fbbf24;
}

/* ===== FOOTER ===== */
.footer {
  margin-top: 20px;
  padding: 15px;
  text-align: center;
  color: #64748b;
  font-size: 10px;
  border-top: 1px solid #334155;
  border-radius: 0 0 10px 10px;
  background: #1e293b;
}

.footer a {
  color: #4ade80;
  text-decoration: none;
}

.footer a:hover {
  text-decoration: underline;
}

/* ===== RESPONSIVE ===== */
@media (max-width: 768px) {
  .grid-2, .grid-3 {
    grid-template-columns: 1fr;
  }
  
  .relay-grid {
    grid-template-columns: repeat(2, 1fr);
  }
  
  .header {
    flex-direction: column;
    gap: 10px;
    align-items: flex-start;
  }
  
  .header-info {
    flex-wrap: wrap;
  }
}
</style>
</head>
<body>
<div class="container">
  <!-- HEADER -->
  <div class="header">
    <h1>🌱 Hệ Thống Trồng Cây Tự Động</h1>
    <div class="header-info">
      <div>
        <span class="status-dot dot-ok" id="wifi-status"></span>
        WiFi: <span id="wifi-ip">-</span>
      </div>
      <div>
        <span class="status-dot dot-err" id="mqtt-status"></span>
        MQTT: <span id="mqtt-state">Chưa kết nối</span>
      </div>
      <div>🕐 <span id="current-time">--:--</span></div>
    </div>
  </div>

  <!-- TABS -->
  <div class="tabs">
    <button class="tab-btn active" onclick="switchTab('overview')">📊 Tổng Quan</button>
    <button class="tab-btn" onclick="switchTab('schedule')">📅 Lịch Hoạt Động</button>
    <button class="tab-btn" onclick="switchTab('relays')">⚡ Relay & I/O</button>
    <button class="tab-btn" onclick="switchTab('equipment')">⚙️ Thiết Bị</button>
    <button class="tab-btn" onclick="switchTab('system')">🔧 Hệ Thống</button>
    <button class="tab-btn" onclick="switchTab('settings')">🛠️ Cài Đặt</button>
  </div>

  <!-- TAB: OVERVIEW -->
  <div id="overview" class="tab-content active">
    <!-- PHASE INFO -->
    <div class="phase-box">
      <div class="phase-label">🌱 Giai Đoạn Hiện Tại</div>
      <div class="phase-name" id="phase-name">VEG</div>
      <div class="phase-progress">
        <div class="progress-bar">
          <div class="progress-fill" id="phase-progress" style="width: 50%;"></div>
        </div>
        <div class="phase-week">
          <span id="phase-week">Tuần 1 / 2</span>
          <span style="color: #64748b;"> | Tuần Toàn Bộ <span id="global-week">1 / 16</span></span>
        </div>
      </div>
      <div class="phase-week" style="margin-top: 6px;">
        Ngày dự án: <span id="project-day">-</span>
        <span style="color: #64748b;"> | Ngày trong tuần: <span id="day-in-week">-</span></span>
      </div>
    </div>

    <!-- TARGETS -->
    <div class="card grid-full">
      <div class="card-title">📊 Mục Tiêu Môi Trường</div>
      <div class="targets-grid">
        <div class="target-item">
          <div class="target-label">Nhiệt độ Ngày</div>
          <div class="target-value"><span id="temp-day">26</span>°C</div>
        </div>
        <div class="target-item">
          <div class="target-label">Nhiệt độ Đêm</div>
          <div class="target-value"><span id="temp-night">20</span>°C</div>
        </div>
        <div class="target-item">
          <div class="target-label">Độ Ẩm Ngày</div>
          <div class="target-value"><span id="humi-day">70</span>%</div>
        </div>
        <div class="target-item">
          <div class="target-label">Độ Ẩm Đêm</div>
          <div class="target-value"><span id="humi-night">65</span>%</div>
        </div>
        <div class="target-item">
          <div class="target-label">CO2 Ngày</div>
          <div class="target-value"><span id="co2-day">1200</span> ppm</div>
        </div>
        <div class="target-item">
          <div class="target-label">CO2 Đêm</div>
          <div class="target-value"><span id="co2-night">500</span> ppm</div>
        </div>
        <div class="target-item">
          <div class="target-label">VPD Min</div>
          <div class="target-value"><span id="vpd-min">0.8</span></div>
        </div>
        <div class="target-item">
          <div class="target-label">VPD Max</div>
          <div class="target-value"><span id="vpd-max">1.2</span></div>
        </div>
      </div>
    </div>

    <!-- SENSOR READINGS -->
    <div class="card">
      <div class="card-title">🌡️ Cảm Biến Hiện Tại</div>
      <div class="grid-2">
        <div class="stat-item">
          <div class="stat-label">Nhiệt Độ</div>
          <div class="stat-value"><span id="sensor-temp">--</span><span class="stat-unit">°C</span></div>
        </div>
        <div class="stat-item">
          <div class="stat-label">Độ Ẩm</div>
          <div class="stat-value"><span id="sensor-humi">--</span><span class="stat-unit">%</span></div>
        </div>
        <div class="stat-item">
          <div class="stat-label">CO2</div>
          <div class="stat-value"><span id="sensor-co2">--</span><span class="stat-unit">ppm</span></div>
        </div>
        <div class="stat-item">
          <div class="stat-label">VPD</div>
          <div class="stat-value"><span id="sensor-vpd">--</span><span class="stat-unit">kPa</span></div>
        </div>
      </div>
    </div>
  </div>

  <!-- TAB: SCHEDULE -->
  <div id="schedule" class="tab-content">
    <!-- LIGHTING SCHEDULE -->
    <div class="card grid-full">
      <div class="card-title-large">💡 Lịch Sáng & Điều Chỉnh Độ Sáng (PWM)</div>
      <div style="margin-top: 12px;">
        <div class="stat-item">
          <div class="stat-label">Giờ Bật / Tắt</div>
          <div class="stat-value"><span id="light-on">06:00</span> - <span id="light-off">18:00</span></div>
        </div>
      </div>
      <table class="schedule-table">
        <thead>
          <tr>
            <th>Thời Gian</th>
            <th>Độ Sáng %</th>
            <th>⚪ White</th>
            <th>🟡 Yellow</th>
            <th>🔴 Red</th>
          </tr>
        </thead>
        <tbody id="lighting-schedule">
          <tr>
            <td class="time-cell">06:00</td>
            <td>30%</td>
            <td><span class="pwm-ch pwm-w">0%</span></td>
            <td><span class="pwm-ch pwm-y">30%</span></td>
            <td><span class="pwm-ch pwm-r">10%</span></td>
          </tr>
          <tr>
            <td class="time-cell">12:00</td>
            <td>100%</td>
            <td><span class="pwm-ch pwm-w">100%</span></td>
            <td><span class="pwm-ch pwm-y">60%</span></td>
            <td><span class="pwm-ch pwm-r">30%</span></td>
          </tr>
          <tr>
            <td class="time-cell">18:00</td>
            <td>0%</td>
            <td><span class="pwm-ch pwm-w">0%</span></td>
            <td><span class="pwm-ch pwm-y">0%</span></td>
            <td><span class="pwm-ch pwm-r">0%</span></td>
          </tr>
        </tbody>
      </table>
    </div>

    <!-- IRRIGATION SCHEDULE -->
    <div class="card grid-full">
      <div class="card-title-large">💧 Lịch Tưới Nước</div>
      <table class="schedule-table">
        <thead>
          <tr>
            <th>Thời Gian Bắt Đầu</th>
            <th>Thời Lượng (phút)</th>
            <th>Lưu Lượng</th>
            <th>Trạng Thái</th>
          </tr>
        </thead>
        <tbody id="irrigation-schedule">
          <tr>
            <td class="time-cell">08:00</td>
            <td>15 phút</td>
            <td>2.5 L/phút</td>
            <td><span class="equip-mode">Chủ Động</span></td>
          </tr>
          <tr>
            <td class="time-cell">16:00</td>
            <td>20 phút</td>
            <td>2.5 L/phút</td>
            <td><span class="equip-mode">Chủ Động</span></td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>

  <!-- TAB: RELAYS -->
  <div id="relays" class="tab-content">
    <div class="card grid-full">
      <div class="card-title-large">⚡ Điều Khiển Relay</div>
      <div class="relay-grid" id="relay-grid">
        <!-- JavaScript sẽ điền vào đây -->
      </div>
    </div>

    <div class="card">
      <div class="card-title">🔌 Trạng Thái Đầu Vào Digital</div>
      <div id="input-status" style="display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px;">
        <!-- JavaScript sẽ điền vào đây -->
      </div>
    </div>
  </div>

  <!-- TAB: EQUIPMENT -->
  <div id="equipment" class="tab-content">
    <div class="card">
      <div class="card-title-large">⚙️ Cấu Hình Thiết Bị Tuần Hiện Tại</div>
      
      <div style="margin-top: 15px;">
        <div class="equip-box">
          <div class="equip-name">🌬️ Quạt Tuần Hoàn</div>
          <div>Mode: <span class="equip-mode" id="fan-circ-mode">24H</span></div>
          <div style="font-size: 11px; color: #cbd5e1; margin-top: 6px;">
            Nhiệt độ kích hoạt: <span id="fan-circ-temp">28°C</span>
          </div>
        </div>

        <div class="equip-box">
          <div class="equip-name">🌀 Quạt Hút</div>
          <div>Mode: <span class="equip-mode" id="fan-exh-mode">24H</span></div>
          <div style="font-size: 11px; color: #cbd5e1; margin-top: 6px;">
            Độ ẩm kích hoạt: <span id="fan-exh-humi">75%</span> | VPD: <span id="fan-exh-vpd">1.5 kPa</span>
          </div>
        </div>

        <div class="equip-box">
          <div class="equip-name">❄️ Máy Lạnh</div>
          <div>Mode: <span class="equip-mode" id="ac-mode">OFF</span></div>
          <div style="font-size: 11px; color: #cbd5e1; margin-top: 6px;">
            Nhiệt độ mục tiêu: <span id="ac-temp">26°C</span> | Tốc độ quạt: <span id="ac-fan">AUTO</span>
          </div>
        </div>

        <div class="equip-box">
          <div class="equip-name">💧 Chế Độ Độ Ẩm</div>
          <div>Chế độ: <span class="equip-mode" id="humidity-mode">DEHUMIDIFY</span></div>
          <div style="font-size: 11px; color: #cbd5e1; margin-top: 6px;">
            <span id="humidity-info">Hút ẩm khi độ ẩm > 70%</span>
          </div>
        </div>
      </div>
    </div>
  </div>

  <!-- TAB: SYSTEM -->
  <div id="system" class="tab-content">
    <div class="card">
      <div class="card-title">📱 Thông Tin Thiết Bị</div>
      <div style="display: grid; gap: 8px; font-size: 12px;">
        <div style="display: flex; justify-content: space-between; padding: 8px; background: #0f172a; border-radius: 4px;">
          <span>Gateway ID:</span>
          <span style="color: #4ade80; font-family: monospace;" id="gateway-id">-</span>
        </div>
        <div style="display: flex; justify-content: space-between; padding: 8px; background: #0f172a; border-radius: 4px;">
          <span>Room ID:</span>
          <span style="color: #4ade80; font-family: monospace;" id="room-id">-</span>
        </div>
        <div style="display: flex; justify-content: space-between; padding: 8px; background: #0f172a; border-radius: 4px;">
          <span>Phiên Bản JSON:</span>
          <span style="color: #4ade80; font-family: monospace;" id="version">v3</span>
        </div>
        <div style="display: flex; justify-content: space-between; padding: 8px; background: #0f172a; border-radius: 4px;">
          <span>Dung Lượng HDD:</span>
          <span style="color: #4ade80; font-family: monospace;" id="storage">-</span>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="card-title">🔄 Tự Động Hóa</div>
      <div id="automation-info" style="font-size: 12px; line-height: 1.6;">
        <div style="padding: 10px; background: #0f172a; border-radius: 4px; margin-bottom: 8px;">
          <div style="color: #4ade80; font-weight: 700;">✅ Hoạt Động</div>
          <div style="color: #cbd5e1; margin-top: 4px;">
            Số quy tắc: <span id="rule-count">0</span> | 
            Lịch tưới: <span id="irrig-count">0</span>
          </div>
        </div>
      </div>
    </div>
  </div>

  <!-- TAB: SETTINGS -->
  <div id="settings" class="tab-content">
    <div class="card">
      <div class="card-title-large">🛠️ Cài Đặt Vòng Đời</div>
      <div style="font-size: 12px; color: #94a3b8; margin-top: 6px;">
        Thay đổi ngày dự án và giai đoạn. Tuần sẽ tự tính theo ngày.
      </div>

      <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 12px; margin-top: 12px;">
        <div class="stat-item">
          <div class="stat-label">Ngày Dự Án</div>
          <input id="setting-project-day" type="number" min="1" value="1" style="margin-top: 6px; width: 100%; padding: 8px; border-radius: 4px; border: 1px solid #334155; background: #0f172a; color: #e2e8f0;">
        </div>
        <div class="stat-item">
          <div class="stat-label">Giai Đoạn</div>
          <select id="setting-current-phase" style="margin-top: 6px; width: 100%; padding: 8px; border-radius: 4px; border: 1px solid #334155; background: #0f172a; color: #e2e8f0;">
            <option value="SEEDING">SEEDING</option>
            <option value="VEG">VEG</option>
            <option value="FLOWER">FLOWER</option>
            <option value="HARVEST">HARVEST</option>
          </select>
        </div>
      </div>

      <div style="margin-top: 12px; display: flex; gap: 10px; align-items: center;">
        <button class="relay-btn on" onclick="saveLifecycleSettings()" style="max-width: 160px;">Lưu Cài Đặt</button>
        <span id="settings-status" style="font-size: 12px; color: #94a3b8;"></span>
      </div>
    </div>

    <div class="card">
      <div class="card-title-large">🧪 Test Cảm Biến (Giả lập)</div>
      <div style="font-size: 12px; color: #94a3b8; margin-top: 6px;">
        Dùng để giả lập cảm biến nhằm kiểm tra cơ chế tự động hoá.
      </div>

      <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 12px; margin-top: 12px;">
        <div class="stat-item">
          <div class="stat-label">Nhiệt độ (°C)</div>
          <input id="mock-temp" type="number" step="0.1" value="25" style="margin-top: 6px; width: 100%; padding: 8px; border-radius: 4px; border: 1px solid #334155; background: #0f172a; color: #e2e8f0;">
        </div>
        <div class="stat-item">
          <div class="stat-label">Độ ẩm (%)</div>
          <input id="mock-humi" type="number" step="0.1" value="60" style="margin-top: 6px; width: 100%; padding: 8px; border-radius: 4px; border: 1px solid #334155; background: #0f172a; color: #e2e8f0;">
        </div>
        <div class="stat-item">
          <div class="stat-label">CO2 (ppm)</div>
          <input id="mock-co2" type="number" step="1" value="800" style="margin-top: 6px; width: 100%; padding: 8px; border-radius: 4px; border: 1px solid #334155; background: #0f172a; color: #e2e8f0;">
        </div>
        <div class="stat-item">
          <div class="stat-label">VPD (kPa)</div>
          <input id="mock-vpd" type="number" step="0.01" value="1.0" style="margin-top: 6px; width: 100%; padding: 8px; border-radius: 4px; border: 1px solid #334155; background: #0f172a; color: #e2e8f0;">
        </div>
      </div>

      <div style="margin-top: 12px; display: flex; gap: 12px; align-items: center;">
        <label style="font-size: 12px; color: #cbd5e1; display: flex; align-items: center; gap: 8px;">
          <input id="mock-enabled" type="checkbox" style="transform: scale(1.1);">
          Bật giả lập
        </label>
        <button class="relay-btn on" onclick="saveSensorMock()" style="max-width: 160px;">Lưu Test</button>
        <span id="mock-status" style="font-size: 12px; color: #94a3b8;"></span>
      </div>
    </div>
  </div>

  <!-- FOOTER -->
  <div class="footer">
    <div>🌱 Hệ Thống Trồng Cây Thông Minh | <span id="footer-time">--:--:--</span></div>
    <div style="margin-top: 8px; font-size: 9px; color: #475569;">
      Được cập nhật in real-time • <a href="javascript:location.reload()">Làm mới</a>
    </div>
  </div>
</div>

<script>
// Relay names
const RELAY_NAMES = ['Đèn', 'Phun Ẩm', 'Hút Ẩm', 'CO2', 'Bơm', 'Quạt Thổi', 'Quạt Hút', 'Tùy Chọn'];

function switchTab(tabName) {
  // Hide all tabs
  document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(el => el.classList.remove('active'));
  
  // Show selected tab
  document.getElementById(tabName).classList.add('active');
  event.target.classList.add('active');
}

async function loadDashboard() {
  try {
    const response = await fetch('/api/automation/full');
    const data = await response.json();
    
    updatePhaseInfo(data);
    updateTargets(data);
    updateLighting(data);
    updateEquipment(data);
    updateIrrigation(data);
    updateRelays(data);
    updateSystemInfo(data);
    updateLifecycleSettings(data);
    updateSensorMock(data);
  } catch (e) {
    console.error('Error loading dashboard:', e);
  }
}

function updatePhaseInfo(data) {
  if (!data.currentPhase) return;
  
  document.getElementById('phase-name').textContent = data.currentPhase;
  document.getElementById('phase-week').textContent = `Tuần ${data.currentWeekInPhase} / ${data.phaseWeekCount}`;
  document.getElementById('global-week').textContent = `${data.currentWeek} / ${data.totalWeeks}`;
  if (data.projectDay !== undefined) {
    document.getElementById('project-day').textContent = data.projectDay;
  }
  if (data.currentDayInWeek !== undefined) {
    document.getElementById('day-in-week').textContent = data.currentDayInWeek;
  }
  
  let progress = (data.currentWeek / data.totalWeeks) * 100;
  document.getElementById('phase-progress').style.width = progress + '%';
}

function updateLifecycleSettings(data) {
  if (data.projectDay !== undefined) {
    document.getElementById('setting-project-day').value = data.projectDay;
  }
  if (data.currentPhase) {
    document.getElementById('setting-current-phase').value = data.currentPhase;
  }
}

function updateSensorMock(data) {
  if (data.sensorReadings) {
    document.getElementById('mock-temp').value = data.sensorReadings.temperature ?? 0;
    document.getElementById('mock-humi').value = data.sensorReadings.humidity ?? 0;
    document.getElementById('mock-co2').value = data.sensorReadings.co2 ?? 0;
    document.getElementById('mock-vpd').value = data.sensorReadings.vpd ?? 0;
  }
  if (data.sensorMockEnabled !== undefined) {
    document.getElementById('mock-enabled').checked = data.sensorMockEnabled;
  }
}

function updateTargets(data) {
  if (!data.targets) return;
  
  document.getElementById('temp-day').textContent = data.targets.tempTargetDay.toFixed(1);
  document.getElementById('temp-night').textContent = data.targets.tempTargetNight.toFixed(1);
  document.getElementById('humi-day').textContent = data.targets.humiHighDay;
  document.getElementById('humi-night').textContent = data.targets.humiHighNight;
  document.getElementById('co2-day').textContent = data.targets.co2StartDay;
  document.getElementById('co2-night').textContent = data.targets.co2StartNight;
  document.getElementById('vpd-min').textContent = data.targets.vpdMin.toFixed(2);
  document.getElementById('vpd-max').textContent = data.targets.vpdMax.toFixed(2);
}

function updateLighting(data) {
  if (!data.lighting) return;
  
  document.getElementById('light-on').textContent = data.lighting.lightsOn;
  document.getElementById('light-off').textContent = data.lighting.lightsOff;
  
  // Update PWM schedule table
  let tbody = document.getElementById('lighting-schedule');
  if (data.lighting.schedule && data.lighting.schedule.length > 0) {
    tbody.innerHTML = data.lighting.schedule.map(point => `
      <tr>
        <td class="time-cell">${point.time}</td>
        <td>${point.brightness}%</td>
        <td><span class="pwm-ch pwm-w">${point.ch1}%</span></td>
        <td><span class="pwm-ch pwm-y">${point.ch2}%</span></td>
        <td><span class="pwm-ch pwm-r">${point.ch3}%</span></td>
      </tr>
    `).join('');
  }
}

function updateEquipment(data) {
  if (!data.equipment) return;
  
  document.getElementById('fan-circ-mode').textContent = data.equipment.fanCircMode;
  document.getElementById('fan-circ-temp').textContent = data.equipment.fanCircTriggerTemp.toFixed(1) + '°C';
  
  document.getElementById('fan-exh-mode').textContent = data.equipment.fanExhMode;
  document.getElementById('fan-exh-humi').textContent = data.equipment.fanExhTriggerHumi.toFixed(0) + '%';
  document.getElementById('fan-exh-vpd').textContent = data.equipment.fanExhTriggerVpd.toFixed(1) + ' kPa';
  
  document.getElementById('ac-mode').textContent = data.equipment.acMode;
  document.getElementById('ac-temp').textContent = data.equipment.acTargetTemp.toFixed(1) + '°C';
  document.getElementById('ac-fan').textContent = data.equipment.acFanSpeed;
  
  if (data.targets) {
    document.getElementById('humidity-mode').textContent = data.targets.humidityMode;
    const info = data.targets.humidityMode === 'HUMIDIFY'
      ? `Phun ẩm khi độ ẩm < ${data.targets.humiLowDay}%`
      : `Hút ẩm khi độ ẩm > ${data.targets.humiHighDay}%`;
    document.getElementById('humidity-info').textContent = info;
  }
}

function updateIrrigation(data) {
  if (!data.irrigation) return;
  
  let tbody = document.getElementById('irrigation-schedule');
  if (data.irrigation.length > 0) {
    tbody.innerHTML = data.irrigation.map(irr => `
      <tr>
        <td class="time-cell">${irr.startTime}</td>
        <td>${irr.durationMinutes} phút</td>
        <td>${irr.flowRate.toFixed(1)} L/phút</td>
        <td><span class="equip-mode">Chủ Động</span></td>
      </tr>
    `).join('');
  }
}

function updateRelays(data) {
  let grid = document.getElementById('relay-grid');
  grid.innerHTML = RELAY_NAMES.map((name, idx) => {
    const state = data.relayStates && data.relayStates[idx] ? 'on' : 'off';
    return `
      <button class="relay-btn ${state}" onclick="toggleRelay(${idx})">
        <span style="font-size: 14px;">${state === 'on' ? '🟢' : '⚫'}</span>
        <span class="relay-name">${name}</span>
      </button>
    `;
  }).join('');
}

function updateSystemInfo(data) {
  document.getElementById('gateway-id').textContent = data.gatewayId || '-';
  document.getElementById('room-id').textContent = data.roomId || '-';
  document.getElementById('rule-count').textContent = data.ruleCount || 0;
  document.getElementById('irrig-count').textContent = data.irrigationCount || 0;
}

async function toggleRelay(idx) {
  try {
    await fetch(`/api/relay/${idx}/toggle`, { method: 'POST' });
    loadDashboard();
  } catch (e) {
    console.error('Error toggling relay:', e);
  }
}

async function saveLifecycleSettings() {
  const projectDay = parseInt(document.getElementById('setting-project-day').value, 10);
  const currentPhase = document.getElementById('setting-current-phase').value;
  const statusEl = document.getElementById('settings-status');

  if (!projectDay || projectDay < 1) {
    statusEl.textContent = 'Ngày dự án không hợp lệ';
    return;
  }

  try {
    const res = await fetch('/api/lifecycle', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ projectDay, currentPhase })
    });
    const data = await res.json();
    if (data.ok) {
      statusEl.textContent = 'Đã lưu và đồng bộ';
      loadDashboard();
    } else {
      statusEl.textContent = data.message || 'Lưu thất bại';
    }
  } catch (e) {
    statusEl.textContent = 'Lỗi kết nối';
  }
}

async function saveSensorMock() {
  const enabled = document.getElementById('mock-enabled').checked;
  const statusEl = document.getElementById('mock-status');

  const payload = {
    enabled: enabled
  };

  if (enabled) {
    payload.temperature = parseFloat(document.getElementById('mock-temp').value || '0');
    payload.humidity = parseFloat(document.getElementById('mock-humi').value || '0');
    payload.co2 = parseInt(document.getElementById('mock-co2').value || '0', 10);
    payload.vpd = parseFloat(document.getElementById('mock-vpd').value || '0');
  }

  try {
    const res = await fetch('/api/sensors/mock', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    const data = await res.json();
    if (data.ok) {
      statusEl.textContent = enabled ? 'Đã bật giả lập' : 'Đã tắt giả lập';
      loadDashboard();
    } else {
      statusEl.textContent = data.message || 'Lưu thất bại';
    }
  } catch (e) {
    statusEl.textContent = 'Lỗi kết nối';
  }
}

function updateTime() {
  const now = new Date();
  const time = now.toLocaleTimeString('vi-VN');
  document.getElementById('current-time').textContent = time;
  document.getElementById('footer-time').textContent = time;
}

// Load data on page load
loadDashboard();
setInterval(loadDashboard, 3000); // Refresh every 3 seconds
setInterval(updateTime, 1000);
updateTime();
</script>
</body>
</html>
)rawliteral";

#endif // DASHBOARD_HTML_V3_H
