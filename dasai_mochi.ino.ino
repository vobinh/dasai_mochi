#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include "SD.h"
#include <SPI.h>
#include <vector>
#include <time.h>
#include "FontMaker.h"

#include "globals.h"
#include "menu_manager.h"
#include "button_manager.h"
#include "flappy_game.h"
#include "car_game.h"
#include "hour_hand.h"
#include "minute_hand.h"
#include "second_hand.h"
#include "chronos_manager.h"
#include "ui_effects.h"
#include "analog_face.h"
#include "DigitaltsLime35pt7b.h"

// --- CẤU HÌNH ---
#define VIDEO_JUMP_TARGET 2
#define CONFIG_FILE "/config.json"
#define SD_CS_PIN 7

// --- CÁC BIẾN TOÀN CỤC ---
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite screenSprite = TFT_eSprite(&tft);
TFT_eSprite hourHandSprite = TFT_eSprite(&tft);
TFT_eSprite minuteHandSprite = TFT_eSprite(&tft);
TFT_eSprite secondHandSprite = TFT_eSprite(&tft);

void setSpritePixel(int16_t x, int16_t y, uint16_t color);
MakeFont myfont(&setSpritePixel);

Mode currentMode = PLAYING;
AppSettings settings;
Mode modeBeforeAlert = PLAYING;
bool isDisplayingAlert = false;

// --- BIẾN CHO VIDEO & MẶT ĐỒNG HỒ ---
typedef struct _VideoInfo
{
  const uint8_t *const *frames;
  const uint16_t *frames_size;
  uint16_t num_frames;
} VideoInfo;
#include "video01.h"
#include "video02.h"
#include "video03.h"
#include "video04.h"
VideoInfo *flashVideoList[] = {&video01, &video02, &video03, &video04};
const uint8_t NUM_FLASH_VIDEOS = sizeof(flashVideoList) / sizeof(flashVideoList[0]);
uint8_t currentVideoIndex = 0;
uint16_t currentFrame = 0;

VideoInfo analogFaces = {analog_face_frames, analog_face_frames_size, analog_face_num_frames};

// --- KHAI BÁO HÀM ---
void saveSettings();
void loadSettings();
void drawDigitalWatchFace();
void drawAnalogWatchFace();
void drawWeatherScreen(); // *** KHAI BÁO HÀM MỚI ***

// =======================================================================================
// --- CÁC HÀM TIỆN ÍCH VÀ CALLBACK CHO VIỆC VẼ ---
// =======================================================================================

// Con trỏ toàn cục để trỏ đến sprite mục tiêu khi vẽ JPEG
TFT_eSprite *jpegSpriteTarget = nullptr;

// Callback để vẽ JPEG trực tiếp lên màn hình (cho video)
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  if (x >= tft.width() || y >= tft.height())
    return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

void drawJPEGFrame(const VideoInfo *video, uint16_t frameIndex)
{
  const uint8_t *jpg_data = (const uint8_t *)pgm_read_ptr(&video->frames[frameIndex]);
  uint16_t jpg_size = pgm_read_word(&video->frames_size[frameIndex]);
  TJpgDec.drawJpg(0, 0, jpg_data, jpg_size);
}

// Callback mới để vẽ JPEG lên một sprite (cho mặt đồng hồ)
bool sprite_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  if (!jpegSpriteTarget)
    return false; // An toàn nếu con trỏ chưa được thiết lập
  jpegSpriteTarget->pushImage(x, y, w, h, bitmap);
  return true;
}

void setSpritePixel(int16_t x, int16_t y, uint16_t color)
{
  screenSprite.drawPixel(x, y, color);
}

void handleSerialCommands()
{
  if (Serial.available() > 0)
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "reset_config")
    {
      Serial.println("Received command: reset_config");
      if (!SPIFFS.begin(true))
      {
        return;
      }
      if (SPIFFS.exists(CONFIG_FILE))
      {
        SPIFFS.remove(CONFIG_FILE);
      }
      Serial.println("Restarting...");
      delay(1000);
      ESP.restart();
    }
  }
}

