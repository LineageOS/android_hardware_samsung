/*
 * Copyright (C) The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/bluetooth/BnBluetoothHci.h>
#include <aidl/android/hardware/bluetooth/IBluetoothHciCallbacks.h>

#include <mutex>
#include <vector>

namespace aidl::android::hardware::bluetooth::impl {

/*
 * Bluetooth HCI HAL on top of the vendor libbt-vendor.so, like the HIDL
 * android.hardware.bluetooth@1.0 default implementation.
 */
class BluetoothHci : public BnBluetoothHci {
  public:
    BluetoothHci();

    ndk::ScopedAStatus initialize(const std::shared_ptr<IBluetoothHciCallbacks>& cb) override;
    ndk::ScopedAStatus sendHciCommand(const std::vector<uint8_t>& packet) override;
    ndk::ScopedAStatus sendAclData(const std::vector<uint8_t>& packet) override;
    ndk::ScopedAStatus sendScoData(const std::vector<uint8_t>& packet) override;
    ndk::ScopedAStatus sendIsoData(const std::vector<uint8_t>& packet) override;
    ndk::ScopedAStatus close() override;

  private:
    static void onClientDied(void* cookie);

    void send(uint8_t type, const std::vector<uint8_t>& packet);
    void closeLocked();

    std::mutex mMutex;
    std::shared_ptr<IBluetoothHciCallbacks> mCb;
    ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;
};

}  // namespace aidl::android::hardware::bluetooth::impl
