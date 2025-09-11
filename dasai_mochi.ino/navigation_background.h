#pragma once
#include <TFT_eSPI.h>

// =======================================================================================
//  Module Vẽ Nền Chỉ Đường Động
// =======================================================================================

namespace NavigationBackground {

// --- Giao diện công khai (Public API) ---

// Khởi tạo module với sprite màn hình
void begin(TFT_eSprite* sprite);

// Cập nhật trạng thái (vị trí vạch kẻ đường, cây cối)
void tick();

// Vẽ toàn bộ khung cảnh nền lên một sprite được cung cấp
void draw(TFT_eSprite* target_sprite);

} // namespace NavigationBackground
