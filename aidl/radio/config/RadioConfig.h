/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/config/BnRadioConfig.h>

#include "RadioSlot.h"
#include "common.h"

namespace aidl::android::hardware::radio::samsung {

class RadioConfig
    : public config::BnRadioConfig,
      public RadioService<config::IRadioConfigResponse, config::IRadioConfigIndication> {
  public:
    RadioConfig();

    ::ndk::ScopedAStatus setResponseFunctions(
            const std::shared_ptr<config::IRadioConfigResponse>& response,
            const std::shared_ptr<config::IRadioConfigIndication>& indication) override;

    ::ndk::ScopedAStatus getHalDeviceCapabilities(int32_t serial) override;
    ::ndk::ScopedAStatus getNumOfLiveModems(int32_t serial) override;
    ::ndk::ScopedAStatus getPhoneCapability(int32_t serial) override;
    ::ndk::ScopedAStatus getSimSlotsStatus(int32_t serial) override;
    ::ndk::ScopedAStatus setNumOfLiveModems(int32_t serial, int8_t numOfLiveModems) override;
    ::ndk::ScopedAStatus setPreferredDataModem(int32_t serial, int8_t modemId) override;
    ::ndk::ScopedAStatus setSimSlotsMapping(
            int32_t serial, const std::vector<config::SlotPortMapping>& slotMap) override;
    ::ndk::ScopedAStatus getSimultaneousCallingSupport(int32_t serial) override;
    ::ndk::ScopedAStatus getSimTypeInfo(int32_t serial) override;
    ::ndk::ScopedAStatus setSimType(int32_t serial,
                                    const std::vector<config::SimType>& simTypes) override;
    ::ndk::ScopedAStatus rebootModem(int32_t serial) override;

    bool onUnsolicited(int id, const void* data, size_t length, RadioIndicationType type);

  private:
    static void request(int id, const void* data, size_t length,
                        RadioSlot::ResponseCallback callback);
    static const SecSimSlotStatusList* slotStatusList(const void* data, size_t length);
    static std::vector<config::SimSlotStatus> convertSlotStatus(const SecSimSlotStatusList* list);
};

}  // namespace aidl::android::hardware::radio::samsung
