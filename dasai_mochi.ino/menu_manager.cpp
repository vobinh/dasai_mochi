#include "menu_manager.h"
#include "ui_utils.h" // Sử dụng tệp .h đã được chuẩn hóa
#include <cmath>

// =======================================================================================
// --- BIẾN STATIC (CHỈ DÙNG TRONG FILE NÀY) ---
// =======================================================================================
// *** SỬA LỖI: Loại bỏ khai báo 'tft' bị xung đột.
// File này sẽ sử dụng biến 'tft' toàn cục được khai báo trong ui_utils.h
static TFT_eSprite* screenSprite = nullptr;
static MakeFont* myfont = nullptr;
static AppSettings* app_settings = nullptr;

static enum MenuTab { TAB_SETTING,
                      TAB_MODE } currentTab;
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
  EDIT_DISPLAY_SHAPE,
  EDIT_BLUETOOTH
} currentEditMode;

static int selectedMenuItem = 0;
static int menuScrollOffset = 0;
static AppSettings temp_settings;

// *** SỬA LỖI: Quay lại sử dụng số lượng mục menu cố định để đảm bảo ổn định ***
static const int NUM_SETTING_ITEMS = 12;
static const int NUM_MODE_ITEMS = 8;
static String settingMenuItems[NUM_SETTING_ITEMS];
static String modeMenuItems[NUM_MODE_ITEMS];
static String tabNames[2];

static bool save_was_triggered = false;
static void draw_menu_internal();
// =======================================================================================
// --- TRIỂN KHAI CÁC HÀM (IMPLEMENTATION) ---
// =======================================================================================

// *** SỬA LỖI: Cập nhật chữ ký hàm init ***
void menu_init(TFT_eSprite* sprite_ptr, MakeFont* font_ptr, AppSettings* settings_ptr) {
  screenSprite = sprite_ptr;
  myfont = font_ptr;
  app_settings = settings_ptr;
}

// *** SỬA LỖI: Cập nhật logic tải chuỗi để xử lý các tệp config cũ một cách an toàn ***
void menu_load_strings(const JsonObject& doc) {
  JsonObject menu_text = (app_settings->currentLang == "vi") ? doc["menu_vi"] : doc["menu_en"];
  
  // Giá trị mặc định để phòng trường hợp tệp config cũ hoặc không hợp lệ
  String default_settings_vi[] = {"Tốc độ video", "Xoay màn hình", "Ngôn ngữ", "TG Thông Báo", "Tốc độ chữ", "Âm thanh", "Âm lượng", "Tự Động Chuyển Bài", "Bluetooth", "Hình Dạng", "Lưu", "Thoát"};
  String default_modes_vi[] = {"Chơi Flappy", "Chơi Đua Xe", "Đồng hồ số", "Đồng hồ kim", "Thời tiết", "Chữ chạy", "Nghe nhạc", "Thoát"};
  String default_settings_en[] = {"Video Speed", "Screen Rotation", "Language", "Notif. Time", "Marquee Speed", "Sound Enabled", "Volume", "Auto Next", "Bluetooth", "Display Shape", "Save", "Exit"};
  String default_modes_en[] = {"Play Flappy", "Play Car Game", "Watch (Digital)", "Watch (Analog)", "Weather", "Scroll Text", "Play Music", "Exit"};

  if (menu_text && !menu_text.isNull()) {
    tabNames[0] = menu_text["tab_setting"] | ((app_settings->currentLang == "vi") ? "Cài đặt" : "Setting");
    tabNames[1] = menu_text["tab_mode"] | ((app_settings->currentLang == "vi") ? "Chế độ" : "Mode");

    JsonObject setting_text = menu_text["setting"];
    JsonObject mode_text = menu_text["mode"];
    
    String* default_settings = (app_settings->currentLang == "vi") ? default_settings_vi : default_settings_en;
    String* default_modes = (app_settings->currentLang == "vi") ? default_modes_vi : default_modes_en;

    for (int i = 0; i < NUM_SETTING_ITEMS; i++) {
        String key = "item" + String(i);
        settingMenuItems[i] = setting_text[key] | default_settings[i];
    }
    for (int i = 0; i < NUM_MODE_ITEMS; i++) {
        String key = "item" + String(i);
        modeMenuItems[i] = mode_text[key] | default_modes[i];
    }
  } else {
      // Nếu không có đối tượng menu_text, tải toàn bộ giá trị mặc định
      tabNames[0] = (app_settings->currentLang == "vi") ? "Cài đặt" : "Setting";
      tabNames[1] = (app_settings->currentLang == "vi") ? "Chế độ" : "Mode";
      if (app_settings->currentLang == "vi") {
          for(int i=0; i<NUM_SETTING_ITEMS; i++) settingMenuItems[i] = default_settings_vi[i];
          for(int i=0; i<NUM_MODE_ITEMS; i++) modeMenuItems[i] = default_modes_vi[i];
      } else {
          for(int i=0; i<NUM_SETTING_ITEMS; i++) settingMenuItems[i] = default_settings_en[i];
          for(int i=0; i<NUM_MODE_ITEMS; i++) modeMenuItems[i] = default_modes_en[i];
      }
  }
}


