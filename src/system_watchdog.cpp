/**
 * ESP32 System Watchdog Implementation
 * 
 * Cung cấp:
 * - Task Watchdog Timer (TWDT) của ESP-IDF
 * - Heap monitoring
 * - Crash counter với Safe Mode
 * - Recovery mechanisms
 */

#include "system_watchdog.h"
#include <esp_system.h>
#include <rom/rtc.h>

// Global instance
SystemWatchdog systemWatchdog;

// ============================================================================
// CONSTRUCTOR
// ============================================================================

SystemWatchdog::SystemWatchdog() {
    _initialized = false;
    _enabled = false;
    _safeMode = false;
    _crashCount = 0;
    _startTime = 0;
    _lastFeed = 0;
    _lastHealthCheck = 0;
    _lastStableTime = 0;
    memset(_lastError, 0, sizeof(_lastError));
}

// ============================================================================
// KHỞI TẠO
// ============================================================================

void SystemWatchdog::begin() {
    _startTime = millis();
    
    Serial.println("[WDT] ═══════════════════════════════════════");
    Serial.println("[WDT] 🛡️  System Watchdog Initializing...");
    
    // In lý do reset lần trước
    esp_reset_reason_t reason = esp_reset_reason();
    printResetReason();
    
    // Load crash counter từ NVS
    loadCrashCount();
    
    // Chỉ tăng counter nếu là crash thật sự (không phải reset thủ công)
    bool isCrash = false;
    switch (reason) {
        case ESP_RST_PANIC:      // Exception/crash
        case ESP_RST_INT_WDT:    // Interrupt watchdog
        case ESP_RST_TASK_WDT:   // Task watchdog (loop hung)
        case ESP_RST_WDT:        // Other watchdog
        case ESP_RST_BROWNOUT:   // Brownout (low voltage)
            isCrash = true;
            break;
        case ESP_RST_POWERON:    // Bật nguồn bình thường
        case ESP_RST_EXT:        // Bấm nút reset
        case ESP_RST_SW:         // Software reset (ESP.restart())
        default:
            isCrash = false;
            break;
    }
    
    // Tăng crash counter chỉ khi là crash
    if (isCrash) {
        _crashCount++;
        saveCrashCount();
        Serial.printf("[WDT] 🔴 Crash detected! Counter: %d/%d\n", _crashCount, MAX_CRASH_COUNT);
    } else {
        // Reset bình thường - xóa crash counter cũ nếu có
        if (_crashCount > 0) {
            Serial.printf("[WDT] ✅ Normal reset detected. Clearing old crash counter (%d)\n", _crashCount);
            _crashCount = 0;
            saveCrashCount();
        } else {
            Serial.printf("[WDT] ✅ Normal reset. Crash counter: %d/%d\n", _crashCount, MAX_CRASH_COUNT);
        }
    }
    
    // Kiểm tra có cần vào Safe Mode không
    if (_crashCount >= MAX_CRASH_COUNT) {
        enterSafeMode();
    }
    
    // Khởi tạo Task Watchdog Timer
    Serial.printf("[WDT] ⏱️  Initializing TWDT with %d second timeout\n", WDT_TIMEOUT_SECONDS);
    
    // Cấu hình TWDT - ESP-IDF API
    esp_err_t err = esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    if (err == ESP_OK) {
        // Đăng ký task hiện tại (loop task)
        err = esp_task_wdt_add(NULL);
        if (err == ESP_OK) {
            _enabled = true;
            _initialized = true;
            Serial.println("[WDT] ✅ Task Watchdog enabled!");
        } else {
            Serial.printf("[WDT] ⚠️  Failed to add task to TWDT: %d\n", err);
        }
    } else if (err == ESP_ERR_INVALID_STATE) {
        // TWDT đã được khởi tạo (bởi framework)
        err = esp_task_wdt_add(NULL);
        if (err == ESP_OK || err == ESP_ERR_INVALID_ARG) {
            _enabled = true;
            _initialized = true;
            Serial.println("[WDT] ✅ Task Watchdog enabled (reused existing)!");
        }
    } else {
        Serial.printf("[WDT] ❌ Failed to init TWDT: %d\n", err);
    }
    
    // In thông tin heap
    Serial.printf("[WDT] 💾 Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[WDT] 💾 Min Free Heap: %d bytes\n", ESP.getMinFreeHeap());
    
    _lastFeed = millis();
    _lastHealthCheck = millis();
    _lastStableTime = millis();
    
    Serial.println("[WDT] ═══════════════════════════════════════");
}

