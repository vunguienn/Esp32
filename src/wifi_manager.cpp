/**
 * WiFi Manager Implementation
 */

#include "wifi_manager.h"
#include <ArduinoJson.h>

GrowWiFiManager::GrowWiFiManager() {
    _server = nullptr;
    _apModeActive = false;
    _apStartCallback = nullptr;
    _connectedCallback = nullptr;
}

void GrowWiFiManager::begin() {
    generateDeviceId();
    loadCredentials();
    
    Serial.println("[WiFi] Manager initialized");
    Serial.printf("[WiFi] Device ID: %s\n", _deviceId.c_str());
}

void GrowWiFiManager::generateDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char id[18];
    sprintf(id, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    _deviceId = String(id);
}

String GrowWiFiManager::getDeviceId() {
    return _deviceId;
}

String GrowWiFiManager::getAPSSID() {
    return String(AP_SSID_PREFIX) + _deviceId.substring(8);  // Last 4 bytes of MAC
}

void GrowWiFiManager::loadCredentials() {
    _prefs.begin(PREF_NAMESPACE, true);  // Read-only
    _savedSSID = _prefs.getString(KEY_WIFI_SSID, "");
    _savedPass = _prefs.getString(KEY_WIFI_PASS, "");
    _prefs.end();
    
    Serial.printf("[WiFi] Loaded SSID: %s\n", _savedSSID.c_str());
}

void GrowWiFiManager::saveCredentials() {
    _prefs.begin(PREF_NAMESPACE, false);
    _prefs.putString(KEY_WIFI_SSID, _savedSSID);
    _prefs.putString(KEY_WIFI_PASS, _savedPass);
    _prefs.end();
    
    Serial.println("[WiFi] Credentials saved");
}

void GrowWiFiManager::setCredentials(const char* ssid, const char* password) {
    _savedSSID = String(ssid);
    _savedPass = String(password);
    saveCredentials();
}

void GrowWiFiManager::clearCredentials() {
    _savedSSID = "";
    _savedPass = "";
    _prefs.begin(PREF_NAMESPACE, false);
    _prefs.remove(KEY_WIFI_SSID);
    _prefs.remove(KEY_WIFI_PASS);
    _prefs.end();
    Serial.println("[WiFi] Credentials cleared");
}

bool GrowWiFiManager::autoConnect() {
    // Always start AP mode first (để có thể config MQTT)
    Serial.println("[WiFi] 🛑 Starting AP mode (always on)...");
    startAPMode();
    
    // If we have saved credentials, try to connect
    if (_savedSSID.length() > 0) {
        Serial.printf("[WiFi] 🔄 Attempting to connect to: '%s'\n", _savedSSID.c_str());
        
        if (connectToWiFi(WIFI_CONNECT_TIMEOUT)) {
            Serial.printf("[WiFi] ✅ Connected! IP: %s (RSSI: %d dBm)\n", 
                         WiFi.localIP().toString().c_str(), WiFi.RSSI());
            if (_connectedCallback) _connectedCallback();
            return true;
        }
    } else {
        Serial.println("[WiFi] ⚠️  No saved credentials found");
    }
    
    return false;
}

bool GrowWiFiManager::connectToWiFi(int timeoutSeconds) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(_savedSSID.c_str(), _savedPass.c_str());
    
    int elapsed = 0;
    while (WiFi.status() != WL_CONNECTED && elapsed < timeoutSeconds) {
        delay(1000);
        elapsed++;
        Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] ✅ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] 📶 Signal: %d dBm | MAC: %s\n", 
                     WiFi.RSSI(), WiFi.macAddress().c_str());
        return true;
    }
    
    Serial.printf("[WiFi] ❌ Connection failed (status: %d)\n", WiFi.status());
    return false;
}

