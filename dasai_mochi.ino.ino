#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include "SD.h"
#include <SPI.h>
#include <vector>
#include <time.h>
#include "flappy_game.h"
#include "car_game.h"
#include <ChronosESP32.h>
#include "FontMaker.h"
#include "background_image.h"
#include "hour_hand.h"
#include "minute_hand.h"
#include "second_hand.h"

// =======================================================================================
// --- CẤU HÌNH ---
// =======================================================================================
#define BUTTON_PIN 0
#define VIDEO_JUMP_TARGET 2
#define CONFIG_FILE "/config.json"

#define SD_CS_PIN 7

// --- Định nghĩa Struct ---
typedef struct _VideoInfo {
  const uint8_t* const* frames;
  const uint16_t* frames_size;
  uint16_t num_frames;
} VideoInfo;

enum ButtonAction { ACTION_NONE, ACTION_SINGLE, ACTION_DOUBLE, ACTION_TRIPLE, ACTION_LONG };

// --- Khai báo các file video (dùng khi không có thẻ SD) ---
#include "video01.h"
#include "video02.h"
#include "video03.h"
#include "video04.h"

// =======================================================================================
// --- KHỞI TẠO BIẾN TOÀN CỤC ---
// =======================================================================================
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite screenSprite = TFT_eSprite(&tft);
ChronosESP32 Chronos("Mochi Watch");

TFT_eSprite hourHandSprite = TFT_eSprite(&tft);
TFT_eSprite minuteHandSprite = TFT_eSprite(&tft);
TFT_eSprite secondHandSprite = TFT_eSprite(&tft);

// BIẾN CHO RTC
bool timeIsSynced = false;
uint8_t rtc_hour, rtc_minute, rtc_second, rtc_day, rtc_month;
uint16_t rtc_year;

void setSpritePixel(int16_t x, int16_t y, uint16_t color) {
  screenSprite.drawPixel(x, y, color);
}
MakeFont myfont(&setSpritePixel);

// Biến dữ liệu
Notification latestNotification;
Navigation latestNavigation;
String callerInfo;
bool hasNewNotification = false;
bool hasNewNavigation = false;
bool isConnected = false;
bool isRinging = false;
unsigned long notificationDisplayTime = 0;
uint32_t nav_icon_crc = 0xFFFFFFFF;

// Biến cuộn thông báo
std::vector<String> wrappedMessageLines;
int messageScrollLine = 0;
unsigned long lastMessageScrollTime = 0;
bool isNotificationScrolling = false;

// Video
VideoInfo* flashVideoList[] = { 
  &video01,
  &video02,
  &video03,
  &video04,
};

const uint8_t NUM_FLASH_VIDEOS = sizeof(flashVideoList) / sizeof(flashVideoList[0]);
std::vector<String> sdVideoList;
bool sdCardOk = false;
uint8_t currentVideoIndex = 0;
uint16_t currentFrame = 0;

enum Mode {
  PLAYING,
  MENU,
  EDIT_SPEED,
  EDIT_ROTATION,
  EDIT_LANGUAGE,
  EDIT_SD,
  EDIT_NOTIF_TIME,
  GAME_FLAPPY,
  GAME_CAR,
  WATCH_MODE,
  ANALOG_WATCH_MODE
};
Mode currentMode = PLAYING;

// Cài đặt
int frameDelay = 20;
int currentRotation = 3;
String currentLang = "vi";
bool useSD = false;
int notificationTimeout = 5;

// Cài đặt tạm thời
int tempFrameDelay = 10;
int tempRotation = 3;
String tempLang = "vi";
bool tempUseSD = false;
int tempNotificationTimeout = 5;

// Menu
enum MenuTab { TAB_SETTING,
               TAB_MODE };
MenuTab currentTab = TAB_SETTING;
const int NUM_SETTING_ITEMS = 7;
const int NUM_MODE_ITEMS = 5; // *** TĂNG LÊN 5 ĐỂ THÊM MỤC "Thoát" ***
String settingMenuItems[NUM_SETTING_ITEMS];
String modeMenuItems[NUM_MODE_ITEMS];
String tabNames[2];
int selectedMenuItem = 0;

// =======================================================================================
// --- BIẾN MỚI CHO GIAO DIỆN MENU CUỘN ---
// =======================================================================================
int menuScrollOffset = 0;
const int MAX_VISIBLE_ITEMS = 6; // Số mục menu tối đa hiển thị cùng lúc