// =======================================================================================
// --- CÁC HÀM VẼ MẶT ĐỒNG HỒ ---
// =======================================================================================
void drawDigitalWatchFace()
{
  drawMatrixRainBackground(&tft, &screenSprite);

  if (!chronos_is_time_synced())
  {
    String msg = "Dang ket noi...";
    myfont.print((tft.width() - myfont.getLength(msg)) / 2, tft.height() / 2, msg, TFT_YELLOW, TFT_BLACK);
  }
  else
  {
    screenSprite.setFreeFont(&DigitaltsLime35pt7b);
    char timeStr[9];
    sprintf(timeStr, "%02d:%02d:%02d", chronos_get_hour(), chronos_get_minute(), chronos_get_second());
    int textWidth = screenSprite.textWidth(timeStr);
    int x_pos = (tft.width() - textWidth) / 2;
    int y_pos = (tft.height() - 50) / 2;
    screenSprite.setTextColor(TFT_CYAN, TFT_BLACK);
    screenSprite.drawString(timeStr, x_pos, y_pos);
    screenSprite.setFreeFont(NULL);

    char dateStr[11];
    sprintf(dateStr, "%02d/%02d/%d", chronos_get_day(), chronos_get_month(), chronos_get_year());
    myfont.print((tft.width() - myfont.getLength(dateStr)) / 2, y_pos + 50 + 10, dateStr, TFT_WHITE, TFT_BLACK);
  }
  screenSprite.pushSprite(0, 0);
}

// *** HÀM ĐÃ ĐƯỢC SỬA LỖI HOÀN TOÀN ***
void drawAnalogWatchFace()
{
  if (analogFaces.num_frames == 0 || settings.currentAnalogFaceIndex >= analogFaces.num_frames)
  {
    screenSprite.fillSprite(TFT_BLACK);
    myfont.print(10, 10, "No analog faces", TFT_RED, TFT_BLACK);
    screenSprite.pushSprite(0, 0);
    return;
  }

  // 1. Thiết lập để TJpgDec vẽ vào sprite của chúng ta
  jpegSpriteTarget = &screenSprite;
  TJpgDec.setCallback(sprite_output);

  // 2. Lấy dữ liệu và vẽ hình nền JPEG trực tiếp lên sprite
  const uint8_t *jpg_data = (const uint8_t *)pgm_read_ptr(&analogFaces.frames[settings.currentAnalogFaceIndex]);
  uint16_t jpg_size = pgm_read_word(&analogFaces.frames_size[settings.currentAnalogFaceIndex]);
  TJpgDec.drawJpg(0, 0, jpg_data, jpg_size);

  // 3. QUAN TRỌNG: Trả callback về mặc định để không làm hỏng chức năng video
  TJpgDec.setCallback(tft_output);

  // 4. Vẽ kim đồng hồ và các thông tin khác LÊN TRÊN hình nền đã có trong sprite
  if (chronos_is_time_synced())
  {
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
  }
  else
  {
    myfont.print((tft.width() - myfont.getLength("--:--")) / 2, tft.height() / 2 - 10, "--:--", TFT_WHITE, TFT_BLACK);
  }

  // 5. Đẩy sprite đã hoàn chỉnh ra màn hình
  screenSprite.pushSprite(0, 0);
}

// *** HÀM MỚI ĐỂ VẼ MÀN HÌNH THỜI TIẾT ***
void drawWeatherScreen()
{
  screenSprite.fillSprite(TFT_BLACK);
  if (!chronos_has_weather_data())
  {
    myfont.print(10, 10, "Khong co du lieu thoi tiet", TFT_YELLOW, TFT_BLACK);
  }
  else
  {
    WeatherData weather = chronos_get_weather();

    // Tên thành phố
    myfont.print((tft.width() - myfont.getLength(weather.city)) / 2, 20, weather.city, TFT_WHITE, TFT_BLACK);

    // Nhiệt độ hiện tại (font lớn)
    screenSprite.setFreeFont(&DigitaltsLime35pt7b);
    String tempStr = String(weather.currentTemp) + "C";
    int textWidth = screenSprite.textWidth(tempStr);
    int x_pos = (tft.width() - textWidth) / 2;
    screenSprite.setTextColor(TFT_ORANGE, TFT_BLACK);
    screenSprite.drawString(tempStr, x_pos, 60);
    screenSprite.setFreeFont(NULL);

    // Nhiệt độ cao/thấp
    String highLowStr = "H:" + String(weather.highTemp) + " L:" + String(weather.lowTemp);
    myfont.print((tft.width() - myfont.getLength(highLowStr)) / 2, 130, highLowStr, TFT_WHITE, TFT_BLACK);

    // Biểu tượng (dạng chữ)
    myfont.print((tft.width() - myfont.getLength(weather.icon)) / 2, 160, weather.icon, TFT_CYAN, TFT_BLACK);

    // Thông tin khác
    String infoStr = "UV: " + String(weather.uv) + " | Ap suat: " + String(weather.pressure);
    myfont.print((tft.width() - myfont.getLength(infoStr)) / 2, 200, infoStr, TFT_WHITE, TFT_BLACK);
  }
  screenSprite.pushSprite(0, 0);
}

