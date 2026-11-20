/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <hardware/audio.h>

#include "core-impl/ChildInterface.h"
#include "core-impl/Module.h"

namespace aidl::android::hardware::audio::core {

/*
 * The primary module, backed by the legacy libhardware audio HAL
 * (audio.primary.<platform>.so), like the HIDL android.hardware.audio@7.0
 * default implementation.
 */
class ModuleLegacy final : public Module {
  public:
    static std::shared_ptr<ModuleLegacy> create(std::unique_ptr<Configuration>&& config);

    ModuleLegacy(std::unique_ptr<Configuration>&& config, audio_hw_device_t* device);
    ~ModuleLegacy();

  protected:
    ndk::ScopedAStatus getTelephony(std::shared_ptr<ITelephony>* _aidl_return) override;
    ndk::ScopedAStatus getBluetooth(std::shared_ptr<IBluetooth>* _aidl_return) override;
    ndk::ScopedAStatus setMicMute(bool in_mute) override;
    ndk::ScopedAStatus updateAudioMode(
            ::aidl::android::media::audio::common::AudioMode in_mode) override;
    ndk::ScopedAStatus updateScreenRotation(
            ::aidl::android::hardware::audio::core::IModule::ScreenRotation in_rotation) override;
    ndk::ScopedAStatus updateScreenState(bool in_isTurnedOn) override;

    ndk::ScopedAStatus createInputStream(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SinkMetadata& sinkMetadata,
            const std::vector<::aidl::android::media::audio::common::MicrophoneInfo>& microphones,
            std::shared_ptr<StreamIn>* result) override;
    ndk::ScopedAStatus createOutputStream(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SourceMetadata& sourceMetadata,
            const std::optional<::aidl::android::media::audio::common::AudioOffloadInfo>&
                    offloadInfo,
            std::shared_ptr<StreamOut>* result) override;
    void onExternalDeviceConnectionChanged(
            const ::aidl::android::media::audio::common::AudioPort& audioPort,
            bool connected) override;
    int32_t getNominalLatencyMs(
            const ::aidl::android::media::audio::common::AudioPortConfig& portConfig) override;
    ndk::ScopedAStatus calculateBufferSizeFrames(
            const ::aidl::android::media::audio::common::AudioFormatDescription& format,
            const ::aidl::android::media::audio::common::AudioIoFlags& flags, int32_t latencyMs,
            int32_t sampleRateHz, int32_t* bufferSizeFrames) override;

  private:
    const ::aidl::android::media::audio::common::AudioPortConfig* findMixPortConfig(int32_t handle);
    ndk::ScopedAStatus checkStreamContext(const StreamContext& context);

    audio_hw_device_t* const mDevice;
    ChildInterface<ITelephony> mTelephony;
    ChildInterface<IBluetooth> mBluetooth;
};

}  // namespace aidl::android::hardware::audio::core
