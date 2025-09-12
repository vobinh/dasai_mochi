#pragma once
#include <TFT_eSPI.h>

// =======================================================================================
//  Module Vẽ Nền Chỉ Đường Động
// =======================================================================================

namespace NavigationBackground {

// --- Giao diện công khai (Public API) ---

// Khởi tạo module với sprite màn hình
void begin(TFT_eSprite* sprite);

// Cập nhật trạng thái (vị trí vạch kẻ đường, đèn đường)
void tick();

// Vẽ toàn bộ khung cảnh nền lên một sprite được cung cấp, dựa trên giờ hiện tại
void draw(TFT_eSprite* target_sprite, int current_hour);

// *** HÀM MỚI: Điều khiển tốc độ của hiệu ứng ***
void setSpeed(int speed);

// *** HÀM MỚI: Reset lại trạng thái của hiệu ứng ***
void reset();

} // namespace NavigationBackground

