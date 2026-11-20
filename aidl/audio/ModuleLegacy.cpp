/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "AHAL_ModuleLegacy"

#include "ModuleLegacy.h"

#include <algorithm>

#include <Utils.h>
#include <android-base/logging.h>
#include <error/expected_utils.h>
#include <media/AidlConversionCppNdk.h>

#include "LegacyUtils.h"
#include "StreamLegacy.h"
#include "TelephonyLegacy.h"

using aidl::android::hardware::audio::common::isBitPositionFlagSet;
using aidl::android::hardware::audio::common::isValidAudioMode;
using aidl::android::hardware::audio::common::SinkMetadata;
using aidl::android::hardware::audio::common::SourceMetadata;
using aidl::android::media::audio::common::AudioDevice;
using aidl::android::media::audio::common::AudioFormatDescription;
using aidl::android::media::audio::common::AudioFormatType;
using aidl::android::media::audio::common::AudioInputFlags;
using aidl::android::media::audio::common::AudioIoFlags;
using aidl::android::media::audio::common::AudioMode;
using aidl::android::media::audio::common::AudioOffloadInfo;
using aidl::android::media::audio::common::AudioOutputFlags;
using aidl::android::media::audio::common::AudioPort;
using aidl::android::media::audio::common::AudioPortConfig;
using aidl::android::media::audio::common::AudioPortExt;
using aidl::android::media::audio::common::AudioPortMixExtUseCase;
using aidl::android::media::audio::common::AudioSource;
using aidl::android::media::audio::common::MicrophoneInfo;
using android::AudioParameter;
using android::String8;

namespace aidl::android::hardware::audio::core {

// Evaluates to the value of a ConversionResult, or returns EX_ILLEGAL_ARGUMENT on failure
#define VALUE_OR_RETURN_ILLEGAL_ARGUMENT(result)                               \
    ({                                                                         \
        auto _tmp = (result);                                                  \
        if (!_tmp.ok()) {                                                      \
            LOG(ERROR) << __func__ << ": unable to convert " #result;          \
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT); \
        }                                                                      \
        std::move(_tmp.value());                                               \
    })

namespace {

// Period sizes of the legacy HAL, used as the stream buffer sizes, so that the mixer threads of
// AudioFlinger keep running with the same burst sizes as with the legacy HAL
constexpr int32_t kLowLatencyMs = 5;
constexpr int32_t kNormalLatencyMs = 20;

audio_hw_device_t* openLegacyDevice() {
    const hw_module_t* module;
    if (int ret = hw_get_module_by_class(AUDIO_HARDWARE_MODULE_ID, AUDIO_HARDWARE_MODULE_ID_PRIMARY,
                                         &module);
        ret != 0) {
        LOG(ERROR) << __func__ << ": unable to load the primary audio HAL: " << ret;
        return nullptr;
    }
    audio_hw_device_t* device;
    if (int ret = audio_hw_device_open(module, &device); ret != 0) {
        LOG(ERROR) << __func__ << ": unable to open the primary audio HAL: " << ret;
        return nullptr;
    }
    if (device->common.version < AUDIO_DEVICE_API_VERSION_2_0) {
        LOG(ERROR) << __func__ << ": unsupported device API version " << std::hex
                   << device->common.version;
        audio_hw_device_close(device);
        return nullptr;
    }
    if (int ret = device->init_check(device); ret != 0) {
        LOG(ERROR) << __func__ << ": init_check failed: " << ret;
        audio_hw_device_close(device);
        return nullptr;
    }
    LOG(INFO) << __func__ << ": opened " << module->name << ", device API version " << std::hex
              << device->common.version;
    return device;
}

}  // namespace

// static
std::shared_ptr<ModuleLegacy> ModuleLegacy::create(std::unique_ptr<Configuration>&& config) {
    audio_hw_device_t* device = openLegacyDevice();
    if (device == nullptr) {
        return nullptr;
    }
    return ndk::SharedRefBase::make<ModuleLegacy>(std::move(config), device);
}

ModuleLegacy::ModuleLegacy(std::unique_ptr<Configuration>&& config, audio_hw_device_t* device)
    : Module(Type::DEFAULT, std::move(config)), mDevice(device) {}

ModuleLegacy::~ModuleLegacy() {
    audio_hw_device_close(mDevice);
}

