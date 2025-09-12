#ifndef CHRONOS_MANAGER_H
#define CHRONOS_MANAGER_H

#include <TFT_eSPI.h>
#include "FontMaker.h"
#include "globals.h" 

// *** CẤU TRÚC MỚI ĐỂ LƯU DỮ LIỆU THỜI TIẾT ***
struct WeatherData {
  String city;
  String updateTime;
  int currentTemp;
  int highTemp;
  int lowTemp;
  int icon;
  int pressure;
  int uv;
};

enum ChronosAction {
  CHRONOS_ACTION_NONE,
  CHRONOS_ACTION_SAVE_SETTINGS,
  CHRONOS_ACTION_RESET_CONFIG
};

void chronos_init(TFT_eSPI* tft, TFT_eSprite* sprite, MakeFont* font, AppSettings* settings);
void chronos_loop();
bool chronos_draw_alerts(ButtonAction action);

// --- CÁC HÀM GETTER ---
bool chronos_is_time_synced();
uint8_t chronos_get_hour();
uint8_t chronos_get_minute();
uint8_t chronos_get_second();
uint8_t chronos_get_day();
uint8_t chronos_get_month();
uint16_t chronos_get_year();
bool chronos_is_ringing();
bool chronos_has_new_notification();
bool chronos_has_new_navigation();

// *** CÁC HÀM MỚI ĐỂ LẤY DỮ LIỆU THỜI TIẾT *** 
bool chronos_has_weather_data();
WeatherData chronos_get_weather();

ChronosAction chronos_get_requested_action();

#endif
