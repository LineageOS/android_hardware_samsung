/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "android.hardware.bluetooth-service.samsung"

#include "BluetoothHci.h"
#include "VendorInterface.h"

#include <log/log.h>

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace impl {

namespace {

constexpr uint8_t kHciDataTypeCommand = 1;
constexpr uint8_t kHciDataTypeAcl = 2;
constexpr uint8_t kHciDataTypeSco = 3;
constexpr uint8_t kHciDataTypeIso = 5;

std::vector<uint8_t> toVector(const hidl_vec<uint8_t>& packet) {
    return std::vector<uint8_t>(packet.data(), packet.data() + packet.size());
}

void logIfFailed(const ndk::ScopedAStatus& status, const char* what) {
    if (!status.isOk()) {
        ALOGE("Unable to call %s(): %s", what, status.getDescription().c_str());
    }
}

}  // namespace

BluetoothHci::BluetoothHci()
    : mDeathRecipient(AIBinder_DeathRecipient_new(&BluetoothHci::onClientDied)) {
    // The cookie is this object, which lives as long as the service, so there
    // is nothing to release when the link goes away
    AIBinder_DeathRecipient_setOnUnlinked(mDeathRecipient.get(), [](void*) {});
}

void BluetoothHci::onClientDied(void* cookie) {
    ALOGE("Bluetooth service died");
    static_cast<BluetoothHci*>(cookie)->close();
}

ndk::ScopedAStatus BluetoothHci::initialize(const std::shared_ptr<IBluetoothHciCallbacks>& cb) {
    ALOGI("%s", __func__);

    if (cb == nullptr) {
        ALOGE("%s: no callbacks", __func__);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard<std::mutex> lock(mMutex);

    if (VendorInterface::get() != nullptr) {
        ALOGW("%s: already initialized", __func__);
        logIfFailed(cb->initializationComplete(Status::ALREADY_INITIALIZED),
                    "initializationComplete");
        return ndk::ScopedAStatus::ok();
    }

    bool ok = VendorInterface::Initialize(
            [cb](bool success) {
                logIfFailed(
                        cb->initializationComplete(success ? Status::SUCCESS
                                                           : Status::HARDWARE_INITIALIZATION_ERROR),
                        "initializationComplete");
            },
            [cb](const hidl_vec<uint8_t>& packet) {
                logIfFailed(cb->hciEventReceived(toVector(packet)), "hciEventReceived");
            },
            [cb](const hidl_vec<uint8_t>& packet) {
                logIfFailed(cb->aclDataReceived(toVector(packet)), "aclDataReceived");
            },
            [cb](const hidl_vec<uint8_t>& packet) {
                logIfFailed(cb->scoDataReceived(toVector(packet)), "scoDataReceived");
            },
            [cb](const hidl_vec<uint8_t>& packet) {
                logIfFailed(cb->isoDataReceived(toVector(packet)), "isoDataReceived");
            });

    if (!ok) {
        ALOGE("%s: unable to open the vendor interface", __func__);
        // Release what was opened, so that the next initialize() can try again
        if (VendorInterface::get() != nullptr) {
            VendorInterface::Shutdown();
        }
        logIfFailed(cb->initializationComplete(Status::UNABLE_TO_OPEN_INTERFACE),
                    "initializationComplete");
        return ndk::ScopedAStatus::ok();
    }

    mCb = cb;
    AIBinder_linkToDeath(mCb->asBinder().get(), mDeathRecipient.get(), this);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BluetoothHci::close() {
    ALOGI("%s", __func__);

    std::lock_guard<std::mutex> lock(mMutex);
    closeLocked();

    return ndk::ScopedAStatus::ok();
}

void BluetoothHci::closeLocked() {
    if (mCb != nullptr) {
        AIBinder_unlinkToDeath(mCb->asBinder().get(), mDeathRecipient.get(), this);
        mCb = nullptr;
    }

    if (VendorInterface::get() != nullptr) {
        VendorInterface::Shutdown();
    }
}

ndk::ScopedAStatus BluetoothHci::sendHciCommand(const std::vector<uint8_t>& packet) {
    send(kHciDataTypeCommand, packet);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BluetoothHci::sendAclData(const std::vector<uint8_t>& packet) {
    send(kHciDataTypeAcl, packet);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BluetoothHci::sendScoData(const std::vector<uint8_t>& packet) {
    send(kHciDataTypeSco, packet);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BluetoothHci::sendIsoData(const std::vector<uint8_t>& packet) {
    send(kHciDataTypeIso, packet);
    return ndk::ScopedAStatus::ok();
}

void BluetoothHci::send(uint8_t type, const std::vector<uint8_t>& packet) {
    VendorInterface* vendorInterface = VendorInterface::get();
    if (vendorInterface == nullptr) {
        ALOGE("%s: not initialized, dropping packet type %u", __func__, type);
        return;
    }

    vendorInterface->Send(type, packet.data(), packet.size());
}

}  // namespace impl
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
