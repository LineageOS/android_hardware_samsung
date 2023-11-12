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

#pragma once

#include <aidl/android/hardware/power/IPower.h>
#include <aidl/android/hardware/thermal/ThrottlingSeverity.h>
#include <aidl/google/hardware/power/extension/pixel/IPowerExt.h>

#include <queue>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

using ::aidl::android::hardware::power::IPower;
using ::aidl::google::hardware::power::extension::pixel::IPowerExt;

using CdevRequestStatus = std::unordered_map<std::string, int>;

class PowerHalService {
  public:
    PowerHalService();
    ~PowerHalService() = default;
    bool connect();
    bool isAidlPowerHalExist() { return power_hal_aidl_exist_; }
    bool isModeSupported(const std::string &type, const ThrottlingSeverity &t);
    bool isPowerHalConnected() { return power_hal_aidl_ != nullptr; }
    bool isPowerHalExtConnected() { return power_hal_ext_aidl_ != nullptr; }
    void setMode(const std::string &type, const ThrottlingSeverity &t, const bool &enable,
                 const bool error_on_exit = false);

  private:
    bool power_hal_aidl_exist_;
    std::shared_ptr<IPower> power_hal_aidl_;
    std::shared_ptr<IPowerExt> power_hal_ext_aidl_;
    std::mutex lock_;
};

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
