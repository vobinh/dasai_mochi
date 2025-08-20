#include "menu_manager.h"
#include "ui_utils.h"

// =======================================================================================
// --- BIẾN STATIC (CHỈ DÙNG TRONG FILE NÀY) ---
// =======================================================================================
static TFT_eSPI* tft = nullptr;
static TFT_eSprite* screenSprite = nullptr;
static MakeFont* myfont = nullptr;
static AppSettings* app_settings = nullptr;

static enum MenuTab { TAB_SETTING, TAB_MODE } currentTab;
static enum EditMode { 
    EDIT_NONE, 
    EDIT_SPEED, 
    EDIT_ROTATION, 
    EDIT_LANGUAGE, 
    EDIT_SD, 
    EDIT_NOTIF_TIME 
} currentEditMode;

static int selectedMenuItem = 0;
static int menuScrollOffset = 0;
static AppSettings temp_settings;

static const int NUM_SETTING_ITEMS = 7;
static const int NUM_MODE_ITEMS = 5;
static String settingMenuItems[NUM_SETTING_ITEMS];
static String modeMenuItems[NUM_MODE_ITEMS];
static String tabNames[2];

static const int MAX_VISIBLE_ITEMS = 6;

static bool save_was_triggered = false;

static void draw_menu_internal();

// =======================================================================================
// --- TRIỂN KHAI CÁC HÀM (IMPLEMENTATION) ---
// =======================================================================================

void menu_init(TFT_eSPI* tft_ptr, TFT_eSprite* sprite_ptr, MakeFont* font_ptr, AppSettings* settings_ptr) {
    tft = tft_ptr;
    screenSprite = sprite_ptr;
    myfont = font_ptr;
    app_settings = settings_ptr;
}

void menu_load_strings(const JsonObject& doc) {
    JsonObject menu_text = (app_settings->currentLang == "vi") ? doc["menu_vi"] : doc["menu_en"];
    if (menu_text) {
        tabNames[0] = menu_text["tab_setting"].as<String>();
        tabNames[1] = menu_text["tab_mode"].as<String>();
        JsonObject setting_text = menu_text["setting"];
        JsonObject mode_text = menu_text["mode"];
        if (setting_text && mode_text && setting_text.size() >= NUM_SETTING_ITEMS && mode_text.size() >= NUM_MODE_ITEMS) {
            for (int i = 0; i < NUM_SETTING_ITEMS; i++) settingMenuItems[i] = setting_text["item" + String(i)].as<String>();
            for (int i = 0; i < NUM_MODE_ITEMS; i++) modeMenuItems[i] = mode_text["item" + String(i)].as<String>();
        }
    }
}

void menu_enter() {
    currentEditMode = EDIT_NONE;
    save_was_triggered = false; 
    memcpy(&temp_settings, app_settings, sizeof(AppSettings));
}

void menu_draw() {
    draw_menu_internal();
}

