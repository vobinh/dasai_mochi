#pragma once
#include <TFT_eSPI.h>

// =======================================================================================
//  Simple Car Dodging Game - Giao diện được nâng cấp bởi Gemini
//  - Đồ họa chi tiết hơn cho xe và môi trường.
//  - Tối ưu hóa bằng Sprite để chạy mượt mà, không giật lag.
//  - Logic game được cải tiến để luôn đảm bảo có lối thoát.
// =======================================================================================

namespace CarGame {

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

static const int OBS_W = 28;
static const int OBS_H = 45;
static const int MIN_SPAWN_GAP = CAR_H * 2; // Khoảng trống tối thiểu giữa các xe

static const uint16_t COL_GRASS = 0x0400; // Xanh lá đậm
static const uint16_t COL_ROAD  = 0x4228; // Xám đậm
static const uint16_t COL_LINE  = TFT_DARKGREY;
static const uint16_t COL_CAR   = TFT_CYAN;
static const uint16_t COL_HUD   = TFT_WHITE;
static const uint16_t COL_PAUSE = TFT_YELLOW;
static const uint16_t COL_TREE_TRUNK = 0x4A40;
static const uint16_t COL_TREE_LEAVES = 0x0520;

// Bảng màu cho xe chướng ngại vật
static const uint16_t OBS_COLORS[] = { TFT_RED, TFT_MAGENTA, TFT_ORANGE, TFT_BLUE, TFT_GREENYELLOW };

// ---- Trạng thái Game ----
struct Obstacle { int lane; int y; bool active; uint16_t color; };
struct Tree { int x; int y; bool active; };
static const int MAX_OBS = 6;
static const int MAX_TREES = 8;

static TFT_eSprite* _sprite = nullptr;
static bool running = false;
static bool paused  = false;
static uint8_t playerLane = 1;
static Obstacle obs[MAX_OBS];
static Tree trees[MAX_TREES];
static unsigned long lastTick = 0;
static unsigned long lastSpawn = 0;
static int speedPx = 3;
static unsigned long spawnInt = 900;
static uint32_t score = 0;
static int lineOffset = 0; // Biến để tạo hiệu ứng đường chạy

// ---- Các hàm vẽ chi tiết ----
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

static void drawPlayerCar(){
  int x = laneCenterX(playerLane) - CAR_W/2;
  drawDetailedCar(x, CAR_Y, COL_CAR);
}

static void drawObs(const Obstacle& o){
  if(!o.active) return;
  int x = laneCenterX(o.lane) - OBS_W/2;
  drawDetailedCar(x, o.y, o.color);
}

static void hud(){
  _sprite->setTextColor(COL_HUD, COL_ROAD);
  _sprite->setTextSize(2);
  _sprite->setCursor(10, 10);
  _sprite->print(score);
  if(paused){ 
      _sprite->setTextDatum(MC_DATUM);
      _sprite->setTextColor(COL_PAUSE, TFT_BLACK); 
      _sprite->drawString("PAUSE", SCREEN_W / 2, SCREEN_H / 2);
      _sprite->setTextDatum(TL_DATUM);
  }
}

static void reset(){
  for(int i=0;i<MAX_OBS;i++) obs[i] = {0,0,false,0};
  for(int i=0;i<MAX_TREES;i++) trees[i] = {0,0,false};
  playerLane = 1; speedPx = 3; spawnInt = 900; score = 0; paused=false;
  lastTick = lastSpawn = millis();
}

static bool collide(){
  for(int i=0;i<MAX_OBS;i++) if(obs[i].active){
    if(obs[i].lane == playerLane){
      int y1 = obs[i].y; int y2 = y1 + OBS_H;
      int py1 = CAR_Y; int py2 = CAR_Y + CAR_H;
      if(!(y2 < py1 || y1 > py2)) return true;
    }
  }
  return false;
}

// **LOGIC TẠO CHƯỚNG NGẠI VẬT ĐÃ ĐƯỢC LÀM LẠI HOÀN TOÀN**
static void spawn(){
  // Kiểm tra "khu vực an toàn"
  for(int i=0; i<MAX_OBS; i++) {
    if(obs[i].active && obs[i].y < MIN_SPAWN_GAP) return;
  }

  // Tìm một hoặc hai slot trống
  int free_slots[MAX_OBS];
  int free_count = 0;
  for(int i=0; i<MAX_OBS && free_count < 2; i++) {
    if(!obs[i].active) {
      free_slots[free_count++] = i;
    }
  }
  if(free_count == 0) return; // Không có slot trống

  // Quyết định tạo 1 hay 2 xe
  int two_car_probability = 20 + (score / 100); // Tăng độ khó theo điểm
  if (two_car_probability > 80) two_car_probability = 80;
  
  if (free_count < 2 || random(0, 100) > two_car_probability) {
    // Tạo 1 xe
    int slot = free_slots[0];
    obs[slot].active = true;
    obs[slot].lane = random(0, LANES);
    obs[slot].y = -OBS_H;
    obs[slot].color = OBS_COLORS[random(0, sizeof(OBS_COLORS)/sizeof(OBS_COLORS[0]))];
  } else {
    // Tạo 2 xe, chừa 1 làn trống
    int safeLane = random(0, LANES);
    int current_slot = 0;
    for (int lane = 0; lane < LANES; lane++) {
      if (lane != safeLane) {
        int slot = free_slots[current_slot++];
        obs[slot].active = true;
        obs[slot].lane = lane;
        obs[slot].y = -OBS_H;
        obs[slot].color = OBS_COLORS[random(0, sizeof(OBS_COLORS)/sizeof(OBS_COLORS[0]))];
      }
    }
  }

  // Tạo cây (logic không đổi)
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

static void step(){
  lineOffset = (lineOffset + speedPx) % 32;
  for(int i=0;i<MAX_OBS;i++) if(obs[i].active){
    obs[i].y += speedPx;
    if(obs[i].y > SCREEN_H) { obs[i].active=false; score += 10; }
  }
  for(int i=0;i<MAX_TREES;i++) if(trees[i].active){
    trees[i].y += speedPx;
    if(trees[i].y > SCREEN_H + 20) { trees[i].active=false; }
  }

  static uint32_t frameCount=0; frameCount++;
  if(frameCount % 120 == 0 && speedPx < 8) speedPx++;
  if(frameCount % 180 == 0 && spawnInt > 400) spawnInt -= 50;
}

// ---- Public API ----
static void begin(TFT_eSprite* sprite){ _sprite = sprite; randomSeed(millis()); }

static void start(){ running=true; reset(); }

static void stop(){ running=false; }

static bool isRunning(){ return running; }

static void togglePause(){ if(!running) return; paused=!paused; }

static void moveRight(){ 
  if(!running || paused) return; 
  playerLane++;
  if (playerLane >= LANES) playerLane = 0;
}

static void moveLeft(){ 
  if(!running || paused) return; 
  playerLane--;
  if (playerLane < 0) playerLane = LANES - 1;
}

static void tick(){
  if(!running) return;
  const unsigned long now = millis();
  
  if(paused){ 
    drawRoad();
    for(int i=0;i<MAX_TREES;i++) drawTree(trees[i]);
    for(int i=0;i<MAX_OBS;i++) drawObs(obs[i]);
    drawPlayerCar();
    hud(); 
    return; 
  }

  if(now - lastSpawn >= spawnInt){ lastSpawn = now; spawn(); }

  if(now - lastTick >= 16){ // ~60 FPS
    lastTick = now;
    
    step();

    drawRoad();
    for(int i=0;i<MAX_TREES;i++) drawTree(trees[i]);
    for(int i=0;i<MAX_OBS;i++) drawObs(obs[i]);
    drawPlayerCar();
    
    if(collide()){
      for(int i=0; i<4; i++) {
        _sprite->pushSprite(-5,0); delay(20);
        _sprite->pushSprite(5,0); delay(20);
      }
      _sprite->pushSprite(0,0);

      _sprite->setTextDatum(MC_DATUM);
      _sprite->setTextColor(TFT_RED, TFT_BLACK);
      _sprite->drawString("CRASH!", SCREEN_W/2, SCREEN_H/2 - 10);
      _sprite->setTextDatum(TL_DATUM);
      
      stop();
      return;
    }
    hud();
  }
}

} // namespace CarGame
