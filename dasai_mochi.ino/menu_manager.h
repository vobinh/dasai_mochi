#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include "FontMaker.h"
#include "globals.h"

void menu_init(TFT_eSPI* tft_ptr, TFT_eSprite* sprite_ptr, MakeFont* font_ptr, AppSettings* settings_ptr);
void menu_load_strings(const JsonObject& doc);
void menu_enter(bool resetSelection = false);
Mode menu_handle_action(ButtonAction action);
void menu_draw();

// Functions to get temporary values for comparison before saving
String menu_manager_get_temp_lang();
int menu_manager_get_temp_rotation();

/**
 * @brief Checks if the "Save" action was the reason for exiting the menu.
 * @return True if save was triggered, false otherwise.
 */
bool menu_manager_save_triggered();

#endif
