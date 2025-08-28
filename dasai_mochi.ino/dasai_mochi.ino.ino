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
#include "ui_utils.h"
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
#include "weather_icons.h"
#include "player_icons.h"
#include "audio_manager.h"

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
typedef struct _VideoInfo {
  const uint8_t *const *frames;
  const uint16_t *frames_size;
  uint16_t num_frames;
} VideoInfo;
#include "video01.h"
#include "video02.h"
#include "video03.h"
#include "video04.h"
VideoInfo *flashVideoList[] = { &video01, &video02, &video03, &video04 };
const uint8_t NUM_FLASH_VIDEOS = sizeof(flashVideoList) / sizeof(flashVideoList[0]);
uint8_t currentVideoIndex = 0;
uint16_t currentFrame = 0;

int musicListScrollOffset = 0;
int selectedMusicItem = 0;

VideoInfo analogFaces = { analog_face_frames, analog_face_frames_size, analog_face_num_frames };

// --- KHAI BÁO HÀM ---
void saveSettings();
void loadSettings();
void drawDigitalWatchFace();
void drawAnalogWatchFace();
void drawWeatherScreen();
void drawMusicListScreen();
void drawMusicPlayerScreen();

// =======================================================================================
// --- CÁC HÀM TIỆN ÍCH VÀ CALLBACK CHO VIỆC VẼ ---
// =======================================================================================

// Con trỏ toàn cục để trỏ đến sprite mục tiêu khi vẽ JPEG
TFT_eSprite *jpegSpriteTarget = nullptr;

// Callback để vẽ JPEG trực tiếp lên màn hình (cho video)
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (x >= tft.width() || y >= tft.height())
    return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// Callback mới để vẽ JPEG lên một sprite (cho mặt đồng hồ)
bool sprite_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (!jpegSpriteTarget)
    return false;  // An toàn nếu con trỏ chưa được thiết lập
  jpegSpriteTarget->pushImage(x, y, w, h, bitmap);
  return true;
}

void setSpritePixel(int16_t x, int16_t y, uint16_t color) {
  screenSprite.drawPixel(x, y, color);
}

void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "reset_config") {
      Serial.println("Received command: reset_config");
      if (!SPIFFS.begin(true)) {
        return;
      }
      if (SPIFFS.exists(CONFIG_FILE)) {
        SPIFFS.remove(CONFIG_FILE);
      }
      Serial.println("Restarting...");
      delay(1000);
      ESP.restart();
    } else if (command.startsWith("set_tracks:")) {
      Serial.println("Received command: set_tracks");
      String trackNamesStr = command.substring(11);  // Lấy chuỗi sau "set_tracks:"

      std::vector<String> newTrackList;
      int lastComma = -1;
      for (int i = 0; i < trackNamesStr.length(); i++) {
        if (trackNamesStr.charAt(i) == ',') {
          newTrackList.push_back(trackNamesStr.substring(lastComma + 1, i));
          lastComma = i;
        }
      }
      newTrackList.push_back(trackNamesStr.substring(lastComma + 1));

      audio_update_tracklist(newTrackList);
      saveSettings();  // Lưu lại file config với danh sách mới
      Serial.println("Tracklist updated. Restarting...");
      delay(1000);
      ESP.restart();
    }
  }
}

void drawMusicListScreen() {
  screenSprite.fillSprite(TFT_BLACK);
  const int itemHeight = 28;
  const int maxVisibleItems = 6;

  int trackCount = audio_get_track_count();
  if (trackCount == 0) {
    myfont.print(10, 110, "Không Có Bài Hát", TFT_YELLOW, TFT_BLACK);
  } else {
    for (int i = 0; i < maxVisibleItems; i++) {
      int trackIndex = musicListScrollOffset + i;
      if (trackIndex >= trackCount) break;

      int yPos = 20 + i * (itemHeight + 5);
      if (trackIndex == selectedMusicItem) {
        screenSprite.drawRoundRect(5, yPos - 4, tft.width() - 10, itemHeight, 5, TFT_WHITE);
      }
      String trackName = audio_get_track_name(trackIndex);
      drawMarqueeText(&screenSprite, &myfont, trackName, 15, yPos, tft.width() - 30, TFT_WHITE, TFT_BLACK, trackIndex == selectedMusicItem, settings.marqueeSpeed);
    }
  }
  screenSprite.pushSprite(0, 0);
}

