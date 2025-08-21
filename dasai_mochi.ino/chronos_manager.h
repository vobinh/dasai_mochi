#ifndef CHRONOS_MANAGER_H
#define CHRONOS_MANAGER_H

#include <TFT_eSPI.h>
#include "FontMaker.h"
#include "globals.h" 
#include "DigitaltsLime35pt7b.h" // *** BAO GỒM FILE FONT MỚI ***

void chronos_init(TFT_eSPI* tft, TFT_eSprite* sprite, MakeFont* font, AppSettings* settings);
void chronos_loop();
void chronos_draw_watch_face();

bool chronos_is_time_synced();
uint8_t chronos_get_hour();
uint8_t chronos_get_minute();
uint8_t chronos_get_second();
uint8_t chronos_get_day();
uint8_t chronos_get_month();
uint16_t chronos_get_year();

#endif
