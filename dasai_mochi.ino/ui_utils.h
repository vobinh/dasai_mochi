#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <TFT_eSPI.h>
#include "FontMaker.h"

/**
 * @brief Vẽ văn bản với hiệu ứng chạy chữ nếu nó quá dài và đang được chọn.
 * @param sprite Con trỏ đến sprite để vẽ lên.
 * @param font Con trỏ đến đối tượng font.
 * @param text Chuỗi văn bản để hiển thị.
 * @param x Tọa độ X của khung nhìn.
 * @param y Tọa độ Y của khung nhìn.
 * @param width Chiều rộng tối đa của khu vực hiển thị.
 * @param textColor Màu chữ.
 * @param bgColor Màu nền.
 * @param isSelected True nếu mục này hiện đang được chọn.
 */
void drawMarqueeText(TFT_eSprite* sprite, MakeFont* font, String text, int16_t x, int16_t y, int16_t width, uint16_t textColor, uint16_t bgColor, bool isSelected) {
    // Biến static để quản lý trạng thái của DUY NHẤT mục đang được chọn
    static int16_t scroll_x = 0;
    static uint32_t last_scroll_time = 0;
    static String currently_selected_text = "";

    int16_t text_width = font->getLength(text);

    // --- TRƯỜNG HỢP 1: Văn bản vừa vặn, không cần làm gì phức tạp ---
    if (text_width <= width) {
        // Nếu mục này trước đó đang chạy chữ, hãy reset trạng thái
        if (currently_selected_text == text) {
            currently_selected_text = "";
        }
        font->print(x, y, text, textColor, bgColor);
        return;
    }

    // --- TRƯỜNG HỢP 2: Văn bản dài, NHƯNG KHÔNG được chọn ---
    if (!isSelected) {
        // Nếu mục này trước đó đang chạy chữ, hãy reset trạng thái
        if (currently_selected_text == text) {
            currently_selected_text = "";
        }
        // Vẽ tĩnh và cắt bớt phần thừa
        sprite->setViewport(x, y, width, 20);
        sprite->fillRect(0, 0, width, 20, bgColor); // Xóa nền trước
        font->print(0, 0, text, textColor, bgColor);
        sprite->resetViewport();
        return;
    }

    // --- TRƯỜNG HỢP 3: Văn bản dài VÀ ĐANG được chọn (Đây là lúc chạy chữ) ---

    // Nếu người dùng vừa chuyển lựa chọn sang mục này, hãy reset vị trí cuộn
    if (currently_selected_text != text) {
        currently_selected_text = text;
        scroll_x = 0;
        last_scroll_time = millis();
    }

    // Cập nhật vị trí cuộn dựa trên thời gian
    if (millis() - last_scroll_time > 35) { // Tốc độ cuộn
        last_scroll_time = millis();
        scroll_x++;
        // Khi đã cuộn hết, quay lại từ đầu
        if (scroll_x > text_width + 15) { // +15 là khoảng cách giữa 2 lần lặp
            scroll_x = 0;
        }
    }

    // Thiết lập khung nhìn để vẽ
    sprite->setViewport(x, y, width, 20);

    // Xóa sạch khu vực viewport trước khi vẽ để tránh lỗi chữ đè lên nhau
    sprite->fillRect(0, 0, width, 20, bgColor);

    // Vẽ văn bản ở vị trí đã cuộn
    font->print(-scroll_x, 0, text, textColor, bgColor);
    
    // Vẽ một bản sao của văn bản ở phía sau để tạo hiệu ứng lặp lại liền mạch
    font->print(-scroll_x + text_width + 15, 0, text, textColor, bgColor);

    // QUAN TRỌNG: Reset viewport
    sprite->resetViewport();
}

#endif // UI_UTILS_H
