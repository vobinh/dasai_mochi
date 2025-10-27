#include "weather_station.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include "chronos_manager.h"
#include "Digitall0132pt7b.h"
#include "Digitall0124pt7b.h"
#include "weather_icons.h"
#include "ui_utils.h"

#include "SPIFFS.h"
#include <FS.h>
#include <TJpg_Decoder.h>
#include <limits>
#include <vector>

extern TFT_eSprite *jpegSpriteTarget;
extern bool sprite_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
// *** SỬA LỖI: Bỏ comment dòng extern này ***
extern bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

// --- CẤU HÌNH NTP ---
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (7 * 3600)
#define DAYLIGHT_OFFSET_SEC 0


// *** THÊM MỚI: Định nghĩa cấu trúc video cục bộ ***
struct FrameInfo {
  uint32_t offset;
  uint32_t size;
};

struct DynamicVideo {
  uint32_t num_frames = 0;
  FrameInfo *index_table = nullptr;
  bool is_loaded = false;
};
// *** KẾT THÚC THÊM MỚI ***

struct DailyForecast {
  time_t date_ts;
  float min_temp;
  float max_temp;
  int icon;
};

// --- BIẾN TOÀN CỤC ---
static TFT_eSPI *_tft;
static TFT_eSprite *_sprite;
static MakeFont *_font;
static AppSettings *_settings;
// static DynamicVideo *_video; // Bỏ con trỏ ngoài

static WeatherData weatherStationData;
static bool weatherStationDataValid = false;
static uint32_t lastWeatherFetchTime = 0;
static String weatherStationStatus = "Đang khởi tạo...";
static struct tm timeinfo;
static bool timeSyncedNTP = false;

// Loading state (chỉ dùng cho lần vào đầu tiên)
static bool firstEnter = true;
static bool isLoading = false;

enum WeatherStationView {
  VIEW_MAIN,
  VIEW_SLIDESHOW,
  VIEW_FORECAST
};
static WeatherStationView currentView = VIEW_MAIN;  // Bắt đầu ở màn hình chính

// *** BIẾN MỚI CHO 2 MÀN HÌNH ***
// static bool isSlideshowViewActive = false;

static std::vector<String> slideshowImageFiles;
static int currentSlideshowImageIndex = 0;
static uint32_t lastSlideTime = 0;
static bool slideshowImagesLoaded = false;  // <<< THÊM MỚI: Cờ theo dõi đã tải ảnh hay chưa

// Biến cho vòng lặp WiFi
static int wifiConnectionFailures = 0;

// *** THÊM MỚI: Biến cục bộ cho Video Động ***
static DynamicVideo ws_dynamicVideo;  // Biến video riêng của module này
static uint32_t ws_lastVideoFrameTime = 0;
static uint16_t ws_currentFrame = 0;

static std::vector<DailyForecast> dailyForecasts;

// Kích thước video
static const int SCALE_FACTOR = 1;
static const int SCALED_W = 80;
static const int SCALED_H = 80;
static const int VIDEO_X_POS = 160;
static const int VIDEO_Y_POS = 240 - SCALED_H;
// *** KẾT THÚC THÊM MỚI ***


// --- GIỮ LẠI THỜI GIAN QUA REBOOT ---
RTC_DATA_ATTR time_t savedEpoch = 0;

// --- NGUYÊN MẪU HÀM ---
static bool connectToWiFi();
void refetchWeatherData();
static void fetchWeatherData();
static int mapWeatherIcon(String iconCode);
static void initNTP();
static void drawWeatherStationScreen();
static void drawWeatherSlideshowScreen();
static void reloadSlideshowImages();
static void drawLoadingScreen(uint8_t percent, const String &statusText);
static void syncTimeToRTC(struct tm *timeinfo);
static void restoreTimeFromRTC();
static void ws_loadDynamicVideo();
static void ws_cleanupDynamicVideo();
static void ws_drawDynamicVideoFrame();
static void drawForecastScreen();

// === KHỞI TẠO ===
void weather_station_init(TFT_eSPI *tft, TFT_eSprite *sprite, MakeFont *font, AppSettings *settings) {
  _tft = tft;
  _sprite = sprite;
  _font = font;
  _settings = settings;
}

void weather_station_enter() {
  weatherStationDataValid = false;
  weatherStationStatus = "Đang khởi tạo...";
  lastWeatherFetchTime = 0;  // Reset thời gian fetch khi vào
  timeSyncedNTP = false;
  currentView = VIEW_MAIN;
  slideshowImagesLoaded = false;
  wifiConnectionFailures = 0;
  firstEnter = true;  // Đánh dấu là lần vào đầu tiên
  isLoading = true;   // Đặt isLoading = true cho lần vào đầu tiên
  restoreTimeFromRTC();
  drawLoadingScreen(3, "Bắt đầu...");  // Vẽ loading screen ban đầu
  ws_loadDynamicVideo();
  ws_lastVideoFrameTime = 0;
  ws_currentFrame = 0;
  dailyForecasts.clear();
}

void weather_station_exit() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(true, true);
    delay(100);
    Serial.println("[Weather] WiFi disconnected.");
  }
  weatherStationDataValid = false;
  isLoading = false;

  // *** THÊM MỚI: Dọn dẹp slideshow khi thoát ***
  slideshowImageFiles.clear();
  slideshowImagesLoaded = false;
  Serial.println("[Slideshow] Cleaned up images.");
  // *** KẾT THÚC THÊM MỚI ***

  // *** THÊM MỚI: Dọn dẹp video cục bộ ***
  ws_cleanupDynamicVideo();
  dailyForecasts.clear();

  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);  // <<< RẤT QUAN TRỌNG
  Serial.println("[Weather] Exit: TJpgDec reset.");
}