// ============================================================================
// CHO ĂN WATCHDOG
// ============================================================================

void SystemWatchdog::feed() {
    if (!_enabled) return;
    
    esp_err_t err = esp_task_wdt_reset();
    if (err != ESP_OK) {
        Serial.printf("[WDT] ⚠️  Feed failed: %d\n", err);
    }
    
    _lastFeed = millis();
}

// ============================================================================
// KIỂM TRA SỨC KHỎE
// ============================================================================

void SystemWatchdog::checkHealth() {
    unsigned long now = millis();
    
    // Kiểm tra mỗi 5 giây
    if (now - _lastHealthCheck < 5000) return;
    _lastHealthCheck = now;
    
    // Kiểm tra heap
    checkHeap();
    
    // Nếu đã chạy ổn định 5 phút, reset crash counter
    if (now - _lastStableTime > CRASH_RESET_TIME * 1000) {
        if (_crashCount > 0) {
            Serial.println("[WDT] ✅ System stable - resetting crash counter");
            _crashCount = 0;
            saveCrashCount();
            
            // Thoát Safe Mode nếu đang ở trong đó
            if (_safeMode) {
                exitSafeMode();
            }
        }
        _lastStableTime = now;
    }
    
    // Log định kỳ (mỗi 60 giây)
    static unsigned long lastLog = 0;
    if (now - lastLog > 60000) {
        lastLog = now;
        Serial.printf("[WDT] 📊 Health: Heap=%dKB (min=%dKB) | Uptime=%lus | Safe=%s\n",
                      ESP.getFreeHeap() / 1024,
                      ESP.getMinFreeHeap() / 1024,
                      now / 1000,
                      _safeMode ? "YES" : "NO");
    }
}

void SystemWatchdog::checkHeap() {
    uint32_t freeHeap = ESP.getFreeHeap();
    
    if (freeHeap < HEAP_CRITICAL_THRESHOLD) {
        Serial.printf("[WDT] 🔴 CRITICAL: Heap very low! %d bytes\n", freeHeap);
        logError("HEAP_CRITICAL");
        
        // Có thể restart để giải phóng memory
        // emergencyRestart("Heap critical");
    } else if (freeHeap < HEAP_WARNING_THRESHOLD) {
        Serial.printf("[WDT] 🟡 WARNING: Heap low! %d bytes\n", freeHeap);
    }
}

// ============================================================================
// LẤY TRẠNG THÁI
// ============================================================================

SystemStatus SystemWatchdog::getStatus() {
    SystemStatus status;
    
    status.freeHeap = ESP.getFreeHeap();
    status.minFreeHeap = ESP.getMinFreeHeap();
    status.uptime = millis() / 1000;
    status.crashCount = _crashCount;
    status.safeMode = _safeMode;
    status.wdtEnabled = _enabled;
    status.lastHeapCheck = _lastHealthCheck;
    status.lastWdtFeed = _lastFeed;
    strncpy(status.lastError, _lastError, sizeof(status.lastError));
    
    // Xác định health
    if (_safeMode) {
        status.health = HEALTH_RECOVERY;
    } else if (status.freeHeap < HEAP_CRITICAL_THRESHOLD) {
        status.health = HEALTH_CRITICAL;
    } else if (status.freeHeap < HEAP_WARNING_THRESHOLD) {
        status.health = HEALTH_WARNING;
    } else {
        status.health = HEALTH_OK;
    }
    
    return status;
}

// ============================================================================
// SAFE MODE
// ============================================================================

void SystemWatchdog::enterSafeMode() {
    _safeMode = true;
    
    Serial.println();
    Serial.println("[WDT] ╔════════════════════════════════════════╗");
    Serial.println("[WDT] ║  ⚠️  ENTERING SAFE MODE               ║");
    Serial.println("[WDT] ║                                        ║");
    Serial.printf( "[WDT] ║  Crash count: %d (max %d)              ║\n", _crashCount, MAX_CRASH_COUNT);
    Serial.println("[WDT] ║                                        ║");
    Serial.println("[WDT] ║  In Safe Mode:                         ║");
    Serial.println("[WDT] ║  - Automation DISABLED                 ║");
    Serial.println("[WDT] ║  - All relays OFF                      ║");
    Serial.println("[WDT] ║  - WiFi + Web Monitor ACTIVE           ║");
    Serial.println("[WDT] ║                                        ║");
    Serial.println("[WDT] ║  To exit: Run stable for 5 minutes     ║");
    Serial.println("[WDT] ║  Or reset crash count via Serial       ║");
    Serial.println("[WDT] ╚════════════════════════════════════════╝");
    Serial.println();
}