// =======================================================================================
// --- CÁC HÀM XỬ LÝ THỜI GIAN RTC ---
// =======================================================================================

void syncTimeToRTC() {
  if (!Chronos.isConnected()) return;

  struct tm timeinfo;
  timeinfo.tm_year = Chronos.getYear() - 1900;
  timeinfo.tm_mon = Chronos.getMonth() - 1;
  timeinfo.tm_mday = Chronos.getDay();
  timeinfo.tm_hour = Chronos.getHourC();
  timeinfo.tm_min = Chronos.getMinute();
  timeinfo.tm_sec = Chronos.getSecond();

  time_t t = mktime(&timeinfo);
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);

  timeIsSynced = true;
  Serial.println("RTC Synced from Bluetooth.");
}

void getTimeFromRTC() {
  if (!timeIsSynced) return;

  struct tm timeinfo;
  time_t now;
  time(&now);
  localtime_r(&now, &timeinfo);

  rtc_hour = timeinfo.tm_hour;
  rtc_minute = timeinfo.tm_min;
  rtc_second = timeinfo.tm_sec;
  rtc_day = timeinfo.tm_mday;
  rtc_month = timeinfo.tm_mon + 1;
  rtc_year = timeinfo.tm_year + 1900;
}


// =======================================================================================
// --- CÁC HÀM CHỨC NĂNG KHÁC ---
// =======================================================================================

void wrapMessage(String text) {
  wrappedMessageLines.clear();
  if (text.length() == 0) return;
  const int maxWidth = tft.width() - 20;
  String currentLine = "";
  int lastSpace = -1;
  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    if (c == '\n') {
      wrappedMessageLines.push_back(currentLine);
      currentLine = "";
      lastSpace = -1;
      continue;
    }
    String testLine = currentLine + c;
    if (myfont.getLength(testLine) <= maxWidth) {
      currentLine += c;
      if (c == ' ') lastSpace = currentLine.length() - 1;
    } else {
      if (lastSpace != -1) {
        wrappedMessageLines.push_back(currentLine.substring(0, lastSpace));
        currentLine = currentLine.substring(lastSpace + 1);
        lastSpace = -1;
      } else {
        wrappedMessageLines.push_back(currentLine);
        currentLine = "";
      }
      currentLine += c;
    }
  }
  if (currentLine.length() > 0) wrappedMessageLines.push_back(currentLine);
}

void drawAnalogWatchFace() {
  getTimeFromRTC();
  screenSprite.pushImage(0, 0, 240, 240, backgroundImage);

  if (timeIsSynced) {
    char dateStr[10];
    sprintf(dateStr, "%02d/%02d", rtc_day, rtc_month);
    myfont.print(30, tft.height() / 2 - 10, dateStr, TFT_WHITE, TFT_BLACK);

    float sec_angle = rtc_second * 6;
    float min_angle = rtc_minute * 6 + rtc_second * 0.1;
    float hour_angle = (rtc_hour % 12) * 30 + rtc_minute * 0.5;

    hourHandSprite.pushRotated(&screenSprite, hour_angle, TFT_BLACK);
    minuteHandSprite.pushRotated(&screenSprite, min_angle, TFT_BLACK);
    secondHandSprite.pushRotated(&screenSprite, sec_angle, TFT_BLACK);

    screenSprite.fillCircle(120, 120, 4, TFT_RED); // SECOND_DOT

    int batteryPercent = 86;
    int steps = 1526;
    int centerX = 120, centerY = 120;
    int battery_angle_start = 225;
    int battery_angle_end = battery_angle_start + (int)(batteryPercent * 0.9);
    screenSprite.drawArc(centerX, centerY, 110, 108, battery_angle_start, battery_angle_end, TFT_CYAN, TFT_DARKGREY);

    int steps_angle_start = 135;
    float step_percent = (steps > 5000) ? 1.0 : (float)steps / 5000.0;
    int steps_angle_end = steps_angle_start - (int)(step_percent * 90);
    screenSprite.drawArc(centerX, centerY, 110, 108, steps_angle_end, steps_angle_start, TFT_ORANGE, TFT_DARKGREY);


  } else {
    myfont.print((tft.width() - myfont.getLength("--:--")) / 2, tft.height() / 2 - 10, "--:--", TFT_WHITE, TFT_BLACK);
  }

  screenSprite.pushSprite(0, 0);
}

