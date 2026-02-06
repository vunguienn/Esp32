/**
 * ESP32 System Watchdog & Recovery Module
 * 
 * Bảo vệ hệ thống khỏi các lỗi:
 * - Watchdog Timer: Reset nếu loop bị treo
 * - Heap Monitor: Cảnh báo khi bộ nhớ thấp
 * - Task Monitor: Kiểm tra các task quan trọng
 * - Safe Mode: Chế độ an toàn khi lỗi liên tục
 */

#ifndef SYSTEM_WATCHDOG_H
#define SYSTEM_WATCHDOG_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <Preferences.h>

// ============================================================================
// CẤU HÌNH WATCHDOG
// ============================================================================

// Thời gian timeout cho Task Watchdog (giây)
#define WDT_TIMEOUT_SECONDS     30

// Ngưỡng cảnh báo heap (bytes)
#define HEAP_WARNING_THRESHOLD  20000   // 20KB - cảnh báo
#define HEAP_CRITICAL_THRESHOLD 10000   // 10KB - critical

// Số lần crash liên tiếp để vào Safe Mode
#define MAX_CRASH_COUNT         3

// Thời gian reset crash counter (giây)
#define CRASH_RESET_TIME        300     // 5 phút không crash → reset counter

// ============================================================================
// TRẠNG THÁI HỆ THỐNG
// ============================================================================

enum SystemHealth {
    HEALTH_OK = 0,          // Hệ thống hoạt động bình thường
    HEALTH_WARNING = 1,     // Có cảnh báo nhẹ
    HEALTH_CRITICAL = 2,    // Tình trạng nghiêm trọng
    HEALTH_RECOVERY = 3     // Đang trong chế độ phục hồi
};

struct SystemStatus {
    SystemHealth health;
    uint32_t freeHeap;
    uint32_t minFreeHeap;
    uint32_t uptime;
    uint8_t crashCount;
    bool safeMode;
    bool wdtEnabled;
    unsigned long lastHeapCheck;
    unsigned long lastWdtFeed;
    char lastError[64];
};

// ============================================================================
// LỚP SYSTEM WATCHDOG
// ============================================================================

class SystemWatchdog {
public:
    SystemWatchdog();
    
    /**
     * Khởi tạo watchdog
     * Gọi trong setup() sau khi Serial đã sẵn sàng
     */
    void begin();
    
    /**
     * "Cho ăn" watchdog - gọi trong loop()
     * Nếu không gọi trong WDT_TIMEOUT_SECONDS, ESP32 sẽ reset
     */
    void feed();
    
    /**
     * Kiểm tra sức khỏe hệ thống
     * Gọi định kỳ trong loop()
     */
    void checkHealth();
    
    /**
     * Lấy trạng thái hệ thống
     */
    SystemStatus getStatus();
    
    /**
     * Kiểm tra có đang trong Safe Mode không
     */
    bool isSafeMode() const { return _safeMode; }
    
    /**
     * Ghi nhận lỗi (để tracking)
     */
    void logError(const char* error);
    
    /**
     * Reset crash counter (khi hệ thống chạy ổn)
     */
    void resetCrashCount();
    
    /**
     * Bật/tắt watchdog tạm thời (cho OTA update)
     */
    void disable();
    void enable();
    
    /**
     * Emergency restart với lý do
     */
    void emergencyRestart(const char* reason);
    
    /**
     * Lấy lý do reset lần trước
     */
    String getLastResetReason();
    
private:
    bool _initialized;
    bool _enabled;
    bool _safeMode;
    uint8_t _crashCount;
    unsigned long _startTime;
    unsigned long _lastFeed;
    unsigned long _lastHealthCheck;
    unsigned long _lastStableTime;
    char _lastError[64];
    
    Preferences _prefs;
    
    void loadCrashCount();
    void saveCrashCount();
    void enterSafeMode();
    void exitSafeMode();
    void checkHeap();
    void printResetReason();
};

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

extern SystemWatchdog systemWatchdog;

#endif // SYSTEM_WATCHDOG_H
