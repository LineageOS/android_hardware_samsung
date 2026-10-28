/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungConfig"

#include "config/RadioConfig.h"

#include "RadioSlot.h"
#include "sim/UiccSlots.h"

#include <algorithm>

namespace aidl::android::hardware::radio::samsung {

using ::ndk::ScopedAStatus;

RadioConfig::RadioConfig() {}

ScopedAStatus RadioConfig::setResponseFunctions(
        const std::shared_ptr<config::IRadioConfigResponse>& response,
        const std::shared_ptr<config::IRadioConfigIndication>& indication) {
    setCallbacks(response, indication);
    return ok();
}

ScopedAStatus RadioConfig::getHalDeviceCapabilities(int32_t serial) {
    LOG_CALL;
    request(SEC_REQUEST_GET_HAL_DEVICE_CAPABILITIES, nullptr, 0,
            [this, serial](auto e, auto data, auto length) {
                const auto reduced = responseInt(data, length);
                respond()->getHalDeviceCapabilitiesResponse(
                        responseInfo(serial, e, reduced.has_value()), reduced.value_or(0) == 1);
            });
    return ok();
}

ScopedAStatus RadioConfig::getNumOfLiveModems(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getNumOfLiveModemsResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioConfig::getPhoneCapability(int32_t serial) {
    LOG_CALL;
    request(SEC_REQUEST_GET_PHONE_CAPABILITY, nullptr, 0,
            [this, serial](auto e, auto data, auto length) {
                const auto* rilCapability = responseAs<SecPhoneCapability>(data, length);
                config::PhoneCapability capability;
                if (rilCapability != nullptr) {
                    capability.maxActiveData = static_cast<int8_t>(rilCapability->maxActiveData);
                    capability.maxActiveInternetData =
                            static_cast<int8_t>(rilCapability->maxActiveInternetData);
                    capability.isInternetLingeringSupported =
                            rilCapability->isInternetLingeringSupported != 0;
                    if (rilCapability->logicalModemIds != nullptr) {
                        capability.logicalModemIds.assign(
                                rilCapability->logicalModemIds,
                                rilCapability->logicalModemIds +
                                        std::max(rilCapability->numLogicalModems, 0));
                    }
                }
                respond()->getPhoneCapabilityResponse(
                        responseInfo(serial, e, rilCapability != nullptr), capability);
            });
    return ok();
}

ScopedAStatus RadioConfig::getSimSlotsStatus(int32_t serial) {
    LOG_CALL;
    request(SEC_REQUEST_GET_SLOT_STATUS, nullptr, 0,
            [this, serial](auto e, auto data, auto length) {
                const auto* list = slotStatusList(data, length);
                if (list != nullptr) {
                    UiccSlots::get().update(*list);
                }
                respond()->getSimSlotsStatusResponse(responseInfo(serial, e, list != nullptr),
                                                     convertSlotStatus(list));
            });
    return ok();
}

ScopedAStatus RadioConfig::setNumOfLiveModems(int32_t serial, int8_t numOfLiveModems) {
    LOG_NOT_SUPPORTED;
    respond()->setNumOfLiveModemsResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioConfig::setPreferredDataModem(int32_t serial, int8_t modemId) {
    LOG_CALL << ' ' << static_cast<int>(modemId);
    int id = modemId;
    request(SEC_REQUEST_SET_PREFERRED_DATA_MODEM, &id, sizeof(id),
            statusResponse(serial, &config::IRadioConfigResponse::setPreferredDataModemResponse));
    return ok();
}

ScopedAStatus RadioConfig::setSimSlotsMapping(int32_t serial,
                                              const std::vector<config::SlotPortMapping>& slotMap) {
    LOG_CALL;
    std::vector<SecSlotMapping> mappings;
    for (const auto& mapping : slotMap) {
        mappings.push_back({mapping.physicalSlotId, mapping.portId});
    }

    // Only the eUICC has a second port; mappings address it as physical slot 2.
    constexpr int kTrayPhysicalSlot = 1;
    constexpr int kEuiccPhysicalSlot = 2;
    const bool usesEuiccPort1 = std::any_of(mappings.begin(), mappings.end(), [](auto& m) {
        return m.physicalSlot == kTrayPhysicalSlot && m.port == 1;
    });
    std::vector<SecSlotMapping*> pointers;
    for (auto& mapping : mappings) {
        if (usesEuiccPort1 && mapping.physicalSlot == kTrayPhysicalSlot) {
            mapping.physicalSlot = kEuiccPhysicalSlot;
        }
        LOG(INFO) << "Slot mapping: logical " << pointers.size() << " -> physical "
                  << mapping.physicalSlot << " port " << mapping.port;
        pointers.push_back(&mapping);
    }

    request(SEC_REQUEST_SET_LOGICAL_TO_PHYSICAL_SLOT_MAPPING, pointers.data(),
            pointers.size() * sizeof(SecSlotMapping*),
            statusResponse(serial, &config::IRadioConfigResponse::setSimSlotsMappingResponse));
    return ok();
}

ScopedAStatus RadioConfig::getSimultaneousCallingSupport(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getSimultaneousCallingSupportResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioConfig::getSimTypeInfo(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getSimTypeInfoResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioConfig::setSimType(int32_t serial,
                                      const std::vector<config::SimType>& simTypes) {
    LOG_NOT_SUPPORTED;
    respond()->setSimTypeResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioConfig::rebootModem(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->rebootModemResponse(notSupported(serial));
    return ok();
}

void RadioConfig::request(int id, const void* data, size_t length,
                          RadioSlot::ResponseCallback callback) {
    RadioSlot::VendorRil::get().request(RIL_SOCKET_1, id, data, length, std::move(callback));
}

const SecSimSlotStatusList* RadioConfig::slotStatusList(const void* data, size_t length) {
    const auto* list = static_cast<const SecSimSlotStatusList*>(data);
    if (list == nullptr || length < offsetof(SecSimSlotStatusList, slots) || list->numSlots < 0 ||
        list->numSlots > kSecMaxSlots ||
        length <
                offsetof(SecSimSlotStatusList, slots) + list->numSlots * sizeof(SecSimSlotStatus)) {
        return nullptr;
    }
    return list;
}

std::vector<config::SimSlotStatus> RadioConfig::convertSlotStatus(
        const SecSimSlotStatusList* list) {
    std::vector<config::SimSlotStatus> slots;
    if (list == nullptr) {
        return slots;
    }
    for (int s = 0; s < list->numSlots; s++) {
        const auto& slot = list->slots[s];
        config::SimSlotStatus status = {
                .cardState = slot.cardState,
                .atr = toString(slot.atr),
                .eid = toString(slot.eid),
        };
        if (slot.mepMode >= static_cast<int>(config::MultipleEnabledProfilesMode::MEP_A1) &&
            slot.mepMode <= static_cast<int>(config::MultipleEnabledProfilesMode::MEP_B)) {
            status.supportedMepMode =
                    static_cast<config::MultipleEnabledProfilesMode>(slot.mepMode);
        }
        for (int p = 0; p < std::clamp(slot.numPorts, 0, kSecMaxPorts); p++) {
            const auto& port = slot.ports[p];
            status.portInfo.push_back({
                    .iccId = toString(port.iccid),
                    .logicalSlotId = port.logicalSlot,
                    .portActive = port.portState != 0,
            });
        }
        slots.push_back(std::move(status));
    }
    return slots;
}

bool RadioConfig::onUnsolicited(int id, const void* data, size_t length, RadioIndicationType type) {
    if (id != SEC_UNSOL_ICC_SLOT_STATUS) {
        LOG(DEBUG) << "Unhandled unsolicited " << id << " (" << length << " bytes)";
        return false;
    }
    const auto* list =
            length == sizeof(SecSimSlotStatusList) ? slotStatusList(data, length) : nullptr;
    if (list == nullptr) {
        LOG(ERROR) << "Invalid slot status (" << length << " bytes)";
        return false;
    }
    UiccSlots::get().update(*list);
    if (const auto indication = indicate()) {
        indication->simSlotsStatusChanged(type, convertSlotStatus(list));
    }
    return false;
}

}  // namespace aidl::android::hardware::radio::samsung
