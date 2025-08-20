#pragma once

#include <TFT_eSPI.h>
#include "FontMaker.h"

// Khai báo các hàm sẽ được sử dụng trong file .ino chính
void chronos_init(TFT_eSPI* tft, TFT_eSprite* sprite, MakeFont* font);
void chronos_loop();
void chronos_draw_watch_face();

// Khai báo các hàm getter để lấy thông tin thời gian và trạng thái
bool chronos_is_time_synced();
uint8_t chronos_get_hour();
uint8_t chronos_get_minute();
uint8_t chronos_get_second();
uint8_t chronos_get_day();
uint8_t chronos_get_month();
uint16_t chronos_get_year();
