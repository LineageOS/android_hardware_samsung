/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * SoundBooster Plus effect wrapper.
 */

#define LOG_TAG "SoundBoosterEffectPlus"

#include <errno.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include <hardware/audio_effect.h>
#include <log/log.h>
#include <system/audio.h>

#include "SoundBoosterInterface.h"

static const effect_uuid_t SOUNDBOOSTER_EFFECT_TYPE = {
        0xee8aeac0, 0x5d4b, 0x11e5, 0xa837, {0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66}};

static const effect_uuid_t SOUNDBOOSTER_EFFECT_IMPL = {
        0x50de45f0, 0x5d4c, 0x11e5, 0xa837, {0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66}};

static const char* const SOUNDBOOSTER_PARAM_FILE = "/vendor/etc/SoundBoosterParam.txt";

static const effect_descriptor_t SOUNDBOOSTER_DESCRIPTOR = {
        SOUNDBOOSTER_EFFECT_TYPE,
        SOUNDBOOSTER_EFFECT_IMPL,
        EFFECT_CONTROL_API_VERSION,
        0x01000290,
        75,
        25,
        "SoundBooster Plus",
        "Samsung",
};

class SoundBooster {
  public:
    explicit SoundBooster(uint32_t mode);
    ~SoundBooster();

    int init(uint32_t sampleRate, audio_format_t format);
    int process(void* in, void* out, size_t frames, float volume);
    void clear();
    void setRotation(int rotation);
    void setFlatMotion(int flat);
    void readParam();
    bool isEnabledDevice(uint32_t device);
    void setSessionId(int32_t sessionId);

  private:
    SoundBooster_Interface_IF* mInterface;
    uint32_t mMode;
    int32_t mSessionId;
    bool mEnabled;
    uint32_t mSampleRate;
    audio_format_t mFormat;
    int mBitDepth;
};

SoundBooster::SoundBooster(uint32_t mode)
    : mInterface(nullptr),
      mMode(mode),
      mSessionId(0),
      mEnabled(false),
      mSampleRate(0),
      mFormat(AUDIO_FORMAT_INVALID),
      mBitDepth(SB_BITDEPTH_NONE) {
    if (mMode > 2) {
        ALOGE("Invalid SoundBooster mode %u", mMode);
        mMode = 2;
    }
    mInterface = SoundBooster_Interface_Factory::Create(static_cast<int>(mMode), 0);
}

SoundBooster::~SoundBooster() {
    if (mInterface != nullptr) {
        SoundBooster_Interface_Factory::Destroy(mInterface);
        mInterface = nullptr;
    }
    mEnabled = false;
}

int SoundBooster::init(uint32_t sampleRate, audio_format_t format) {
    if (mInterface == nullptr) {
        mInterface = SoundBooster_Interface_Factory::Create(static_cast<int>(mMode), 0);
        if (mInterface == nullptr) {
            return -ENODEV;
        }
    }

    int bitDepth = SB_BITDEPTH_NONE;
    if (format == AUDIO_FORMAT_PCM_FLOAT) {
        bitDepth = SB_BITDEPTH_FLOAT;
    } else if (format == AUDIO_FORMAT_PCM_8_24_BIT) {
        bitDepth = SB_BITDEPTH_8_24;
    }

    mSampleRate = sampleRate;
    mFormat = format;
    mBitDepth = bitDepth;

    mInterface->Init(static_cast<SB_BitDepth_T>(bitDepth));
    mInterface->SamplingRateConfig(static_cast<int>(sampleRate));

    if (access(SOUNDBOOSTER_PARAM_FILE, 0) == 0) {
        mInterface->LoadParameter(SOUNDBOOSTER_PARAM_FILE, 0, nullptr);
    } else {
        ALOGE("Can not find SoundBoosterParam.txt");
    }
    return 0;
}

int SoundBooster::process(void* in, void* out, size_t frames, float volume) {
    if (frames == 0 || mInterface == nullptr) {
        return 0;
    }
    mEnabled = true;
    return mInterface->Exe(in, out, static_cast<int>(frames), volume);
}

void SoundBooster::clear() {
    if (mInterface != nullptr) {
        mInterface->BuffClear();
    }
}

void SoundBooster::setRotation(int rotation) {
    if (mInterface == nullptr) {
        return;
    }
    if (rotation == 3) {
        rotation = 1;
    } else if (rotation == 1) {
        rotation = 3;
    }
    mInterface->SetOrientation(static_cast<SB_Device_Orientation_T>(rotation));
}

