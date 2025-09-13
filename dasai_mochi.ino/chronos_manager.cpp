#include "chronos_manager.h"
#include <ChronosESP32.h>
#include <vector>
#include <time.h>
#include "ui_utils.h"
#include "audio_manager.h"
#include "navigation_background.h"

// --- CÁC BIẾN TĨNH ---
static TFT_eSPI *_tft;
static TFT_eSprite *_sprite;
static MakeFont *_font;
static AppSettings *_settings;
static ChronosESP32 Chronos("Mochi Watch");

// *** BIẾN MỚI ĐỂ QUẢN LÝ KÍCH HOẠT ANIMATION ***
// static NavInstructionType last_instruction_sent_to_anim = NAV_UNKNOWN;
static String last_nav_directions_text = "";
static bool has_played_turn_anim = false;
static uint32_t last_nav_crc = 0;
static bool is_arrived = false;
static int current_nav_speed = 4;
static NavDirectionID latest_nav_direction_id = DirectionNone;

extern TFT_eSprite *fontTargetSprite;

static Notification latestNotification;
static Navigation latestNavigation;
static String callerInfo;
static bool hasNewNotification = false;
static bool hasNewNavigation = false;
static bool isConnected = false;
static bool isRinging = false;
static unsigned long notificationStartTime = 0;
static bool hasScrolledOnce = false;
static unsigned long scrollFinishedTime = 0;
static uint32_t nav_icon_crc = 0xFFFFFFFF;
static bool navIconVisible = true;
static unsigned long lastNavIconBlinkTime = 0;
static std::vector<String> wrappedMessageLines;
static int messageScrollLine = 0;
static unsigned long lastMessageScrollTime = 0;
static bool isNotificationScrolling = false;
static bool timeIsSynced = false;
static uint8_t rtc_hour, rtc_minute, rtc_second, rtc_day, rtc_month;
static uint16_t rtc_year;

// *** BIẾN MỚI ĐỂ LƯU DỮ LIỆU THỜI TIẾT ***
static WeatherData latestWeather;
static bool hasWeatherData = false;
static ChronosAction requested_action = CHRONOS_ACTION_NONE;

enum NavTimeOverride { TIME_AUTO,
                       TIME_MANUAL_DAY,
                       TIME_MANUAL_NIGHT };
static NavTimeOverride navTimeOverride = TIME_AUTO;

// *** BIẾN MỚI CHO HIỆU ỨNG CHỮ CHẠY DỌC ***
struct RoadText {
  String text;  // Mỗi đối tượng chữ sẽ có nội dung riêng
  int y;
  bool active;
  bool is_left_lane;
};
static const int MAX_ROAD_TEXTS = 6;  // Số lượng chữ hiển thị đồng thời
static RoadText road_texts[MAX_ROAD_TEXTS];
static unsigned long last_text_spawn_time = 0;
static unsigned int text_spawn_interval = 2000;  // ms giữa mỗi lần xuất hiện
static String current_nav_title_for_anim = "";
static int nav_anim_speed = 4;  // Tốc độ cuộn của chữ


static NavDirectionID map_crc_to_direction_id(uint32_t crc) {
  Serial.println("crc: ");
  Serial.printf("0x%04X\n", crc);
  if (crc == 0xE15D3531 || crc == 0x9F417DA0) return DirectionLeft;
  if (crc == 0xBC7CAA8A || crc == 0x65D324CE) return DirectionRight;
  if (crc == 0xE324CE84 || crc == 0x77A464B0) return DirectionStraight;
  if (crc == 0xF3CCE64A) return DirectionRoundaboutRSE;
  if (crc == 0x4E492E6) return DirectionRoundaboutRE;
  if (crc == 0x56910207) return DirectionRoundaboutRNE;
  if (crc == 0xF98F61) return DirectionRoundaboutRN;
  if (crc == 0x6F6DF52A || crc == 0x127B26BB) return DirectionEasyRight;
  if (crc == 0xB18135CE) return DirectionUTurnRight;
  // Thêm các giá trị CRC khác ở đây nếu cần
  return DirectionNone;
  // 0x28058C45
}