Mode weather_station_loop(ButtonAction action) {
  if (action == ACTION_LONG)
    return MENU;
  else if (action == ACTION_DOUBLE) {
    switch (currentView) {
      case VIEW_MAIN:
        currentView = VIEW_FORECAST;
        break;
      case VIEW_SLIDESHOW:
        currentView = VIEW_MAIN;
        break;
      case VIEW_FORECAST:
        currentView = VIEW_SLIDESHOW;
        if (!slideshowImagesLoaded) {
          Serial.println("[Slideshow] Loading images on demand...");
          reloadSlideshowImages();
          slideshowImagesLoaded = true;
        }
        break;
    }
  } else if (action == ACTION_TRIPLE) {
    refetchWeatherData();
  }

  // 1) WiFi
  if (WiFi.status() != WL_CONNECTED) {
    if (!connectToWiFi()) {
      if (wifiConnectionFailures >= 3) {
        Serial.println("[Weather] Quá nhiều lần thất bại, thoát...");
        return MENU;
      }
    }
    if (WiFi.status() != WL_CONNECTED) {
      if (!firstEnter) {
        switch (currentView) {
          case VIEW_MAIN: drawWeatherStationScreen(); break;
          case VIEW_SLIDESHOW: drawWeatherSlideshowScreen(); break;
          case VIEW_FORECAST: drawForecastScreen(); break;
        }
      }
      return WEATHER_STATION_MODE;
    } else {
      wifiConnectionFailures = 0;  // Reset khi kết nối lại thành công
    }
  }

  // 2) NTP
  if (!timeSyncedNTP) {
    initNTP();  // Thử đồng bộ NTP
  }

  // Luôn cập nhật thời gian từ RTC nội bộ nếu NTP đã từng sync
  if (timeSyncedNTP) getLocalTime(&timeinfo);

  // 3) Thời tiết (Chỉ gọi khi cần)
  // Điều kiện fetch:
  // - Đã quá 10 phút
  // - HOẶC (Dữ liệu không hợp lệ VÀ KHÔNG đang loading) -> Tức là lỗi fetch trước đó, thử lại sau 10p
  // - HOẶC (isLoading VÀ dữ liệu chưa hợp lệ) -> Tức là đang trong lần đầu hoặc manual refetch
  bool needsInitialFetch = isLoading && (!weatherStationDataValid || dailyForecasts.empty());
  bool needsPeriodicFetch = !isLoading && (millis() - lastWeatherFetchTime > (10 * 60 * 1000));
  if (needsInitialFetch || needsPeriodicFetch) {
    fetchWeatherData();
  }

  // 4) Render
  // Nếu đang loading (lần đầu hoặc đang fetch lại) thì KHÔNG vẽ màn hình chính/slideshow/forecast
  if (isLoading) {
    if (!isLoading) {
      if (firstEnter && weatherStationDataValid) firstEnter = false;  // Đánh dấu hoàn tất lần đầu
      if (firstEnter && weatherStationDataValid) {
        firstEnter = false;
      }

      switch (currentView) {
        case VIEW_MAIN:
          drawWeatherStationScreen();
          break;
        case VIEW_SLIDESHOW:
          drawWeatherSlideshowScreen();
          break;
        case VIEW_FORECAST:
          drawForecastScreen();
          break;
      }
    }
  } else  // Chỉ vẽ các màn hình chính khi KHÔNG loading
  {
    // Đặt firstEnter = false sau khi đã fetch thành công lần đầu và không còn loading
    if (firstEnter && weatherStationDataValid) {
      firstEnter = false;
    }

    switch (currentView) {
      case VIEW_MAIN:
        drawWeatherStationScreen();
        break;
      case VIEW_SLIDESHOW:
        drawWeatherSlideshowScreen();
        break;
      case VIEW_FORECAST:
        drawForecastScreen();
        break;
    }
  }

  return WEATHER_STATION_MODE;
}

// === NTP ===
static void initNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    weatherStationStatus = "Lỗi WiFi, không thể lấy giờ";
    return;  // Không thử NTP nếu không có WiFi
  }
  weatherStationStatus = "Đang đồng bộ giờ...";
  // Chỉ vẽ loading nếu đang loading lần đầu hoặc đang fetch lại
  if (isLoading) drawLoadingScreen(45, weatherStationStatus);

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  unsigned long ntp_start_time = millis();
  while (!getLocalTime(&timeinfo, 1000)) {
    // Chỉ vẽ loading nếu đang loading lần đầu hoặc đang fetch lại
    if (isLoading) {
      uint32_t elapsed = millis() - ntp_start_time;
      uint8_t p = 40 + min<uint32_t>(30, (elapsed * 30) / 10000);
      drawLoadingScreen(p, "Đang đồng bộ giờ...");
    }
    if (millis() - ntp_start_time > 10000) {
      weatherStationStatus = "Lỗi đồng bộ giờ";
      timeSyncedNTP = false;  // Đánh dấu NTP thất bại
      Serial.println("[NTP] ❌ Failed to sync time.");
      return;  // Thoát nếu quá timeout
    }
  }
  timeSyncedNTP = true;  // Đánh dấu NTP thành công
  syncTimeToRTC(&timeinfo);
  Serial.println("[NTP] ✅ Time synced and saved to RTC");
}

// === KẾT NỐI WIFI ===
static bool connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    weatherStationStatus = "WiFi đã kết nối";
    return true;
  }
  weatherStationStatus = "Đang kết nối WiFi...";
  if (isLoading) drawLoadingScreen(8, weatherStationStatus);
  if (_settings->stationSsid.length() == 0) {
    weatherStationStatus = "Chưa cài đặt WiFi";
    if (isLoading) drawLoadingScreen(8, weatherStationStatus);
    wifiConnectionFailures++;
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(_settings->stationSsid.c_str(), _settings->stationPassword.c_str());
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (isLoading) {
      uint32_t elapsed = millis() - startTime;
      uint8_t p = 10 + min<uint32_t>(30, (elapsed * 30) / 15000);
      drawLoadingScreen(p, "Đang kết nối WiFi...");
    }
    if (millis() - startTime > 15000) {
      Serial.println("\n[WiFi] connection failed!");
      weatherStationStatus = "Lỗi kết nối WiFi";
      wifiConnectionFailures++;
      return false;
    }
  }
  Serial.println("\n[WiFi] connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  weatherStationStatus = "Đã kết nối";
  if (isLoading) drawLoadingScreen(40, "WiFi đã kết nối");
  return true;
}

