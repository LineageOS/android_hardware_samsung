/*
 * Copyright (C) 2022-2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ChargingControl.h"
#include "FastCharge.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using ::aidl::vendor::lineage::health::ChargingControl;
using ::aidl::vendor::lineage::health::FastCharge;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<ChargingControl> lh = ndk::SharedRefBase::make<ChargingControl>();
    std::shared_ptr<FastCharge> fc = ndk::SharedRefBase::make<FastCharge>();

    std::string instance = std::string() + ChargingControl::descriptor + "/default";
    binder_status_t status = AServiceManager_addService(lh->asBinder().get(), instance.c_str());
    CHECK_EQ(status, STATUS_OK);

    instance = std::string() + FastCharge::descriptor + "/default";
    status = AServiceManager_addService(fc->asBinder().get(), instance.c_str());
    CHECK_EQ(status, STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
