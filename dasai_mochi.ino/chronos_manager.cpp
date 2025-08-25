#include "chronos_manager.h"
#include <ChronosESP32.h>
#include <vector>
#include <time.h>
#include "ui_utils.h"

// --- CÁC BIẾN TĨNH ---
static TFT_eSPI* _tft;
static TFT_eSprite* _sprite;
static MakeFont* _font;
static AppSettings* _settings;
static ChronosESP32 Chronos("Mochi Watch");

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

// --- CÁC HÀM NỘI BỘ ---
static void syncTimeToRTC() {
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
}

static void getTimeFromRTC() {
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

static void wrapMessage(String text) {
    wrappedMessageLines.clear();
    if (text.length() == 0) return;
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

// --- CÁC HÀM CALLBACK ---
static void connectionCallback(bool state) { isConnected = state; if (state) syncTimeToRTC(); }
static void ringerCallback(String caller, bool state) { callerInfo = caller; isRinging = state; }
static void notificationCallback(Notification notification) {
    latestNotification = notification;
    hasNewNotification = true;
    hasNewNavigation = false; 
    wrapMessage(latestNotification.title + "\n" + latestNotification.message); 
    
    notificationStartTime = millis();
    lastMessageScrollTime = millis();
    messageScrollLine = 0; 
    hasScrolledOnce = false;
    scrollFinishedTime = 0;
    isNotificationScrolling = (wrappedMessageLines.size() > 7);
}

static void configCallback(Config config, uint32_t a, uint32_t b) {
    switch(config) {
        case CF_NAV_DATA:
            if (a) { 
                latestNavigation = Chronos.getNavigation();
                hasNewNavigation = true;
                hasNewNotification = false; 
            } else {
                hasNewNavigation = false;
            }
            break;
        case CF_NAV_ICON:
            if (a == 2) { 
                Navigation tempNav = Chronos.getNavigation();
                if (nav_icon_crc != tempNav.iconCRC) {
                    nav_icon_crc = tempNav.iconCRC;
                    latestNavigation = tempNav; 
                    hasNewNavigation = true;
                }
            }
            break;
        // *** THÊM LOGIC XỬ LÝ THỜI TIẾT ***
        case CF_WEATHER:
            Serial.println("Weather received");
            if (a > 0) { // Có dữ liệu mới
                Weather w = Chronos.getWeatherAt(0);
                latestWeather.currentTemp = w.temp;
                latestWeather.highTemp = w.high;
                latestWeather.lowTemp = w.low;
                latestWeather.icon = w.icon;
                latestWeather.pressure = w.pressure;
                latestWeather.uv = w.uv;

                hasWeatherData = true;
            }
            if (b) { // Có tên thành phố
                String city = Chronos.getWeatherCity();
                latestWeather.city = city;
            }
            break;
        default:
            break;
    }
}

// --- CÁC HÀM CÔNG KHAI ---
void chronos_init(TFT_eSPI* tft, TFT_eSprite* sprite, MakeFont* font, AppSettings* settings) {
    _tft = tft; _sprite = sprite; _font = font; _settings = settings;
    Chronos.setConnectionCallback(connectionCallback);
    Chronos.setNotificationCallback(notificationCallback);
    Chronos.setRingerCallback(ringerCallback); 
    Chronos.setConfigurationCallback(configCallback);
    Chronos.begin(); 
}

void chronos_loop() {
    Chronos.loop();
    getTimeFromRTC();
}

bool chronos_draw_alerts() {
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
        _sprite->fillSprite(TFT_BLACK);
        float distanceInMeters = 0;
        String distStr = latestNavigation.title;
        distStr.trim();
        distStr.replace(",", ".");

        if (distStr.indexOf("k") > -1) {
            distanceInMeters = distStr.toFloat() * 1000;
        } else {
            distanceInMeters = distStr.toFloat();
        }

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

        if (nav_icon_crc != 0xFFFFFFFF && navIconVisible) {
            int iconSize = 96;
            int pixelSize = iconSize / 48;
            for (int y = 0; y < 48; y++) {
                for (int x = 0; x < 48; x++) {
                    int byte_index = (y * 48 + x) / 8;
                    int bit_pos = 7 - (x % 8);
                    bool px_on = (latestNavigation.icon[byte_index] >> bit_pos) & 0x01;
                    if (px_on) {
                        _sprite->fillRect(10 + x * pixelSize, 10 + y * pixelSize, pixelSize, pixelSize, iconColor);
                    }
                }
            }
        }
        
        _font->print(15, 120, latestNavigation.title, TFT_WHITE, TFT_BLACK);
        int marqueeWidth = _tft->width() - 30; 
        drawMarqueeText(_sprite, _font, latestNavigation.directions, 15, 160, marqueeWidth, TFT_WHITE, TFT_BLACK, true, _settings->marqueeSpeed);
        
        String footer = String(latestNavigation.distance) + " - " + String(latestNavigation.eta);
        _font->print((_tft->width() - _font->getLength(footer)) / 2, _tft->height() - 30, footer, TFT_WHITE, TFT_BLACK);
        _sprite->pushSprite(0, 0);
        return true;
    }

    if (hasNewNotification) {
        // ... (logic hiển thị tin nhắn giữ nguyên)
        return true;
    }

    return false;
}

// --- CÁC HÀM GETTER ---
bool chronos_is_time_synced() { return timeIsSynced; }
uint8_t chronos_get_hour() { return rtc_hour; }
uint8_t chronos_get_minute() { return rtc_minute; }
uint8_t chronos_get_second() { return rtc_second; }
uint8_t chronos_get_day() { return rtc_day; }
uint8_t chronos_get_month() { return rtc_month; }
uint16_t chronos_get_year() { return rtc_year; }
bool chronos_is_ringing() { return isRinging; }
bool chronos_has_new_notification() { return hasNewNotification; }
bool chronos_has_new_navigation() { return hasNewNavigation; }

// *** TRIỂN KHAI CÁC HÀM MỚI ***
bool chronos_has_weather_data() {
    return hasWeatherData;
}
WeatherData chronos_get_weather() {
    return latestWeather;
}
