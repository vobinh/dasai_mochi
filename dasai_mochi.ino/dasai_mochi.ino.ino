#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include "SD.h"
#include <SPI.h>
#include <vector>
#include <time.h>
#include "FontMaker.h"
#include "Digitall0132pt7b.h"
#include "Digitall0124pt7b.h"

#include "globals.h"
#include "ui_utils.h"
#include "menu_manager.h"
#include "button_manager.h"
#include "audio_manager.h"
#include "chronos_manager.h"
#include "ui_effects.h"
#include "wifi_manager.h"
#include "weather_station.h"

#include "flappy_game.h"
#include "car_game.h"
#include "hour_hand.h"
#include "minute_hand.h"
#include "second_hand.h"
#include "analog_face.h"
#include "weather_icons.h"
#include "player_icons.h"
#include "navigation_background.h"


// --- CẤU HÌNH ---
#define VIDEO_JUMP_TARGET 2
// *** THÊM MỚI: Cấu hình cho chế độ không hoạt động ***
#define IDLE_VIDEO_INDEX 1              // Video sẽ nhảy đến sau 2 phút (video02)
#define IDLE_TIMEOUT_VIDEO 30000        // 2 phút (tính bằng mili giây)
#define IDLE_TIMEOUT_MODE_SWITCH 60000  // 5 phút (tính bằng mili giây)

// --- CÁC BIẾN TOÀN CỤC ---
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite screenSprite = TFT_eSprite(&tft);
TFT_eSprite hourHandSprite = TFT_eSprite(&tft);
TFT_eSprite minuteHandSprite = TFT_eSprite(&tft);
TFT_eSprite secondHandSprite = TFT_eSprite(&tft);
TFT_eSprite *fontTargetSprite = nullptr;  // Con trỏ để chỉ định sprite nào sẽ nhận chữ vẽ

static uint32_t lastUserInteractionTime = 0;
static bool isIdleVideoActive = false;

void setSpritePixel_dynamic(int16_t x, int16_t y, uint16_t color) {
  if (fontTargetSprite) {
    fontTargetSprite->drawPixel(x, y, color);
  }
}
// *** KẾT THÚC SỬA LỖI ***

void setSpritePixel(int16_t x, int16_t y, uint16_t color);
MakeFont myfont(&setSpritePixel_dynamic);

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

uint16_t currentBgFrame = 0;

// Cấu trúc cho video từ file .bin
struct FrameInfo {
  uint32_t offset;
  uint32_t size;
};

struct DynamicVideo {
  File file;
  uint32_t num_frames = 0;
  FrameInfo *index_table = nullptr;
  bool is_loaded = false;
};

struct JpegInput {
  File *file;
  uint32_t start_pos;
  uint32_t current_pos;
  uint32_t size;
};

// Biến toàn cục cho video động
DynamicVideo dynamicVideo;

VideoInfo analogFaces = { analog_face_frames, analog_face_frames_size, analog_face_num_frames };

// --- MUSIC VARS ---
int musicListScrollOffset = 0;
int selectedMusicItem = 0;

// --- SCROLL TEXT SETTINGS VARS ---
static int scrollTextSettingsSelectedItem = 0;
static int scrollTextSettingsScrollOffset = 0;
static String scrollTextSettingsItems[NUM_SCROLL_TEXT_SETTINGS_ITEMS_CONST];
static bool reset_scroll_text_position = false;
// Options for color setting
static const uint16_t colorOptions[] = { TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA };
static const String colorNames[] = { "Trắng", "Đỏ", "Xanh L", "Xanh D", "Vàng", "Lơ", "Tím" };
static const int numColorOptions = sizeof(colorOptions) / sizeof(colorOptions[0]);
static int tempColorIndex = 0;
// Options for speed setting
static const int speedOptions[] = { 50, 35, 20 };  // Chậm, Vừa, Nhanh
static const String speedLabels[] = { "Chậm", "Vừa", "Nhanh" };
static const int numSpeedOptions = sizeof(speedOptions) / sizeof(speedOptions[0]);
static int tempSpeedIndex = 0;

// --- KHAI BÁO HÀM ---
void saveSettings();
void loadSettings();
void loadUiStrings();
void loadAudioTracklist();
void drawDigitalWatchFace();
void drawAnalogWatchFace();
void drawWeatherScreen();
void drawMusicListScreen();
void drawMusicPlayerScreen();
void drawScrollTextMode(bool reset = false);
void drawScrollTextSettingsScreen();
void initScrollTextSettings();
void populateMenuText(JsonDocument &doc);

void drawWifiUploadScreen();
void wifi_manager_save_settings();  // Forward declaration
// =======================================================================================
// --- CÁC HÀM TIỆN ÍCH VÀ CALLBACK CHO VIỆC VẼ ---
// =======================================================================================

// Con trỏ toàn cục để trỏ đến sprite mục tiêu khi vẽ JPEG
TFT_eSprite *jpegSpriteTarget = nullptr;

