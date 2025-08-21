#include "chronos_manager.h"
#include <ChronosESP32.h>
#include <vector>
#include <time.h>

// =======================================================================================
// --- CẤU TRÚC DỮ LIỆU VÀ BIẾN CHO HIỆU ỨNG MỚI ---
// =======================================================================================
struct RainParticle {
    float x, y;
    float speed;
    char character;
};

#define NUM_RAIN_PARTICLES 100
static std::vector<RainParticle> rainParticles;

// --- CÁC BIẾN TOÀN CỤC (CHỈ DÙNG TRONG FILE NÀY) ---
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
static unsigned long notificationDisplayTime = 0;
static uint32_t nav_icon_crc = 0xFFFFFFFF; 

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

void chronos_init(TFT_eSPI* tft, TFT_eSprite* sprite, MakeFont* font, AppSettings* settings) {
    _tft = tft;
    _sprite = sprite;
    _font = font;
    _settings = settings;

    // Khởi tạo các hạt mưa số
    for (int i = 0; i < NUM_RAIN_PARTICLES; i++) {
        RainParticle p;
        p.x = random(0, _tft->width());
        p.y = random(0, _tft->height());
        p.speed = random(2, 6);
        p.character = (char)random(48, 58); // Ký tự số từ '0' đến '9'
        rainParticles.push_back(p);
    }

    Chronos.setConnectionCallback(connectionCallback);
    Chronos.setNotificationCallback(notificationCallback);
    Chronos.setConfigurationCallback(configCallback);
    Chronos.setRingerCallback(ringerCallback); 
    Chronos.begin(); 
}

void chronos_loop() {
    Chronos.loop();
    getTimeFromRTC();
}

// *** HÀM VẼ MẶT ĐỒNG HỒ ĐÃ ĐƯỢC VIẾT LẠI HOÀN TOÀN ***
void chronos_draw_watch_face() {
    // Xử lý ẩn thông báo
    if (hasNewNotification && (millis() - notificationDisplayTime > _settings->notificationTimeout * 1000)) {
        hasNewNotification = false;
    }

    _sprite->fillSprite(TFT_BLACK);

    // --- VẼ HIỆU ỨNG NỀN ---
    for (auto& p : rainParticles) {
        // Cập nhật vị trí
        p.y += p.speed;
        if (p.y > _tft->height()) {
            p.y = 0;
            p.x = random(0, _tft->width());
        }
        // Vẽ hạt mưa với màu tối
        _sprite->drawChar(p.x, p.y, p.character, TFT_DARKGREEN, TFT_BLACK, 1);
    }
    
    // --- VẼ CÁC THÔNG TIN CHÍNH ---
    if (isRinging) {
        _font->print((_tft->width() - _font->getLength("CUỘC GỌI ĐẾN")) / 2, 30, "CUỘC GỌI ĐẾN", TFT_WHITE, TFT_BLACK);
        _font->print((_tft->width() - _font->getLength(callerInfo)) / 2, _tft->height() / 2, callerInfo, TFT_WHITE, TFT_BLACK);
        _sprite->fillCircle(_tft->width() / 4, _tft->height() - 50, 30, TFT_GREEN);
        _sprite->fillCircle(_tft->width() * 3 / 4, _tft->height() - 50, 30, TFT_RED);
    } else if (hasNewNavigation) {
        _font->print(15, 120, latestNavigation.title, TFT_WHITE, TFT_BLACK);
        _font->print(15, 160, latestNavigation.directions, TFT_WHITE, TFT_BLACK);
        String footer = String(latestNavigation.distance) + " - " + String(latestNavigation.eta);
        _font->print((_tft->width() - _font->getLength(footer)) / 2, _tft->height() - 30, footer, TFT_WHITE, TFT_BLACK);
    } else if (hasNewNotification) {
        _font->print(15, 30, latestNotification.title, TFT_WHITE, TFT_BLACK);
        _font->print(15, 60, latestNotification.message, TFT_WHITE, TFT_BLACK);
    } else if (!isConnected) {
        String msg = "ĐÃ NGẮT KẾT NỐI";
        _font->print((_tft->width() - _font->getLength(msg)) / 2, _tft->height() / 2, msg, TFT_RED, TFT_BLACK);
    } else { 
        // --- VẼ ĐỒNG HỒ SỐ VỚI FONT MỚI ---
        // _sprite->loadFont(digital_font);
        _sprite->setFreeFont(&DigitaltsLime35pt7b);
        
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d:%02d", rtc_hour, rtc_minute, rtc_second);
        
        // Căn giữa thời gian trên màn hình
        int textWidth = _sprite->textWidth(timeStr);
        int textHeight = 50; // Chiều cao của font
        int x_pos = (_tft->width() - textWidth) / 2;
        int y_pos = (_tft->height() - textHeight) / 2;

        _sprite->setTextColor(TFT_CYAN, TFT_BLACK);
        _sprite->drawString(timeStr, x_pos, y_pos);
        _sprite->setFreeFont(NULL);
        // _sprite->unloadFont();

        // Vẽ ngày tháng và giây bằng font cũ
        char dateStr[10];
        sprintf(dateStr, "%02d/%02d/%d", rtc_day, rtc_month, rtc_year);
        _font->print((_tft->width() - _font->getLength(dateStr)) / 2, y_pos + textHeight + 10, dateStr, TFT_WHITE, TFT_BLACK);
        
        // char secStr[4];
        // sprintf(secStr, ":%02d", rtc_second);
        // _font->print((_tft->width() - _font->getLength(secStr)) / 2, y_pos - 20, secStr, TFT_WHITE, TFT_BLACK);
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
