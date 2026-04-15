/**
 * @file dfplayer.h
 * @brief DFPlayer Pro Audio Module Interface
 * 
 * Audio playback control with speaker/line-out mode switching.
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#pragma once

#include <stdint.h>

// Audio output mode enumeration
typedef enum {
    AUDIO_OUTPUT_SPEAKER = 0,
    AUDIO_OUTPUT_LINE_OUT = 1
} audio_output_mode_t;

// DFPlayer initialization and control functions
void dfplayer_init(void);
void dfplayer_play_fire(void);
void dfplayer_set_volume(uint8_t volume);
void dfplayer_play_reload(void);
void dfplayer_stop(void);

// Audio mode control
audio_output_mode_t dfplayer_get_audio_mode(void);
void dfplayer_set_audio_mode(audio_output_mode_t mode);
