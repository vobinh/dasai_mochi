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
    activeDebounceDelay = 8;  // 8ms
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
  static int lastState = HIGH;
  static int currentState;
  static unsigned long lastDebounceTime = 0;

  static int clickCount = 0;
  static unsigned long lastClickTime = 0;
  static unsigned long multiClickWindow = 350;
  static unsigned long longPressTime = 700;
  static unsigned long pressTime = 0;

  ButtonAction action = ACTION_NONE;
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastState) {
    lastDebounceTime = millis();
  }

  // *** SỬ DỤNG BIẾN CHỐNG NHIỄU LINH HOẠT ***
  if ((millis() - lastDebounceTime) > activeDebounceDelay) {
    if (reading != currentState) {
      currentState = reading;
      if (currentState == activeButtonState) {
        clickCount++;
        pressTime = millis();
        Serial.println("NHAN");
      } else {
        lastClickTime = millis();
        Serial.println("NHA");
      }
    }
  }

  if (currentState == activeButtonState && (millis() - pressTime > longPressTime)) {
    if (clickCount > 0) {
      Serial.println("ACTION_LONG");
      action = ACTION_LONG;
      clickCount = 0;
    }
  }

  if (clickCount > 0 && currentState != activeButtonState && (millis() - lastClickTime > multiClickWindow)) {
    if (clickCount == 1) {
      Serial.println("ACTION_SINGLE");
      action = ACTION_SINGLE;
    }
    if (clickCount == 2) {
      Serial.println("ACTION_DOUBLE");
      action = ACTION_DOUBLE;
    };
    if (clickCount == 3) {
      Serial.println("ACTION_TRIPLE");
      action = ACTION_TRIPLE;
    }
    clickCount = 0;
  }

  lastState = reading;
  return action;
}

#endif  // BUTTON_MANAGER_H
