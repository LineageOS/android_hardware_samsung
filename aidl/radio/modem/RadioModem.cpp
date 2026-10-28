/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungModem"

#include "modem/RadioModem.h"

#include "RadioSlot.h"
#include "WakeLock.h"

namespace aidl::android::hardware::radio::samsung {

using ::ndk::ScopedAStatus;

RadioModem::RadioModem(RadioSlot& slot) : mSlot(slot) {}

ScopedAStatus RadioModem::setResponseFunctions(
        const std::shared_ptr<modem::IRadioModemResponse>& response,
        const std::shared_ptr<modem::IRadioModemIndication>& indication) {
    setCallbacks(response, indication);
    if (indication != nullptr) {
        indication->rilConnected(kUnsolicited);
        indication->radioStateChanged(kUnsolicited, radioState());
    }
    return ok();
}

ScopedAStatus RadioModem::responseAcknowledgement() {
    WakeLock::get().release();
    return ok();
}

ScopedAStatus RadioModem::enableModem(int32_t serial, bool on) {
    LOG_CALL << ' ' << on;
    mSlot.requestInts(SEC_REQUEST_ENABLE_MODEM, {on},
                      statusResponse(serial, &modem::IRadioModemResponse::enableModemResponse));
    return ok();
}

ScopedAStatus RadioModem::getBasebandVersion(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_BASEBAND_VERSION, [this, serial](auto e, auto data, auto) {
        respond()->getBasebandVersionResponse(responseInfo(serial, e),
                                              toString(static_cast<const char*>(data)));
    });
    return ok();
}

ScopedAStatus RadioModem::getDeviceIdentity(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_DEVICE_IDENTITY, [this, serial](auto e, auto data, auto length) {
        const auto* ids = static_cast<char* const*>(data);
        const bool valid = ids != nullptr && length == 4 * sizeof(char*);
        respond()->getDeviceIdentityResponse(
                responseInfo(serial, e, valid), valid ? toString(ids[0]) : "",
                valid ? toString(ids[1]) : "", valid ? toString(ids[2]) : "",
                valid ? toString(ids[3]) : "");
    });
    return ok();
}

ScopedAStatus RadioModem::getHardwareConfig(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getHardwareConfigResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioModem::getModemActivityInfo(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getModemActivityInfoResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioModem::getModemStackStatus(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            SEC_REQUEST_GET_MODEM_STATUS,
            intResponse<bool>(serial, &modem::IRadioModemResponse::getModemStackStatusResponse));
    return ok();
}

ScopedAStatus RadioModem::getRadioCapability(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_GET_RADIO_CAPABILITY, [this, serial](auto e, auto data, auto length) {
        const auto* rc = responseAs<RIL_RadioCapability>(data, length);
        modem::RadioCapability capability;
        if (rc != nullptr) {
            capability = {
                    .session = rc->session,
                    .phase = rc->phase,
                    .raf = rc->rat,
                    .logicalModemUuid = std::string(
                            rc->logicalModemUuid,
                            strnlen(rc->logicalModemUuid, sizeof(rc->logicalModemUuid))),
                    .status = rc->status,
            };
        }
        respond()->getRadioCapabilityResponse(responseInfo(serial, e, rc != nullptr), capability);
    });
    return ok();
}

ScopedAStatus RadioModem::nvReadItem(int32_t serial, modem::NvItem itemId) {
    LOG_NOT_SUPPORTED;
    respond()->nvReadItemResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioModem::nvResetConfig(int32_t serial, modem::ResetNvType resetType) {
    LOG_NOT_SUPPORTED;
    respond()->nvResetConfigResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioModem::nvWriteCdmaPrl(int32_t serial, const std::vector<uint8_t>& prl) {
    LOG_NOT_SUPPORTED;
    respond()->nvWriteCdmaPrlResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioModem::nvWriteItem(int32_t serial, const modem::NvWriteItem& item) {
    LOG_NOT_SUPPORTED;
    respond()->nvWriteItemResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioModem::requestShutdown(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_SHUTDOWN,
                  statusResponse(serial, &modem::IRadioModemResponse::requestShutdownResponse));
    return ok();
}

ScopedAStatus RadioModem::sendDeviceState(int32_t serial, modem::DeviceStateType deviceStateType,
                                          bool state) {
    LOG_CALL << ' ' << toString(deviceStateType) << ' ' << state;
    mSlot.requestInts(RIL_REQUEST_SEND_DEVICE_STATE, {static_cast<int>(deviceStateType), state},
                      statusResponse(serial, &modem::IRadioModemResponse::sendDeviceStateResponse));
    return ok();
}

ScopedAStatus RadioModem::setRadioCapability(int32_t serial, const modem::RadioCapability& rc) {
    LOG_NOT_SUPPORTED;
    respond()->setRadioCapabilityResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioModem::setRadioPower(int32_t serial, bool powerOn, bool forEmergencyCall,
                                        bool preferredForEmergencyCall) {
    LOG_CALL << ' ' << powerOn << ' ' << forEmergencyCall << ' ' << preferredForEmergencyCall;
    mSlot.requestInts(RIL_REQUEST_RADIO_POWER,
                      {powerOn, forEmergencyCall, preferredForEmergencyCall},
                      statusResponse(serial, &modem::IRadioModemResponse::setRadioPowerResponse));
    return ok();
}

ScopedAStatus RadioModem::getImei(int32_t serial) {
    LOG_CALL;
    mSlot.request(SEC_REQUEST_DEVICE_IMEI, [this, serial](auto e, auto data, auto length) {
        const auto* imei = responseAs<SecImei>(data, length);
        modem::ImeiInfo info;
        if (imei != nullptr) {
            info = {
                    .type = static_cast<modem::ImeiInfo::ImeiType>(imei->type),
                    .imei = std::string(imei->imei, strnlen(imei->imei, sizeof(imei->imei))),
                    .svn = std::string(imei->svn, strnlen(imei->svn, sizeof(imei->svn))),
            };
        }
        respond()->getImeiResponse(responseInfo(serial, e, imei != nullptr), info);
    });
    return ok();
}

modem::RadioState RadioModem::radioState() {
    return static_cast<modem::RadioState>(RadioSlot::VendorRil::get().radioState(mSlot.socket()));
}

bool RadioModem::onUnsolicited(int id, const void* data, size_t length, RadioIndicationType type) {
    const auto indication = indicate();
    if (indication == nullptr) {
        return false;
    }
    switch (id) {
        case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED:
            indication->radioStateChanged(type, radioState());
            return true;
        case RIL_UNSOL_RIL_CONNECTED:
            indication->rilConnected(type);
            return true;
        case RIL_UNSOL_MODEM_RESTART:
            indication->modemReset(type, toString(static_cast<const char*>(data)));
            return true;
        default:
            LOG(DEBUG) << "Unhandled unsolicited " << id << " (" << length << " bytes)";
    }
    return false;
}

}  // namespace aidl::android::hardware::radio::samsung
