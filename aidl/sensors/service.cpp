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

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include "HalProxyAidl.h"

using ::aidl::android::hardware::sensors::implementation::HalProxyAidl;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    // Make a default multihal sensors service
    auto halProxy = ndk::SharedRefBase::make<HalProxyAidl>();
    const std::string halProxyName = std::string() + HalProxyAidl::descriptor + "/default";
    binder_status_t status =
            AServiceManager_addService(halProxy->asBinder().get(), halProxyName.c_str());
    CHECK_EQ(status, STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
