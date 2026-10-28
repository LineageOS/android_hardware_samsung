/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/modem/BnRadioModem.h>

#include "common.h"

namespace aidl::android::hardware::radio::samsung {

class RadioModem : public modem::BnRadioModem,
                   public RadioService<modem::IRadioModemResponse, modem::IRadioModemIndication> {
  public:
    explicit RadioModem(RadioSlot& slot);

    ::ndk::ScopedAStatus setResponseFunctions(
            const std::shared_ptr<modem::IRadioModemResponse>& response,
            const std::shared_ptr<modem::IRadioModemIndication>& indication) override;
    ::ndk::ScopedAStatus responseAcknowledgement() override;

    ::ndk::ScopedAStatus enableModem(int32_t serial, bool on) override;
    ::ndk::ScopedAStatus getBasebandVersion(int32_t serial) override;
    ::ndk::ScopedAStatus getDeviceIdentity(int32_t serial) override;
    ::ndk::ScopedAStatus getHardwareConfig(int32_t serial) override;
    ::ndk::ScopedAStatus getModemActivityInfo(int32_t serial) override;
    ::ndk::ScopedAStatus getModemStackStatus(int32_t serial) override;
    ::ndk::ScopedAStatus getRadioCapability(int32_t serial) override;
    ::ndk::ScopedAStatus nvReadItem(int32_t serial, modem::NvItem itemId) override;
    ::ndk::ScopedAStatus nvResetConfig(int32_t serial, modem::ResetNvType resetType) override;
    ::ndk::ScopedAStatus nvWriteCdmaPrl(int32_t serial, const std::vector<uint8_t>& prl) override;
    ::ndk::ScopedAStatus nvWriteItem(int32_t serial, const modem::NvWriteItem& item) override;
    ::ndk::ScopedAStatus requestShutdown(int32_t serial) override;
    ::ndk::ScopedAStatus sendDeviceState(int32_t serial, modem::DeviceStateType deviceStateType,
                                         bool state) override;
    ::ndk::ScopedAStatus setRadioCapability(int32_t serial,
                                            const modem::RadioCapability& rc) override;
    ::ndk::ScopedAStatus setRadioPower(int32_t serial, bool powerOn, bool forEmergencyCall,
                                       bool preferredForEmergencyCall) override;
    ::ndk::ScopedAStatus getImei(int32_t serial) override;

    bool onUnsolicited(int id, const void* data, size_t length, RadioIndicationType type);

  private:
    modem::RadioState radioState();

    RadioSlot& mSlot;
};

}  // namespace aidl::android::hardware::radio::samsung
