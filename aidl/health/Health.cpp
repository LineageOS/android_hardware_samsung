/*
 * Copyright (C) 2021 The Android Open Source Project
 * Copyright (C) 2022 The LineageOS Project
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

#include <android-base/logging.h>
#include <android/binder_interface_utils.h>
#include <health-impl/Health.h>
#include <health/utils.h>

#ifndef CHARGER_FORCE_NO_UI
#define CHARGER_FORCE_NO_UI 0
#endif

#if !CHARGER_FORCE_NO_UI
#include <health-impl/ChargerUtils.h>
#endif

#ifndef __ANDROID_RECOVERY__
#include <samsung-health/BatteryData.h>
#include <samsung-health/BatteryInfo.h>
#include <samsung-health/ShortDetection.h>
#endif

using aidl::android::hardware::health::HalHealthLoop;
using aidl::android::hardware::health::Health;
using aidl::android::hardware::health::HealthInfo;

#ifndef __ANDROID_RECOVERY__
static hardware::samsung::health::BatteryData batteryData;
static hardware::samsung::health::BatteryInfo batteryInfo;
static hardware::samsung::health::ShortDetection shortDetection;
#endif

#if !CHARGER_FORCE_NO_UI
using aidl::android::hardware::health::charger::ChargerCallback;
using aidl::android::hardware::health::charger::ChargerModeMain;
#endif

static constexpr const char* gInstanceName = "default";
static constexpr std::string_view gChargerArg{"--charger"};

#ifdef __ANDROID_RECOVERY__
void private_healthd_board_init() {}
int private_healthd_board_battery_update(HealthInfo*) {
    return 0;
}
#else   // !__ANDROID__RECOVERY__
void private_healthd_board_init() {
    batteryInfo.RestoreBattType();
    batteryData.RestoreRtcStatus();
    shortDetection.RestoreCisdData();
    batteryData.UpdateCapacityMax();
}

int private_healthd_board_battery_update(HealthInfo* health_info) {
    shortDetection.UpdateCableCount(*health_info);
    shortDetection.UpdateCisdData();
    batteryData.UpdatePrevBattData();
    batteryData.UpdateAgingHistory(*health_info);
    return 0;
}
#endif  // __ANDROID_RECOVERY__

namespace aidl::android::hardware::health::implementation {
class SehHealthImpl : public Health {
  public:
    SehHealthImpl(std::string_view instance_name, std::unique_ptr<healthd_config>&& config)
        : Health(std::move(instance_name), std::move(config)) {}

    void UpdateHealthInfo(HealthInfo* health_info) override;
};

void SehHealthImpl::UpdateHealthInfo(HealthInfo* health_info) {
    private_healthd_board_battery_update(health_info);
}

}  // namespace aidl::android::hardware::health::implementation

#if !CHARGER_FORCE_NO_UI
namespace aidl::android::hardware::health {
class ChargerCallbackImpl : public ChargerCallback {
  public:
    using ChargerCallback::ChargerCallback;
    bool ChargerEnableSuspend() override { return true; }
};
}  // namespace aidl::android::hardware::health
#endif

int main(int argc, char** argv) {
#ifdef __ANDROID_RECOVERY__
    android::base::InitLogging(argv, android::base::KernelLogger);
#endif

    // make a default health service
    auto config = std::make_unique<healthd_config>();
    ::android::hardware::health::InitHealthdConfig(config.get());
    auto binder = ndk::SharedRefBase::make<Health>(gInstanceName, std::move(config));

    private_healthd_board_init();

    if (argc >= 2 && argv[1] == gChargerArg) {
#if !CHARGER_FORCE_NO_UI
        // If charger shouldn't have UI for your device, simply drop the line below
        // for your service implementation. This corresponds to
        // ro.charger.no_ui=true
        return ChargerModeMain(
                binder,
                std::make_shared<aidl::android::hardware::health::ChargerCallbackImpl>(binder));
#endif

        LOG(INFO) << "Starting charger mode without UI.";
    } else {
        LOG(INFO) << "Starting health HAL.";
    }

    auto hal_health_loop = std::make_shared<HalHealthLoop>(binder, binder);
    return hal_health_loop->StartLoop();
}
