#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <vector>

// --- KHAI BÁO HÀM ---
void audio_init();
void audio_loop();
void audio_set_volume(int volume);
void audio_play_in_folder(uint8_t folder, uint8_t fileIndex);
void audio_play_video_sound(int videoIndex);
void audio_update_tracklist(const std::vector<String>& new_list);
const std::vector<String>& audio_get_tracklist_ref();
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

#endif // AUDIO_MANAGER_H

