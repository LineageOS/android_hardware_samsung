/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SoundBoosterEffect"
// #define LOG_NDEBUG 0

#include <errno.h>
#include <math.h>
#include <string.h>
#include <new>


#include <log/log.h>
#include <system/audio.h>

#include "SoundBoosterEffect.h"

// effect_handle_t interface implementation for SoundBooster
extern "C" const struct effect_interface_s gSoundBoosterEffectInterface;

#define CHECK_ARG(cond)                           \
    {                                             \
        if (!(cond)) {                            \
            ALOGE("Invalid argument: %s", #cond); \
            return -EINVAL;                       \
        }                                         \
    }

namespace android {
namespace {

// Samsung SoundBooster Plus
const effect_descriptor_t gSoundBoosterDescriptor = {
        {0xee8aeac0, 0x5d4b, 0x11e5, 0xa837, {0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66}},  // type
        {0x50de45f0, 0x5d4c, 0x11e5, 0xa837, {0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66}},  // uuid
        EFFECT_CONTROL_API_VERSION,
        (EFFECT_FLAG_TYPE_INSERT | EFFECT_FLAG_INSERT_LAST | EFFECT_FLAG_VOLUME_IND |
         EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_NOT_ELIGIBLE_SUSPEND),
        SOUNDBOOSTER_CUP_LOAD,
        SOUNDBOOSTER_MEM_USAGE,
        "SoundBooster Plus",
        "Samsung",
};

int Effect_init(EffectContext* pContext, effect_config_t* pConfig) {
    uint32_t mode = SB_MODE_DEFAULT;
    if (pConfig != nullptr && pConfig->inputCfg.samplingRate > SB_MODE_MAX) {
        ALOGE("Invalid SoundBooster mode %u", pConfig->inputCfg.samplingRate);
    }

    pContext->mode = mode;
    if (pContext->dsp == nullptr) {
        pContext->dsp = new SoundBooster(mode);
        ALOGV("%s: allocated SoundBooster mode %u", __func__, mode);
    }
    pContext->dsp->setSessionId(pContext->sessionId);
    pContext->rotation = 0;
    pContext->spkMode = 0;
    // EFFECT_CMD_ENABLE transitions state to active.
    pContext->state = EFFECT_STATE_INITIALIZED;
    return 0;
}

int Effect_setConfig(EffectContext* pContext, effect_config_t* pConfig) {
    CHECK_ARG(pContext != nullptr && pConfig != nullptr && pContext->dsp != nullptr);
    CHECK_ARG(pConfig->inputCfg.samplingRate == pConfig->outputCfg.samplingRate);
    CHECK_ARG(pConfig->inputCfg.format == pConfig->outputCfg.format);

    pContext->dsp->init(pConfig->inputCfg.samplingRate,
                        static_cast<audio_format_t>(pConfig->inputCfg.format));
    return 0;
}

int Effect_setDevice(EffectContext* pContext, uint32_t device) {
    if (pContext->state == EFFECT_STATE_UNINITIALIZED) {
        ALOGE("%s: not initialized", __func__);
        return -ENODATA;
    }

    const bool wasEnabled = pContext->enabled;
    pContext->curDevice = device;
    pContext->enabled = pContext->dsp != nullptr && SoundBooster::isEnabledDevice(device);

    if (!wasEnabled && pContext->enabled) {
        ALOGV("%s: reset sessionId %d", __func__, pContext->sessionId);
        pContext->dsp->clear();
    }
    return 0;
}

int Effect_setVolume(EffectContext* pContext, const uint32_t* pVolume) {
    if (pContext->state == EFFECT_STATE_UNINITIALIZED) {
        ALOGE("%s: not initialized", __func__);
        return -ENODATA;
    }

    const float left = static_cast<float>(pVolume[0]) * SB_VOLUME_SCALE;
    const float right = static_cast<float>(pVolume[1]) * SB_VOLUME_SCALE;
    if (pContext->leftVolume == left && pContext->rightVolume == right) {
        return 0;
    }

    pContext->leftVolume = left;
    pContext->rightVolume = right;
    const float maxVolume = left > right ? left : right;
    pContext->logVolume = logf(maxVolume + SB_VOLUME_EPSILON) * SB_LINEAR_TO_DB_SCALE;
    return 0;
}

int Effect_setParameter(EffectContext* pContext, effect_param_t* pParam) {
    if (pParam->psize - sizeof(int32_t) >= 5 || pParam->vsize <= 3) {
        return -EINVAL;
    }

    const int32_t* pData = reinterpret_cast<const int32_t*>(pParam->data);
    const int32_t value = pData[1];

    switch (pData[0]) {
        case SB_PARAM_FLAT_MOTION:
            pContext->flatMotion = value;
            if (pContext->dsp != nullptr) {
                pContext->dsp->setFlatMotion(value);
            }
            break;
        case SB_PARAM_ROTATION:
            if (pContext->rotation != value) {
                pContext->rotation = value;
                if (pContext->dsp != nullptr) {
                    pContext->dsp->setRotation(value);
                }
            }
            break;
        case SB_PARAM_READ_PARAM:
            if (pContext->dsp != nullptr) {
                pContext->dsp->readParam();
            }
            break;
        default:
            break;
    }
    return 0;
}

}  // namespace

/* Effect Library Interface Implementation */

extern "C" int EffectCreate(const effect_uuid_t* uuid, int32_t sessionId,
                            int32_t /* ioId __unused */, effect_handle_t* pHandle) {
    if (pHandle == nullptr || uuid == nullptr) {
        ALOGE("%s: called with NULL pointer", __func__);
        return -EINVAL;
    }

    if (memcmp(uuid, &gSoundBoosterDescriptor.uuid, sizeof(effect_uuid_t)) != 0) {
        ALOGE("%s: invalid UUID", __func__);
        return -EINVAL;
    }

    EffectContext* pContext = new (std::nothrow) EffectContext;
    if (pContext == nullptr) {
        return -ENOMEM;
    }
    pContext->itfe = &gSoundBoosterEffectInterface;
    pContext->sessionId = sessionId;
    pContext->state = EFFECT_STATE_INITIALIZED;

    *pHandle = reinterpret_cast<effect_handle_t>(pContext);
    return 0;
}

extern "C" int EffectRelease(effect_handle_t handle) {
    EffectContext* pContext = reinterpret_cast<EffectContext*>(handle);
    if (pContext == nullptr) {
        ALOGE("%s: called with NULL pointer", __func__);
        return -EINVAL;
    }

    pContext->state = EFFECT_STATE_UNINITIALIZED;
    delete pContext->dsp;
    delete pContext;
    return 0;
}

extern "C" int EffectGetDescriptor(const effect_uuid_t* uuid, effect_descriptor_t* pDescriptor) {
    if (pDescriptor == nullptr || uuid == nullptr) {
        ALOGE("%s: called with NULL pointer", __func__);
        return -EINVAL;
    }

    if (memcmp(uuid, &gSoundBoosterDescriptor.uuid, sizeof(effect_uuid_t)) != 0) {
        return -EINVAL;
    }

    *pDescriptor = gSoundBoosterDescriptor;
    return 0;
}

}  // namespace android

extern "C" {
/* Effect Control Interface Implementation: Process */
int Effect_process(effect_handle_t self, audio_buffer_t* inBuffer, audio_buffer_t* outBuffer) {
    EffectContext* pContext = reinterpret_cast<EffectContext*>(self);

    if (pContext == nullptr) {
        ALOGE("%s: pContext == NULL", __func__);
        return -EINVAL;
    }
    if (pContext->state != EFFECT_STATE_ACTIVE) {
        return -ENODATA;
    }
    if (pContext->dsp == nullptr) {
        ALOGE("%s: DSP not allocated", __func__);
        return -EINVAL;
    }
    if (inBuffer == nullptr || inBuffer->raw == nullptr || outBuffer == nullptr ||
        outBuffer->raw == nullptr || inBuffer->frameCount == 0 ||
        inBuffer->frameCount != outBuffer->frameCount) {
        ALOGE("%s: NULL input pointer or wrong frame count", __func__);
        return -EINVAL;
    }

    if (pContext->enabled) {
        pContext->dsp->process(inBuffer->raw, inBuffer->raw, inBuffer->frameCount,
                               pContext->logVolume);
    }

    if (inBuffer->raw != outBuffer->raw) {
        memcpy(outBuffer->raw, inBuffer->raw, inBuffer->frameCount * SB_BYTES_PER_FRAME);
    }
    return 0;
}

/* Effect Control Interface Implementation: Command */
int Effect_command(effect_handle_t self, uint32_t cmdCode, uint32_t cmdSize, void* pCmdData,
                   uint32_t* replySize, void* pReplyData) {
    EffectContext* pContext = reinterpret_cast<EffectContext*>(self);

    if (pContext == nullptr || pContext->state == EFFECT_STATE_UNINITIALIZED) {
        ALOGE("%s: invalid context", __func__);
        return -EINVAL;
    }

    switch (cmdCode) {
        case EFFECT_CMD_INIT:
            CHECK_ARG(pReplyData != nullptr && replySize != nullptr && *replySize == sizeof(int));
            android::Effect_init(pContext, reinterpret_cast<effect_config_t*>(pCmdData));
            *reinterpret_cast<int*>(pReplyData) = 0;
            break;

        case EFFECT_CMD_SET_CONFIG:
            CHECK_ARG(pCmdData != nullptr && cmdSize == sizeof(effect_config_t) &&
                      pReplyData != nullptr && replySize != nullptr && *replySize == sizeof(int));
            *reinterpret_cast<int*>(pReplyData) = android::Effect_setConfig(
                    pContext, reinterpret_cast<effect_config_t*>(pCmdData));
            break;

        case EFFECT_CMD_GET_CONFIG:
            CHECK_ARG(pReplyData != nullptr && replySize != nullptr &&
                      *replySize == sizeof(effect_config_t));
            break;

        case EFFECT_CMD_RESET:
            ALOGV("%s: EFFECT_CMD_RESET sessionId %d", __func__, pContext->sessionId);
            if (pContext->dsp != nullptr) {
                pContext->dsp->clear();
            }
            break;

        case EFFECT_CMD_ENABLE:
            CHECK_ARG(pReplyData != nullptr && replySize != nullptr && *replySize == sizeof(int));
            if (pContext->state != EFFECT_STATE_INITIALIZED) {
                return -ENODATA;
            }
            pContext->state = EFFECT_STATE_ACTIVE;
            *reinterpret_cast<int*>(pReplyData) = 0;
            break;

        case EFFECT_CMD_DISABLE:
            CHECK_ARG(pReplyData != nullptr && replySize != nullptr && *replySize == sizeof(int));
            if (pContext->state != EFFECT_STATE_ACTIVE) {
                return -ENODATA;
            }
            pContext->state = EFFECT_STATE_INITIALIZED;
            *reinterpret_cast<int*>(pReplyData) = 0;
            break;

        case EFFECT_CMD_SET_PARAM:
            CHECK_ARG(pCmdData != nullptr && cmdSize > SB_MIN_SET_PARAM_CMD_SIZE &&
                      pReplyData != nullptr && replySize != nullptr && *replySize == sizeof(int));
            *reinterpret_cast<int*>(pReplyData) = android::Effect_setParameter(
                    pContext, reinterpret_cast<effect_param_t*>(pCmdData));
            break;

        case EFFECT_CMD_SET_DEVICE:
            CHECK_ARG(pCmdData != nullptr && cmdSize == sizeof(uint32_t));
            return android::Effect_setDevice(pContext, *reinterpret_cast<uint32_t*>(pCmdData));

        case EFFECT_CMD_SET_VOLUME:
            CHECK_ARG(pCmdData != nullptr && cmdSize == 2 * sizeof(uint32_t));
            return android::Effect_setVolume(pContext, reinterpret_cast<uint32_t*>(pCmdData));

        case EFFECT_CMD_GET_PARAM:
        case EFFECT_CMD_SET_AUDIO_MODE:
            break;

        case EFFECT_CMD_DUMP:
            ALOGI("device: %u, spkMode: %d, rotation: %d, flatMotion: %d", pContext->curDevice,
                  pContext->spkMode, pContext->rotation, pContext->flatMotion);
            break;

        default:
            ALOGW("%s: invalid command %u", __func__, cmdCode);
            return -EINVAL;
    }

    return 0;
} /* end Effect_command */

/* Effect Control Interface Implementation: get_descriptor */
int Effect_getDescriptor(effect_handle_t self, effect_descriptor_t* pDescriptor) {
    if (self == nullptr || pDescriptor == nullptr) {
        ALOGE("%s: invalid param", __func__);
        return -EINVAL;
    }

    *pDescriptor = android::gSoundBoosterDescriptor;
    return 0;
} /* end Effect_getDescriptor */

// effect_handle_t interface implementation for effect
const struct effect_interface_s gSoundBoosterEffectInterface = {
        Effect_process,
        Effect_command,
        Effect_getDescriptor,
        nullptr,
}; /* end gSoundBoosterEffectInterface */

// This is the only symbol that needs to be exported
__attribute__((visibility("default"))) audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = {
        .tag = AUDIO_EFFECT_LIBRARY_TAG,
        .version = EFFECT_LIBRARY_API_VERSION,
        .name = "SoundBooster Plus Library",
        .implementor = "Samsung",
        .create_effect = android::EffectCreate,
        .release_effect = android::EffectRelease,
        .get_descriptor = android::EffectGetDescriptor,
};
}