// *** HÀM ĐÃ ĐƯỢC SỬA LẠI HOÀN TOÀN ***
Mode menu_handle_action(ButtonAction action) {
    // Nếu không có hành động nào, không làm gì cả
    if (action == ACTION_NONE) {
        return MENU;
    }

    if (currentEditMode != EDIT_NONE) {
        if (action == ACTION_SINGLE) {
            switch (currentEditMode) {
                case EDIT_SPEED:
                    temp_settings.frameDelay += 5;
                    if (temp_settings.frameDelay > 50) temp_settings.frameDelay = 0;
                    break;
                case EDIT_ROTATION:
                    temp_settings.currentRotation = (temp_settings.currentRotation + 1) % 4;
                    break;
                case EDIT_LANGUAGE:
                    temp_settings.currentLang = (temp_settings.currentLang == "vi") ? "en" : "vi";
                    break;
                case EDIT_SD:
                    temp_settings.useSD = !temp_settings.useSD;
                    break;
                case EDIT_NOTIF_TIME:
                    temp_settings.notificationTimeout++;
                    if (temp_settings.notificationTimeout > 10) temp_settings.notificationTimeout = 3;
                    break;
                default: break;
            }
        } else if (action == ACTION_LONG) {
            currentEditMode = EDIT_NONE;
        }
    } 
    else {
        if (action == ACTION_DOUBLE) {
            currentTab = (currentTab == TAB_SETTING) ? TAB_MODE : TAB_SETTING;
            selectedMenuItem = 0;
            menuScrollOffset = 0;
        } else if (action == ACTION_SINGLE) {
            int maxItems = (currentTab == TAB_SETTING) ? NUM_SETTING_ITEMS : NUM_MODE_ITEMS;
            selectedMenuItem = (selectedMenuItem + 1) % maxItems;
            if (selectedMenuItem == 0) {
                menuScrollOffset = 0;
            } else if (selectedMenuItem >= menuScrollOffset + MAX_VISIBLE_ITEMS) {
                menuScrollOffset = selectedMenuItem - MAX_VISIBLE_ITEMS + 1;
            } else if (selectedMenuItem < menuScrollOffset) {
                menuScrollOffset = selectedMenuItem;
            }
        } else if (action == ACTION_LONG) {
            if (currentTab == TAB_SETTING) {
                switch (selectedMenuItem) {
                    case 0: currentEditMode = EDIT_SPEED; break;
                    case 1: currentEditMode = EDIT_ROTATION; break;
                    case 2: currentEditMode = EDIT_LANGUAGE; break;
                    case 3: currentEditMode = EDIT_SD; break;
                    case 4: currentEditMode = EDIT_NOTIF_TIME; break;
                    case 5: // Save
                        memcpy(app_settings, &temp_settings, sizeof(AppSettings));
                        save_was_triggered = true;
                        return PLAYING;
                    case 6: // Exit
                        save_was_triggered = false;
                        return PLAYING;
                }
            } else {
                switch (selectedMenuItem) {
                    case 0: return GAME_FLAPPY;
                    case 1: return GAME_CAR;
                    case 2: return WATCH_MODE;
                    case 3: return ANALOG_WATCH_MODE;
                    case 4: return PLAYING;
                }
            }
        }
    }

    return MENU; // Mặc định là vẫn ở trong menu
}

String menu_manager_get_temp_lang() {
    return temp_settings.currentLang;
}

int menu_manager_get_temp_rotation() {
    return temp_settings.currentRotation;
}

bool menu_manager_save_triggered() {
    return save_was_triggered;
}

