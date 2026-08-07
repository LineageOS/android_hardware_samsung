/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * Interface for the SoundBooster DSP library.
 */

#ifndef SOUNDBOOSTER_INTERFACE_H
#define SOUNDBOOSTER_INTERFACE_H

#include <stdint.h>

enum SB_BitDepth_T {
    SB_BITDEPTH_NONE = 0,
    SB_BITDEPTH_8_24 = 1,
    SB_BITDEPTH_FLOAT = 3,
};

enum SB_Device_Orientation_T {
    SB_ORIENTATION_0 = 0,
    SB_ORIENTATION_90 = 1,
    SB_ORIENTATION_180 = 2,
    SB_ORIENTATION_270 = 3,
};

class SoundBooster_Interface_IF {
  public:
    /* slot 0 (0x00) */
    virtual int Init(SB_BitDepth_T bitDepth) = 0;
    /* slot 1 (0x08) */
    virtual int SamplingRateConfig(int sampleRate) = 0;
    /* slot 2 (0x10) */
    virtual int LoadParameter(const char* path, bool b, float* f) = 0;
    /* slot 3 (0x18) - SetPar, unused by wrapper */
    virtual int SetPar(void*) = 0;
    /* slot 4 (0x20) */
    virtual int SetMotion(int flat) = 0;
    /* slot 5 (0x28) */
    virtual int SetOrientation(SB_Device_Orientation_T orientation) = 0;
    /* slot 6 (0x30) - SetVolumeTable, unused by wrapper */
    virtual int SetVolumeTable(float* table) = 0;
    /* slot 7 (0x38) - SetSEDMode, unused by wrapper */
    virtual int SetSEDMode(int mode) = 0;
    /* slot 8 (0x40) */
    virtual int BuffClear() = 0;
    /* slot 9 (0x48) - in-place DSP processing */
    virtual int Exe(void* in, void* out, int frames, float volume) = 0;

  protected:
    // Destroyed via SoundBooster_Interface_Factory::Destroy(), never via delete.
    ~SoundBooster_Interface_IF() {}
};

#include <dlfcn.h>
#include <log/log.h>

class SoundBooster_Interface_Factory {
  public:
    static inline SoundBooster_Interface_IF* Create(int mode, int fmFlag = 0) {
        typedef SoundBooster_Interface_IF* (*CreateFn2)(int, int);
        typedef SoundBooster_Interface_IF* (*CreateFn1)(int);

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

        ALOGE("SoundBooster_Interface_Factory::Create symbol not found");
        return nullptr;
    }

    static inline void Destroy(SoundBooster_Interface_IF* interface) {
        typedef void (*DestroyFn)(SoundBooster_Interface_IF*);
        DestroyFn fnDestroy = reinterpret_cast<DestroyFn>(
            dlsym(RTLD_DEFAULT, "_ZN30SoundBooster_Interface_Factory7DestroyEP24SoundBooster_Interface_IF"));
        if (fnDestroy != nullptr) {
            fnDestroy(interface);
        }
    }
};

#endif /* SOUNDBOOSTER_INTERFACE_H */