void connectionCallback(bool state) {
  isConnected = state;
  if (state) {
    syncTimeToRTC();
  }
}

void notificationCallback(Notification notification) {
  latestNotification = notification;
  hasNewNotification = true;
  wrapMessage(latestNotification.message);
}

void ringerCallback(String caller, bool state) {
  callerInfo = caller;
  isRinging = state;
}

void configCallback(Config config, uint32_t a, uint32_t b) {
  if (config == CF_NAV_DATA) {
    if (a) {
      latestNavigation = Chronos.getNavigation();
      hasNewNavigation = true;
      hasNewNotification = false;
    } else {
      hasNewNavigation = false;
    }
  }
  if (config == CF_NAV_ICON) {
    if (a == 2) {
      Navigation tempNav = Chronos.getNavigation();
      if (nav_icon_crc != tempNav.iconCRC) {
        nav_icon_crc = tempNav.iconCRC;
        latestNavigation = tempNav;
        hasNewNavigation = true;
      }
    }
  }
}

void saveSettings() {
  Serial.println("Saving settings to SPIFFS...");
  File configFile = SPIFFS.open(CONFIG_FILE, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  StaticJsonDocument<1024> doc;
  doc["frameDelay"] = frameDelay;
  doc["currentRotation"] = currentRotation;
  doc["language"] = currentLang;
  doc["useSD"] = useSD;
  doc["notificationTimeout"] = notificationTimeout;

  JsonObject menu_vi = doc.createNestedObject("menu_vi");
  menu_vi["tab_setting"] = "Cài đặt";
  menu_vi["tab_mode"] = "Chế độ";
  JsonObject setting_vi = menu_vi.createNestedObject("setting");
  setting_vi["item0"] = "Tốc độ video";
  setting_vi["item1"] = "Xoay màn hình";
  setting_vi["item2"] = "Ngôn ngữ";
  setting_vi["item3"] = "Thẻ SD";
  setting_vi["item4"] = "TG Thông Báo";
  setting_vi["item5"] = "Lưu";
  setting_vi["item6"] = "Thoát";
  JsonObject mode_vi = menu_vi.createNestedObject("mode");
  mode_vi["item0"] = "Chơi Flappy";
  mode_vi["item1"] = "Chơi Đua Xe";
  mode_vi["item2"] = "Đồng hồ số";
  mode_vi["item3"] = "Đồng hồ kim";
  mode_vi["item4"] = "Thoát";

  JsonObject menu_en = doc.createNestedObject("menu_en");
  menu_en["tab_setting"] = "SETTING";
  menu_en["tab_mode"] = "MODE";
  JsonObject setting_en = menu_en.createNestedObject("setting");
  setting_en["item0"] = "Video Speed";
  setting_en["item1"] = "Screen Rotation";
  setting_en["item2"] = "Language";
  setting_en["item3"] = "SD Card";
  setting_en["item4"] = "Notif. Time";
  setting_en["item5"] = "Save";
  setting_en["item6"] = "Exit";
  JsonObject mode_en = menu_en.createNestedObject("mode");
  mode_en["item0"] = "Play Flappy";
  mode_en["item1"] = "Play Car Game";
  mode_en["item2"] = "Watch (Digital)";
  mode_en["item3"] = "Watch (Analog)";
  mode_en["item4"] = "Exit";

  if (serializeJson(doc, configFile) == 0) {
    Serial.println("Failed to write to file");
  } else {
    Serial.println("Settings saved successfully");
  }
  configFile.close();
}

void loadSettings() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed. Formatting...");
    return;
  }

  if (SPIFFS.exists(CONFIG_FILE)) {
    Serial.println("Reading config file from SPIFFS...");
    File configFile = SPIFFS.open(CONFIG_FILE, "r");
    if (configFile) {
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, configFile);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      } else {
        frameDelay = doc["frameDelay"] | 20;
        currentRotation = doc["currentRotation"] | 3;
        currentLang = doc["language"] | "vi";
        useSD = doc["useSD"] | false;
        notificationTimeout = doc["notificationTimeout"] | 5;

        JsonObject menu_text = (currentLang == "vi") ? doc["menu_vi"] : doc["menu_en"];
        if (menu_text) {
          tabNames[0] = menu_text["tab_setting"].as<String>();
          tabNames[1] = menu_text["tab_mode"].as<String>();
          JsonObject setting_text = menu_text["setting"];
          JsonObject mode_text = menu_text["mode"];
          if (setting_text && mode_text && setting_text.size() >= NUM_SETTING_ITEMS && mode_text.size() >= NUM_MODE_ITEMS) {
            for (int i = 0; i < NUM_SETTING_ITEMS; i++) settingMenuItems[i] = setting_text["item" + String(i)].as<String>();
            for (int i = 0; i < NUM_MODE_ITEMS; i++) modeMenuItems[i] = mode_text["item" + String(i)].as<String>();
          } else {
            Serial.println("Menu items outdated in config, creating and saving.");
            saveSettings();
            loadSettings();
          }
        } else {
          Serial.println("Menu text not found in config, creating and saving.");
          saveSettings();
          loadSettings();
        }

        Serial.println("Settings loaded successfully");
      }
      configFile.close();
    }
  } else {
    Serial.println("Config file not found. Creating with default settings.");
    saveSettings();
    loadSettings();
  }
}

