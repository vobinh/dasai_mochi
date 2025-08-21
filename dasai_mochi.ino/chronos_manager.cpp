#include "chronos_manager.h"
#include <ChronosESP32.h>
#include <vector>
#include <time.h>

// --- CÁC BIẾN TĨNH ---
static TFT_eSPI* _tft;
static TFT_eSprite* _sprite;
static MakeFont* _font;
static AppSettings* _settings;
static ChronosESP32 Chronos("Mochi Watch");
static Notification latestNotification;
static String callerInfo;
static bool hasNewNotification = false;
static bool isConnected = false;
static bool isRinging = false;
static unsigned long notificationDisplayTime = 0;
static std::vector<String> wrappedMessageLines;
static int messageScrollLine = 0;
static unsigned long lastMessageScrollTime = 0;
static bool isNotificationScrolling = false;
static bool timeIsSynced = false;
static uint8_t rtc_hour, rtc_minute, rtc_second, rtc_day, rtc_month;
static uint16_t rtc_year;

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
    const int maxWidth = _tft->width() - 40; // Thêm padding cho text
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
    notificationDisplayTime = millis();
    wrapMessage(latestNotification.message); 
    messageScrollLine = 0; 
    lastMessageScrollTime = millis();
    isNotificationScrolling = (wrappedMessageLines.size() > 7); // 7 dòng tối đa
}

// --- CÁC HÀM CÔNG KHAI ---
void chronos_init(TFT_eSPI* tft, TFT_eSprite* sprite, MakeFont* font, AppSettings* settings) {
    _tft = tft; _sprite = sprite; _font = font; _settings = settings;
    Chronos.setConnectionCallback(connectionCallback);
    Chronos.setNotificationCallback(notificationCallback);
    Chronos.setRingerCallback(ringerCallback); 
    Chronos.begin(); 
}

void chronos_loop() {
    Chronos.loop();
    getTimeFromRTC();
}

// *** HÀM ĐÃ ĐƯỢC TÁI CẤU TRÚC HOÀN TOÀN ***
bool chronos_draw_alerts() {
    if (isRinging) {
        _sprite->fillSprite(TFT_BLACK);
        _font->print((_tft->width() - _font->getLength("CUỘC GỌI ĐẾN")) / 2, 30, "CUỘC GỌI ĐẾN", TFT_WHITE, TFT_BLACK);
        _font->print((_tft->width() - _font->getLength(callerInfo)) / 2, _tft->height() / 2, callerInfo, TFT_WHITE, TFT_BLACK);
        _sprite->fillCircle(_tft->width() / 4, _tft->height() - 50, 30, TFT_GREEN);
        _sprite->fillCircle(_tft->width() * 3 / 4, _tft->height() - 50, 30, TFT_RED);
        _sprite->pushSprite(0, 0);
        return true; // Có cảnh báo đang hiển thị
    }

    if (hasNewNotification) {
        if (millis() - notificationDisplayTime > _settings->notificationTimeout * 1000) {
            hasNewNotification = false;
            return false; // Hết thời gian, không còn cảnh báo
        }

        // *** KHÔI PHỤC LẠI GIAO DIỆN TIN NHẮN ĐẦY ĐỦ ***
        _sprite->fillSprite(TFT_BLACK);
        const int boxX = 5, boxY = 10, boxW = 230, boxH = 200, cornerRadius = 10;
        const int textPadding = 8;
        const int lineHeight = 22;
        _sprite->drawRoundRect(boxX, boxY, boxW, boxH, cornerRadius, TFT_CYAN);
        String appName = latestNotification.app;
        _font->print(_tft->width() - _font->getLength(appName) - 15, _tft->height() - 30, appName, TFT_CYAN, TFT_BLACK);
        
        int maxLines = (boxH - textPadding * 2) / lineHeight;
        if (isNotificationScrolling) {
            if (millis() - lastMessageScrollTime > 1500) {
                lastMessageScrollTime = millis();
                messageScrollLine++;
                if (messageScrollLine > wrappedMessageLines.size() - maxLines) {
                    messageScrollLine = 0; // Quay lại từ đầu
                }
            }
        }
        
        for (int i = 0; i < maxLines; i++) {
            int lineIndex = messageScrollLine + i;
            if (lineIndex < wrappedMessageLines.size()) {
                _font->print(boxX + textPadding, boxY + textPadding + i * lineHeight, wrappedMessageLines[lineIndex], TFT_WHITE, TFT_BLACK);
            }
        }
        _sprite->pushSprite(0, 0);
        return true; // Có cảnh báo đang hiển thị
    }

    return false; // Không có cảnh báo nào
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
