/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "convert.h"

#include <CameraMetadata.h>
#include <HandleImporter.h>
#include <aidl/android/hardware/camera/common/Status.h>
#include <aidl/android/hardware/camera/device/BnCameraDeviceSession.h>
#include <aidl/android/hardware/camera/device/CaptureResult.h>
#include <aidl/android/hardware/camera/device/ICameraOfflineSession.h>
#include <aidl/android/hardware/camera/device/NotifyMsg.h>
#include <aidl/android/hardware/camera/device/StreamBuffer.h>
#include <fmq/AidlMessageQueue.h>
#include <hardware/camera3.h>
#include <utils/Mutex.h>

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {

using ::aidl::android::hardware::camera::common::Status;
using ::aidl::android::hardware::camera::device::BnCameraDeviceSession;
using ::aidl::android::hardware::camera::device::BufferCache;
using ::aidl::android::hardware::camera::device::CameraMetadata;
using ::aidl::android::hardware::camera::device::CameraOfflineSessionInfo;
using ::aidl::android::hardware::camera::device::CaptureRequest;
using ::aidl::android::hardware::camera::device::CaptureResult;
using ::aidl::android::hardware::camera::device::HalStream;
using ::aidl::android::hardware::camera::device::ICameraDeviceCallback;
using ::aidl::android::hardware::camera::device::ICameraOfflineSession;
using ::aidl::android::hardware::camera::device::NotifyMsg;
using ::aidl::android::hardware::camera::device::RequestTemplate;
using ::aidl::android::hardware::camera::device::StreamBuffer;
using ::aidl::android::hardware::camera::device::StreamConfiguration;
using ::android::AidlMessageQueue;
using ::android::hardware::camera::common::helper::HandleImporter;

/**
 * Function pointer types with C calling convention to
 * use for HAL callback functions.
 */
extern "C" {
typedef void(callbacks_process_capture_result_t)(const struct camera3_callback_ops*,
                                                 const camera3_capture_result_t*);

typedef void(callbacks_notify_t)(const struct camera3_callback_ops*, const camera3_notify_msg_t*);
}

class CameraDeviceSession : public BnCameraDeviceSession, protected camera3_callback_ops {
  public:
    CameraDeviceSession(camera3_device_t*, const camera_metadata_t* deviceInfo,
                        const std::shared_ptr<ICameraDeviceCallback>&);
    virtual ~CameraDeviceSession();
    // Caller must use this method to check if CameraDeviceSession ctor failed
    bool isInitFailed() { return mInitFail; }
    // Used by CameraDevice to signal external camera disconnected
    void disconnect();
    bool isClosed();

    ndk::ScopedAStatus close() override;
    ndk::ScopedAStatus configureStreams(const StreamConfiguration& in_requestedConfiguration,
                                        std::vector<HalStream>* _aidl_return) override;
    ndk::ScopedAStatus constructDefaultRequestSettings(RequestTemplate in_type,
                                                       CameraMetadata* _aidl_return) override;
    ndk::ScopedAStatus flush() override;
    ndk::ScopedAStatus getCaptureRequestMetadataQueue(
            MQDescriptor<int8_t, SynchronizedReadWrite>* _aidl_return) override;
    ndk::ScopedAStatus getCaptureResultMetadataQueue(
            MQDescriptor<int8_t, SynchronizedReadWrite>* _aidl_return) override;
    ndk::ScopedAStatus isReconfigurationRequired(const CameraMetadata& in_oldSessionParams,
                                                 const CameraMetadata& in_newSessionParams,
                                                 bool* _aidl_return) override;
    ndk::ScopedAStatus processCaptureRequest(const std::vector<CaptureRequest>& in_requests,
                                             const std::vector<BufferCache>& in_cachesToRemove,
                                             int32_t* _aidl_return) override;
    ndk::ScopedAStatus signalStreamFlush(const std::vector<int32_t>& in_streamIds,
                                         int32_t in_streamConfigCounter) override;
    ndk::ScopedAStatus switchToOffline(
            const std::vector<int32_t>& in_streamsToKeep,
            CameraOfflineSessionInfo* out_offlineSessionInfo,
            std::shared_ptr<ICameraOfflineSession>* _aidl_return) override;
    ndk::ScopedAStatus repeatingRequestEnd(int32_t in_frameNumber,
                                           const std::vector<int32_t>& in_streamIds) override;

  protected:
    // Helper methods
    Status constructDefaultRequestSettingsRaw(int type, CameraMetadata* outMetadata);

    bool preProcessConfigurationLocked(const StreamConfiguration& requestedConfiguration,
                                       camera3_stream_configuration_t* stream_list /*out*/,
                                       std::vector<camera3_stream_t*>* streams /*out*/);
    void postProcessConfigurationLocked(const StreamConfiguration& requestedConfiguration);
    void postProcessConfigurationFailureLocked(const StreamConfiguration& requestedConfiguration);
    Status processOneCaptureRequest(const CaptureRequest& request);

    // Method to pop buffer's bufferId from mBufferIdMaps
    // BUFFER_ID_NO_BUFFER is returned if no matching buffer is found
    uint64_t popBufferId(const buffer_handle_t& buf, int streamId);

    // By default camera service uses frameNumber/streamId pair to retrieve the buffer that
    // was sent to HAL. Override this implementation if HAL is using buffers from buffer management
    // APIs to send output buffer.
    virtual uint64_t getCapResultBufferId(const buffer_handle_t& buf, int streamId);

    status_t constructCaptureResult(CaptureResult& result,
                                    const camera3_capture_result* hal_result);

    // Static helper method to copy/shrink capture result metadata sent by HAL
    // Temporarily allocated metadata copy will be hold in mds
    static void sShrinkCaptureResult(
            camera3_capture_result* dst, const camera3_capture_result* src,
            std::vector<::android::hardware::camera::common::helper::CameraMetadata>* mds,
            std::vector<const camera_metadata_t*>* physCamMdArray, bool handlePhysCam);
    static bool sShouldShrink(const camera_metadata_t* md);
    static camera_metadata_t* sCreateCompactCopy(const camera_metadata_t* src);

    // protecting mClosed/mDisconnected/mInitFail
    mutable Mutex mStateLock;
    // device is closed either
    //    - closed by user
    //    - init failed
    //    - camera disconnected
    bool mClosed = false;

    // Set by CameraDevice (when external camera is disconnected)
    bool mDisconnected = false;

    struct AETriggerCancelOverride {
        bool applyAeLock;
        uint8_t aeLock;
        bool applyAePrecaptureTrigger;
        uint8_t aePrecaptureTrigger;
    };

    camera3_device_t* mDevice;
    const uint32_t mDeviceVersion;
    const bool mFreeBufEarly;
    const int32_t mStartupFlushWaitMs;
    bool mIsAELockAvailable;
    bool mDerivePostRawSensKey;
    uint32_t mNumPartialResults;
    // Stream ID -> Camera3Stream cache
    std::map<int, Camera3Stream> mStreamMap;

    std::map<int, std::string> mPhysicalCameraIdMap;
    // Physical camera ids for the logical multi-camera. Empty if this
    // is not a logical multi-camera.
    std::unordered_set<std::string> mPhysicalCameraIds;

    Mutex mStreamConfigCounterLock;
    uint32_t mStreamConfigCounter = 1;

    mutable Mutex mInflightLock;  // protecting mInflightBuffers and mCirculatingBuffers
    // (streamID, frameNumber) -> inflight buffer cache
    std::map<std::pair<int, uint32_t>, camera3_stream_buffer_t> mInflightBuffers;

    // (frameNumber, AETriggerOverride) -> inflight request AETriggerOverrides
    std::map<uint32_t, AETriggerCancelOverride> mInflightAETriggerOverrides;
    ::android::hardware::camera::common::helper::CameraMetadata mOverridenResult;
    std::map<uint32_t, bool> mInflightRawBoostPresent;
    ::android::hardware::camera::common::helper::CameraMetadata mOverridenRequest;

    static const uint64_t BUFFER_ID_NO_BUFFER = 0;
    // buffers currently ciculating between HAL and camera service
    // key: bufferId sent via HIDL interface
    // value: imported buffer_handle_t
    // Buffer will be imported during process_capture_request and will be freed
    // when the its stream is deleted or camera device session is closed
    typedef std::unordered_map<uint64_t, buffer_handle_t> CirculatingBuffers;
    // Stream ID -> circulating buffers map
    std::map<int, CirculatingBuffers> mCirculatingBuffers;

    static HandleImporter sHandleImporter;
    static buffer_handle_t sEmptyBuffer;

    bool mInitFail;
    bool mFirstRequest = false;

    // Some HALs (e.g. Exynos libexynoscamera3) abort if flush() arrives while the stream
    // is still starting up, and they flush internally on close(). Between the first request
    // after configure/flush and the first HAL callback, hold off flush()/close() for up to
    // mStartupFlushWaitMs. Disabled when ro.vendor.camera.startup_flush_wait_ms is 0.
    std::mutex mStreamStartLock;  // protecting mStreamIdle and mStreamStarting
    std::condition_variable mStreamStartCond;
    // No request has been sent since the last configureStreams/flush
    bool mStreamIdle = true;
    // A request was sent while idle and the HAL has not called back yet
    bool mStreamStarting = false;

    struct BufferHasher {
        size_t operator()(const buffer_handle_t& buf) const {
            if (buf == nullptr) return 0;

            size_t result = 1;
            result = 31 * result + buf->numFds;
            for (int i = 0; i < buf->numFds; i++) {
                result = 31 * result + buf->data[i];
            }
            return result;
        }
    };

    struct BufferComparator {
        bool operator()(const buffer_handle_t& buf1, const buffer_handle_t& buf2) const {
            if (buf1->numFds == buf2->numFds) {
                for (int i = 0; i < buf1->numFds; i++) {
                    if (buf1->data[i] != buf2->data[i]) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }
    };

    std::mutex mBufferIdMapLock;  // protecting mBufferIdMaps and mNextBufferId
    typedef std::unordered_map<const buffer_handle_t, uint64_t, BufferHasher, BufferComparator>
            BufferIdMap;
    // stream ID -> per stream buffer ID map for buffers coming from requestStreamBuffers API
    // Entries are created during requestStreamBuffers when a stream first request a buffer, and
    // deleted in returnStreamBuffers/processCaptureResult* when all buffers are returned
    std::unordered_map<int, BufferIdMap> mBufferIdMaps;

    ::android::hardware::camera::common::helper::CameraMetadata mDeviceInfo;

    using RequestMetadataQueue = AidlMessageQueue<int8_t, SynchronizedReadWrite>;
    std::unique_ptr<RequestMetadataQueue> mRequestMetadataQueue;
    using ResultMetadataQueue = AidlMessageQueue<int8_t, SynchronizedReadWrite>;
    std::shared_ptr<ResultMetadataQueue> mResultMetadataQueue;

    class ResultBatcher {
      public:
        ResultBatcher(const std::shared_ptr<ICameraDeviceCallback>& callback);
        void setNumPartialResults(uint32_t n);
        void setBatchedStreams(const std::vector<int>& streamsToBatch);
        void setResultMetadataQueue(std::shared_ptr<ResultMetadataQueue> q);

        void registerBatch(uint32_t frameNumber, uint32_t batchSize);
        void notify(NotifyMsg& msg);
        void processCaptureResult(CaptureResult& result);

      protected:
        struct InflightBatch {
            // Protect access to entire struct. Acquire this lock before read/write any data or
            // calling any methods. processCaptureResult and notify will compete for this lock
            // HIDL IPCs might be issued while the lock is held
            Mutex mLock;

            bool allDelivered() const;

            uint32_t mFirstFrame;
            uint32_t mLastFrame;
            uint32_t mBatchSize;

            bool mShutterDelivered = false;
            std::vector<NotifyMsg> mShutterMsgs;

            struct BufferBatch {
                BufferBatch(uint32_t batchSize) { mBuffers.reserve(batchSize); }
                bool mDelivered = false;
                // This currently assumes every batched request will output to the batched stream
                // and since HAL must always send buffers in order, no frameNumber tracking is
                // needed
                std::vector<StreamBuffer> mBuffers;
            };
            // Stream ID -> VideoBatch
            std::unordered_map<int, BufferBatch> mBatchBufs;

            struct MetadataBatch {
                //                   (frameNumber, metadata)
                std::vector<std::pair<uint32_t, CameraMetadata>> mMds;
            };
            // Partial result IDs that has been delivered to framework
            uint32_t mNumPartialResults;
            uint32_t mPartialResultProgress = 0;
            // partialResult -> MetadataBatch
            std::map<uint32_t, MetadataBatch> mResultMds;

            // Set to true when batch is removed from mInflightBatches
            // processCaptureResult and notify must check this flag after acquiring mLock to make
            // sure this batch isn't removed while waiting for mLock
            bool mRemoved = false;
        };

        // Get the batch index and pointer to InflightBatch (nullptrt if the frame is not batched)
        // Caller must acquire the InflightBatch::mLock before accessing the InflightBatch
        // It's possible that the InflightBatch is removed from mInflightBatches before the
        // InflightBatch::mLock is acquired (most likely caused by an error notification), so
        // caller must check InflightBatch::mRemoved flag after the lock is acquried.
        // This method will hold ResultBatcher::mLock briefly
        std::pair<int, std::shared_ptr<InflightBatch>> getBatch(uint32_t frameNumber);

        static const int NOT_BATCHED = -1;

        // move/push function avoids "hidl_handle& operator=(hidl_handle&)", which clones native
        // handle
        void moveStreamBuffer(StreamBuffer&& src, StreamBuffer& dst);
        void pushStreamBuffer(StreamBuffer&& src, std::vector<StreamBuffer>& dst);

        void sendBatchMetadataLocked(std::shared_ptr<InflightBatch> batch,
                                     uint32_t lastPartialResultIdx);

        // Check if the first batch in mInflightBatches is ready to be removed, and remove it if so
        // This method will hold ResultBatcher::mLock briefly
        void checkAndRemoveFirstBatch();

        // The following sendXXXX methods must be called while the InflightBatch::mLock is locked
        // HIDL IPC methods will be called during these methods.
        void sendBatchShutterCbsLocked(std::shared_ptr<InflightBatch> batch);
        // send buffers for all batched streams
        void sendBatchBuffersLocked(std::shared_ptr<InflightBatch> batch);
        // send buffers for specified streams
        void sendBatchBuffersLocked(std::shared_ptr<InflightBatch> batch,
                                    const std::vector<int>& streams);
        // End of sendXXXX methods

        // helper methods
        void freeReleaseFences(std::vector<CaptureResult>&);
        void notifySingleMsg(NotifyMsg& msg);
        void processOneCaptureResult(CaptureResult& result);
        void invokeProcessCaptureResultCallback(std::vector<CaptureResult>& results,
                                                bool tryWriteFmq);

        // Protect access to mInflightBatches, mNumPartialResults and mStreamsToBatch
        // processCaptureRequest, processCaptureResult, notify will compete for this lock
        // Do NOT issue HIDL IPCs while holding this lock (except when HAL reports error)
        mutable Mutex mLock;
        std::deque<std::shared_ptr<InflightBatch>> mInflightBatches;
        uint32_t mNumPartialResults;
        std::vector<int> mStreamsToBatch;
        const std::shared_ptr<ICameraDeviceCallback> mCallback;
        std::shared_ptr<ResultMetadataQueue> mResultMetadataQueue;

        // Protect against invokeProcessCaptureResultCallback()
        Mutex mProcessCaptureResultLock;

    } mResultBatcher;

    std::vector<int> mVideoStreamIds;

    bool initialize();

    static bool shouldFreeBufEarly();
    static int32_t getStartupFlushWaitMs();

    void markStreamIdle();
    void markStreamStarting();
    void markStreamStarted();
    void waitForStreamStart(const char* caller);

    Status initStatus() const;

    // Validate and import request's input buffer and acquire fence
    virtual Status importRequest(const CaptureRequest& request,
                                 std::vector<buffer_handle_t*>& allBufPtrs,
                                 std::vector<int>& allFences);

    Status importRequestImpl(const CaptureRequest& request,
                             std::vector<buffer_handle_t*>& allBufPtrs, std::vector<int>& allFences,
                             // Optional argument for ICameraDeviceSession@3.5 impl
                             bool allowEmptyBuf = false);

    Status importBuffer(int32_t streamId, uint64_t bufId, buffer_handle_t buf,
                        /*out*/ buffer_handle_t** outBufPtr, bool allowEmptyBuf);

    static void cleanupInflightFences(std::vector<int>& allFences, size_t numFences);

    void cleanupBuffersLocked(int id);

    void updateBufferCaches(const std::vector<BufferCache>& cachesToRemove);

    bool handleAePrecaptureCancelRequestLocked(
            const camera3_capture_request_t& halRequest,
            android::hardware::camera::common::helper::CameraMetadata* settings /*out*/,
            AETriggerCancelOverride* override /*out*/);

    void overrideResultForPrecaptureCancelLocked(
            const AETriggerCancelOverride& aeTriggerCancelOverride,
            ::android::hardware::camera::common::helper::CameraMetadata* settings /*out*/);

    /**
     * Static callback forwarding methods from HAL to instance
     */
    static callbacks_process_capture_result_t sProcessCaptureResult;
    static callbacks_notify_t sNotify;
};

}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