// === LƯU/KHÔI PHỤC GIỜ RTC NỘI ===
static void syncTimeToRTC(struct tm *timeinfo) {
  time_t now;
  time(&now);
  savedEpoch = now;
  Serial.printf("[RTC] ✅ Lưu thời gian vào RTC nội (epoch=%ld)\n", savedEpoch);
}

static void restoreTimeFromRTC() {
  if (savedEpoch == 0) {
    Serial.println("[RTC] ⚠️ Chưa có thời gian lưu trước đó");
    return;
  }

  struct timeval tv = { .tv_sec = savedEpoch, .tv_usec = 0 };
  settimeofday(&tv, nullptr);

  if (getLocalTime(&timeinfo)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S %d/%m/%Y", &timeinfo);
    Serial.printf("[RTC] 🔄 Phục hồi thời gian: %s\n", buf);
    timeSyncedNTP = true;  // đã có giờ hợp lệ để hiển thị
  } else {
    Serial.println("[RTC] ❌ Không thể phục hồi thời gian");
  }
}

void refetchWeatherData() {
  lastWeatherFetchTime = 0;
  weatherStationDataValid = false;
  dailyForecasts.clear();
  isLoading = true;
  drawLoadingScreen(50, "Đang tải lại...");
}

// === FETCH WEATHER ===
static void fetchWeatherData() {
  Serial.println("[Weather] Bắt đầu fetchWeatherData (kết hợp /weather và /forecast)...");

  weatherStationStatus = "Đang cập nhật...";
  if (isLoading) drawLoadingScreen(60, "Lấy dữ liệu hiện tại...");

  lastWeatherFetchTime = millis();

  if (_settings->owmApiKey.length() == 0) {
    weatherStationStatus = "Chưa có API Key";
    if (isLoading) drawLoadingScreen(60, weatherStationStatus);
    isLoading = false;
    return;
  }

  if (_settings->owmCityId.length() == 0 || _settings->latitude.length() == 0 || _settings->longitude.length() == 0) {
    weatherStationStatus = "Chưa cài đặt Vị Trí";
    if (isLoading) drawLoadingScreen(60, weatherStationStatus);
    isLoading = false;
    return;
  }

  HTTPClient http;
  String url_current, url_forecast;
  bool current_ok = false;
  bool forecast_ok = false;

  if (_settings->latitude.length() != 0 && _settings->longitude.length() != 0) {
    url_current = "http://api.openweathermap.org/data/2.5/weather?lat=" + _settings->latitude + "&lon=" + _settings->longitude + "&appid=" + _settings->owmApiKey + "&units=metric&lang=" + _settings->language;
    url_forecast = "http://api.openweathermap.org/data/2.5/forecast?lat=" + _settings->latitude + "&lon=" + _settings->longitude + "&appid=" + _settings->owmApiKey + "&units=metric&lang=" + _settings->language;
  } else {
    url_current = "http://api.openweathermap.org/data/2.5/weather?id=" + _settings->owmCityId + "&appid=" + _settings->owmApiKey + "&units=metric&lang=" + _settings->language;
    url_forecast = "http://api.openweathermap.org/data/2.5/forecast?id=" + _settings->owmCityId + "&appid=" + _settings->owmApiKey + "&units=metric&lang=" + _settings->language;
  }

  Serial.println("[Weather] Request: " + url_current);
  http.setTimeout(7000);
  http.begin(url_current);

  int httpCode_current = http.GET();
  Serial.printf("[Weather] HTTP Code: %d\n", httpCode_current);

  if (httpCode_current == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      String cityName = doc["name"].as<String>();
      cityName = replaceAllUtf8(cityName, "City", "");
      cityName = replaceAllUtf8(cityName, "Thành phố", "");
      cityName.trim();
      weatherStationData.city = cityName;  // Lưu tên thành phố
      weatherStationData.currentTemp = round(doc["main"]["temp"].as<float>());
      weatherStationData.pressure = doc["main"]["humidity"].as<int>();
      weatherStationData.uv = doc["wind"]["speed"].as<float>() * 3.6;  // Tốc độ gió
      weatherStationData.updateTime = doc["weather"][0]["description"].as<String>();
      weatherStationData.updateTime.toUpperCase();
      String iconCode = doc["weather"][0]["icon"];
      weatherStationData.icon = mapWeatherIcon(iconCode);
      current_ok = true;
      Serial.println("[Weather] ✅ Current data fetched.");
    } else {
      Serial.print("[Weather] JSON error (Current): ");
      Serial.println(error.c_str());
      weatherStationStatus = "Lỗi dữ liệu hiện tại";
    }
  } else {
    weatherStationStatus = "Lỗi HTTP: " + String(httpCode_current);
    Serial.println(weatherStationStatus);
  }
  http.end();

  if (!current_ok) {
    if (isLoading) drawLoadingScreen(65, weatherStationStatus);
    weatherStationDataValid = false;  // Đảm bảo đánh dấu không hợp lệ
    isLoading = false;                // Kết thúc loading
    return;                           // Thoát nếu current lỗi
  }
  // --- 2. Gọi API /forecast (Lấy dự báo 5 ngày) ---

  if (isLoading) drawLoadingScreen(75, "Lấy dự báo...");
  Serial.println("[Weather] Request (Forecast): " + url_forecast);
  http.begin(url_forecast);

  int httpCode_forecast = http.GET();
  Serial.printf("[Weather] HTTP Code (Forecast): %d\n", httpCode_forecast);

  if (httpCode_forecast == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      dailyForecasts.clear();      // Xóa dữ liệu cũ trước khi xử lý
      int last_day = -1;           // Theo dõi ngày cuối cùng đã xử lý
      int icon_counts[8] = { 0 };  // Đếm số lần xuất hiện icon trong ngày
      int most_frequent_icon = 7;  // Icon mặc định (unknown)

      JsonArray list = doc["list"].as<JsonArray>();
      // Serial.printf("[Weather] Forecast list size: %d\n", list.size());

      for (JsonObject item : list) {
        time_t dt = item["dt"].as<time_t>();
        if (dt <= 0) continue;
        struct tm *item_tm = localtime(&dt);
        if (!item_tm) continue;

        int current_item_day = item_tm->tm_yday;
        float item_temp = item["main"]["temp"].as<float>();
        int item_icon = mapWeatherIcon(item["weather"][0]["icon"].as<String>());

        if (current_item_day != last_day) {
          if (!dailyForecasts.empty()) {
            int max_count = 0;
            for (int j = 0; j < 8; ++j)
              if (icon_counts[j] > max_count) {
                max_count = icon_counts[j];
                most_frequent_icon = j;
              }
            dailyForecasts.back().icon = most_frequent_icon;
          }

          if (dailyForecasts.size() < 5) {
            DailyForecast new_day;
            new_day.date_ts = dt;
            new_day.min_temp = item_temp;
            new_day.max_temp = item_temp;
            for (int j = 0; j < 8; ++j) icon_counts[j] = 0;
            icon_counts[item_icon]++;
            most_frequent_icon = item_icon;
            dailyForecasts.push_back(new_day);
            last_day = current_item_day;
          } else break;
        } else if (!dailyForecasts.empty()) {
          DailyForecast &last_entry = dailyForecasts.back();
          if (item_temp < last_entry.min_temp) last_entry.min_temp = item_temp;
          if (item_temp > last_entry.max_temp) last_entry.max_temp = item_temp;
          icon_counts[item_icon]++;
        }
      }  // end for

      if (!dailyForecasts.empty()) {
        int max_count = 0;
        for (int j = 0; j < 8; ++j)
          if (icon_counts[j] > max_count) {
            max_count = icon_counts[j];
            most_frequent_icon = j;
          }
        dailyForecasts.back().icon = most_frequent_icon;
      }

      if (!dailyForecasts.empty()) {
        weatherStationData.lowTemp = round(dailyForecasts[0].min_temp);
        weatherStationData.highTemp = round(dailyForecasts[0].max_temp);
        forecast_ok = true;
        Serial.println("[Weather] ✅ Forecast processed.");
        // Serial.printf("[Weather] Today's Min: %.0f, Today's Max: %.0f\n", weatherStationData.lowTemp, weatherStationData.highTemp);
      } else {
        Serial.println("[Weather] Forecast error: No daily data processed.");
        weatherStationStatus = "Lỗi xử lý dự báo";
        weatherStationData.lowTemp = weatherStationData.currentTemp;  // Lấy tạm
        weatherStationData.highTemp = weatherStationData.currentTemp;
        forecast_ok = true;  // Vẫn OK nếu chấp nhận min/max tạm
      }
    } else {
      Serial.print("[Weather] JSON error (Forecast): ");
      Serial.println(error.c_str());
      weatherStationStatus = "Lỗi dữ liệu dự báo";
      weatherStationData.lowTemp = weatherStationData.currentTemp;  // Lấy tạm
      weatherStationData.highTemp = weatherStationData.currentTemp;
      forecast_ok = true;  // Vẫn OK
    }
  } else {
    weatherStationStatus = "Lỗi HTTP (Forecast): " + String(httpCode_forecast);
    Serial.println(weatherStationStatus);
    weatherStationData.lowTemp = weatherStationData.currentTemp;  // Lấy tạm
    weatherStationData.highTemp = weatherStationData.currentTemp;
    forecast_ok = true;  // Vẫn OK
  }
  http.end();

  // --- 3. Hoàn tất ---
  if (current_ok && forecast_ok) {   // Chỉ cần current_ok để đánh dấu valid
    weatherStationDataValid = true;  // <<< Đánh dấu dữ liệu hợp lệ
    if (timeSyncedNTP) {
      char timeStr[6];
      sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
      weatherStationStatus = "Cập nhật lúc " + String(timeStr);
    } else {
      weatherStationStatus = "Cập nhật thành công";
    }
    if (isLoading) drawLoadingScreen(100, "Hoàn tất");
    Serial.println("[Weather] ✅ Data updated successfully (Combined).");
    // lastWeatherFetchTime đã được cập nhật ở đầu hàm

  } else {                              // Nếu current_ok là false (đã return trước đó) hoặc forecast_ok là false (nhưng vẫn có thể tiếp tục)
    if (!current_ok) {                  // Trường hợp current lỗi nghiêm trọng
      weatherStationDataValid = false;  // Đánh dấu không hợp lệ
      Serial.println("[Weather] ❌ Failed to update data (Current failed).");
    } else {                           // Trường hợp chỉ forecast lỗi
      weatherStationDataValid = true;  // Vẫn coi là hợp lệ vì có current
      Serial.println("[Weather] ⚠️ Updated current data, but forecast failed.");
      if (timeSyncedNTP) {
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        weatherStationStatus = "Lỗi dự báo (" + String(timeStr) + ")";  // Hiển thị lỗi dự báo
      } else {
        weatherStationStatus = "Lỗi dự báo";
      }
      if (isLoading) drawLoadingScreen(100, "Lỗi dự báo");
    }
    // lastWeatherFetchTime đã được cập nhật ở đầu hàm
  }
  isLoading = false;  // Luôn kết thúc loading sau khi fetch xong (thành công hoặc thất bại)
}

