/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <mutex>

#include <android-base/thread_annotations.h>
#include <hardware/audio.h>

#include "core-impl/Stream.h"

namespace aidl::android::hardware::audio::core {

/*
 * Drives a stream of the legacy audio HAL from the stream worker thread.
 *
 * The legacy HAL has no audio patch support, so the devices of the patch a
 * stream is connected to are passed to it with the "routing" parameter, the
 * way AudioFlinger does it for such HALs.
 */
class StreamLegacy : public StreamCommonImpl {
  public:
    StreamLegacy(StreamContext* context, const Metadata& metadata, audio_hw_device_t* device,
                 audio_stream_out_t* out);
    StreamLegacy(StreamContext* context, const Metadata& metadata, audio_hw_device_t* device,
                 audio_stream_in_t* in, audio_source_t source);
    ~StreamLegacy();

    // Methods of 'DriverInterface'.
    ::android::status_t init(DriverCallbackInterface* callback) override;
    ::android::status_t drain(StreamDescriptor::DrainMode mode) override;
    ::android::status_t flush() override;
    ::android::status_t pause() override;
    ::android::status_t standby() override;
    ::android::status_t start() override;
    ::android::status_t transfer(void* buffer, size_t frameCount, size_t* actualFrameCount,
                                 int32_t* latencyMs) override;
    ::android::status_t refinePosition(StreamDescriptor::Position* position) override;
    void shutdown() override;

    // Overridden methods of 'StreamCommonImpl', called on a Binder thread.
    ndk::ScopedAStatus setConnectedDevices(const ConnectedDevices& devices) override;
    ndk::ScopedAStatus updateMetadataCommon(const Metadata& metadata) override;

  protected:
    // Calls of the legacy stream from Binder threads must hold this lock, so that they do not race
    // with the worker thread closing it. The worker thread itself does not need it.
    std::mutex mLock;
    audio_stream_out_t* out() const NO_THREAD_SAFETY_ANALYSIS { return mOut; }

  private:
    void closeLegacyStream();

    const bool mIsInput;
    const size_t mFrameSizeBytes;
    audio_hw_device_t* const mDevice;
    audio_stream_out_t* mOut = nullptr;
    audio_stream_in_t* mIn = nullptr;
    audio_stream* mStream = nullptr;
    const audio_source_t mSource;

    // Used by the worker thread only.
    int64_t mFramesTransferred = 0;
};

class StreamInLegacy final : public StreamIn, public StreamLegacy {
  public:
    friend class ndk::SharedRefBase;
    StreamInLegacy(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SinkMetadata& sinkMetadata,
            const std::vector<::aidl::android::media::audio::common::MicrophoneInfo>& microphones,
            audio_hw_device_t* device, audio_stream_in_t* in, audio_source_t source);

  private:
    void onClose(StreamDescriptor::State) override { defaultOnClose(); }
};

class StreamOutLegacy final : public StreamOut,
                              public StreamLegacy,
                              public StreamOutHwVolumeHelper {
  public:
    friend class ndk::SharedRefBase;
    StreamOutLegacy(StreamContext&& context,
                    const ::aidl::android::hardware::audio::common::SourceMetadata& sourceMetadata,
                    const std::optional<::aidl::android::media::audio::common::AudioOffloadInfo>&
                            offloadInfo,
                    audio_hw_device_t* device, audio_stream_out_t* out);

  private:
    void onClose(StreamDescriptor::State) override { defaultOnClose(); }
    ndk::ScopedAStatus getHwVolume(std::vector<float>* _aidl_return) override;
    ndk::ScopedAStatus setHwVolume(const std::vector<float>& in_channelVolumes) override;
};

}  // namespace aidl::android::hardware::audio::core
