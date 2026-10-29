/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/sap/BnSap.h>

#include "common.h"

namespace aidl::android::hardware::radio::samsung {

class RadioSap : public sap::BnSap {
  public:
    explicit RadioSap(RadioSlot& slot);

    ::ndk::ScopedAStatus apduReq(int32_t serial, sap::SapApduType type,
                                 const std::vector<uint8_t>& command) override;
    ::ndk::ScopedAStatus connectReq(int32_t serial, int32_t maxMsgSizeBytes) override;
    ::ndk::ScopedAStatus disconnectReq(int32_t serial) override;
    ::ndk::ScopedAStatus powerReq(int32_t serial, bool powerOn) override;
    ::ndk::ScopedAStatus resetSimReq(int32_t serial) override;
    ::ndk::ScopedAStatus setCallback(const std::shared_ptr<sap::ISapCallback>& callback) override;
    ::ndk::ScopedAStatus setTransferProtocolReq(int32_t serial,
                                                sap::SapTransferProtocol transferProtocol) override;
    ::ndk::ScopedAStatus transferAtrReq(int32_t serial) override;
    ::ndk::ScopedAStatus transferCardReaderStatusReq(int32_t serial) override;

  private:
    void request(int32_t serial, int id, const std::vector<uint8_t>& payload);
    void onResponse(int32_t serial, int id, const void* data, size_t length);
    void onFailure(int32_t serial, int id);
    void onUnsolicited(int id, const void* data, size_t length);
    std::shared_ptr<sap::ISapCallback> callback();

    RadioSlot& mSlot;
    std::mutex mMutex;
    std::shared_ptr<sap::ISapCallback> mCallback;
};

}  // namespace aidl::android::hardware::radio::samsung
