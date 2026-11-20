/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "AHAL_StreamLegacy"

#include "StreamLegacy.h"

#include <cstring>

#include <android-base/logging.h>
#include <audio_utils/clock.h>
#include <error/expected_utils.h>
#include <media/AidlConversionCppNdk.h>

#include "LegacyUtils.h"

using aidl::android::hardware::audio::common::SinkMetadata;
using aidl::android::hardware::audio::common::SourceMetadata;
using aidl::android::media::audio::common::AudioOffloadInfo;
using aidl::android::media::audio::common::MicrophoneInfo;
using android::AudioParameter;
using android::String8;

namespace aidl::android::hardware::audio::core {

StreamLegacy::StreamLegacy(StreamContext* context, const Metadata& metadata,
                           audio_hw_device_t* device, audio_stream_out_t* out)
    : StreamCommonImpl(context, metadata),
      mIsInput(false),
      mFrameSizeBytes(getContext().getFrameSize()),
      mDevice(device),
      mOut(out),
      mStream(&out->common),
      mSource(AUDIO_SOURCE_DEFAULT) {}

StreamLegacy::StreamLegacy(StreamContext* context, const Metadata& metadata,
                           audio_hw_device_t* device, audio_stream_in_t* in, audio_source_t source)
    : StreamCommonImpl(context, metadata),
      mIsInput(true),
      mFrameSizeBytes(getContext().getFrameSize()),
      mDevice(device),
      mIn(in),
      mStream(&in->common),
      mSource(source) {}

StreamLegacy::~StreamLegacy() {
    cleanupWorker();
    closeLegacyStream();
}

void StreamLegacy::closeLegacyStream() {
    std::lock_guard l(mLock);
    if (mOut != nullptr) {
        mDevice->close_output_stream(mDevice, mOut);
        mOut = nullptr;
    }
    if (mIn != nullptr) {
        mDevice->close_input_stream(mDevice, mIn);
        mIn = nullptr;
    }
    mStream = nullptr;
}

::android::status_t StreamLegacy::init(DriverCallbackInterface*) {
    return mStream != nullptr ? ::android::OK : ::android::NO_INIT;
}

::android::status_t StreamLegacy::drain(StreamDescriptor::DrainMode) {
    // Only offloaded streams have something to drain, and they are not supported
    return ::android::OK;
}

::android::status_t StreamLegacy::flush() {
    if (mOut != nullptr && mOut->flush != nullptr) {
        return mOut->flush(mOut);
    }
    return mIsInput ? standby() : ::android::OK;
}

::android::status_t StreamLegacy::pause() {
    if (mOut != nullptr && mOut->pause != nullptr) {
        if (int ret = mOut->pause(mOut); ret != 0 && ret != -ENOSYS) {
            LOG(ERROR) << __func__ << ": pause failed: " << ret;
            return ret;
        }
    }
    return ::android::OK;
}

::android::status_t StreamLegacy::standby() {
    if (int ret = mStream->standby(mStream); ret != 0) {
        LOG(ERROR) << __func__ << ": standby failed: " << ret;
        return ret;
    }
    return ::android::OK;
}

::android::status_t StreamLegacy::start() {
    if (mOut != nullptr && mOut->resume != nullptr) {
        if (int ret = mOut->resume(mOut); ret != 0 && ret != -ENOSYS) {
            LOG(WARNING) << __func__ << ": resume failed: " << ret;
        }
    }
    // The legacy streams start on the first write or read
    return ::android::OK;
}

::android::status_t StreamLegacy::transfer(void* buffer, size_t frameCount,
                                           size_t* actualFrameCount, int32_t* latencyMs) {
    const size_t bytes = frameCount * mFrameSizeBytes;
    if (mIsInput) {
        ssize_t ret = mIn->read(mIn, buffer, bytes);
        if (ret < 0) {
            // The legacy HAL already waits for the duration of the buffer on errors, provide
            // silence like AudioFlinger would have
            LOG(ERROR) << __func__ << ": read of " << bytes << " bytes failed: " << ret;
            memset(buffer, 0, bytes);
            ret = bytes;
        }
        *actualFrameCount = ret / mFrameSizeBytes;
        *latencyMs = getContext().getNominalLatencyMs();
    } else {
        ssize_t ret = mOut->write(mOut, buffer, bytes);
        if (ret < 0) {
            LOG(ERROR) << __func__ << ": write of " << bytes << " bytes failed: " << ret;
            ret = bytes;
        }
        *actualFrameCount = ret / mFrameSizeBytes;
        *latencyMs = mOut->get_latency(mOut);
    }
    mFramesTransferred += *actualFrameCount;
    return ::android::OK;
}

::android::status_t StreamLegacy::refinePosition(StreamDescriptor::Position* position) {
    // The position of the legacy stream does not include the frames that were consumed while
    // the stream was not connected to a device, add them on top of it
    const int64_t notTransferred = position->frames - mFramesTransferred;
    if (mIsInput) {
        if (mIn->get_capture_position == nullptr) return ::android::OK;
        int64_t frames, timeNs;
        if (mIn->get_capture_position(mIn, &frames, &timeNs) == 0) {
            position->frames = frames + notTransferred;
            position->timeNs = timeNs;
        }
    } else {
        if (mOut->get_presentation_position == nullptr) return ::android::OK;
        uint64_t frames;
        struct timespec timestamp;
        if (mOut->get_presentation_position(mOut, &frames, &timestamp) == 0) {
            position->frames = static_cast<int64_t>(frames) + notTransferred;
            position->timeNs = audio_utils_ns_from_timespec(&timestamp);
        }
    }
    // Otherwise the stream is in standby, keep the position maintained by the Stream class
    return ::android::OK;
}

void StreamLegacy::shutdown() {
    closeLegacyStream();
}

ndk::ScopedAStatus StreamLegacy::setConnectedDevices(const ConnectedDevices& devices) {
    LOG(DEBUG) << __func__ << ": " << ::android::internal::ToString(devices);
    std::lock_guard l(mLock);
    if (mStream == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    audio_devices_t types;
    std::string address;
    if (!legacy::toLegacyDevices(devices, &types, &address)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    // Only the first device type of an input can be passed
    if (mIsInput && !devices.empty()) {
        ::aidl::android::aidl2legacy_AudioDevice_audio_device(devices[0], &types, &address);
    }
    AudioParameter parameters = legacy::addressToParameters(types, address);
    parameters.addInt(String8(AudioParameter::keyRouting), static_cast<int>(types));
    if (mIsInput && !devices.empty()) {
        parameters.addInt(String8(AudioParameter::keyInputSource), static_cast<int>(mSource));
    }
    if (legacy::setParameters(mStream, parameters) != 0) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return StreamCommonImpl::setConnectedDevices(devices);
}

ndk::ScopedAStatus StreamLegacy::updateMetadataCommon(const Metadata& metadata) {
    RETURN_STATUS_IF_ERROR(StreamCommonImpl::updateMetadataCommon(metadata));
    std::lock_guard l(mLock);
    if (mStream == nullptr) {
        return ndk::ScopedAStatus::ok();
    }

    if (mOut != nullptr && mOut->update_source_metadata != nullptr) {
        const auto& tracks = std::get<SourceMetadata>(metadata).tracks;
        std::vector<playback_track_metadata_t> legacyTracks;
        for (const auto& track : tracks) {
            auto usage = ::aidl::android::aidl2legacy_AudioUsage_audio_usage_t(track.usage);
            auto contentType = ::aidl::android::aidl2legacy_AudioContentType_audio_content_type_t(
                    track.contentType);
            if (!usage.ok() || !contentType.ok()) continue;
            playback_track_metadata_t legacyTrack{};
            legacyTrack.usage = usage.value();
            legacyTrack.content_type = contentType.value();
            legacyTrack.gain = track.gain;
            legacyTracks.push_back(legacyTrack);
        }
        const source_metadata_t legacy = {.track_count = legacyTracks.size(),
                                          .tracks = legacyTracks.data()};
        mOut->update_source_metadata(mOut, &legacy);
    } else if (mIn != nullptr && mIn->update_sink_metadata != nullptr) {
        const auto& tracks = std::get<SinkMetadata>(metadata).tracks;
        std::vector<record_track_metadata_t> legacyTracks;
        for (const auto& track : tracks) {
            auto source = ::aidl::android::aidl2legacy_AudioSource_audio_source_t(track.source);
            if (!source.ok()) continue;
            record_track_metadata_t legacyTrack{};
            legacyTrack.source = source.value();
            legacyTrack.gain = track.gain;
            legacyTrack.dest_device = AUDIO_DEVICE_NONE;
            legacyTracks.push_back(legacyTrack);
        }
        const sink_metadata_t legacy = {.track_count = legacyTracks.size(),
                                        .tracks = legacyTracks.data()};
        mIn->update_sink_metadata(mIn, &legacy);
    }
    return ndk::ScopedAStatus::ok();
}

StreamInLegacy::StreamInLegacy(StreamContext&& context, const SinkMetadata& sinkMetadata,
                               const std::vector<MicrophoneInfo>& microphones,
                               audio_hw_device_t* device, audio_stream_in_t* in,
                               audio_source_t source)
    : StreamIn(std::move(context), microphones),
      StreamLegacy(&mContextInstance, sinkMetadata, device, in, source) {}

StreamOutLegacy::StreamOutLegacy(StreamContext&& context, const SourceMetadata& sourceMetadata,
                                 const std::optional<AudioOffloadInfo>& offloadInfo,
                                 audio_hw_device_t* device, audio_stream_out_t* out)
    : StreamOut(std::move(context), offloadInfo),
      StreamLegacy(&mContextInstance, sourceMetadata, device, out),
      StreamOutHwVolumeHelper(&mContextInstance) {}

ndk::ScopedAStatus StreamOutLegacy::getHwVolume(std::vector<float>* _aidl_return) {
    std::lock_guard l(mLock);
    if (out() == nullptr || out()->set_volume == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    return getHwVolumeImpl(_aidl_return);
}

ndk::ScopedAStatus StreamOutLegacy::setHwVolume(const std::vector<float>& in_channelVolumes) {
    std::lock_guard l(mLock);
    if (out() == nullptr || out()->set_volume == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    if (in_channelVolumes.empty() || in_channelVolumes.size() > 2) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const float left = in_channelVolumes[0];
    const float right = in_channelVolumes.size() > 1 ? in_channelVolumes[1] : left;
    if (int ret = out()->set_volume(out(), left, right); ret != 0) {
        LOG(DEBUG) << __func__ << ": set_volume failed: " << ret;
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    return setHwVolumeImpl(in_channelVolumes);
}

}  // namespace aidl::android::hardware::audio::core
