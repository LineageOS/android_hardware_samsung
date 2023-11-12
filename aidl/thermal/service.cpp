/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "Thermal.h"

constexpr std::string_view kThermalLogTag("pixel-thermal");

using ::android::OK;
using ::android::status_t;

// Generated AIDL files:
using Thermal = ::aidl::android::hardware::thermal::implementation::Thermal;

#if !defined(THERMAL_INSTANCE_NAME)
#define THERMAL_INSTANCE_NAME "default"
#endif

int main(int /* argc */, char ** /* argv */) {
    android::base::SetDefaultTag(kThermalLogTag.data());

    auto svc = ndk::SharedRefBase::make<Thermal>();
    const auto svcName = std::string() + svc->descriptor + "/" + THERMAL_INSTANCE_NAME;
    LOG(INFO) << "Pixel Thermal AIDL Service starting..." + svcName;
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    auto svcBinder = svc->asBinder();
    binder_status_t status = AServiceManager_addService(svcBinder.get(), svcName.c_str());
    if (status != STATUS_OK) {
        LOG(ERROR) << "Pixel Thermal AIDL Service failed to start: " << status << ".";
        return EXIT_FAILURE;
    }
    LOG(INFO) << "Pixel Thermal HAL AIDL Service started.";
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
