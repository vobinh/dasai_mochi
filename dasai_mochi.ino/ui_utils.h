#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <TFT_eSPI.h>
#include "FontMaker.h"
#include <math.h>

// Khai báo các biến toàn cục sẽ được sử dụng bởi các hàm tiện ích
// Các biến này được định nghĩa trong tệp .ino chính
extern TFT_eSPI tft;
extern TFT_eSprite* fontTargetSprite;

/**
 * @brief Vẽ văn bản với hiệu ứng chạy chữ (nếu cần) và nền trong suốt.
 * Đây là phiên bản đã được tối ưu hóa để sử dụng kỹ thuật "đóng dấu" sprite.
 * @param sprite Sprite đích để vẽ lên.
 * @param font Con trỏ tới đối tượng MakeFont.
 * @param text Chuỗi văn bản để hiển thị.
 * @param x Tọa độ X.
 * @param y Tọa độ Y.
 * @param width Chiều rộng tối đa của khu vực văn bản.
 * @param textColor Màu của văn bản.
 * @param bgColor Tham số này được giữ lại để tương thích, nhưng sẽ bị bỏ qua. Nền luôn trong suốt.
 * @param isSelected Nếu là true và văn bản dài hơn width, hiệu ứng chạy chữ sẽ được kích hoạt.
 * @param speed_ms Tốc độ chạy chữ (ms).
 */
void drawMarqueeText(TFT_eSprite* sprite, MakeFont* font, String text, int16_t x, int16_t y, int16_t width, uint16_t textColor, uint16_t bgColor, bool isSelected, uint32_t speed_ms);

void drawMarqueeText(TFT_eSprite* sprite, String text, int16_t x, int16_t y, int16_t width, uint16_t textColor, uint16_t bgColor, bool isSelected, uint32_t speed_ms);

String extractTimeSafe(const String& text);

void drawWeatherIcon(int iconIndex, int x, int y);

String formatFloatSmart(float value);

String replaceAllUtf8(String src, const String& target, const String& replacement);

#endif // UI_UTILS_H
