/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include <optional>
#include <type_traits>

#include <android/binder_auto_utils.h>
#include <charger/healthd_mode_charger.h>
#include <health-impl/Health.h>

#pragma once

namespace aidl::android::hardware::health::charger {

// Implements ChargerHalHealthLoopInterface for AIDL charger
// Adapter of (Health, HalHealthLoop) -> ChargerHalHealthLoopInterface
class ChargerCallback : public ::android::ChargerConfigurationInterface {
  public:
    ChargerCallback(const std::shared_ptr<Health>& service) : service_(service) {}
    std::optional<bool> ChargerShouldKeepScreenOn() override;
    bool ChargerIsOnline() override;
    void ChargerInitConfig(healthd_config* config) override;
    int ChargerRegisterEvent(int fd, BoundFunction func, EventWakeup wakeup) override;

    // Override to return true to replace `ro.charger.enable_suspend=true`
    bool ChargerEnableSuspend() override { return false; }

    void set_hal_health_loop(const std::weak_ptr<HalHealthLoop>& hal_health_loop);

  private:
    std::shared_ptr<Health> service_;
    std::weak_ptr<HalHealthLoop> hal_health_loop_;
};

int ChargerModeMain(const std::shared_ptr<Health>& binder,
                    const std::shared_ptr<ChargerCallback>& charger_callback);

}  // namespace aidl::android::hardware::health::charger
