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

#pragma once

#include <memory>
#include <optional>

#include <aidl/android/hardware/health/IHealth.h>
#include <health/HealthLoop.h>

namespace aidl::android::hardware::health {

class HalHealthLoop;

class HalHealthLoopCallback {
  public:
    virtual ~HalHealthLoopCallback() = default;

    // Called by HalHealthLoop::Init
    virtual void OnInit(HalHealthLoop* hal_health_loop, struct healthd_config* config) = 0;
    // Called by HalHealthLoop::Heartbeat
    virtual void OnHeartbeat(){};
    // Called by HalHealthLoop::PrepareToWait
    virtual int OnPrepareToWait() { return -1; }
    // Called by HalHealthLoop::ScheduleBatteryUpdate
    virtual void OnHealthInfoChanged(const HealthInfo&) {}
};

// AIDL version of android::hardware::health::V2_1::implementation::HalHealthLoop.
// An implementation of HealthLoop for using a given health HAL.
class HalHealthLoop final : public ::android::hardware::health::HealthLoop {
  public:
    // Caller must ensure that the lifetime of service_ is not shorter than this object.
    HalHealthLoop(std::shared_ptr<IHealth> service, std::shared_ptr<HalHealthLoopCallback> callback)
        : service_(std::move(service)), callback_(std::move(callback)) {}

    using HealthLoop::RegisterEvent;

    bool charger_online() const { return charger_online_; }

  protected:
    virtual void Init(struct healthd_config* config) override;
    virtual void Heartbeat() override;
    virtual int PrepareToWait() override;
    virtual void ScheduleBatteryUpdate() override;

  private:
    std::shared_ptr<IHealth> service_;
    std::shared_ptr<HalHealthLoopCallback> callback_;
    bool charger_online_ = false;

    // Helpers of OnHealthInfoChanged.
    void set_charger_online(const HealthInfo& health_info);

    // HealthLoop periodically calls ScheduleBatteryUpdate, which calls
    // OnHealthInfoChanged callback. A subclass of the callback can override
    // HalHealthLoopCallback::OnHealthInfoChanged to
    // broadcast the health_info to interested listeners.
    // This adjust uevents / wakealarm periods.
    void OnHealthInfoChanged(const HealthInfo& health_info);
};

}  // namespace aidl::android::hardware::health
