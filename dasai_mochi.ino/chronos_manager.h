#ifndef CHRONOS_MANAGER_H
#define CHRONOS_MANAGER_H

#include <TFT_eSPI.h>
#include "FontMaker.h"
#include "globals.h" 

void chronos_init(TFT_eSPI* tft, TFT_eSprite* sprite, MakeFont* font, AppSettings* settings);
void chronos_loop();

bool chronos_draw_alerts();

// --- CÁC HÀM GETTER ĐỂ LẤY THÔNG TIN ---
bool chronos_is_time_synced();
uint8_t chronos_get_hour();
uint8_t chronos_get_minute();
uint8_t chronos_get_second();
uint8_t chronos_get_day();
uint8_t chronos_get_month();
uint16_t chronos_get_year();
bool chronos_is_ringing();
bool chronos_has_new_notification();
bool chronos_has_new_navigation(); // *** HÀM MỚI ĐỂ KIỂM TRA CHỈ ĐƯỜNG ***

#endif
