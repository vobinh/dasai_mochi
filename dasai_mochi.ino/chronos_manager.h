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

// *** ENUM ĐÃ ĐƯỢC BỔ SUNG ĐẦY ĐỦ TỪ FILE THAM KHẢO ***
enum NavDirectionID
{
    DirectionNone = 0,
    DirectionStart = 1,
    DirectionEasyLeft = 2,
    DirectionEasyRight = 3,
    DirectionEnd = 4,
    DirectionVia = 5,
    DirectionKeepLeft = 6,
    DirectionKeepRight = 7,
    DirectionLeft = 8,
    DirectionOutOfRoute = 9,
    DirectionRight = 10,
    DirectionSharpLeft = 11,
    DirectionSharpRight = 12,
    DirectionStraight = 13,
    DirectionUTurnLeft = 14,
    DirectionUTurnRight = 15,
    DirectionFerry = 16,
    DirectionStateBoundary = 17,
    DirectionFollow = 18,
    DirectionMotorway = 19,
    DirectionTunnel = 20,
    DirectionExitLeft = 21,
    DirectionExitRight = 22,
    DirectionRoundaboutRSE  = 23,
    DirectionRoundaboutRE   = 24,
    DirectionRoundaboutRNE  = 25,
    DirectionRoundaboutRN   = 26,
    DirectionRoundaboutRNW  = 27,
    DirectionRoundaboutRW   = 28,
    DirectionRoundaboutRSW  = 29,
    DirectionRoundaboutRS   = 30,
    DirectionRoundaboutLSE  = 31,
    DirectionRoundaboutLE   = 32,
    DirectionRoundaboutLNE  = 33,
    DirectionRoundaboutLN   = 34,
    DirectionRoundaboutLNW  = 35,
    DirectionRoundaboutLW   = 36,
    DirectionRoundaboutLSW  = 37,
    DirectionRoundaboutLS   = 38
};

enum NavInstructionType {
    NAV_UNKNOWN,
    NAV_STRAIGHT,
    NAV_TURN_LEFT,
    NAV_TURN_RIGHT,
    NAV_SHARP_LEFT,
    NAV_SHARP_RIGHT,
    NAV_ROUNDABOUT,
    NAV_ARRIVED,
    NAV_U_TURN
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