// =======================================================================================
// --- SETUP & LOOP ---
// =======================================================================================
void setup()
{
  Serial.begin(115200);
  tft.begin();
  screenSprite.createSprite(tft.width(), tft.height());
  myfont.set_font(Fira_Code_16);

  button_init();

  menu_init(&tft, &screenSprite, &myfont, &settings);
  chronos_init(&tft, &screenSprite, &myfont, &settings);
  Flappy::begin(&screenSprite);
  CarGame::begin(&screenSprite);
  initMatrixRain(&tft);

  loadSettings();

  tft.setRotation(settings.currentRotation);
  tft.fillScreen(TFT_BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output); // Thiết lập callback mặc định

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

void loop()
{
  handleSerialCommands();
  chronos_loop();
  ButtonAction action = getButtonAction();

  bool isAlertEvent = chronos_is_ringing() || chronos_has_new_notification() || chronos_has_new_navigation();

  if (isAlertEvent && !isDisplayingAlert)
  {
    if (currentMode != MENU)
    {
      modeBeforeAlert = currentMode;
      if (currentMode != WATCH_MODE && currentMode != ANALOG_WATCH_MODE)
      {
        currentMode = WATCH_MODE;
      }
      isDisplayingAlert = true;
    }
  }

  switch (currentMode)
  {
  case PLAYING:
  {
    if (action == ACTION_TRIPLE)
    {
      currentMode = MENU;
      menu_enter();
      break;
    }
    if (action == ACTION_SINGLE)
    {
      currentVideoIndex = (currentVideoIndex + 1) % NUM_FLASH_VIDEOS;
      currentFrame = 0;
    }
    else if (action == ACTION_DOUBLE)
    {
      currentVideoIndex = VIDEO_JUMP_TARGET;
      currentFrame = 0;
    }
    if (currentVideoIndex >= NUM_FLASH_VIDEOS) currentVideoIndex = 0;
    VideoInfo *currentVideo = flashVideoList[currentVideoIndex];
    TJpgDec.setCallback(tft_output);
    drawJPEGFrame(currentVideo, currentFrame);

    // Vẽ video trực tiếp lên màn hình
    // const uint8_t *jpg_data = (const uint8_t *)pgm_read_ptr(&currentVideo->frames[currentFrame]);
    // uint16_t jpg_size = pgm_read_word(&currentVideo->frames_size[currentFrame]);
    // TJpgDec.drawJpg(0, 0, jpg_data, jpg_size);

    delay(settings.frameDelay);
    currentFrame++;
    if (currentFrame >= currentVideo->num_frames) {
      currentFrame = 0;
      currentVideoIndex = (currentVideoIndex + 1) % NUM_FLASH_VIDEOS;
    }
    break;
  }

  case MENU:
  {
    Mode newMode = menu_handle_action(action);
    if (newMode != MENU)
    {
      if (menu_manager_save_triggered())
      {
        saveSettings();
        loadSettings();
        tft.setRotation(settings.currentRotation);
      }
      currentMode = newMode;
      tft.fillScreen(TFT_BLACK);
      if (currentMode == GAME_FLAPPY)
        Flappy::start();
      if (currentMode == GAME_CAR)
        CarGame::start();
    }
    else
    {
      menu_draw();
    }
    break;
  }

  case GAME_FLAPPY:
  {
    if (action == ACTION_TRIPLE)
    {
      Flappy::stop();
      currentMode = MENU;
      menu_enter();
      break;
    }

    static unsigned long lastFlapTime = 0;
    if (is_button_held() && Flappy::isRunning() && !Flappy::isPaused())
    {
      if (millis() - lastFlapTime > 120)
      {
        Flappy::flap();
        lastFlapTime = millis();
      }
    }

    if (action == ACTION_SINGLE && !Flappy::isRunning())
    {
      Flappy::start();
    }

    Flappy::tick();
    screenSprite.pushSprite(0, 0);
    break;
  }

  case GAME_CAR:
  {
    if (action == ACTION_LONG)
    {
      CarGame::stop();
      currentMode = MENU;
      menu_enter();
      break;
    }
    if (action == ACTION_SINGLE)
      CarGame::moveRight();
    if (action == ACTION_DOUBLE)
      CarGame::moveLeft();
    CarGame::tick();
    screenSprite.pushSprite(0, 0);
    break;
  }

  case WATCH_MODE:
  case ANALOG_WATCH_MODE:
  {
    static unsigned long lastAnalogUpdate = 0;

    if (currentMode == ANALOG_WATCH_MODE && action == ACTION_DOUBLE)
    {
      if (analogFaces.num_frames > 0)
      {
        settings.currentAnalogFaceIndex = (settings.currentAnalogFaceIndex + 1) % analogFaces.num_frames;
        saveSettings();
      }
    }

    if (action == ACTION_LONG)
    {
      isDisplayingAlert = false;
      currentMode = MENU;
      menu_enter();
      break;
    }

    bool alertDrawn = chronos_draw_alerts();

    if (!alertDrawn)
    {
      if (isDisplayingAlert)
      {
        isDisplayingAlert = false;
        currentMode = modeBeforeAlert;
        break;
      }

      if (currentMode == WATCH_MODE)
      {
        drawDigitalWatchFace();
      }
      else
      {
        if (millis() - lastAnalogUpdate > 1000)
        {
          lastAnalogUpdate = millis();
          drawAnalogWatchFace();
        }
      }
    }
    break;
  }

  // *** THÊM CASE MỚI CHO CHẾ ĐỘ THỜI TIẾT ***
  case WEATHER_MODE:
  {
    if (action == ACTION_LONG)
    {
      currentMode = MENU;
      menu_enter();
      break;
    }
    drawWeatherScreen();
    delay(1000); // Cập nhật mỗi giây
    break;
  }
  }
}

// --- HÀM LOAD/SAVE SETTINGS ---
void saveSettings()
{
  Serial.println("Saving settings to SPIFFS...");
  File configFile = SPIFFS.open(CONFIG_FILE, "w");
  if (!configFile)
    return;

  StaticJsonDocument<1024> doc;
  doc["frameDelay"] = settings.frameDelay;
  doc["currentRotation"] = settings.currentRotation;
  doc["language"] = settings.currentLang;
  doc["useSD"] = settings.useSD;
  doc["notificationTimeout"] = settings.notificationTimeout;
  doc["marqueeSpeed"] = settings.marqueeSpeed;
  doc["currentAnalogFaceIndex"] = settings.currentAnalogFaceIndex;

  JsonObject menu_vi = doc.createNestedObject("menu_vi");
  menu_vi["tab_setting"] = "Cài đặt";
  menu_vi["tab_mode"] = "Chế độ";
  JsonObject setting_vi = menu_vi.createNestedObject("setting");
  setting_vi["item0"] = "Tốc độ video";
  setting_vi["item1"] = "Xoay màn hình";
  setting_vi["item2"] = "Ngôn ngữ";
  setting_vi["item3"] = "Thẻ SD";
  setting_vi["item4"] = "TG Thông Báo";
  setting_vi["item5"] = "Tốc độ chữ";
  setting_vi["item6"] = "Lưu";
  setting_vi["item7"] = "Thoát";
  JsonObject mode_vi = menu_vi.createNestedObject("mode");
  mode_vi["item0"] = "Chơi Flappy";
  mode_vi["item1"] = "Chơi Đua Xe";
  mode_vi["item2"] = "Đồng hồ số";
  mode_vi["item3"] = "Đồng hồ kim";
  mode_vi["item4"] = "Thời tiết";
  mode_vi["item5"] = "Thoát";

  JsonObject menu_en = doc.createNestedObject("menu_en");
  menu_en["tab_setting"] = "SETTING";
  menu_en["tab_mode"] = "MODE";
  JsonObject setting_en = menu_en.createNestedObject("setting");
  setting_en["item0"] = "Video Speed";
  setting_en["item1"] = "Screen Rotation";
  setting_en["item2"] = "Language";
  setting_en["item3"] = "SD Card";
  setting_en["item4"] = "Notif. Time";
  setting_en["item5"] = "Marquee Speed";
  setting_en["item6"] = "Save";
  setting_en["item7"] = "Exit";
  JsonObject mode_en = menu_en.createNestedObject("mode");
  mode_en["item0"] = "Play Flappy";
  mode_en["item1"] = "Play Car Game";
  mode_en["item2"] = "Watch (Digital)";
  mode_en["item3"] = "Watch (Analog)";
  mode_en["item4"] = "Weather";
  mode_en["item5"] = "Exit";

  serializeJson(doc, configFile);
  configFile.close();
}

void loadSettings()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS Mount Failed.");
    return;
  }

  bool success = false;
  File configFile = SPIFFS.open(CONFIG_FILE, "r");
  if (configFile)
  {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    if (!error)
    {
      settings.frameDelay = doc["frameDelay"] | 20;
      settings.currentRotation = doc["currentRotation"] | 3;
      settings.currentLang = doc["language"] | "vi";
      settings.useSD = doc["useSD"] | false;
      settings.notificationTimeout = doc["notificationTimeout"] | 5;
      settings.marqueeSpeed = doc["marqueeSpeed"] | 35;
      settings.currentAnalogFaceIndex = doc["currentAnalogFaceIndex"] | 0;
      menu_load_strings(doc.as<JsonObject>());
      success = true;
    }
    configFile.close();
  }

  if (!success)
  {
    Serial.println("Config not loaded or invalid. Creating default.");
    settings.frameDelay = 20;
    settings.currentRotation = 3;
    settings.currentLang = "vi";
    settings.useSD = false;
    settings.notificationTimeout = 5;
    settings.marqueeSpeed = 35;
    settings.currentAnalogFaceIndex = 0;

    StaticJsonDocument<1024> default_doc;
    JsonObject menu_vi = default_doc.createNestedObject("menu_vi");
    menu_vi["tab_setting"] = "Cài đặt";
    menu_vi["tab_mode"] = "Chế độ";
    JsonObject setting_vi = default_doc.createNestedObject("setting");
    setting_vi["item0"] = "Tốc độ video";
    setting_vi["item1"] = "Xoay màn hình";
    setting_vi["item2"] = "Ngôn ngữ";
    setting_vi["item3"] = "Thẻ SD";
    setting_vi["item4"] = "TG Thông Báo";
    setting_vi["item5"] = "Tốc độ chữ";
    setting_vi["item6"] = "Lưu";
    setting_vi["item7"] = "Thoát";
    JsonObject mode_vi = default_doc.createNestedObject("mode");
    mode_vi["item0"] = "Chơi Flappy";
    mode_vi["item1"] = "Chơi Đua Xe";
    mode_vi["item2"] = "Đồng hồ số";
    mode_vi["item3"] = "Đồng hồ kim";
    mode_vi["item4"] = "Thời tiết";
    mode_vi["item5"] = "Thoát";

    JsonObject menu_en = default_doc.createNestedObject("menu_en");
    menu_en["tab_setting"] = "SETTING";
    menu_en["tab_mode"] = "MODE";
    JsonObject setting_en = menu_en.createNestedObject("setting");
    setting_en["item0"] = "Video Speed";
    setting_en["item1"] = "Screen Rotation";
    setting_en["item2"] = "Language";
    setting_en["item3"] = "SD Card";
    setting_en["item4"] = "Notif. Time";
    setting_en["item5"] = "Marquee Speed";
    setting_en["item6"] = "Save";
    setting_en["item7"] = "Exit";
    JsonObject mode_en = default_doc.createNestedObject("mode");
    mode_en["item0"] = "Play Flappy";
    mode_en["item1"] = "Play Car Game";
    mode_en["item2"] = "Watch (Digital)";
    mode_en["item3"] = "Watch (Analog)";
    mode_en["item4"] = "Weather";
    mode_en["item5"] = "Exit";

    menu_load_strings(default_doc.as<JsonObject>());

    saveSettings();
  }
}
