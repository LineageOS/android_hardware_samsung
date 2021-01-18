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

#define LOG_TAG "fastcharge@1.0-service.samsung"

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>

#include "FastCharge.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

using vendor::lineage::fastcharge::V1_0::IFastCharge;
using vendor::lineage::fastcharge::V1_0::implementation::FastCharge;

using android::OK;
using android::status_t;

int main() {
    android::sp<FastCharge> service = new FastCharge();

    configureRpcThreadpool(1, true);

    status_t status = service->registerAsService();
    if (status != OK) {
        LOG(ERROR) << "Cannot register FastCharge HAL service.";
        return 1;
    }

    LOG(INFO) << "FastCharge HAL service ready.";

    joinRpcThreadpool();

    LOG(ERROR) << "FastCharge HAL service failed to join thread pool.";
    return 1;
}
