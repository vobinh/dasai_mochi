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

// --- CẤU HÌNH NTP ---
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (7 * 3600)
#define DAYLIGHT_OFFSET_SEC 0

// --- BIẾN TOÀN CỤC ---
static TFT_eSPI *_tft;
static TFT_eSprite *_sprite;
static MakeFont *_font;
static AppSettings *_settings;

static WeatherData weatherStationData;
static bool weatherStationDataValid = false;
static uint32_t lastWeatherFetchTime = 0;
static String weatherStationStatus = "Đang khởi tạo...";
static struct tm timeinfo;
static bool timeSyncedNTP = false;

// Loading state (chỉ dùng cho lần vào đầu tiên)
static bool firstEnter = true;
static bool isLoading = false;

// --- GIỮ LẠI THỜI GIAN QUA REBOOT ---
RTC_DATA_ATTR time_t savedEpoch = 0;

// --- NGUYÊN MẪU HÀM ---
static void connectToWiFi();
void refetchWeatherData();
static void fetchWeatherData();
static int mapWeatherIcon(String iconCode);
static void initNTP();
static void drawWeatherStationScreen();
static void drawLoadingScreen(uint8_t percent, const String &statusText);
static void syncTimeToRTC(struct tm *timeinfo);
static void restoreTimeFromRTC();

// === KHỞI TẠO ===
void weather_station_init(TFT_eSPI *tft, TFT_eSprite *sprite, MakeFont *font, AppSettings *settings)
{
    _tft = tft;
    _sprite = sprite;
    _font = font;
    _settings = settings;
}

void weather_station_enter()
{
    weatherStationDataValid = false;
    weatherStationStatus = "Đang khởi tạo...";
    lastWeatherFetchTime = 0;
    timeSyncedNTP = false;

    firstEnter = true;
    isLoading = true;

    restoreTimeFromRTC(); // ✅ Khôi phục giờ cũ từ RTC nội nếu có
    drawLoadingScreen(3, "Bắt đầu...");
}

void weather_station_exit()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        WiFi.disconnect(true, true);
        delay(100);
        Serial.println("[Weather] WiFi disconnected.");
    }
    weatherStationDataValid = false;
    isLoading = false;
}

Mode weather_station_loop(ButtonAction action)
{
    if (action == ACTION_LONG)
        return MENU;
    else if (action == ACTION_DOUBLE){
        refetchWeatherData();
        return WEATHER_STATION_MODE;
    }
    
    // 1) WiFi
    if (WiFi.status() != WL_CONNECTED)
    {
        connectToWiFi();
        if (WiFi.status() != WL_CONNECTED)
        {
            if (firstEnter) drawLoadingScreen(10, weatherStationStatus);
            else drawWeatherStationScreen();
            return WEATHER_STATION_MODE;
        }
    }

    // 2) NTP
    if (!timeSyncedNTP)
    {
        initNTP();
        if (!timeSyncedNTP)
        {
            if (firstEnter) drawLoadingScreen(40, weatherStationStatus);
            else drawWeatherStationScreen();
            return WEATHER_STATION_MODE;
        }
    }

    // luôn cập nhật thời gian hiển thị theo RTC nội
    if (timeSyncedNTP) getLocalTime(&timeinfo);

    // 3) Thời tiết
    bool shouldFetch = (millis() - lastWeatherFetchTime > (10 * 60 * 1000)) || !weatherStationDataValid;
    if (shouldFetch)
    {
        fetchWeatherData();
    }

    // 4) Render
    if (firstEnter && (!weatherStationDataValid))
    {
        drawLoadingScreen(90, "Đang lấy dữ liệu...");
    }
    else
    {
        isLoading = false;
        firstEnter = false;
        drawWeatherStationScreen();
    }

    return WEATHER_STATION_MODE;
}

