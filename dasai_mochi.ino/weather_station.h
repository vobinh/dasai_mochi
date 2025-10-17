#ifndef WEATHER_STATION_H
#define WEATHER_STATION_H

#include <TFT_eSPI.h>
#include "FontMaker.h"
#include "globals.h"

// Khởi tạo các tài nguyên cần thiết cho chế độ trạm thời tiết.
void weather_station_init(TFT_eSPI* tft, TFT_eSprite* sprite, MakeFont* font, AppSettings* settings);

// Được gọi khi vào chế độ trạm thời tiết.
void weather_station_enter();

// Vòng lặp chính xử lý logic và vẽ màn hình cho chế độ.
Mode weather_station_loop(ButtonAction action);

// Được gọi khi thoát khỏi chế độ để dọn dẹp.
void weather_station_exit();

void refetchWeatherData();


#endif // WEATHER_STATION_H