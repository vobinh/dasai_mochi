#ifndef ANCS_MANAGER_H
#define ANCS_MANAGER_H

#include "globals.h"

// Forward declaration để tránh include vòng lặp
struct Notification; 
struct Navigation;

/**
 * @brief Cấu trúc chứa các con trỏ đến các biến trạng thái chung.
 * Điều này cho phép ancs_manager cập nhật trạng thái của chronos_manager
 * mà không cần biến toàn cục.
 */
struct BleSharedState {
    bool* isConnected;
    bool* isRinging;
    bool* hasNewNotification;
    String* callerInfo;
    Notification* latestNotification;
    // Thêm các con trỏ khác nếu cần
};


/**
 * @brief Khởi tạo module ANCS cho iOS.
 * @param settings Con trỏ đến cấu hình ứng dụng.
 * @param state Con trỏ đến các biến trạng thái chia sẻ.
 */
void ancs_init(AppSettings* settings, BleSharedState* state);

/**
 * @brief Vòng lặp cho module ANCS, xử lý quét và kết nối lại.
 */
void ancs_loop();

#endif // ANCS_MANAGER_H
