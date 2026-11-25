/*
 * SPDX-FileCopyrightText: 2016 The Android Open Source Project
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HIDL_GENERATED_android_hardware_bluetooth_V1_0_BluetoothHci_H_
#define HIDL_GENERATED_android_hardware_bluetooth_V1_0_BluetoothHci_H_

#include <android/hardware/bluetooth/1.0/IBluetoothHci.h>

#include <hidl/MQDescriptor.h>

#include <functional>

namespace android {
namespace hardware {
namespace bluetooth {
namespace V1_0 {
namespace implementation {

using ::android::hardware::hidl_vec;
using ::android::hardware::Return;

class BluetoothDeathRecipient;

class BluetoothHci : public IBluetoothHci {
  public:
    BluetoothHci();
    Return<void> initialize(const ::android::sp<IBluetoothHciCallbacks>& cb) override;
    Return<void> sendHciCommand(const hidl_vec<uint8_t>& packet) override;
    Return<void> sendAclData(const hidl_vec<uint8_t>& data) override;
    Return<void> sendScoData(const hidl_vec<uint8_t>& data) override;
    Return<void> close() override;

  private:
    void sendDataToController(const uint8_t type, const hidl_vec<uint8_t>& data);
    ::android::sp<BluetoothDeathRecipient> death_recipient_;
    std::function<void(sp<BluetoothDeathRecipient>&)> unlink_cb_;
};

extern "C" IBluetoothHci* HIDL_FETCH_IBluetoothHci(const char* name);

}  // namespace implementation
}  // namespace V1_0
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android

#endif  // HIDL_GENERATED_android_hardware_bluetooth_V1_0_BluetoothHci_H_
