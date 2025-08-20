#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include "SD.h"
#include <SPI.h>
#include <vector>
#include <time.h>
#include "FontMaker.h"

// --- INCLUDE CÁC MODULE QUẢN LÝ ---
#include "globals.h"
#include "menu_manager.h"
#include "flappy_game.h"
#include "car_game.h"

#include "background_image.h"
#include "hour_hand.h"
#include "minute_hand.h"
#include "second_hand.h"
#include "chronos_manager.h"

// =======================================================================================
// --- CẤU HÌNH ---
// =======================================================================================
#define BUTTON_PIN 0
#define VIDEO_JUMP_TARGET 2
#define CONFIG_FILE "/config.json"
#define SD_CS_PIN 7

// --- Định nghĩa Struct Video ---
typedef struct _VideoInfo {
  const uint8_t* const* frames;
  const uint16_t* frames_size;
  uint16_t num_frames;
} VideoInfo;

// --- Khai báo các file video ---
#include "video01.h"
#include "video02.h"
#include "video03.h"
#include "video04.h"

// =======================================================================================
// --- KHỞI TẠO BIẾN TOÀN CỤC ---
// =======================================================================================
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite screenSprite = TFT_eSprite(&tft);
TFT_eSprite hourHandSprite = TFT_eSprite(&tft);
TFT_eSprite minuteHandSprite = TFT_eSprite(&tft);
TFT_eSprite secondHandSprite = TFT_eSprite(&tft);

// --- Forward declaration for the pixel drawing function ---
void setSpritePixel(int16_t x, int16_t y, uint16_t color);

MakeFont myfont(&setSpritePixel);

// --- Biến trạng thái ứng dụng ---
Mode currentMode = PLAYING;
AppSettings settings;

// --- Biến cho Video ---
VideoInfo* flashVideoList[] = { &video01, &video02, &video03, &video04 };
const uint8_t NUM_FLASH_VIDEOS = sizeof(flashVideoList) / sizeof(flashVideoList[0]);
std::vector<String> sdVideoList;
bool sdCardOk = false;
uint8_t currentVideoIndex = 0;
uint16_t currentFrame = 0;

// =======================================================================================
// --- CÁC HÀM TIỆN ÍCH (HELPER FUNCTIONS) ---
// =======================================================================================

void saveSettings();  // Forward declaration
void loadSettings();  // Forward declaration

void setSpritePixel(int16_t x, int16_t y, uint16_t color) {
  screenSprite.drawPixel(x, y, color);
}

void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "reset_config") {
      Serial.println("Received command: reset_config");
      Serial.println("Removing config file...");

      if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
      }

      if (SPIFFS.exists(CONFIG_FILE)) {
        SPIFFS.remove(CONFIG_FILE);
        Serial.println("Config file removed.");
      } else {
        Serial.println("Config file not found, nothing to remove.");
      }

      Serial.println("Restarting to generate new default config...");
      delay(1000);
      ESP.restart();
    }
  }
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

  if (clickCount > 0 && currentState == HIGH && (millis() - lastClickTime > multiClickWindow))
  {
    if (clickCount == 1)
    {
      Serial.println("-> Event: SINGLE CLICK");
      action = ACTION_SINGLE;
    }
    if (clickCount == 2)
    {
      Serial.println("-> Event: DOUBLE CLICK");
      action = ACTION_DOUBLE;
    }
    if (clickCount == 3)
    {
      Serial.println("-> Event: TRIPLE CLICK");
      action = ACTION_TRIPLE;
    }
    clickCount = 0;
  }

  lastState = reading;
  return action;
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

