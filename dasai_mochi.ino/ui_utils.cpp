#include "ui_utils.h"
#include <vector>

// Cấu trúc để lưu trạng thái cho mỗi dòng chữ chạy
struct MarqueeState {
    int16_t y_pos; // Dùng tọa độ y làm ID duy nhất
    int32_t scroll_x = 0;
    uint32_t last_scroll_time = 0;
    String current_text = "";
};
// Vector để lưu trạng thái của tất cả các dòng chữ chạy
static std::vector<MarqueeState> marquee_states;

// Hàm tiện ích để tìm hoặc tạo trạng thái cho một dòng chữ
MarqueeState* getMarqueeState(int16_t y) {
    for (size_t i = 0; i < marquee_states.size(); ++i) {
        if (marquee_states[i].y_pos == y) {
            return &marquee_states[i];
        }
    }
    // Nếu không tìm thấy, tạo một trạng thái mới
    // *** FIX: Sử dụng cú pháp tương thích với trình biên dịch cũ hơn ***
    MarqueeState newState;
    newState.y_pos = y;
    newState.scroll_x = 0;
    newState.last_scroll_time = 0;
    newState.current_text = "";
    marquee_states.push_back(newState);
    return &marquee_states.back();
}


/**
 * @brief Phiên bản gốc của drawMarqueeText, sử dụng MakeFont.
 */
void drawMarqueeText(TFT_eSprite* sprite, MakeFont* font, String text, int16_t x, int16_t y, int16_t width, uint16_t textColor, uint16_t bgColor, bool isSelected, uint32_t speed_ms) {
    int16_t text_width = font->getLength(text);

    if (text_width <= width || !isSelected) {
        TFT_eSprite textSprite(&tft);
        textSprite.createSprite(width, 20); 
        textSprite.fillSprite(bgColor);
        fontTargetSprite = &textSprite; 
        font->print(0, 0, text, textColor, bgColor); 
        textSprite.pushToSprite(sprite, x, y, bgColor); 
        textSprite.deleteSprite();
        fontTargetSprite = sprite;
        return;
    }

    // Lấy trạng thái độc lập cho dòng chữ này
    MarqueeState* state = getMarqueeState(y);

    if (state->current_text != text) {
        state->current_text = text;
        state->scroll_x = 0;
        state->last_scroll_time = millis();
    }

    if (millis() - state->last_scroll_time > speed_ms) {
        state->last_scroll_time = millis();
        state->scroll_x++;
        if (state->scroll_x > text_width + 15) {
            state->scroll_x = 0;
        }
    }
    
    TFT_eSprite marqueeSprite(&tft);
    marqueeSprite.createSprite(width, 20);
    marqueeSprite.fillSprite(bgColor);
    fontTargetSprite = &marqueeSprite;

    font->print(-state->scroll_x, 0, text, textColor, bgColor);
    font->print(-state->scroll_x + text_width + 15, 0, text, textColor, bgColor);
    
    marqueeSprite.pushToSprite(sprite, x, y, bgColor);
    marqueeSprite.deleteSprite();

    fontTargetSprite = sprite;
}

/**
 * @brief Phiên bản nạp chồng của drawMarqueeText, sử dụng các font tích hợp.
 */
void drawMarqueeText(TFT_eSprite* sprite, String text, int16_t x, int16_t y, int16_t width, uint16_t textColor, uint16_t bgColor, bool isSelected, uint32_t speed_ms) {
    int16_t text_width = sprite->textWidth(text);

    if (text_width <= width || !isSelected) {
        TFT_eSprite textSprite(&tft);
        textSprite.createSprite(width, sprite->fontHeight());
        textSprite.fillSprite(bgColor);
        textSprite.setTextFont(sprite->textfont);
        textSprite.setTextSize(sprite->textsize);
        textSprite.setTextDatum(sprite->textdatum);
        textSprite.setTextColor(textColor, bgColor);
        textSprite.drawString(text, 0, 0);
        textSprite.pushToSprite(sprite, x, y, bgColor);
        textSprite.deleteSprite();
        return;
    }

    // Lấy trạng thái độc lập cho dòng chữ này
    MarqueeState* state = getMarqueeState(y);

    if (state->current_text != text) {
        state->current_text = text;
        state->scroll_x = 0;
        state->last_scroll_time = millis();
    }

    if (millis() - state->last_scroll_time > speed_ms) {
        state->last_scroll_time = millis();
        state->scroll_x++;
        if (state->scroll_x > text_width + 15) {
            state->scroll_x = 0;
        }
    }
    
    TFT_eSprite marqueeSprite(&tft);
    marqueeSprite.createSprite(width, sprite->fontHeight());
    marqueeSprite.fillSprite(bgColor);

    marqueeSprite.setTextFont(sprite->textfont);
    marqueeSprite.setTextSize(sprite->textsize);
    marqueeSprite.setTextDatum(sprite->textdatum);
    marqueeSprite.setTextColor(textColor, bgColor);

    marqueeSprite.drawString(text, -state->scroll_x, 0);
    marqueeSprite.drawString(text, -state->scroll_x + text_width + 15, 0);
    
    marqueeSprite.pushToSprite(sprite, x, y, bgColor);
    marqueeSprite.deleteSprite();
}

String extractTimeSafe(const String& text) {
  int timeStart = text.lastIndexOf('(');
  int timeEnd = text.lastIndexOf(')');
  if (timeStart != -1 && timeEnd != -1 && timeEnd > timeStart) {
    return text.substring(timeStart + 1, timeEnd);
  }
  return "";
}

