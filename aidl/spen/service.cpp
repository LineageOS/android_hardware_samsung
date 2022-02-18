/*
 * SPDX-FileCopyrightText: 2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.samsung.hardware.spen-service"

#include "SPen.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using ::aidl::vendor::samsung::hardware::spen::SPen;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<SPen> spen = ndk::SharedRefBase::make<SPen>();

    const std::string instance = std::string() + SPen::descriptor + "/default";
    binder_status_t status = AServiceManager_addService(spen->asBinder().get(), instance.c_str());
    CHECK(status == STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
