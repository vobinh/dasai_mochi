#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <TFT_eSPI.h>
#include "FontMaker.h"

/**
 * @brief Vẽ văn bản với hiệu ứng chạy chữ nếu nó quá dài và đang được chọn.
 * @param speed_ms Thời gian (mili giây) giữa mỗi lần cập nhật vị trí, số nhỏ hơn = nhanh hơn.
 */
// *** ĐÃ SỬA LỖI: Thêm từ khóa "inline" để tránh lỗi "multiple definition" ***
inline void drawMarqueeText(TFT_eSprite* sprite, MakeFont* font, String text, int16_t x, int16_t y, int16_t width, uint16_t textColor, uint16_t bgColor, bool isSelected, uint32_t speed_ms) {
    static int16_t scroll_x = 0;
    static uint32_t last_scroll_time = 0;
    static String currently_selected_text = "";

    int16_t text_width = font->getLength(text);

    if (text_width <= width) {
        if (currently_selected_text == text) {
            currently_selected_text = "";
        }
        font->print(x, y, text, textColor, bgColor);
        return;
    }

    if (!isSelected) {
        if (currently_selected_text == text) {
            currently_selected_text = "";
        }
        sprite->setViewport(x, y, width, 20);
        sprite->fillRect(0, 0, width, 20, bgColor);
        font->print(0, 0, text, textColor, bgColor);
        sprite->resetViewport();
        return;
    }

    if (currently_selected_text != text) {
        currently_selected_text = text;
        scroll_x = 0;
        last_scroll_time = millis();
    }

    if (millis() - last_scroll_time > speed_ms) {
        last_scroll_time = millis();
        scroll_x++;
        if (scroll_x > text_width + 15) {
            scroll_x = 0;
        }
    }

    sprite->setViewport(x, y, width, 20);
    sprite->fillRect(0, 0, width, 20, bgColor);
    font->print(-scroll_x, 0, text, textColor, bgColor);
    font->print(-scroll_x + text_width + 15, 0, text, textColor, bgColor);
    sprite->resetViewport();
}

#endif // UI_UTILS_H
