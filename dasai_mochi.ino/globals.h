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
  SCROLL_TEXT_MODE
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
  bool soundEnabled; // *** BIẾN MỚI: Bật/tắt âm thanh ***
  int volume;        // *** BIẾN MỚI: Mức âm lượng (0-30) ***
  bool musicAutoPlayNext;
  DisplayShape displayShape;
  ScrollTextSettings scrollText;
  bool bluetoothEnabled;
};

#endif
