// Setup_ST7789_ESP32C3_7PIN.h
#define USER_SETUP_ID 900

// ----- Driver & size -----
#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 240
// Nếu màu sai, mở dòng dưới:
// #define TFT_RGB_ORDER TFT_BGR

// ----- Pin mapping ESP32-C3 -----
#define TFT_MOSI 7   // SDA
#define TFT_SCLK 6   // SCL
#define TFT_DC   2   // SDC (Data/Command)
#define TFT_RST 10   // RE  (Reset)
#define TFT_CS  -1   // Không có CS
#define TFT_MISO -1  // Không đọc

// ----- Fonts (tuỳ chọn) -----
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT

// ----- SPI speed -----
#define SPI_FREQUENCY 27000000   // Bắt đầu 27 MHz cho chắc; ổn rồi nâng 40 MHz
//#define SPI_READ_FREQUENCY 20000000
