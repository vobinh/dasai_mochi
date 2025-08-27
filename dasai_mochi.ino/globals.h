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

enum Mode {
  PLAYING,
  MENU,
  GAME_FLAPPY,
  GAME_CAR,
  WATCH_MODE,
  ANALOG_WATCH_MODE,
  WEATHER_MODE,
  MUSIC_LIST_MODE,
  MUSIC_PLAYER_MODE
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
};

#endif
