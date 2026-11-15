/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "android.hardware.bluetooth-service.samsung"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "BluetoothHci.h"

using aidl::android::hardware::bluetooth::impl::BluetoothHci;

// Extra threads make priority inheritance faster.
static const size_t kMaxThreads = 5;

int main() {
    // The main thread joins the pool as well
    ABinderProcess_setThreadPoolMaxThreadCount(kMaxThreads - 1);
    ABinderProcess_startThreadPool();

    std::shared_ptr<BluetoothHci> hci = ndk::SharedRefBase::make<BluetoothHci>();

    const std::string instance = std::string(BluetoothHci::descriptor) + "/default";
    binder_status_t status = AServiceManager_addService(hci->asBinder().get(), instance.c_str());
    CHECK_EQ(status, STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
