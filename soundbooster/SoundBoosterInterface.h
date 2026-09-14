/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <dlfcn.h>
#include <log/log.h>
#include <cerrno>
#include <cstdint>

enum BitDepth {
    BIT_DEPTH_NONE = 0,
    BIT_DEPTH_8_24 = 1,
    BIT_DEPTH_FLOAT = 3,
};

enum Orientation {
    ORIENTATION_0 = 0,
    ORIENTATION_90 = 1,
    ORIENTATION_180 = 2,
    ORIENTATION_270 = 3,
};

class ISoundBooster {
  public:
    ISoundBooster(const ISoundBooster&) = delete;
    ISoundBooster& operator=(const ISoundBooster&) = delete;

    /* slot 0 (0x00) */
    virtual int Init(BitDepth bitDepth) = 0;
    /* slot 1 (0x08) */
    virtual int SamplingRateConfig(int sampleRate) = 0;
    /* slot 2 (0x10) */
    virtual int LoadParameter(const char* path, bool b, float* f) = 0;
    /* slot 3 (0x18) - SetPar, unused by wrapper */
    virtual int SetPar(void*) = 0;
    /* slot 4 (0x20) */
    virtual int SetMotion(int flat) = 0;
    /* slot 5 (0x28) */
    virtual int SetOrientation(Orientation orientation) = 0;
    /* slot 6 (0x30) - SetVolumeTable, unused by wrapper */
    virtual int SetVolumeTable(float* table) = 0;
    /* slot 7 (0x38) - SetSEDMode, unused by wrapper */
    virtual int SetSEDMode(int mode) = 0;
    /* slot 8 (0x40) */
    virtual int BuffClear() = 0;
    /* slot 9 (0x48) - in-place DSP processing */
    virtual int Exe(void* in, void* out, int frames, float volume) = 0;

  protected:
    ISoundBooster() = default;
    // Destroyed via SoundBoosterFactory::Destroy(), never via delete.
    ~ISoundBooster() = default;
};

// SoundBooster_Interface_Factory is resolved dynamically because it lives in
// a closed-source Samsung library whose symbol layout differs across devices.
namespace SoundBoosterFactory {

inline ISoundBooster* Create(int mode, int fmFlag = 0) {
    using CreateFn2 = ISoundBooster* (*)(int, int);
    using CreateFn1 = ISoundBooster* (*)(int);

    if (auto* fn2 = reinterpret_cast<CreateFn2>(
                dlsym(RTLD_DEFAULT, "_ZN30SoundBooster_Interface_Factory6CreateEii"))) {
        return fn2(mode, fmFlag);
    }
    if (auto* fn1 = reinterpret_cast<CreateFn1>(
                dlsym(RTLD_DEFAULT, "_ZN30SoundBooster_Interface_Factory6CreateEi"))) {
        return fn1(mode);
    }

    ALOGE("SoundBoosterFactory::Create symbol not found");
    return nullptr;
}

inline void Destroy(ISoundBooster* booster) {
    using DestroyFn = void (*)(ISoundBooster*);

    static const DestroyFn fnDestroy = []() -> DestroyFn {
        if (auto* fn = reinterpret_cast<DestroyFn>(dlsym(
                    RTLD_DEFAULT,
                    "_ZN30SoundBooster_Interface_Factory7DestroyEP25SoundBooster_Interface_IF"))) {
            return fn;
        }
        auto* fn = reinterpret_cast<DestroyFn>(
                dlsym(RTLD_DEFAULT,
                      "_ZN30SoundBooster_Interface_Factory7DestroyEP24SoundBooster_Interface_IF"));
        if (fn == nullptr) {
            ALOGE("SoundBoosterFactory::Destroy symbol not found");
        }
        return fn;
    }();

    if (fnDestroy != nullptr) {
        fnDestroy(booster);
    }
}

}  // namespace SoundBoosterFactory

inline int compatBuffClear(ISoundBooster* booster) {
    if (booster == nullptr) {
        return -EINVAL;
    }
    using BuffClearFn = int (*)(void*);
    static const auto fn = reinterpret_cast<BuffClearFn>(
            dlsym(RTLD_DEFAULT, "_ZN22SoundBooster_Interface9BuffClearEv"));
    return fn != nullptr ? fn(booster) : booster->BuffClear();
}

inline int compatExe(ISoundBooster* booster, void* in, void* out, int frames, float volume) {
    if (booster == nullptr) {
        return -EINVAL;
    }
    using ExeFn = int (*)(void*, void*, const void*, int, float);
    static const auto fn =
            reinterpret_cast<ExeFn>(dlsym(RTLD_DEFAULT, "_ZN22SoundBooster_Interface3ExeEPvPKvif"));
    return fn != nullptr ? fn(booster, in, out, frames, volume)
                         : booster->Exe(in, out, frames, volume);
}
