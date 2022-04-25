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

#define LOG_TAG "fastcharge@1.1-service.samsung"

#include <android-base/logging.h>
#include <binder/ProcessState.h>
#include <hidl/HidlTransportSupport.h>

#include "FastCharge.h"
#include "SuperFastCharge.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

using vendor::lineage::fastcharge::V1_1::IFastCharge;
using vendor::lineage::fastcharge::V1_1::implementation::FastCharge;
using vendor::lineage::fastcharge::V1_1::ISuperFastCharge;
using vendor::lineage::fastcharge::V1_1::implementation::SuperFastCharge;

using android::sp;
using android::OK;
using android::status_t;

int main() {
    sp<FastCharge> fastCharge;
    sp<SuperFastCharge> superFastCharge;
    status_t status;
    
    fastCharge = new FastCharge();
    if (fastCharge == nullptr) {
        LOG(ERROR)
            << "Can not create an instance of FastCharge HAL FastCharge Iface, exiting.";
        goto shutdown;
    } 

    superFastCharge = new SuperFastCharge();
    if (superFastCharge == nullptr) {
        LOG(ERROR)
            << "Can not create an instance of SuperFastCharge HAL FastCharge Iface, exiting.";
        goto shutdown;
    } 

    configureRpcThreadpool(1, true);

    status = fastCharge->registerAsService();
    if (status != OK) {
        LOG(ERROR)
            << "Could not register service for FastCharge HAL FastCharge Iface ("
            << status << ")";
        goto shutdown;
    }

    status = superFastCharge->registerAsService();
    if (status != OK) {
        LOG(ERROR)
            << "Could not register service for FastCharge HAL SuperFastCharge Iface ("
            << status << ")";
        goto shutdown;
    }

    LOG(INFO) << "FastCharge HAL service ready.";
    joinRpcThreadpool();

shutdown:
    LOG(ERROR) << "FastCharge HAL service failed to join thread pool.";
    return 1;
}
