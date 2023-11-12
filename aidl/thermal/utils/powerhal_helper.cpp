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

#include "powerhal_helper.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android/binder_manager.h>

#include <iterator>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#include "thermal_info.h"
#include "thermal_throttling.h"

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

using ::android::base::StringPrintf;

PowerHalService::PowerHalService()
    : power_hal_aidl_exist_(true), power_hal_aidl_(nullptr), power_hal_ext_aidl_(nullptr) {
    connect();
}

bool PowerHalService::connect() {
    std::lock_guard<std::mutex> lock(lock_);

    if (!power_hal_aidl_exist_) {
        return false;
    }

    if (power_hal_aidl_ && power_hal_ext_aidl_) {
        return true;
    }

    const std::string kInstance = std::string(IPower::descriptor) + "/default";
    ndk::SpAIBinder power_binder = ndk::SpAIBinder(AServiceManager_getService(kInstance.c_str()));
    ndk::SpAIBinder ext_power_binder;

    if (power_binder.get() == nullptr) {
        LOG(ERROR) << "Cannot get Power Hal Binder";
        power_hal_aidl_exist_ = false;
        return false;
    }

    power_hal_aidl_ = IPower::fromBinder(power_binder);

    if (power_hal_aidl_ == nullptr) {
        power_hal_aidl_exist_ = false;
        LOG(ERROR) << "Cannot get Power Hal AIDL" << kInstance.c_str();
        return false;
    }

    if (STATUS_OK != AIBinder_getExtension(power_binder.get(), ext_power_binder.getR()) ||
        ext_power_binder.get() == nullptr) {
        LOG(ERROR) << "Cannot get Power Hal Extension Binder";
        power_hal_aidl_exist_ = false;
        return false;
    }

    power_hal_ext_aidl_ = IPowerExt::fromBinder(ext_power_binder);
    if (power_hal_ext_aidl_ == nullptr) {
        LOG(ERROR) << "Cannot get Power Hal Extension AIDL";
        power_hal_aidl_exist_ = false;
    }

    return true;
}

bool PowerHalService::isModeSupported(const std::string &type, const ThrottlingSeverity &t) {
    bool isSupported = false;
    if (!connect()) {
        return false;
    }
    std::string power_hint = StringPrintf("THERMAL_%s_%s", type.c_str(), toString(t).c_str());
    lock_.lock();
    if (!power_hal_ext_aidl_->isModeSupported(power_hint, &isSupported).isOk()) {
        LOG(ERROR) << "Fail to check supported mode, Hint: " << power_hint;
        power_hal_ext_aidl_ = nullptr;
        power_hal_aidl_ = nullptr;
        lock_.unlock();
        return false;
    }
    lock_.unlock();
    return isSupported;
}

void PowerHalService::setMode(const std::string &type, const ThrottlingSeverity &t,
                              const bool &enable, const bool error_on_exit) {
    if (!connect()) {
        return;
    }

    std::string power_hint = StringPrintf("THERMAL_%s_%s", type.c_str(), toString(t).c_str());
    LOG(INFO) << (error_on_exit ? "Resend Hint " : "Send Hint ") << power_hint
              << " Enable: " << std::boolalpha << enable;
    lock_.lock();
    if (!power_hal_ext_aidl_->setMode(power_hint, enable).isOk()) {
        LOG(ERROR) << "Fail to set mode, Hint: " << power_hint;
        power_hal_ext_aidl_ = nullptr;
        power_hal_aidl_ = nullptr;
        lock_.unlock();
        if (!error_on_exit) {
            setMode(type, t, enable, true);
        }
        return;
    }
    lock_.unlock();
}

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
