/*
 * Copyright (C) 2018 The LineageOS Project
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

#define LOG_TAG "android.hardware.power@1.0-service.samsung"

#include <android/log.h>
#include <hidl/HidlTransportSupport.h>
#include <hardware/power.h>
#include "Power.h"

using android::sp;
using android::status_t;
using android::OK;

// libhwbinder:
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

// Generated HIDL files
using android::hardware::power::V1_0::implementation::Power;

int main() {
    status_t status;
    android::sp<Power> service = nullptr;

    ALOGI("Power HAL Service 1.0 for Samsung is starting.");

    service = new Power();
    if (service == nullptr) {
        ALOGE("Can not create an instance of Power HAL Iface, exiting.");

        goto shutdown;
    }

    configureRpcThreadpool(1, true /*callerWillJoin*/);

    status = service->registerAsSystemService();
    if (status != OK) {
        ALOGE("Could not register service for Power HAL Iface (%d).", status);
        goto shutdown;
    }

    ALOGI("Power Service is ready");
    joinRpcThreadpool();
    //Should not pass this line

shutdown:
    // In normal operation, we don't expect the thread pool to exit

    ALOGE("Power Service is shutting down");
    return 1;
}
