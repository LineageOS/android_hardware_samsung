/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <mutex>
#include <vector>

#include <aidl/android/hardware/audio/core/BnBluetooth.h>
#include <aidl/android/hardware/audio/core/BnTelephony.h>
#include <android-base/thread_annotations.h>
#include <hardware/audio.h>

namespace aidl::android::hardware::audio::core {

/* ITelephony on top of the voice controls of the legacy audio HAL. */
class TelephonyLegacy : public BnTelephony {
  public:
    explicit TelephonyLegacy(audio_hw_device_t* device);

  private:
    ndk::ScopedAStatus getSupportedAudioModes(
            std::vector<::aidl::android::media::audio::common::AudioMode>* _aidl_return) override;
    ndk::ScopedAStatus switchAudioMode(
            ::aidl::android::media::audio::common::AudioMode in_mode) override;
    ndk::ScopedAStatus setTelecomConfig(const TelecomConfig& in_config,
                                        TelecomConfig* _aidl_return) override;

    const std::vector<::aidl::android::media::audio::common::AudioMode> mSupportedAudioModes = {
            ::aidl::android::media::audio::common::AudioMode::NORMAL,
            ::aidl::android::media::audio::common::AudioMode::RINGTONE,
            ::aidl::android::media::audio::common::AudioMode::IN_CALL,
            ::aidl::android::media::audio::common::AudioMode::IN_COMMUNICATION,
    };

    audio_hw_device_t* const mDevice;
    std::mutex mLock;
    TelecomConfig mTelecomConfig GUARDED_BY(mLock);
};

/* IBluetooth (SCO and HFP) on top of the parameters of the legacy audio HAL. */
class BluetoothLegacy : public BnBluetooth {
  public:
    explicit BluetoothLegacy(audio_hw_device_t* device);

  private:
    ndk::ScopedAStatus setScoConfig(const ScoConfig& in_config, ScoConfig* _aidl_return) override;
    ndk::ScopedAStatus setHfpConfig(const HfpConfig& in_config, HfpConfig* _aidl_return) override;

    audio_hw_device_t* const mDevice;
    std::mutex mLock;
    ScoConfig mScoConfig GUARDED_BY(mLock);
    HfpConfig mHfpConfig GUARDED_BY(mLock);
};

}  // namespace aidl::android::hardware::audio::core