// === ICON MAP ===
static int mapWeatherIcon(String iconCode) {
  if (iconCode == "01d" || iconCode == "01n") return 0;
  if (iconCode == "02d" || iconCode == "02n") return 1;
  if (iconCode == "03d" || iconCode == "03n") return 2;
  if (iconCode == "04d" || iconCode == "04n") return 2;
  if (iconCode == "09d" || iconCode == "09n") return 3;
  if (iconCode == "10d" || iconCode == "10n") return 4;
  if (iconCode == "11d" || iconCode == "11n") return 5;
  if (iconCode == "13d" || iconCode == "13n") return 6;
  if (iconCode == "50d" || iconCode == "50n") return 2;
  return 7;
}

// === DRAW LOADING ===
static void drawLoadingScreen(uint8_t percent, const String &statusText) {
  if (!isLoading) return;

  _sprite->fillSprite(TFT_BLACK);

  fontTargetSprite = _sprite;
  _font->set_font(Fira_Code_16);
  int titleW = _font->getLength("MOCHI");
  _font->print((_tft->width() - titleW) / 2, 30, "MOCHI", TFT_WHITE, TFT_BLACK);

  int statusW = _font->getLength(statusText);
  _font->print((_tft->width() - statusW) / 2, 70, statusText, TFT_YELLOW, TFT_BLACK);

  int pad = 20;
  int barW = _tft->width() - pad * 2;
  int barH = 14;
  int barX = pad;
  int barY = _tft->height() / 2 - barH / 2 + 10;

  _sprite->drawRoundRect(barX, barY, barW, barH, 4, TFT_DARKGREY);

  int fillW = (int)(barW * constrain(percent, 0, 100) / 100.0f);
  _sprite->fillRoundRect(barX + 2, barY + 2, max(0, fillW - 4), barH - 4, 3, TFT_CYAN);

  char pbuf[8];
  sprintf(pbuf, "%u%%", (unsigned)constrain(percent, 0, 100));
  int pW = _font->getLength(pbuf);
  _font->print((_tft->width() - pW) / 2, barY + barH + 18, pbuf, TFT_WHITE, TFT_BLACK);

  _sprite->pushSprite(0, 0);

  // Đặt lại isLoading = false sau khi vẽ xong loading 100%
  if (percent >= 100) {
    isLoading = false;
  }
}

