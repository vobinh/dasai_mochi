#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h> // Include for String type

// Enum for button press types
enum ButtonAction { 
  ACTION_NONE, 
  ACTION_SINGLE, 
  ACTION_DOUBLE, 
  ACTION_TRIPLE, 
  ACTION_LONG 
};

// Enum for different application modes
enum Mode {
  PLAYING,
  MENU,
  GAME_FLAPPY,
  GAME_CAR,
  WATCH_MODE,
  ANALOG_WATCH_MODE
};

// Struct to hold all application settings
struct AppSettings {
  int frameDelay;
  int currentRotation;
  String currentLang;
  bool useSD;
  int notificationTimeout;
  int marqueeSpeed; // *** BIẾN MỚI ĐỂ LƯU TỐC ĐỘ CHỮ CHẠY ***
};

#endif
