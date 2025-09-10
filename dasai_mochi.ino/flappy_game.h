#pragma once
#include <TFT_eSPI.h>

/* ====== Flappy (header-only) ===============================================
   Cách dùng:
     - Flappy::begin(&tft);    // trong setup, sau khi tft.init()
     - Flappy::start();        // khi vào game
     - Flappy::tick();         // gọi liên tục trong loop (update/render)
     - Ánh xạ nút:
         single-click  -> Flappy::flap();
         double-click  -> Flappy::togglePause();
         long-press    -> Flappy::stop(); (thoát game)
   ========================================================================== */

namespace Flappy {
  struct State {
    bool    running=false, paused=false;
    int16_t birdX=40, birdY=120, birdR=6;
    float   velY=0.0f, gravity=0.28f, flapImpulse=-4.2f;
    int16_t pipeX1=0, pipeX2=0, pipeW=22, gapY1=80, gapY2=110, gapH=62, speedX=2;
    uint16_t score=0;
    unsigned long lastStepMs=0;
  };

  static TFT_eSPI* _tft=nullptr;
  static State s;

  static inline int16_t rnd(int16_t a, int16_t b) { return a + (rand() % (b - a + 1)); }

  void begin(TFT_eSPI* tft){
    _tft = tft;
    srand((unsigned)millis());
  }

  void _drawPipe(int16_t x, int16_t gapY){
    _tft->fillRect(x, 0, s.pipeW, gapY, TFT_DARKGREEN);
    _tft->fillRect(x, gapY + s.gapH, s.pipeW, _tft->height() - (gapY + s.gapH), TFT_DARKGREEN);
    _tft->drawRect(x, 0, s.pipeW, gapY, TFT_GREEN);
    _tft->drawRect(x, gapY + s.gapH, s.pipeW, _tft->height() - (gapY + s.gapH), TFT_GREEN);
  }

  void _render(bool clearOnly=false){
    _tft->fillScreen(TFT_BLACK);
    if (clearOnly) return;
    _drawPipe(s.pipeX1, s.gapY1);
    _drawPipe(s.pipeX2, s.gapY2);
    _tft->fillCircle(s.birdX, s.birdY, s.birdR, TFT_YELLOW);
    _tft->drawCircle(s.birdX, s.birdY, s.birdR, TFT_ORANGE);
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(_tft->width()/2, 4);
    _tft->setTextSize(2);
    _tft->printf("%u", s.score);
    if (s.paused){
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_CYAN, TFT_BLACK);
      _tft->drawString("PAUSE", _tft->width()/2, _tft->height()/2);
      _tft->setTextDatum(TL_DATUM);
    }
  }

  bool _collide(){
    if (s.birdY - s.birdR < 0) return true;
    if (s.birdY + s.birdR >= _tft->height()) return true;
    auto hit = [&](int16_t px, int16_t gapY){
      if (s.birdX + s.birdR >= px && s.birdX - s.birdR <= px + s.pipeW){
        if (s.birdY - s.birdR < gapY) return true;
        if (s.birdY + s.birdR > gapY + s.gapH) return true;
      }
      return false;
    };
    return hit(s.pipeX1, s.gapY1) || hit(s.pipeX2, s.gapY2);
  }

  void _nextPipe(int which){
    int16_t newX = _tft->width() + 10;
    int16_t newGap = rnd(30, _tft->height()-30-s.gapH);
    if (which==1){ s.pipeX1=newX; s.gapY1=newGap; }
    else          { s.pipeX2=newX; s.gapY2=newGap; }
    s.score++;
  }

  void start(){
    s.running=true; s.paused=false;
    s.birdR=6; s.birdX=40; s.birdY=_tft->height()/2; s.velY=0.0f;
    s.gravity=0.28f; s.flapImpulse=-4.2f; s.pipeW=22; s.gapH=62; s.speedX=2;
    s.pipeX1=_tft->width()+10; s.pipeX2=s.pipeX1 + (_tft->width()/2);
    s.gapY1=rnd(30,_tft->height()-30-s.gapH); s.gapY2=rnd(30,_tft->height()-30-s.gapH);
    s.score=0; s.lastStepMs=millis();
    _render();
  }

  void stop(){
    s.running=false; s.paused=false;
    _render(true);
  }

  void flap(){ if (s.running && !s.paused) s.velY = s.flapImpulse; }
  void togglePause(){ if (s.running) { s.paused = !s.paused; _render(); } }

  bool isRunning(){ return s.running; }
  bool isPaused(){ return s.paused; }

  void tick(){
    if (!_tft || !s.running || s.paused) return;
    unsigned long now = millis();
    if (now - s.lastStepMs < 16) return; // ~60 FPS
    s.lastStepMs = now;

    s.velY += s.gravity;
    s.birdY = (int16_t)(s.birdY + s.velY);

    s.pipeX1 -= s.speedX;
    s.pipeX2 -= s.speedX;
    if (s.pipeX1 + s.pipeW < 0) _nextPipe(1);
    if (s.pipeX2 + s.pipeW < 0) _nextPipe(2);

    _render();

    if (_collide()){
      s.running=false;
      _tft->setTextDatum(MC_DATUM);
      _tft->setTextColor(TFT_RED, TFT_BLACK);
      _tft->drawString("GAME OVER", _tft->width()/2, _tft->height()/2 - 10);
      _tft->setTextColor(TFT_WHITE, TFT_BLACK);
      char buf[32]; snprintf(buf,sizeof(buf),"Score: %u",s.score);
      _tft->drawString(buf, _tft->width()/2, _tft->height()/2 + 10);
      _tft->setTextDatum(TL_DATUM);
    }
  }
} // namespace Flappy
