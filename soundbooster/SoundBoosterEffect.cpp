/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SoundBoosterEffectPlus"

#include <cerrno>
#include <cmath>
#include <cstring>
#include <unistd.h>

#include <hardware/audio_effect.h>
#include <log/log.h>
#include <system/audio.h>

#include "SoundBoosterInterface.h"

namespace {

constexpr char kParamFile[] = "/vendor/etc/SoundBoosterParam.txt";

constexpr uint32_t kModeDefault = 2;
constexpr uint32_t kModeMax = 2;

// Stereo output, 4-byte samples (PCM float or PCM 8.24).
constexpr size_t kBytesPerSample = sizeof(int32_t);
constexpr size_t kChannelCount = 2;
constexpr size_t kBytesPerFrame = kBytesPerSample * kChannelCount;

constexpr float kMinVolumeDb = -100.0f;
constexpr float kVolumeScale = 1.0f / static_cast<float>(1 << 24);
constexpr float kVolumeEpsilon = 1e-5f;
constexpr float kLinearToDbScale = 8.68589f;  // 20 / ln(10)

constexpr uint32_t kMinSetParamCmdSize = 0x13;
constexpr uint32_t kEffectConfigReplySize = 0x70;  // sizeof(effect_config_t)

const effect_uuid_t kEffectTypeUuid = {
        0xee8aeac0, 0x5d4b, 0x11e5, 0xa837, {0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66}};
const effect_uuid_t kEffectImplUuid = {
        0x50de45f0, 0x5d4c, 0x11e5, 0xa837, {0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66}};

const effect_descriptor_t kEffectDescriptor = {
        kEffectTypeUuid,
        kEffectImplUuid,
        EFFECT_CONTROL_API_VERSION,
        (EFFECT_FLAG_TYPE_INSERT | EFFECT_FLAG_INSERT_LAST | EFFECT_FLAG_VOLUME_IND |
         EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_NOT_ELIGIBLE_SUSPEND),
        75,
        25,
        "SoundBooster Plus",
        "Samsung",
};

enum EffectState {
    EFFECT_STATE_UNINITIALIZED = 0,
    EFFECT_STATE_INITIALIZED = 1,
    EFFECT_STATE_ACTIVE = 2,
};

enum EffectParam {
    PARAM_READ_PARAM = 0,
    PARAM_ROTATION = 1,
    PARAM_FLAT_MOTION = 2,
};

}  // namespace

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
    void setSessionId(int32_t sessionId);
    static bool isEnabledDevice(uint32_t device);

  private:
    ISoundBooster* mInterface;
    uint32_t mMode;
    int32_t mSessionId;
    uint32_t mSampleRate;
    audio_format_t mFormat;
    BitDepth mBitDepth;
};

SoundBooster::SoundBooster(uint32_t mode)
    : mInterface(nullptr),
      mMode(mode),
      mSessionId(0),
      mSampleRate(0),
      mFormat(AUDIO_FORMAT_INVALID),
      mBitDepth(BIT_DEPTH_NONE) {
    if (mMode > kModeMax) {
        ALOGE("Invalid SoundBooster mode %u", mMode);
        mMode = kModeDefault;
    }
    mInterface = SoundBoosterFactory::Create(static_cast<int>(mMode));
}