static NavInstructionType get_nav_instruction_type(NavDirectionID dir_id) {
  switch (dir_id) {
    case DirectionEasyLeft:
    case DirectionKeepLeft:
    case DirectionLeft:
    case DirectionExitLeft: return NAV_TURN_LEFT;
    case DirectionEasyRight:
    case DirectionKeepRight:
    case DirectionRight:
    case DirectionExitRight: return NAV_TURN_RIGHT;
    case DirectionSharpLeft: return NAV_SHARP_LEFT;
    case DirectionSharpRight: return NAV_SHARP_RIGHT;
    case DirectionStraight:
    case DirectionFollow: return NAV_STRAIGHT;
    case DirectionEnd: return NAV_ARRIVED;
    case DirectionUTurnRight:  // *** THÊM LOGIC XỬ LÝ QUAY ĐẦU ***
    case DirectionUTurnLeft:
      return NAV_U_TURN;
    default:
      if (dir_id >= DirectionRoundaboutRSE && dir_id <= DirectionRoundaboutLS) return NAV_ROUNDABOUT;
      return NAV_UNKNOWN;
  }
}

void chronos_debug_trigger_animation() {
  // Chỉ hoạt động khi đang ở chế độ chỉ đường
  if (!hasNewNavigation) return;

  static int debug_anim_counter = 0;
  debug_anim_counter = (debug_anim_counter + 1) % 2;  // Chuyển đổi giữa 0 và 1

  if (debug_anim_counter == 0) {
    NavigationBackground::triggerAnimation(NAV_TURN_LEFT);
  } else {
    NavigationBackground::triggerAnimation(NAV_TURN_RIGHT);
  }
}

static void handle_navigation_animation(float distance) {
    const int TURN_TRIGGER_DISTANCE = 30;
    NavInstructionType currentInstruction = get_nav_instruction_type(latest_nav_direction_id);

    if (latestNavigation.directions != last_nav_directions_text) {
        has_played_turn_anim = false;
        last_nav_directions_text = latestNavigation.directions;
    }

    if (distance < TURN_TRIGGER_DISTANCE && !has_played_turn_anim) {
        // *** THÊM NAV_U_TURN VÀO ĐIỀU KIỆN KÍCH HOẠT ***
        if (currentInstruction == NAV_TURN_LEFT || currentInstruction == NAV_TURN_RIGHT || currentInstruction == NAV_U_TURN) {
            NavigationBackground::triggerAnimation(currentInstruction);
            has_played_turn_anim = true;
        }
    }
}

// --- CÁC HÀM NỘI BỘ ---
static void syncTimeToRTC() {
  if (!Chronos.isConnected())
    return;
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
}

static void getTimeFromRTC() {
  if (!timeIsSynced)
    return;
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

static void wrapMessage(String text) {
  wrappedMessageLines.clear();
  if (text.length() == 0)
    return;
  const int maxWidth = _tft->width() - 40;
  String currentLine = "";
  String currentWord = "";
  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    if (c == ' ' || c == '\n') {
      if (_font->getLength(currentLine + currentWord) > maxWidth) {
        wrappedMessageLines.push_back(currentLine);
        currentLine = currentWord + " ";
      } else {
        currentLine += currentWord + " ";
      }
      currentWord = "";
      if (c == '\n') {
        wrappedMessageLines.push_back(currentLine);
        currentLine = "";
      }
    } else {
      currentWord += c;
    }
  }
  currentLine += currentWord;
  wrappedMessageLines.push_back(currentLine);
}

