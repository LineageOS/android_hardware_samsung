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

#include <android-base/logging.h>
#include <health-impl/ChargerUtils.h>

namespace aidl::android::hardware::health::charger {

std::optional<bool> ChargerCallback::ChargerShouldKeepScreenOn() {
    return service_->ShouldKeepScreenOn();
}

bool ChargerCallback::ChargerIsOnline() {
    auto hal_health_loop_sp = hal_health_loop_.lock();
    if (hal_health_loop_sp == nullptr) {
        // Assume no charger
        return false;
    }
    return hal_health_loop_sp->charger_online();
}

void ChargerCallback::ChargerInitConfig(healthd_config* config) {
    auto hal_health_loop_sp = hal_health_loop_.lock();
    if (hal_health_loop_sp == nullptr) {
        return;
    }
    return service_->OnInit(hal_health_loop_sp.get(), config);
}

int ChargerCallback::ChargerRegisterEvent(int fd, BoundFunction func, EventWakeup wakeup) {
    auto hal_health_loop_sp = hal_health_loop_.lock();
    if (hal_health_loop_sp == nullptr) {
        return -1;
    }
    return hal_health_loop_sp->RegisterEvent(fd, func, wakeup);
}

void ChargerCallback::set_hal_health_loop(const std::weak_ptr<HalHealthLoop>& hal_health_loop) {
    hal_health_loop_ = std::move(hal_health_loop);
}

// Implements HalHealthLoopCallback for AIDL charger
// Adapter of (Charger, Health) ->  HalHealthLoopCallback
class LoopCallback : public HalHealthLoopCallback {
  public:
    LoopCallback(const std::shared_ptr<Health>& service, ChargerCallback* charger_callback)
        : service_(service), charger_(std::make_unique<::android::Charger>(charger_callback)) {}

    void OnHeartbeat() override {
        service_->OnHeartbeat();
        charger_->OnHeartbeat();
    }
    // Return the minimum timeout. Negative values are treated as no values.
    int OnPrepareToWait() override {
        int timeout1 = service_->OnPrepareToWait();
        int timeout2 = charger_->OnPrepareToWait();

        if (timeout1 < 0) return timeout2;
        if (timeout2 < 0) return timeout1;
        return std::min(timeout1, timeout2);
    }

    void OnInit(HalHealthLoop*, struct healthd_config* config) override {
        // Charger::OnInit calls ChargerInitConfig, which calls into the real Health::OnInit.
        charger_->OnInit(config);
    }

    void OnHealthInfoChanged(const HealthInfo& health_info) override {
        charger_->OnHealthInfoChanged(::android::ChargerHealthInfo{
                .battery_level = health_info.batteryLevel,
                .battery_status = health_info.batteryStatus,
        });
        service_->OnHealthInfoChanged(health_info);
    }

  private:
    std::shared_ptr<Health> service_;
    std::unique_ptr<::android::Charger> charger_;
};

int ChargerModeMain(const std::shared_ptr<Health>& binder,
                    const std::shared_ptr<ChargerCallback>& charger_callback) {
    LOG(INFO) << "Starting charger mode.";
    //   parent stack ==========================================
    //   current stack                                         ||
    //    ||                                                   ||
    //    V                                                    V
    // hal_health_loop => loop_callback => charger --(raw)--> charger_callback
    //    ^----------------(weak)---------------------------------'
    auto loop_callback = std::make_shared<LoopCallback>(binder, charger_callback.get());
    auto hal_health_loop = std::make_shared<HalHealthLoop>(binder, std::move(loop_callback));
    charger_callback->set_hal_health_loop(hal_health_loop);
    return hal_health_loop->StartLoop();
}

}  // namespace aidl::android::hardware::health::charger
