/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "HalProxy.h"

#include <android/hardware/sensors/2.0/types.h>

#include <android-base/file.h>
#include "hardware_legacy/power.h"

#include <dlfcn.h>

#include <cinttypes>
#include <cmath>
#include <fstream>
#include <functional>
#include <thread>

namespace android {
namespace hardware {
namespace sensors {
namespace V2_1 {
namespace implementation {

using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V2_0::EventQueueFlagBits;
using ::android::hardware::sensors::V2_0::WakeLockQueueFlagBits;
using ::android::hardware::sensors::V2_0::implementation::getTimeNow;
using ::android::hardware::sensors::V2_0::implementation::kWakelockTimeoutNs;

typedef V2_0::implementation::ISensorsSubHal*(SensorsHalGetSubHalFunc)(uint32_t*);
typedef V2_1::implementation::ISensorsSubHal*(SensorsHalGetSubHalV2_1Func)(uint32_t*);

static constexpr int32_t kBitsAfterSubHalIndex = 24;

/**
 * Set the subhal index as first byte of sensor handle and return this modified version.
 *
 * @param sensorHandle The sensor handle to modify.
 * @param subHalIndex The index in the hal proxy of the sub hal this sensor belongs to.
 *
 * @return The modified sensor handle.
 */
int32_t setSubHalIndex(int32_t sensorHandle, size_t subHalIndex) {
    return sensorHandle | (static_cast<int32_t>(subHalIndex) << kBitsAfterSubHalIndex);
}

/**
 * Extract the subHalIndex from sensorHandle.
 *
 * @param sensorHandle The sensorHandle to extract from.
 *
 * @return The subhal index.
 */
size_t extractSubHalIndex(int32_t sensorHandle) {
    return static_cast<size_t>(sensorHandle >> kBitsAfterSubHalIndex);
}

/**
 * Convert nanoseconds to milliseconds.
 *
 * @param nanos The nanoseconds input.
 *
 * @return The milliseconds count.
 */
int64_t msFromNs(int64_t nanos) {
    constexpr int64_t nanosecondsInAMillsecond = 1000000;
    return nanos / nanosecondsInAMillsecond;
}

HalProxy::HalProxy() {
    const char* kMultiHalConfigFile = "/vendor/etc/sensors/hals.conf";
    initializeSubHalListFromConfigFile(kMultiHalConfigFile);
    init();
}

HalProxy::HalProxy(std::vector<ISensorsSubHalV2_0*>& subHalList) {
    for (ISensorsSubHalV2_0* subHal : subHalList) {
        mSubHalList.push_back(std::make_unique<SubHalWrapperV2_0>(subHal));
    }

    init();
}

HalProxy::HalProxy(std::vector<ISensorsSubHalV2_0*>& subHalList,
                   std::vector<ISensorsSubHalV2_1*>& subHalListV2_1) {
    for (ISensorsSubHalV2_0* subHal : subHalList) {
        mSubHalList.push_back(std::make_unique<SubHalWrapperV2_0>(subHal));
    }

    for (ISensorsSubHalV2_1* subHal : subHalListV2_1) {
        mSubHalList.push_back(std::make_unique<SubHalWrapperV2_1>(subHal));
    }

    init();
}

HalProxy::~HalProxy() {
    stopThreads();
}

Return<void> HalProxy::getSensorsList_2_1(ISensorsV2_1::getSensorsList_2_1_cb _hidl_cb) {
    std::vector<V2_1::SensorInfo> sensors;
    for (const auto& iter : mSensors) {
        sensors.push_back(iter.second);
    }
    _hidl_cb(sensors);
    return Void();
}

Return<void> HalProxy::getSensorsList(ISensorsV2_0::getSensorsList_cb _hidl_cb) {
    std::vector<V1_0::SensorInfo> sensors;
    for (const auto& iter : mSensors) {
        sensors.push_back(convertToOldSensorInfo(iter.second));
    }
    _hidl_cb(sensors);
    return Void();
}

Return<Result> HalProxy::setOperationMode(OperationMode mode) {
    Result result = Result::OK;
    size_t subHalIndex;
    for (subHalIndex = 0; subHalIndex < mSubHalList.size(); subHalIndex++) {
        result = mSubHalList[subHalIndex]->setOperationMode(mode);
        if (result != Result::OK) {
            ALOGE("setOperationMode failed for SubHal: %s",
                  mSubHalList[subHalIndex]->getName().c_str());
            break;
        }
    }

    if (result != Result::OK) {
        // Reset the subhal operation modes that have been flipped
        for (size_t i = 0; i < subHalIndex; i++) {
            mSubHalList[i]->setOperationMode(mCurrentOperationMode);
        }
    } else {
        mCurrentOperationMode = mode;
    }
    return result;
}

Return<Result> HalProxy::activate(int32_t sensorHandle, bool enabled) {
    if (!isSubHalIndexValid(sensorHandle)) {
        return Result::BAD_VALUE;
    }
    return getSubHalForSensorHandle(sensorHandle)
            ->activate(clearSubHalIndex(sensorHandle), enabled);
}

Return<Result> HalProxy::initialize_2_1(
        const ::android::hardware::MQDescriptorSync<V2_1::Event>& eventQueueDescriptor,
        const ::android::hardware::MQDescriptorSync<uint32_t>& wakeLockDescriptor,
        const sp<V2_1::ISensorsCallback>& sensorsCallback) {
    sp<ISensorsCallbackWrapperBase> dynamicCallback =
            new ISensorsCallbackWrapperV2_1(sensorsCallback);

    // Create the Event FMQ from the eventQueueDescriptor. Reset the read/write positions.
    auto eventQueue =
            std::make_unique<EventMessageQueueV2_1>(eventQueueDescriptor, true /* resetPointers */);
    std::unique_ptr<EventMessageQueueWrapperBase> queue =
            std::make_unique<EventMessageQueueWrapperV2_1>(eventQueue);

    return initializeCommon(queue, wakeLockDescriptor, dynamicCallback);
}

Return<Result> HalProxy::initialize(
        const ::android::hardware::MQDescriptorSync<V1_0::Event>& eventQueueDescriptor,
        const ::android::hardware::MQDescriptorSync<uint32_t>& wakeLockDescriptor,
        const sp<V2_0::ISensorsCallback>& sensorsCallback) {
    sp<ISensorsCallbackWrapperBase> dynamicCallback =
            new ISensorsCallbackWrapperV2_0(sensorsCallback);

    // Create the Event FMQ from the eventQueueDescriptor. Reset the read/write positions.
    auto eventQueue =
            std::make_unique<EventMessageQueueV2_0>(eventQueueDescriptor, true /* resetPointers */);
    std::unique_ptr<EventMessageQueueWrapperBase> queue =
            std::make_unique<EventMessageQueueWrapperV1_0>(eventQueue);

    return initializeCommon(queue, wakeLockDescriptor, dynamicCallback);
}

Return<Result> HalProxy::initializeCommon(
        std::unique_ptr<EventMessageQueueWrapperBase>& eventQueue,
        const ::android::hardware::MQDescriptorSync<uint32_t>& wakeLockDescriptor,
        const sp<ISensorsCallbackWrapperBase>& sensorsCallback) {
    Result result = Result::OK;

    stopThreads();
    resetSharedWakelock();

    // So that the pending write events queue can be cleared safely and when we start threads
    // again we do not get new events until after initialize resets the subhals.
    disableAllSensors();

    // Clears the queue if any events were pending write before.
    mPendingWriteEventsQueue = std::queue<std::pair<std::vector<V2_1::Event>, size_t>>();
    mSizePendingWriteEventsQueue = 0;

    // Clears previously connected dynamic sensors
    mDynamicSensors.clear();

    mDynamicSensorsCallback = sensorsCallback;

    // Create the Event FMQ from the eventQueueDescriptor. Reset the read/write positions.
    mEventQueue = std::move(eventQueue);

    // Create the Wake Lock FMQ that is used by the framework to communicate whenever WAKE_UP
    // events have been successfully read and handled by the framework.
    mWakeLockQueue =
            std::make_unique<WakeLockMessageQueue>(wakeLockDescriptor, true /* resetPointers */);

    if (mEventQueueFlag != nullptr) {
        EventFlag::deleteEventFlag(&mEventQueueFlag);
    }
    if (mWakelockQueueFlag != nullptr) {
        EventFlag::deleteEventFlag(&mWakelockQueueFlag);
    }
    if (EventFlag::createEventFlag(mEventQueue->getEventFlagWord(), &mEventQueueFlag) != OK) {
        result = Result::BAD_VALUE;
    }
    if (EventFlag::createEventFlag(mWakeLockQueue->getEventFlagWord(), &mWakelockQueueFlag) != OK) {
        result = Result::BAD_VALUE;
    }
    if (!mDynamicSensorsCallback || !mEventQueue || !mWakeLockQueue || mEventQueueFlag == nullptr) {
        result = Result::BAD_VALUE;
    }

    mThreadsRun.store(true);

    mPendingWritesThread = std::thread(startPendingWritesThread, this);
    mWakelockThread = std::thread(startWakelockThread, this);

    for (size_t i = 0; i < mSubHalList.size(); i++) {
        Result currRes = mSubHalList[i]->initialize(this, this, i);
        if (currRes != Result::OK) {
            result = currRes;
            ALOGE("Subhal '%s' failed to initialize.", mSubHalList[i]->getName().c_str());
            break;
        }
    }

    mCurrentOperationMode = OperationMode::NORMAL;

    return result;
}

Return<Result> HalProxy::batch(int32_t sensorHandle, int64_t samplingPeriodNs,
                               int64_t maxReportLatencyNs) {
    if (!isSubHalIndexValid(sensorHandle)) {
        return Result::BAD_VALUE;
    }
    return getSubHalForSensorHandle(sensorHandle)
            ->batch(clearSubHalIndex(sensorHandle), samplingPeriodNs, maxReportLatencyNs);
}

Return<Result> HalProxy::flush(int32_t sensorHandle) {
    if (!isSubHalIndexValid(sensorHandle)) {
        return Result::BAD_VALUE;
    }
    return getSubHalForSensorHandle(sensorHandle)->flush(clearSubHalIndex(sensorHandle));
}

Return<Result> HalProxy::injectSensorData_2_1(const V2_1::Event& event) {
    return injectSensorData(convertToOldEvent(event));
}

Return<Result> HalProxy::injectSensorData(const V1_0::Event& event) {
    Result result = Result::OK;
    if (mCurrentOperationMode == OperationMode::NORMAL &&
        event.sensorType != V1_0::SensorType::ADDITIONAL_INFO) {
        ALOGE("An event with type != ADDITIONAL_INFO passed to injectSensorData while operation"
              " mode was NORMAL.");
        result = Result::BAD_VALUE;
    }
    if (result == Result::OK) {
        V1_0::Event subHalEvent = event;
        if (!isSubHalIndexValid(event.sensorHandle)) {
            return Result::BAD_VALUE;
        }
        subHalEvent.sensorHandle = clearSubHalIndex(event.sensorHandle);
        result = getSubHalForSensorHandle(event.sensorHandle)
                         ->injectSensorData(convertToNewEvent(subHalEvent));
    }
    return result;
}

Return<void> HalProxy::registerDirectChannel(const SharedMemInfo& mem,
                                             ISensorsV2_0::registerDirectChannel_cb _hidl_cb) {
    if (mDirectChannelSubHal == nullptr) {
        _hidl_cb(Result::INVALID_OPERATION, -1 /* channelHandle */);
    } else {
        mDirectChannelSubHal->registerDirectChannel(mem, _hidl_cb);
    }
    return Return<void>();
}

Return<Result> HalProxy::unregisterDirectChannel(int32_t channelHandle) {
    Result result;
    if (mDirectChannelSubHal == nullptr) {
        result = Result::INVALID_OPERATION;
    } else {
        result = mDirectChannelSubHal->unregisterDirectChannel(channelHandle);
    }
    return result;
}

Return<void> HalProxy::configDirectReport(int32_t sensorHandle, int32_t channelHandle,
                                          RateLevel rate,
                                          ISensorsV2_0::configDirectReport_cb _hidl_cb) {
    if (mDirectChannelSubHal == nullptr) {
        _hidl_cb(Result::INVALID_OPERATION, -1 /* reportToken */);
    } else if (sensorHandle == -1 && rate != RateLevel::STOP) {
        _hidl_cb(Result::BAD_VALUE, -1 /* reportToken */);
    } else {
        // -1 denotes all sensors should be disabled
        if (sensorHandle != -1) {
            sensorHandle = clearSubHalIndex(sensorHandle);
        }
        mDirectChannelSubHal->configDirectReport(sensorHandle, channelHandle, rate, _hidl_cb);
    }
    return Return<void>();
}

Return<void> HalProxy::debug(const hidl_handle& fd, const hidl_vec<hidl_string>& /*args*/) {
    if (fd.getNativeHandle() == nullptr || fd->numFds < 1) {
        ALOGE("%s: missing fd for writing", __FUNCTION__);
        return Void();
    }

    android::base::borrowed_fd writeFd = dup(fd->data[0]);

    std::ostringstream stream;
    stream << "===HalProxy===" << std::endl;
    stream << "Internal values:" << std::endl;
    stream << "  Threads are running: " << (mThreadsRun.load() ? "true" : "false") << std::endl;
    int64_t now = getTimeNow();
    stream << "  Wakelock timeout start time: " << msFromNs(now - mWakelockTimeoutStartTime)
           << " ms ago" << std::endl;
    stream << "  Wakelock timeout reset time: " << msFromNs(now - mWakelockTimeoutResetTime)
           << " ms ago" << std::endl;
    // TODO(b/142969448): Add logging for history of wakelock acquisition per subhal.
    stream << "  Wakelock ref count: " << mWakelockRefCount << std::endl;
    stream << "  # of events on pending write writes queue: " << mSizePendingWriteEventsQueue
           << std::endl;
    stream << " Most events seen on pending write events queue: "
           << mMostEventsObservedPendingWriteEventsQueue << std::endl;
    if (!mPendingWriteEventsQueue.empty()) {
        stream << "  Size of events list on front of pending writes queue: "
               << mPendingWriteEventsQueue.front().first.size() << std::endl;
    }
    stream << "  # of non-dynamic sensors across all subhals: " << mSensors.size() << std::endl;
    stream << "  # of dynamic sensors across all subhals: " << mDynamicSensors.size() << std::endl;
    stream << "SubHals (" << mSubHalList.size() << "):" << std::endl;
    for (auto& subHal : mSubHalList) {
        stream << "  Name: " << subHal->getName() << std::endl;
        stream << "  Debug dump: " << std::endl;
        android::base::WriteStringToFd(stream.str(), writeFd);
        subHal->debug(fd, {});
        stream.str("");
        stream << std::endl;
    }
    android::base::WriteStringToFd(stream.str(), writeFd);
    return Return<void>();
}

Return<void> HalProxy::onDynamicSensorsConnected(const hidl_vec<SensorInfo>& dynamicSensorsAdded,
                                                 int32_t subHalIndex) {
    std::vector<SensorInfo> sensors;
    {
        std::lock_guard<std::mutex> lock(mDynamicSensorsMutex);
        for (SensorInfo sensor : dynamicSensorsAdded) {
            if (!subHalIndexIsClear(sensor.sensorHandle)) {
                ALOGE("Dynamic sensor added %s had sensorHandle with first byte not 0.",
                      sensor.name.c_str());
            } else {
                sensor.sensorHandle = setSubHalIndex(sensor.sensorHandle, subHalIndex);
                mDynamicSensors[sensor.sensorHandle] = sensor;
                sensors.push_back(sensor);
            }
        }
    }
    mDynamicSensorsCallback->onDynamicSensorsConnected(sensors);
    return Return<void>();
}

Return<void> HalProxy::onDynamicSensorsDisconnected(
        const hidl_vec<int32_t>& dynamicSensorHandlesRemoved, int32_t subHalIndex) {
    // TODO(b/143302327): Block this call until all pending events are flushed from queue
    std::vector<int32_t> sensorHandles;
    {
        std::lock_guard<std::mutex> lock(mDynamicSensorsMutex);
        for (int32_t sensorHandle : dynamicSensorHandlesRemoved) {
            if (!subHalIndexIsClear(sensorHandle)) {
                ALOGE("Dynamic sensorHandle removed had first byte not 0.");
            } else {
                sensorHandle = setSubHalIndex(sensorHandle, subHalIndex);
                if (mDynamicSensors.find(sensorHandle) != mDynamicSensors.end()) {
                    mDynamicSensors.erase(sensorHandle);
                    sensorHandles.push_back(sensorHandle);
                }
            }
        }
    }
    mDynamicSensorsCallback->onDynamicSensorsDisconnected(sensorHandles);
    return Return<void>();
}

void HalProxy::initializeSubHalListFromConfigFile(const char* configFileName) {
    std::ifstream subHalConfigStream(configFileName);
    if (!subHalConfigStream) {
        ALOGE("Failed to load subHal config file: %s", configFileName);
    } else {
        std::string subHalLibraryFile;
        while (subHalConfigStream >> subHalLibraryFile) {
            void* handle = getHandleForSubHalSharedObject(subHalLibraryFile);
            if (handle == nullptr) {
                ALOGE("dlopen failed for library: %s", subHalLibraryFile.c_str());
            } else {
                SensorsHalGetSubHalFunc* sensorsHalGetSubHalPtr =
                        (SensorsHalGetSubHalFunc*)dlsym(handle, "sensorsHalGetSubHal");
                if (sensorsHalGetSubHalPtr != nullptr) {
                    std::function<SensorsHalGetSubHalFunc> sensorsHalGetSubHal =
                            *sensorsHalGetSubHalPtr;
                    uint32_t version;
                    ISensorsSubHalV2_0* subHal = sensorsHalGetSubHal(&version);
                    if (version != SUB_HAL_2_0_VERSION) {
                        ALOGE("SubHal version was not 2.0 for library: %s",
                              subHalLibraryFile.c_str());
                    } else {
                        ALOGV("Loaded SubHal from library: %s", subHalLibraryFile.c_str());
                        mSubHalList.push_back(std::make_unique<SubHalWrapperV2_0>(subHal));
                    }
                } else {
                    SensorsHalGetSubHalV2_1Func* getSubHalV2_1Ptr =
                            (SensorsHalGetSubHalV2_1Func*)dlsym(handle, "sensorsHalGetSubHal_2_1");

                    if (getSubHalV2_1Ptr == nullptr) {
                        ALOGE("Failed to locate sensorsHalGetSubHal function for library: %s",
                              subHalLibraryFile.c_str());
                    } else {
                        std::function<SensorsHalGetSubHalV2_1Func> sensorsHalGetSubHal_2_1 =
                                *getSubHalV2_1Ptr;
                        uint32_t version;
                        ISensorsSubHalV2_1* subHal = sensorsHalGetSubHal_2_1(&version);
                        if (version != SUB_HAL_2_1_VERSION) {
                            ALOGE("SubHal version was not 2.1 for library: %s",
                                  subHalLibraryFile.c_str());
                        } else {
                            ALOGV("Loaded SubHal from library: %s", subHalLibraryFile.c_str());
                            mSubHalList.push_back(std::make_unique<SubHalWrapperV2_1>(subHal));
                        }
                    }
                }
            }
        }
    }
}

void HalProxy::initializeSensorList() {
    for (size_t subHalIndex = 0; subHalIndex < mSubHalList.size(); subHalIndex++) {
        auto result = mSubHalList[subHalIndex]->getSensorsList([&](const auto& list) {
            for (SensorInfo sensor : list) {
                if (!subHalIndexIsClear(sensor.sensorHandle)) {
                    ALOGE("SubHal sensorHandle's first byte was not 0");
                } else {
                    ALOGV("Loaded sensor: %s", sensor.name.c_str());
                    sensor.sensorHandle = setSubHalIndex(sensor.sensorHandle, subHalIndex);
                    setDirectChannelFlags(&sensor, mSubHalList[subHalIndex]);
                    mSensors[sensor.sensorHandle] = sensor;
                }
            }
        });
        if (!result.isOk()) {
            ALOGE("getSensorsList call failed for SubHal: %s",
                  mSubHalList[subHalIndex]->getName().c_str());
        }
    }
}

void* HalProxy::getHandleForSubHalSharedObject(const std::string& filename) {
    static const std::string kSubHalShareObjectLocations[] = {
            "",  // Default locations will be searched
#ifdef __LP64__
            "/vendor/lib64/hw/", "/odm/lib64/hw/"
#else
            "/vendor/lib/hw/", "/odm/lib/hw/"
#endif
    };

    for (const std::string& dir : kSubHalShareObjectLocations) {
        void* handle = dlopen((dir + filename).c_str(), RTLD_NOW);
        if (handle != nullptr) {
            return handle;
        }
    }
    return nullptr;
}

void HalProxy::init() {
    initializeSensorList();
}

void HalProxy::stopThreads() {
    mThreadsRun.store(false);
    if (mEventQueueFlag != nullptr && mEventQueue != nullptr) {
        size_t numToRead = mEventQueue->availableToRead();
        std::vector<Event> events(numToRead);
        mEventQueue->read(events.data(), numToRead);
        mEventQueueFlag->wake(static_cast<uint32_t>(EventQueueFlagBits::EVENTS_READ));
    }
    if (mWakelockQueueFlag != nullptr && mWakeLockQueue != nullptr) {
        uint32_t kZero = 0;
        mWakeLockQueue->write(&kZero);
        mWakelockQueueFlag->wake(static_cast<uint32_t>(WakeLockQueueFlagBits::DATA_WRITTEN));
    }
    mWakelockCV.notify_one();
    mEventQueueWriteCV.notify_one();
    if (mPendingWritesThread.joinable()) {
        mPendingWritesThread.join();
    }
    if (mWakelockThread.joinable()) {
        mWakelockThread.join();
    }
}

void HalProxy::disableAllSensors() {
    for (const auto& sensorEntry : mSensors) {
        int32_t sensorHandle = sensorEntry.first;
        activate(sensorHandle, false /* enabled */);
    }
    std::lock_guard<std::mutex> dynamicSensorsLock(mDynamicSensorsMutex);
    for (const auto& sensorEntry : mDynamicSensors) {
        int32_t sensorHandle = sensorEntry.first;
        activate(sensorHandle, false /* enabled */);
    }
}

void HalProxy::startPendingWritesThread(HalProxy* halProxy) {
    halProxy->handlePendingWrites();
}

void HalProxy::handlePendingWrites() {
    // TODO(b/143302327): Find a way to optimize locking strategy maybe using two mutexes instead of
    // one.
    std::unique_lock<std::mutex> lock(mEventQueueWriteMutex);
    while (mThreadsRun.load()) {
        mEventQueueWriteCV.wait(
                lock, [&] { return !mPendingWriteEventsQueue.empty() || !mThreadsRun.load(); });
        if (mThreadsRun.load()) {
            std::vector<Event>& pendingWriteEvents = mPendingWriteEventsQueue.front().first;
            size_t numWakeupEvents = mPendingWriteEventsQueue.front().second;
            size_t eventQueueSize = mEventQueue->getQuantumCount();
            size_t numToWrite = std::min(pendingWriteEvents.size(), eventQueueSize);
            lock.unlock();
            if (!mEventQueue->writeBlocking(
                        pendingWriteEvents.data(), numToWrite,
                        static_cast<uint32_t>(EventQueueFlagBits::EVENTS_READ),
                        static_cast<uint32_t>(EventQueueFlagBits::READ_AND_PROCESS),
                        kPendingWriteTimeoutNs, mEventQueueFlag)) {
                ALOGE("Dropping %zu events after blockingWrite failed.", numToWrite);
                if (numWakeupEvents > 0) {
                    if (pendingWriteEvents.size() > eventQueueSize) {
                        decrementRefCountAndMaybeReleaseWakelock(
                                countNumWakeupEvents(pendingWriteEvents, eventQueueSize));
                    } else {
                        decrementRefCountAndMaybeReleaseWakelock(numWakeupEvents);
                    }
                }
            }
            lock.lock();
            mSizePendingWriteEventsQueue -= numToWrite;
            if (pendingWriteEvents.size() > eventQueueSize) {
                // TODO(b/143302327): Check if this erase operation is too inefficient. It will copy
                // all the events ahead of it down to fill gap off array at front after the erase.
                pendingWriteEvents.erase(pendingWriteEvents.begin(),
                                         pendingWriteEvents.begin() + eventQueueSize);
            } else {
                mPendingWriteEventsQueue.pop();
            }
        }
    }
}

void HalProxy::startWakelockThread(HalProxy* halProxy) {
    halProxy->handleWakelocks();
}

void HalProxy::handleWakelocks() {
    std::unique_lock<std::recursive_mutex> lock(mWakelockMutex);
    while (mThreadsRun.load()) {
        mWakelockCV.wait(lock, [&] { return mWakelockRefCount > 0 || !mThreadsRun.load(); });
        if (mThreadsRun.load()) {
            int64_t timeLeft;
            if (sharedWakelockDidTimeout(&timeLeft)) {
                resetSharedWakelock();
            } else {
                uint32_t numWakeLocksProcessed;
                lock.unlock();
                bool success = mWakeLockQueue->readBlocking(
                        &numWakeLocksProcessed, 1, 0,
                        static_cast<uint32_t>(WakeLockQueueFlagBits::DATA_WRITTEN), timeLeft);
                lock.lock();
                if (success) {
                    decrementRefCountAndMaybeReleaseWakelock(
                            static_cast<size_t>(numWakeLocksProcessed));
                }
            }
        }
    }
    resetSharedWakelock();
}

bool HalProxy::sharedWakelockDidTimeout(int64_t* timeLeft) {
    bool didTimeout;
    int64_t duration = getTimeNow() - mWakelockTimeoutStartTime;
    if (duration > kWakelockTimeoutNs) {
        didTimeout = true;
    } else {
        didTimeout = false;
        *timeLeft = kWakelockTimeoutNs - duration;
    }
    return didTimeout;
}

void HalProxy::resetSharedWakelock() {
    std::lock_guard<std::recursive_mutex> lockGuard(mWakelockMutex);
    decrementRefCountAndMaybeReleaseWakelock(mWakelockRefCount);
    mWakelockTimeoutResetTime = getTimeNow();
}

void HalProxy::postEventsToMessageQueue(const std::vector<Event>& events, size_t numWakeupEvents,
                                        V2_0::implementation::ScopedWakelock wakelock) {
    size_t numToWrite = 0;
    std::lock_guard<std::mutex> lock(mEventQueueWriteMutex);
    if (wakelock.isLocked()) {
        incrementRefCountAndMaybeAcquireWakelock(numWakeupEvents);
    }
    if (mPendingWriteEventsQueue.empty()) {
        numToWrite = std::min(events.size(), mEventQueue->availableToWrite());
        if (numToWrite > 0) {
            if (mEventQueue->write(events.data(), numToWrite)) {
                // TODO(b/143302327): While loop if mEventQueue->avaiableToWrite > 0 to possibly fit
                // in more writes immediately
                mEventQueueFlag->wake(static_cast<uint32_t>(EventQueueFlagBits::READ_AND_PROCESS));
            } else {
                numToWrite = 0;
            }
        }
    }
    size_t numLeft = events.size() - numToWrite;
    if (numToWrite < events.size() &&
        mSizePendingWriteEventsQueue + numLeft <= kMaxSizePendingWriteEventsQueue) {
        std::vector<Event> eventsLeft(events.begin() + numToWrite, events.end());
        mPendingWriteEventsQueue.push({eventsLeft, numWakeupEvents});
        mSizePendingWriteEventsQueue += numLeft;
        mMostEventsObservedPendingWriteEventsQueue =
                std::max(mMostEventsObservedPendingWriteEventsQueue, mSizePendingWriteEventsQueue);
        mEventQueueWriteCV.notify_one();
    }
}

bool HalProxy::incrementRefCountAndMaybeAcquireWakelock(size_t delta,
                                                        int64_t* timeoutStart /* = nullptr */) {
    if (!mThreadsRun.load()) return false;
    std::lock_guard<std::recursive_mutex> lockGuard(mWakelockMutex);
    if (mWakelockRefCount == 0) {
        acquire_wake_lock(PARTIAL_WAKE_LOCK, kWakelockName);
        mWakelockCV.notify_one();
    }
    mWakelockTimeoutStartTime = getTimeNow();
    mWakelockRefCount += delta;
    if (timeoutStart != nullptr) {
        *timeoutStart = mWakelockTimeoutStartTime;
    }
    return true;
}

void HalProxy::decrementRefCountAndMaybeReleaseWakelock(size_t delta,
                                                        int64_t timeoutStart /* = -1 */) {
    if (!mThreadsRun.load()) return;
    std::lock_guard<std::recursive_mutex> lockGuard(mWakelockMutex);
    if (delta > mWakelockRefCount) {
        ALOGE("Decrementing wakelock ref count by %zu when count is %zu",
              delta, mWakelockRefCount);
    }
    if (timeoutStart == -1) timeoutStart = mWakelockTimeoutResetTime;
    if (mWakelockRefCount == 0 || timeoutStart < mWakelockTimeoutResetTime) return;
    mWakelockRefCount -= std::min(mWakelockRefCount, delta);
    if (mWakelockRefCount == 0) {
        release_wake_lock(kWakelockName);
    }
}

void HalProxy::setDirectChannelFlags(SensorInfo* sensorInfo,
                                     std::shared_ptr<ISubHalWrapperBase> subHal) {
    bool sensorSupportsDirectChannel =
            (sensorInfo->flags & (V1_0::SensorFlagBits::MASK_DIRECT_REPORT |
                                  V1_0::SensorFlagBits::MASK_DIRECT_CHANNEL)) != 0;
    if (mDirectChannelSubHal == nullptr && sensorSupportsDirectChannel) {
        mDirectChannelSubHal = subHal;
    } else if (mDirectChannelSubHal != nullptr && subHal != mDirectChannelSubHal) {
        // disable direct channel capability for sensors in subHals that are not
        // the only one we will enable
        sensorInfo->flags &= ~(V1_0::SensorFlagBits::MASK_DIRECT_REPORT |
                               V1_0::SensorFlagBits::MASK_DIRECT_CHANNEL);
    }
}

std::shared_ptr<ISubHalWrapperBase> HalProxy::getSubHalForSensorHandle(int32_t sensorHandle) {
    return mSubHalList[extractSubHalIndex(sensorHandle)];
}

bool HalProxy::isSubHalIndexValid(int32_t sensorHandle) {
    return extractSubHalIndex(sensorHandle) < mSubHalList.size();
}

size_t HalProxy::countNumWakeupEvents(const std::vector<Event>& events, size_t n) {
    size_t numWakeupEvents = 0;
    for (size_t i = 0; i < n; i++) {
        int32_t sensorHandle = events[i].sensorHandle;
        if (mSensors[sensorHandle].flags & static_cast<uint32_t>(V1_0::SensorFlagBits::WAKE_UP)) {
            numWakeupEvents++;
        }
    }
    return numWakeupEvents;
}

int32_t HalProxy::clearSubHalIndex(int32_t sensorHandle) {
    return sensorHandle & (~kSensorHandleSubHalIndexMask);
}

bool HalProxy::subHalIndexIsClear(int32_t sensorHandle) {
    return (sensorHandle & kSensorHandleSubHalIndexMask) == 0;
}

}  // namespace implementation
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android
