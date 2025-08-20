// =====================================================================
// mjpg_player.h — SD-card MJPG player (single .mjpg file, Mode 2)
// Target: ESP32 / ESP32-C3 with TFT_eSPI + TJpg_Decoder
// Author: Dasai Mochi helper (0818)
//
// File format (.mjpg): sequence of frames
//   [u32_le length][JPEG bytes]  repeated ...
// Optional index (.idx): array of u32_le offsets to the START of each frame (the length field)
//
// Integration:
//   #include <TJpg_Decoder.h>
//   #include "mjpg_player.h"
//   MjpgPlayer player;
//   player.beginSD("/videos/demo.mjpg", "/videos/demo.idx"); // idx optional
//   while(player.drawNextFrame(0,0)) { pace_to_fps(); }
//   // call player.rewind() to loop
//
// Notes:
//  - This implementation pre-allocates a single frame buffer (default 64 KB).
//    Ensure your encoded frames are <= MJPG_MAX_FRAME.
//  - Use the provided pack_mjpg.py to build .mjpg + .idx from a folder of JPGs.
// =====================================================================

#pragma once
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <TJpg_Decoder.h>

#ifndef MJPG_MAX_FRAME
  #define MJPG_MAX_FRAME (64*1024)   // 64 KB max JPEG size per frame (tune for your device)
#endif

class MjpgPlayer {
public:
  MjpgPlayer(): _buf(nullptr), _bufSize(0), _loop(true), _frameCount(0), _useIndex(false), _fpsLimitMs(0) {}
  ~MjpgPlayer(){ end(); }

  bool beginSD(const char* mjpgPath, const char* idxPath=nullptr, size_t bufSize=MJPG_MAX_FRAME, bool loop=true){
    end();
    _loop = loop;
    _buf = (uint8_t*) malloc(bufSize);
    if(!_buf){ Serial.println("[MJPG] malloc failed"); return false; }
    _bufSize = bufSize;

    _file = SD.open(mjpgPath, FILE_READ);
    if(!_file){ Serial.printf("[MJPG] open fail: %s\n", mjpgPath); return false; }

    _useIndex = false; _frameCount = 0; _idxOffsets.clear();
    if(idxPath){
      File idx = SD.open(idxPath, FILE_READ);
      if(idx){
        // Read all u32 offsets
        while(idx.available()>=4){
          uint32_t off = readU32Le(idx);
          _idxOffsets.push_back(off);
          _frameCount++;
        }
        idx.close();
        _useIndex = _frameCount>0;
        Serial.printf("[MJPG] index loaded: %u frames\n", _frameCount);
      }
    }

    _curFrame = 0; _nextOffset = 0; _file.seek(0);
    return true;
  }

  void end(){
    if(_file){ _file.close(); }
    if(_buf){ free(_buf); _buf=nullptr; }
    _bufSize = 0; _idxOffsets.clear(); _frameCount=0; _useIndex=false; _nextOffset=0; _curFrame=0;
  }

  void setLoop(bool loop){ _loop = loop; }
  void setFpsLimit(uint16_t fps){ _fpsLimitMs = (fps>0)? (1000UL/fps) : 0; }

  // Draw next frame at (x,y). Returns false on EOF (and no loop), true if drawn or looped.
  bool drawNextFrame(int16_t x, int16_t y){
    if(!_file) return false;

    uint32_t startMs = millis();

    // Seek to frame start
    if(_useIndex){
      if(_curFrame >= _frameCount){
        if(!_loop) return false; _curFrame = 0;
      }
      uint32_t off = _idxOffsets[_curFrame];
      if(!_file.seek(off)){ Serial.println("[MJPG] seek fail (idx)"); return false; }
    } else {
      if(_nextOffset>=0){ if(!_file.seek(_nextOffset)){ Serial.println("[MJPG] seek fail (seq)"); return false; } }
    }

    if(_file.available()<4){
      if(_loop){ _curFrame=0; _nextOffset=0; _file.seek(0); return true; }
      return false;
    }

    uint32_t len = readU32Le(_file);
    if(len==0 || len>_bufSize){
      Serial.printf("[MJPG] bad frame len=%u (buf=%u)\n", len, (unsigned)_bufSize);
      // Try to skip this frame safely
      if(!_file.seek(_file.position() + len)) return false;
      advanceOffsets(len);
      return true;
    }

    if(!readFull(_file, _buf, len)){
      Serial.println("[MJPG] read frame failed");
      if(_loop){ _curFrame=0; _nextOffset=0; _file.seek(0); return true; }
      return false;
    }

    // Decode + draw
    TJpgDec.drawJpg(x, y, _buf, len);

    // Prepare for next
    advanceOffsets(len);

    // Pace to fps if needed
    if(_fpsLimitMs>0){
      uint32_t used = millis() - startMs;
      if(used < _fpsLimitMs){ delay(_fpsLimitMs - used); }
    }
    return true;
  }

  // Seek to exact frame (requires index)
  bool seekFrame(uint32_t frameIndex){
    if(!_useIndex) return false;
    if(frameIndex>=_frameCount) return false;
    _curFrame = frameIndex; return true;
  }

  void rewind(){ _curFrame=0; _nextOffset=0; _file.seek(0); }
  uint32_t frameCount() const { return _frameCount; }
  uint32_t currentFrame() const { return _curFrame; }

private:
  File _file;
  uint8_t* _buf; size_t _bufSize; bool _loop; uint32_t _curFrame; int64_t _nextOffset;
  std::vector<uint32_t> _idxOffsets; bool _useIndex; uint32_t _frameCount; uint16_t _fpsLimitMs;

  static uint32_t readU32Le(File &f){
    uint8_t b[4]; size_t n=f.read(b,4); if(n<4) return 0; return (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
  }

  static bool readFull(File &f, uint8_t* dst, uint32_t len){
    uint32_t got=0; while(got<len){ int n=f.read(dst+got, len-got); if(n<=0) return false; got+=n; yield(); }
    return true;
  }

  inline void advanceOffsets(uint32_t justReadLen){
    if(_useIndex){ _curFrame++; }
    else { _nextOffset = (int64_t)_file.position(); }
  }
};
