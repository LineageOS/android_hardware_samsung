/*
 * SPDX-FileCopyrightText: 2016 The Android Open Source Project
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "android.hardware.bluetooth@1.0-impl"
#include "bluetooth_hci.h"

#include <log/log.h>

#include "vendor_interface.h"

namespace android {
namespace hardware {
namespace bluetooth {
namespace V1_0 {
namespace implementation {

static const uint8_t HCI_DATA_TYPE_COMMAND = 1;
static const uint8_t HCI_DATA_TYPE_ACL = 2;
static const uint8_t HCI_DATA_TYPE_SCO = 3;

class BluetoothDeathRecipient : public hidl_death_recipient {
  public:
    BluetoothDeathRecipient(const sp<IBluetoothHci> hci) : mHci(hci) {}

    virtual void serviceDied(uint64_t /*cookie*/,
                             const wp<::android::hidl::base::V1_0::IBase>& /*who*/) {
        ALOGE("BluetoothDeathRecipient::serviceDied - Bluetooth service died");
        has_died_ = true;
        mHci->close();
    }
    sp<IBluetoothHci> mHci;
    bool getHasDied() const { return has_died_; }
    void setHasDied(bool has_died) { has_died_ = has_died; }

  private:
    bool has_died_;
};

BluetoothHci::BluetoothHci() : death_recipient_(new BluetoothDeathRecipient(this)) {}

Return<void> BluetoothHci::initialize(const ::android::sp<IBluetoothHciCallbacks>& cb) {
    ALOGI("BluetoothHci::initialize()");
    if (cb == nullptr) {
        ALOGE("cb == nullptr! -> Unable to call initializationComplete(ERR)");
        return Void();
    }

    if (VendorInterface::get() != nullptr) {
        ALOGW("BluetoothHci is already initialized");
        auto hidl_status = cb->initializationComplete(Status::INITIALIZATION_ERROR);
        if (!hidl_status.isOk()) {
            ALOGE("Unable to call initializationComplete(ERR)");
        }
        return Void();
    }

    death_recipient_->setHasDied(false);
    cb->linkToDeath(death_recipient_, 0);

    bool rc = VendorInterface::Initialize(
            [cb](bool status) {
                auto hidl_status = cb->initializationComplete(
                        status ? Status::SUCCESS : Status::INITIALIZATION_ERROR);
                if (!hidl_status.isOk()) {
                    ALOGE("VendorInterface -> Unable to call initializationComplete()");
                }
            },
            [cb](const hidl_vec<uint8_t>& packet) {
                auto hidl_status = cb->hciEventReceived(packet);
                if (!hidl_status.isOk()) {
                    ALOGE("VendorInterface -> Unable to call hciEventReceived()");
                }
            },
            [cb](const hidl_vec<uint8_t>& packet) {
                auto hidl_status = cb->aclDataReceived(packet);
                if (!hidl_status.isOk()) {
                    ALOGE("VendorInterface -> Unable to call aclDataReceived()");
                }
            },
            [cb](const hidl_vec<uint8_t>& packet) {
                auto hidl_status = cb->scoDataReceived(packet);
                if (!hidl_status.isOk()) {
                    ALOGE("VendorInterface -> Unable to call scoDataReceived()");
                }
            },
            [cb](const hidl_vec<uint8_t>&) {
                ALOGE("VendorInterface -> No callback for ISO packets in HAL V1_0");
            });

    if (!rc) {
        auto hidl_status = cb->initializationComplete(Status::INITIALIZATION_ERROR);
        if (!hidl_status.isOk()) {
            ALOGE("VendorInterface -> Unable to call initializationComplete(ERR)");
        }
    }

    unlink_cb_ = [cb](sp<BluetoothDeathRecipient>& death_recipient) {
        if (death_recipient->getHasDied())
            ALOGI("Skipping unlink call, service died.");
        else
            cb->unlinkToDeath(death_recipient);
    };

    return Void();
}

Return<void> BluetoothHci::close() {
    ALOGI("BluetoothHci::close()");
    unlink_cb_(death_recipient_);
    VendorInterface::Shutdown();
    return Void();
}

Return<void> BluetoothHci::sendHciCommand(const hidl_vec<uint8_t>& command) {
    sendDataToController(HCI_DATA_TYPE_COMMAND, command);
    return Void();
}

Return<void> BluetoothHci::sendAclData(const hidl_vec<uint8_t>& data) {
    sendDataToController(HCI_DATA_TYPE_ACL, data);
    return Void();
}

Return<void> BluetoothHci::sendScoData(const hidl_vec<uint8_t>& data) {
    sendDataToController(HCI_DATA_TYPE_SCO, data);
    return Void();
}

void BluetoothHci::sendDataToController(const uint8_t type, const hidl_vec<uint8_t>& data) {
    VendorInterface::get()->Send(type, data.data(), data.size());
}

IBluetoothHci* HIDL_FETCH_IBluetoothHci(const char* /* name */) {
    return new BluetoothHci();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