SoundBooster::~SoundBooster() {
    if (mInterface != nullptr) {
        SoundBoosterFactory::Destroy(mInterface);
        mInterface = nullptr;
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

    if (access(kParamFile, F_OK) == 0) {
        mInterface->LoadParameter(kParamFile, false, nullptr);
    } else {
        ALOGE("Cannot find SoundBoosterParam.txt");
    }
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
    ALOGI("setFlatMotion: flat %d", flat);
}

void SoundBooster::readParam() {
    if (mInterface == nullptr) {
        return;
    }
    if (access(kParamFile, F_OK) != 0) {
        ALOGE("Cannot find SoundBoosterParam.txt");
        return;
    }
    mInterface->LoadParameter(kParamFile, false, nullptr);
}

bool SoundBooster::isEnabledDevice(uint32_t device) {
    return device == AUDIO_DEVICE_OUT_SPEAKER;
}

void SoundBooster::setSessionId(int32_t sessionId) {
    mSessionId = sessionId;
}

namespace {

struct SoundBoosterEffect {
    const effect_interface_s* itfe = nullptr;
    int32_t sessionId = 0;
    EffectState state = EFFECT_STATE_UNINITIALIZED;
    int32_t mode = kModeDefault;
    float logVolume = kMinVolumeDb;
    float leftVolume = kMinVolumeDb;
    float rightVolume = 0.0f;
    uint32_t curDevice = 0;
    bool enabled = true;
    int32_t rotation = 0;
    int32_t flatMotion = 0;
    int32_t spkMode = 0;
    SoundBooster* dsp = nullptr;
};

int SoundBooster_process(effect_handle_t self, audio_buffer_t* in, audio_buffer_t* out) {
    auto* e = reinterpret_cast<SoundBoosterEffect*>(self);
    if (e == nullptr || e->dsp == nullptr) {
        return -EINVAL;
    }
    if (e->state != EFFECT_STATE_ACTIVE) {
        return -ENODATA;
    }
    if (in == nullptr || out == nullptr || in->raw == nullptr || out->raw == nullptr) {
        return -EINVAL;
    }
    if (in->frameCount == 0 || in->frameCount != out->frameCount) {
        return -EINVAL;
    }

    if (e->enabled) {
        e->dsp->process(in->raw, in->raw, in->frameCount, e->logVolume);
    }

    if (in->raw != out->raw) {
        memcpy(out->raw, in->raw, in->frameCount * kBytesPerFrame);
    }
    return 0;
}

int SoundBooster_setDevice(SoundBoosterEffect* e, uint32_t device) {
    if (e == nullptr) {
        return -EINVAL;
    }
    if (e->state == EFFECT_STATE_UNINITIALIZED) {
        ALOGE("Not initialized");
        return -ENODATA;
    }

    bool wasEnabled = e->enabled;
    e->curDevice = device;
    e->enabled = e->dsp != nullptr && SoundBooster::isEnabledDevice(device);

    if (!wasEnabled && e->enabled) {
        ALOGI("SB_reset SoundBooster sessionId(%d)", e->sessionId);
        if (e->dsp != nullptr) {
            e->dsp->clear();
        }
    }
    return 0;
}

int SoundBooster_setVolume(SoundBoosterEffect* e, uint32_t* vol) {
    if (e == nullptr) {
        return -EINVAL;
    }
    if (e->state == EFFECT_STATE_UNINITIALIZED) {
        ALOGE("Not initialized");
        return -ENODATA;
    }

    float left = static_cast<float>(vol[0]) * kVolumeScale;
    float right = static_cast<float>(vol[1]) * kVolumeScale;
    if (e->leftVolume == left && e->rightVolume == right) {
        return 0;
    }

    e->leftVolume = left;
    e->rightVolume = right;
    float maxVol = left > right ? left : right;
    e->logVolume = logf(maxVol + kVolumeEpsilon) * kLinearToDbScale;
    return 0;
}

int SoundBooster_init(SoundBoosterEffect* e, effect_config_t* config) {
    uint32_t mode = kModeDefault;
    if (config != nullptr && config->inputCfg.samplingRate > kModeMax) {
        mode = config->inputCfg.samplingRate;
        if (mode > kModeMax) {
            ALOGE("Invalid SoundBooster mode");
            mode = kModeDefault;
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
    // EFFECT_CMD_ENABLE transitions state to active.
    e->state = EFFECT_STATE_INITIALIZED;
    return 0;
}

int SoundBooster_setConfig(SoundBoosterEffect* e, effect_config_t* config) {
    if (e == nullptr || config == nullptr || e->dsp == nullptr) {
        return -EINVAL;
    }
    if (config->inputCfg.samplingRate != config->outputCfg.samplingRate ||
        config->inputCfg.format != config->outputCfg.format) {
        return -EINVAL;
    }
    e->dsp->init(config->inputCfg.samplingRate,
                 static_cast<audio_format_t>(config->inputCfg.format));
    return 0;
}

int SoundBooster_command(effect_handle_t self, uint32_t cmdCode, uint32_t cmdSize, void* cmdData,
                         uint32_t* replySize, void* replyData) {
    auto* e = reinterpret_cast<SoundBoosterEffect*>(self);
    if (e == nullptr) {
        return -EINVAL;
    }
    if (e->state == EFFECT_STATE_UNINITIALIZED) {
        return -EINVAL;
    }

    switch (cmdCode) {
        case EFFECT_CMD_INIT:
            if (replySize == nullptr || replyData == nullptr || *replySize != sizeof(int)) {
                ALOGE("EFFECT_CMD_INIT failed");
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
            if (e->state != EFFECT_STATE_INITIALIZED) {
                return -ENODATA;
            }
            e->state = EFFECT_STATE_ACTIVE;
            *reinterpret_cast<int*>(replyData) = 0;
            return 0;

        case EFFECT_CMD_DISABLE:
            if (replySize == nullptr || replyData == nullptr || *replySize != sizeof(int)) {
                return -EINVAL;
            }
            if (e->state != EFFECT_STATE_ACTIVE) {
                return -ENODATA;
            }
            e->state = EFFECT_STATE_INITIALIZED;
            *reinterpret_cast<int*>(replyData) = 0;
            return 0;

        case EFFECT_CMD_SET_PARAM: {
            if (replySize == nullptr || replyData == nullptr || cmdData == nullptr ||
                cmdSize <= kMinSetParamCmdSize || *replySize != sizeof(int)) {
                return -EINVAL;
            }
            auto* param = reinterpret_cast<effect_param_t*>(cmdData);
            *reinterpret_cast<int*>(replyData) = 0;
            if (param->psize - sizeof(int32_t) >= 5 || param->vsize <= 3) {
                *reinterpret_cast<int*>(replyData) = -EINVAL;
                return 0;
            }

            int32_t paramId = *reinterpret_cast<int32_t*>(param->data);
            switch (paramId) {
                case PARAM_FLAT_MOTION: {
                    int32_t value = *reinterpret_cast<int32_t*>(param->data + sizeof(int32_t));
                    e->flatMotion = value;
                    if (e->dsp != nullptr) {
                        e->dsp->setFlatMotion(value);
                    }
                    return 0;
                }
                case PARAM_ROTATION: {
                    int32_t value = *reinterpret_cast<int32_t*>(param->data + sizeof(int32_t));
                    if (e->rotation != value) {
                        e->rotation = value;
                        if (e->dsp != nullptr) {
                            e->dsp->setRotation(value);
                        }
                    }
                    return 0;
                }
                case PARAM_READ_PARAM:
                    if (e->dsp != nullptr) {
                        e->dsp->readParam();
                    }
                    return 0;
                default:
                    return 0;
            }
        }

        case EFFECT_CMD_SET_DEVICE:
            if (cmdSize != sizeof(uint32_t) || cmdData == nullptr) {
                return -EINVAL;
            }
            return SoundBooster_setDevice(e, *reinterpret_cast<uint32_t*>(cmdData));

        case EFFECT_CMD_SET_VOLUME:
            if (cmdSize != 2 * sizeof(uint32_t) || cmdData == nullptr) {
                ALOGE("EFFECT_CMD_SET_VOLUME failed");
                return -EINVAL;
            }
            return SoundBooster_setVolume(e, reinterpret_cast<uint32_t*>(cmdData));

        case EFFECT_CMD_GET_PARAM:
        case EFFECT_CMD_SET_AUDIO_MODE:
            return 0;

        case EFFECT_CMD_GET_CONFIG:
            if (replyData == nullptr || replySize == nullptr ||
                *replySize != kEffectConfigReplySize) {
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

int SoundBooster_getDescriptor(effect_handle_t self, effect_descriptor_t* descriptor) {
    if (self == nullptr || descriptor == nullptr) {
        return -EINVAL;
    }
    *descriptor = kEffectDescriptor;
    return 0;
}

const effect_interface_s kSoundBoosterEffectInterface = {
        SoundBooster_process,
        SoundBooster_command,
        SoundBooster_getDescriptor,
        nullptr,
};

int SoundBoosterLib_Create(const effect_uuid_t* uuid, int32_t sessionId, int32_t /* ioId */,
                           effect_handle_t* handle) {
    if (handle == nullptr || uuid == nullptr) {
        return -EINVAL;
    }
    // The EffectsFactory passes the implementation UUID (audio_effects.xml).
    if (memcmp(uuid, &kEffectImplUuid, sizeof(effect_uuid_t)) != 0) {
        return -EINVAL;
    }

    auto* e = new SoundBoosterEffect();
    e->itfe = &kSoundBoosterEffectInterface;
    e->sessionId = sessionId;
    e->state = EFFECT_STATE_INITIALIZED;

    *handle = reinterpret_cast<effect_handle_t>(e);
    return 0;
}

int SoundBoosterLib_Release(effect_handle_t handle) {
    auto* e = reinterpret_cast<SoundBoosterEffect*>(handle);
    if (e == nullptr) {
        return -EINVAL;
    }
    e->state = EFFECT_STATE_UNINITIALIZED;
    delete e->dsp;
    delete e;
    return 0;
}

int SoundBoosterLib_GetDescriptor(const effect_uuid_t* uuid, effect_descriptor_t* descriptor) {
    if (descriptor == nullptr || uuid == nullptr) {
        return -EINVAL;
    }
    if (memcmp(uuid, &kEffectTypeUuid, sizeof(effect_uuid_t)) != 0 &&
        memcmp(uuid, &kEffectImplUuid, sizeof(effect_uuid_t)) != 0) {
        return -EINVAL;
    }
    *descriptor = kEffectDescriptor;
    return 0;
}

const audio_effect_library_t kSoundBoosterLibraryInfo = {
        .tag = AUDIO_EFFECT_LIBRARY_TAG,
        .version = EFFECT_LIBRARY_API_VERSION,
        .name = "SoundBooster Plus Library",
        .implementor = "Samsung",
        .create_effect = SoundBoosterLib_Create,
        .release_effect = SoundBoosterLib_Release,
        .get_descriptor = SoundBoosterLib_GetDescriptor,
};

}  // namespace

extern "C" __attribute__((visibility("default")))
audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = kSoundBoosterLibraryInfo;
