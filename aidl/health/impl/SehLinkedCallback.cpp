/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <android/binder_ibinder.h>

#include <health-impl/SehHealth.h>
#include <utils/Errors.h>

#include <health-impl/SehLinkedCallback.h>

namespace aidl::vendor::samsung::hardware::health {

::android::base::Result<SehLinkedCallback*> SehLinkedCallback::Make(
        std::shared_ptr<SehHealth> service, std::shared_ptr<ISehHealthInfoCallback> callback) {
    SehLinkedCallback* ret(new SehLinkedCallback());
    // pass ownership of this object to the death recipient
    binder_status_t linkRet =
            AIBinder_linkToDeath(callback->asBinder().get(), service->death_recipient_.get(),
                                 reinterpret_cast<void*>(ret));
    if (linkRet != ::STATUS_OK) {
        LOG(WARNING) << __func__ << "Cannot link to death: " << linkRet;
        return ::android::base::Error(-linkRet);
    }
    ret->service_ = service;
    ret->callback_ = callback;
    return ret;
}

SehLinkedCallback::SehLinkedCallback() = default;

std::shared_ptr<SehHealth> SehLinkedCallback::service() {
    auto service_sp = service_.lock();
    CHECK_NE(nullptr, service_sp);
    return service_sp;
}

void SehLinkedCallback::OnCallbackDied() {
    auto sCb = callback_.lock();
    if (sCb) {
        service()->unregisterCallback(sCb);
    }
}

}  // namespace aidl::vendor::samsung::hardware::health
