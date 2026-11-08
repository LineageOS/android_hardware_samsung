/*
 * SPDX-FileCopyrightText: 2020 The Android Open Source Project
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "Gnss-main"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <cstdlib>
#include <string>
#include "Gnss.h"

using aidl::android::hardware::gnss::Gnss;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(1);
    ABinderProcess_startThreadPool();

    std::shared_ptr<Gnss> gnssAidl = ndk::SharedRefBase::make<Gnss>();
    const std::string instance = std::string() + Gnss::descriptor + "/default";
    binder_status_t status =
            AServiceManager_addService(gnssAidl->asBinder().get(), instance.c_str());
    CHECK_EQ(status, STATUS_OK);

    ABinderProcess_joinThreadPool();

    return EXIT_FAILURE;  // should not reach
}
