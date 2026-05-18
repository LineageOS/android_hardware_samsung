/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "android.hardware.ir-service.samsung"

#include "ConsumerIr.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using ::aidl::android::hardware::ir::ConsumerIr;

int main() {
    LOG(DEBUG) << ("ConsumerIrService start");
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<ConsumerIr> service = ndk::SharedRefBase::make<ConsumerIr>();

    const std::string instance = std::string() + ConsumerIr::descriptor + "/default";
    binder_status_t status =
            AServiceManager_addService(service->asBinder().get(), instance.c_str());
    CHECK(status == STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
