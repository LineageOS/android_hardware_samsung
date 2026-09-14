/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SoundBooster"

#include <errno.h>
#include <unistd.h>

#include <log/log.h>

#include "SoundBooster.h"

namespace android {
namespace {

const char gParamFile[] = "/vendor/etc/SoundBoosterParam.txt";

}  // namespace

SoundBooster::SoundBooster(uint32_t mode)
    : mInterface(nullptr),
      mMode(mode),
      mSessionId(0),
      mSampleRate(0),
      mFormat(AUDIO_FORMAT_INVALID),
      mBitDepth(BIT_DEPTH_NONE) {
    if (mMode > SB_MODE_MAX) {
        ALOGE("Invalid SoundBooster mode %u", mMode);
        mMode = SB_MODE_DEFAULT;
    }
    mInterface = SoundBoosterFactory::Create(static_cast<int>(mMode));
}

SoundBooster::~SoundBooster() {
    if (mInterface != nullptr) {
        SoundBoosterFactory::Destroy(mInterface);
    }
}

int SoundBooster::init(uint32_t sampleRate, audio_format_t format) {
    if (mInterface == nullptr) {
        mInterface = SoundBoosterFactory::Create(static_cast<int>(mMode));
        if (mInterface == nullptr) {
            return -ENODEV;
        }
    }

    BitDepth bitDepth = BIT_DEPTH_NONE;
    if (format == AUDIO_FORMAT_PCM_FLOAT) {
        bitDepth = BIT_DEPTH_FLOAT;
    } else if (format == AUDIO_FORMAT_PCM_8_24_BIT) {
        bitDepth = BIT_DEPTH_8_24;
    }

    mSampleRate = sampleRate;
    mFormat = format;
    mBitDepth = bitDepth;

    mInterface->Init(mBitDepth);
    mInterface->SamplingRateConfig(static_cast<int>(sampleRate));
    readParam();
    return 0;
}

int SoundBooster::process(void* in, void* out, size_t frames, float volume) {
    if (frames == 0 || mInterface == nullptr) {
        return 0;
    }
    return compatExe(mInterface, in, out, static_cast<int>(frames), volume);
}

void SoundBooster::clear() {
    if (mInterface != nullptr) {
        compatBuffClear(mInterface);
    }
}

void SoundBooster::setRotation(int rotation) {
    if (mInterface == nullptr) {
        return;
    }
    if (rotation == ORIENTATION_270) {
        rotation = ORIENTATION_90;
    } else if (rotation == ORIENTATION_90) {
        rotation = ORIENTATION_270;
    }
    mInterface->SetOrientation(static_cast<Orientation>(rotation));
}

void SoundBooster::setFlatMotion(int flat) {
    if (mInterface == nullptr) {
        return;
    }
    mInterface->SetMotion(flat);
    ALOGV("setFlatMotion: flat %d", flat);
}

void SoundBooster::readParam() {
    if (mInterface == nullptr) {
        return;
    }
    if (access(gParamFile, F_OK) != 0) {
        ALOGE("Cannot find %s", gParamFile);
        return;
    }
    mInterface->LoadParameter(gParamFile, false, nullptr);
}

}  // namespace android