void drawWifiUploadScreen() {
  screenSprite.fillSprite(TFT_BLACK);
  myfont.print((tft.width() - myfont.getLength("WiFi Upload")) / 2, 20, "WiFi Upload", TFT_CYAN, TFT_BLACK);

  myfont.print(20, 60, "Turn on WiFi & Connect:", TFT_WHITE, TFT_BLACK);
  myfont.print(30, 85, "SSID: Mochi-Watch", TFT_YELLOW, TFT_BLACK);
  myfont.print(30, 110, "Pass: 12345678", TFT_YELLOW, TFT_BLACK);

  myfont.print(20, 145, "Open Browser & Access:", TFT_WHITE, TFT_BLACK);
  String ip = wifi_manager_get_ip();
  myfont.print(30, 170, ip, TFT_YELLOW, TFT_BLACK);

  String exitMsg = "Press & hold - Exit";
  myfont.print((tft.width() - myfont.getLength(exitMsg)) / 2, 210, exitMsg, TFT_RED, TFT_BLACK);

  screenSprite.pushSprite(0, 0);
}

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
    } else if (command.startsWith("set_scroll_text:")) {
      Serial.println("Received command: set_scroll_text");
      String newText = command.substring(16);
      settings.scrollText.text = newText;
      saveSettings();
      Serial.println("Scroll text updated.");
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
// void drawWeatherIcon(int iconIndex, int x, int y) {
//   if (iconIndex < 0 || iconIndex > 7) {
//     iconIndex = 7;  // Mặc định là icon "Unknown" nếu chỉ số không hợp lệ
//   }
//   // Đọc con trỏ từ PROGMEM, sau đó đọc dữ liệu ảnh từ con trỏ đó
//   const uint16_t *icon_ptr = (const uint16_t *)pgm_read_ptr(&weather_icons[iconIndex]);
//   Serial.printf("First pixel = 0x%04X\n", icon_ptr[0]);
//   screenSprite.pushImage(x, y, WEATHER_W, WEATHER_H, (uint16_t*)icon_ptr, TFT_BLACK);
// }

void drawWeatherIcon(int iconIndex, int x, int y) {
  if (iconIndex < 0 || iconIndex >= (sizeof(weather_icons) / sizeof(weather_icons[0]))) {
    iconIndex = 7;  // Mặc định là icon "Unknown" nếu chỉ số không hợp lệ
  }
  // Đọc con trỏ từ PROGMEM
  const uint16_t *icon_ptr = (const uint16_t *)pgm_read_ptr(&weather_icons[iconIndex]);

  // Tạo một bộ đệm trên stack để chứa một dòng của icon
  uint16_t line_buffer[WEATHER_W];

  // Lặp qua từng dòng (y) và từng pixel (x) của icon
  for (int j = 0; j < WEATHER_H; j++) {
    // Sao chép một dòng từ PROGMEM vào bộ đệm RAM để tăng tốc độ truy cập
    memcpy_P(line_buffer, &icon_ptr[j * WEATHER_W], WEATHER_W * 2);

    for (int i = 0; i < WEATHER_W; i++) {
      uint16_t color = line_buffer[i];
      // Chỉ vẽ pixel nếu nó không phải là màu đen (màu trong suốt)
      if (color != TFT_BLACK) {
        // *** SỬA LỖI MÀU: Hoán đổi byte cao và byte thấp của màu ***
        uint16_t swapped_color = (color << 8) | (color >> 8);
        screenSprite.drawPixel(x + i, y + j, swapped_color);
      }
    }
  }
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

  // Đảm bảo font mặc định của myfont được thiết lập
  fontTargetSprite = &screenSprite;
  myfont.set_font(Fira_Code_16);

  if (!chronos_is_time_synced()) {
    String msg = "Đang Kết Nối...";
    myfont.print((tft.width() - myfont.getLength(msg)) / 2, tft.height() / 2, msg, TFT_YELLOW, TFT_BLACK);
  } else {
    // Thiết lập font Digitall0132pt7b cho thời gian
    screenSprite.setFreeFont(&Digitall0132pt7b);
    char timeStr[9];
    sprintf(timeStr, "%02d:%02d:%02d", chronos_get_hour(), chronos_get_minute(), chronos_get_second());
    int textWidth = screenSprite.textWidth(timeStr);
    int x_pos = (tft.width() - textWidth) / 2;
    int y_pos = (tft.height() - 50) / 2;
    screenSprite.setTextColor(TFT_CYAN, TFT_BLACK);
    screenSprite.drawString(timeStr, x_pos, y_pos);
    // Trả về font mặc định sau khi sử dụng Digitall0132pt7b
    screenSprite.setFreeFont(NULL);

    // Sử dụng myfont cho ngày tháng
    fontTargetSprite = &screenSprite;
    myfont.set_font(Fira_Code_16);  // Đảm bảo myfont là Fira_Code_16
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

  // Đảm bảo font mặc định của myfont được thiết lập
  fontTargetSprite = &screenSprite;
  myfont.set_font(Fira_Code_16);

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

int centerTextInRegion(String text, int offsetX, int regionW) {
  int textW = myfont.getLength(text);
  return offsetX + (regionW - textW) / 2;
}

// *** HÀM MỚI ĐỂ VẼ MÀN HÌNH THỜI TIẾT ***
void drawWeatherScreen() {
  if (!chronos_has_weather_data()) {
    screenSprite.fillSprite(TFT_BLACK);
    myfont.print(10, 110, "Chưa Đồng Bộ", TFT_YELLOW, TFT_BLACK);
    screenSprite.pushSprite(0, 0);
    return;
  }

  // *** FIX: Khôi phục lại trạng thái font của sprite ***
  screenSprite.setTextSize(1);
  screenSprite.setTextDatum(TL_DATUM);
  screenSprite.setFreeFont(nullptr);
  fontTargetSprite = &screenSprite;  // Đảm bảo myfont cũng vẽ đúng chỗ
  // *** END FIX ***

  screenSprite.fillSprite(TFT_BLACK);

  VideoInfo *backgroundVideo = flashVideoList[settings.currentWeatherIndex];
  jpegSpriteTarget = &screenSprite;
  TJpgDec.setCallback(sprite_output);
  TJpgDec.setJpgScale(4);

  const uint8_t *jpg_data = (const uint8_t *)pgm_read_ptr(&backgroundVideo->frames[currentBgFrame]);
  uint16_t jpg_size = pgm_read_word(&backgroundVideo->frames_size[currentBgFrame]);

  int video_w = 240 / 4;
  int video_h = 240 / 4;
  int x_img = tft.width() - video_w;
  int y_img = tft.height() - video_h;

  TJpgDec.drawJpg(x_img, y_img, jpg_data, jpg_size);

  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);  // Khôi phục callback mặc định

  currentBgFrame = (currentBgFrame + 1) % backgroundVideo->num_frames;

  WeatherData weather = chronos_get_weather();
  int iconIndex = weather.icon;

  int regionW = tft.width() / 3;
  int offsetX = tft.width() * 2 / 3;

  String city = weather.city;
  int lastSpace = city.lastIndexOf(' ');
  if (lastSpace > 0) {
    city = city.substring(0, lastSpace);
  }

  screenSprite.setTextFont(4);
  screenSprite.setTextColor(0xFFFF);
  screenSprite.drawString(city, (160 - screenSprite.textWidth(city)) / 2, 20);
  screenSprite.setTextFont(1);

  drawWeatherIcon(iconIndex, offsetX + (regionW - WEATHER_W) / 2, 0);

  screenSprite.setFreeFont(&Digitall0132pt7b);

  char hourStr[3];
  char minuteStr[3];
  sprintf(hourStr, "%02d", chronos_get_hour());
  sprintf(minuteStr, "%02d", chronos_get_minute());

  int hourW = screenSprite.textWidth(hourStr);
  int minuteW = screenSprite.textWidth(minuteStr);
  int totalW = hourW + minuteW;
  int timeX = 0 + (offsetX - totalW) / 2;

  screenSprite.setTextColor(0xFEE0);
  screenSprite.drawString(hourStr, timeX, 55);
  screenSprite.setTextColor(0xF206);
  screenSprite.drawString(minuteStr, timeX + hourW + 5, 55);

  screenSprite.setFreeFont(&Digitall0124pt7b);
  char secondStr[3];
  sprintf(secondStr, "%02d", chronos_get_second());
  screenSprite.setTextColor(0x24BE);
  screenSprite.drawString(secondStr, offsetX + (regionW - screenSprite.textWidth(secondStr)) / 2, 95);
  screenSprite.setFreeFont(nullptr);

  screenSprite.setTextFont(4);
  char dateStr[11];
  sprintf(dateStr, "%02d/%02d/%d", chronos_get_day(), chronos_get_month(), chronos_get_year());
  screenSprite.setTextColor(0xFFFF);
  screenSprite.drawString(dateStr, (160 - screenSprite.textWidth(dateStr)) / 2, 120);
  screenSprite.setTextFont(1);
  // screenSprite.setTextSize(1);
  // screenSprite.setTextDatum(TL_DATUM);
  // screenSprite.setFreeFont(nullptr);

  String label = getWeatherLabel(iconIndex);
  int textW = myfont.getLength(label);
  int textX = offsetX + (regionW - textW) / 2;
  int textY = 66;
  screenSprite.fillRoundRect(166, textY, 72, 26, 2, TFT_WHITE);
  if (textW <= 72) {
    myfont.print(textX, textY + 2, label, 0x8410, TFT_WHITE);
  } else {
    drawMarqueeText(&screenSprite, &myfont, label, 167, textY + 2, 72, 0x8410, TFT_WHITE, true, settings.marqueeSpeed);
  }

  screenSprite.setFreeFont(&Digitall0132pt7b);
  String tempStr = String(weather.currentTemp);
  int tempW = screenSprite.textWidth(tempStr);
  int cW = screenSprite.textWidth("C");
  int totalTempW = tempW + cW;
  int tempX = 0 + (offsetX - totalTempW) / 2;
  // Add degree symbol manually
  // screenSprite.setTextDatum(MC_DATUM);
  screenSprite.setTextColor(int(weather.currentTemp) > 29 ? TFT_ORANGE : TFT_GREEN, TFT_BLACK);
  screenSprite.drawString(tempStr, tempX - 5, 145);
  screenSprite.setTextColor(int(weather.currentTemp) > 29 ? TFT_ORANGE : TFT_GREEN, TFT_BLACK);
  screenSprite.drawString("C", tempX + tempW + 5, 145);
  screenSprite.drawCircle(tempX + tempW, 155, 4, int(weather.currentTemp) > 29 ? TFT_ORANGE : TFT_GREEN);
  screenSprite.setTextDatum(TL_DATUM);  // Reset datum
  screenSprite.setFreeFont(NULL);

  String highLowStr = "H:" + String(weather.highTemp) + "°C L:" + String(weather.lowTemp) + "°C";
  String infoStr = "UV:" + String(weather.uv);
  String other = highLowStr + " " + infoStr;
  int otherW = myfont.getLength(other);

  Serial.printf("otherW: ");
  Serial.println(otherW);

  if (otherW <= 150) {
    myfont.print(6, 220, other, TFT_WHITE, TFT_BLACK);
  } else {
    drawMarqueeText(&screenSprite, &myfont, other, 6, 220, 150, TFT_WHITE, TFT_BLACK, true, settings.marqueeSpeed);
  }

  screenSprite.pushSprite(0, 0);
}


void drawScrollTextMode(bool reset) {
  screenSprite.fillSprite(TFT_BLACK);
  static int32_t scroll_x = 0;
  static uint32_t last_scroll_time = 0;
  int screen_width = tft.width();

  if (reset) {
    scroll_x = screen_width;  // Bắt đầu chạy từ cạnh phải
  }

  String text = settings.scrollText.text;
  uint16_t textColor = settings.scrollText.textColor;
  int speed_delay = settings.scrollText.speed;

  int text_width = myfont.getLength(text);
  int y_pos = (tft.height() - 20) / 2;

  // Nếu văn bản ngắn hơn màn hình, hiển thị tĩnh ở giữa
  if (text_width <= screen_width) {
    int x_pos = (screen_width - text_width) / 2;
    myfont.print(x_pos, y_pos, text, textColor, TFT_BLACK);
  }
  // Nếu văn bản dài hơn, chạy chữ
  else {
    // Cập nhật vị trí cuộn dựa trên thời gian
    if (millis() - last_scroll_time > speed_delay) {
      last_scroll_time = millis();
      scroll_x--;
    }

    // Khoảng cách giữa các lần lặp lại của văn bản
    int gap = 100;
    int cycle_length = text_width + gap;

    // Vẽ bản chính
    myfont.print(scroll_x, y_pos, text, textColor, TFT_BLACK);

    // Vẽ bản sao ngay sau bản chính để tạo hiệu ứng nối liền
    myfont.print(scroll_x + cycle_length, y_pos, text, textColor, TFT_BLACK);

    // Khi bản chính đã chạy hoàn toàn ra khỏi màn hình,
    // reset vị trí của nó về phía trước một chu kỳ để tạo vòng lặp.
    if (scroll_x < -cycle_length) {
      scroll_x += cycle_length;
    }
  }
  screenSprite.pushSprite(0, 0);
}

void drawScrollTextSettingsScreen() {
  fontTargetSprite = &screenSprite;
  screenSprite.fillSprite(TFT_BLACK);

  const int titleHeight = 30;
  const int itemHeight = 28;
  const int startY = titleHeight + 5;
  const int screen_center_x = tft.width() / 2;
  const int screen_radius = tft.width() / 2;

  // 1. Vẽ tiêu đề
  String title = "C.Đặt Chữ Chạy";
  int title_w = myfont.getLength(title);
  myfont.print((tft.width() - title_w) / 2, 7, title, TFT_CYAN, TFT_BLACK);

  // 2. Lấy các thông số
  const int numItems = NUM_SCROLL_TEXT_SETTINGS_ITEMS_CONST;
  const int visibleItems = (settings.displayShape == SHAPE_ROUND) ? 5 : 6;
  const int listHeight = tft.height() - startY;
  const int totalItemsHeight = visibleItems * itemHeight;
  const int itemSpacing = (visibleItems > 1) ? (listHeight - totalItemsHeight) / (visibleItems - 1) : 0;

  // 3. Vẽ danh sách các mục
  if (settings.displayShape == SHAPE_ROUND) {
    const int centerSlot = visibleItems / 2;
    int firstItemLogicalIndex = scrollTextSettingsSelectedItem - centerSlot;

    for (int i = 0; i < visibleItems; i++) {
      int itemIndex = (firstItemLogicalIndex + i + numItems) % numItems;
      int currentY = startY + i * (itemHeight + itemSpacing);
      bool isSelected = (itemIndex == scrollTextSettingsSelectedItem);
      uint16_t bgColor = isSelected ? TFT_BLUE : TFT_BLACK;
      uint16_t textColor = isSelected ? TFT_WHITE : TFT_LIGHTGREY;

      int itemX = 5, itemW = tft.width() - 10;
      int itemCenterY = currentY + (itemHeight / 2);
      int d = abs(screen_radius - itemCenterY);
      if (d < screen_radius) {
        int w_half = sqrt(screen_radius * screen_radius - d * d) - 5;
        if (w_half > 0) {
          itemW = w_half * 2;
          itemX = screen_center_x - w_half;
        } else {
          itemW = 0;
        }
      } else {
        itemW = 0;
      }

      if (itemW > 0) {
        screenSprite.fillRoundRect(itemX, currentY - 4, itemW, itemHeight + 2, 5, bgColor);
        myfont.print(itemX + 10, currentY, scrollTextSettingsItems[itemIndex], textColor, bgColor);

        String valueStr = "";
        if (itemIndex == 0) {
          valueStr = colorNames[tempColorIndex];
          screenSprite.fillRoundRect(itemX + itemW - 50, currentY - 2, 40, itemHeight - 2, 3, colorOptions[tempColorIndex]);
        } else if (itemIndex == 1) {
          valueStr = speedLabels[tempSpeedIndex];
        }

        if (valueStr.length() > 0) {
          int textW = myfont.getLength(valueStr);
          myfont.print(itemX + itemW - textW - 60, currentY, valueStr, textColor, bgColor);
        }
      }
    }
  } else {  // SHAPE_SQUARE
    for (int i = 0; i < visibleItems; i++) {
      int itemIndex = scrollTextSettingsScrollOffset + i;
      if (itemIndex >= numItems) break;

      int currentY = startY + i * (itemHeight + itemSpacing);
      bool isSelected = (itemIndex == scrollTextSettingsSelectedItem);
      uint16_t bgColor = isSelected ? TFT_BLUE : TFT_BLACK;
      uint16_t textColor = isSelected ? TFT_WHITE : TFT_LIGHTGREY;

      int itemX = 5, itemW = tft.width() - 10;
      screenSprite.fillRoundRect(itemX, currentY - 4, itemW, itemHeight + 2, 5, bgColor);
      myfont.print(itemX + 10, currentY, scrollTextSettingsItems[itemIndex], textColor, bgColor);

      String valueStr = "";
      if (itemIndex == 0) {
        valueStr = colorNames[tempColorIndex];
        screenSprite.fillRoundRect(itemX + itemW - 50, currentY - 2, 40, itemHeight - 2, 3, colorOptions[tempColorIndex]);
      } else if (itemIndex == 1) {
        valueStr = speedLabels[tempSpeedIndex];
      }

      if (valueStr.length() > 0) {
        int textW = myfont.getLength(valueStr);
        myfont.print(itemX + itemW - textW - 60, currentY, valueStr, textColor, bgColor);
      }
    }
  }
  screenSprite.pushSprite(0, 0);
}

void initScrollTextSettings() {
  tempColorIndex = 0;
  for (int i = 0; i < numColorOptions; i++) {
    if (settings.scrollText.textColor == colorOptions[i]) {
      tempColorIndex = i;
      break;
    }
  }
  tempSpeedIndex = 0;
  for (int i = 0; i < numSpeedOptions; i++) {
    if (settings.scrollText.speed == speedOptions[i]) {
      tempSpeedIndex = i;
      break;
    }
  }
  scrollTextSettingsSelectedItem = 0;
  scrollTextSettingsScrollOffset = 0;  // *** RESET OFFSET KHI VÀO MENU ***
}

// =======================================================================================
// --- SETUP & LOOP ---
// =======================================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Mochi Watch Booting Up ---");

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  loadSettings();

  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(settings.currentRotation);

  screenSprite.createSprite(tft.width(), tft.height());

  // Mặc định, font sẽ vẽ lên sprite chính
  fontTargetSprite = &screenSprite;
  myfont.set_font(Fira_Code_16);

  if (settings.wifiEnabled) {
    Serial.println("*** WiFi Only Boot Mode Activated! ***");
    button_init();

    wifi_manager_init(&settings);
    wifi_manager_connect();

    currentMode = WIFI_UPLOAD_MODE;
    Serial.println("--- Booted directly into WiFi Upload Mode ---");
  } else if (settings.weatherEnabled) {
    Serial.println("*** Weather Boot Mode Activated! ***");
    button_init();

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);

    weather_station_init(&tft, &screenSprite, &myfont, &settings);
    weather_station_enter();
    currentMode = WEATHER_STATION_MODE;
    Serial.println("--- Booted directly into WEATHER STATION MODE ---");
  } else {
    Serial.println("--- Normal Boot Mode ---");

    button_init();
    menu_init(&screenSprite, &myfont, &settings);
    loadUiStrings();
    chronos_init(&tft, &screenSprite, &myfont, &settings);
    audio_init();

    audio_set_volume(0);

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);

    Flappy::begin(&screenSprite);
    CarGame::begin(&screenSprite);
    NavigationBackground::begin(&screenSprite);

    initMatrixRain(&tft);
    initRainEffect(&tft);
    initMusicVisualizer(&tft);

    audio_set_volume(settings.volume);
    audio_set_autoplay(settings.musicAutoPlayNext);
    loadAudioTracklist();

    hourHandSprite.createSprite(HOUR_HAND_WIDTH, HOUR_HAND_HEIGHT);
    hourHandSprite.setPivot(HOUR_PIVOT_X, HOUR_PIVOT_Y);
    hourHandSprite.pushImage(0, 0, HOUR_HAND_WIDTH, HOUR_HAND_HEIGHT, hourHandImage);
    minuteHandSprite.createSprite(MINUTE_HAND_WIDTH, MINUTE_HAND_HEIGHT);
    minuteHandSprite.setPivot(MINUTE_PIVOT_X, MINUTE_PIVOT_Y);
    minuteHandSprite.pushImage(0, 0, MINUTE_HAND_WIDTH, MINUTE_HAND_HEIGHT, minuteHandImage);
    secondHandSprite.createSprite(SECOND_HAND_WIDTH, SECOND_HAND_HEIGHT);
    secondHandSprite.setPivot(SECOND_PIVOT_X, SECOND_PIVOT_Y);
    secondHandSprite.pushImage(0, 0, SECOND_HAND_WIDTH, SECOND_HAND_HEIGHT, secondHandImage);
  }

  String auth = "...VOBINH...";
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString(auth, (tft.width() - tft.textWidth(auth)) / 2, tft.height() / 2);
  Serial.println("Setup done!");
  delay(1000);
}

