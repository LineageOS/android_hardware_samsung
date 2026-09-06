/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <dlfcn.h>
#include <log/log.h>
#include <stdint.h>

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
    // Destroyed via SoundBoosterFactory::Destroy(), never via delete.
    ~ISoundBooster() {}
};

class SoundBoosterFactory {
  public:
    static inline ISoundBooster* Create(int mode, int fmFlag = 0) {
        typedef ISoundBooster* (*CreateFn2)(int, int);
        typedef ISoundBooster* (*CreateFn1)(int);

        CreateFn2 fn2 = reinterpret_cast<CreateFn2>(
                dlsym(RTLD_DEFAULT, "_ZN30SoundBooster_Interface_Factory6CreateEii"));
        if (fn2 != nullptr) {
            return fn2(mode, fmFlag);
        }

        CreateFn1 fn1 = reinterpret_cast<CreateFn1>(
                dlsym(RTLD_DEFAULT, "_ZN30SoundBooster_Interface_Factory6CreateEi"));
        if (fn1 != nullptr) {
            return fn1(mode);
        }

        ALOGE("SoundBoosterFactory::Create symbol not found");
        return nullptr;
    }

    static inline void Destroy(ISoundBooster* booster) {
        typedef void (*DestroyFn)(ISoundBooster*);
        static const auto fnDestroy = []() -> DestroyFn {
            auto fn = reinterpret_cast<DestroyFn>(dlsym(
                    RTLD_DEFAULT,
                    "_ZN30SoundBooster_Interface_Factory7DestroyEP25SoundBooster_Interface_IF"));
            if (fn == nullptr) {
                fn = reinterpret_cast<DestroyFn>(
                        dlsym(RTLD_DEFAULT,
                              "_ZN30SoundBooster_Interface_Factory7DestroyEP24SoundBooster_"
                              "Interface_IF"));
            }
            if (fn == nullptr) {
                ALOGE("SoundBoosterFactory::Destroy symbol not found");
            }
            return fn;
        }();
        if (fnDestroy != nullptr) {
            fnDestroy(booster);
        }
    }
};

static inline int compatBuffClear(ISoundBooster* booster) {
    if (booster == nullptr) {
        return -EINVAL;
    }
    typedef int (*BuffClearFn)(void*);
    static const auto fn = reinterpret_cast<BuffClearFn>(
            dlsym(RTLD_DEFAULT, "_ZN22SoundBooster_Interface9BuffClearEv"));
    if (fn != nullptr) {
        return fn(booster);
    }
    return booster->BuffClear();
}

static inline int compatExe(ISoundBooster* booster, void* in, void* out, int frames, float volume) {
    if (booster == nullptr) {
        return -EINVAL;
    }
    typedef int (*ExeFn)(void*, void*, const void*, int, float);
    static const auto fn =
            reinterpret_cast<ExeFn>(dlsym(RTLD_DEFAULT, "_ZN22SoundBooster_Interface3ExeEPvPKvif"));
    if (fn != nullptr) {
        return fn(booster, in, out, frames, volume);
    }
    return booster->Exe(in, out, frames, volume);
}