// *** HÀM MỚI ĐỂ CẬP NHẬT TRẠNG THÁI ANIMATION ***
static void chronos_tick_nav_animations() {
  if (!hasNewNavigation) {
    // Dọn dẹp animation khi không còn chỉ đường
    for (int i = 0; i < MAX_ROAD_TEXTS; i++) {
      road_texts[i].active = false;
    }
    return;
  }

  // Cập nhật vị trí các chữ đang có
  for (int i = 0; i < MAX_ROAD_TEXTS; i++) {
    if (road_texts[i].active) {
      road_texts[i].y += nav_anim_speed;
      if (road_texts[i].y > _tft->height() + 100) {
        road_texts[i].active = false;
      }
    }
  }

  // Tạo chữ mới với nội dung hiện tại
  if (millis() - last_text_spawn_time > text_spawn_interval) {
    last_text_spawn_time = millis();
    for (int i = 0; i < MAX_ROAD_TEXTS; i++) {
      if (!road_texts[i].active) {
        road_texts[i].active = true;
        road_texts[i].y = 60;
        road_texts[i].text = latestNavigation.title;  // Gán nội dung tại thời điểm tạo

        static bool spawn_on_left = true;
        road_texts[i].is_left_lane = spawn_on_left;
        spawn_on_left = !spawn_on_left;

        break;
      }
    }
  }
}


// --- CÁC HÀM CALLBACK ---
static void connectionCallback(bool state) {
  isConnected = state;
  if (state)
    syncTimeToRTC();
}
static void ringerCallback(String caller, bool state) {
  callerInfo = caller;
  isRinging = state;
}
static void notificationCallback(Notification notification) {
  if (notification.title == "set_text") {
    _settings->scrollText.text = notification.message;
    requested_action = CHRONOS_ACTION_SAVE_SETTINGS;
    return;  // Không hiển thị thông báo này
  } else if (notification.title == "set_tracks") {
    String trackNamesStr = notification.message;
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
    requested_action = CHRONOS_ACTION_SAVE_SETTINGS;
    return;  // Không hiển thị thông báo này
  } else if (notification.title == "reset_config") {
    requested_action = CHRONOS_ACTION_RESET_CONFIG;
    return;  // Không hiển thị thông báo này
  }

  latestNotification = notification;
  hasNewNotification = true;
  hasNewNavigation = false;
  wrapMessage(latestNotification.message);

  notificationStartTime = millis();
  lastMessageScrollTime = millis();
  messageScrollLine = 0;
  hasScrolledOnce = false;
  scrollFinishedTime = 0;
  isNotificationScrolling = (wrappedMessageLines.size() > 7);
}

static void configCallback(Config config, uint32_t a, uint32_t b) {
  switch (config) {
    case CF_NAV_DATA:
      if (a) {
        if (!hasNewNavigation) {  // Bắt đầu một chuyến đi mới
          NavigationBackground::reset();
          is_arrived = false;
          current_nav_speed = 4;
        }
        latestNavigation = Chronos.getNavigation();
        hasNewNavigation = true;
        hasNewNotification = false;
      } else {
        hasNewNavigation = false;
        navTimeOverride = TIME_AUTO;
        last_nav_directions_text = "";
      }
      break;
    case CF_NAV_ICON:
      if (a == 2) {
        latest_nav_direction_id = map_crc_to_direction_id(b);
        Serial.println("latest_nav_direction_id: ");
        Serial.println(latest_nav_direction_id);
        Serial.println("latest_id: ");
        Serial.println(b);
        Navigation tempNav = Chronos.getNavigation();
        if (nav_icon_crc != tempNav.iconCRC) {
          nav_icon_crc = tempNav.iconCRC;
          latestNavigation = tempNav;
          hasNewNavigation = true;

          // Kiểm tra xem có phải là icon "Đã đến" không
          String dirs = latestNavigation.directions;
          dirs.toLowerCase();
          if (dirs.indexOf("đã đến") > -1 || dirs.indexOf("arrived") > -1) {
            is_arrived = true;
          }
        }
      }
      break;
    // *** THÊM LOGIC XỬ LÝ THỜI TIẾT ***
    case CF_WEATHER:
      Serial.println("Weather received");
      if (a > 0) {  // Có dữ liệu mới
        Weather w = Chronos.getWeatherAt(0);
        latestWeather.currentTemp = w.temp;
        latestWeather.highTemp = w.high;
        latestWeather.lowTemp = w.low;
        latestWeather.icon = w.icon;
        latestWeather.pressure = w.pressure;
        latestWeather.uv = w.uv;

        hasWeatherData = true;
      }
      if (b) {  // Có tên thành phố
        String city = Chronos.getWeatherCity();
        latestWeather.city = city;
      }
      break;
    default:
      break;
  }
}

