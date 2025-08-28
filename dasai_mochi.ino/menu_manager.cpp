#include "menu_manager.h"
#include "ui_utils.h"
#include <cmath>

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
    EDIT_NOTIF_TIME,
    EDIT_MARQUEE_SPEED,
    EDIT_SOUND_ENABLED,
    EDIT_VOLUME,
    EDIT_AUTOPLAY,
    EDIT_DISPLAY_SHAPE
} currentEditMode;

static int selectedMenuItem = 0;
static int menuScrollOffset = 0;
static AppSettings temp_settings;

static const int NUM_SETTING_ITEMS = 11;
static const int NUM_MODE_ITEMS = 7;
static String settingMenuItems[NUM_SETTING_ITEMS];
static String modeMenuItems[NUM_MODE_ITEMS];
static String tabNames[2];

static const int MAX_VISIBLE_ITEMS = 6;
static bool save_was_triggered = false;
static void draw_menu_internal();

struct MenuItem {
  String* items;
  int num_items;
};

struct Menu {
  MenuItem tabs[2];
} menu;

// =======================================================================================
// --- TRIỂN KHAI CÁC HÀM (IMPLEMENTATION) ---
// =======================================================================================

void menu_init(TFT_eSPI* tft_ptr, TFT_eSprite* sprite_ptr, MakeFont* font_ptr, AppSettings* settings_ptr) {
  tft = tft_ptr;
  screenSprite = sprite_ptr;
  myfont = font_ptr;
  app_settings = settings_ptr;

  menu.tabs[TAB_SETTING].items = settingMenuItems;
  menu.tabs[TAB_SETTING].num_items = NUM_SETTING_ITEMS;
  menu.tabs[TAB_MODE].items = modeMenuItems;
  menu.tabs[TAB_MODE].num_items = NUM_MODE_ITEMS;
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
  currentTab = TAB_MODE;
  currentEditMode = EDIT_NONE;
  selectedMenuItem = 0;
  menuScrollOffset = 0;
  save_was_triggered = false;
  temp_settings = *app_settings; 
}

bool menu_manager_save_triggered() {
    return save_was_triggered;
}

static void draw_setting_value(int itemIndex, int x, int y, int w, uint16_t textColor, uint16_t bgColor) {
    String valueStr = "";
    switch (itemIndex) {
        case 0: valueStr = String(temp_settings.frameDelay); break;
        case 1: valueStr = String(temp_settings.currentRotation); break;
        case 2: valueStr = temp_settings.currentLang; break;
        case 3: valueStr = String(temp_settings.notificationTimeout) + "s"; break;
        case 4: valueStr = String(temp_settings.marqueeSpeed); break;
        case 5: valueStr = temp_settings.soundEnabled ? "ON" : "OFF"; break;
        case 6: valueStr = String(temp_settings.volume); break;
        case 7: valueStr = temp_settings.musicAutoPlayNext ? "ON" : "OFF"; break;
        case 8: valueStr = (temp_settings.displayShape == SHAPE_SQUARE) ? "Vuong" : "Tron"; break;
    }

    if (valueStr.length() > 0) {
        int textW = myfont->getLength(valueStr);
        myfont->print(x + w - textW - 5, y, valueStr, textColor, bgColor);
    }
}

