/*
 * Copyright 2019 The LineageOS Project
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

#define LOG_TAG "android.hardware.keymaster@4.0-service.samsung"

#include <android-base/logging.h>
#include <android/hardware/keymaster/4.0/IKeymasterDevice.h>
#include <hidl/HidlTransportSupport.h>

#include <AndroidKeymaster4Device.h>

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

using android::hardware::keymaster::V4_0::IKeymasterDevice;
using android::hardware::keymaster::V4_0::SecurityLevel;

using android::OK;
using android::status_t;

namespace skeymaster {
IKeymasterDevice* CreateSKeymasterDevice(SecurityLevel securityLevel);
}  // namespace skeymaster

int main() {
    IKeymasterDevice* keymaster =
            skeymaster::CreateSKeymasterDevice(SecurityLevel::TRUSTED_ENVIRONMENT);

    configureRpcThreadpool(1, true);

    status_t status = keymaster->registerAsService();

    if (status != OK) {
        LOG(ERROR) << "Could not register service for Keymaster HAL";
        goto shutdown;
    }

    LOG(INFO) << "Keymaster HAL service is Ready.";
    joinRpcThreadpool();

shutdown:
    // In normal operation, we don't expect the thread pool to shutdown
    LOG(ERROR) << "Keymaster HAL failed to join thread pool.";
    return -1;
}