void menu_enter(bool resetSelection) {
  currentEditMode = EDIT_NONE;
  save_was_triggered = false;
  memcpy(&temp_settings, app_settings, sizeof(AppSettings));
  if (resetSelection) {
    currentTab = TAB_SETTING;
    selectedMenuItem = 0;
    menuScrollOffset = 0;
  }
}

void menu_draw() {
  draw_menu_internal();
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
    case 8: valueStr = temp_settings.bluetoothEnabled ? "ON" : "OFF"; break;
    case 9: valueStr = (temp_settings.displayShape == SHAPE_SQUARE) ? "Vuông" : "Tròn"; break;
  }

  if (valueStr.length() > 0) {
    int textW = myfont->getLength(valueStr);
    myfont->print(x + w - textW - 5, y, valueStr, textColor, bgColor);
  }
}

static void draw_menu_internal() {
  fontTargetSprite = screenSprite;
  
  screenSprite->fillSprite(TFT_BLACK);
  const int tabHeight = 30;
  // *** SỬA LỖI: Sử dụng tft. thay vì tft-> ***
  const int screen_center_x = tft.width() / 2;
  const int screen_radius = tft.width() / 2;

  // Draw tabs
  for (int i = 0; i < 2; i++) {
    uint16_t bgColor = (i == currentTab) ? TFT_DARKCYAN : TFT_DARKGREY;
    uint16_t textColor = (i == currentTab) ? TFT_WHITE : TFT_LIGHTGREY;
    int tabWidth = tft.width() / 2;
    int tabX = i * tabWidth;
    screenSprite->fillRect(tabX, 0, tabWidth, tabHeight, bgColor);

    int textW = myfont->getLength(tabNames[i]);
    int textX;
    if (app_settings->displayShape == SHAPE_ROUND) {
      int padding = 5;
      if (i == 0) { textX = screen_center_x - textW - padding; } 
      else { textX = screen_center_x + padding; }
    } else {
      textX = tabX + (tabWidth - textW) / 2;
    }
    myfont->print(textX, 7, tabNames[i], textColor, bgColor);
  }

  // Draw menu items
  String* currentItems = (currentTab == TAB_SETTING) ? settingMenuItems : modeMenuItems;
  int numItems = (currentTab == TAB_SETTING) ? NUM_SETTING_ITEMS : NUM_MODE_ITEMS;
  
  int visibleItems = (temp_settings.displayShape == SHAPE_ROUND) ? 5 : 6;
  const int listHeight = tft.height() - tabHeight - 10;
  const int itemHeight = 28;
  const int totalItemsHeight = visibleItems * itemHeight;
  const int itemSpacing = (visibleItems > 1) ? (listHeight - totalItemsHeight) / (visibleItems - 1) : 0;
  const int startY = tabHeight + 5;
  
  if (temp_settings.displayShape == SHAPE_ROUND) {
    const int centerSlot = visibleItems / 2;
    int firstItemLogicalIndex = selectedMenuItem - centerSlot;

    for (int i = 0; i < visibleItems; i++) {
      int itemIndex = (firstItemLogicalIndex + i + numItems) % numItems;
      int currentY = startY + i * (itemHeight + itemSpacing);
      bool isSelected = (itemIndex == selectedMenuItem);
      uint16_t bgColor = isSelected ? TFT_BLUE : TFT_BLACK;
      uint16_t textColor = isSelected ? TFT_WHITE : TFT_LIGHTGREY;
      if (currentEditMode != EDIT_NONE && isSelected) bgColor = TFT_RED;
      
      int itemX = 5, itemW = tft.width() - 10;
      int itemCenterY = currentY + (itemHeight / 2);
      int d = abs(screen_radius - itemCenterY);
      if (d < screen_radius) {
        int w_half = sqrt(screen_radius * screen_radius - d * d) - 5;
        if (w_half > 0) { itemW = w_half * 2; itemX = screen_center_x - w_half; } 
        else { itemW = 0; }
      } else { itemW = 0; }

      if (itemW > 0) {
        screenSprite->fillRoundRect(itemX, currentY - 4, itemW, itemHeight + 2, 5, bgColor);
        String title = currentItems[itemIndex];
        int titleMaxWidth = (currentTab == TAB_SETTING) ? itemW * 0.6 : itemW - 5;
        drawMarqueeText(screenSprite, myfont, title, itemX + 5, currentY, titleMaxWidth, textColor, bgColor, isSelected, temp_settings.marqueeSpeed);
        if (currentTab == TAB_SETTING) draw_setting_value(itemIndex, itemX, currentY, itemW, textColor, bgColor);
      }
    }
  } else { // SHAPE_SQUARE
    for (int i = 0; i < visibleItems; i++) {
      int itemIndex = menuScrollOffset + i;
      if (itemIndex >= numItems) break;

      int currentY = startY + i * (itemHeight + itemSpacing);
      bool isSelected = (itemIndex == selectedMenuItem);
      uint16_t bgColor = isSelected ? TFT_BLUE : TFT_BLACK;
      uint16_t textColor = isSelected ? TFT_WHITE : TFT_LIGHTGREY;
      if (currentEditMode != EDIT_NONE && isSelected) bgColor = TFT_RED;
      
      int itemX = 5, itemW = tft.width() - 10;
      screenSprite->fillRoundRect(itemX, currentY - 4, itemW, itemHeight + 2, 5, bgColor);
      String title = currentItems[itemIndex];
      int titleMaxWidth = (currentTab == TAB_SETTING) ? itemW * 0.6 : itemW - 5;
      drawMarqueeText(screenSprite, myfont, title, itemX + 5, currentY, titleMaxWidth, textColor, bgColor, isSelected, temp_settings.marqueeSpeed);
      if (currentTab == TAB_SETTING) draw_setting_value(itemIndex, itemX, currentY, itemW, textColor, bgColor);
    }
  }
  screenSprite->pushSprite(0, 0);
}


