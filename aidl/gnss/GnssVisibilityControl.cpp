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

#define LOG_TAG "GnssVisibilityControl"

#include "GnssVisibilityControl.h"
#include <log/log.h>

namespace aidl::android::hardware::gnss::visibility_control {

std::shared_ptr<IGnssVisibilityControlCallback> GnssVisibilityControl::sCallback = nullptr;

ndk::ScopedAStatus GnssVisibilityControl::enableNfwLocationAccess(
        const std::vector<std::string>& proxyApps) {
    std::string os;
    bool first = true;
    for (const auto& proxyApp : proxyApps) {
        if (first) {
            first = false;
        } else {
            os += " ";
        }
        os += proxyApp;
    }

    ALOGD("GnssVisibilityControl::enableNfwLocationAccess proxyApps: %s", os.c_str());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssVisibilityControl::setCallback(
        const std::shared_ptr<IGnssVisibilityControlCallback>& callback) {
    ALOGD("GnssVisibilityControl::setCallback");
    std::unique_lock<std::mutex> lock(mMutex);
    sCallback = callback;
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss::visibility_control
