/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include <aidl/android/hardware/health/HealthInfo.h>
#include <aidl/android/hardware/health/IHealth.h>
#include <aidl/vendor/samsung/hardware/health/BnSehHealth.h>
#include <aidl/vendor/samsung/hardware/health/ISehHealthInfoCallback.h>
#include <aidl/vendor/samsung/hardware/health/SehHealthInfo.h>
#include <android/binder_auto_utils.h>

namespace aidl::vendor::samsung::hardware::health {

class SehLinkedCallback;

class SehHealth : public BnSehHealth {
  public:
    explicit SehHealth(std::string_view instance_name,
                       std::shared_ptr<::aidl::android::hardware::health::IHealth> health);
    virtual ~SehHealth();

    ndk::ScopedAStatus registerCallback(
            const std::shared_ptr<ISehHealthInfoCallback>& callback) override;
    ndk::ScopedAStatus unregisterCallback(
            const std::shared_ptr<ISehHealthInfoCallback>& callback) override;
    ndk::ScopedAStatus update() override;
    ndk::ScopedAStatus sehWriteEnableToParam(int32_t in_offset, bool in_enable) override;

    binder_status_t dump(int fd, const char** args, uint32_t num_args) override;

    // Broadcast the given info to all registered ISehHealthInfoCallbacks. Called from update().
    void OnHealthInfoChanged(const SehHealthInfo& info);
    void OnHealthInfoChanged(const ::aidl::android::hardware::health::HealthInfo& health_info);

  private:
    friend SehLinkedCallback;  // for exposing death_recipient_

    ndk::ScopedAStatus getSehHealthInfo(SehHealthInfo* out);
    void FillSehExtras(SehHealthInfo* out);

    std::string instance_name_;
    std::shared_ptr<::aidl::android::hardware::health::IHealth> health_;

    ndk::ScopedAIBinder_DeathRecipient death_recipient_;
    std::mutex callbacks_lock_;
    std::map<SehLinkedCallback*, std::shared_ptr<ISehHealthInfoCallback>> callbacks_;
};

}  // namespace aidl::vendor::samsung::hardware::health