Mode menu_handle_action(ButtonAction action) {
  if (action == ACTION_NONE) return MENU;

  if (currentEditMode != EDIT_NONE) {
    if (action == ACTION_SINGLE) {
      switch (currentEditMode) {
        case EDIT_SPEED: temp_settings.frameDelay += 5; if (temp_settings.frameDelay > 50) temp_settings.frameDelay = 0; break;
        case EDIT_ROTATION: temp_settings.currentRotation = (temp_settings.currentRotation + 1) % 4; break;
        case EDIT_LANGUAGE: temp_settings.currentLang = (temp_settings.currentLang == "vi") ? "en" : "vi"; break;
        case EDIT_NOTIF_TIME: temp_settings.notificationTimeout++; if (temp_settings.notificationTimeout > 10) temp_settings.notificationTimeout = 3; break;
        case EDIT_MARQUEE_SPEED: temp_settings.marqueeSpeed -= 5; if (temp_settings.marqueeSpeed < 10) temp_settings.marqueeSpeed = 50; break;
        case EDIT_SOUND_ENABLED: temp_settings.soundEnabled = !temp_settings.soundEnabled; break;
        case EDIT_VOLUME: temp_settings.volume += 5; if (temp_settings.volume > 30) temp_settings.volume = 0; break;
        case EDIT_AUTOPLAY: temp_settings.musicAutoPlayNext = !temp_settings.musicAutoPlayNext; break;
        case EDIT_BLUETOOTH: temp_settings.bluetoothEnabled = !temp_settings.bluetoothEnabled; break;
        case EDIT_DISPLAY_SHAPE: temp_settings.displayShape = (temp_settings.displayShape == SHAPE_SQUARE) ? SHAPE_ROUND : SHAPE_SQUARE; break;
        default: break;
      }
    } else if (action == ACTION_LONG) {
      currentEditMode = EDIT_NONE;
    }
  } else {
    if (action == ACTION_DOUBLE) {
      currentTab = (currentTab == TAB_SETTING) ? TAB_MODE : TAB_SETTING;
      selectedMenuItem = 0;
      menuScrollOffset = 0;
    } else if (action == ACTION_SINGLE) {
      int maxItems = (currentTab == TAB_SETTING) ? NUM_SETTING_ITEMS : NUM_MODE_ITEMS;
      selectedMenuItem = (selectedMenuItem + 1) % maxItems;
      int maxVisibleItems = (temp_settings.displayShape == SHAPE_ROUND) ? 5 : 6;

      if (temp_settings.displayShape == SHAPE_ROUND) {
        const int centerSlot = maxVisibleItems / 2;
        menuScrollOffset = selectedMenuItem - centerSlot;
        if (menuScrollOffset < 0) menuScrollOffset = 0;
        if (maxItems > maxVisibleItems) {
            if (menuScrollOffset > maxItems - maxVisibleItems) {
                menuScrollOffset = maxItems - maxVisibleItems;
            }
        } else {
            menuScrollOffset = 0;
        }
      } else {
        if (selectedMenuItem == 0) menuScrollOffset = 0;
        else if (selectedMenuItem >= menuScrollOffset + maxVisibleItems) menuScrollOffset = selectedMenuItem - maxVisibleItems + 1;
        else if (selectedMenuItem < menuScrollOffset) menuScrollOffset = selectedMenuItem;
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
          case 8: currentEditMode = EDIT_BLUETOOTH; break;
          case 9: currentEditMode = EDIT_DISPLAY_SHAPE; break;
          case 10:
            memcpy(app_settings, &temp_settings, sizeof(AppSettings));
            save_was_triggered = true;
            return PLAYING;
          case 11:
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
          case 5: return SCROLL_TEXT_SETTINGS_MODE;
          case 6: return MUSIC_LIST_MODE;
          case 7: return PLAYING;
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

bool menu_manager_save_triggered() {
  return save_was_triggered;
}