void drawMusicPlayerScreen() {
  // 1. Vẽ nền hiệu ứng sóng nhạc
  drawMusicVisualizer(&screenSprite, audio_is_playing());

  // 2. Vẽ tên bài hát (chạy chữ)
  String trackName = audio_get_current_track_name();
  int trackNameWidth = myfont.getLength(trackName);
  int availableWidth = tft.width() - 20;
  if (trackNameWidth <= availableWidth) {
    int x_pos = (tft.width() - trackNameWidth) / 2;
    myfont.print(x_pos, 80, trackName, TFT_CYAN, TFT_BLACK);
  } else {
    drawMarqueeText(&screenSprite, &myfont, trackName, 10, 80, availableWidth, TFT_CYAN, TFT_BLACK, true, settings.marqueeSpeed);
  }

  // 3. Vẽ icon Play/Pause
  if (audio_is_playing()) {
    screenSprite.pushImage((tft.width() - 64) / 2, 120, 64, 64, pause_icon);
  } else {
    screenSprite.pushImage((tft.width() - 64) / 2, 120, 64, 64, play_icon);
  }

  // 4. Vẽ hướng dẫn
  // myfont.print(10, 210, "Next(2)", TFT_WHITE, TFT_BLACK);
  // myfont.print(tft.width() - myfont.getLength("List(3)") - 10, 210, "List(3)", TFT_WHITE, TFT_BLACK);

  screenSprite.pushSprite(0, 0);
}

// *** HÀM MỚI ĐỂ VẼ ICON THỜI TIẾT ***
void drawWeatherIcon(int iconIndex, int x, int y) {
  if (iconIndex < 0 || iconIndex > 7) {
    iconIndex = 7;  // Mặc định là icon "Unknown" nếu chỉ số không hợp lệ
  }
  // Đọc con trỏ từ PROGMEM, sau đó đọc dữ liệu ảnh từ con trỏ đó
  const uint16_t *icon_ptr = (const uint16_t *)pgm_read_ptr(&weather_icons[iconIndex]);
  screenSprite.pushImage(x, y, WEATHER48_W, WEATHER48_H, icon_ptr);
}

String getWeatherLabel(int iconIndex) {
  if (iconIndex < 0 || iconIndex > 7) {
    iconIndex = 7;
  }
  char buffer[20];
  if (settings.currentLang == "vi") {
    strcpy_P(buffer, (char *)pgm_read_ptr(&(WEATHER_LABELS_VI[iconIndex])));
  } else {
    strcpy_P(buffer, (char *)pgm_read_ptr(&(WEATHER_LABELS_EN[iconIndex])));
  }
  return String(buffer);
}

