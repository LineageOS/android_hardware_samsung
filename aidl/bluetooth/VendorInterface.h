/*
 * SPDX-FileCopyrightText: 2016 The Android Open Source Project
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <hidl/HidlSupport.h>

#include <functional>

#include "async_fd_watcher.h"
#include "bt_vendor_lib.h"
#include "hci_protocol.h"

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace impl {

namespace async = ::android::hardware::bluetooth::async;
namespace hci = ::android::hardware::bluetooth::hci;

using ::android::hardware::hidl_vec;
using InitializeCompleteCallback = std::function<void(bool success)>;
using PacketReadCallback = std::function<void(const hidl_vec<uint8_t>&)>;

class FirmwareStartupTimer;

class VendorInterface {
  public:
    static bool Initialize(InitializeCompleteCallback initialize_complete_cb,
                           PacketReadCallback event_cb, PacketReadCallback acl_cb,
                           PacketReadCallback sco_cb, PacketReadCallback iso_cb);
    static void Shutdown();
    static VendorInterface* get();

    size_t Send(uint8_t type, const uint8_t* data, size_t length);

    void OnFirmwareConfigured(uint8_t result);

  private:
    virtual ~VendorInterface() = default;

    bool Open(InitializeCompleteCallback initialize_complete_cb, PacketReadCallback event_cb,
              PacketReadCallback acl_cb, PacketReadCallback sco_cb, PacketReadCallback iso_cb);
    void Close();

    void OnTimeout();

    void HandleIncomingEvent(const hidl_vec<uint8_t>& hci_packet);

    void* lib_handle_ = nullptr;
    bt_vendor_interface_t* lib_interface_ = nullptr;
    async::AsyncFdWatcher fd_watcher_;
    InitializeCompleteCallback initialize_complete_cb_;
    hci::HciProtocol* hci_ = nullptr;

    PacketReadCallback event_cb_;

    FirmwareStartupTimer* firmware_startup_timer_ = nullptr;
};

}  // namespace impl
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
