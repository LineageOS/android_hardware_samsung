/*
 * Copyright (C) 2016 The Android Open Source Project
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
#define LOG_TAG "android.hardware.gatekeeper-service.nonsecure"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <cutils/properties.h>

#include "GateKeeper.h"
#include "SharedSecret.h"
#include "SoftGateKeeper.h"

using aidl::android::hardware::gatekeeper::SoftGateKeeperDevice;
using aidl::android::hardware::security::sharedsecret::SoftSharedSecret;

int main(int, char** argv) {
    ::android::base::InitLogging(argv, ::android::base::KernelLogger);
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    auto secret = ndk::SharedRefBase::make<SoftSharedSecret>();
    std::string secret_instance = SoftSharedSecret::descriptor + std::string("/gatekeeper");
    auto status = AServiceManager_addService(secret->asBinder().get(), secret_instance.c_str());
    CHECK_EQ(status, STATUS_OK);

    ::gatekeeper::SoftGateKeeper implementation(*secret);
    auto gatekeeper = ndk::SharedRefBase::make<SoftGateKeeperDevice>(implementation);
    const std::string instance = SoftGateKeeperDevice::descriptor + std::string("/default");
    status = AServiceManager_addService(gatekeeper->asBinder().get(), instance.c_str());
    CHECK_EQ(status, STATUS_OK);

    ABinderProcess_joinThreadPool();
    return -1;  // Should never get here.
}
