#include "navigation_background.h"
#include <cmath>

namespace NavigationBackground {

// Enum nội bộ để quản lý trạng thái animation
enum AnimState {
    STATE_STRAIGHT,
    STATE_TURNING_LEFT,
    STATE_TURNING_RIGHT,
    STATE_U_TURN
};

// ---- Cấu hình & Hằng số ----
static const int SCREEN_W = 240;
static const int SCREEN_H = 240;
static const int ROAD_W   = 150;
static const int ROAD_X   = (SCREEN_W - ROAD_W) / 2;
static const int LANE_W   = ROAD_W / 3;
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
static int lineOffset = 0; 
static AnimState current_anim_state = STATE_STRAIGHT;
static float animation_progress = 1.0f;
static float player_rotation = 0.0f;
static float player_x_offset = 0.0f;
static int anim_speed = 4;

// ---- Các hàm nội bộ ----
static void drawDetailedCar(TFT_eSprite* s, int x, int y, uint16_t bodyColor, bool lights_on) {
    s->fillRoundRect(x, y, CAR_W, CAR_H, 6, bodyColor);
    s->fillRoundRect(x + 2, y + 5, CAR_W - 4, 10, 4, TFT_DARKGREY);
    s->fillRoundRect(x + 4, y + 8, CAR_W - 8, 18, 4, TFT_SKYBLUE);
    uint16_t headlight_color = lights_on ? TFT_YELLOW : TFT_WHITE;
    s->fillRect(x, y + 4, 4, 6, headlight_color);
    s->fillRect(x + CAR_W - 4, y + 4, 4, 6, headlight_color);
    s->fillRect(x, y + CAR_H - 8, 4, 6, TFT_RED);
    s->fillRect(x + CAR_W - 4, y + CAR_H - 8, 4, 6, TFT_RED);
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

static void spawnStreetlight() {
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
    current_anim_state = STATE_STRAIGHT;
    animation_progress = 1.0f;
    player_rotation = 0.0f;
    player_x_offset = 0.0f;
    anim_speed = 4;
}

void setSpeed(int speed) {
    anim_speed = speed;
}

void triggerAnimation(NavInstructionType type) {
    if (animation_progress < 1.0f) return;

    switch(type) {
        case NAV_TURN_LEFT: current_anim_state = STATE_TURNING_LEFT; animation_progress = 0.0f; break;
        case NAV_TURN_RIGHT: current_anim_state = STATE_TURNING_RIGHT; animation_progress = 0.0f; break;
        case NAV_U_TURN: current_anim_state = STATE_U_TURN; animation_progress = 0.0f; break;
        default: break;
    }
}

void tick(){
    if (animation_progress < 1.0f) {
        animation_progress += 0.02f;
        if (animation_progress >= 1.0f) {
            current_anim_state = STATE_STRAIGHT;
            reset(); 
        }
    }

    if (current_anim_state == STATE_STRAIGHT && anim_speed > 0) {
        lineOffset = (lineOffset + anim_speed) % 32;
        for(int i=0; i<MAX_STREETLIGHTS; i++) {
            if(streetlights[i].active){
              streetlights[i].y += anim_speed;
              if(streetlights[i].y > SCREEN_H + 20) { streetlights[i].active=false; }
            }
        }
        static unsigned long last_spawn = 0;
        if (millis() - last_spawn > (1600 / (anim_speed / 2.0f))) {
            last_spawn = millis();
            spawnStreetlight();
        }
    }
}

void draw(TFT_eSprite* target_sprite, int current_hour) {
    _sprite = target_sprite;
    bool is_night = (current_hour >= 18 || current_hour < 6);
    
    uint16_t grass_color = is_night ? COL_GRASS_NIGHT : COL_GRASS_DAY;
    uint16_t road_color = is_night ? COL_ROAD_NIGHT : COL_ROAD_DAY;

    _sprite->fillRect(0, 0, SCREEN_W, SCREEN_H, grass_color);
    
    // --- VẼ ĐƯỜNG ---
    float t = animation_progress;
    float sin_t = sin(t * PI);

    if (current_anim_state == STATE_TURNING_LEFT || current_anim_state == STATE_TURNING_RIGHT) {
        float turn_direction = (current_anim_state == STATE_TURNING_LEFT) ? -1.0 : 1.0;
        for(int y=0; y < SCREEN_H; y++) {
            float perspective = (float)y / SCREEN_H;
            float curve = sin_t * (1.0 - perspective) * 80.0 * turn_direction;
            _sprite->drawFastHLine(ROAD_X - (perspective * 50) + curve, y, ROAD_W + (perspective * 100), road_color);
        }
    } else if (current_anim_state == STATE_U_TURN) {
        float turn_direction = 1.0;
        for(int y=0; y < SCREEN_H; y++) {
            float perspective = (float)y / SCREEN_H;
            float curve = sin_t * (1.0 - perspective) * 150.0 * turn_direction;
            _sprite->drawFastHLine(ROAD_X - (perspective * 80) + curve, y, ROAD_W + (perspective * 160), road_color);
        }
    } else { // STATE_STRAIGHT
        _sprite->fillRect(ROAD_X, 0, ROAD_W, SCREEN_H, road_color);
        
        // --- VẼ ÁNH SÁNG TRÊN MẶT ĐẤT (CHỈ Ở CHẾ ĐỘ ĐƯỜNG THẲNG) ---
        if (is_night) {
            // *** BẮT ĐẦU SỬA LỖI: KHÔI PHỤC ÁNH SÁNG ĐÈN ĐƯỜNG ***
            uint16_t ground_glow_color = _sprite->color565(100, 100, 0);
            uint16_t beam_color = _sprite->color565(80, 80, 0);

            for(int i=0; i<MAX_STREETLIGHTS; i++) {
                if (streetlights[i].active) {
                    int lamp_x = streetlights[i].is_left ? (streetlights[i].x + 20) : (streetlights[i].x - 20);
                    _sprite->fillTriangle(lamp_x, streetlights[i].y - 50, lamp_x - 10, streetlights[i].y, lamp_x + 10, streetlights[i].y, beam_color);
                    _sprite->fillEllipse(lamp_x, streetlights[i].y, 25, 8, ground_glow_color);
                }
            }
            // *** KẾT THÚC SỬA LỖI ***

            // Hiệu ứng đèn xe mềm mại
            int player_center_x = ROAD_X + ROAD_W / 2;
            _sprite->fillEllipse(player_center_x, CAR_Y - 5, 20, 15, _sprite->color565(120, 120, 0)); // Lớp sáng nhất, gần xe nhất
            _sprite->fillEllipse(player_center_x, CAR_Y - 20, 35, 30, ground_glow_color);
            _sprite->fillEllipse(player_center_x, CAR_Y - 45, 50, 45, beam_color); // Lớp rộng nhất, mờ nhất
        }

        // Vẽ vạch kẻ đường
        for(int i = 1; i < 3; i++){
           int x = ROAD_X + i * LANE_W;
           for(int y = lineOffset - 32; y < SCREEN_H; y += 32){ 
             _sprite->drawFastVLine(x, y, 16, COL_LINE); 
           }
        }
    }

    // --- VẼ ĐÈN ĐƯỜNG VÀ XE (LUÔN VẼ TRÊN CÙNG) ---
    for(int i=0; i<MAX_STREETLIGHTS; i++) drawStreetlight(streetlights[i], is_night);
    
    player_rotation = 0.0f;
    player_x_offset = 0.0f;
    if (current_anim_state == STATE_TURNING_LEFT || current_anim_state == STATE_TURNING_RIGHT) {
        float dir = (current_anim_state == STATE_TURNING_LEFT) ? -1.0 : 1.0;
        player_rotation = 45.0f * sin_t * dir;
        player_x_offset = (float)LANE_W * sin_t * dir;
    } else if (current_anim_state == STATE_U_TURN) {
        player_rotation = 90.0f * sin_t;
        player_x_offset = (float)(ROAD_W / 2 + CAR_W/2) * sin_t;
    }
    
    int playerX = ROAD_X + ROAD_W/2 - CAR_W/2 + player_x_offset;
    
    TFT_eSprite carSprite(&tft);
    carSprite.createSprite(CAR_W, CAR_H);
    carSprite.fillSprite(TFT_BLACK);

    drawDetailedCar(&carSprite, 0, 0, COL_CAR, is_night);
    
    carSprite.setPivot(CAR_W / 2, CAR_H / 2);

    int max_dim = sqrt(CAR_W * CAR_W + CAR_H * CAR_H) + 2;
    TFT_eSprite rotatedCanvas(&tft);
    rotatedCanvas.createSprite(max_dim, max_dim);
    rotatedCanvas.fillSprite(TFT_BLACK); 
    rotatedCanvas.setPivot(max_dim / 2, max_dim / 2);

    carSprite.pushRotated(&rotatedCanvas, player_rotation, TFT_BLACK);

    int finalX = playerX - (max_dim - CAR_W) / 2;
    int finalY = CAR_Y - (max_dim - CAR_H) / 2;
    rotatedCanvas.pushToSprite(target_sprite, finalX, finalY, TFT_BLACK);
    
    carSprite.deleteSprite();
    rotatedCanvas.deleteSprite();
}

} // namespace NavigationBackground