// === NTP ===
static void initNTP()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        weatherStationStatus = "Lỗi WiFi, không thể lấy giờ";
        return;
    }

    weatherStationStatus = "Đang đồng bộ giờ...";
    if (firstEnter) drawLoadingScreen(45, weatherStationStatus);

    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    unsigned long ntp_start_time = millis();
    while (!getLocalTime(&timeinfo, 1000))
    {
        // cập nhật thanh tiến trình trong lúc đợi (40 → 70)
        if (firstEnter)
        {
            uint32_t elapsed = millis() - ntp_start_time;
            uint8_t p = 40 + min<uint32_t>(30, (elapsed * 30) / 10000);
            drawLoadingScreen(p, "Đang đồng bộ giờ...");
        }
        if (millis() - ntp_start_time > 10000)
        {
            weatherStationStatus = "Lỗi đồng bộ giờ";
            timeSyncedNTP = false;
            return;
        }
    }

    timeSyncedNTP = true;
    syncTimeToRTC(&timeinfo);
    Serial.println("[NTP] ✅ Time synced and saved to RTC");
}

// === KẾT NỐI WIFI ===
static void connectToWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        weatherStationStatus = "WiFi đã kết nối";
        return;
    }

    weatherStationStatus = "Đang kết nối WiFi...";
    if (firstEnter) drawLoadingScreen(8, weatherStationStatus);

    if (_settings->stationSsid.length() == 0)
    {
        weatherStationStatus = "Chưa cài đặt WiFi";
        if (firstEnter) drawLoadingScreen(8, weatherStationStatus);
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(_settings->stationSsid.c_str(), _settings->stationPassword.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(300);
        Serial.print(".");
        if (firstEnter)
        {
            // 10 → 40 trong vòng 15s
            uint32_t elapsed = millis() - startTime;
            uint8_t p = 10 + min<uint32_t>(30, (elapsed * 30) / 15000);
            drawLoadingScreen(p, "Đang kết nối WiFi...");
        }
        if (millis() - startTime > 15000)
        {
            Serial.println("\n[WiFi] connection failed!");
            weatherStationStatus = "Lỗi kết nối WiFi";
            return;
        }
    }

    Serial.println("\n[WiFi] connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    weatherStationStatus = "Đã kết nối";
    if (firstEnter) drawLoadingScreen(40, "WiFi đã kết nối");
}

// === LƯU/KHÔI PHỤC GIỜ RTC NỘI ===
static void syncTimeToRTC(struct tm *timeinfo)
{
    time_t now;
    time(&now);
    savedEpoch = now;
    Serial.printf("[RTC] ✅ Lưu thời gian vào RTC nội (epoch=%ld)\n", savedEpoch);
}

static void restoreTimeFromRTC()
{
    if (savedEpoch == 0)
    {
        Serial.println("[RTC] ⚠️ Chưa có thời gian lưu trước đó");
        return;
    }

    struct timeval tv = { .tv_sec = savedEpoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);

    if (getLocalTime(&timeinfo))
    {
        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M:%S %d/%m/%Y", &timeinfo);
        Serial.printf("[RTC] 🔄 Phục hồi thời gian: %s\n", buf);
        timeSyncedNTP = true; // đã có giờ hợp lệ để hiển thị
    }
    else
    {
        Serial.println("[RTC] ❌ Không thể phục hồi thời gian");
    }
}

void refetchWeatherData() {
    fetchWeatherData();
}

// === FETCH WEATHER ===
static void fetchWeatherData()
{
    Serial.println("[Weather] Starting fetchWeatherData...");

    weatherStationStatus = "Đang cập nhật...";
    if (firstEnter) drawLoadingScreen(72, weatherStationStatus);

    if (_settings->owmApiKey.length() == 0)
    {
        weatherStationStatus = "Chưa có API Key";
        if (firstEnter) drawLoadingScreen(72, weatherStationStatus);
        return;
    }

    HTTPClient http;
    String url;

    Serial.println("[latitude] Request: ");
    Serial.println(_settings->latitude);
    Serial.println("[longitude] Request: ");
    Serial.println(_settings->longitude);

    if (_settings->latitude.length() != 0 && _settings->longitude.length() != 0)
    {
        url = "http://api.openweathermap.org/data/2.5/weather?lat=" + _settings->latitude +
            "&lon=" + _settings->longitude +
            "&appid=" + _settings->owmApiKey +
            "&units=metric&lang=" + _settings->language;
    }
    else
    {
        url = "http://api.openweathermap.org/data/2.5/weather?id=" + _settings->owmCityId +
            "&appid=" + _settings->owmApiKey +
            "&units=metric&lang=" + _settings->language;
    }


    Serial.println("[Weather] Request: " + url);
    http.setTimeout(7000);
    http.begin(url);

    if (firstEnter) drawLoadingScreen(78, "Đang gọi API...");

    int httpCode = http.GET();
    Serial.printf("[Weather] HTTP Code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK)
    {
        if (firstEnter) drawLoadingScreen(88, "Đang nhận dữ liệu...");
        String payload = http.getString();
        if (firstEnter) drawLoadingScreen(92, "Đang phân tích...");

        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error)
        {
            Serial.print("[Weather] JSON error: ");
            Serial.println(error.f_str());
            weatherStationStatus = "Lỗi phân tích dữ liệu";
            http.end();
            return;
        }
        String cityName = doc["name"].as<String>();
        cityName = replaceAllUtf8(cityName, "City", "");
        cityName = replaceAllUtf8(cityName, "Thành phố", "");
        cityName.trim();
        weatherStationData.city = cityName;
        weatherStationData.currentTemp = round(doc["main"]["temp"].as<float>());
        weatherStationData.highTemp = round(doc["main"]["temp_max"].as<float>());
        weatherStationData.lowTemp = round(doc["main"]["temp_min"].as<float>());
        weatherStationData.pressure = doc["main"]["humidity"].as<int>();
        // Lưu gió (m/s) nếu bạn cần hiển thị về sau
        // weatherStationData.wind_speed = doc["wind"]["speed"].as<float>();
        weatherStationData.uv = doc["wind"]["speed"].as<float>() * 3.6; // tạm dùng UV = tốc độ gió (km/h)
        weatherStationData.updateTime = doc["weather"][0]["description"].as<String>();
        weatherStationData.updateTime.toUpperCase();

        String iconCode = doc["weather"][0]["icon"];
        weatherStationData.icon = mapWeatherIcon(iconCode);

        weatherStationDataValid = true;
        lastWeatherFetchTime = millis();

        if (timeSyncedNTP)
        {
            char timeStr[6];
            sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            weatherStationStatus = "Cập nhật lúc " + String(timeStr);
        }

        if (firstEnter) drawLoadingScreen(100, "Hoàn tất");
        Serial.println("[Weather] ✅ Data updated.");
    }
    else
    {
        weatherStationStatus = "Lỗi HTTP: " + String(httpCode);
        Serial.println(weatherStationStatus);
        if (firstEnter) drawLoadingScreen(80, weatherStationStatus);
    }
    http.end();
}

// === ICON MAP ===
static int mapWeatherIcon(String iconCode)
{
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
static void drawLoadingScreen(uint8_t percent, const String &statusText)
{
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
}

// === DRAW MAIN SCREEN ===
static void drawWeatherStationScreen()
{
    if (!weatherStationDataValid || !timeSyncedNTP)
    {
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

    _sprite->setTextFont(4);
    char dateStr[11];
    sprintf(dateStr, "%02d/%02d/%d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
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
        weatherStationData.pressure
    );

    if (_font->getLength(otherStr) <= 150)
        _font->print(6, 220, otherStr, TFT_WHITE, TFT_BLACK);
    else
        drawMarqueeText(_sprite, _font, otherStr, 6, 220, 150, TFT_WHITE, TFT_BLACK, true, 35);

    _sprite->pushSprite(0, 0);
}
