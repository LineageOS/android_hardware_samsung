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

#define LOG_TAG "android.hardware.power@1.0-service.exynos"

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>
#include <utils/Errors.h>

#include "Power.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

using android::hardware::power::V1_0::IPower;
using android::hardware::power::V1_0::implementation::Power;

using android::OK;
using android::sp;
using android::status_t;

int main() {
    sp<Power> power = new Power();
    status_t status = 0;

    configureRpcThreadpool(1, true);

    status = power->IPower::registerAsService();
    if (status != OK) {
        LOG(ERROR) << "Could not register service (IPower) for Power HAL";
        goto shutdown;
    }

    status = power->ILineagePower::registerAsService();
    if (status != OK) {
        LOG(ERROR) << "Could not register service (ILineagePower) for Power HAL";
        goto shutdown;
    }

    LOG(INFO) << "Power HAL service is Ready.";
    joinRpcThreadpool();

shutdown:
    // In normal operation, we don't expect the thread pool to shutdown
    LOG(ERROR) << "Power HAL failed to join thread pool.";
    return 1;
}
