// =============================================
// font_vn.h — Header-only Vietnamese bitmap font engine for TFT_eSPI
// Version: 0.1 (0817)
// Author: Dasai Mochi helper
// ---------------------------------------------
// Usage:
//   #include "font_vn.h"
//   VNFont::begin(vn_font_24_bitmap, vn_font_24_glyphs, VN_FONT_24_COUNT, 24);
//   VNFont::setColor(TFT_WHITE, TFT_BLACK);
//   VNFont::drawString(sprite, "Tiếng Việt có dấu", 10, 10);
//   int w = VNFont::textWidth("Xin chào");
//
// Notes:
// - Requires a glyph atlas header like: vn_font_24.h (bitmap + glyph table)
// - Generate a full atlas for production use.
// =============================================

#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace VNFont {

// --- Glyph format (similar to Adafruit GFX font glyph) ---
struct Glyph {
  uint16_t codepoint;   // Unicode code point
  uint16_t w, h;        // width, height (px)
  int8_t   xAdvance;    // how much to advance cursor.x after this glyph
  int8_t   xOffset;     // draw offset from cursor.x
  int8_t   yOffset;     // draw offset from baseline (negative = up)
  uint32_t bitmapOffset;// offset in bitmap array (byte index)
};

// --- Global state ---
static const uint8_t*   s_bitmap = nullptr;   // 1-bit packed, MSB first
static const Glyph*     s_glyphs = nullptr;
static size_t           s_count  = 0;
static uint8_t          s_lineH  = 24;        // line height
static uint16_t         s_fg     = 0xFFFF;    // draw color
static uint16_t         s_bg     = 0x0000;    // (optional, unused by default)

// --- API ---
inline void begin(const uint8_t* bitmap, const Glyph* glyphs, size_t count, uint8_t lineHeight){
  s_bitmap = bitmap; s_glyphs = glyphs; s_count = count; s_lineH = lineHeight; }
inline void setColor(uint16_t fg, uint16_t bg=0x0000){ s_fg = fg; s_bg = bg; }
inline uint8_t lineHeight(){ return s_lineH; }

// Simple binary search on glyph table (must be sorted by codepoint)
static inline const Glyph* findGlyph(uint32_t cp){
  size_t lo=0, hi=s_count; 
  while(lo<hi){
    size_t mid=(lo+hi)>>1; uint32_t k=s_glyphs[mid].codepoint;
    if(cp==k) return &s_glyphs[mid];
    (cp<k)? hi=mid : lo=mid+1;
  }
  return nullptr;
}

// UTF-8 decoder → codepoint iterator
class Utf8 {
public:
  Utf8(const char* s):_s((const uint8_t*)s){}
  bool next(uint32_t &cp){
    uint8_t c=* _s++; if(!c){ return false; }
    if((c & 0x80)==0){ cp=c; return true; }
    if((c & 0xE0)==0xC0){ uint8_t c2=* _s++; cp=((c&0x1F)<<6)|(c2&0x3F); return true; }
    if((c & 0xF0)==0xE0){ uint8_t c2=* _s++; uint8_t c3=* _s++; cp=((c&0x0F)<<12)|((c2&0x3F)<<6)|(c3&0x3F); return true; }
    if((c & 0xF8)==0xF0){ uint8_t c2=* _s++; uint8_t c3=* _s++; uint8_t c4=* _s++; cp=((c&0x07)<<18)|((c2&0x3F)<<12)|((c3&0x3F)<<6)|(c4&0x3F); return true; }
    cp='?'; return true;
  }
private:
  const uint8_t* _s;
};

// Measure width in pixels
inline int16_t textWidth(const String& s){
  Utf8 it(s.c_str()); uint32_t cp; int16_t x=0; 
  while(it.next(cp)){
    if(cp=='\n'){ break; } // single-line width
    const Glyph* g=findGlyph(cp);
    x += g? g->xAdvance : (int16_t) (s_lineH*0.6f);
  }
  return x;
}

// Draw a single glyph at (cursorX, baselineY)
static inline void drawGlyph(TFT_eSprite& spr, const Glyph& g, int16_t cursorX, int16_t baselineY){
  int16_t x0 = cursorX + g.xOffset; 
  int16_t y0 = baselineY + g.yOffset; // yOffset is typically negative
  if(g.w==0 || g.h==0) return;

  const uint8_t* p = s_bitmap + g.bitmapOffset; 
  uint16_t bitIdx=0; 
  for(uint16_t yy=0; yy<g.h; ++yy){
    for(uint16_t xx=0; xx<g.w; ++xx){
      uint8_t byte = p[(bitIdx>>3)];
      uint8_t mask = 0x80 >> (bitIdx & 7);
      if(byte & mask){ spr.drawPixel(x0+xx, y0+yy, s_fg); }
      bitIdx++;
    }
  }
}

// Draw string at top-left (x,y) baseline handled internally
inline void drawString(TFT_eSprite& spr, const String& s, int16_t x, int16_t y){
  Utf8 it(s.c_str()); uint32_t cp; int16_t cx=x; int16_t baseline = y + s_lineH; 
  while(it.next(cp)){
    if(cp=='\n'){ baseline += s_lineH; cx = x; continue; }
    const Glyph* g=findGlyph(cp);
    if(!g){ // fallback box for missing glyph
      spr.drawRect(cx, baseline - s_lineH + 2, (int)(s_lineH*0.6f), s_lineH-4, s_fg);
      cx += (int)(s_lineH*0.6f); 
      continue;
    }
    drawGlyph(spr, *g, cx, baseline);
    cx += g->xAdvance;
  }
}

// Datum variants
inline void drawStringDatum(TFT_eSprite& spr, const String& s, int16_t x, int16_t y, uint8_t datum){
  int16_t w = textWidth(s);
  int16_t h = s_lineH;
  int16_t xx=x, yy=y;
  switch(datum){
    case TL_DATUM: default: break;
    case TC_DATUM: xx = x - w/2; break;
    case TR_DATUM: xx = x - w; break;
    case ML_DATUM: yy = y - h/2; break;
    case MC_DATUM: xx = x - w/2; yy = y - h/2; break;
    case MR_DATUM: xx = x - w; yy = y - h/2; break;
    case BL_DATUM: yy = y - h; break;
    case BC_DATUM: xx = x - w/2; yy = y - h; break;
    case BR_DATUM: xx = x - w; yy = y - h; break;
  }
  drawString(spr, s, xx, yy);
}

} // namespace VNFont