static void draw_menu_internal() {
  screenSprite->fillSprite(TFT_BLACK);

  const int paddingX = 10;
  const int itemHeight = 28;
  const int cornerRadius = 5;
  const int tabHeight = 30;
  const int tabWidth = tft->width() / 2;
  const int scrollbarWidth = 6;

  if (currentTab == TAB_SETTING) {
    screenSprite->fillRoundRect(0, 0, tabWidth, tabHeight, cornerRadius, TFT_BLUE);
    screenSprite->drawRoundRect(tabWidth, 0, tabWidth, tabHeight, cornerRadius, TFT_WHITE);
  } else {
    screenSprite->drawRoundRect(0, 0, tabWidth, tabHeight, cornerRadius, TFT_WHITE);
    screenSprite->fillRoundRect(tabWidth, 0, tabWidth, tabHeight, cornerRadius, TFT_BLUE);
  }
  myfont->print((tabWidth - myfont->getLength(tabNames[0])) / 2, (tabHeight - 16) / 2, tabNames[0], TFT_WHITE, (currentTab == TAB_SETTING) ? TFT_BLUE : TFT_BLACK);
  myfont->print(tabWidth + (tabWidth - myfont->getLength(tabNames[1])) / 2, (tabHeight - 16) / 2, tabNames[1], TFT_WHITE, (currentTab == TAB_MODE) ? TFT_BLUE : TFT_BLACK);

  String* currentMenuItems = (currentTab == TAB_SETTING) ? settingMenuItems : modeMenuItems;
  int numCurrentItems = (currentTab == TAB_SETTING) ? NUM_SETTING_ITEMS : NUM_MODE_ITEMS;
  
  int itemToHighlight = -1;
  if (currentTab == TAB_SETTING) {
    if (currentEditMode == EDIT_SPEED) itemToHighlight = 0;
    if (currentEditMode == EDIT_ROTATION) itemToHighlight = 1;
    if (currentEditMode == EDIT_LANGUAGE) itemToHighlight = 2;
    if (currentEditMode == EDIT_SD) itemToHighlight = 3;
    if (currentEditMode == EDIT_NOTIF_TIME) itemToHighlight = 4;
  }

  int startItem = menuScrollOffset;
  int endItem = min(startItem + MAX_VISIBLE_ITEMS, numCurrentItems);

  for (int i = startItem; i < endItem; i++) {
    int displayIndex = i - menuScrollOffset;
    int currentY = tabHeight + 10 + displayIndex * (itemHeight + 5);
    uint16_t textColor = TFT_WHITE;
    uint16_t bgColor = TFT_BLACK;

    bool isSelected = (i == selectedMenuItem && currentEditMode == EDIT_NONE);

    if (isSelected) {
      screenSprite->drawRoundRect(paddingX / 2, currentY - 4, tft->width() - paddingX - scrollbarWidth - 5, itemHeight, cornerRadius, TFT_WHITE);
    }
    if (i == itemToHighlight) {
      screenSprite->fillRoundRect(paddingX / 2, currentY - 4, tft->width() - paddingX - scrollbarWidth - 5, itemHeight, cornerRadius, TFT_WHITE);
      textColor = TFT_BLACK;
      bgColor = TFT_WHITE;
    }

    String title = currentMenuItems[i];
    String valueStr = "";
    if (currentTab == TAB_SETTING) {
      if (i == 0) valueStr = String(temp_settings.frameDelay);
      if (i == 1) valueStr = String(temp_settings.currentRotation*90) + " deg";
      if (i == 2) valueStr = (temp_settings.currentLang == "vi") ? "VI" : "EN";
      if (i == 3) valueStr = temp_settings.useSD ? "ON" : "OFF";
      if (i == 4) valueStr = String(temp_settings.notificationTimeout) + "s";
    }

    int totalAvailableWidth = tft->width() - paddingX * 2 - scrollbarWidth - 10;
    int titleDrawWidth;

    if (currentTab == TAB_SETTING && valueStr.length() > 0) {
        int valueWidth = myfont->getLength(valueStr);
        titleDrawWidth = totalAvailableWidth - valueWidth - 10;
    } else {
        titleDrawWidth = totalAvailableWidth;
    }
    
    drawMarqueeText(screenSprite, myfont, title, paddingX + 5, currentY, titleDrawWidth, textColor, bgColor, isSelected);

    if (valueStr.length() > 0) {
      int textW = myfont->getLength(valueStr);
      myfont->print(tft->width() - textW - paddingX - scrollbarWidth - 5, currentY, valueStr, textColor, bgColor);
    }
  }

  if (numCurrentItems > MAX_VISIBLE_ITEMS) {
    int menuHeight = tft->height() - tabHeight - 10;
    int scrollbarX = tft->width() - scrollbarWidth - 2;
    
    screenSprite->drawRect(scrollbarX, tabHeight + 5, scrollbarWidth, menuHeight, TFT_DARKGREY);
    
    float thumbHeight = (float)MAX_VISIBLE_ITEMS / numCurrentItems * menuHeight;
    float thumbY = tabHeight + 5 + ((float)menuScrollOffset / numCurrentItems * menuHeight);
    screenSprite->fillRoundRect(scrollbarX, thumbY, scrollbarWidth, thumbHeight, 2, TFT_WHITE);
  }
  
  screenSprite->pushSprite(0, 0);
}
