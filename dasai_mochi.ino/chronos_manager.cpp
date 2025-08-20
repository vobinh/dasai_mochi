#include "chronos_manager.h"
#include <ChronosESP32.h>
#include <vector>
#include <time.h>

// --- CÁC BIẾN TOÀN CỤC (CHỈ DÙNG TRONG FILE NÀY) ---
static TFT_eSPI* _tft;
static TFT_eSprite* _sprite;
static MakeFont* _font;

static ChronosESP32 Chronos("Mochi Watch");

static Notification latestNotification;
static Navigation latestNavigation;
static String callerInfo;
static bool hasNewNotification = false;
static bool hasNewNavigation = false;
static bool isConnected = false;
static bool isRinging = false;
static unsigned long notificationDisplayTime = 0;
static uint32_t nav_icon_crc = 0xFFFFFFFF; 

static std::vector<String> wrappedMessageLines;
static int messageScrollLine = 0;
static unsigned long lastMessageScrollTime = 0;
static bool isNotificationScrolling = false;

// Biến RTC giờ đây được quản lý trong file này
static bool timeIsSynced = false;
static uint8_t rtc_hour, rtc_minute, rtc_second, rtc_day, rtc_month;
static uint16_t rtc_year;

extern int notificationTimeout; 

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
    Serial.println("RTC Synced from Bluetooth.");
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
    const int maxWidth = _tft->width() - 20;
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
        if (_font->getLength(testLine) <= maxWidth) {
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

// --- CÁC HÀM CALLBACK CỦA CHRONOS ---

static void connectionCallback(bool state) {
    isConnected = state;
    if (state) {
        syncTimeToRTC();
    }
}

static void notificationCallback(Notification notification) {
    latestNotification = notification;
    hasNewNotification = true;
    hasNewNavigation = false; 
    notificationDisplayTime = millis();
    wrapMessage(latestNotification.message); 
    messageScrollLine = 0; 
    lastMessageScrollTime = millis();
    
    const int boxH = 200;
    const int lineHeight = 22;
    const int textPadding = 8;
    int maxLines = (boxH - textPadding * 2) / lineHeight;
    isNotificationScrolling = (wrappedMessageLines.size() > maxLines);
}

static void ringerCallback(String caller, bool state) {
    callerInfo = caller;
    isRinging = state;
}

static void configCallback(Config config, uint32_t a, uint32_t b) {
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


// --- CÁC HÀM CÔNG KHAI ---

void chronos_init(TFT_eSPI* tft, TFT_eSprite* sprite, MakeFont* font) {
    _tft = tft;
    _sprite = sprite;
    _font = font;

    Chronos.setConnectionCallback(connectionCallback);
    Chronos.setNotificationCallback(notificationCallback);
    Chronos.setConfigurationCallback(configCallback);
    Chronos.setRingerCallback(ringerCallback); 
    Chronos.begin(); 
}

void chronos_loop() {
    Chronos.loop();
    getTimeFromRTC(); // Cập nhật thời gian RTC liên tục
}

void chronos_draw_watch_face() {
    bool scrollFinished = false;
    if (isNotificationScrolling) {
        const int boxH = 200;
        const int textPadding = 8;
        const int lineHeight = 22;
        int maxLines = (boxH - textPadding * 2) / lineHeight;
        if (wrappedMessageLines.size() <= maxLines || messageScrollLine > wrappedMessageLines.size() - maxLines) {
            scrollFinished = true;
        }
    }

    if (hasNewNotification) {
        bool hideNow = false;
        if (!isNotificationScrolling) { 
            if (millis() - notificationDisplayTime > notificationTimeout * 1000) {
                hideNow = true;
            }
        } else if (scrollFinished) { 
            if (millis() - lastMessageScrollTime > notificationTimeout * 1000) {
                hideNow = true;
            }
        }
        if (millis() - notificationDisplayTime > 10000) {
            hideNow = true;
        }
        if (hideNow) {
            hasNewNotification = false;
        }
    }

    _sprite->fillSprite(TFT_BLACK);
    
    if (isRinging) {
        _font->print((_tft->width() - _font->getLength("CUỘC GỌI ĐẾN")) / 2, 30, "CUỘC GỌI ĐẾN", TFT_WHITE, TFT_BLACK);
        _font->print((_tft->width() - _font->getLength(callerInfo)) / 2, _tft->height() / 2, callerInfo, TFT_WHITE, TFT_BLACK);
        _sprite->fillCircle(_tft->width() / 4, _tft->height() - 50, 30, TFT_GREEN);
        _sprite->fillCircle(_tft->width() * 3 / 4, _tft->height() - 50, 30, TFT_RED);

    } else if (hasNewNavigation) {
        if (nav_icon_crc != 0xFFFFFFFF) {
            int iconSize = 96;
            int pixelSize = iconSize / 48;
            for (int y = 0; y < 48; y++) {
                for (int x = 0; x < 48; x++) {
                    int byte_index = (y * 48 + x) / 8;
                    int bit_pos = 7 - (x % 8);
                    bool px_on = (latestNavigation.icon[byte_index] >> bit_pos) & 0x01;
                    if (px_on) {
                        _sprite->fillRect(10 + x * pixelSize, 10 + y * pixelSize, pixelSize, pixelSize, TFT_WHITE);
                    }
                }
            }
        }
        _font->print(15, 120, latestNavigation.title, TFT_WHITE, TFT_BLACK);
        _font->print(15, 160, latestNavigation.directions, TFT_WHITE, TFT_BLACK);
        String footer = String(latestNavigation.distance) + " - " + String(latestNavigation.eta);
        _font->print((_tft->width() - _font->getLength(footer)) / 2, _tft->height() - 30, footer, TFT_WHITE, TFT_BLACK);
        
    } else if (hasNewNotification) {
        const int boxX = 5, boxY = 10, boxW = 230, boxH = 200, cornerRadius = 10;
        const int textPadding = 8;
        const int lineHeight = 22;

        _sprite->drawRoundRect(boxX, boxY, boxW, boxH, cornerRadius, TFT_CYAN);

        String appName = latestNotification.app;
        _font->print(_tft->width() - _font->getLength(appName) - 15, _tft->height() - 30, appName, TFT_CYAN, TFT_BLACK);
        
        int maxLines = (boxH - textPadding * 2) / lineHeight;

        if (isNotificationScrolling && !scrollFinished) {
            if (millis() - lastMessageScrollTime > 1500) {
                lastMessageScrollTime = millis();
                messageScrollLine++;
            }
        }
        
        _sprite->fillRect(boxX + textPadding, boxY + textPadding, boxW - textPadding*2, boxH - textPadding*2, TFT_BLACK);

        for (int i = 0; i < maxLines; i++) {
            int lineIndex = messageScrollLine + i;
            if (lineIndex < wrappedMessageLines.size()) {
                _font->print(boxX + textPadding, boxY + textPadding + i * lineHeight, wrappedMessageLines[lineIndex], TFT_WHITE, TFT_BLACK);
            }
        }

    } else if (!isConnected) {
        String msg = "Đã ngắt kết nối";
        _font->print((_tft->width() - _font->getLength(msg)) / 2, _tft->height() / 2, msg, TFT_RED, TFT_BLACK);

    } else { 
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", rtc_hour, rtc_minute);
        _font->print((_tft->width() - _font->getLength(timeStr))/2, _tft->height() / 2 - 10, timeStr, TFT_WHITE, TFT_BLACK);
        _font->print((_tft->width() - _font->getLength(timeStr))/2 + 1, _tft->height() / 2 - 10, timeStr, TFT_WHITE, TFT_BLACK);
    }
    
    _sprite->pushSprite(0, 0);
}

// --- Các hàm getter ---
bool chronos_is_time_synced() { return timeIsSynced; }
uint8_t chronos_get_hour() { return rtc_hour; }
uint8_t chronos_get_minute() { return rtc_minute; }
uint8_t chronos_get_second() { return rtc_second; }
uint8_t chronos_get_day() { return rtc_day; }
uint8_t chronos_get_month() { return rtc_month; }
uint16_t chronos_get_year() { return rtc_year; }
