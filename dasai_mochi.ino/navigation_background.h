#pragma once
#include <TFT_eSPI.h>
#include "chronos_manager.h" 

// *** SỬA LỖI: Thêm include để có khai báo biến toàn cục ***
#include "ui_utils.h" 

// =======================================================================================
//  Module Vẽ Nền Chỉ Đường Động (Đã nâng cấp Animation)
// =======================================================================================

namespace NavigationBackground {

// --- Giao diện công khai (Public API) ---

// Khởi tạo module với sprite màn hình
void begin(TFT_eSprite* sprite);

// Cập nhật trạng thái của tất cả các animation
void tick();

// Vẽ toàn bộ khung cảnh nền lên một sprite được cung cấp, dựa trên giờ hiện tại
void draw(TFT_eSprite* target_sprite, int current_hour);

// Reset lại trạng thái của hiệu ứng
void reset();

// Kích hoạt một animation chuyển cảnh
void triggerAnimation(NavInstructionType type);

// Điều khiển tốc độ của hiệu ứng
void setSpeed(int speed);

} // namespace NavigationBackground

