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

#include <map>
#include <memory>
#include <optional>

#include <aidl/android/hardware/health/BnHealth.h>
#include <aidl/android/hardware/health/IHealthInfoCallback.h>
#include <android/binder_auto_utils.h>
#include <health-impl/HalHealthLoop.h>
#include <healthd/BatteryMonitor.h>
#include <healthd/healthd.h>

namespace aidl::android::hardware::health {

class LinkedCallback;

// AIDL version of android::hardware::health::V2_1::implementation::Health and BinderHealth.
// There's no need to separate the two in AIDL because AIDL does not support passthrough transport.
//
// Instead of inheriting from HalHealthLoop directly, implements the callback interface to
// HalHealthLoop instead.
//
// Sample implementation of health HAL.
class Health : public BnHealth, public HalHealthLoopCallback {
  public:
    // Initialize with |config|.
    // A subclass may modify |config| before passing it to the parent constructor.
    // See implementation of Health for code samples.
    Health(std::string_view instance_name, std::unique_ptr<struct healthd_config>&& config);
    virtual ~Health();

    ndk::ScopedAStatus registerCallback(
            const std::shared_ptr<IHealthInfoCallback>& callback) override;
    ndk::ScopedAStatus unregisterCallback(
            const std::shared_ptr<IHealthInfoCallback>& callback) override;
    ndk::ScopedAStatus update() override;

    // A subclass should not override this. Override UpdateHealthInfo instead.
    ndk::ScopedAStatus getHealthInfo(HealthInfo* out) override final;

    // A subclass is recommended to override the path in healthd_config in the constructor.
    // Only override these if there are no suitable kernel interfaces to read these values.
    ndk::ScopedAStatus getChargeCounterUah(int32_t* out) override;
    ndk::ScopedAStatus getCurrentNowMicroamps(int32_t* out) override;
    ndk::ScopedAStatus getCurrentAverageMicroamps(int32_t* out) override;
    ndk::ScopedAStatus getCapacity(int32_t* out) override;
    ndk::ScopedAStatus getChargeStatus(BatteryStatus* out) override;

    // A subclass may either override these or provide function pointers in
    // in healthd_config in the constructor.
    // Prefer overriding these for efficiency.
    ndk::ScopedAStatus getEnergyCounterNwh(int64_t* out) override;

    // A subclass may override these for a specific device.
    // The default implementations return nothing in |out|.
    ndk::ScopedAStatus getDiskStats(std::vector<DiskStats>* out) override;
    ndk::ScopedAStatus getStorageInfo(std::vector<StorageInfo>* out) override;
    ndk::ScopedAStatus getHingeInfo(std::vector<HingeInfo>* out) override;

    ndk::ScopedAStatus setChargingPolicy(BatteryChargingPolicy in_value) override;
    ndk::ScopedAStatus getChargingPolicy(BatteryChargingPolicy* out) override;
    ndk::ScopedAStatus getBatteryHealthData(BatteryHealthData* out) override;

    // A subclass may override these to provide a different implementation.
    binder_status_t dump(int fd, const char** args, uint32_t num_args) override;

    // HalHealthLoopCallback implementation.
    void OnInit(HalHealthLoop* hal_health_loop, struct healthd_config* config) override;
    void OnHealthInfoChanged(const HealthInfo& health_info) override;

    // A subclass may override this if it wants to handle binder events differently.
    virtual void BinderEvent(uint32_t epevents);

    // A subclass may override this to determine whether screen should be kept on in charger mode.
    // Default is to invoke healthd_config->screen_on() on the BatteryProperties converted
    // from getHealthInfo.
    // Prefer overriding these to providing screen_on in healthd_config in the constructor
    // for efficiency.
    virtual std::optional<bool> ShouldKeepScreenOn();

  protected:
    // A subclass can override this to modify any health info object before
    // returning to clients. This is similar to healthd_board_battery_update().
    // By default, it does nothing.
    // See implementation of Health for code samples.
    virtual void UpdateHealthInfo(HealthInfo* health_info);

  private:
    friend LinkedCallback;  // for exposing death_recipient_

    bool unregisterCallbackInternal(std::shared_ptr<IHealthInfoCallback> callback);

    std::string instance_name_;
    ::android::BatteryMonitor battery_monitor_;
    std::unique_ptr<struct healthd_config> healthd_config_;

    ndk::ScopedAIBinder_DeathRecipient death_recipient_;
    int binder_fd_ = -1;
    std::mutex callbacks_lock_;
    std::map<LinkedCallback*, std::shared_ptr<IHealthInfoCallback>> callbacks_;
};

}  // namespace aidl::android::hardware::health