ndk::ScopedAStatus ModuleLegacy::getTelephony(std::shared_ptr<ITelephony>* _aidl_return) {
    if (!mTelephony) {
        mTelephony = ndk::SharedRefBase::make<TelephonyLegacy>(mDevice);
    }
    *_aidl_return = mTelephony.getInstance();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleLegacy::getBluetooth(std::shared_ptr<IBluetooth>* _aidl_return) {
    if (!mBluetooth) {
        mBluetooth = ndk::SharedRefBase::make<BluetoothLegacy>(mDevice);
    }
    *_aidl_return = mBluetooth.getInstance();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleLegacy::setMicMute(bool in_mute) {
    if (int ret = mDevice->set_mic_mute(mDevice, in_mute); ret != 0) {
        LOG(ERROR) << __func__ << ": set_mic_mute(" << in_mute << ") failed: " << ret;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return Module::setMicMute(in_mute);
}

ndk::ScopedAStatus ModuleLegacy::updateAudioMode(AudioMode in_mode) {
    RETURN_STATUS_IF_ERROR(Module::updateAudioMode(in_mode));
    auto mode = ::aidl::android::aidl2legacy_AudioMode_audio_mode_t(in_mode);
    if (!mode.ok()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (int ret = mDevice->set_mode(mDevice, mode.value()); ret != 0) {
        LOG(ERROR) << __func__ << ": set_mode(" << mode.value() << ") failed: " << ret;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleLegacy::updateScreenRotation(ScreenRotation in_rotation) {
    RETURN_STATUS_IF_ERROR(Module::updateScreenRotation(in_rotation));
    int degrees = 0;
    switch (in_rotation) {
        case ScreenRotation::DEG_0:
            degrees = 0;
            break;
        case ScreenRotation::DEG_90:
            degrees = 90;
            break;
        case ScreenRotation::DEG_180:
            degrees = 180;
            break;
        case ScreenRotation::DEG_270:
            degrees = 270;
            break;
    }
    AudioParameter parameters;
    parameters.addInt(String8(AudioParameter::keyScreenRotation), degrees);
    legacy::setParameters(mDevice, parameters);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleLegacy::updateScreenState(bool in_isTurnedOn) {
    RETURN_STATUS_IF_ERROR(Module::updateScreenState(in_isTurnedOn));
    AudioParameter parameters;
    parameters.add(String8(AudioParameter::keyScreenState),
                   String8(in_isTurnedOn ? AudioParameter::valueOn : AudioParameter::valueOff));
    legacy::setParameters(mDevice, parameters);
    return ndk::ScopedAStatus::ok();
}

const AudioPortConfig* ModuleLegacy::findMixPortConfig(int32_t handle) {
    const auto& portConfigs = getConfig().portConfigs;
    auto it = std::find_if(portConfigs.begin(), portConfigs.end(), [&](const auto& config) {
        return config.ext.getTag() == AudioPortExt::Tag::mix &&
               config.ext.template get<AudioPortExt::Tag::mix>().handle == handle;
    });
    return it != portConfigs.end() ? &(*it) : nullptr;
}

ndk::ScopedAStatus ModuleLegacy::checkStreamContext(const StreamContext& context) {
    // Neither are used by the audio policy configuration of the legacy HAL
    if (context.isMmap()) {
        LOG(ERROR) << __func__ << ": MMAP streams are not supported";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (context.getFlags().getTag() == AudioIoFlags::Tag::output &&
        isBitPositionFlagSet(context.getFlags().get<AudioIoFlags::Tag::output>(),
                             AudioOutputFlags::COMPRESS_OFFLOAD)) {
        LOG(ERROR) << __func__ << ": offloaded streams are not supported";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleLegacy::createInputStream(StreamContext&& context,
                                                   const SinkMetadata& sinkMetadata,
                                                   const std::vector<MicrophoneInfo>& microphones,
                                                   std::shared_ptr<StreamIn>* result) {
    RETURN_STATUS_IF_ERROR(checkStreamContext(context));

    const AudioPortConfig* portConfig = findMixPortConfig(context.getMixPortHandle());
    if (portConfig == nullptr) {
        LOG(ERROR) << __func__ << ": no port config for handle " << context.getMixPortHandle();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    audio_devices_t devices;
    std::string address;
    if (!legacy::toLegacyDevices(findConnectedDevices(portConfig->id), &devices, &address)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    // Like AudioFlinger, take the source from the use case of the mix port
    AudioSource aidlSource = AudioSource::DEFAULT;
    if (const auto& useCase = portConfig->ext.get<AudioPortExt::Tag::mix>().usecase;
        useCase.getTag() == AudioPortMixExtUseCase::Tag::source) {
        aidlSource = useCase.get<AudioPortMixExtUseCase::Tag::source>();
    } else if (!sinkMetadata.tracks.empty()) {
        aidlSource = sinkMetadata.tracks[0].source;
    }
    audio_source_t source = VALUE_OR_RETURN_ILLEGAL_ARGUMENT(
            ::aidl::android::aidl2legacy_AudioSource_audio_source_t(aidlSource));

    audio_config_t config = AUDIO_CONFIG_INITIALIZER;
    config.sample_rate = context.getSampleRate();
    config.channel_mask = VALUE_OR_RETURN_ILLEGAL_ARGUMENT(
            ::aidl::android::aidl2legacy_AudioChannelLayout_audio_channel_mask_t(
                    context.getChannelLayout(), true /*isInput*/));
    config.format = VALUE_OR_RETURN_ILLEGAL_ARGUMENT(
            ::aidl::android::aidl2legacy_AudioFormatDescription_audio_format_t(
                    context.getFormat()));
    audio_input_flags_t flags = VALUE_OR_RETURN_ILLEGAL_ARGUMENT(
            ::aidl::android::aidl2legacy_int32_t_audio_input_flags_t_mask(
                    context.getFlags().get<AudioIoFlags::Tag::input>()));

    audio_stream_in_t* in = nullptr;
    if (int ret = mDevice->open_input_stream(mDevice, context.getMixPortHandle(), devices, &config,
                                             &in, flags, address.c_str(), source);
        ret != 0 || in == nullptr) {
        LOG(ERROR) << __func__ << ": open_input_stream failed: " << ret;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    LOG(DEBUG) << __func__ << ": opened handle " << context.getMixPortHandle() << ", devices "
               << std::hex << devices << ", source " << std::dec << source;

    return createStreamInstance<StreamInLegacy>(result, std::move(context), sinkMetadata,
                                                microphones, mDevice, in, source);
}

ndk::ScopedAStatus ModuleLegacy::createOutputStream(
        StreamContext&& context, const SourceMetadata& sourceMetadata,
        const std::optional<AudioOffloadInfo>& offloadInfo, std::shared_ptr<StreamOut>* result) {
    RETURN_STATUS_IF_ERROR(checkStreamContext(context));

    const AudioPortConfig* portConfig = findMixPortConfig(context.getMixPortHandle());
    if (portConfig == nullptr) {
        LOG(ERROR) << __func__ << ": no port config for handle " << context.getMixPortHandle();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    audio_devices_t devices;
    std::string address;
    if (!legacy::toLegacyDevices(findConnectedDevices(portConfig->id), &devices, &address)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    audio_config_t config = AUDIO_CONFIG_INITIALIZER;
    config.sample_rate = context.getSampleRate();
    config.channel_mask = VALUE_OR_RETURN_ILLEGAL_ARGUMENT(
            ::aidl::android::aidl2legacy_AudioChannelLayout_audio_channel_mask_t(
                    context.getChannelLayout(), false /*isInput*/));
    config.format = VALUE_OR_RETURN_ILLEGAL_ARGUMENT(
            ::aidl::android::aidl2legacy_AudioFormatDescription_audio_format_t(
                    context.getFormat()));
    audio_output_flags_t flags = VALUE_OR_RETURN_ILLEGAL_ARGUMENT(
            ::aidl::android::aidl2legacy_int32_t_audio_output_flags_t_mask(
                    context.getFlags().get<AudioIoFlags::Tag::output>()));

    audio_stream_out_t* out = nullptr;
    if (int ret = mDevice->open_output_stream(mDevice, context.getMixPortHandle(), devices, flags,
                                              &config, &out, address.c_str());
        ret != 0 || out == nullptr) {
        LOG(ERROR) << __func__ << ": open_output_stream failed: " << ret;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    LOG(DEBUG) << __func__ << ": opened handle " << context.getMixPortHandle() << ", devices "
               << std::hex << devices << ", flags " << flags;

    return createStreamInstance<StreamOutLegacy>(result, std::move(context), sourceMetadata,
                                                 offloadInfo, mDevice, out);
}

void ModuleLegacy::onExternalDeviceConnectionChanged(const AudioPort& audioPort, bool connected) {
    if (audioPort.ext.getTag() != AudioPortExt::Tag::device) {
        return;
    }
    // Like the audio policy manager does for HALs without audio patch support
    audio_devices_t type;
    std::string address;
    if (!legacy::toLegacyDevices({audioPort.ext.get<AudioPortExt::Tag::device>().device}, &type,
                                 &address)) {
        return;
    }
    AudioParameter parameters = legacy::addressToParameters(type, address);
    parameters.addInt(String8(connected ? AudioParameter::keyDeviceConnect
                                        : AudioParameter::keyDeviceDisconnect),
                      static_cast<int>(type));
    legacy::setParameters(mDevice, parameters);
}

int32_t ModuleLegacy::getNominalLatencyMs(const AudioPortConfig& portConfig) {
    const auto& flags = portConfig.flags.value();
    if ((flags.getTag() == AudioIoFlags::Tag::output &&
         isBitPositionFlagSet(flags.get<AudioIoFlags::Tag::output>(), AudioOutputFlags::FAST)) ||
        (flags.getTag() == AudioIoFlags::Tag::input &&
         isBitPositionFlagSet(flags.get<AudioIoFlags::Tag::input>(), AudioInputFlags::FAST))) {
        return kLowLatencyMs;
    }
    return kNormalLatencyMs;
}

ndk::ScopedAStatus ModuleLegacy::calculateBufferSizeFrames(const AudioFormatDescription& format,
                                                           const AudioIoFlags& flags,
                                                           int32_t latencyMs, int32_t sampleRateHz,
                                                           int32_t* bufferSizeFrames) {
    if (format.type != AudioFormatType::PCM) {
        return Module::calculateBufferSizeFrames(format, flags, latencyMs, sampleRateHz,
                                                 bufferSizeFrames);
    }
    // Unlike the default implementation, do not round up to a power of 2, the legacy HAL works
    // in periods of the latency
    const int32_t frames =
            static_cast<int32_t>((static_cast<int64_t>(latencyMs) * sampleRateHz + 999) / 1000);
    *bufferSizeFrames = (frames + 15) & ~15;
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::audio::core