// --- CÁC HÀM CÔNG KHAI ---
void chronos_init(TFT_eSPI *tft, TFT_eSprite *sprite, MakeFont *font, AppSettings *settings) {
  _tft = tft;
  _sprite = sprite;
  _font = font;
  _settings = settings;
  Chronos.setConnectionCallback(connectionCallback);
  Chronos.setNotificationCallback(notificationCallback);
  Chronos.setRingerCallback(ringerCallback);
  Chronos.setConfigurationCallback(configCallback);
  Chronos.begin();
}

void chronos_loop() {
  Chronos.loop();
  getTimeFromRTC();
  chronos_tick_nav_animations();
  // *** LOGIC GIẢM TỐC KHI ĐÃ ĐẾN NƠI ***
  if (hasNewNavigation && is_arrived) {
    if (current_nav_speed > 0) {
      static unsigned long last_slowdown = 0;
      if (millis() - last_slowdown > 200) {  // Giảm tốc từ từ
        last_slowdown = millis();
        current_nav_speed--;
      }
    }
  } else {
    current_nav_speed = 4;  // Tốc độ bình thường
  }
  NavigationBackground::setSpeed(current_nav_speed);
}

bool chronos_draw_alerts(ButtonAction action) {
  if (isRinging) {
    _sprite->fillSprite(TFT_BLACK);
    _font->print((_tft->width() - _font->getLength("CUỘC GỌI ĐẾN")) / 2, 30, "CUỘC GỌI ĐẾN", TFT_WHITE, TFT_BLACK);
    _font->print((_tft->width() - _font->getLength(callerInfo)) / 2, _tft->height() / 2, callerInfo, TFT_WHITE, TFT_BLACK);
    _sprite->fillCircle(_tft->width() / 4, _tft->height() - 50, 30, TFT_GREEN);
    _sprite->fillCircle(_tft->width() * 3 / 4, _tft->height() - 50, 30, TFT_RED);
    _sprite->pushSprite(0, 0);
    return true;
  }

  if (hasNewNavigation) {
    // 1. Draw the dynamic driving background
    if (action == ACTION_LONG) {
      navTimeOverride = (NavTimeOverride)((navTimeOverride + 1) % 3);  // Chuyển vòng qua 3 trạng thái
    }

    if (action == ACTION_DOUBLE) {
      static int debug_anim_counter = 0;
      debug_anim_counter = (debug_anim_counter + 1) % 3;  // Chuyển đổi giữa 0 và 1
      Serial.println("debug_anim_counter: " + debug_anim_counter);
      if (debug_anim_counter == 0) {
        NavigationBackground::triggerAnimation(NAV_TURN_LEFT);
      } else if (debug_anim_counter == 1) {
        NavigationBackground::triggerAnimation(NAV_TURN_LEFT);
      } else {
        NavigationBackground::triggerAnimation(NAV_U_TURN);
      }
    }

    // Xác định giờ hiệu lực để vẽ
    int effective_hour;
    if (navTimeOverride == TIME_AUTO) {
      effective_hour = chronos_get_hour();
    } else if (navTimeOverride == TIME_MANUAL_NIGHT) {
      effective_hour = 20;  // Giả lập ban đêm
    } else {                // TIME_MANUAL_DAY
      effective_hour = 12;  // Giả lập ban ngày
    }

    // 1. Vẽ nền động với giờ đã được xác định
    NavigationBackground::draw(_sprite, effective_hour);

    // *** BẮT ĐẦU VẼ CHỮ XOAY DỌC (PHIÊN BẢN CẢI TIẾN) ***
    if (hasNewNavigation) {
      _sprite->setFreeFont(NULL);

      TFT_eSprite textSprite(_tft);
      TFT_eSprite canvasSprite(_tft);
      String lastDrawnText = "";

      for (int i = 0; i < MAX_ROAD_TEXTS; i++) {
        if (road_texts[i].active && road_texts[i].text.length() > 0) {
          String currentText = road_texts[i].text;

          // Chỉ tạo lại sprite khi nội dung văn bản thay đổi
          if (currentText != lastDrawnText) {
            if (textSprite.created()) textSprite.deleteSprite();
            if (canvasSprite.created()) canvasSprite.deleteSprite();

            int text_w = _sprite->textWidth(currentText, 4);  // DÙNG FONT 4
            int text_h = _sprite->fontHeight(4);              // DÙNG FONT 4

            textSprite.createSprite(text_w, text_h);
            textSprite.setTextColor(TFT_WHITE, TFT_BLUE);
            textSprite.setTextDatum(MC_DATUM);
            textSprite.drawString(currentText, text_w / 2, text_h / 2, 4);  // DÙNG FONT 4

            canvasSprite.createSprite(text_h, text_w);
            textSprite.setPivot(text_w / 2, text_h / 2);
            lastDrawnText = currentText;
          }

          if (!textSprite.created()) continue;

          const int ROAD_W = 150;
          const int ROAD_X = (_tft->width() - ROAD_W) / 2;
          const int LANE_W = ROAD_W / 3;
          int lane_x = road_texts[i].is_left_lane ? (ROAD_X + LANE_W / 2) : (ROAD_X + LANE_W * 2 + LANE_W / 2);
          int rotation = road_texts[i].is_left_lane ? -90 : 90;

          canvasSprite.fillSprite(TFT_BLUE);
          textSprite.pushRotated(&canvasSprite, rotation, TFT_BLUE);
          canvasSprite.pushToSprite(_sprite, lane_x - canvasSprite.width() / 2, road_texts[i].y - canvasSprite.height() / 2, TFT_BLUE);
        }
      }

      if (textSprite.created()) textSprite.deleteSprite();
      if (canvasSprite.created()) canvasSprite.deleteSprite();

      fontTargetSprite = _sprite;  // KHÔI PHỤC CON TRỎ
    }

    // --- START DRAWING NEW UI OVERLAY ---

    // 2. Get data and calculate blinking/color
    float distanceInMeters = 0;
    String distStr = latestNavigation.title;
    distStr.trim();
    distStr.replace(",", ".");
    if (distStr.indexOf("k") > -1) distanceInMeters = distStr.toFloat() * 1000;
    else distanceInMeters = distStr.toFloat();

    // *** HOÀN THIỆN LOGIC KÍCH HOẠT ANIMATION ***
    handle_navigation_animation(distanceInMeters);

    uint16_t iconColor = TFT_WHITE;
    bool shouldBlink = false;
    if (distanceInMeters > 0 && distanceInMeters < 50) {
      iconColor = TFT_RED;
      shouldBlink = true;
    } else if (distanceInMeters > 0 && distanceInMeters < 100) {
      shouldBlink = true;
    }

    if (shouldBlink) {
      if (millis() - lastNavIconBlinkTime > 500) {
        lastNavIconBlinkTime = millis();
        navIconVisible = !navIconVisible;
      }
    } else {
      navIconVisible = true;
    }

    // 3. Draw the main navigation icon in the center
    if (nav_icon_crc != 0xFFFFFFFF && navIconVisible) {
      const int icon_display_size = 96;
      const int bitmap_native_size = 48;
      const int pixel_size = icon_display_size / bitmap_native_size;

      const int base_x = (_tft->width() - icon_display_size) / 2;
      const int base_y = 60;  // Y position for the icon

      for (int y = 0; y < bitmap_native_size; y++) {
        for (int x = 0; x < bitmap_native_size; x++) {
          int byte_index = (y * bitmap_native_size + x) / 8;
          int bit_pos = 7 - (x % 8);
          bool px_on = (latestNavigation.icon[byte_index] >> bit_pos) & 0x01;
          if (px_on) {
            _sprite->fillRect(base_x + x * pixel_size, base_y + y * pixel_size, pixel_size, pixel_size, iconColor);
          }
        }
      }
    }

    // 4. Draw the top info panel
    int panelY = 5;
    int panelHeight = 50;
    _sprite->fillRoundRect(5, panelY, _tft->width() - 10, panelHeight, 8, TFT_BLUE);
    _sprite->drawRoundRect(5, panelY, _tft->width() - 10, panelHeight, 8, TFT_WHITE);

    // Center the title text
    String titleStr = latestNavigation.title;
    int title_w = _font->getLength(titleStr);
    int title_x = (_tft->width() - title_w) / 2;
    drawMarqueeText(_sprite, _font, titleStr, title_x, panelY + 5, title_w, TFT_WHITE, 0, false, 0);

    // Center or scroll the directions text
    String dirStr = latestNavigation.directions;
    int dir_w = _font->getLength(dirStr);
    int panel_inner_width = _tft->width() - 20;
    if (dir_w > panel_inner_width) {
      drawMarqueeText(_sprite, _font, dirStr, 10, panelY + 28, panel_inner_width, TFT_WHITE, 0, true, 35);
    } else {
      int dir_x = (_tft->width() - dir_w) / 2;
      drawMarqueeText(_sprite, _font, dirStr, dir_x, panelY + 28, dir_w, TFT_WHITE, 0, false, 0);
    }

    // 5. Draw the side info signs
    int signCenterY = 170;

    // Distance Sign (Left)
    int distSignX = 40;
    int signRadius = 28;
    _sprite->fillCircle(distSignX, signCenterY, signRadius, TFT_WHITE);
    _sprite->fillCircle(distSignX, signCenterY, signRadius - 3, TFT_RED);
    _sprite->fillCircle(distSignX, signCenterY, signRadius - 5, TFT_WHITE);
    _sprite->setTextColor(TFT_BLACK);
    _sprite->setTextDatum(MC_DATUM);
    _sprite->drawString(latestNavigation.distance, distSignX + 1, signCenterY, 2);
    _sprite->drawString(latestNavigation.distance, distSignX, signCenterY, 2);

    // ETA Sign (Right)
    int etaSignX = _tft->width() - 40;
    _sprite->fillCircle(etaSignX, signCenterY, signRadius, TFT_WHITE);
    _sprite->fillCircle(etaSignX, signCenterY, signRadius - 3, TFT_YELLOW);
    _sprite->fillCircle(etaSignX, signCenterY, signRadius - 5, TFT_WHITE);
    String etaStr = extractTimeSafe(latestNavigation.eta);
    if (etaStr.length() == 0) { etaStr = latestNavigation.eta; }
    _sprite->setTextColor(TFT_BLACK);
    _sprite->setTextDatum(MC_DATUM);
    _sprite->drawString(etaStr, etaSignX, signCenterY, 2);
    _sprite->drawString(etaStr, etaSignX + 1, signCenterY, 2);

    _sprite->pushSprite(0, 0);
    return true;
  }

  if (hasNewNotification) {
    bool shouldHide = false;
    const int MAX_DISPLAY_TIME = 10000;

    if (millis() - notificationStartTime > MAX_DISPLAY_TIME) {
      shouldHide = true;
    }

    int maxLines = 7;
    if (isNotificationScrolling) {
      if (!hasScrolledOnce) {
        if (millis() - lastMessageScrollTime > 1500) {
          lastMessageScrollTime = millis();
          messageScrollLine++;
          if (messageScrollLine > wrappedMessageLines.size() - maxLines) {
            hasScrolledOnce = true;
            scrollFinishedTime = millis();
            messageScrollLine = 0;
          }
        }
      } else {
        if (millis() - scrollFinishedTime > _settings->notificationTimeout * 1000) {
          shouldHide = true;
        }
      }
    } else {
      if (millis() - notificationStartTime > _settings->notificationTimeout * 1000) {
        shouldHide = true;
      }
    }

    if (shouldHide) {
      hasNewNotification = false;
      return false;
    }

    _sprite->fillSprite(TFT_BLACK);
    const int boxX = 5, boxY = 30, boxW = 230, boxH = 180, cornerRadius = 10;
    const int textPadding = 8, scrollbarWidth = 6;
    const int lineHeight = 22;

    _sprite->drawRoundRect(boxX, boxY, boxW, boxH, cornerRadius, TFT_CYAN);

    String title = latestNotification.title;
    String appName = latestNotification.app;
    if (_settings->displayShape == SHAPE_ROUND) {
      _font->print((_tft->width() - _font->getLength(title)) / 2, 4, title, TFT_CYAN, TFT_BLACK);
      _font->print((_tft->width() - _font->getLength(appName)) / 2, _tft->height() - 23, appName, TFT_CYAN, TFT_BLACK);
    } else {
      _font->print(15, 4, title, TFT_CYAN, TFT_BLACK);
      _font->print(_tft->width() - _font->getLength(appName) - 15, _tft->height() - 23, appName, TFT_CYAN, TFT_BLACK);
    }

    for (int i = 0; i < maxLines; i++) {
      int lineIndex = messageScrollLine + i;
      if (lineIndex < wrappedMessageLines.size()) {
        _font->print(boxX + textPadding, boxY + textPadding + i * lineHeight, wrappedMessageLines[lineIndex], TFT_WHITE, TFT_BLACK);
      }
    }

    if (isNotificationScrolling) {
      int menuHeight = boxH - 10;
      int scrollbarX = boxX + boxW - scrollbarWidth - 5;
      _sprite->drawRect(scrollbarX, boxY + 5, scrollbarWidth, menuHeight, TFT_DARKGREY);

      float thumbHeight = (float)maxLines / wrappedMessageLines.size() * menuHeight;
      float thumbY = boxY + 5 + ((float)messageScrollLine / wrappedMessageLines.size() * menuHeight);
      _sprite->fillRoundRect(scrollbarX, thumbY, scrollbarWidth, thumbHeight, 2, TFT_WHITE);
    }

    _sprite->pushSprite(0, 0);
    return true;
  }

  return false;
}

// --- CÁC HÀM GETTER ---
bool chronos_is_time_synced() {
  return timeIsSynced;
}
uint8_t chronos_get_hour() {
  return rtc_hour;
}
uint8_t chronos_get_minute() {
  return rtc_minute;
}
uint8_t chronos_get_second() {
  return rtc_second;
}
uint8_t chronos_get_day() {
  return rtc_day;
}
uint8_t chronos_get_month() {
  return rtc_month;
}
uint16_t chronos_get_year() {
  return rtc_year;
}
bool chronos_is_ringing() {
  return isRinging;
}
bool chronos_has_new_notification() {
  return hasNewNotification;
}
bool chronos_has_new_navigation() {
  return hasNewNavigation;
}

// *** TRIỂN KHAI CÁC HÀM MỚI ***
bool chronos_has_weather_data() {
  return hasWeatherData;
}

WeatherData chronos_get_weather() {
  return latestWeather;
}

ChronosAction chronos_get_requested_action() {
  ChronosAction action = requested_action;
  requested_action = CHRONOS_ACTION_NONE;  // Reset lại sau khi đã lấy
  return action;
}
