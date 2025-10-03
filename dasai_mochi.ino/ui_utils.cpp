#include "ui_utils.h"

/**
 * @brief Triển khai hàm drawMarqueeText đã được tối ưu hóa.
 * Sử dụng kỹ thuật tạo sprite tạm thời ("đóng dấu") để đạt được nền trong suốt
 * với các font chữ tùy chỉnh.
 */
void drawMarqueeText(TFT_eSprite* sprite, MakeFont* font, String text, int16_t x, int16_t y, int16_t width, uint16_t textColor, uint16_t bgColor, bool isSelected, uint32_t speed_ms) {
  int16_t text_width = font->getLength(text);

  // TH1: Văn bản ngắn hoặc không được chọn -> Vẽ tĩnh (có cắt bớt nếu cần).
  if (text_width <= width || !isSelected) {
    TFT_eSprite textSprite(&tft);
    // Tạo một sprite tạm thời vừa đủ với khu vực hiển thị
    textSprite.createSprite(width, 20);

    fontTargetSprite = &textSprite;                 // Hướng mục tiêu vẽ font vào sprite tạm
    font->print(0, 0, text, textColor, TFT_BLACK);  // Vẽ lên sprite tạm với nền đen

    // "Đóng dấu" lên sprite chính, coi màu đen là trong suốt
    textSprite.pushToSprite(sprite, x, y, TFT_BLACK);
    textSprite.deleteSprite();  // Giải phóng bộ nhớ

    fontTargetSprite = sprite;
    return;
  }

  // TH2: Văn bản dài VÀ đang được chọn -> Kích hoạt hiệu ứng chạy chữ.
  static int16_t scroll_x = 0;
  static uint32_t last_scroll_time = 0;
  static String currently_selected_text = "";

  // Reset vị trí cuộn nếu văn bản thay đổi
  if (currently_selected_text != text) {
    currently_selected_text = text;
    scroll_x = 0;
    last_scroll_time = millis();
  }

  // Cập nhật vị trí cuộn dựa trên thời gian
  if (millis() - last_scroll_time > speed_ms) {
    last_scroll_time = millis();
    scroll_x++;
    if (scroll_x > text_width + 15) {  // +15 để tạo khoảng trống lặp lại
      scroll_x = 0;
    }
  }

  // Tạo một "cửa sổ" tạm thời cho hiệu ứng chạy chữ
  TFT_eSprite marqueeSprite(&tft);
  marqueeSprite.createSprite(width, 20);
  fontTargetSprite = &marqueeSprite;

  // Vẽ văn bản 2 lần để tạo vòng lặp mượt mà
  font->print(-scroll_x, 0, text, textColor, TFT_BLACK);
  font->print(-scroll_x + text_width + 15, 0, text, textColor, TFT_BLACK);

  // "Đóng dấu" cửa sổ chạy chữ này lên sprite chính
  marqueeSprite.pushToSprite(sprite, x, y, TFT_BLACK);
  marqueeSprite.deleteSprite();

  fontTargetSprite = sprite;
}

void drawMarqueeText(TFT_eSprite* sprite, String text, int16_t x, int16_t y, int16_t width, uint16_t textColor, uint16_t bgColor, bool isSelected, uint32_t speed_ms) {
  int16_t text_width = sprite->textWidth(text);

  // TH1: Văn bản ngắn hoặc không được chọn -> Vẽ tĩnh.
  if (text_width <= width || !isSelected) {
    TFT_eSprite textSprite(&tft);
    textSprite.createSprite(width, sprite->fontHeight());
    textSprite.fillSprite(bgColor);

    // Sao chép các thuộc tính font từ sprite chính
    textSprite.setTextFont(sprite->textfont);
    textSprite.setTextSize(sprite->textsize);
    textSprite.setTextDatum(sprite->textdatum);
    textSprite.setTextColor(textColor, bgColor);

    textSprite.drawString(text, 0, 0);

    textSprite.pushToSprite(sprite, x, y);
    textSprite.deleteSprite();
    return;
  }

  // TH2: Văn bản dài và được chọn -> Chạy chữ.
  static int32_t scroll_x_default = 0;
  static uint32_t last_scroll_time_default = 0;
  static String currently_selected_text_default = "";

  if (currently_selected_text_default != text) {
    currently_selected_text_default = text;
    scroll_x_default = 0;
    last_scroll_time_default = millis();
  }

  if (millis() - last_scroll_time_default > speed_ms) {
    last_scroll_time_default = millis();
    scroll_x_default++;
    if (scroll_x_default > text_width + 15) {
      scroll_x_default = 0;
    }
  }

  TFT_eSprite marqueeSprite(&tft);
  marqueeSprite.createSprite(width, sprite->fontHeight());
  marqueeSprite.fillSprite(bgColor);

  // Sao chép các thuộc tính font
  marqueeSprite.setTextFont(sprite->textfont);
  marqueeSprite.setTextSize(sprite->textsize);
  marqueeSprite.setTextDatum(sprite->textdatum);
  marqueeSprite.setTextColor(textColor, bgColor);

  marqueeSprite.drawString(text, -scroll_x_default, 0);
  marqueeSprite.drawString(text, -scroll_x_default + text_width + 15, 0);

  marqueeSprite.pushToSprite(sprite, x, y);
  marqueeSprite.deleteSprite();
}

String extractTimeSafe(const String& text) {
  // Duyệt qua chuỗi để tìm định dạng "HH:MM"
  for (int i = 0; i <= text.length() - 5; i++) {
    // Kiểm tra 5 ký tự: số, số, hai chấm, số, số
    if (isdigit(text[i]) && isdigit(text[i + 1]) && text[i + 2] == ':' && isdigit(text[i + 3]) && isdigit(text[i + 4])) {
      return text.substring(i, i + 5);  // Trả về chuỗi "HH:MM"
    }
  }
  return "";  // Trả về chuỗi rỗng nếu không tìm thấy
}
