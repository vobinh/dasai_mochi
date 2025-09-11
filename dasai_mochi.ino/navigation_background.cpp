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

static const uint16_t COL_GRASS = 0x0400; 
static const uint16_t COL_ROAD  = 0x4228; 
static const uint16_t COL_LINE  = TFT_DARKGREY;
static const uint16_t COL_CAR   = TFT_CYAN;
static const uint16_t COL_TREE_TRUNK = 0x4A40;
static const uint16_t COL_TREE_LEAVES = 0x0520;

// ---- Trạng thái ----
struct Tree { int x; int y; bool active; };
static const int MAX_TREES = 8;

static TFT_eSprite* _sprite = nullptr;
static Tree trees[MAX_TREES];
static int speedPx = 4; // Tốc độ chạy nền
static int lineOffset = 0; 
static uint8_t playerLane = 1; // Xe luôn ở giữa

// ---- Các hàm nội bộ ----
static inline int laneCenterX(int lane){ return LANE_X[lane]; }

static void drawDetailedCar(int x, int y, uint16_t bodyColor) {
    _sprite->fillRoundRect(x, y, CAR_W, CAR_H, 6, bodyColor);
    _sprite->fillRoundRect(x + 2, y + 5, CAR_W - 4, 10, 4, TFT_DARKGREY);
    _sprite->fillRoundRect(x + 4, y + 8, CAR_W - 8, 18, 4, TFT_SKYBLUE);
    _sprite->fillRect(x, y + 4, 4, 6, TFT_YELLOW);
    _sprite->fillRect(x + CAR_W - 4, y + 4, 4, 6, TFT_YELLOW);
    _sprite->fillRect(x, y + CAR_H - 8, 4, 6, TFT_RED);
    _sprite->fillRect(x + CAR_W - 4, y + CAR_H - 8, 4, 6, TFT_RED);
}

static void drawTree(const Tree& t) {
    if (!t.active) return;
    _sprite->fillRect(t.x - 2, t.y, 4, 20, COL_TREE_TRUNK);
    _sprite->fillCircle(t.x, t.y, 12, COL_TREE_LEAVES);
    _sprite->fillCircle(t.x-5, t.y-5, 8, COL_TREE_LEAVES);
    _sprite->fillCircle(t.x+5, t.y-5, 8, COL_TREE_LEAVES);
}

static void drawRoad(){
  _sprite->fillRect(0, 0, ROAD_X, SCREEN_H, COL_GRASS);
  _sprite->fillRect(ROAD_X + ROAD_W, 0, SCREEN_W - (ROAD_X + ROAD_W), SCREEN_H, COL_GRASS);
  _sprite->fillRect(ROAD_X, 0, ROAD_W, SCREEN_H, COL_ROAD);
  for(int i = 1; i < LANES; i++){
    int x = ROAD_X + i * LANE_W;
    for(int y = lineOffset - 32; y < SCREEN_H; y += 32){ 
      _sprite->drawFastVLine(x, y, 16, COL_LINE); 
    }
  }
}

static void spawnTree(){
  for(int i=0;i<MAX_TREES;i++) if(!trees[i].active){
    trees[i].active = true;
    trees[i].y = -20;
    if(random(0,2) == 0) {
        trees[i].x = random(10, ROAD_X - 15);
    } else {
        trees[i].x = random(ROAD_X + ROAD_W + 15, SCREEN_W - 10);
    }
    break;
  }
}

// ---- Public API ----
void begin(TFT_eSprite* sprite){ 
    _sprite = sprite; 
    randomSeed(millis()); 
    for(int i=0;i<MAX_TREES;i++) trees[i] = {0,0,false};
}

void tick(){
  lineOffset = (lineOffset + speedPx) % 32;
  for(int i=0;i<MAX_TREES;i++) if(trees[i].active){
    trees[i].y += speedPx;
    if(trees[i].y > SCREEN_H + 20) { trees[i].active=false; }
  }
  
  if (random(0, 15) == 0) {
      spawnTree();
  }
}

void draw(TFT_eSprite* target_sprite) {
    _sprite = target_sprite; // Đảm bảo vẽ lên đúng sprite
    drawRoad();
    for(int i=0;i<MAX_TREES;i++) drawTree(trees[i]);
    int playerX = laneCenterX(playerLane) - CAR_W/2;
    drawDetailedCar(playerX, CAR_Y, COL_CAR);
}

} // namespace NavigationBackground
