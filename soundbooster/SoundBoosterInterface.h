/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <log/log.h>
#include <cerrno>
#include <cstdint>

#define SB_WEAK_SYMBOL(decl, mangled) decl __asm__(mangled) __attribute__((weak))

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

namespace SoundBoosterSymbols {

SB_WEAK_SYMBOL(ISoundBooster* FactoryCreate2(int, int),
               "_ZN30SoundBooster_Interface_Factory6CreateEii");
SB_WEAK_SYMBOL(ISoundBooster* FactoryCreate1(int), "_ZN30SoundBooster_Interface_Factory6CreateEi");

SB_WEAK_SYMBOL(void FactoryDestroyV1(ISoundBooster*),
               "_ZN30SoundBooster_Interface_Factory7DestroyEP25SoundBooster_Interface_IF");
SB_WEAK_SYMBOL(void FactoryDestroyV2(ISoundBooster*),
               "_ZN30SoundBooster_Interface_Factory7DestroyEP24SoundBooster_Interface_IF");

SB_WEAK_SYMBOL(int InterfaceBuffClear(void*), "_ZN22SoundBooster_Interface9BuffClearEv");
SB_WEAK_SYMBOL(int InterfaceExe(void*, void*, const void*, int, float),
               "_ZN22SoundBooster_Interface3ExeEPvPKvif");

}  // namespace SoundBoosterSymbols

namespace SoundBoosterFactory {

inline ISoundBooster* Create(int mode, int fmFlag = 0) {
    if (SoundBoosterSymbols::FactoryCreate2 != nullptr) {
        return SoundBoosterSymbols::FactoryCreate2(mode, fmFlag);
    }
    if (SoundBoosterSymbols::FactoryCreate1 != nullptr) {
        return SoundBoosterSymbols::FactoryCreate1(mode);
    }

    ALOGE("SoundBoosterFactory::Create symbol not found");
    return nullptr;
}

inline void Destroy(ISoundBooster* booster) {
    using DestroyFn = void (*)(ISoundBooster*);

    static const DestroyFn fnDestroy = []() -> DestroyFn {
        if (SoundBoosterSymbols::FactoryDestroyV1 != nullptr) {
            return SoundBoosterSymbols::FactoryDestroyV1;
        }
        if (SoundBoosterSymbols::FactoryDestroyV2 != nullptr) {
            return SoundBoosterSymbols::FactoryDestroyV2;
        }
        ALOGE("SoundBoosterFactory::Destroy symbol not found");
        return nullptr;
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
    return SoundBoosterSymbols::InterfaceBuffClear != nullptr
                   ? SoundBoosterSymbols::InterfaceBuffClear(booster)
                   : booster->BuffClear();
}

inline int compatExe(ISoundBooster* booster, void* in, void* out, int frames, float volume) {
    if (booster == nullptr) {
        return -EINVAL;
    }
    return SoundBoosterSymbols::InterfaceExe != nullptr
                   ? SoundBoosterSymbols::InterfaceExe(booster, in, out, frames, volume)
                   : booster->Exe(in, out, frames, volume);
}