// === RELOAD SLIDESHOW IMAGES ===
static void reloadSlideshowImages() {
  slideshowImageFiles.clear();
  fs::File root = SPIFFS.open("/");
  if (!root) {
    Serial.println("Failed to open root directory");
    return;
  }
  fs::File file = root.openNextFile();
  while (file) {
    String fileName = String(file.name());
    String checkName = fileName.startsWith("/") ? fileName.substring(1) : fileName;

    if (checkName.startsWith("ss_") && !file.isDirectory()) {
      String fullPath = "/" + checkName;
      slideshowImageFiles.push_back(fullPath);
    }
    file = root.openNextFile();
  }
  root.close();
  Serial.printf("[Slideshow] Found %d images.\n", slideshowImageFiles.size());
}

// === DRAW SLIDESHOW SCREEN ===
static void drawWeatherSlideshowScreen() {
  // --- Vẽ ảnh nền slideshow (logic không đổi) ---
  if (slideshowImageFiles.empty()) {
    _sprite->fillSprite(TFT_BLACK);
    fontTargetSprite = _sprite;  // Đặt target trước khi dùng _font
    // Tạm dùng font mặc định nếu _font lỗi
    if (_font) {
      _font->set_font(Fira_Code_16);
      _font->print(10, 110, "Không có ảnh", TFT_YELLOW, TFT_BLACK);
    } else {
      _sprite->setTextFont(2);
      _sprite->setTextColor(TFT_YELLOW, TFT_BLACK);
      _sprite->setTextDatum(MC_DATUM);  // Canh giữa
      _sprite->drawString("Khong co anh", _tft->width() / 2, 110);
      _sprite->setTextDatum(TL_DATUM);  // Reset
    }
    _sprite->pushSprite(0, 0);
    return;
  }
  if (millis() - lastSlideTime > 5000) {
    lastSlideTime = millis();
    currentSlideshowImageIndex = (currentSlideshowImageIndex + 1) % slideshowImageFiles.size();
    String imagePath = slideshowImageFiles[currentSlideshowImageIndex];
    File imageFile = SPIFFS.open(imagePath, "r");
    if (imageFile) {
      jpegSpriteTarget = _sprite;
      TJpgDec.setCallback(sprite_output);
      TJpgDec.drawFsJpg(0, 0, imageFile);
      imageFile.close();
    }
  }

  // --- Vẽ lớp phủ thời gian và icon ở góc dưới bên phải (BỐ CỤC MỚI) ---
  int padding = 10;       // Khoảng cách lề
  int icon_time_gap = 5;  // Khoảng cách giữa icon (phía trên) và thời gian (phía dưới)

  if (timeSyncedNTP) {
    // 1. Chuẩn bị font và text cho thời gian
    _sprite->setFreeFont(&Digitall0124pt7b);      // Font giờ (nhỏ hơn)
    _sprite->setTextColor(TFT_WHITE);  // Chữ trắng
    _sprite->setTextDatum(BR_DATUM);              // Canh dưới phải (Bottom Right)
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    // 2. Tính toán vị trí THỜI GIAN
    int time_x = _tft->width() - padding;
    int time_y = _tft->height() - padding;
    int time_w = _sprite->textWidth(timeStr);  // Lấy chiều rộng chữ thời gian
    int time_h = _sprite->fontHeight();        // Lấy chiều cao font

    // 3. Vẽ THỜI GIAN
    _sprite->drawString(timeStr, time_x, time_y);

    // 4. Vẽ ICON thời tiết (nếu có dữ liệu) - PHÍA TRÊN và CANH GIỮA thời gian
    if (weatherStationDataValid) {
      // Tính toán X để icon canh giữa phía trên chữ thời gian
      int icon_center_x = time_x - (time_w / 2);     // Tâm X của icon = Tâm X của thời gian
      int icon_x = icon_center_x - (WEATHER_W / 2);  // Tọa độ X góc trái của icon

      // Tính toán Y để icon nằm phía trên thời gian
      int icon_y = time_y - time_h - icon_time_gap - WEATHER_H;  // Y = baseline chữ - chiều cao chữ - khoảng cách - chiều cao icon

      fontTargetSprite = _sprite;  // Đảm bảo target đúng
      drawWeatherIcon(weatherStationData.icon, icon_x, icon_y);
    }

    // 5. Reset font và datum
    _sprite->setFreeFont(NULL);
    _sprite->setTextDatum(TL_DATUM);
  }
  // --- Kết thúc vẽ lớp phủ ---

  _sprite->pushSprite(0, 0);
}


