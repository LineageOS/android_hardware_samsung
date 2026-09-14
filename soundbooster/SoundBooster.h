/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <system/audio.h>

#include "SoundBoosterInterface.h"

#define SB_MODE_DEFAULT 2
#define SB_MODE_MAX 2

namespace android {

class SoundBooster {
  public:
    explicit SoundBooster(uint32_t mode);
    ~SoundBooster();

    SoundBooster(const SoundBooster&) = delete;
    SoundBooster& operator=(const SoundBooster&) = delete;

    int init(uint32_t sampleRate, audio_format_t format);
    int process(void* in, void* out, size_t frames, float volume);
    void clear();
    void setRotation(int rotation);
    void setFlatMotion(int flat);
    void readParam();
    void setSessionId(int32_t sessionId) { mSessionId = sessionId; }

    static bool isEnabledDevice(uint32_t device) { return device == AUDIO_DEVICE_OUT_SPEAKER; }

  private:
    ISoundBooster* mInterface;
    uint32_t mMode;
    int32_t mSessionId;
    uint32_t mSampleRate;
    audio_format_t mFormat;
    BitDepth mBitDepth;
};

}  // namespace android