void scanSdVideos() {
  sdVideoList.clear();
  Serial.println("Scanning SD card for videos...");
  File root = SD.open("/videos");
  if (!root || !root.isDirectory()) {
    Serial.println("Failed to open /videos directory");
    return;
  }
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      sdVideoList.push_back(file.name());
      Serial.print("Found video directory: ");
      Serial.println(file.name());
    }
    file = root.openNextFile();
  }
  root.close();
  Serial.printf("Found %d videos on SD card.\n", sdVideoList.size());
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (x >= tft.width() || y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

void drawJPEGFrame(const VideoInfo* video, uint16_t frameIndex) {
  const uint8_t* jpg_data = (const uint8_t*)pgm_read_ptr(&video->frames[frameIndex]);
  uint16_t jpg_size = pgm_read_word(&video->frames_size[frameIndex]);
  TJpgDec.drawJpg(0, 0, jpg_data, jpg_size);
}

// =======================================================================================
// --- HÀM drawMenu() ĐÃ ĐƯỢC CẬP NHẬT ---
// =======================================================================================
void drawMenu() {
  screenSprite.fillSprite(TFT_BLACK);

  // --- Các hằng số cho giao diện ---
  const int paddingX = 10;
  const int itemHeight = 28;
  const int cornerRadius = 5;
  const int tabHeight = 30;
  const int tabWidth = tft.width() / 2;

  // --- Vẽ các Tab (Setting/Mode) ---
  if (currentTab == TAB_SETTING) {
    screenSprite.fillRoundRect(0, 0, tabWidth, tabHeight, cornerRadius, TFT_BLUE);
    screenSprite.drawRoundRect(tabWidth, 0, tabWidth, tabHeight, cornerRadius, TFT_WHITE);
  } else {
    screenSprite.drawRoundRect(0, 0, tabWidth, tabHeight, cornerRadius, TFT_WHITE);
    screenSprite.fillRoundRect(tabWidth, 0, tabWidth, tabHeight, cornerRadius, TFT_BLUE);
  }
  myfont.print((tabWidth - myfont.getLength(tabNames[0])) / 2, (tabHeight - 16) / 2, tabNames[0], TFT_WHITE, (currentTab == TAB_SETTING) ? TFT_BLUE : TFT_BLACK);
  myfont.print(tabWidth + (tabWidth - myfont.getLength(tabNames[1])) / 2, (tabHeight - 16) / 2, tabNames[1], TFT_WHITE, (currentTab == TAB_MODE) ? TFT_BLUE : TFT_BLACK);

  // --- Logic vẽ danh sách Menu ---
  String* currentMenuItems = (currentTab == TAB_SETTING) ? settingMenuItems : modeMenuItems;
  int numCurrentItems = (currentTab == TAB_SETTING) ? NUM_SETTING_ITEMS : NUM_MODE_ITEMS;
  
  // Xác định mục nào cần được tô sáng nền (khi đang ở chế độ chỉnh sửa)
  int itemToHighlight = -1;
  if (currentTab == TAB_SETTING) {
    if (currentMode == EDIT_SPEED) itemToHighlight = 0;
    if (currentMode == EDIT_ROTATION) itemToHighlight = 1;
    if (currentMode == EDIT_LANGUAGE) itemToHighlight = 2;
    if (currentMode == EDIT_SD) itemToHighlight = 3;
    if (currentMode == EDIT_NOTIF_TIME) itemToHighlight = 4;
  }

  // Vòng lặp chỉ vẽ các mục có thể nhìn thấy (dựa trên menuScrollOffset)
  int startItem = menuScrollOffset;
  int endItem = min(startItem + MAX_VISIBLE_ITEMS, numCurrentItems);

  for (int i = startItem; i < endItem; i++) {
    int displayIndex = i - menuScrollOffset; // Vị trí tương đối trên màn hình (0, 1, 2...)
    int currentY = tabHeight + 10 + displayIndex * (itemHeight + 5);
    
    uint16_t textColor = TFT_WHITE;
    uint16_t bgColor = TFT_BLACK;

    // Vẽ khung trắng xung quanh mục đang được chọn
    if (i == selectedMenuItem && currentMode == MENU) {
      screenSprite.drawRoundRect(paddingX / 2, currentY - 4, tft.width() - paddingX - 15, itemHeight, cornerRadius, TFT_WHITE);
    }
    // Tô nền trắng cho mục đang được chỉnh sửa
    if (i == itemToHighlight) {
      screenSprite.fillRoundRect(paddingX / 2, currentY - 4, tft.width() - paddingX - 15, itemHeight, cornerRadius, TFT_WHITE);
      textColor = TFT_BLACK;
      bgColor = TFT_WHITE;
    }

    String title = currentMenuItems[i];
    myfont.print(paddingX + 5, currentY, title, textColor, bgColor);

    // Vẽ giá trị cho các mục cài đặt
    String valueStr = "";
    if (currentTab == TAB_SETTING) {
      if (i == 0) valueStr = String(tempFrameDelay);
      if (i == 1) {
        switch (tempRotation) {
          case 0: valueStr = "0 deg"; break;
          case 1: valueStr = "90 deg"; break;
          case 2: valueStr = "180 deg"; break;
          case 3: valueStr = "270 deg"; break;
        }
      }
      if (i == 2) valueStr = (tempLang == "vi") ? "VI" : "EN";
      if (i == 3) valueStr = tempUseSD ? "ON" : "OFF";
      if (i == 4) valueStr = String(tempNotificationTimeout) + "s";
    }

    if (valueStr.length() > 0) {
      int textW = myfont.getLength(valueStr);
      myfont.print(tft.width() - textW - paddingX - 20, currentY, valueStr, textColor, bgColor);
    }
  }

  // --- Vẽ chỉ báo cuộn (Scroll Indicators) ---
  if (numCurrentItems > MAX_VISIBLE_ITEMS) {
    // Mũi tên lên: chỉ hiển thị khi ta đã cuộn xuống
    if (menuScrollOffset > 0) {
      screenSprite.fillTriangle(
        tft.width() - 15, tabHeight + 10,
        tft.width() - 5, tabHeight + 10,
        tft.width() - 10, tabHeight + 5,
        TFT_WHITE
      );
    }
    // Mũi tên xuống: chỉ hiển thị khi vẫn còn mục ở bên dưới chưa hiển thị
    if (menuScrollOffset + MAX_VISIBLE_ITEMS < numCurrentItems) {
      screenSprite.fillTriangle(
        tft.width() - 15, tft.height() - 10,
        tft.width() - 5, tft.height() - 10,
        tft.width() - 10, tft.height() - 5,
        TFT_WHITE
      );
    }
  }
  
  screenSprite.pushSprite(0, 0);
}


ButtonAction getButtonAction() {
  static int lastState = HIGH;
  static int currentState;
  static unsigned long lastDebounceTime = 0;
  static unsigned long debounceDelay = 50;

  static int clickCount = 0;
  static unsigned long lastClickTime = 0;
  static unsigned long multiClickWindow = 400;
  static unsigned long longPressTime = 1000;
  static unsigned long pressTime = 0;

  ButtonAction action = ACTION_NONE;
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentState) {
      currentState = reading;
      if (currentState == LOW) {
        Serial.println("Button Pressed");
        clickCount++;
        pressTime = millis();
      } else {
        Serial.println("Button Released");
        lastClickTime = millis();
      }
    }
  }

  if (currentState == LOW && (millis() - pressTime > longPressTime)) {
    if (clickCount > 0) {
      Serial.println("-> Event: LONG PRESS");
      action = ACTION_LONG;
      clickCount = 0;
    }
  }

  if (clickCount > 0 && currentState == HIGH && (millis() - lastClickTime > multiClickWindow)) {
    if (clickCount == 1) {
      Serial.println("-> Event: SINGLE CLICK");
      action = ACTION_SINGLE;
    }
    if (clickCount == 2) {
      Serial.println("-> Event: DOUBLE CLICK");
      action = ACTION_DOUBLE;
    }
    if (clickCount == 3) {
      Serial.println("-> Event: TRIPLE CLICK");
      action = ACTION_TRIPLE;
    }
    clickCount = 0;
  }

  lastState = reading;
  return action;
}