void SoundBooster::setFlatMotion(int flat) {
    if (mInterface == nullptr) {
        return;
    }
    mInterface->SetMotion(flat);
    ALOGI("[GOF] setFlatMotion : flat %d", flat);
}

void SoundBooster::readParam() {
    if (mInterface == nullptr) {
        return;
    }
    if (access(SOUNDBOOSTER_PARAM_FILE, 0) != 0) {
        ALOGE("Can not find SoundBoosterParam.txt");
        return;
    }
    mInterface->LoadParameter(SOUNDBOOSTER_PARAM_FILE, 0, nullptr);
}

bool SoundBooster::isEnabledDevice(uint32_t device) {
    return device == AUDIO_DEVICE_OUT_SPEAKER;
}

void SoundBooster::setSessionId(int32_t sessionId) {
    mSessionId = sessionId;
}

struct SoundBoosterEffect {
    const struct effect_interface_s* itfe;
    int32_t sessionId;
    int32_t state; /* 0=uninit, 1=init(disabled), 2=running(enabled) */
    int32_t mode;
    float logVolume;
    float leftVolume;
    float rightVolume;
    uint32_t curDevice;
    bool enabled;
    int32_t rotation;
    int32_t flatMotion;
    int32_t spkMode;
    SoundBooster* dsp;
};

static int SoundBooster_process(effect_handle_t self, audio_buffer_t* in, audio_buffer_t* out) {
    SoundBoosterEffect* e = reinterpret_cast<SoundBoosterEffect*>(self);
    if (e == nullptr) {
        return -EINVAL;
    }
    if (e->state != 2) {
        return -ENODATA;
    }
    if (e->dsp == nullptr) {
        return -EINVAL;
    }
    if (in == nullptr || out == nullptr) {
        return -EINVAL;
    }
    if (in->raw == nullptr || out->raw == nullptr) {
        return -EINVAL;
    }
    if (in->frameCount == 0 || in->frameCount != out->frameCount) {
        return -EINVAL;
    }

    if (!e->enabled) {
        // Passthrough: device not processed, just copy the input through.
        if (in->raw != out->raw) {
            memcpy(out->raw, in->raw, in->frameCount * 8);
        }
        return 0;
    }

    e->dsp->process(in->raw, in->raw, in->frameCount, e->logVolume);

    if (in->raw != out->raw) {
        // Stereo float: 2 channels * 4 bytes per frame.
        memcpy(out->raw, in->raw, in->frameCount * 8);
    }
    return 0;
}

static int SoundBooster_setDevice(SoundBoosterEffect* e, uint32_t device) {
    if (e == nullptr) {
        return -EINVAL;
    }
    if (e->state == 0) {
        ALOGE("Not initialized");
        return -ENODATA;
    }
    bool wasEnabled = e->enabled;
    e->curDevice = device;
    bool enabled = e->dsp != nullptr ? e->dsp->isEnabledDevice(device) : false;
    e->enabled = enabled;
    if (!wasEnabled && enabled) {
        ALOGI("SB_reset SoundBooster sessionId(%d)", e->sessionId);
        if (e->dsp != nullptr) {
            e->dsp->clear();
        }
    }
    return 0;
}

static int SoundBooster_setVolume(SoundBoosterEffect* e, uint32_t* vol) {
    if (e == nullptr) {
        return -EINVAL;
    }
    if (e->state == 0) {
        ALOGE("Not initialized");
        return -ENODATA;
    }
    float left = static_cast<float>(vol[0]) * 5.9604645e-08f;
    float right = static_cast<float>(vol[0] + 1) * 5.9604645e-08f;
    if (e->leftVolume == left && e->rightVolume == right) {
        return 0;
    }
    e->leftVolume = left;
    e->rightVolume = right;
    float maxVol = left > right ? left : right;
    e->logVolume = logf(maxVol + 1e-05f) * 8.68589f;
    return 0;
}

static int SoundBooster_init(SoundBoosterEffect* e, effect_config_t* config) {
    uint32_t mode = 2;
    if (config != nullptr && config->inputCfg.samplingRate > 2) {
        mode = config->inputCfg.samplingRate;
        if (mode > 2) {
            ALOGE("Invalid SoundBooster mode");
            mode = 2;
        }
    }
    e->mode = mode;
    if (e->dsp == nullptr) {
        e->dsp = new SoundBooster(mode);
        ALOGI("Allocate SoundBooster mode:%d", e->mode);
    }
    e->dsp->setSessionId(e->sessionId);
    e->rotation = 0;
    e->spkMode = 0;
    // EFFECT_CMD_ENABLE transitions state to 2.
    e->state = 1;
    return 0;
}