static void draw_menu_internal() {
    screenSprite->fillSprite(TFT_BLACK);
    const int tabHeight = 30;
    const int paddingY = 10;
    const int itemHeight = 25;
    const int startY = tabHeight + paddingY;
    const int screen_center_x = tft->width() / 2;
    const int screen_radius = tft->width() / 2;

    // Vẽ các tab
    for (int i = 0; i < 2; i++) {
        int tabWidth = tft->width() / 2;
        int tabX = i * tabWidth;
        uint16_t bgColor = (i == currentTab) ? TFT_DARKCYAN : TFT_DARKGREY;
        uint16_t textColor = (i == currentTab) ? TFT_WHITE : TFT_LIGHTGREY;

        if (temp_settings.displayShape == SHAPE_ROUND) {
            // Vẽ tab cong cho màn hình tròn
            for(int y_line = 0; y_line < tabHeight; y_line++) {
                int d = abs(screen_radius - y_line);
                int w = sqrt(screen_radius * screen_radius - d * d);
                int x_start = screen_center_x - w;
                int x_end = screen_center_x + w;
                screenSprite->drawFastHLine(x_start, y_line, x_end - x_start, bgColor);
            }
        } else {
            screenSprite->fillRect(tabX, 0, tabWidth, tabHeight, bgColor);
        }
        
        int textW = myfont->getLength(tabNames[i]);
        myfont->print(tabX + (tabWidth - textW) / 2, 8, tabNames[i], textColor, bgColor);
    }

    // Vẽ danh sách các mục
    MenuItem currentList = menu.tabs[currentTab];
    int numItems = currentList.num_items;
    
    for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
        int itemIndex = menuScrollOffset + i;
        if (itemIndex >= numItems) break;

        int currentY = startY + i * (itemHeight + 5);
        bool isSelected = (itemIndex == selectedMenuItem);
        uint16_t bgColor = isSelected ? TFT_BLUE : TFT_BLACK;
        uint16_t textColor = isSelected ? TFT_WHITE : TFT_LIGHTGREY;
        
        if (currentEditMode != EDIT_NONE && isSelected) {
             bgColor = TFT_RED;
        }

        int itemX = 5;
        int itemW = tft->width() - 10;

        // *** BẮT ĐẦU THAY ĐỔI: Tính toán lại X và Width cho màn hình tròn ***
        if (temp_settings.displayShape == SHAPE_ROUND) {
            int itemCenterY = currentY + (itemHeight / 2);
            int d = abs(screen_radius - itemCenterY);
            if (d < screen_radius) { // Chỉ vẽ nếu mục nằm trong vòng tròn
                int w_half = sqrt(screen_radius * screen_radius - d * d) - 10; // trừ padding
                itemW = w_half * 2;
                itemX = screen_center_x - w_half;
            } else {
                itemW = 0; // Không vẽ mục này
            }
        }
        // *** KẾT THÚC THAY ĐỔI ***

        if (itemW > 0) {
            screenSprite->fillRoundRect(itemX, currentY - 4, itemW, itemHeight + 2, 5, bgColor);
            
            String title = currentList.items[itemIndex];
            int titleMaxWidth = (currentTab == TAB_SETTING) ? itemW * 0.6 : itemW - 10;
            drawMarqueeText(screenSprite, myfont, title, itemX + 5, currentY, titleMaxWidth, textColor, bgColor, isSelected, temp_settings.marqueeSpeed);

            if (currentTab == TAB_SETTING) {
                draw_setting_value(itemIndex, itemX, currentY, itemW, textColor, bgColor);
            }
        }
    }
}

void menu_draw() {
    draw_menu_internal();
    screenSprite->pushSprite(0, 0);
}

Mode menu_handle_action(ButtonAction action) {
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
                case EDIT_NOTIF_TIME:
                    temp_settings.notificationTimeout++;
                    if (temp_settings.notificationTimeout > 10) temp_settings.notificationTimeout = 3;
                    break;
                // *** THÊM LOGIC CHỈNH SỬA TỐC ĐỘ CHỮ ***
                case EDIT_MARQUEE_SPEED:
                    temp_settings.marqueeSpeed -= 5; // Số nhỏ hơn = nhanh hơn
                    if (temp_settings.marqueeSpeed < 10) temp_settings.marqueeSpeed = 50;
                    break;
                case EDIT_SOUND_ENABLED:
                    temp_settings.soundEnabled = !temp_settings.soundEnabled;
                    break;
                case EDIT_VOLUME:
                    temp_settings.volume += 5;
                    if (temp_settings.volume > 30) temp_settings.volume = 0;
                    break;
                case EDIT_AUTOPLAY:
                    temp_settings.musicAutoPlayNext = !temp_settings.musicAutoPlayNext;
                    break;
                case EDIT_DISPLAY_SHAPE:
                    temp_settings.displayShape = (temp_settings.displayShape == SHAPE_SQUARE) ? SHAPE_ROUND : SHAPE_SQUARE;
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
                    case 3: currentEditMode = EDIT_NOTIF_TIME; break;
                    case 4: currentEditMode = EDIT_MARQUEE_SPEED; break;
                    case 5: currentEditMode = EDIT_SOUND_ENABLED; break;
                    case 6: currentEditMode = EDIT_VOLUME; break;
                    case 7: currentEditMode = EDIT_AUTOPLAY; break;
                    case 8: currentEditMode = EDIT_DISPLAY_SHAPE; break;
                    case 9: // Save
                        save_was_triggered = true;
                        *app_settings = temp_settings;
                        return PLAYING; 
                    case 10: // Exit
                        save_was_triggered = false;
                        return PLAYING;
                }
            } else {
                switch (selectedMenuItem) {
                    case 0: return GAME_FLAPPY;
                    case 1: return GAME_CAR;
                    case 2: return WATCH_MODE;
                    case 3: return ANALOG_WATCH_MODE;
                    case 4: return WEATHER_MODE;
                    case 5: return MUSIC_LIST_MODE;
                    case 6: return PLAYING;
                }
            }
        }
    }

    return MENU;
}

String menu_manager_get_temp_lang() {
    return temp_settings.currentLang;
}

int menu_manager_get_temp_rotation() {
    return temp_settings.currentRotation;
}
