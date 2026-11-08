/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "GnssPsdsAidl"

#include "GnssPsds.h"
#include <aidl/android/hardware/gnss/BnGnss.h>
#include <log/log.h>

namespace aidl::android::hardware::gnss {

std::shared_ptr<IGnssPsdsCallback> GnssPsds::sCallback = nullptr;

ndk::ScopedAStatus GnssPsds::setCallback(const std::shared_ptr<IGnssPsdsCallback>& callback) {
    ALOGD("setCallback");
    std::unique_lock<std::mutex> lock(mMutex);
    sCallback = callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssPsds::injectPsdsData(PsdsType psdsType,
                                            const std::vector<uint8_t>& psdsData) {
    ALOGD("injectPsdsData. psdsType: %d, psdsData: %d bytes", static_cast<int>(psdsType),
          static_cast<int>(psdsData.size()));
    if (psdsData.size() > 0) {
        return ndk::ScopedAStatus::ok();
    } else {
        return ndk::ScopedAStatus::fromServiceSpecificError(IGnss::ERROR_INVALID_ARGUMENT);
    }
}
}  // namespace aidl::android::hardware::gnss
