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

#ifndef __ANDROID_RECOVERY__
#include <android/binder_ibinder.h>
#include <health-impl/SehHealth.h>
#endif

#ifndef CHARGER_FORCE_NO_UI
#define CHARGER_FORCE_NO_UI 0
#endif

#if !CHARGER_FORCE_NO_UI
#include <health-impl/ChargerUtils.h>
#endif

using aidl::android::hardware::health::HalHealthLoop;
using aidl::android::hardware::health::HalHealthLoopCallback;
using aidl::android::hardware::health::Health;
using aidl::android::hardware::health::HealthInfo;

#ifndef __ANDROID_RECOVERY__
using aidl::vendor::samsung::hardware::health::SehHealth;
#endif

#if !CHARGER_FORCE_NO_UI
using aidl::android::hardware::health::charger::ChargerCallback;
using aidl::android::hardware::health::charger::ChargerModeMain;
#endif

static constexpr const char* gInstanceName = "default";
static constexpr std::string_view gChargerArg{"--charger"};

#if !CHARGER_FORCE_NO_UI
namespace aidl::android::hardware::health {
class ChargerCallbackImpl : public ChargerCallback {
  public:
    using ChargerCallback::ChargerCallback;
    bool ChargerEnableSuspend() override { return true; }
};
}  // namespace aidl::android::hardware::health
#endif

#ifndef __ANDROID_RECOVERY__
namespace {
class SehLoopCallback : public HalHealthLoopCallback {
  public:
    SehLoopCallback(std::shared_ptr<Health> health, std::shared_ptr<SehHealth> seh_health)
        : health_(std::move(health)), seh_health_(std::move(seh_health)) {}

    void OnInit(HalHealthLoop* hal_health_loop, struct healthd_config* config) override {
        health_->OnInit(hal_health_loop, config);
    }
    void OnHeartbeat() override { health_->OnHeartbeat(); }
    int OnPrepareToWait() override { return health_->OnPrepareToWait(); }
    void OnHealthInfoChanged(const HealthInfo& health_info) override {
        health_->OnHealthInfoChanged(health_info);
        seh_health_->OnHealthInfoChanged(health_info);
    }

  private:
    std::shared_ptr<Health> health_;
    std::shared_ptr<SehHealth> seh_health_;
};
}  // namespace
#endif

int main(int argc, char** argv) {
#ifdef __ANDROID_RECOVERY__
    android::base::InitLogging(argv, android::base::KernelLogger);
#endif

    // make a default health service
    auto config = std::make_unique<healthd_config>();
    ::android::hardware::health::InitHealthdConfig(config.get());
    auto binder = ndk::SharedRefBase::make<Health>(gInstanceName, std::move(config));

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
        LOG(INFO) << "Starting seh health HAL.";
    }

    std::shared_ptr<HalHealthLoopCallback> loop_callback = binder;

#ifndef __ANDROID_RECOVERY__
    auto seh_health = ndk::SharedRefBase::make<SehHealth>(gInstanceName, binder);
    CHECK_EQ(STATUS_OK,
             AIBinder_setExtension(binder->asBinder().get(), seh_health->asBinder().get()))
            << "Failed to set ISehHealth as an extension of IHealth";
    loop_callback = std::make_shared<SehLoopCallback>(binder, seh_health);
#endif

    auto hal_health_loop = std::make_shared<HalHealthLoop>(binder, loop_callback);
    return hal_health_loop->StartLoop();
}
