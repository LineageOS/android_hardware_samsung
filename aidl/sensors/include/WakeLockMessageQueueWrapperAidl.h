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
#include "WakeLockMessageQueueWrapper.h"

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
namespace implementation {

class WakeLockMessageQueueWrapperAidl
    : public ::android::hardware::sensors::V2_1::implementation::WakeLockMessageQueueWrapperBase {
  public:
    WakeLockMessageQueueWrapperAidl(
            std::unique_ptr<::android::AidlMessageQueue<
                    int32_t, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>>& queue)
        : mQueue(std::move(queue)) {}

    virtual std::atomic<uint32_t>* getEventFlagWord() override {
        return mQueue->getEventFlagWord();
    }

    bool readBlocking(uint32_t* wakeLocks, size_t numToRead, uint32_t readNotification,
                      uint32_t writeNotification, int64_t timeOutNanos,
                      ::android::hardware::EventFlag* evFlag) override {
        return mQueue->readBlocking(reinterpret_cast<int32_t*>(wakeLocks), numToRead,
                                    readNotification, writeNotification, timeOutNanos, evFlag);
    }

    bool write(const uint32_t* wakeLock) override {
        return mQueue->write(reinterpret_cast<const int32_t*>(wakeLock));
    }

  private:
    std::unique_ptr<::android::AidlMessageQueue<
            int32_t, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>>
            mQueue;
};

}  // namespace implementation
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