void SystemWatchdog::exitSafeMode() {
    if (!_safeMode) return;
    
    _safeMode = false;
    Serial.println("[WDT] ✅ Exiting Safe Mode - normal operation resumed");
}

// ============================================================================
// ERROR LOGGING
// ============================================================================

void SystemWatchdog::logError(const char* error) {
    strncpy(_lastError, error, sizeof(_lastError) - 1);
    _lastError[sizeof(_lastError) - 1] = '\0';
    
    // Reset stable timer khi có lỗi
    _lastStableTime = millis();
    
    Serial.printf("[WDT] 📝 Error logged: %s\n", error);
}

void SystemWatchdog::resetCrashCount() {
    _crashCount = 0;
    saveCrashCount();
    Serial.println("[WDT] 🔄 Crash counter reset to 0");
    
    if (_safeMode) {
        exitSafeMode();
    }
}

// ============================================================================
// NVS STORAGE
// ============================================================================

void SystemWatchdog::loadCrashCount() {
    _prefs.begin("watchdog", true);  // Read-only
    _crashCount = _prefs.getUChar("crash_cnt", 0);
    _prefs.end();
}

void SystemWatchdog::saveCrashCount() {
    _prefs.begin("watchdog", false);  // Read-write
    _prefs.putUChar("crash_cnt", _crashCount);
    _prefs.end();
}

// ============================================================================
// ENABLE/DISABLE
// ============================================================================

void SystemWatchdog::disable() {
    if (!_enabled) return;
    
    esp_task_wdt_delete(NULL);
    _enabled = false;
    Serial.println("[WDT] ⏸️  Watchdog disabled (for OTA?)");
}

void SystemWatchdog::enable() {
    if (_enabled) return;
    
    esp_task_wdt_add(NULL);
    _enabled = true;
    _lastFeed = millis();
    Serial.println("[WDT] ▶️  Watchdog re-enabled");
}

// ============================================================================
// EMERGENCY RESTART
// ============================================================================

void SystemWatchdog::emergencyRestart(const char* reason) {
    Serial.println();
    Serial.println("[WDT] ╔════════════════════════════════════════╗");
    Serial.println("[WDT] ║  🔴 EMERGENCY RESTART                  ║");
    Serial.printf( "[WDT] ║  Reason: %-30s║\n", reason);
    Serial.println("[WDT] ╚════════════════════════════════════════╝");
    Serial.println();
    
    // Flush serial
    Serial.flush();
    delay(100);
    
    // Restart
    ESP.restart();
}

// ============================================================================
// RESET REASON
// ============================================================================

void SystemWatchdog::printResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    
    Serial.print("[WDT] 🔄 Last reset reason: ");
    
    switch (reason) {
        case ESP_RST_POWERON:
            Serial.println("Power-on reset");
            break;
        case ESP_RST_EXT:
            Serial.println("External reset (button)");
            break;
        case ESP_RST_SW:
            Serial.println("Software reset (ESP.restart())");
            break;
        case ESP_RST_PANIC:
            Serial.println("🔴 PANIC (exception/crash)");
            break;
        case ESP_RST_INT_WDT:
            Serial.println("🔴 Interrupt Watchdog");
            break;
        case ESP_RST_TASK_WDT:
            Serial.println("🔴 Task Watchdog (loop hung)");
            break;
        case ESP_RST_WDT:
            Serial.println("🔴 Other Watchdog");
            break;
        case ESP_RST_DEEPSLEEP:
            Serial.println("Wake from deep sleep");
            break;
        case ESP_RST_BROWNOUT:
            Serial.println("🟡 Brownout (low voltage)");
            break;
        case ESP_RST_SDIO:
            Serial.println("SDIO reset");
            break;
        default:
            Serial.printf("Unknown (%d)\n", reason);
            break;
    }
}

String SystemWatchdog::getLastResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    
    switch (reason) {
        case ESP_RST_POWERON:    return "POWER_ON";
        case ESP_RST_EXT:        return "EXTERNAL";
        case ESP_RST_SW:         return "SOFTWARE";
        case ESP_RST_PANIC:      return "PANIC";
        case ESP_RST_INT_WDT:    return "INT_WDT";
        case ESP_RST_TASK_WDT:   return "TASK_WDT";
        case ESP_RST_WDT:        return "WDT";
        case ESP_RST_DEEPSLEEP:  return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:   return "BROWNOUT";
        default:                 return "UNKNOWN";
    }
}
