#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include "DFRobotDFPlayerMini.h"

// --- CẤU HÌNH PHẦN CỨNG ---
#define DFPLAYER_RX_PIN 7
#define DFPLAYER_TX_PIN 8

// --- CẤU TRÚC THƯ MỤC VÀ FILE TRÊN THẺ NHỚ ---
#define FOLDER_VIDEO      1
#define FOLDER_NOTI       2
#define FOLDER_GAME       3
#define FOLDER_MUSIC      4

#define SOUND_NOTI_NEW    1 // File /02/001.mp3
#define SOUND_CALL_IN     2 // File /02/002.mp3
#define SOUND_CAR_MOVE    1 // File /03/001.mp3
#define SOUND_GAME_OVER   2 // File /03/002.mp3

// --- KHỞI TẠO CÁC ĐỐI TƯỢNG ---
// Sử dụng UART1 của ESP32
static HardwareSerial dfplayerSerial(1);
static DFRobotDFPlayerMini dfPlayer;
static std::vector<String> trackList;
static int currentTrackIndex = 0;
static bool isPlaying = false;
static bool autoPlayEnabled = false;

// --- KHAI BÁO HÀM ---
void audio_init();
void audio_loop();
void audio_set_volume(int volume);
void audio_play_in_folder(uint8_t folder, uint8_t fileIndex);
void audio_play_video_sound(int videoIndex);
void audio_update_tracklist(const std::vector<String>& new_list);
int audio_get_track_count();
String audio_get_track_name(int index);
String audio_get_current_track_name();
void audio_play_music(int trackIndex);
void audio_pause_resume();
void audio_stop();
void audio_next();
void audio_prev();
void audio_set_autoplay(bool enabled);
bool audio_is_playing();
const std::vector<String>& audio_get_tracklist_ref();

// --- TRIỂN KHAI HÀM ---
void audio_init() {
    dfplayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
    Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));
    if (!dfPlayer.begin(dfplayerSerial)) {
        Serial.println(F("Unable to begin:"));
        Serial.println(F("1.Please recheck the connection!"));
        Serial.println(F("2.Please insert the SD card!"));
    } else {
        Serial.println(F("DFPlayer Mini online."));
    }
}

void audio_loop() {
    if (dfPlayer.available()) {
        uint8_t type = dfPlayer.readType();
        if (type == DFPlayerPlayFinished) {
            if (autoPlayEnabled) {
                audio_next();
            } else {
                isPlaying = false;
            }
        }
    }
}

void audio_set_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 30) volume = 30;
    dfPlayer.volume(volume);
}

void audio_play_in_folder(uint8_t folder, uint8_t fileIndex) {
    dfPlayer.playFolder(folder, fileIndex);
}

void audio_play_video_sound(int videoIndex) {
    audio_play_in_folder(FOLDER_VIDEO, videoIndex + 1);
}

void audio_update_tracklist(const std::vector<String>& new_list) {
    trackList = new_list;
    Serial.printf("Audio manager updated with %d tracks.\n", trackList.size());
}

const std::vector<String>& audio_get_tracklist_ref() {
    return trackList;
}

int audio_get_track_count() {
    return trackList.size();
}

String audio_get_track_name(int index) {
    if (index >= 0 && index < trackList.size()) {
        return trackList[index];
    }
    return "Unknown Track";
}

String audio_get_current_track_name() {
    return audio_get_track_name(currentTrackIndex);
}

void audio_play_music(int trackIndex) {
    if (trackIndex >= 0 && trackIndex < trackList.size()) {
        currentTrackIndex = trackIndex;
        dfPlayer.playFolder(FOLDER_MUSIC, currentTrackIndex + 1);
        isPlaying = true;
    }
}

void audio_pause_resume() {
    if (isPlaying) {
        dfPlayer.pause();
        isPlaying = false;
    } else {
        dfPlayer.start();
        isPlaying = true;
    }
}

void audio_stop() {
    dfPlayer.pause();
}

void audio_next() {
    currentTrackIndex++;
    if (currentTrackIndex >= trackList.size()) {
        currentTrackIndex = 0;
    }
    audio_play_music(currentTrackIndex);
}

void audio_prev() {
    currentTrackIndex--;
    if (currentTrackIndex < 0) {
        currentTrackIndex = trackList.size() - 1;
    }
    audio_play_music(currentTrackIndex);
}

bool audio_is_playing() {
    return isPlaying;
}

void audio_set_autoplay(bool enabled) {
    autoPlayEnabled = enabled;
}

#endif // AUDIO_MANAGER_H
