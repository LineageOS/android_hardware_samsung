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

#define LOG_TAG "android.hardware.ir@1.0-service.samsung"

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>
#include <utils/Errors.h>

#include "ConsumerIr.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

using android::hardware::ir::V1_0::IConsumerIr;
using android::hardware::ir::V1_0::implementation::ConsumerIr;

using android::OK;
using android::sp;
using android::status_t;

int main() {
    sp<IConsumerIr> ir = new ConsumerIr();

    configureRpcThreadpool(1, true);

    status_t status = ir->registerAsService();

    if (status != OK) {
        LOG(ERROR) << "Could not register service for IR HAL";
        goto shutdown;
    }

    LOG(INFO) << "IR HAL service is Ready.";
    joinRpcThreadpool();

shutdown:
    // In normal operation, we don't expect the thread pool to shutdown
    LOG(ERROR) << "IR HAL failed to join thread pool.";
    return 1;
}