// *** THÊM MỚI: CÁC HÀM VIDEO CỤC BỘ ***
static void ws_loadDynamicVideo() {
  // Dọn dẹp nếu đã load trước đó
  ws_cleanupDynamicVideo();

  Serial.println("[WS_Vid] Loading dynamic video index...");
  if (!SPIFFS.exists(DYNAMIC_VIDEO_FILE)) {
    Serial.println("[WS_Vid] FAIL: video_custom.bin not found.");
    return;
  }
  Serial.println("[WS_Vid] OK: File exists.");  // <<< DEBUG

  // <<< THAY ĐỔI: Mở file và LƯU NÓ LẠI
  fs::File file = SPIFFS.open(DYNAMIC_VIDEO_FILE, "r");
  if (!file) {
    Serial.println("[WS_Vid] FAIL: Could not open file.");
    return;
  }
  Serial.println("[WS_Vid] OK: File opened.");  // <<< DEBUG

  // Đọc số lượng frame (4 byte đầu tiên)
  file.read((uint8_t *)&ws_dynamicVideo.num_frames, sizeof(uint32_t));
  if (ws_dynamicVideo.num_frames == 0) {
    Serial.println("[WS_Vid] FAIL: num_frames is 0.");
    file.close();
    return;
  }
  Serial.printf("[WS_Vid] OK: Found %u frames.\n", ws_dynamicVideo.num_frames);  // <<< DEBUG

  // Cấp phát bộ nhớ cho bảng index
  size_t index_size = sizeof(FrameInfo) * ws_dynamicVideo.num_frames;
  ws_dynamicVideo.index_table = (FrameInfo *)malloc(index_size);

  if (!ws_dynamicVideo.index_table) {
    Serial.println("[WS_Vid] FAIL: Malloc failed for index table.");
    ws_dynamicVideo.num_frames = 0;
    file.close();
    return;
  }
  Serial.println("[WS_Vid] OK: Malloc successful.");  // <<< DEBUG

  // Đọc toàn bộ bảng index
  file.read((uint8_t *)ws_dynamicVideo.index_table, index_size);
  // <<< THAY ĐỔI: ĐÓNG FILE NGAY SAU KHI ĐỌC INDEX
  file.close();
  Serial.println("[WS_Vid] OK: Index loaded. File closed.");

  ws_dynamicVideo.is_loaded = true;
  Serial.printf("[WS_Vid] OK: Loaded index for %u frames.\n", ws_dynamicVideo.num_frames);
}

static void ws_cleanupDynamicVideo() {
  // <<< THAY ĐỔI: Không cần đóng file ở đây
  // if (ws_dynamicVideo.file) {
  //     ws_dynamicVideo.file.close();
  //     Serial.println("[WS_Vid] File closed.");
  // }
  if (ws_dynamicVideo.index_table) {
    free(ws_dynamicVideo.index_table);
    ws_dynamicVideo.index_table = nullptr;
  }
  ws_dynamicVideo.is_loaded = false;
  ws_dynamicVideo.num_frames = 0;
  Serial.println("[WS_Vid] Cleaned up video index.");
}

static void ws_drawDynamicVideoFrame() {
  // 1. Kiểm tra xem index đã được load VÀ file đã được mở
  if (!ws_dynamicVideo.is_loaded) {
    return;
  }

  // 2. Kiểm tra thời gian frame
  uint32_t now = millis();
  if (now - ws_lastVideoFrameTime < _settings->frameDelay) {
    return;  // Chưa đến lúc vẽ
  }
  ws_lastVideoFrameTime = now;

  // 3. <<< THAY ĐỔI: MỞ FILE MỖI LẦN VẼ
  fs::File frameFile = SPIFFS.open(DYNAMIC_VIDEO_FILE, "r");
  if (!frameFile) {
    Serial.println("[WS_Vid] FAIL: Could not re-open file for drawing.");
    ws_dynamicVideo.is_loaded = false;  // Dừng hẳn nếu file lỗi
    return;
  }

  // 4. Lấy thông tin frame và seek đến đúng vị trí
  if (ws_currentFrame >= ws_dynamicVideo.num_frames) {
    ws_currentFrame = 0;
  }
  FrameInfo info = ws_dynamicVideo.index_table[ws_currentFrame];
  // *** THÊM MỚI: Kiểm tra seek an toàn ***
  if (!frameFile.seek(info.offset)) {
    Serial.println("[WS_Vid] FAIL: File seek failed!");
    frameFile.close();                  // Đóng file hỏng
    ws_dynamicVideo.is_loaded = false;  // Dừng vẽ
    return;
  }

  // 5. Thiết lập TJpgDec để vẽ vào sprite VÀ thu nhỏ 4x
  jpegSpriteTarget = _sprite;
  TJpgDec.setCallback(sprite_output);
  TJpgDec.setJpgScale(SCALE_FACTOR);  // Tỷ lệ 4

  // 6. Vẽ JPEG đã được thu nhỏ vào đúng vị trí
  TJpgDec.drawFsJpg(VIDEO_X_POS, VIDEO_Y_POS, frameFile);

  // 7. Dọn dẹp: Reset lại các cài đặt TJpgDec về mặc định
  TJpgDec.setJpgScale(1);
  // <<< THAY ĐỔI: ĐÓNG FILE SAU KHI DÙNG
  frameFile.close();

  // 8. Chuyển sang frame tiếp theo
  ws_currentFrame++;
}
// *** KẾT THÚC THÊM MỚI ***


