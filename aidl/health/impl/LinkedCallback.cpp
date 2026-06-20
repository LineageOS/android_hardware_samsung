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
#include <android/binder_ibinder.h>

#include <health-impl/Health.h>
#include <utils/Errors.h>

#include "LinkedCallback.h"

namespace aidl::android::hardware::health {

::android::base::Result<LinkedCallback*> LinkedCallback::Make(
        std::shared_ptr<Health> service, std::shared_ptr<IHealthInfoCallback> callback) {
    LinkedCallback* ret(new LinkedCallback());
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

LinkedCallback::LinkedCallback() = default;

std::shared_ptr<Health> LinkedCallback::service() {
    auto service_sp = service_.lock();
    CHECK_NE(nullptr, service_sp);
    return service_sp;
}

void LinkedCallback::OnCallbackDied() {
    auto sCb = callback_.lock();
    if (sCb) {
        service()->unregisterCallback(sCb);
    }
}

}  // namespace aidl::android::hardware::health
