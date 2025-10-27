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

String formatFloatSmart(float value)
{
    if (fabs(value - int(value)) < 0.05)
    {
        return String(int(round(value)));
    }
    else
    {
        return String(value, 1);
    }
}

String replaceAllUtf8(String src, const String& target, const String& replacement) {
  int idx = src.indexOf(target);
  while (idx != -1) {
    src = src.substring(0, idx) + replacement + src.substring(idx + target.length());
    idx = src.indexOf(target, idx + replacement.length());
  }
  return src;
}

// void drawWeatherIcon(int iconIndex, int x, int y) {
//      if (!fontTargetSprite) { // Kiểm tra sprite đích
//          Serial.println("Error: fontTargetSprite is NULL in drawWeatherIcon!");
//          return;
//      }

//     if (iconIndex < 0 || iconIndex >= (sizeof(weather_icons) / sizeof(weather_icons[0]))) {
//         iconIndex = 7; // Mặc định là icon "Unknown" nếu chỉ số không hợp lệ
//         Serial.printf("Warning: Invalid iconIndex %d, using default 7.\n", iconIndex);
//     }
//     // Đọc con trỏ từ PROGMEM
//     const uint16_t *icon_ptr = (const uint16_t *)pgm_read_ptr(&weather_icons[iconIndex]);

//     // Vẽ từng pixel, kiểm tra màu trong suốt
//     for (int j = 0; j < WEATHER_H; j++) { // Duyệt qua các hàng (y)
//         for (int i = 0; i < WEATHER_W; i++) { // Duyệt qua các cột (x)
//             // Đọc màu pixel từ PROGMEM
//             uint16_t color = pgm_read_word(&icon_ptr[j * WEATHER_W + i]);
//             // Chỉ vẽ nếu màu không phải là màu đen (trong suốt)
//             if (color != TFT_BLACK) {
//                 // Hoán đổi byte để sửa lỗi màu
//                 fontTargetSprite->drawPixel(x + i, y + j, (color >> 8) | (color << 8));
//             }
//         }
//     }
// }

void drawWeatherIconScaled(int iconIndex, int x, int y, int scale) {
    if (!fontTargetSprite) {
        Serial.println("Error: fontTargetSprite is NULL in drawWeatherIconScaled!");
        return;
    }
     if (scale <= 0) {
         Serial.println("Error: Scale must be positive in drawWeatherIconScaled!");
         drawWeatherIcon(iconIndex, x, y); // Vẽ kích thước gốc nếu scale lỗi
         return;
     }

    if (iconIndex < 0 || iconIndex >= (sizeof(weather_icons) / sizeof(weather_icons[0]))) {
        iconIndex = 7; // Mặc định
        Serial.printf("Warning: Invalid iconIndex %d in scaled, using default 7.\n", iconIndex);
    }

    const uint16_t *icon_ptr = (const uint16_t *)pgm_read_ptr(&weather_icons[iconIndex]);
    int scaled_w = WEATHER_W / scale;
    int scaled_h = WEATHER_H / scale;

    for (int j = 0; j < scaled_h; j++) { // Duyệt qua các hàng (y) của ảnh thu nhỏ
        for (int i = 0; i < scaled_w; i++) { // Duyệt qua các cột (x) của ảnh thu nhỏ
            // Tính toán vị trí pixel tương ứng trong ảnh gốc
            int orig_x = i * scale;
            int orig_y = j * scale;

            // Đọc màu pixel từ PROGMEM tại vị trí gốc
            uint16_t color = pgm_read_word(&icon_ptr[orig_y * WEATHER_W + orig_x]);

            // Chỉ vẽ nếu màu không phải là màu đen (trong suốt)
            if (color != TFT_BLACK) {
                // Hoán đổi byte và vẽ pixel tại vị trí thu nhỏ
                fontTargetSprite->drawPixel(x + i, y + j, (color >> 8) | (color << 8));
            }
        }
    }
}