void loop() {
  handleSerialCommands();

  if (currentMode == WIFI_UPLOAD_MODE) {
    wifi_manager_loop();
    drawWifiUploadScreen();

    ButtonAction action = getButtonAction();
    if (action == ACTION_LONG) {
      // Tắt WiFi, lưu cài đặt và khởi động lại về chế độ bình thường
      settings.wifiEnabled = false;
      saveSettings();
      Serial.println("Exiting WiFi mode. Restarting into normal mode...");
      delay(500);
      ESP.restart();
    }
    return;  // Dừng vòng lặp tại đây
  }

  if (currentMode == WEATHER_STATION_MODE) {
    // weather_station_enter();

    ButtonAction action = getButtonAction();
    Mode newMode = weather_station_loop(action);
    if (newMode != WEATHER_STATION_MODE) {
      settings.weatherEnabled = false;
      saveSettings();
      Serial.println("Exiting WiFi mode. Restarting into normal mode...");
      delay(500);
      ESP.restart();
    }
    return;  // Dừng vòng lặp tại đây
  }

  if (settings.bluetoothEnabled) {
    chronos_loop();
    NavigationBackground::tick();
    switch (chronos_get_requested_action()) {
      case CHRONOS_ACTION_SAVE_SETTINGS:
        Serial.println("Settings changed via Chronos, saving...");
        saveSettings();
        break;
      case CHRONOS_ACTION_RESET_CONFIG:
        Serial.println("Remote reset command received via Chronos.");
        if (SPIFFS.exists(CONFIG_FILE)) {
          SPIFFS.remove(CONFIG_FILE);
        }
        Serial.println("Restarting...");
        delay(1000);
        ESP.restart();
        break;
      case CHRONOS_ACTION_NONE:
      default:
        break;
    }
  }

  audio_loop();

  ButtonAction action = getButtonAction();

  if (action != ACTION_NONE) {
    lastUserInteractionTime = millis();
    if (isIdleVideoActive) {
      isIdleVideoActive = false;
      if (settings.soundEnabled) {
        audio_play_video_sound(currentVideoIndex);
      }
    }
  }

  bool isAlertEvent = settings.bluetoothEnabled && (chronos_is_ringing() || chronos_has_new_notification() || chronos_has_new_navigation());

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
        uint32_t idleTime = millis() - lastUserInteractionTime;
        if (idleTime > IDLE_TIMEOUT_MODE_SWITCH) {
          currentMode = WEATHER_MODE;
          lastUserInteractionTime = millis();
          break;
        }
        if (!isIdleVideoActive && idleTime > IDLE_TIMEOUT_VIDEO) {
          isIdleVideoActive = true;
          currentVideoIndex = IDLE_VIDEO_INDEX;
          currentFrame = 0;
          if (settings.soundEnabled) {
            audio_play_video_sound(currentVideoIndex);
          }
        }
        static int lastVideoIndex = -1;
        if (action == ACTION_LONG) {
          currentMode = SCROLL_TEXT_MODE;
          break;
        }
        if (action == ACTION_TRIPLE) {
          currentMode = MENU;
          menu_enter(true);
          break;
        }
        if (action == ACTION_SINGLE) {
          currentVideoIndex = (currentVideoIndex + 1) % NUM_FLASH_VIDEOS;
          currentFrame = 0;
        } else if (action == ACTION_DOUBLE) {
          currentVideoIndex = VIDEO_JUMP_TARGET;
          currentFrame = 0;
        }
        if (lastVideoIndex != currentVideoIndex) {
          if (settings.soundEnabled) {
            audio_set_autoplay(false);
            if (!isIdleVideoActive) {
              audio_play_video_sound(currentVideoIndex);
            }
          }
          lastVideoIndex = currentVideoIndex;
        }
        if (currentVideoIndex >= NUM_FLASH_VIDEOS)
          currentVideoIndex = 0;
        VideoInfo *currentVideo = flashVideoList[currentVideoIndex];
        // TJpgDec.setCallback(tft_output);
        const uint8_t *jpg_data = (const uint8_t *)pgm_read_ptr(&currentVideo->frames[currentFrame]);
        uint16_t jpg_size = pgm_read_word(&currentVideo->frames_size[currentFrame]);
        TJpgDec.drawJpg(0, 0, jpg_data, jpg_size);
        delay(settings.frameDelay);
        currentFrame++;
        if (currentFrame >= currentVideo->num_frames && !isIdleVideoActive) {
          currentFrame = 0;
          currentVideoIndex = (currentVideoIndex + 1) % NUM_FLASH_VIDEOS;
          if (settings.soundEnabled) {
            audio_stop();
          }
        } else if (currentFrame >= currentVideo->num_frames && isIdleVideoActive) {
          currentFrame = 0;
        }
        break;
      }

    case MENU:
      {

        Mode newMode = menu_handle_action(action);
        if (newMode != MENU) {
          if (!menu_manager_save_triggered()) {
            if (newMode == WIFI_UPLOAD_MODE) {
              settings.wifiEnabled = true;
              saveSettings();
              Serial.println("Entering WiFi mode. Restarting...");
              delay(1000);
              ESP.restart();
            }

            if (newMode == WEATHER_STATION_MODE) {
              settings.weatherEnabled = true;
              saveSettings();
              Serial.println("Entering WiFi mode. Restarting...");
              delay(1000);
              ESP.restart();
            }
            currentMode = newMode;
          } else {
            bool oldBluetoothSetting = settings.bluetoothEnabled;
            saveSettings();
            if (oldBluetoothSetting != settings.bluetoothEnabled) {
              ESP.restart();
            }
            loadSettings();
            tft.setRotation(settings.currentRotation);
            audio_set_volume(settings.volume);
            audio_set_autoplay(settings.musicAutoPlayNext);
          }
          currentMode = newMode;
          if (currentMode == SCROLL_TEXT_SETTINGS_MODE) {
            initScrollTextSettings();
          }
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
        bool alertDrawn = chronos_draw_alerts(action);
        if (action == ACTION_LONG && !alertDrawn) {
          isDisplayingAlert = false;
          currentMode = MENU;
          menu_enter();
          break;
        }
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
        if (action == ACTION_DOUBLE) {
          if (NUM_FLASH_VIDEOS > 0) {
            settings.currentWeatherIndex = (settings.currentWeatherIndex + 1) % NUM_FLASH_VIDEOS;
            saveSettings();
            currentBgFrame = 0;
          }
        } else if (action == ACTION_LONG) {
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
        }
        if (action == ACTION_LONG) {
          audio_set_autoplay(settings.musicAutoPlayNext);
          audio_play_music(selectedMusicItem);
          currentMode = MUSIC_PLAYER_MODE;
        }
        if (action == ACTION_TRIPLE) {
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
    case SCROLL_TEXT_MODE:
      {
        if (action == ACTION_LONG) {
          currentMode = PLAYING;
          break;
        }
        if (action == ACTION_DOUBLE) {
          currentMode = SCROLL_TEXT_SETTINGS_MODE;
          initScrollTextSettings();
        }
        drawScrollTextMode(reset_scroll_text_position);
        reset_scroll_text_position = false;
        break;
      }
    case SCROLL_TEXT_SETTINGS_MODE:
      {
        if (action == ACTION_SINGLE) {
          scrollTextSettingsSelectedItem = (scrollTextSettingsSelectedItem + 1) % NUM_SCROLL_TEXT_SETTINGS_ITEMS_CONST;
        } else if (action == ACTION_DOUBLE) {
          switch (scrollTextSettingsSelectedItem) {
            case 0:
              tempColorIndex = (tempColorIndex + 1) % numColorOptions;
              break;
            case 1:
              tempSpeedIndex = (tempSpeedIndex + 1) % numSpeedOptions;
              break;
          }
        } else if (action == ACTION_LONG) {
          switch (scrollTextSettingsSelectedItem) {
            case 2:
              settings.scrollText.textColor = colorOptions[tempColorIndex];
              settings.scrollText.speed = speedOptions[tempSpeedIndex];
              currentMode = SCROLL_TEXT_MODE;
              reset_scroll_text_position = true;
              break;
            case 3:
              settings.scrollText.textColor = colorOptions[tempColorIndex];
              settings.scrollText.speed = speedOptions[tempSpeedIndex];
              saveSettings();
              currentMode = MENU;
              menu_enter();
              break;
          }
        }
        drawScrollTextSettingsScreen();
        break;
      }
  }
}

void populateMenuText(JsonDocument &doc) {
  JsonObject menu_vi = doc.createNestedObject("menu_vi");
  menu_vi["tab_setting"] = FPSTR(TAB_SETTING_VI);
  menu_vi["tab_mode"] = FPSTR(TAB_MODE_VI);
  JsonObject setting_vi = menu_vi.createNestedObject("setting");
  for (int i = 0; i < NUM_SETTING_ITEMS_CONST; i++) {
    setting_vi["item" + String(i)] = FPSTR(pgm_read_ptr(&setting_items_vi[i]));
  }
  JsonObject mode_vi = menu_vi.createNestedObject("mode");
  for (int i = 0; i < NUM_MODE_ITEMS_CONST; i++) {
    mode_vi["item" + String(i)] = FPSTR(pgm_read_ptr(&mode_items_vi[i]));
  }
  JsonObject scroll_text_settings_vi = menu_vi.createNestedObject("scroll_text_settings");
  for (int i = 0; i < NUM_SCROLL_TEXT_SETTINGS_ITEMS_CONST; i++) {
    scroll_text_settings_vi["item" + String(i)] = FPSTR(pgm_read_ptr(&scroll_text_settings_items_vi[i]));
  }

  JsonObject menu_en = doc.createNestedObject("menu_en");
  menu_en["tab_setting"] = FPSTR(TAB_SETTING_EN);
  menu_en["tab_mode"] = FPSTR(TAB_MODE_EN);
  JsonObject setting_en = menu_en.createNestedObject("setting");
  for (int i = 0; i < NUM_SETTING_ITEMS_CONST; i++) {
    setting_en["item" + String(i)] = FPSTR(pgm_read_ptr(&setting_items_en[i]));
  }
  JsonObject mode_en = menu_en.createNestedObject("mode");
  for (int i = 0; i < NUM_MODE_ITEMS_CONST; i++) {
    mode_en["item" + String(i)] = FPSTR(pgm_read_ptr(&mode_items_en[i]));
  }
  JsonObject scroll_text_settings_en = menu_en.createNestedObject("scroll_text_settings");
  for (int i = 0; i < NUM_SCROLL_TEXT_SETTINGS_ITEMS_CONST; i++) {
    scroll_text_settings_en["item" + String(i)] = FPSTR(pgm_read_ptr(&scroll_text_settings_items_en[i]));
  }
}

// --- HÀM LOAD/SAVE SETTINGS ---
void saveSettings() {
  Serial.println("Saving settings to SPIFFS...");
  File configFile = SPIFFS.open(CONFIG_FILE, "w");
  if (!configFile)
    return;

  StaticJsonDocument<2048> doc;
  doc["frameDelay"] = settings.frameDelay;
  doc["currentRotation"] = settings.currentRotation;
  doc["language"] = settings.currentLang;
  doc["notificationTimeout"] = settings.notificationTimeout;
  doc["marqueeSpeed"] = settings.marqueeSpeed;
  doc["currentAnalogFaceIndex"] = settings.currentAnalogFaceIndex;
  doc["currentWeatherIndex"] = settings.currentWeatherIndex;
  doc["soundEnabled"] = settings.soundEnabled;
  doc["volume"] = settings.volume;
  doc["musicAutoPlayNext"] = settings.musicAutoPlayNext;
  doc["displayShape"] = (settings.displayShape == SHAPE_SQUARE) ? "square" : "round";
  doc["bluetoothEnabled"] = settings.bluetoothEnabled;
  doc["wifiEnabled"] = settings.wifiEnabled;
  doc["weatherEnabled"] = settings.weatherEnabled;

  // Lưu cài đặt Weather Station
  doc["stationSsid"] = settings.stationSsid;
  doc["stationPassword"] = settings.stationPassword;
  doc["owmApiKey"] = settings.owmApiKey;
  doc["owmCityId"] = settings.owmCityId;
  doc["latitude"] = settings.latitude;
  doc["longitude"] = settings.longitude;
  doc["gmtOffsetHours"] = settings.gmtOffsetHours;

  JsonObject scrollText = doc.createNestedObject("scrollText");
  scrollText["text"] = settings.scrollText.text;
  scrollText["speed"] = settings.scrollText.speed;
  scrollText["textColor"] = settings.scrollText.textColor;

  JsonArray tracks = doc.createNestedArray("trackList");
  const std::vector<String> &trackListRef = audio_get_tracklist_ref();
  for (const String &track : trackListRef) {
    tracks.add(track);
  }

  populateMenuText(doc);

  if (serializeJson(doc, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  configFile.close();
}

void loadSettings() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed.");
  }

  bool success = false;
  File configFile = SPIFFS.open(CONFIG_FILE, "r");
  if (configFile) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    if (!error) {

      Serial.println("loadSettings");

      settings.frameDelay = doc["frameDelay"] | 20;
      settings.currentRotation = doc["currentRotation"] | 3;
      settings.currentLang = doc["language"] | "vi";
      settings.notificationTimeout = doc["notificationTimeout"] | 5;
      settings.marqueeSpeed = doc["marqueeSpeed"] | 35;
      settings.currentAnalogFaceIndex = doc["currentAnalogFaceIndex"] | 0;
      settings.currentWeatherIndex = doc["currentWeatherIndex"] | 0;
      settings.soundEnabled = doc["soundEnabled"] | false;
      settings.volume = doc["volume"] | 10;
      settings.musicAutoPlayNext = doc["musicAutoPlayNext"] | true;
      settings.bluetoothEnabled = doc["bluetoothEnabled"] | true;
      settings.wifiEnabled = doc["wifiEnabled"] | false;
      settings.weatherEnabled = doc["weatherEnabled"] | false;
      String shapeStr = doc["displayShape"] | "square";
      settings.displayShape = (shapeStr == "round") ? SHAPE_ROUND : SHAPE_SQUARE;

      JsonObject scrollText = doc["scrollText"];
      settings.scrollText.text = scrollText["text"] | "Chào mừng đến với Mochi Watch!";
      settings.scrollText.speed = scrollText["speed"] | 35;
      settings.scrollText.textColor = scrollText["textColor"] | TFT_WHITE;

      // Tải cài đặt Weather Station
      settings.stationSsid = doc["stationSsid"] | "";
      settings.stationPassword = doc["stationPassword"] | "";
      settings.owmApiKey = doc["owmApiKey"] | "";
      settings.owmCityId = doc["owmCityId"] | "1566083";
      settings.latitude = doc["latitude"] | "";
      settings.longitude = doc["longitude"] | "";
      settings.gmtOffsetHours = doc["gmtOffsetHours"] | 7;
      settings.language = doc["language"] | "vi";

      success = true;
    }
    configFile.close();
  }

  if (!success) {
    Serial.println("Config not loaded or invalid. Creating default.");
    settings.frameDelay = 20;
    settings.currentRotation = 0;
    settings.currentLang = "vi";
    settings.notificationTimeout = 5;
    settings.marqueeSpeed = 35;
    settings.currentAnalogFaceIndex = 0;
    settings.currentWeatherIndex = 0;
    settings.soundEnabled = false;
    settings.volume = 15;
    settings.musicAutoPlayNext = true;
    settings.bluetoothEnabled = true;
    settings.wifiEnabled = false;
    settings.weatherEnabled = false;
    settings.displayShape = SHAPE_SQUARE;

    settings.scrollText.text = "Hello! Dasai Mochi.";
    settings.scrollText.speed = 35;
    settings.scrollText.textColor = TFT_YELLOW;

    // Giá trị mặc định cho Weather Station
    settings.stationSsid = "";
    settings.stationPassword = "";
    settings.owmApiKey = "";
    settings.owmCityId = "1566083";
    settings.latitude = "";
    settings.longitude = "";
    settings.gmtOffsetHours = 7;
    settings.language = "vi";

    saveSettings();
  }
}