void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "reset_config") {
      Serial.println("Received command: reset_config");
      Serial.println("Resetting config file to default...");

      frameDelay = 20;
      currentRotation = 3;
      currentLang = "vi";
      useSD = false;
      notificationTimeout = 5;

      saveSettings();
      Serial.println("Config reset. Restarting device...");
      delay(1000);
      ESP.restart();
    }
  }
}

void drawWatchFace() {
  getTimeFromRTC();
  screenSprite.fillSprite(TFT_BLACK);

  if (timeIsSynced) {
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", rtc_hour, rtc_minute);
    myfont.print((tft.width() - myfont.getLength(timeStr)) / 2, tft.height() / 2 - 10, timeStr, TFT_WHITE, TFT_BLACK);
    myfont.print((tft.width() - myfont.getLength(timeStr)) / 2 + 1, tft.height() / 2 - 10, timeStr, TFT_WHITE, TFT_BLACK);
  } else {
    myfont.print((tft.width() - myfont.getLength("--:--")) / 2, tft.height() / 2 - 10, "--:--", TFT_WHITE, TFT_BLACK);
  }

  screenSprite.pushSprite(0, 0);
}

// =======================================================================================
// --- SETUP & LOOP ---
// =======================================================================================

void setup() {
  Serial.begin(115200);
  loadSettings();
  tft.begin();
  tft.setRotation(currentRotation);
  tft.fillScreen(TFT_BLACK);
  screenSprite.createSprite(tft.width(), tft.height());

  if (useSD) {
    if (SD.begin(SD_CS_PIN)) {
      sdCardOk = true;
      Serial.println("SD Card initialized successfully.");
      scanSdVideos();
    } else {
      sdCardOk = false;
      Serial.println("SD Card initialization failed!");
    }
  }

  myfont.set_font(Fira_Code_16);
  Flappy::begin(&screenSprite);
  CarGame::begin(&screenSprite);

  hourHandSprite.setColorDepth(8);
  hourHandSprite.createSprite(HOUR_HAND_WIDTH, HOUR_HAND_HEIGHT);
  hourHandSprite.setPivot(HOUR_PIVOT_X, HOUR_PIVOT_Y);
  hourHandSprite.pushImage(0, 0, HOUR_HAND_WIDTH, HOUR_HAND_HEIGHT, hourHandImage);

  minuteHandSprite.setColorDepth(8);
  minuteHandSprite.createSprite(MINUTE_HAND_WIDTH, MINUTE_HAND_HEIGHT);
  minuteHandSprite.setPivot(MINUTE_PIVOT_X, MINUTE_PIVOT_Y);
  minuteHandSprite.pushImage(0, 0, MINUTE_HAND_WIDTH, MINUTE_HAND_HEIGHT, minuteHandImage);

  secondHandSprite.setColorDepth(8);
  secondHandSprite.createSprite(SECOND_HAND_WIDTH, SECOND_HAND_HEIGHT);
  secondHandSprite.setPivot(SECOND_PIVOT_X, SECOND_PIVOT_Y);
  secondHandSprite.pushImage(0, 0, SECOND_HAND_WIDTH, SECOND_HAND_HEIGHT, secondHandImage);

  Chronos.setConnectionCallback(connectionCallback);
  Chronos.setNotificationCallback(notificationCallback);
  Chronos.setConfigurationCallback(configCallback);
  Chronos.setRingerCallback(ringerCallback);
  Chronos.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
  Serial.println("Setup done!");
}

