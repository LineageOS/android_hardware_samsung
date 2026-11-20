/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "AHAL_TelephonyLegacy"

#include "TelephonyLegacy.h"

#include <algorithm>
#include <cmath>

#include <Utils.h>
#include <android-base/logging.h>

#include "LegacyUtils.h"

using aidl::android::hardware::audio::common::isValidAudioMode;
using aidl::android::media::audio::common::AudioMode;
using aidl::android::media::audio::common::Boolean;
using aidl::android::media::audio::common::Float;
using aidl::android::media::audio::common::Int;
using android::AudioParameter;
using android::String8;

namespace aidl::android::hardware::audio::core {

TelephonyLegacy::TelephonyLegacy(audio_hw_device_t* device) : mDevice(device) {
    mTelecomConfig.voiceVolume = Float{TelecomConfig::VOICE_VOLUME_MAX};
    mTelecomConfig.ttyMode = TelecomConfig::TtyMode::OFF;
    mTelecomConfig.isHacEnabled = Boolean{false};
}

ndk::ScopedAStatus TelephonyLegacy::getSupportedAudioModes(std::vector<AudioMode>* _aidl_return) {
    *_aidl_return = mSupportedAudioModes;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus TelephonyLegacy::switchAudioMode(AudioMode in_mode) {
    if (!isValidAudioMode(in_mode)) {
        LOG(ERROR) << __func__ << ": invalid mode " << toString(in_mode);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (std::find(mSupportedAudioModes.begin(), mSupportedAudioModes.end(), in_mode) ==
        mSupportedAudioModes.end()) {
        LOG(ERROR) << __func__ << ": unsupported mode " << toString(in_mode);
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    // The legacy HAL is switched in IModule::updateAudioMode, which follows this call and also
    // covers the modes that are not related to telephony
    LOG(DEBUG) << __func__ << ": " << toString(in_mode);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus TelephonyLegacy::setTelecomConfig(const TelecomConfig& in_config,
                                                     TelecomConfig* _aidl_return) {
    if (in_config.voiceVolume.has_value() &&
        (in_config.voiceVolume.value().value < TelecomConfig::VOICE_VOLUME_MIN ||
         in_config.voiceVolume.value().value > TelecomConfig::VOICE_VOLUME_MAX ||
         std::isnan(in_config.voiceVolume.value().value))) {
        LOG(ERROR) << __func__
                   << ": voice volume value is invalid: " << in_config.voiceVolume.value().value;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard l(mLock);

    if (in_config.voiceVolume.has_value()) {
        const float volume = in_config.voiceVolume.value().value;
        if (int ret = mDevice->set_voice_volume(mDevice, volume); ret != 0) {
            LOG(ERROR) << __func__ << ": set_voice_volume(" << volume << ") failed: " << ret;
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        }
        mTelecomConfig.voiceVolume = in_config.voiceVolume;
    }

    AudioParameter parameters;
    if (in_config.ttyMode != TelecomConfig::TtyMode::UNSPECIFIED) {
        const char* value = nullptr;
        switch (in_config.ttyMode) {
            case TelecomConfig::TtyMode::OFF:
                value = AudioParameter::valueTtyModeOff;
                break;
            case TelecomConfig::TtyMode::FULL:
                value = AudioParameter::valueTtyModeFull;
                break;
            case TelecomConfig::TtyMode::HCO:
                value = AudioParameter::valueTtyModeHco;
                break;
            case TelecomConfig::TtyMode::VCO:
                value = AudioParameter::valueTtyModeVco;
                break;
            default:
                LOG(ERROR) << __func__ << ": invalid TTY mode " << toString(in_config.ttyMode);
                return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        parameters.add(String8(AudioParameter::keyTtyMode), String8(value));
        mTelecomConfig.ttyMode = in_config.ttyMode;
    }
    if (in_config.isHacEnabled.has_value()) {
        parameters.add(String8(AudioParameter::keyHacSetting),
                       String8(in_config.isHacEnabled.value().value ? AudioParameter::valueHacOn
                                                                    : AudioParameter::valueHacOff));
        mTelecomConfig.isHacEnabled = in_config.isHacEnabled;
    }
    if (parameters.size() > 0) {
        legacy::setParameters(mDevice, parameters);
    }

    *_aidl_return = mTelecomConfig;
    LOG(DEBUG) << __func__ << ": received " << in_config.toString() << ", returning "
               << _aidl_return->toString();
    return ndk::ScopedAStatus::ok();
}

BluetoothLegacy::BluetoothLegacy(audio_hw_device_t* device) : mDevice(device) {
    mScoConfig.isEnabled = Boolean{false};
    mScoConfig.isNrecEnabled = Boolean{false};
    mScoConfig.mode = ScoConfig::Mode::SCO;
    mHfpConfig.isEnabled = Boolean{false};
    mHfpConfig.sampleRate = Int{32000};
    mHfpConfig.volume = Float{HfpConfig::VOLUME_MAX};
}

ndk::ScopedAStatus BluetoothLegacy::setScoConfig(const ScoConfig& in_config,
                                                 ScoConfig* _aidl_return) {
    std::lock_guard l(mLock);

    AudioParameter parameters;
    if (in_config.isEnabled.has_value()) {
        parameters.add(String8(AudioParameter::keyBtSco),
                       String8(in_config.isEnabled.value().value ? AudioParameter::valueOn
                                                                 : AudioParameter::valueOff));
        mScoConfig.isEnabled = in_config.isEnabled;
    }
    if (in_config.isNrecEnabled.has_value()) {
        parameters.add(String8(AudioParameter::keyBtNrec),
                       String8(in_config.isNrecEnabled.value().value ? AudioParameter::valueOn
                                                                     : AudioParameter::valueOff));
        mScoConfig.isNrecEnabled = in_config.isNrecEnabled;
    }
    if (in_config.mode != ScoConfig::Mode::UNSPECIFIED) {
        // The legacy HAL only knows narrow band and wide band
        parameters.add(String8(AudioParameter::keyBtScoWb),
                       String8(in_config.mode == ScoConfig::Mode::SCO ? AudioParameter::valueOff
                                                                      : AudioParameter::valueOn));
        mScoConfig.mode = in_config.mode;
    }
    if (in_config.debugName.has_value()) {
        parameters.add(String8(AudioParameter::keyBtScoHeadsetName),
                       String8(in_config.debugName.value().c_str()));
        mScoConfig.debugName = in_config.debugName;
    }
    if (parameters.size() > 0) {
        legacy::setParameters(mDevice, parameters);
    }

    *_aidl_return = mScoConfig;
    LOG(DEBUG) << __func__ << ": received " << in_config.toString() << ", returning "
               << _aidl_return->toString();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BluetoothLegacy::setHfpConfig(const HfpConfig& in_config,
                                                 HfpConfig* _aidl_return) {
    if (in_config.sampleRate.has_value() && in_config.sampleRate.value().value <= 0) {
        LOG(ERROR) << __func__ << ": invalid sample rate: " << in_config.sampleRate.value().value;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (in_config.volume.has_value() && (in_config.volume.value().value < HfpConfig::VOLUME_MIN ||
                                         in_config.volume.value().value > HfpConfig::VOLUME_MAX)) {
        LOG(ERROR) << __func__ << ": invalid volume: " << in_config.volume.value().value;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard l(mLock);

    AudioParameter parameters;
    if (in_config.isEnabled.has_value()) {
        parameters.add(String8(AudioParameter::keyBtHfpEnable),
                       String8(in_config.isEnabled.value().value ? AudioParameter::valueTrue
                                                                 : AudioParameter::valueFalse));
        mHfpConfig.isEnabled = in_config.isEnabled;
    }
    if (in_config.sampleRate.has_value()) {
        parameters.addInt(String8(AudioParameter::keyBtHfpSamplingRate),
                          in_config.sampleRate.value().value);
        mHfpConfig.sampleRate = in_config.sampleRate;
    }
    if (in_config.volume.has_value()) {
        // The legacy parameter takes the 0-15 range of the Bluetooth stack
        parameters.addInt(String8(AudioParameter::keyBtHfpVolume),
                          static_cast<int>(std::lround(in_config.volume.value().value * 15)));
        mHfpConfig.volume = in_config.volume;
    }
    if (parameters.size() > 0) {
        legacy::setParameters(mDevice, parameters);
    }

    *_aidl_return = mHfpConfig;
    LOG(DEBUG) << __func__ << ": received " << in_config.toString() << ", returning "
               << _aidl_return->toString();
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::audio::core