void loadAudioTracklist() {
  File configFile = SPIFFS.open(CONFIG_FILE, "r");
  if (configFile) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    if (!error) {
      JsonArray tracks = doc["trackList"];
      std::vector<String> loadedTracks;
      for (JsonVariant v : tracks) {
        loadedTracks.push_back(v.as<String>());
      }
      audio_update_tracklist(loadedTracks);
    }
    configFile.close();
  } else {
    // If no config file, ensure tracklist is empty
    audio_update_tracklist({});
  }
}

void loadUiStrings() {
  bool success = false;
  File configFile = SPIFFS.open(CONFIG_FILE, "r");
  if (configFile) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    if (!error) {
      menu_load_strings(doc.as<JsonObject>());

      JsonObject menu_text = (settings.currentLang == "vi") ? doc["menu_vi"] : doc["menu_en"];
      if (menu_text) {
        JsonObject scroll_settings_text = menu_text["scroll_text_settings"];
        const char *const *items_pgm = (settings.currentLang == "vi") ? scroll_text_settings_items_vi : scroll_text_settings_items_en;
        for (int i = 0; i < NUM_SCROLL_TEXT_SETTINGS_ITEMS_CONST; i++) {
          JsonVariant item = scroll_settings_text["item" + String(i)];
          if (item.isNull()) {
            scrollTextSettingsItems[i] = FPSTR(pgm_read_ptr(&items_pgm[i]));
          } else {
            scrollTextSettingsItems[i] = item.as<String>();
          }
        }
      }
      success = true;
    }
    configFile.close();
  }

  if (!success) {
    // If config file fails, load default strings from PROGMEM
    StaticJsonDocument<1024> default_doc;
    populateMenuText(default_doc);
    menu_load_strings(default_doc.as<JsonObject>());

    const char *const *items_pgm = (settings.currentLang == "vi") ? scroll_text_settings_items_vi : scroll_text_settings_items_en;
    for (int i = 0; i < NUM_SCROLL_TEXT_SETTINGS_ITEMS_CONST; i++) {
      scrollTextSettingsItems[i] = FPSTR(pgm_read_ptr(&items_pgm[i]));
    }
  }
}
