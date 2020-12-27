/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "android.hardware.power-service.pixel-libperfmgr"

#include <thread>

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "Power.h"
#include "PowerExt.h"
#include "disp-power/DisplayLowPower.h"

using aidl::google::hardware::power::impl::pixel::Power;
using aidl::google::hardware::power::impl::pixel::PowerExt;
using ::android::perfmgr::HintManager;

constexpr char kPowerHalConfigPath[] = "/vendor/etc/powerhint.json";
constexpr char kPowerHalInitProp[] = "vendor.powerhal.init";

int main() {
    LOG(INFO) << "Pixel Power HAL AIDL Service with Extension is starting.";

    // Parse config but do not start the looper
    std::shared_ptr<HintManager> hm = HintManager::GetFromJSON(kPowerHalConfigPath, false);
    if (!hm) {
        LOG(FATAL) << "Invalid config: " << kPowerHalConfigPath;
    }

    std::shared_ptr<DisplayLowPower> dlpw = std::make_shared<DisplayLowPower>();

    // single thread
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    // core service
    std::shared_ptr<Power> pw = ndk::SharedRefBase::make<Power>(hm, dlpw);
    ndk::SpAIBinder pwBinder = pw->asBinder();

    // extension service
    std::shared_ptr<PowerExt> pwExt = ndk::SharedRefBase::make<PowerExt>(hm, dlpw);

    // attach the extension to the same binder we will be registering
    CHECK(STATUS_OK == AIBinder_setExtension(pwBinder.get(), pwExt->asBinder().get()));

    const std::string instance = std::string() + Power::descriptor + "/default";
    binder_status_t status = AServiceManager_addService(pw->asBinder().get(), instance.c_str());
    CHECK(status == STATUS_OK);
    LOG(INFO) << "Pixel Power HAL AIDL Service with Extension is started.";

    std::thread initThread([&]() {
        ::android::base::WaitForProperty(kPowerHalInitProp, "1");
        hm->Start();
        dlpw->Init();
    });
    initThread.detach();

    ABinderProcess_joinThreadPool();

    // should not reach
    LOG(ERROR) << "Pixel Power HAL AIDL Service with Extension just died.";
    return EXIT_FAILURE;
}