static int SoundBooster_setConfig(SoundBoosterEffect* e, effect_config_t* config) {
    if (e == nullptr || config == nullptr) {
        return -EINVAL;
    }
    if (config->inputCfg.samplingRate != config->outputCfg.samplingRate ||
        config->inputCfg.format != config->outputCfg.format || e->dsp == nullptr) {
        return -EINVAL;
    }
    e->dsp->init(config->inputCfg.samplingRate,
                 static_cast<audio_format_t>(config->inputCfg.format));
    return 0;
}

static int SoundBooster_command(effect_handle_t self, uint32_t cmdCode, uint32_t cmdSize,
                                void* cmdData, uint32_t* replySize, void* replyData) {
    SoundBoosterEffect* e = reinterpret_cast<SoundBoosterEffect*>(self);
    if (e == nullptr) {
        return -EINVAL;
    }
    if (e->state == 0) {
        return -EINVAL;
    }

    switch (cmdCode) {
        case EFFECT_CMD_INIT:
            if (replySize == nullptr || replyData == nullptr || *replySize != sizeof(int)) {
                ALOGE("return EFFECT_CMD_INIT fail");
                return -EINVAL;
            }
            SoundBooster_init(e, reinterpret_cast<effect_config_t*>(cmdData));
            *reinterpret_cast<int*>(replyData) = 0;
            return 0;

        case EFFECT_CMD_SET_CONFIG:
            if (cmdData == nullptr || replySize == nullptr || replyData == nullptr ||
                cmdSize != sizeof(effect_config_t) || *replySize != sizeof(int)) {
                return -EINVAL;
            }
            *reinterpret_cast<int*>(replyData) =
                    SoundBooster_setConfig(e, reinterpret_cast<effect_config_t*>(cmdData));
            return 0;

        case EFFECT_CMD_RESET:
            ALOGI("SB_reset SoundBooster sessionId(%d)", e->sessionId);
            if (e->dsp != nullptr) {
                e->dsp->clear();
            }
            return 0;

        case EFFECT_CMD_ENABLE:
            if (replySize == nullptr || replyData == nullptr || *replySize != sizeof(int)) {
                return -EINVAL;
            }
            if (e->state != 1) {
                return -ENODATA;
            }
            e->state = 2;
            *reinterpret_cast<int*>(replyData) = 0;
            return 0;

        case EFFECT_CMD_DISABLE:
            if (replySize == nullptr || replyData == nullptr || *replySize != sizeof(int)) {
                return -EINVAL;
            }
            if (e->state != 2) {
                return -ENODATA;
            }
            e->state = 1;
            *reinterpret_cast<int*>(replyData) = 0;
            return 0;

        case EFFECT_CMD_SET_PARAM: {
            if (replySize == nullptr || replyData == nullptr || cmdData == nullptr ||
                cmdSize <= 0x13 || *replySize != sizeof(int)) {
                return -EINVAL;
            }
            effect_param_t* param = reinterpret_cast<effect_param_t*>(cmdData);
            *reinterpret_cast<int*>(replyData) = 0;
            if (param->psize - 4 < 5 && param->vsize > 3) {
                int32_t paramId = *reinterpret_cast<int32_t*>(param->data);
                if (paramId == 2) { /* flatMotion */
                    int32_t value = *reinterpret_cast<int32_t*>(param->data + 4);
                    e->flatMotion = value;
                    if (e->dsp != nullptr) {
                        e->dsp->setFlatMotion(value);
                    }
                    return 0;
                }
                if (paramId == 1) { /* rotation */
                    int32_t value = *reinterpret_cast<int32_t*>(param->data + 4);
                    if (e->rotation != value) {
                        e->rotation = value;
                        if (e->dsp != nullptr) {
                            e->dsp->setRotation(value);
                        }
                    }
                    return 0;
                }
                if (paramId == 0) { /* readParam */
                    if (e->dsp != nullptr) {
                        e->dsp->readParam();
                    }
                    return 0;
                }
                return 0;
            }
            *reinterpret_cast<int*>(replyData) = -EINVAL;
            return 0;
        }

        case EFFECT_CMD_SET_DEVICE:
            if (cmdSize != sizeof(uint32_t) || cmdData == nullptr) {
                return -EINVAL;
            }
            return SoundBooster_setDevice(e, *reinterpret_cast<uint32_t*>(cmdData));

        case EFFECT_CMD_SET_VOLUME:
            if (cmdSize != 2 * sizeof(uint32_t) || cmdData == nullptr) {
                ALOGE("command EFFECT_CMD_SET_VOLUME");
                return -EINVAL;
            }
            return SoundBooster_setVolume(e, reinterpret_cast<uint32_t*>(cmdData));

        case EFFECT_CMD_GET_PARAM:
            return 0;

        case EFFECT_CMD_SET_AUDIO_MODE:
            return 0;

        case EFFECT_CMD_GET_CONFIG:
            if (replyData == nullptr || replySize == nullptr || *replySize != 0x70) {
                return -EINVAL;
            }
            return 0;

        case EFFECT_CMD_DUMP:
            ALOGI("[context] mCurDevice: %d, mSpkmode: %d, rotation_info: %d, flatMotion_info: %d",
                  e->curDevice, e->spkMode, e->rotation, e->flatMotion);
            return 0;

        default:
            ALOGW("SB_command invalid command %d", cmdCode);
            return -EINVAL;
    }
}