void GrowWiFiManager::startAPMode() {
    _apModeActive = true;
    
    // Set up AP
    WiFi.mode(WIFI_AP_STA);
    String apSSID = getAPSSID();
    WiFi.softAP(apSSID.c_str(), AP_PASSWORD);
    
    Serial.printf("[WiFi] 📶 AP Started\n");
    Serial.printf("[WiFi] 📡 SSID: '%s' | Password: '%s'\n", apSSID.c_str(), AP_PASSWORD);
    Serial.printf("[WiFi] 🌐 AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("[WiFi] 📍 Open browser: http://%s\n", WiFi.softAPIP().toString().c_str());
    
    // Start web server
    if (_server == nullptr) {
        _server = new WebServer(80);
    }
    
    _server->on("/", [this]() { handleRoot(); });
    _server->on("/save", HTTP_POST, [this]() { handleSave(); });
    _server->on("/scan", [this]() { handleScan(); });
    _server->on("/reset", [this]() { handleReset(); });
    _server->on("/status", [this]() { handleStatus(); });
    
    _server->begin();
    Serial.println("[WiFi] 🌐 Web server started on port 80");
    
    if (_apStartCallback) _apStartCallback();
}

void GrowWiFiManager::stopAPMode() {
    if (_server != nullptr) {
        _server->stop();
    }
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    _apModeActive = false;
    Serial.println("[WiFi] AP mode stopped");
}

void GrowWiFiManager::handleClient() {
    if (_apModeActive && _server != nullptr) {
        _server->handleClient();
    }
}

bool GrowWiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool GrowWiFiManager::isAPModeActive() {
    return _apModeActive;
}

String GrowWiFiManager::getIP() {
    return WiFi.localIP().toString();
}

int GrowWiFiManager::getRSSI() {
    return WiFi.RSSI();
}

String GrowWiFiManager::getSavedSSID() {
    return _savedSSID;
}

void GrowWiFiManager::setAPStartCallback(void (*callback)()) {
    _apStartCallback = callback;
}

void GrowWiFiManager::setConnectedCallback(void (*callback)()) {
    _connectedCallback = callback;
}

// ============================================================================
// Web Server Handlers
// ============================================================================

void GrowWiFiManager::handleRoot() {
    _server->send(200, "text/html", getConfigPage());
}

void GrowWiFiManager::handleSave() {
    if (_server->hasArg("ssid") && _server->hasArg("pass")) {
        _savedSSID = _server->arg("ssid");
        _savedPass = _server->arg("pass");
        saveCredentials();
        
        // Also save MQTT settings if provided
        if (_server->hasArg("mqtt_server")) {
            _prefs.begin(PREF_NAMESPACE, false);
            _prefs.putString(KEY_MQTT_SERVER, _server->arg("mqtt_server"));
            if (_server->hasArg("mqtt_port")) {
                _prefs.putInt(KEY_MQTT_PORT, _server->arg("mqtt_port").toInt());
            }
            if (_server->hasArg("mqtt_user")) {
                _prefs.putString(KEY_MQTT_USER, _server->arg("mqtt_user"));
            }
            if (_server->hasArg("mqtt_pass")) {
                _prefs.putString(KEY_MQTT_PASS, _server->arg("mqtt_pass"));
            }
            _prefs.end();
        }
        
        String response = R"({
            "success": true,
            "message": "Đã lưu cấu hình. Thiết bị sẽ khởi động lại..."
        })";
        _server->send(200, "application/json", response);
        
        delay(1000);
        ESP.restart();
    } else {
        _server->send(400, "application/json", "{\"success\":false,\"message\":\"Missing parameters\"}");
    }
}

void GrowWiFiManager::handleScan() {
    _server->send(200, "application/json", getScanResultsJSON());
}

void GrowWiFiManager::handleReset() {
    clearCredentials();
    _server->send(200, "application/json", "{\"success\":true,\"message\":\"Đã xóa cấu hình\"}");
    delay(1000);
    ESP.restart();
}

void GrowWiFiManager::handleStatus() {
    JsonDocument doc;
    doc["connected"] = isConnected();
    doc["ssid"] = _savedSSID;
    doc["ip"] = getIP();
    doc["rssi"] = getRSSI();
    doc["deviceId"] = _deviceId;
    doc["apMode"] = _apModeActive;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;
    
    String output;
    serializeJson(doc, output);
    _server->send(200, "application/json", output);
}