// === DRAW MAIN SCREEN ===
static void drawWeatherStationScreen() {
  if (!weatherStationDataValid || !timeSyncedNTP) {
    _sprite->fillSprite(TFT_BLACK);
    fontTargetSprite = _sprite;
    _font->set_font(Fira_Code_16);
    _font->print((_tft->width() - _font->getLength(weatherStationStatus)) / 2, 110, weatherStationStatus, TFT_YELLOW, TFT_BLACK);
    _sprite->pushSprite(0, 0);
    return;
  }

  // luôn lấy thời gian hiện tại để hiển thị liên tục
  getLocalTime(&timeinfo);

  _sprite->fillSprite(TFT_BLACK);
  fontTargetSprite = _sprite;
  _sprite->setTextSize(1);
  _sprite->setTextDatum(TL_DATUM);

  int regionW = _tft->width() / 3;
  int offsetX = _tft->width() * 2 / 3;

  String city = weatherStationData.city;
  // _sprite->setTextFont(4);
  // _sprite->setTextColor(0xFFFF);
  // _sprite->drawString(city, (160 - _sprite->textWidth(city)) / 2, 20);

  _font->print((160 - _font->getLength(city)) / 2, 20, city, TFT_WHITE, TFT_BLACK);

  drawWeatherIcon(weatherStationData.icon, offsetX + (regionW - WEATHER_W) / 2, 0);

  _sprite->setFreeFont(&Digitall0132pt7b);
  char hourStr[3], minuteStr[3];
  sprintf(hourStr, "%02d", timeinfo.tm_hour);
  sprintf(minuteStr, "%02d", timeinfo.tm_min);
  int hourW = _sprite->textWidth(hourStr);
  int minuteW = _sprite->textWidth(minuteStr);
  int timeX = (offsetX - (hourW + minuteW)) / 2;
  _sprite->setTextColor(0xFEE0);
  _sprite->drawString(hourStr, timeX, 55);
  _sprite->setTextColor(0xF206);
  _sprite->drawString(minuteStr, timeX + hourW + 5, 55);

  _sprite->setFreeFont(&Digitall0124pt7b);
  char secondStr[3];
  sprintf(secondStr, "%02d", timeinfo.tm_sec);
  _sprite->setTextColor(0x24BE);
  _sprite->drawString(secondStr, offsetX + (regionW - _sprite->textWidth(secondStr)) / 2, 95);

  // *** THÊM MỚI: Mảng Tên các Thứ trong Tuần ***
  static const char *days_vi[] = { "CN", "T2", "T3", "T4", "T5", "T6", "T7" };
  static const char *days_en[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  // Tự động chọn danh sách ngày dựa trên ngôn ngữ đã lưu
  const char **days_list = (_settings->language == "vi") ? days_vi : days_en;
  // *** KẾT THÚC THÊM MỚI ***

  _sprite->setTextFont(4);
  char dateStr[20];  // Tăng kích thước buffer để chứa thêm thứ
  // Cập nhật hàm sprintf để thêm thứ vào đầu
  sprintf(dateStr, "%s, %02d/%02d/%02d",
          days_list[timeinfo.tm_wday],  // Thêm ngày trong tuần
          timeinfo.tm_mday,
          timeinfo.tm_mon + 1,
          timeinfo.tm_year % 100);  // Chỉ lấy 2 số cuối của năm
  _sprite->setTextColor(0xFFFF);
  _sprite->drawString(dateStr, (160 - _sprite->textWidth(dateStr)) / 2, 120);

  String label = weatherStationData.updateTime;
  int textW = _font->getLength(label);
  int textX = offsetX + (regionW - textW) / 2;
  int textY = 66;
  _sprite->fillRoundRect(166, textY, 72, 26, 2, TFT_WHITE);
  if (textW <= 72) {
    _font->print(textX, textY + 2, label, 0x8410, TFT_WHITE);
  } else {
    drawMarqueeText(_sprite, _font, label, 167, textY + 2, 72, 0x8410, TFT_WHITE, true, 35);
  }

  _sprite->setFreeFont(&Digitall0132pt7b);
  String tempStr = String(weatherStationData.currentTemp);
  int tempW = _sprite->textWidth(tempStr);
  int cW = _sprite->textWidth("C");
  int tempX = (offsetX - (tempW + cW)) / 2;
  uint16_t tempColor = (weatherStationData.currentTemp > 29) ? TFT_ORANGE : TFT_GREEN;
  _sprite->setTextColor(tempColor, TFT_BLACK);
  _sprite->drawString(tempStr, tempX - 5, 145);
  _sprite->drawString("C", tempX + tempW + 5, 145);
  _sprite->drawCircle(tempX + tempW, 155, 4, tempColor);

  char otherStr[64];
  sprintf(otherStr,
          "H: %s°C L: %s°C W: %skm/h P: %d%%",
          formatFloatSmart(weatherStationData.highTemp),
          formatFloatSmart(weatherStationData.lowTemp),
          formatFloatSmart(weatherStationData.uv),
          weatherStationData.pressure);

  _font->set_font(Fira_Code_16);
  if (_font->getLength(otherStr) <= 150)
    _font->print(6, 220, otherStr, TFT_WHITE, TFT_BLACK);
  else
    drawMarqueeText(_sprite, _font, otherStr, 6, 220, 150, TFT_WHITE, TFT_BLACK, true, 35);

  // *** THAY ĐỔI: Gọi hàm vẽ video mới ***
  ws_drawDynamicVideoFrame();

  _sprite->pushSprite(0, 0);
}

static void drawForecastScreen() {
  _sprite->fillSprite(TFT_BLACK);
  fontTargetSprite = _sprite;  // Đặt target sprite

  // Lấy tên các thứ trong tuần
  static const char *days_vi[] = { "CN", "T2", "T3", "T4", "T5", "T6", "T7" };
  static const char *days_en[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  const char **days_list = (_settings->language == "vi") ? days_vi : days_en;

  // Kiểm tra xem có dữ liệu không
  if (dailyForecasts.empty()) {
    _sprite->setTextFont(2);  // Dùng font nhỏ
    _sprite->setTextColor(TFT_YELLOW, TFT_BLACK);
    _sprite->setTextDatum(MC_DATUM);  // Canh giữa
    _sprite->drawString("Khong co du lieu", _tft->width() / 2, _tft->height() / 2);
    _sprite->pushSprite(0, 0);
    _sprite->setTextFont(1);          // Reset font
    _sprite->setTextDatum(TL_DATUM);  // Reset datum
    return;
  }

  // --- Phần trên: Thông tin Hôm nay ---
  int top_section_y = 10;
  int today_icon_x = 20;
  int today_icon_y = top_section_y + 25;
  int today_temp_x = today_icon_x + WEATHER_W + 15;
  int today_temp_y = today_icon_y + 5;
  int today_hilow_y = today_temp_y + 45;  // Dưới nhiệt độ lớn
  int today_desc_y = today_hilow_y + 25;  // Dưới mô tả


  // Lấy dữ liệu hôm nay (index 0)
  DailyForecast today = dailyForecasts[0];
  // Kiểm tra timestamp hợp lệ
  time_t now_ts_check = time(nullptr);
  struct tm *now_tm_check = localtime(&now_ts_check);
  bool is_really_today = false;
  if (today.date_ts > 0) {
    struct tm *today_tm_check = localtime(&today.date_ts);
    if (now_tm_check && today_tm_check && today_tm_check->tm_yday == now_tm_check->tm_yday && today_tm_check->tm_year == now_tm_check->tm_year) {
      is_really_today = true;
    }
  }

  _font->set_font(Fira_Code_16);  // Font cho "Hôm nay"
  if (timeSyncedNTP) {
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    _font->print(today_icon_x, top_section_y, weatherStationData.city + " " + timeStr, TFT_WHITE, TFT_BLACK);
  } else {
    _font->print(today_icon_x, top_section_y, weatherStationData.city, TFT_WHITE, TFT_BLACK);
  }
  // _font->print(today_icon_x, top_section_y, is_really_today ? "Hom nay" : days_list[localtime(&today.date_ts)->tm_wday], TFT_WHITE, TFT_BLACK);

  drawWeatherIcon(today.icon, today_icon_x, today_icon_y);  // Icon lớn

  _sprite->setFreeFont(&Digitall0124pt7b);  // Font lớn cho nhiệt độ hiện tại
  _sprite->setTextColor(TFT_WHITE, TFT_BLACK);
  _sprite->drawString(String(weatherStationData.currentTemp), today_temp_x, today_temp_y);
  // Vẽ chữ C nhỏ và vòng tròn độ
  int current_temp_w = _sprite->textWidth(String(weatherStationData.currentTemp));
  _sprite->setTextFont(4);  // Font nhỏ hơn cho chữ C
  _sprite->drawString("C", today_temp_x + current_temp_w + 10, today_temp_y + 5);
  _sprite->drawCircle(today_temp_x + current_temp_w + 5, today_temp_y + 10, 3, TFT_WHITE);  // Vòng tròn độ

  // Mô tả thời tiết hiện tại
  _font->set_font(Fira_Code_16);  // Font Fira Code
  _font->print(today_icon_x, today_desc_y, weatherStationData.updateTime, TFT_WHITE, TFT_BLACK);

  // Nhiệt độ Cao/Thấp hôm nay
  char today_hilow_str[20];
  sprintf(today_hilow_str, "H:%.0f L:%.0f", round(today.max_temp), round(today.min_temp));
  _font->print(today_temp_x, today_hilow_y, today_hilow_str, TFT_WHITE, TFT_BLACK);


  // --- Phần dưới: Dự báo 4 ngày tiếp theo ---
  int forecast_start_y = 140;                                      // Vị trí bắt đầu của hàng ngang
  int num_forecast_days = min((int)dailyForecasts.size() - 1, 4);  // Lấy tối đa 4 ngày tiếp theo

  if (num_forecast_days > 0) {
    int forecast_col_width = _tft->width() / num_forecast_days;  // Chiều rộng mỗi cột
    int forecast_label_y = forecast_start_y;
    int forecast_icon_y = forecast_label_y + 20;
    int forecast_temp_y = forecast_icon_y + (WEATHER_H / 2) + 5;  // Icon giờ là 20x20
    int forecast_icon_scale = 2;                                  // Tỷ lệ thu nhỏ icon

    _sprite->setTextFont(2);  // Font nhỏ cho dự báo
    _sprite->setTextColor(TFT_WHITE, TFT_BLACK);
    _sprite->setTextDatum(TC_DATUM);  // Canh giữa trên

    for (int i = 0; i < num_forecast_days; ++i) {
      DailyForecast day = dailyForecasts[i + 1];  // Bắt đầu từ ngày mai (index 1)
      if (day.date_ts <= 0) continue;
      struct tm *day_tm = localtime(&day.date_ts);
      if (!day_tm) continue;

      int col_center_x = (i * forecast_col_width) + (forecast_col_width / 2);

      // 1. Vẽ nhãn ("Th X DD")
      char date_buf[10];
      sprintf(date_buf, "%s %d", days_list[day_tm->tm_wday], day_tm->tm_mday);
      _sprite->drawString(date_buf, col_center_x, forecast_label_y);

      // 2. Vẽ icon thu nhỏ
      drawWeatherIconScaled(day.icon, col_center_x - (WEATHER_W / (2 * forecast_icon_scale)), forecast_icon_y, forecast_icon_scale);

      // 3. Vẽ nhiệt độ Max/Min
      char temp_str[15];
      sprintf(temp_str, "%.0f/%.0f", round(day.max_temp), round(day.min_temp));
      _sprite->drawString(temp_str, col_center_x, forecast_temp_y);
    }
  }

  _sprite->pushSprite(0, 0);
  // Reset font và datum sau khi vẽ xong
  _sprite->setTextFont(1);
  _sprite->setTextDatum(TL_DATUM);
  _font->set_font(Fira_Code_16);  // Đảm bảo font custom được đặt lại
  fontTargetSprite = _sprite;     // Đảm bảo target đúng
}
