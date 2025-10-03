#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

enum ButtonAction {
  ACTION_NONE,
  ACTION_SINGLE,
  ACTION_DOUBLE,
  ACTION_TRIPLE,
  ACTION_LONG
};

enum DisplayShape {
  SHAPE_SQUARE,
  SHAPE_ROUND
};

enum Mode {
  PLAYING,
  MENU,
  GAME_FLAPPY,
  GAME_CAR,
  WATCH_MODE,
  ANALOG_WATCH_MODE,
  WEATHER_MODE,
  MUSIC_LIST_MODE,
  MUSIC_PLAYER_MODE,
  SCROLL_TEXT_SETTINGS_MODE,
  SCROLL_TEXT_MODE,
  SLIDESHOW_MODE,
  WIFI_UPLOAD_MODE,
  DYNAMIC_VIDEO_MODE
};

struct ScrollTextSettings {
  String text;
  int speed;
  uint16_t textColor;
};

struct AppSettings {
  int frameDelay;
  int currentRotation;
  String currentLang;
  int notificationTimeout;
  int marqueeSpeed;
  int currentAnalogFaceIndex;
  bool soundEnabled;  // *** BIẾN MỚI: Bật/tắt âm thanh ***
  int volume;         // *** BIẾN MỚI: Mức âm lượng (0-30) ***
  bool musicAutoPlayNext;
  DisplayShape displayShape;
  ScrollTextSettings scrollText;
  bool bluetoothEnabled;
  bool wifiEnabled;
};

// =======================================================================================
// --- HẰNG SỐ VĂN BẢN MENU (ĐỂ TÁI SỬ DỤNG) ---
// =======================================================================================

// --- Số lượng mục ---
const int NUM_SETTING_ITEMS_CONST = 13;
const int NUM_MODE_ITEMS_CONST = 10;
const int NUM_SCROLL_TEXT_SETTINGS_ITEMS_CONST = 4;

// --- Tiếng Việt ---
const char* const TAB_SETTING_VI PROGMEM = "Cài đặt";
const char* const TAB_MODE_VI PROGMEM = "Chế độ";
const char* const setting_items_vi[NUM_SETTING_ITEMS_CONST] PROGMEM = {
  "Tốc độ video", "Xoay màn hình", "Ngôn ngữ", "TG Thông Báo", "Tốc độ chữ",
  "Âm thanh", "Âm lượng", "Tự Động Chuyển Bài", "Bluetooth", "WiFi", "Hình Dạng", "Lưu", "Thoát"
};
const char* const mode_items_vi[NUM_MODE_ITEMS_CONST] PROGMEM = {
  "Chơi Flappy", "Chơi Đua Xe", "Đồng hồ số", "Đồng hồ kim",
  "Thời tiết", "Chữ chạy", "Nghe nhạc", "Trình chiếu ảnh", "Video Động", "Thoát"
};
const char* const scroll_text_settings_items_vi[NUM_SCROLL_TEXT_SETTINGS_ITEMS_CONST] PROGMEM = {
  "Màu Sắc", "Tốc Độ", "Xem", "Lưu & Thoát"
};

// --- Tiếng Anh ---
const char* const TAB_SETTING_EN PROGMEM = "SETTING";
const char* const TAB_MODE_EN PROGMEM = "MODE";
const char* const setting_items_en[NUM_SETTING_ITEMS_CONST] PROGMEM = {
  "Video Speed", "Screen Rotation", "Language", "Notif. Time", "Marquee Speed",
  "Sound Enabled", "Volume", "Auto Next", "Bluetooth", "WiFi", "Display Shape", "Save", "Exit"
};
const char* const mode_items_en[NUM_MODE_ITEMS_CONST] PROGMEM = {
  "Play Flappy", "Play Car Game", "Watch (Digital)", "Watch (Analog)",
  "Weather", "Scroll Text", "Play Music", "Slideshow", "Dynamic Video", "Exit"
};
const char* const scroll_text_settings_items_en[NUM_SCROLL_TEXT_SETTINGS_ITEMS_CONST] PROGMEM = {
  "Color", "Speed", "View", "Save & Exit"
};

#endif
