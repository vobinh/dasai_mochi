#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include "globals.h"  // Để sử dụng enum ButtonAction

// --- CẤU HÌNH ---
#define BUTTON_PIN 1

// --- BIẾN TĨNH (CHỈ DÙNG TRONG MODULE NÀY) ---
static int activeButtonState;
// *** BIẾN MỚI ĐỂ LƯU THỜI GIAN CHỐNG NHIỄU TƯƠNG ỨNG ***
static unsigned long activeDebounceDelay;

// --- KHAI BÁO HÀM ---
void button_init();
ButtonAction getButtonAction();
bool is_button_held();  // *** HÀM MỚI ĐỂ KIỂM TRA NHẤN GIỮ ***

// --- TRIỂN KHAI HÀM ---

/**
 * @brief Tự động phát hiện loại nút và cấu hình chân GPIO.
 * Được gọi một lần duy nhất trong hàm setup().
 */
void button_init() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  delay(10);

  int idleState = digitalRead(BUTTON_PIN);

  if (idleState == LOW) {
    Serial.println("Button Type: Touch (TTP223) detected.");
    activeButtonState = HIGH;
    // *** NÚT CẢM ỨNG CẦN THỜI GIAN CHỐNG NHIỄU DÀI HƠN ***
    activeDebounceDelay = 5;  // 5ms
    pinMode(BUTTON_PIN, INPUT);
  } else {
    Serial.println("Button Type: Physical detected.");
    activeButtonState = LOW;
    // *** NÚT CƠ CÓ THỂ DÙNG THỜI GIAN NGẮN HƠN ***
    activeDebounceDelay = 50;  // 50ms
  }
}

/**
 * @brief Kiểm tra xem nút có đang được nhấn giữ hay không.
 * @return true nếu nút đang được nhấn, ngược lại trả về false.
 */
bool is_button_held() {
  return digitalRead(BUTTON_PIN) == activeButtonState;
}

/**
 * @brief Lấy hành động của nút bấm (nhấn đơn, đúp, dài...).
 * Hoạt động với cả nút cơ và nút cảm ứng đã được nhận diện.
 */
ButtonAction getButtonAction() {
  // --- Trạng thái nút ---
  enum BtnState { IDLE, PRESSING, HELD, RELEASING };
  static BtnState btnState = IDLE;

  static int clickCount = 0;
  static unsigned long pressTime = 0;
  static unsigned long releaseTime = 0;

  const unsigned long longPressTime = 700;    // >700ms = LONG
  const unsigned long multiClickWindow = 600; // thời gian gom click

  ButtonAction action = ACTION_NONE;

  // --- Đọc trạng thái nút ---
  bool isPressed = (digitalRead(BUTTON_PIN) == activeButtonState);

  switch (btnState) {
    case IDLE:
      if (isPressed) {
        btnState = PRESSING;
        pressTime = millis();
        Serial.println("[BTN] → PRESSING");
      }
      break;

    case PRESSING:
      if (!isPressed) {
        // Nhả nhanh => click ngắn
        btnState = RELEASING;
        releaseTime = millis();
        clickCount++;
        Serial.printf("[BTN] RELEASE -> clickCount=%d\n", clickCount);
      } else if (millis() - pressTime > longPressTime) {
        // Nhấn giữ lâu
        action = ACTION_LONG;
        clickCount = 0;
        btnState = HELD;
        Serial.println("[BTN] ACTION_LONG");
      }
      break;

    case HELD:
      if (!isPressed) {
        btnState = IDLE;  // reset sau khi thả
        Serial.println("[BTN] HELD → IDLE");
      }
      break;

    case RELEASING:
      if (isPressed) {
        // Lại bấm tiếp
        btnState = PRESSING;
        pressTime = millis();
        Serial.println("[BTN] RE-PRESS → PRESSING");
      } else if (millis() - releaseTime > multiClickWindow) {
        // Hết thời gian gom click -> xác nhận loại click
        if (clickCount == 1) {
          action = ACTION_SINGLE;
          Serial.println("[BTN] ACTION_SINGLE");
        } else if (clickCount == 2) {
          action = ACTION_DOUBLE;
          Serial.println("[BTN] ACTION_DOUBLE");
        } else if (clickCount == 3) {
          action = ACTION_TRIPLE;
          Serial.println("[BTN] ACTION_TRIPLE");
        }
        clickCount = 0;
        btnState = IDLE;
        Serial.println("[BTN] RELEASING → IDLE");
      }
      break;
  }

  return action;
}




#endif  // BUTTON_MANAGER_H
