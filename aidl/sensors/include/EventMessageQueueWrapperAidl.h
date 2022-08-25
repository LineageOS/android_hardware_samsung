/*
 * Copyright (C) 2021 The Android Open Source Project
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

#pragma once

#include <android/hardware/sensors/2.1/types.h>
#include <fmq/AidlMessageQueue.h>
#include "ConvertUtils.h"
#include "EventMessageQueueWrapper.h"
#include "ISensorsWrapper.h"

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
namespace implementation {

class EventMessageQueueWrapperAidl
    : public ::android::hardware::sensors::V2_1::implementation::EventMessageQueueWrapperBase {
  public:
    EventMessageQueueWrapperAidl(
            std::unique_ptr<::android::AidlMessageQueue<
                    ::aidl::android::hardware::sensors::Event,
                    ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>>& queue)
        : mQueue(std::move(queue)) {}

    virtual std::atomic<uint32_t>* getEventFlagWord() override {
        return mQueue->getEventFlagWord();
    }

    virtual size_t availableToRead() override { return mQueue->availableToRead(); }

    size_t availableToWrite() override { return mQueue->availableToWrite(); }

    virtual bool read(::android::hardware::sensors::V2_1::Event* events,
                      size_t numToRead) override {
        bool success = mQueue->read(mIntermediateEventBuffer.data(), numToRead);
        for (int i = 0; i < numToRead; ++i) {
            convertToHidlEvent(mIntermediateEventBuffer[i], &events[i]);
        }
        return success;
    }

    bool write(const ::android::hardware::sensors::V2_1::Event* events,
               size_t numToWrite) override {
        for (int i = 0; i < numToWrite; ++i) {
            convertToAidlEvent(events[i], &mIntermediateEventBuffer[i]);
        }
        return mQueue->write(mIntermediateEventBuffer.data(), numToWrite);
    }

    virtual bool write(
            const std::vector<::android::hardware::sensors::V2_1::Event>& events) override {
        for (int i = 0; i < events.size(); ++i) {
            convertToAidlEvent(events[i], &mIntermediateEventBuffer[i]);
        }
        return mQueue->write(mIntermediateEventBuffer.data(), events.size());
    }

    bool writeBlocking(const ::android::hardware::sensors::V2_1::Event* events, size_t count,
                       uint32_t readNotification, uint32_t writeNotification, int64_t timeOutNanos,
                       ::android::hardware::EventFlag* evFlag) override {
        for (int i = 0; i < count; ++i) {
            convertToAidlEvent(events[i], &mIntermediateEventBuffer[i]);
        }
        return mQueue->writeBlocking(mIntermediateEventBuffer.data(), count, readNotification,
                                     writeNotification, timeOutNanos, evFlag);
    }

    size_t getQuantumCount() override { return mQueue->getQuantumCount(); }

  private:
    std::unique_ptr<::android::AidlMessageQueue<
            ::aidl::android::hardware::sensors::Event,
            ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>>
            mQueue;
    std::array<::aidl::android::hardware::sensors::Event,
               ::android::hardware::sensors::V2_1::implementation::MAX_RECEIVE_BUFFER_EVENT_COUNT>
            mIntermediateEventBuffer;
};

}  // namespace implementation
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
