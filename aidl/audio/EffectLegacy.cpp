/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "AHAL_EffectLegacy"

#include "EffectLegacy.h"

#include <cstring>
#include <map>
#include <mutex>

#include <Utils.h>
#include <aidl/android/hardware/audio/effect/DefaultExtension.h>
#include <android-base/logging.h>
#include <audio_utils/primitives.h>
#include <media/AidlConversionCppNdk.h>
#include <media/AidlConversionEffect.h>

using aidl::android::hardware::audio::common::getChannelCount;
using aidl::android::media::audio::common::AudioDeviceDescription;
using aidl::android::media::audio::common::AudioUuid;

namespace aidl::android::hardware::audio::effect {

namespace {

std::mutex sLibrariesLock;
std::map<AudioUuid, const audio_effect_library_t*> sLibraries;

const audio_effect_library_t* findLibrary(const AudioUuid& uuid) {
    std::lock_guard l(sLibrariesLock);
    auto it = sLibraries.find(uuid);
    return it != sLibraries.end() ? it->second : nullptr;
}

uint32_t toFixed824(float volume) {
    return static_cast<uint32_t>(std::clamp(volume, 0.0f, 1.0f) * (1 << 24));
}

}  // namespace

void registerLegacyEffectLibrary(const AudioUuid& impl, const audio_effect_library_t* library) {
    std::lock_guard l(sLibrariesLock);
    sLibraries[impl] = library;
}

binder_exception_t queryLegacyEffect(const AudioUuid* uuid, Descriptor* descriptor) {
    const audio_effect_library_t* library = uuid ? findLibrary(*uuid) : nullptr;
    if (library == nullptr || descriptor == nullptr) {
        return EX_ILLEGAL_ARGUMENT;
    }
    auto legacyUuid = ::aidl::android::aidl2legacy_AudioUuid_audio_uuid_t(*uuid);
    if (!legacyUuid.ok()) {
        return EX_ILLEGAL_ARGUMENT;
    }
    effect_descriptor_t legacyDescriptor;
    if (int ret = library->get_descriptor(&legacyUuid.value(), &legacyDescriptor); ret != 0) {
        LOG(ERROR) << __func__ << ": get_descriptor of " << library->name << " failed: " << ret;
        return EX_ILLEGAL_ARGUMENT;
    }
    auto aidlDescriptor =
            ::aidl::android::legacy2aidl_effect_descriptor_Descriptor(legacyDescriptor);
    if (!aidlDescriptor.ok()) {
        LOG(ERROR) << __func__ << ": unable to convert the descriptor of " << legacyDescriptor.name;
        return EX_ILLEGAL_ARGUMENT;
    }
    *descriptor = aidlDescriptor.value();
    return EX_NONE;
}

binder_exception_t createLegacyEffect(const AudioUuid* uuid, std::shared_ptr<IEffect>* instance) {
    const audio_effect_library_t* library = uuid ? findLibrary(*uuid) : nullptr;
    if (library == nullptr || instance == nullptr) {
        return EX_ILLEGAL_ARGUMENT;
    }
    Descriptor descriptor;
    if (binder_exception_t ret = queryLegacyEffect(uuid, &descriptor); ret != EX_NONE) {
        return ret;
    }
    auto legacyUuid = ::aidl::android::aidl2legacy_AudioUuid_audio_uuid_t(*uuid);
    *instance = ndk::SharedRefBase::make<EffectLegacy>(library, legacyUuid.value(), descriptor);
    LOG(DEBUG) << __func__ << ": created " << descriptor.common.name;
    return EX_NONE;
}

binder_exception_t destroyLegacyEffect(const std::shared_ptr<IEffect>& instance) {
    // Same as for the AIDL effect libraries
    return destroyEffect(instance);
}

EffectLegacy::EffectLegacy(const audio_effect_library_t* library, const effect_uuid_t& uuid,
                           const Descriptor& descriptor)
    : mLibrary(library),
      mUuid(uuid),
      mDescriptor(descriptor),
      mIsPreProcessing(descriptor.common.flags.type == Flags::Type::PRE_PROC) {}

EffectLegacy::~EffectLegacy() {
    cleanUp();
}

ndk::ScopedAStatus EffectLegacy::getDescriptor(Descriptor* desc) {
    *desc = mDescriptor;
    return ndk::ScopedAStatus::ok();
}

int EffectLegacy::legacyCommand(uint32_t code, uint32_t size, void* data, uint32_t* replySize,
                                void* replyData) {
    if (mHandle == nullptr) {
        return -ENODEV;
    }
    return (*mHandle)->command(mHandle, code, size, data, replySize, replyData);
}

int EffectLegacy::legacyCommandWithStatus(uint32_t code, uint32_t size, void* data) {
    int reply = 0;
    uint32_t replySize = sizeof(reply);
    if (int ret = legacyCommand(code, size, data, &replySize, &reply); ret != 0) {
        return ret;
    }
    return reply;
}

bool EffectLegacy::setConfig(const Parameter::Common& common) {
    auto inputChannels = ::aidl::android::aidl2legacy_AudioChannelLayout_audio_channel_mask_t(
            common.input.base.channelMask, mIsPreProcessing);
    auto outputChannels = ::aidl::android::aidl2legacy_AudioChannelLayout_audio_channel_mask_t(
            common.output.base.channelMask, mIsPreProcessing);
    if (!inputChannels.ok() || !outputChannels.ok()) {
        LOG(ERROR) << __func__ << ": unsupported channel masks " << common.toString();
        return false;
    }

    effect_config_t config = {};
    config.inputCfg.samplingRate = common.input.base.sampleRate;
    config.inputCfg.channels = inputChannels.value();
    config.inputCfg.accessMode = EFFECT_BUFFER_ACCESS_READ;
    config.inputCfg.mask = EFFECT_CONFIG_ALL;
    config.outputCfg.samplingRate = common.output.base.sampleRate;
    config.outputCfg.channels = outputChannels.value();
    config.outputCfg.accessMode = EFFECT_BUFFER_ACCESS_WRITE;
    config.outputCfg.mask = EFFECT_CONFIG_ALL;

    // Like AudioFlinger, prefer float samples and fall back to 16 bit ones
    for (audio_format_t format : {AUDIO_FORMAT_PCM_FLOAT, AUDIO_FORMAT_PCM_16_BIT}) {
        config.inputCfg.format = format;
        config.outputCfg.format = format;
        if (int ret = legacyCommandWithStatus(EFFECT_CMD_SET_CONFIG, sizeof(config), &config);
            ret == 0) {
            mUse16Bit = format == AUDIO_FORMAT_PCM_16_BIT;
            mInputChannelCount = getChannelCount(common.input.base.channelMask);
            mOutputChannelCount = getChannelCount(common.output.base.channelMask);
            mInput16.resize(common.input.frameCount * mInputChannelCount);
            mOutput16.resize(common.output.frameCount * mOutputChannelCount);
            LOG(DEBUG) << __func__ << ": " << getEffectName() << " configured with "
                       << (mUse16Bit ? "16 bit" : "float") << " samples";
            return true;
        }
    }
    LOG(ERROR) << __func__ << ": " << getEffectName() << " rejected " << common.toString();
    return false;
}

std::shared_ptr<EffectContext> EffectLegacy::createContext(const Parameter::Common& common) {
    if (mImplContext) {
        LOG(DEBUG) << __func__ << ": context already exists";
        return mImplContext;
    }

    if (int ret = mLibrary->create_effect(&mUuid, common.session, common.ioHandle, &mHandle);
        ret != 0 || mHandle == nullptr) {
        LOG(ERROR) << __func__ << ": create_effect of " << getEffectName() << " failed: " << ret;
        mHandle = nullptr;
        return nullptr;
    }
    if (int ret = legacyCommandWithStatus(EFFECT_CMD_INIT, 0, nullptr); ret != 0) {
        LOG(ERROR) << __func__ << ": EFFECT_CMD_INIT of " << getEffectName() << " failed: " << ret;
        releaseContext();
        return nullptr;
    }
    if (!setConfig(common)) {
        releaseContext();
        return nullptr;
    }

    mImplContext = std::make_shared<EffectContext>(1 /* statusMqDepth */, common);
    return mImplContext;
}

RetCode EffectLegacy::releaseContext() {
    if (mHandle != nullptr) {
        mLibrary->release_effect(mHandle);
        mHandle = nullptr;
    }
    mImplContext.reset();
    return RetCode::SUCCESS;
}

ndk::ScopedAStatus EffectLegacy::setParameterCommon(const Parameter& param) {
    RETURN_IF_ASTATUS_NOT_OK(EffectImpl::setParameterCommon(param), "setParameterCommonFailed");

    switch (param.getTag()) {
        case Parameter::common:
            RETURN_IF(!setConfig(param.get<Parameter::common>()), EX_ILLEGAL_ARGUMENT,
                      "setConfigFailed");
            break;
        case Parameter::deviceDescription: {
            uint32_t outputDevices = AUDIO_DEVICE_NONE, inputDevices = AUDIO_DEVICE_NONE;
            for (const AudioDeviceDescription& device : param.get<Parameter::deviceDescription>()) {
                auto type =
                        ::aidl::android::aidl2legacy_AudioDeviceDescription_audio_devices_t(device);
                RETURN_IF(!type.ok(), EX_ILLEGAL_ARGUMENT, "invalidDevice");
                if (audio_is_input_device(type.value())) {
                    inputDevices |= type.value();
                } else {
                    outputDevices |= type.value();
                }
            }
            if (outputDevices != AUDIO_DEVICE_NONE) {
                legacyCommand(EFFECT_CMD_SET_DEVICE, sizeof(outputDevices), &outputDevices);
            }
            if (inputDevices != AUDIO_DEVICE_NONE) {
                legacyCommand(EFFECT_CMD_SET_INPUT_DEVICE, sizeof(inputDevices), &inputDevices);
            }
            break;
        }
        case Parameter::mode: {
            auto mode = ::aidl::android::aidl2legacy_AudioMode_audio_mode_t(
                    param.get<Parameter::mode>());
            RETURN_IF(!mode.ok(), EX_ILLEGAL_ARGUMENT, "invalidMode");
            uint32_t legacyMode = mode.value();
            legacyCommand(EFFECT_CMD_SET_AUDIO_MODE, sizeof(legacyMode), &legacyMode);
            break;
        }
        case Parameter::source: {
            auto source = ::aidl::android::aidl2legacy_AudioSource_audio_source_t(
                    param.get<Parameter::source>());
            RETURN_IF(!source.ok(), EX_ILLEGAL_ARGUMENT, "invalidSource");
            uint32_t legacySource = source.value();
            legacyCommand(EFFECT_CMD_SET_AUDIO_SOURCE, sizeof(legacySource), &legacySource);
            break;
        }
        case Parameter::volumeStereo: {
            const auto& volume = param.get<Parameter::volumeStereo>();
            uint32_t volumes[2] = {toFixed824(volume.left), toFixed824(volume.right)};
            uint32_t replyVolumes[2] = {volumes[0], volumes[1]};
            uint32_t replySize = sizeof(replyVolumes);
            legacyCommand(EFFECT_CMD_SET_VOLUME, sizeof(volumes), volumes, &replySize,
                          replyVolumes);
            break;
        }
        default:
            break;
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus EffectLegacy::setParameterSpecific(const Parameter::Specific& specific) {
    RETURN_IF(specific.getTag() != Parameter::Specific::vendorEffect, EX_ILLEGAL_ARGUMENT,
              "unsupportedParameterTag");

    std::optional<DefaultExtension> extension;
    specific.get<Parameter::Specific::vendorEffect>().extension.getParcelable(&extension);
    RETURN_IF(!extension.has_value() || extension->bytes.size() < sizeof(effect_param_t),
              EX_ILLEGAL_ARGUMENT, "invalidExtension");

    std::vector<uint8_t> bytes = extension->bytes;
    int ret = legacyCommandWithStatus(EFFECT_CMD_SET_PARAM, bytes.size(), bytes.data());
    RETURN_IF(ret != 0, EX_ILLEGAL_ARGUMENT, "setParamFailed");
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus EffectLegacy::getParameterSpecific(const Parameter::Id& id,
                                                      Parameter::Specific* specific) {
    RETURN_IF(id.getTag() != Parameter::Id::vendorEffectTag, EX_ILLEGAL_ARGUMENT,
              "unsupportedParameterTag");

    std::optional<DefaultExtension> extension;
    id.get<Parameter::Id::vendorEffectTag>().extension.getParcelable(&extension);
    RETURN_IF(!extension.has_value() || extension->bytes.size() < sizeof(effect_param_t),
              EX_ILLEGAL_ARGUMENT, "invalidExtension");

    // The request carries the parameter and the size of the value to return
    const auto* request = reinterpret_cast<const effect_param_t*>(extension->bytes.data());
    const uint32_t paddedParameterSize = ((request->psize - 1) / sizeof(int) + 1) * sizeof(int);
    const uint32_t commandSize = sizeof(effect_param_t) + request->psize;
    uint32_t replySize = sizeof(effect_param_t) + paddedParameterSize + request->vsize;
    RETURN_IF(request->psize == 0 || extension->bytes.size() < commandSize, EX_ILLEGAL_ARGUMENT,
              "invalidParameter");

    std::vector<uint8_t> commandData(extension->bytes.begin(),
                                     extension->bytes.begin() + commandSize);
    std::vector<uint8_t> reply(replySize);
    int ret = legacyCommand(EFFECT_CMD_GET_PARAM, commandSize, commandData.data(), &replySize,
                            reply.data());
    RETURN_IF(ret != 0 || replySize < sizeof(effect_param_t), EX_ILLEGAL_ARGUMENT,
              "getParamFailed");
    reply.resize(replySize);

    DefaultExtension replyExtension;
    replyExtension.bytes = std::move(reply);
    VendorExtension vendorExtension;
    vendorExtension.extension.setParcelable(replyExtension);
    specific->set<Parameter::Specific::vendorEffect>(vendorExtension);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus EffectLegacy::commandImpl(CommandId id) {
    switch (id) {
        case CommandId::START:
            if (int ret = legacyCommandWithStatus(EFFECT_CMD_ENABLE, 0, nullptr); ret != 0) {
                LOG(ERROR) << __func__ << ": EFFECT_CMD_ENABLE failed: " << ret;
            }
            break;
        case CommandId::STOP:
            if (int ret = legacyCommandWithStatus(EFFECT_CMD_DISABLE, 0, nullptr); ret != 0) {
                LOG(ERROR) << __func__ << ": EFFECT_CMD_DISABLE failed: " << ret;
            }
            break;
        case CommandId::RESET:
            legacyCommand(EFFECT_CMD_RESET, 0, nullptr);
            break;
        default:
            break;
    }
    return EffectImpl::commandImpl(id);
}

IEffect::Status EffectLegacy::effectProcessImpl(float* in, float* out, int samples) {
    IEffect::Status status = {EX_NULL_POINTER, 0, 0};
    RETURN_VALUE_IF(mHandle == nullptr || mInputChannelCount == 0, status, "nullHandle");

    const size_t frames = samples / mInputChannelCount;
    const size_t outputSamples = frames * mOutputChannelCount;
    audio_buffer_t inBuffer = {}, outBuffer = {};
    inBuffer.frameCount = outBuffer.frameCount = frames;
    int ret;
    if (!mUse16Bit) {
        inBuffer.f32 = in;
        outBuffer.f32 = out;
        ret = (*mHandle)->process(mHandle, &inBuffer, &outBuffer);
    } else {
        if (mInput16.size() < static_cast<size_t>(samples)) mInput16.resize(samples);
        if (mOutput16.size() < outputSamples) mOutput16.resize(outputSamples);
        memcpy_to_i16_from_float(mInput16.data(), in, samples);
        inBuffer.s16 = mInput16.data();
        outBuffer.s16 = mOutput16.data();
        ret = (*mHandle)->process(mHandle, &inBuffer, &outBuffer);
        if (ret == 0) {
            memcpy_to_float_from_i16(out, mOutput16.data(), outputSamples);
        }
    }
    // -ENODATA means the effect is done processing, after being disabled
    if (ret != 0 && ret != -ENODATA) {
        LOG(ERROR) << __func__ << ": process of " << getEffectName() << " failed: " << ret;
    }
    if (ret != 0 && in != out) {
        memcpy(out, in, std::min(static_cast<size_t>(samples), outputSamples) * sizeof(float));
    }
    return {STATUS_OK, samples, static_cast<int32_t>(outputSamples)};
}

}  // namespace aidl::android::hardware::audio::effect