String GrowWiFiManager::getScanResultsJSON() {
    int n = WiFi.scanNetworks();
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    
    for (int i = 0; i < n; i++) {
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

String GrowWiFiManager::getConfigPage() {
    // Load saved MQTT settings
    _prefs.begin(PREF_NAMESPACE, true);
    String mqttServer = _prefs.getString(KEY_MQTT_SERVER, DEFAULT_MQTT_SERVER);
    int mqttPort = _prefs.getInt(KEY_MQTT_PORT, DEFAULT_MQTT_PORT);
    String mqttUser = _prefs.getString(KEY_MQTT_USER, "");
    _prefs.end();
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Grow Gateway - Cấu hình</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #f5f5f5; color: #333; min-height: 100vh; padding: 20px; }
        .container { max-width: 400px; margin: 0 auto; }
        .card { background: white; border-radius: 16px; padding: 24px; margin-bottom: 16px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }
        h1 { font-size: 24px; margin-bottom: 8px; color: #1a1a1a; }
        h2 { font-size: 16px; margin-bottom: 16px; color: #666; font-weight: 500; }
        .device-id { font-family: monospace; background: #f0f0f0; padding: 8px 12px; border-radius: 8px; font-size: 14px; margin-bottom: 20px; }
        label { display: block; font-size: 12px; font-weight: 600; color: #666; margin-bottom: 6px; text-transform: uppercase; letter-spacing: 0.5px; }
        input, select { width: 100%; padding: 12px 16px; border: 2px solid #e0e0e0; border-radius: 10px; font-size: 16px; margin-bottom: 16px; transition: border-color 0.2s; }
        input:focus, select:focus { outline: none; border-color: #10b981; }
        button { width: 100%; padding: 14px; border: none; border-radius: 10px; font-size: 16px; font-weight: 600; cursor: pointer; transition: transform 0.1s, opacity 0.2s; }
        button:active { transform: scale(0.98); }
        .btn-primary { background: #10b981; color: white; }
        .btn-primary:hover { background: #059669; }
        .btn-secondary { background: #f0f0f0; color: #333; margin-top: 8px; }
        .btn-danger { background: #ef4444; color: white; margin-top: 8px; }
        .networks { max-height: 200px; overflow-y: auto; margin-bottom: 16px; }
        .network-item { padding: 12px; border: 2px solid #e0e0e0; border-radius: 10px; margin-bottom: 8px; cursor: pointer; display: flex; justify-content: space-between; align-items: center; }
        .network-item:hover { border-color: #10b981; background: #f0fdf4; }
        .network-item.selected { border-color: #10b981; background: #d1fae5; }
        .signal { font-size: 12px; color: #666; }
        .status { padding: 12px; border-radius: 10px; margin-bottom: 16px; font-size: 14px; }
        .status.success { background: #d1fae5; color: #065f46; }
        .status.error { background: #fee2e2; color: #991b1b; }
        .status.info { background: #e0f2fe; color: #075985; }
        .section-title { font-size: 14px; font-weight: 600; color: #333; margin-bottom: 12px; padding-bottom: 8px; border-bottom: 1px solid #e0e0e0; }
        .loading { text-align: center; padding: 20px; color: #666; }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <h1>🌱 Grow Gateway</h1>
            <h2>Cấu hình thiết bị IoT</h2>
            <div class="device-id">Device ID: )rawliteral" + _deviceId + R"rawliteral(</div>
            <div id="status" class="status info">Đang quét mạng WiFi...</div>
        </div>

        <div class="card">
            <div class="section-title">📶 Kết nối WiFi</div>
            <div id="networks" class="networks">
                <div class="loading">Đang quét...</div>
            </div>
            <input type="text" id="ssid" placeholder="Tên mạng WiFi" value=")rawliteral" + _savedSSID + R"rawliteral(">
            <input type="password" id="pass" placeholder="Mật khẩu WiFi">
        </div>

        <div class="card">
            <div class="section-title">☁️ Cấu hình MQTT Cloud</div>
            <label>Địa chỉ Server</label>
            <input type="text" id="mqtt_server" placeholder="mqtt.example.com" value=")rawliteral" + mqttServer + R"rawliteral(">
            <label>Port</label>
            <input type="number" id="mqtt_port" placeholder="1883" value=")rawliteral" + String(mqttPort) + R"rawliteral(">
            <label>Username (tùy chọn)</label>
            <input type="text" id="mqtt_user" placeholder="mqtt_user" value=")rawliteral" + mqttUser + R"rawliteral(">
            <label>Password (tùy chọn)</label>
            <input type="password" id="mqtt_pass" placeholder="mqtt_password">
        </div>

        <div class="card">
            <button class="btn-primary" onclick="saveConfig()">💾 Lưu & Kết nối</button>
            <button class="btn-secondary" onclick="scanNetworks()">🔄 Quét lại WiFi</button>
            <button class="btn-danger" onclick="resetConfig()">🗑️ Xóa cấu hình</button>
        </div>
    </div>

    <script>
        let selectedSSID = '';
        
        function scanNetworks() {
            document.getElementById('networks').innerHTML = '<div class="loading">Đang quét...</div>';
            fetch('/scan')
                .then(r => r.json())
                .then(data => {
                    let html = '';
                    data.networks.forEach(n => {
                        const signal = n.rssi > -50 ? '📶' : n.rssi > -70 ? '📶' : '📶';
                        html += `<div class="network-item" onclick="selectNetwork('${n.ssid}')">
                            <span>${n.ssid} ${n.secure ? '🔒' : ''}</span>
                            <span class="signal">${signal} ${n.rssi}dBm</span>
                        </div>`;
                    });
                    document.getElementById('networks').innerHTML = html || '<div class="loading">Không tìm thấy mạng WiFi</div>';
                    document.getElementById('status').className = 'status info';
                    document.getElementById('status').textContent = 'Đã quét xong. Chọn mạng WiFi để kết nối.';
                })
                .catch(e => {
                    document.getElementById('status').className = 'status error';
                    document.getElementById('status').textContent = 'Lỗi quét mạng: ' + e.message;
                });
        }
        
        function selectNetwork(ssid) {
            selectedSSID = ssid;
            document.getElementById('ssid').value = ssid;
            document.querySelectorAll('.network-item').forEach(el => el.classList.remove('selected'));
            event.currentTarget.classList.add('selected');
        }
        
        function saveConfig() {
            const ssid = document.getElementById('ssid').value;
            const pass = document.getElementById('pass').value;
            const mqttServer = document.getElementById('mqtt_server').value;
            const mqttPort = document.getElementById('mqtt_port').value;
            const mqttUser = document.getElementById('mqtt_user').value;
            const mqttPass = document.getElementById('mqtt_pass').value;
            
            if (!ssid) {
                document.getElementById('status').className = 'status error';
                document.getElementById('status').textContent = 'Vui lòng nhập tên mạng WiFi';
                return;
            }
            
            document.getElementById('status').className = 'status info';
            document.getElementById('status').textContent = 'Đang lưu cấu hình...';
            
            const formData = new FormData();
            formData.append('ssid', ssid);
            formData.append('pass', pass);
            formData.append('mqtt_server', mqttServer);
            formData.append('mqtt_port', mqttPort);
            formData.append('mqtt_user', mqttUser);
            formData.append('mqtt_pass', mqttPass);
            
            fetch('/save', { method: 'POST', body: formData })
                .then(r => r.json())
                .then(data => {
                    document.getElementById('status').className = 'status success';
                    document.getElementById('status').textContent = data.message;
                })
                .catch(e => {
                    document.getElementById('status').className = 'status error';
                    document.getElementById('status').textContent = 'Lỗi: ' + e.message;
                });
        }
        
        function resetConfig() {
            if (confirm('Bạn có chắc muốn xóa tất cả cấu hình?')) {
                fetch('/reset').then(() => location.reload());
            }
        }
        
        // Auto scan on load
        scanNetworks();
    </script>
</body>
</html>
)rawliteral";
    
    return html;
}
