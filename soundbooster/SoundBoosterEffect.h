/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ANDROID_SOUNDBOOSTEREFFECT_H_
#define ANDROID_SOUNDBOOSTEREFFECT_H_

#include <stdint.h>

#include <hardware/audio_effect.h>

#include "SoundBooster.h"

#define SOUNDBOOSTER_CUP_LOAD 75
#define SOUNDBOOSTER_MEM_USAGE 25

#define SB_BYTES_PER_FRAME (2 * sizeof(int32_t))  // Stereo, 4-byte samples
#define SB_MIN_SET_PARAM_CMD_SIZE 0x13
#define SB_MIN_VOLUME_DB (-100.0f)
#define SB_VOLUME_SCALE (1.0f / (1 << 24))
#define SB_VOLUME_EPSILON 1e-5f
#define SB_LINEAR_TO_DB_SCALE 8.68589f  // 20 / ln(10)

typedef enum {
    EFFECT_STATE_UNINITIALIZED,
    EFFECT_STATE_INITIALIZED,
    EFFECT_STATE_ACTIVE,
} effect_state_en;

/* enumerated parameter settings for the SoundBooster effect */
typedef enum {
    SB_PARAM_READ_PARAM,
    SB_PARAM_ROTATION,
    SB_PARAM_FLAT_MOTION,
} t_soundbooster_params;

struct EffectContext {
    const struct effect_interface_s* itfe = nullptr;
    int32_t sessionId = 0;
    effect_state_en state = EFFECT_STATE_UNINITIALIZED;
    int32_t mode = SB_MODE_DEFAULT;
    float logVolume = SB_MIN_VOLUME_DB;
    float leftVolume = SB_MIN_VOLUME_DB;
    float rightVolume = 0.0f;
    uint32_t curDevice = 0;
    bool enabled = true;
    int32_t rotation = 0;
    int32_t flatMotion = 0;
    int32_t spkMode = 0;
    android::SoundBooster* dsp = nullptr;
};

#endif /*ANDROID_SOUNDBOOSTEREFFECT_H_*/
