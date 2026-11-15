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

int main() {
    // Packets have to be handled in order, serve the calls on a single thread
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    std::shared_ptr<BluetoothHci> hci = ndk::SharedRefBase::make<BluetoothHci>();

    const std::string instance = std::string(BluetoothHci::descriptor) + "/default";
    binder_status_t status = AServiceManager_addService(hci->asBinder().get(), instance.c_str());
    CHECK_EQ(status, STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