void drawAnalogWatchFace() {
  screenSprite.pushImage(0, 0, 240, 240, backgroundImage);
  if (chronos_is_time_synced()) {
    char dateStr[10];
    sprintf(dateStr, "%02d/%02d", chronos_get_day(), chronos_get_month());
    myfont.print(30, tft.height() / 2 - 10, dateStr, TFT_WHITE, TFT_BLACK);
    float sec_angle = chronos_get_second() * 6;
    float min_angle = chronos_get_minute() * 6 + chronos_get_second() * 0.1;
    float hour_angle = (chronos_get_hour() % 12) * 30 + chronos_get_minute() * 0.5;
    hourHandSprite.pushRotated(&screenSprite, hour_angle, TFT_BLACK);
    minuteHandSprite.pushRotated(&screenSprite, min_angle, TFT_BLACK);
    secondHandSprite.pushRotated(&screenSprite, sec_angle, TFT_BLACK);
    screenSprite.fillCircle(120, 120, 4, TFT_RED);
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

  tft.begin();
  screenSprite.createSprite(tft.width(), tft.height());
  myfont.set_font(Fira_Code_16);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  menu_init(&tft, &screenSprite, &myfont, &settings);
  chronos_init(&tft, &screenSprite, &myfont, &settings);
  Flappy::begin(&screenSprite);
  CarGame::begin(&screenSprite);

  loadSettings();

  tft.setRotation(settings.currentRotation);
  tft.fillScreen(TFT_BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  hourHandSprite.createSprite(HOUR_HAND_WIDTH, HOUR_HAND_HEIGHT);
  hourHandSprite.setPivot(HOUR_PIVOT_X, HOUR_PIVOT_Y);
  hourHandSprite.pushImage(0, 0, HOUR_HAND_WIDTH, HOUR_HAND_HEIGHT, hourHandImage);
  minuteHandSprite.createSprite(MINUTE_HAND_WIDTH, MINUTE_HAND_HEIGHT);
  minuteHandSprite.setPivot(MINUTE_PIVOT_X, MINUTE_PIVOT_Y);
  minuteHandSprite.pushImage(0, 0, MINUTE_HAND_WIDTH, MINUTE_HAND_HEIGHT, minuteHandImage);
  secondHandSprite.createSprite(SECOND_HAND_WIDTH, SECOND_HAND_HEIGHT);
  secondHandSprite.setPivot(SECOND_PIVOT_X, SECOND_PIVOT_Y);
  secondHandSprite.pushImage(0, 0, SECOND_HAND_WIDTH, SECOND_HAND_HEIGHT, secondHandImage);

  Serial.println("Setup done!");
}

void loop() {
  handleSerialCommands();
  chronos_loop();
  ButtonAction action = getButtonAction();

  switch (currentMode) {
    case PLAYING:
      {
        if (action == ACTION_TRIPLE) {
          currentMode = MENU;
          menu_enter();
          menu_draw();
          break;
        }
        if (action == ACTION_SINGLE) {
          currentVideoIndex = (currentVideoIndex + 1) % NUM_FLASH_VIDEOS;
          currentFrame = 0;
        } else if (action == ACTION_DOUBLE) {
          currentVideoIndex = VIDEO_JUMP_TARGET;
          currentFrame = 0;
        }

        if (currentVideoIndex >= NUM_FLASH_VIDEOS) currentVideoIndex = 0;
        VideoInfo* currentVideo = flashVideoList[currentVideoIndex];
        drawJPEGFrame(currentVideo, currentFrame);
        delay(settings.frameDelay);
        currentFrame = (currentFrame + 1) % currentVideo->num_frames;
        break;
      }

    case MENU:
      {
        Mode newMode = menu_handle_action(action);
        if (newMode != MENU) {

          if (menu_manager_save_triggered()) {
            saveSettings();
            loadSettings();
            tft.setRotation(settings.currentRotation);
          }

          currentMode = newMode;
          tft.fillScreen(TFT_BLACK);

          if (currentMode == GAME_FLAPPY) Flappy::start();
          if (currentMode == GAME_CAR) CarGame::start();
        }
        break;
      }

    // *** ĐÃ CẬP NHẬT LẠI ĐẦY ĐỦ LOGIC ĐIỀU KHIỂN GAME ***
    case GAME_FLAPPY:
      {
        if (action == ACTION_DOUBLE) Flappy::togglePause();
        if (action == ACTION_TRIPLE) {
          Flappy::stop();
          currentMode = MENU;
          menu_enter();  // Quay lại menu và giữ nguyên trạng thái
          menu_draw();
          break;
        }

        static unsigned long lastFlapTime = 0;
        if (digitalRead(BUTTON_PIN) == LOW && Flappy::isRunning() && !Flappy::isPaused()) {
          if (millis() - lastFlapTime > 120) {  // Debounce cho nút nhấn giữ
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

    // *** ĐÃ CẬP NHẬT LẠI ĐẦY ĐỦ LOGIC ĐIỀU KHIỂN GAME ***
    case GAME_CAR:
      {
        if (action == ACTION_SINGLE) CarGame::moveRight();
        if (action == ACTION_DOUBLE) CarGame::moveLeft();
        if (action == ACTION_TRIPLE) CarGame::togglePause();
        if (action == ACTION_LONG) {
          CarGame::stop();
          currentMode = MENU;
          menu_enter();  // Quay lại menu và giữ nguyên trạng thái
          menu_draw();
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
          menu_enter();
          menu_draw();
          break;
        }
        chronos_draw_watch_face();
        delay(1000);
        break;
      }

    case ANALOG_WATCH_MODE:
      {
        if (action == ACTION_LONG) {
          currentMode = MENU;
          menu_enter();
          menu_draw();
          break;
        }
        drawAnalogWatchFace();
        delay(1000);
        break;
      }
  }
}

// =======================================================================================
// --- HÀM LOAD/SAVE SETTINGS ---
// =======================================================================================
void saveSettings() {
  Serial.println("Saving settings to SPIFFS...");
  File configFile = SPIFFS.open(CONFIG_FILE, "w");
  if (!configFile) return;

  StaticJsonDocument<1024> doc;
  doc["frameDelay"] = settings.frameDelay;
  doc["currentRotation"] = settings.currentRotation;
  doc["language"] = settings.currentLang;
  doc["useSD"] = settings.useSD;
  doc["notificationTimeout"] = settings.notificationTimeout;

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

  serializeJson(doc, configFile);
  configFile.close();
}

void loadSettings() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed.");
    return;
  }

  bool success = false;
  File configFile = SPIFFS.open(CONFIG_FILE, "r");
  if (configFile) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    if (!error) {
      settings.frameDelay = doc["frameDelay"] | 20;
      settings.currentRotation = doc["currentRotation"] | 3;
      settings.currentLang = doc["language"] | "vi";
      settings.useSD = doc["useSD"] | false;
      settings.notificationTimeout = doc["notificationTimeout"] | 5;
      menu_load_strings(doc.as<JsonObject>());
      success = true;
    }
    configFile.close();
  }

  if (!success) {
    Serial.println("Config not loaded or invalid. Creating default.");
    settings.frameDelay = 20;
    settings.currentRotation = 3;
    settings.currentLang = "vi";
    settings.useSD = false;
    settings.notificationTimeout = 5;

    StaticJsonDocument<1024> default_doc;
    JsonObject menu_vi = default_doc.createNestedObject("menu_vi");
    menu_vi["tab_setting"] = "Cài đặt";
    menu_vi["tab_mode"] = "Chế độ";
    JsonObject setting_vi = menu_vi.createNestedObject("setting");
    setting_vi["item0"] = "Tốc độ video";
    setting_vi["item1"] = "Xoay màn hình test cho day để coi chạy được không";
    setting_vi["item2"] = "Ngôn ngữ";
    setting_vi["item3"] = "Thẻ SD";
    setting_vi["item4"] = "TG Thông Báo";
    setting_vi["item5"] = "Lưu";
    setting_vi["item6"] = "Thoát";
    JsonObject mode_vi = menu_vi.createNestedObject("mode");
    mode_vi["item0"] = "Chơi Flappy test cho day để coi chạy được không";
    mode_vi["item1"] = "Chơi Đua Xe";
    mode_vi["item2"] = "Đồng hồ số";
    mode_vi["item3"] = "Đồng hồ kim";
    mode_vi["item4"] = "Thoát";

    JsonObject menu_en = default_doc.createNestedObject("menu_en");
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

    menu_load_strings(default_doc.as<JsonObject>());

    saveSettings();
  }
}
