/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <aidl/vendor/samsung/hardware/health/ISehHealthInfoCallback.h>
#include <android-base/macros.h>
#include <android-base/result.h>
#include <android/binder_auto_utils.h>

#include <health-impl/SehHealth.h>

namespace aidl::vendor::samsung::hardware::health {

// Type of the cookie pointer in linkToDeath.
// A (SehHealth, ISehHealthInfoCallback) tuple.
class SehLinkedCallback {
  public:
    // Automatically linkToDeath upon construction with the returned object as the cookie.
    // The deathRecipient owns the SehLinkedCallback object and will delete it with
    // cookie when it's unlinked.
    static ::android::base::Result<SehLinkedCallback*> Make(
            std::shared_ptr<SehHealth> service,
            std::shared_ptr<ISehHealthInfoCallback> callback);
    // On callback died, unregister it from the service.
    void OnCallbackDied();

  private:
    SehLinkedCallback();
    DISALLOW_COPY_AND_ASSIGN(SehLinkedCallback);

    std::shared_ptr<SehHealth> service();

    std::weak_ptr<SehHealth> service_;
    std::weak_ptr<ISehHealthInfoCallback> callback_;
};

}  // namespace aidl::vendor::samsung::hardware::health
