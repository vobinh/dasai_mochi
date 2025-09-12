#include "navigation_background.h"

// =======================================================================================
//  Triển khai (Implementation)
// =======================================================================================

namespace NavigationBackground {

// ---- Cấu hình & Hằng số ----
static const int SCREEN_W = 240;
static const int SCREEN_H = 240;
static const int LANES    = 3;
static const int ROAD_W   = 150;
static const int ROAD_X   = (SCREEN_W - ROAD_W) / 2;
static const int LANE_W   = ROAD_W / LANES;
static const int LANE_X[LANES] = { ROAD_X + LANE_W/2, ROAD_X + LANE_W + LANE_W/2, ROAD_X + 2*LANE_W + LANE_W/2 };

static const int CAR_W = 28;
static const int CAR_H = 45;
static const int CAR_Y = SCREEN_H - CAR_H - 10;

static const uint16_t COL_GRASS_DAY = 0x0400; 
static const uint16_t COL_GRASS_NIGHT = 0x0200;
static const uint16_t COL_ROAD_DAY  = 0x4228; 
static const uint16_t COL_ROAD_NIGHT = 0x2124;
static const uint16_t COL_LINE  = TFT_DARKGREY;
static const uint16_t COL_CAR   = TFT_CYAN;

// ---- Trạng thái ----
struct Streetlight { int x; int y; bool active; bool is_left; };
static const int MAX_STREETLIGHTS = 4;

static TFT_eSprite* _sprite = nullptr;
static Streetlight streetlights[MAX_STREETLIGHTS];
static int currentSpeedPx = 4;
static int lineOffset = 0; 
static uint8_t playerLane = 1;

// ---- Các hàm nội bộ ----
static inline int laneCenterX(int lane){ return LANE_X[lane]; }

static void drawDetailedCar(int x, int y, uint16_t bodyColor, bool lights_on) {
    _sprite->fillRoundRect(x, y, CAR_W, CAR_H, 6, bodyColor);
    _sprite->fillRoundRect(x + 2, y + 5, CAR_W - 4, 10, 4, TFT_DARKGREY);
    _sprite->fillRoundRect(x + 4, y + 8, CAR_W - 8, 18, 4, TFT_SKYBLUE);
    
    uint16_t headlight_color = lights_on ? TFT_YELLOW : TFT_WHITE;
    _sprite->fillRect(x, y + 4, 4, 6, headlight_color);
    _sprite->fillRect(x + CAR_W - 4, y + 4, 4, 6, headlight_color);

    _sprite->fillRect(x, y + CAR_H - 8, 4, 6, TFT_RED);
    _sprite->fillRect(x + CAR_W - 4, y + CAR_H - 8, 4, 6, TFT_RED);
}

static void drawStreetlight(const Streetlight& light, bool is_on) {
    if (!light.active) return;
    
    int pole_height = 50;
    int pole_width = 4;
    int arm_length = 20;

    _sprite->fillRect(light.x - pole_width/2, light.y - pole_height, pole_width, pole_height, TFT_DARKGREY);
    
    int arm_y = light.y - pole_height;
    if (light.is_left) {
        _sprite->fillRect(light.x, arm_y, arm_length, pole_width, TFT_DARKGREY);
    } else {
        _sprite->fillRect(light.x - arm_length, arm_y, arm_length, pole_width, TFT_DARKGREY);
    }

    int lamp_x = light.is_left ? (light.x + arm_length) : (light.x - arm_length);
    int lamp_y = arm_y;

    if (is_on) {
        _sprite->fillCircle(lamp_x, lamp_y, 9, _sprite->color565(180, 180, 0));
        _sprite->fillCircle(lamp_x, lamp_y, 5, TFT_YELLOW);
        _sprite->fillCircle(lamp_x, lamp_y, 2, TFT_WHITE);
    } else {
        _sprite->fillCircle(lamp_x, lamp_y, 5, TFT_GOLD);
    }
}

static void spawnStreetlight(){
    static bool spawn_on_left = true;
    for(int i=0; i<MAX_STREETLIGHTS; i++) {
        if(!streetlights[i].active){
            streetlights[i].active = true;
            streetlights[i].y = -20;
            streetlights[i].is_left = spawn_on_left;
            if(spawn_on_left) {
                streetlights[i].x = random(15, ROAD_X - 25);
            } else {
                streetlights[i].x = random(ROAD_X + ROAD_W + 25, SCREEN_W - 15);
            }
            spawn_on_left = !spawn_on_left;
            break;
        }
    }
}

// ---- Public API ----
void begin(TFT_eSprite* sprite){ 
    _sprite = sprite; 
    randomSeed(millis()); 
    reset();
}

void reset() {
    for(int i=0; i<MAX_STREETLIGHTS; i++) streetlights[i] = {0,0,false,false};
    currentSpeedPx = 4;
}

void setSpeed(int speed) {
    currentSpeedPx = speed;
}

void tick(){
  if (currentSpeedPx > 0) {
    lineOffset = (lineOffset + currentSpeedPx) % 32;
    for(int i=0; i<MAX_STREETLIGHTS; i++) {
        if(streetlights[i].active){
          streetlights[i].y += currentSpeedPx;
          if(streetlights[i].y > SCREEN_H + 20) { streetlights[i].active=false; }
        }
    }
    
    static unsigned long last_spawn = 0;
    if (millis() - last_spawn > (1600 / (currentSpeedPx / 2.0f) )) {
        last_spawn = millis();
        spawnStreetlight();
    }
  }
}

// *** HÀM VẼ ĐÃ ĐƯỢC CẬP NHẬT VỚI HIỆU ỨNG ÁNH SÁNG MỀM MẠI HƠN ***
void draw(TFT_eSprite* target_sprite, int current_hour) {
    _sprite = target_sprite;
    bool is_night = (current_hour >= 18 || current_hour < 6);

    uint16_t grass_color = is_night ? COL_GRASS_NIGHT : COL_GRASS_DAY;
    uint16_t road_color = is_night ? COL_ROAD_NIGHT : COL_ROAD_DAY;

    // 1. Vẽ các lớp nền
    _sprite->fillRect(0, 0, ROAD_X, SCREEN_H, grass_color);
    _sprite->fillRect(ROAD_X + ROAD_W, 0, SCREEN_W - (ROAD_X + ROAD_W), SCREEN_H, grass_color);
    _sprite->fillRect(ROAD_X, 0, ROAD_W, SCREEN_H, road_color);
    
    // 2. Nếu là ban đêm, vẽ tất cả các hiệu ứng ánh sáng trên mặt đất trước
    if (is_night) {
        uint16_t ground_glow_color = _sprite->color565(100, 100, 0); // Đậm hơn
        uint16_t beam_color = _sprite->color565(80, 80, 0);       // Nhạt hơn

        // 2a. Vẽ ánh sáng đèn đường trên mặt đất
        for(int i=0; i<MAX_STREETLIGHTS; i++) {
            if (streetlights[i].active) {
                int lamp_x = streetlights[i].is_left ? (streetlights[i].x + 20) : (streetlights[i].x - 20);
                // Chùm sáng tam giác nhạt hơn
                _sprite->fillTriangle(lamp_x, streetlights[i].y - 50, lamp_x - 10, streetlights[i].y, lamp_x + 10, streetlights[i].y, ground_glow_color);
                // Vầng elip đậm
                _sprite->fillEllipse(lamp_x, streetlights[i].y, 25, 8, ground_glow_color);
            }
        }

        // 2b. Vẽ ánh sáng đèn pha xe trên mặt đất (phiên bản cải tiến)
        int player_center_x = laneCenterX(playerLane);
        int beam_y_start = CAR_Y;
        int beam_y_end = 80; // Điểm cuối của chùm sáng

        // Vẽ nhiều lớp elip để tạo hiệu ứng mềm mại
        _sprite->fillEllipse(player_center_x, beam_y_start - 5, 20, 15, _sprite->color565(120, 120, 0)); // Lớp sáng nhất, gần xe nhất
        _sprite->fillEllipse(player_center_x, beam_y_start - 20, 35, 30, ground_glow_color);
        _sprite->fillEllipse(player_center_x, beam_y_start - 45, 50, 45, beam_color); // Lớp rộng nhất, mờ nhất
    }
    
    // 3. Vẽ vạch kẻ đường (đè lên trên các vầng sáng)
    for(int i = 1; i < LANES; i++){
      int x = ROAD_X + i * LANE_W;
      for(int y = lineOffset - 32; y < SCREEN_H; y += 32){ 
        _sprite->drawFastVLine(x, y, 16, COL_LINE); 
      }
    }

    // 4. Vẽ các cột đèn đường
    for(int i=0; i<MAX_STREETLIGHTS; i++) drawStreetlight(streetlights[i], is_night);
    
    // 5. Vẽ xe của người chơi (lớp trên cùng)
    int playerX = laneCenterX(playerLane) - CAR_W/2;
    drawDetailedCar(playerX, CAR_Y, COL_CAR, is_night);
}

} // namespace NavigationBackground