static int SoundBooster_getDescriptor(effect_handle_t self, effect_descriptor_t* descriptor) {
    if (self == nullptr || descriptor == nullptr) {
        return -EINVAL;
    }
    *descriptor = SOUNDBOOSTER_DESCRIPTOR;
    return 0;
}

static const struct effect_interface_s SOUNDBOOSTER_EFFECT_INTERFACE = {
        SoundBooster_process,
        SoundBooster_command,
        SoundBooster_getDescriptor,
        nullptr,
};

static int SoundBoosterLib_Create(const effect_uuid_t* uuid, int32_t sessionId, int32_t ioId,
                                  effect_handle_t* handle) {
    if (handle == nullptr || uuid == nullptr) {
        return -EINVAL;
    }
    // The EffectsFactory passes the implementation UUID (audio_effects.xml).
    if (memcmp(uuid, &SOUNDBOOSTER_EFFECT_IMPL, sizeof(effect_uuid_t)) != 0) {
        return -EINVAL;
    }

    SoundBoosterEffect* e = new SoundBoosterEffect();
    if (e == nullptr) {
        return -ENOMEM;
    }
    e->itfe = &SOUNDBOOSTER_EFFECT_INTERFACE;
    e->sessionId = sessionId;
    e->state = 1;
    e->mode = 2;
    e->logVolume = -100.0f;
    e->leftVolume = -100.0f;
    e->rightVolume = 0.0f;
    e->curDevice = 0;
    e->enabled = true;
    e->rotation = 0;
    e->flatMotion = 0;
    e->spkMode = 0;
    e->dsp = nullptr;

    *handle = reinterpret_cast<effect_handle_t>(e);
    return 0;
}

static int SoundBoosterLib_Release(effect_handle_t handle) {
    SoundBoosterEffect* e = reinterpret_cast<SoundBoosterEffect*>(handle);
    if (e == nullptr) {
        return -EINVAL;
    }
    e->state = 0;
    if (e->dsp != nullptr) {
        delete e->dsp;
        e->dsp = nullptr;
    }
    delete e;
    return 0;
}

static int SoundBoosterLib_GetDescriptor(const effect_uuid_t* uuid,
                                         effect_descriptor_t* descriptor) {
    if (descriptor == nullptr || uuid == nullptr) {
        return -EINVAL;
    }
    if (memcmp(uuid, &SOUNDBOOSTER_EFFECT_TYPE, sizeof(effect_uuid_t)) != 0 &&
        memcmp(uuid, &SOUNDBOOSTER_EFFECT_IMPL, sizeof(effect_uuid_t)) != 0) {
        return -EINVAL;
    }
    *descriptor = SOUNDBOOSTER_DESCRIPTOR;
    return 0;
}

static const audio_effect_library_t SOUNDBOOSTER_LIBRARY_INFO = {
        .tag = AUDIO_EFFECT_LIBRARY_TAG,
        .version = EFFECT_LIBRARY_API_VERSION,
        .name = "SoundBooster Plus Library",
        .implementor = "Samsung",
        .create_effect = SoundBoosterLib_Create,
        .release_effect = SoundBoosterLib_Release,
        .get_descriptor = SoundBoosterLib_GetDescriptor,
};

audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = SOUNDBOOSTER_LIBRARY_INFO;