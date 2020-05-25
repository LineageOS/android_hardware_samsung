/*
 * Copyright (C) 2020 The LineageOS Project
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

#define LOG_TAG "vibrator@1.3-samsung"

#include <android-base/logging.h>
#include <android/hardware/vibrator/1.3/IVibrator.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>
#include <utils/Errors.h>
#include <utils/StrongPointer.h>

#include "Vibrator.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::vibrator::V1_3::IVibrator;
using android::hardware::vibrator::V1_3::implementation::Vibrator;

using android::OK;
using android::sp;
using android::status_t;

int main() {
    status_t status;
    sp<IVibrator> vibrator;

    LOG(INFO) << "Vibrator HAL service is starting.";

    vibrator = new Vibrator();
    if (vibrator == nullptr) {
        LOG(ERROR) << "Can not create an instance of Vibrator HAL IVibrator, "
                      "exiting.";
        goto shutdown;
    }

    configureRpcThreadpool(1, true);

    status = vibrator->registerAsService();
    if (status != OK) {
        LOG(ERROR) << "Could not register service for Vibrator HAL";
        goto shutdown;
    }

    LOG(INFO) << "Vibrator HAL service is Ready.";
    joinRpcThreadpool();

shutdown:
    // In normal operation, we don't expect the thread pool to shutdown
    LOG(ERROR) << "Vibrator HAL failed to join thread pool.";
    return 1;
}