void loop() {
  handleSerialCommands();
  Chronos.loop();
  ButtonAction action = getButtonAction();

  switch (currentMode) {
    case PLAYING:
      {
        if (action != ACTION_NONE) {
          if (action == ACTION_SINGLE) {
            currentVideoIndex = (currentVideoIndex + 1) % NUM_FLASH_VIDEOS;
            currentFrame = 0;
          } else if (action == ACTION_DOUBLE) {
            currentVideoIndex = VIDEO_JUMP_TARGET;
            currentFrame = 0;
          } else if (action == ACTION_TRIPLE) {
            currentMode = MENU;
            currentTab = TAB_SETTING;
            selectedMenuItem = 0;
            menuScrollOffset = 0; // Reset cuộn khi vào menu
            tempFrameDelay = frameDelay;
            tempRotation = currentRotation;
            tempLang = currentLang;
            tempUseSD = useSD;
            tempNotificationTimeout = notificationTimeout;
            drawMenu();
          }
          break;
        }

        if (useSD && sdCardOk) {
          if (sdVideoList.empty()) break;
          if (currentVideoIndex >= sdVideoList.size()) currentVideoIndex = 0;
          String videoPath = "/videos/" + sdVideoList[currentVideoIndex] + "/" + String(currentFrame) + ".jpg";
          if (SD.exists(videoPath)) {
            TJpgDec.drawSdJpg(0, 0, videoPath);
            delay(frameDelay);
            currentFrame++;
          } else {
            currentFrame = 0;
            currentVideoIndex = (currentVideoIndex + 1) % sdVideoList.size();
          }
        } else {
          if (currentVideoIndex >= NUM_FLASH_VIDEOS) currentVideoIndex = 0;
          VideoInfo* currentVideo = flashVideoList[currentVideoIndex];
          drawJPEGFrame(currentVideo, currentFrame);
          delay(frameDelay);
          currentFrame++;
          if (currentFrame >= currentVideo->num_frames) {
            currentFrame = 0;
            currentVideoIndex = (currentVideoIndex + 1) % NUM_FLASH_VIDEOS;
          }
        }
        break;
      }
    // =======================================================================================
    // --- CASE MENU ĐÃ ĐƯỢC CẬP NHẬT ---
    // =======================================================================================
    case MENU:
      {
        bool needsRedraw = false;
        if (action == ACTION_DOUBLE) { // Chuyển Tab
            currentTab = (currentTab == TAB_SETTING) ? TAB_MODE : TAB_SETTING;
            selectedMenuItem = 0;
            menuScrollOffset = 0; // Reset vị trí cuộn khi chuyển tab
            needsRedraw = true;
        } else if (action == ACTION_SINGLE) { // Di chuyển xuống trong menu
            int maxItems = (currentTab == TAB_SETTING) ? NUM_SETTING_ITEMS : NUM_MODE_ITEMS;
            selectedMenuItem = (selectedMenuItem + 1) % maxItems;

            // === LOGIC CUỘN MỚI ===
            // Nếu mục chọn quay về đầu danh sách, reset thanh cuộn
            if (selectedMenuItem == 0) {
                menuScrollOffset = 0;
            } 
            // Nếu mục chọn đi ra khỏi cạnh dưới của vùng hiển thị, cuộn xuống
            else if (selectedMenuItem >= menuScrollOffset + MAX_VISIBLE_ITEMS) {
                menuScrollOffset = selectedMenuItem - MAX_VISIBLE_ITEMS + 1;
            }
            // Nếu mục chọn nằm trên vùng hiển thị (xảy ra khi cuộn lên), cuộn lên
            else if (selectedMenuItem < menuScrollOffset) {
                menuScrollOffset = selectedMenuItem;
            }
            // =======================
            
            needsRedraw = true;
        } else if (action == ACTION_LONG) { // Chọn một mục
            if (currentTab == TAB_SETTING) {
                switch (selectedMenuItem) {
                    case 0: currentMode = EDIT_SPEED; needsRedraw = true; break;
                    case 1: currentMode = EDIT_ROTATION; needsRedraw = true; break;
                    case 2: currentMode = EDIT_LANGUAGE; needsRedraw = true; break;
                    case 3: currentMode = EDIT_SD; needsRedraw = true; break;
                    case 4: currentMode = EDIT_NOTIF_TIME; needsRedraw = true; break;
                    case 5: // Save
                        frameDelay = tempFrameDelay;
                        currentRotation = tempRotation;
                        currentLang = tempLang;
                        useSD = tempUseSD;
                        notificationTimeout = tempNotificationTimeout;
                        tft.setRotation(currentRotation);
                        saveSettings();
                        loadSettings(); // Tải lại để cập nhật ngôn ngữ menu
                        needsRedraw = true; 
                        // ESP.restart(); // Có thể không cần restart ngay
                        break;
                    case 6: // Exit
                        currentMode = PLAYING;
                        currentFrame = 0;
                        tft.fillScreen(TFT_BLACK);
                        break;
                }
            } else {  // currentTab == TAB_MODE
                switch (selectedMenuItem) {
                    case 0: currentMode = GAME_FLAPPY; Flappy::start(); break;
                    case 1: currentMode = GAME_CAR; CarGame::start(); break;
                    case 2: currentMode = WATCH_MODE; tft.fillScreen(TFT_BLACK); break;
                    case 3: currentMode = ANALOG_WATCH_MODE; tft.fillScreen(TFT_BLACK); break;
                    case 4: // Exit
                        currentMode = PLAYING;
                        currentFrame = 0;
                        tft.fillScreen(TFT_BLACK);
                        break;
                }
            }
        }

        if (needsRedraw) {
            drawMenu();
        }
        break;
      }
    case EDIT_SPEED:
      {
        if (action == ACTION_SINGLE) {
          tempFrameDelay += 5;
          if (tempFrameDelay > 50) tempFrameDelay = 0;
          drawMenu();
        } else if (action == ACTION_LONG) {
          currentMode = MENU;
          drawMenu();
        }
        break;
      }
    case EDIT_ROTATION:
      {
        if (action == ACTION_SINGLE) {
          tempRotation = (tempRotation + 1) % 4;
          drawMenu();
        } else if (action == ACTION_LONG) {
          currentMode = MENU;
          drawMenu();
        }
        break;
      }
    case EDIT_LANGUAGE:
      {
        if (action == ACTION_SINGLE) {
          tempLang = (tempLang == "vi") ? "en" : "vi";
          drawMenu();
        } else if (action == ACTION_LONG) {
          currentMode = MENU;
          drawMenu();
        }
        break;
      }
    case EDIT_SD:
      {
        if (action == ACTION_SINGLE) {
          tempUseSD = !tempUseSD;
          drawMenu();
        } else if (action == ACTION_LONG) {
          currentMode = MENU;
          drawMenu();
        }
        break;
      }
    case EDIT_NOTIF_TIME:
      {
        if (action == ACTION_SINGLE) {
          tempNotificationTimeout++;
          if (tempNotificationTimeout > 10) tempNotificationTimeout = 3;
          drawMenu();
        } else if (action == ACTION_LONG) {
          currentMode = MENU;
          drawMenu();
        }
        break;
      }
    case GAME_FLAPPY:
      {
        if (action == ACTION_DOUBLE) Flappy::togglePause();
        if (action == ACTION_TRIPLE) {
          Flappy::stop();
          currentMode = MENU;
          drawMenu();
          break;
        }

        static unsigned long lastFlapTime = 0;
        if (digitalRead(BUTTON_PIN) == LOW && Flappy::isRunning() && !Flappy::isPaused()) {
          if (millis() - lastFlapTime > 120) {
            Flappy::flap();
            lastFlapTime = millis();
          }
        }

        if (action == ACTION_SINGLE && !Flappy::isRunning()) {
          Flappy::start();
        }

        Flappy::tick();
        screenSprite.pushSprite(0, 0);

        break;
      }

    case GAME_CAR: {
      if (action == ACTION_SINGLE) CarGame::moveRight();
      if (action == ACTION_DOUBLE) CarGame::moveLeft();
      if (action == ACTION_TRIPLE) CarGame::togglePause();
      if (action == ACTION_LONG) { 
        CarGame::stop(); 
        currentMode = MENU; 
        drawMenu(); 
        break; 
      }
      if (action == ACTION_SINGLE && !CarGame::isRunning()) {
        CarGame::start();
      }
      
      CarGame::tick();
      screenSprite.pushSprite(0, 0);
      
      break;
    }
      
    case WATCH_MODE:
      {
        if (action == ACTION_LONG) {
          currentMode = MENU;
          drawMenu();
          hasNewNotification = false;
          hasNewNavigation = false;
          isRinging = false;
          break;
        }
        drawWatchFace();
        delay(1000);
        break;
      }
    case ANALOG_WATCH_MODE:
      {
        if (action == ACTION_LONG) {
          currentMode = MENU;
          drawMenu();
          break;
        }
        drawAnalogWatchFace();
        delay(1000);
        break;
      }
  }
}