// =======================================================================================
// --- CÁC HÀM VẼ MẶT ĐỒNG HỒ ---
// =======================================================================================
void drawDigitalWatchFace() {
  drawMatrixRainBackground(&tft, &screenSprite);

  if (!chronos_is_time_synced()) {
    String msg = "Đang Kết Nối...";
    myfont.print((tft.width() - myfont.getLength(msg)) / 2, tft.height() / 2, msg, TFT_YELLOW, TFT_BLACK);
  } else {
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
void drawAnalogWatchFace() {
  if (analogFaces.num_frames == 0 || settings.currentAnalogFaceIndex >= analogFaces.num_frames) {
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

  // 5. Đẩy sprite đã hoàn chỉnh ra màn hình
  screenSprite.pushSprite(0, 0);
}

// *** HÀM MỚI ĐỂ VẼ MÀN HÌNH THỜI TIẾT ***
void drawWeatherScreen() {
  if (!chronos_has_weather_data()) {
    screenSprite.fillSprite(TFT_BLACK);
    myfont.print(10, 110, "Chưa Đồng Bộ", TFT_YELLOW, TFT_BLACK);
    screenSprite.pushSprite(0, 0);
    return;
  }

  WeatherData weather = chronos_get_weather();
  int iconIndex = weather.icon;

  // --- GIAI ĐOẠN 3: VẼ HIỆU ỨNG NỀN ĐỘNG ---
  if (iconIndex == 3 || iconIndex == 4) {  // Mưa hoặc Dông
    drawRainEffect(&tft, &screenSprite);
  } else {
    screenSprite.fillSprite(TFT_BLACK);
  }

  // --- GIAI ĐOẠN 2: VẼ GIAO DIỆN CHÍNH ---
  myfont.print((tft.width() - myfont.getLength(weather.city)) / 2, 20, weather.city, TFT_WHITE, TFT_BLACK);

  // Vẽ icon thời tiết
  drawWeatherIcon(iconIndex, (tft.width() - WEATHER48_W) / 2, 50);

  // Vẽ nhãn thời tiết
  String label = getWeatherLabel(iconIndex);
  myfont.print((tft.width() - myfont.getLength(label)) / 2, 105, label, TFT_CYAN, TFT_BLACK);

  // Nhiệt độ hiện tại
  String tempStr = String(weather.currentTemp) + " C";
  myfont.print((tft.width() - myfont.getLength(tempStr)) / 2, 140, tempStr, TFT_ORANGE, TFT_BLACK);

  // Nhiệt độ cao/thấp
  String highLowStr = "H:" + String(weather.highTemp) + " L:" + String(weather.lowTemp);
  myfont.print((tft.width() - myfont.getLength(highLowStr)) / 2, 170, highLowStr, TFT_WHITE, TFT_BLACK);

  // Thông tin khác
  String infoStr = "UV: " + String(weather.uv) + " | Áp Suất: " + String(weather.pressure);
  myfont.print((tft.width() - myfont.getLength(infoStr)) / 2, 200, infoStr, TFT_WHITE, TFT_BLACK);

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

  button_init();

  menu_init(&tft, &screenSprite, &myfont, &settings);

  chronos_init(&tft, &screenSprite, &myfont, &settings);
  Flappy::begin(&screenSprite);
  CarGame::begin(&screenSprite);
  initMatrixRain(&tft);
  initRainEffect(&tft);
  initMusicVisualizer(&tft);
  audio_init();

  loadSettings();

  tft.setRotation(settings.currentRotation);
  tft.fillScreen(TFT_BLACK);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  audio_set_volume(settings.volume);
  audio_set_autoplay(settings.musicAutoPlayNext);

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
  audio_loop();  // *** GỌI HÀM LOOP CỦA MODULE ÂM THANH ***
  ButtonAction action = getButtonAction();

  bool isAlertEvent = chronos_is_ringing() || chronos_has_new_notification() || chronos_has_new_navigation();

  if (isAlertEvent && !isDisplayingAlert) {
    if (currentMode != MENU) {
      modeBeforeAlert = currentMode;
      if (currentMode != WATCH_MODE && currentMode != ANALOG_WATCH_MODE) {
        currentMode = WATCH_MODE;
      }
      isDisplayingAlert = true;
    }
  }

  switch (currentMode) {
    case PLAYING:
      {
        static int lastVideoIndex = -1;
        if (action == ACTION_TRIPLE || action == ACTION_LONG) {
          currentMode = MENU;
          menu_enter();
          break;
        }
        if (action == ACTION_SINGLE) {
          currentVideoIndex = (currentVideoIndex + 1) % NUM_FLASH_VIDEOS;
          currentFrame = 0;
        } else if (action == ACTION_DOUBLE) {
          currentVideoIndex = VIDEO_JUMP_TARGET;
          currentFrame = 0;
        }

        // *** PHÁT ÂM THANH KHI VIDEO THAY ĐỔI ***
        if (lastVideoIndex != currentVideoIndex) {
          if (settings.soundEnabled) {
            audio_set_autoplay(false);
            audio_play_video_sound(currentVideoIndex);
          }
          lastVideoIndex = currentVideoIndex;
        }

        if (currentVideoIndex >= NUM_FLASH_VIDEOS)
          currentVideoIndex = 0;
        VideoInfo *currentVideo = flashVideoList[currentVideoIndex];
        // Vẽ video trực tiếp lên màn hình
        TJpgDec.setCallback(tft_output);
        const uint8_t *jpg_data = (const uint8_t *)pgm_read_ptr(&currentVideo->frames[currentFrame]);
        uint16_t jpg_size = pgm_read_word(&currentVideo->frames_size[currentFrame]);
        TJpgDec.drawJpg(0, 0, jpg_data, jpg_size);

        delay(settings.frameDelay);
        currentFrame++;
        if (currentFrame >= currentVideo->num_frames) {
          currentFrame = 0;
          currentVideoIndex = (currentVideoIndex + 1) % NUM_FLASH_VIDEOS;
          if (settings.soundEnabled) {
            audio_stop();
          }
        }
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
            audio_set_volume(settings.volume);
            audio_set_autoplay(settings.musicAutoPlayNext);
          }
          currentMode = newMode;
          tft.fillScreen(TFT_BLACK);
          if (currentMode == GAME_FLAPPY)
            Flappy::start();
          if (currentMode == GAME_CAR)
            CarGame::start();
        } else {
          menu_draw();
        }
        break;
      }

    case GAME_FLAPPY:
      {
        if (action == ACTION_TRIPLE) {
          Flappy::stop();
          currentMode = MENU;
          menu_enter();
          break;
        }

        static unsigned long lastFlapTime = 0;
        if (is_button_held() && Flappy::isRunning() && !Flappy::isPaused()) {
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

    case GAME_CAR:
      {
        if (action == ACTION_LONG) {
          CarGame::stop();
          currentMode = MENU;
          menu_enter();
          break;
        }
        if (action == ACTION_SINGLE)
          CarGame::moveRight();
        if (action == ACTION_DOUBLE)
          CarGame::moveLeft();
        if (action == ACTION_SINGLE && !CarGame::isRunning()) {
          CarGame::start();
        }
        CarGame::tick();
        screenSprite.pushSprite(0, 0);
        break;
      }

    case WATCH_MODE:
    case ANALOG_WATCH_MODE:
      {
        static unsigned long lastAnalogUpdate = 0;

        if (currentMode == ANALOG_WATCH_MODE && action == ACTION_DOUBLE) {
          if (analogFaces.num_frames > 0) {
            settings.currentAnalogFaceIndex = (settings.currentAnalogFaceIndex + 1) % analogFaces.num_frames;
            saveSettings();
          }
        }

        if (action == ACTION_LONG) {
          isDisplayingAlert = false;
          currentMode = MENU;
          menu_enter();
          break;
        }

        bool alertDrawn = chronos_draw_alerts();

        if (!alertDrawn) {
          if (isDisplayingAlert) {
            isDisplayingAlert = false;
            currentMode = modeBeforeAlert;
            break;
          }

          if (currentMode == WATCH_MODE) {
            drawDigitalWatchFace();
          } else {
            if (millis() - lastAnalogUpdate > 1000) {
              lastAnalogUpdate = millis();
              drawAnalogWatchFace();
            }
          }
        }
        break;
      }
    case WEATHER_MODE:
      {
        if (action == ACTION_LONG) {
          currentMode = MENU;
          menu_enter();
          break;
        }
        drawWeatherScreen();
        break;
      }
    case MUSIC_LIST_MODE:
      {
        if (action == ACTION_SINGLE) {
          selectedMusicItem = (selectedMusicItem + 1) % audio_get_track_count();
          // Logic cuộn tương tự menu
        }
        if (action == ACTION_LONG) {
          audio_set_autoplay(settings.musicAutoPlayNext);
          audio_play_music(selectedMusicItem);
          currentMode = MUSIC_PLAYER_MODE;
        }
        if (action == ACTION_TRIPLE) {  // Thoát về menu
          currentMode = MENU;
          menu_enter();
        }
        drawMusicListScreen();
        break;
      }

    case MUSIC_PLAYER_MODE:
      {
        if (action == ACTION_SINGLE) audio_pause_resume();
        if (action == ACTION_DOUBLE) audio_next();
        if (action == ACTION_TRIPLE) currentMode = MUSIC_LIST_MODE;
        if (action == ACTION_LONG) {
          audio_stop();
          currentMode = MENU;
          menu_enter();
        }
        drawMusicPlayerScreen();
        break;
      }
  }
}

// --- HÀM LOAD/SAVE SETTINGS ---
void saveSettings() {
  Serial.println("Saving settings to SPIFFS...");
  File configFile = SPIFFS.open(CONFIG_FILE, "w");
  if (!configFile)
    return;

  StaticJsonDocument<1024> doc;
  doc["frameDelay"] = settings.frameDelay;
  doc["currentRotation"] = settings.currentRotation;
  doc["language"] = settings.currentLang;
  doc["notificationTimeout"] = settings.notificationTimeout;
  doc["marqueeSpeed"] = settings.marqueeSpeed;
  doc["currentAnalogFaceIndex"] = settings.currentAnalogFaceIndex;
  doc["soundEnabled"] = settings.soundEnabled;
  doc["volume"] = settings.volume;
  doc["musicAutoPlayNext"] = settings.musicAutoPlayNext;
  doc["displayShape"] = (settings.displayShape == SHAPE_SQUARE) ? "square" : "round";

  // *** LƯU DANH SÁCH NHẠC VÀO JSON ***
  JsonArray tracks = doc.createNestedArray("trackList");
  const std::vector<String> &trackListRef = audio_get_tracklist_ref();
  for (const String &track : trackListRef) {
    tracks.add(track);
  }

  JsonObject menu_vi = doc.createNestedObject("menu_vi");
  menu_vi["tab_setting"] = "Cài đặt";
  menu_vi["tab_mode"] = "Chế độ";
  JsonObject setting_vi = menu_vi.createNestedObject("setting");
  setting_vi["item0"] = "Tốc độ video";
  setting_vi["item1"] = "Xoay màn hình";
  setting_vi["item2"] = "Ngôn ngữ";
  setting_vi["item3"] = "TG Thông Báo";
  setting_vi["item4"] = "Tốc độ chữ";
  setting_vi["item5"] = "Âm thanh";
  setting_vi["item6"] = "Âm lượng";
  setting_vi["item7"] = "Tự Động Chuyển Bài";
  setting_vi["item8"] = "Hình Dạng";
  setting_vi["item9"] = "Lưu";
  setting_vi["item10"] = "Thoát";
  JsonObject mode_vi = menu_vi.createNestedObject("mode");
  mode_vi["item0"] = "Chơi Flappy";
  mode_vi["item1"] = "Chơi Đua Xe";
  mode_vi["item2"] = "Đồng hồ số";
  mode_vi["item3"] = "Đồng hồ kim";
  mode_vi["item4"] = "Thời Tiết";
  mode_vi["item5"] = "Nghe Nhạc";
  mode_vi["item6"] = "Thoát";

  JsonObject menu_en = doc.createNestedObject("menu_en");
  menu_en["tab_setting"] = "SETTING";
  menu_en["tab_mode"] = "MODE";
  JsonObject setting_en = menu_en.createNestedObject("setting");
  setting_en["item0"] = "Video Speed";
  setting_en["item1"] = "Screen Rotation";
  setting_en["item2"] = "Language";
  setting_en["item3"] = "Notif. Time";
  setting_en["item4"] = "Marquee Speed";
  setting_en["item5"] = "Sound Enabled";
  setting_en["item6"] = "Volume";
  setting_en["item7"] = "Auto Next";
  setting_en["item8"] = "Display Shape";
  setting_en["item9"] = "Save";
  setting_en["item10"] = "Exit";
  JsonObject mode_en = menu_en.createNestedObject("mode");
  mode_en["item0"] = "Play Flappy";
  mode_en["item1"] = "Play Car Game";
  mode_en["item2"] = "Watch (Digital)";
  mode_en["item3"] = "Watch (Analog)";
  mode_en["item4"] = "Weather";
  mode_en["item5"] = "Play Music";
  mode_en["item6"] = "Exit";

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
      settings.notificationTimeout = doc["notificationTimeout"] | 5;
      settings.marqueeSpeed = doc["marqueeSpeed"] | 35;
      settings.currentAnalogFaceIndex = doc["currentAnalogFaceIndex"] | 0;
      settings.soundEnabled = doc["soundEnabled"] | false;
      settings.volume = doc["volume"] | 10;
      settings.musicAutoPlayNext = doc["musicAutoPlayNext"] | true;
      String shapeStr = doc["displayShape"] | "square";
      settings.displayShape = (shapeStr == "round") ? SHAPE_ROUND : SHAPE_SQUARE;

      // *** TẢI DANH SÁCH NHẠC TỪ JSON ***
      JsonArray tracks = doc["trackList"];
      std::vector<String> loadedTracks;
      for (JsonVariant v : tracks) {
        loadedTracks.push_back(v.as<String>());
      }
      audio_update_tracklist(loadedTracks);

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
    settings.notificationTimeout = 5;
    settings.marqueeSpeed = 35;
    settings.currentAnalogFaceIndex = 0;
    settings.soundEnabled = false;
    settings.volume = 15;
    settings.musicAutoPlayNext = true;
    settings.displayShape = SHAPE_SQUARE;

    audio_update_tracklist({});  // Tạo danh sách trống

    StaticJsonDocument<1024> default_doc;
    JsonObject menu_vi = default_doc.createNestedObject("menu_vi");
    menu_vi["tab_setting"] = "Cài đặt";
    menu_vi["tab_mode"] = "Chế độ";
    JsonObject setting_vi = default_doc.createNestedObject("setting");
    setting_vi["item0"] = "Tốc độ video";
    setting_vi["item1"] = "Xoay màn hình";
    setting_vi["item2"] = "Ngôn ngữ";
    setting_vi["item3"] = "TG Thông Báo";
    setting_vi["item4"] = "Tốc độ chữ";
    setting_vi["item5"] = "Âm thanh";
    setting_vi["item6"] = "Âm lượng";
    setting_vi["item7"] = "Tự Động Chuyển Bài";
    setting_vi["item8"] = "Hình Dạng";
    setting_vi["item9"] = "Lưu";
    setting_vi["item10"] = "Thoát";
    JsonObject mode_vi = default_doc.createNestedObject("mode");
    mode_vi["item0"] = "Chơi Flappy";
    mode_vi["item1"] = "Chơi Đua Xe";
    mode_vi["item2"] = "Đồng hồ số";
    mode_vi["item3"] = "Đồng hồ kim";
    mode_vi["item4"] = "Thời tiết";
    mode_vi["item5"] = "Nghe nhạc";
    mode_vi["item6"] = "Thoát";

    JsonObject menu_en = default_doc.createNestedObject("menu_en");
    menu_en["tab_setting"] = "SETTING";
    menu_en["tab_mode"] = "MODE";
    JsonObject setting_en = menu_en.createNestedObject("setting");
    setting_en["item0"] = "Video Speed";
    setting_en["item1"] = "Screen Rotation";
    setting_en["item2"] = "Language";
    setting_en["item3"] = "Notif. Time";
    setting_en["item4"] = "Marquee Speed";
    setting_en["item5"] = "Sound Enabled";
    setting_en["item6"] = "Volume";
    setting_en["item7"] = "Auto Next";
    setting_en["item8"] = "Display Shape";
    setting_en["item9"] = "Save";
    setting_en["item10"] = "Exit";
    JsonObject mode_en = default_doc.createNestedObject("mode");
    mode_en["item0"] = "Play Flappy";
    mode_en["item1"] = "Play Car Game";
    mode_en["item2"] = "Watch (Digital)";
    mode_en["item3"] = "Watch (Analog)";
    mode_en["item4"] = "Weather";
    mode_en["item5"] = "Play Music";
    mode_en["item6"] = "Exit";

    menu_load_strings(default_doc.as<JsonObject>());

    saveSettings();
  }
}
