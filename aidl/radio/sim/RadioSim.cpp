/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungSim"

#include "sim/RadioSim.h"

#include <android-base/properties.h>

#include <cstdio>

#include "RadioSlot.h"
#include "WakeLock.h"
#include "sim/UiccSlots.h"

namespace aidl::android::hardware::radio::samsung {

using ::android::base::GetProperty;
using ::ndk::ScopedAStatus;

RadioSim::RadioSim(RadioSlot& slot) : mSlot(slot) {}

ScopedAStatus RadioSim::setResponseFunctions(
        const std::shared_ptr<sim::IRadioSimResponse>& response,
        const std::shared_ptr<sim::IRadioSimIndication>& indication) {
    setCallbacks(response, indication);
    return ok();
}

ScopedAStatus RadioSim::responseAcknowledgement() {
    WakeLock::get().release();
    return ok();
}

ScopedAStatus RadioSim::areUiccApplicationsEnabled(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            SEC_REQUEST_GET_UICC_APPLICATIONS_ENABLEMENT,
            intResponse<bool>(serial, &sim::IRadioSimResponse::areUiccApplicationsEnabledResponse));
    return ok();
}

ScopedAStatus RadioSim::changeIccPin2ForApp(int32_t serial, const std::string& oldPin2,
                                            const std::string& newPin2, const std::string& aid) {
    LOG_CALL;
    mSlot.requestStrings(RIL_REQUEST_CHANGE_SIM_PIN2, {oldPin2, newPin2, aid},
                         [this, serial](auto e, auto data, auto length) {
                             respond()->changeIccPin2ForAppResponse(
                                     responseInfo(serial, e),
                                     responseInt(data, length).value_or(-1));
                         });
    return ok();
}

ScopedAStatus RadioSim::changeIccPinForApp(int32_t serial, const std::string& oldPin,
                                           const std::string& newPin, const std::string& aid) {
    LOG_CALL;
    mSlot.requestStrings(RIL_REQUEST_CHANGE_SIM_PIN, {oldPin, newPin, aid},
                         [this, serial](auto e, auto data, auto length) {
                             respond()->changeIccPinForAppResponse(
                                     responseInfo(serial, e),
                                     responseInt(data, length).value_or(-1));
                         });
    return ok();
}

ScopedAStatus RadioSim::enableUiccApplications(int32_t serial, bool enable) {
    LOG_CALL << ' ' << enable;
    mSlot.requestInts(SEC_REQUEST_ENABLE_UICC_APPLICATIONS, {enable},
                      [this, serial, enable](auto e, auto, auto) {
                          respond()->enableUiccApplicationsResponse(responseInfo(serial, e));
                          if (e == RIL_E_SUCCESS) {
                              updateUiccApplicationsEnabled(enable);
                          }
                      });
    return ok();
}

ScopedAStatus RadioSim::getAllowedCarriers(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getAllowedCarriersResponse(notSupported(serial), {}, {});
    return ok();
}

ScopedAStatus RadioSim::getCdmaSubscription(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getCdmaSubscriptionResponse(notSupported(serial), {}, {}, {}, {}, {});
    return ok();
}

ScopedAStatus RadioSim::getCdmaSubscriptionSource(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getCdmaSubscriptionSourceResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioSim::getFacilityLockForApp(int32_t serial, const std::string& facility,
                                              const std::string& password, int32_t serviceClass,
                                              const std::string& appId) {
    LOG_CALL << ' ' << facility;
    mSlot.requestStrings(RIL_REQUEST_QUERY_FACILITY_LOCK,
                         {facility, password, std::to_string(serviceClass), appId},
                         [this, serial](auto e, auto data, auto length) {
                             const auto response = responseInt(data, length);
                             respond()->getFacilityLockForAppResponse(
                                     responseInfo(serial, e, response.has_value()),
                                     response.value_or(0));
                         });
    return ok();
}

ScopedAStatus RadioSim::getIccCardStatus(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_GET_SIM_STATUS, [this, serial](auto e, auto data, auto length) {
        const auto* status = responseAs<SecCardStatus>(data, length);
        sim::CardStatus cardStatus;
        const bool valid = status != nullptr && convertCardStatus(*status, cardStatus);
        respond()->getIccCardStatusResponse(responseInfo(serial, e, valid), cardStatus);
    });
    return ok();
}

ScopedAStatus RadioSim::getImsiForApp(int32_t serial, const std::string& aid) {
    LOG_CALL;
    mSlot.requestStrings(
            RIL_REQUEST_GET_IMSI, {aid},
            [this, serial](auto e, auto data, auto) {
                respond()->getImsiForAppResponse(responseInfo(serial, e),
                                                 toString(static_cast<const char*>(data)));
            },
            false);
    return ok();
}

ScopedAStatus RadioSim::getSimPhonebookCapacity(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getSimPhonebookCapacityResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioSim::getSimPhonebookRecords(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getSimPhonebookRecordsResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioSim::iccCloseLogicalChannel(int32_t serial, int32_t channelId) {
    LOG_CALL << ' ' << channelId;
    mSlot.requestInts(
            RIL_REQUEST_SIM_CLOSE_CHANNEL, {channelId},
            statusResponse(serial, &sim::IRadioSimResponse::iccCloseLogicalChannelResponse));
    return ok();
}

ScopedAStatus RadioSim::iccIoForApp(int32_t serial, const sim::IccIo& iccIo) {
    LOG_CALL << ' ' << iccIo.command << ' ' << iccIo.fileId;
    RIL_SIM_IO_v6 io = {
            .command = iccIo.command,
            .fileid = iccIo.fileId,
            .path = cString(iccIo.path),
            .p1 = iccIo.p1,
            .p2 = iccIo.p2,
            .p3 = iccIo.p3,
            .data = cString(iccIo.data),
            .pin2 = cString(iccIo.pin2),
            .aidPtr = cString(iccIo.aid),
    };
    mSlot.request(RIL_REQUEST_SIM_IO, &io, sizeof(io),
                  [this, serial](auto e, auto data, auto length) {
                      const auto* result = responseAs<RIL_SIM_IO_Response>(data, length);
                      respond()->iccIoForAppResponse(responseInfo(serial, e, result != nullptr),
                                                     convertIccIoResult(result));
                  });
    return ok();
}

ScopedAStatus RadioSim::iccOpenLogicalChannel(int32_t serial, const std::string& aid, int32_t p2) {
    LOG_CALL << ' ' << aid << ' ' << p2;
    RIL_OpenChannelParams params = {
            .aidPtr = cString(aid),
            .p2 = p2,
    };
    mSlot.request(RIL_REQUEST_SIM_OPEN_CHANNEL, &params, sizeof(params),
                  [this, serial](auto e, auto data, auto length) {
                      const auto ints = responseInts(data, length);
                      std::vector<uint8_t> selectResponse;
                      for (size_t i = 1; i < ints.size(); i++) {
                          selectResponse.push_back(static_cast<uint8_t>(ints[i]));
                      }
                      respond()->iccOpenLogicalChannelResponse(
                              responseInfo(serial, e, !ints.empty()), ints.empty() ? -1 : ints[0],
                              selectResponse);
                  });
    return ok();
}

ScopedAStatus RadioSim::iccTransmitApduBasicChannel(int32_t serial, const sim::SimApdu& message) {
    LOG_CALL;
    transmitApdu(serial, RIL_REQUEST_SIM_TRANSMIT_APDU_BASIC, message, message.data);
    return ok();
}

ScopedAStatus RadioSim::iccTransmitApduLogicalChannel(int32_t serial, const sim::SimApdu& message) {
    LOG_CALL << ' ' << message.sessionId;
    auto apdu = message;
    // libsec-ril session ids exceed the logical channel bits the framework ORs into CLA.
    if (apdu.sessionId > 3 && (apdu.cla & apdu.sessionId) == apdu.sessionId) {
        apdu.cla &= ~apdu.sessionId;
    }
    transmitApdu(serial, RIL_REQUEST_SIM_TRANSMIT_APDU_CHANNEL, apdu,
                 addMepA1Port(apdu.data, apdu.p3));
    return ok();
}

ScopedAStatus RadioSim::reportStkServiceIsRunning(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING,
            statusResponse(serial, &sim::IRadioSimResponse::reportStkServiceIsRunningResponse));
    return ok();
}

ScopedAStatus RadioSim::requestIccSimAuthentication(int32_t serial, int32_t authContext,
                                                    const std::string& authData,
                                                    const std::string& aid) {
    LOG_CALL << ' ' << authContext << ' ' << aid;
    RIL_SimAuthentication auth = {
            .authContext = authContext,
            .authData = cString(authData),
            .aid = cString(aid),
    };
    mSlot.request(RIL_REQUEST_SIM_AUTHENTICATION, &auth, sizeof(auth),
                  [this, serial](auto e, auto data, auto length) {
                      const auto* result = responseAs<RIL_SIM_IO_Response>(data, length);
                      respond()->requestIccSimAuthenticationResponse(
                              responseInfo(serial, e, result != nullptr),
                              convertIccIoResult(result));
                  });
    return ok();
}

ScopedAStatus RadioSim::sendEnvelope(int32_t serial, const std::string& contents) {
    LOG_CALL;
    mSlot.requestString(RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND, contents,
                        [this, serial](auto e, auto data, auto) {
                            respond()->sendEnvelopeResponse(
                                    responseInfo(serial, e),
                                    toString(static_cast<const char*>(data)));
                        });
    return ok();
}

ScopedAStatus RadioSim::sendEnvelopeWithStatus(int32_t serial, const std::string& contents) {
    LOG_CALL;
    mSlot.requestString(RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS, contents,
                        [this, serial](auto e, auto data, auto length) {
                            const auto* result = responseAs<RIL_SIM_IO_Response>(data, length);
                            respond()->sendEnvelopeWithStatusResponse(
                                    responseInfo(serial, e, result != nullptr),
                                    convertIccIoResult(result));
                        });
    return ok();
}

ScopedAStatus RadioSim::sendTerminalResponseToSim(int32_t serial,
                                                  const std::string& commandResponse) {
    LOG_CALL;
    mSlot.requestString(
            RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE, commandResponse,
            statusResponse(serial, &sim::IRadioSimResponse::sendTerminalResponseToSimResponse));
    return ok();
}

ScopedAStatus RadioSim::setAllowedCarriers(int32_t serial, const sim::CarrierRestrictions& carriers,
                                           sim::SimLockMultiSimPolicy multiSimPolicy) {
    LOG_NOT_SUPPORTED;
    respond()->setAllowedCarriersResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioSim::setCarrierInfoForImsiEncryption(
        int32_t serial, const sim::ImsiEncryptionInfo& imsiEncryptionInfo) {
    LOG_NOT_SUPPORTED;
    respond()->setCarrierInfoForImsiEncryptionResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioSim::setCdmaSubscriptionSource(int32_t serial,
                                                  sim::CdmaSubscriptionSource cdmaSub) {
    LOG_NOT_SUPPORTED;
    respond()->setCdmaSubscriptionSourceResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioSim::setFacilityLockForApp(int32_t serial, const std::string& facility,
                                              bool lockState, const std::string& password,
                                              int32_t serviceClass, const std::string& appId) {
    LOG_CALL << ' ' << facility << ' ' << lockState;
    mSlot.requestStrings(
            RIL_REQUEST_SET_FACILITY_LOCK,
            {facility, lockState ? "1" : "0", password, std::to_string(serviceClass), appId},
            [this, serial](auto e, auto data, auto length) {
                respond()->setFacilityLockForAppResponse(responseInfo(serial, e),
                                                         responseInt(data, length).value_or(-1));
            });
    return ok();
}

ScopedAStatus RadioSim::setSimCardPower(int32_t serial, sim::CardPowerState powerUp) {
    LOG_NOT_SUPPORTED;
    respond()->setSimCardPowerResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioSim::setUiccSubscription(int32_t serial, const sim::SelectUiccSub& uiccSub) {
    LOG_NOT_SUPPORTED;
    respond()->setUiccSubscriptionResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioSim::supplyIccPin2ForApp(int32_t serial, const std::string& pin2,
                                            const std::string& aid) {
    LOG_CALL;
    mSlot.requestStrings(RIL_REQUEST_ENTER_SIM_PIN2, {pin2, aid},
                         [this, serial](auto e, auto data, auto length) {
                             respond()->supplyIccPin2ForAppResponse(
                                     responseInfo(serial, e),
                                     responseInt(data, length).value_or(-1));
                         });
    return ok();
}

ScopedAStatus RadioSim::supplyIccPinForApp(int32_t serial, const std::string& pin,
                                           const std::string& aid) {
    LOG_CALL;
    mSlot.requestStrings(
            RIL_REQUEST_ENTER_SIM_PIN, {pin, aid}, [this, serial](auto e, auto data, auto length) {
                respond()->supplyIccPinForAppResponse(responseInfo(serial, e),
                                                      responseInt(data, length).value_or(-1));
            });
    return ok();
}

ScopedAStatus RadioSim::supplyIccPuk2ForApp(int32_t serial, const std::string& puk2,
                                            const std::string& pin2, const std::string& aid) {
    LOG_CALL;
    mSlot.requestStrings(RIL_REQUEST_ENTER_SIM_PUK2, {puk2, pin2, aid},
                         [this, serial](auto e, auto data, auto length) {
                             respond()->supplyIccPuk2ForAppResponse(
                                     responseInfo(serial, e),
                                     responseInt(data, length).value_or(-1));
                         });
    return ok();
}

ScopedAStatus RadioSim::supplyIccPukForApp(int32_t serial, const std::string& puk,
                                           const std::string& pin, const std::string& aid) {
    LOG_CALL;
    mSlot.requestStrings(RIL_REQUEST_ENTER_SIM_PUK, {puk, pin, aid},
                         [this, serial](auto e, auto data, auto length) {
                             respond()->supplyIccPukForAppResponse(
                                     responseInfo(serial, e),
                                     responseInt(data, length).value_or(-1));
                         });
    return ok();
}

ScopedAStatus RadioSim::supplySimDepersonalization(int32_t serial, sim::PersoSubstate persoType,
                                                   const std::string& controlKey) {
    LOG_NOT_SUPPORTED;
    respond()->supplySimDepersonalizationResponse(notSupported(serial), {}, {});
    return ok();
}

ScopedAStatus RadioSim::updateSimPhonebookRecords(int32_t serial,
                                                  const sim::PhonebookRecordInfo& recordInfo) {
    LOG_NOT_SUPPORTED;
    respond()->updateSimPhonebookRecordsResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioSim::iccCloseLogicalChannelWithSessionInfo(int32_t serial,
                                                              const sim::SessionInfo& sessionInfo) {
    LOG_CALL << ' ' << sessionInfo.sessionId;
    mSlot.requestInts(
            RIL_REQUEST_SIM_CLOSE_CHANNEL, {sessionInfo.sessionId},
            statusResponse(serial,
                           &sim::IRadioSimResponse::iccCloseLogicalChannelWithSessionInfoResponse));
    return ok();
}

void RadioSim::transmitApdu(int32_t serial, int request, const sim::SimApdu& message,
                            const std::string& apduData) {
    RIL_SIM_APDU apdu = {
            .sessionid = message.sessionId,
            .cla = message.cla,
            .instruction = message.instruction,
            .p1 = message.p1,
            .p2 = message.p2,
            .p3 = message.p3,
            .data = cString(apduData),
    };
    mSlot.request(request, &apdu, sizeof(apdu),
                  [this, serial, request](auto e, auto data, auto length) {
                      const auto* result = responseAs<RIL_SIM_IO_Response>(data, length);
                      const auto info = responseInfo(serial, e, result != nullptr);
                      if (request == RIL_REQUEST_SIM_TRANSMIT_APDU_BASIC) {
                          respond()->iccTransmitApduBasicChannelResponse(
                                  info, convertIccIoResult(result));
                      } else {
                          respond()->iccTransmitApduLogicalChannelResponse(
                                  info, convertIccIoResult(result));
                      }
                  });
}

std::string RadioSim::addMepA1Port(const std::string& hex, int32_t& p3) {
    if (!hex.starts_with("BF31") ||
        GetProperty("ril.esim.mep_mode", "").find('1') == std::string::npos) {
        return hex;
    }
    const size_t length = hex.size();
    if (length >= 6 && hex.compare(length - 6, 4, "8201") == 0) {
        return hex;
    }
    unsigned contentLength = 0;
    if (length < 6 || sscanf(hex.c_str() + 4, "%2x", &contentLength) != 1 ||
        length != 6 + contentLength * 2 || contentLength + 3 > 0x7f) {
        LOG(WARNING) << "EnableProfile: unexpected length, not adding port";
        return hex;
    }
    // Framework port N is eUICC port N + 1.
    const int port = UiccSlots::get().logicalSlot(mSlot.socket()).port + 1;
    char tail[16];
    snprintf(tail, sizeof(tail), "8201%02X", port);
    char header[8];
    snprintf(header, sizeof(header), "%.4s%02X", hex.c_str(), contentLength + 3);
    LOG(INFO) << "EnableProfile: adding target port " << port;
    p3 = static_cast<int32_t>((length + 6) / 2);
    return header + hex.substr(6) + tail;
}

bool RadioSim::convertCardStatus(const SecCardStatus& status, sim::CardStatus& out) {
    const auto& base = status.base;
    const int numApps = base.num_applications;
    if (numApps < 0 || numApps > RIL_CARD_MAX_APPS ||
        base.gsm_umts_subscription_app_index >= numApps ||
        base.cdma_subscription_app_index >= numApps || base.ims_subscription_app_index >= numApps) {
        return false;
    }

    const auto logical = UiccSlots::get().logicalSlot(mSlot.socket());
    const bool mapped = logical.physicalSlot == status.physicalSlotId;
    std::string eid = toString(status.eid);
    if (eid.empty() && mapped) {
        eid = logical.eid;
    }

    out = {};
    out.cardState = base.card_state;
    out.universalPinState = static_cast<sim::PinState>(base.universal_pin_state);
    out.gsmUmtsSubscriptionAppIndex = base.gsm_umts_subscription_app_index;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    out.cdmaSubscriptionAppIndex = base.cdma_subscription_app_index;
#pragma clang diagnostic pop
    out.imsSubscriptionAppIndex = base.ims_subscription_app_index;
    out.atr = toString(status.atr);
    out.iccid = toString(status.iccid);
    out.eid = eid;
    out.slotMap = {
            .physicalSlotId = status.physicalSlotId,
            .portId = mapped ? logical.port : 0,
    };
    for (int i = 0; i < numApps; i++) {
        const auto& app = base.applications[i];
        out.applications.push_back({
                .appType = app.app_type,
                .appState = app.app_state,
                .persoSubstate = static_cast<sim::PersoSubstate>(app.perso_substate),
                .aidPtr = toString(app.aid_ptr),
                .appLabelPtr = toString(app.app_label_ptr),
                .pin1Replaced = app.pin1_replaced != 0,
                .pin1 = static_cast<sim::PinState>(app.pin1),
                .pin2 = static_cast<sim::PinState>(app.pin2),
        });
    }
    return true;
}

sim::IccIoResult RadioSim::convertIccIoResult(const RIL_SIM_IO_Response* result) {
    if (result == nullptr) {
        return {};
    }
    return {
            .sw1 = result->sw1,
            .sw2 = result->sw2,
            .simResponse = toString(result->simResponse),
    };
}

// libsec-ril only reports enablement changes on One UI 6.1+, so track them here.
void RadioSim::queryUiccApplicationsEnabled() {
    // Runs on libsec-ril's indication thread, which must not send requests.
    RadioSlot::VendorRil::get().post([this] {
        mSlot.request(SEC_REQUEST_GET_UICC_APPLICATIONS_ENABLEMENT,
                      [this](auto e, auto data, auto length) {
                          const auto enabled = responseInt(data, length);
                          if (e == RIL_E_SUCCESS && enabled.has_value()) {
                              updateUiccApplicationsEnabled(*enabled != 0);
                          }
                      });
    });
}

void RadioSim::updateUiccApplicationsEnabled(bool enabled) {
    {
        std::lock_guard lock(mUiccMutex);
        if (mUiccApplicationsEnabled == enabled) {
            return;
        }
        mUiccApplicationsEnabled = enabled;
    }
    if (const auto indication = indicate()) {
        indication->uiccApplicationsEnablementChanged(kUnsolicited, enabled);
    }
}

bool RadioSim::onUnsolicited(int id, const void* data, size_t length, RadioIndicationType type) {
    const auto indication = indicate();
    if (indication == nullptr) {
        return false;
    }
    switch (id) {
        case RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED:
            indication->simStatusChanged(type);
            queryUiccApplicationsEnabled();
            return true;
        case RIL_UNSOL_SIM_REFRESH:
            if (const auto* refresh = responseAs<RIL_SimRefreshResponse_v7>(data, length)) {
                indication->simRefresh(type, {
                                                     .type = refresh->result,
                                                     .efId = refresh->ef_id,
                                                     .aid = toString(refresh->aid),
                                             });
            }
            return true;
        case RIL_UNSOL_STK_SESSION_END:
            indication->stkSessionEnd(type);
            return true;
        case RIL_UNSOL_STK_PROACTIVE_COMMAND:
            indication->stkProactiveCommand(type, toString(static_cast<const char*>(data)));
            return true;
        case RIL_UNSOL_STK_EVENT_NOTIFY:
            indication->stkEventNotify(type, toString(static_cast<const char*>(data)));
            return true;
        case RIL_UNSOL_UICC_SUBSCRIPTION_STATUS_CHANGED:
            if (const auto activate = responseInt(data, length)) {
                indication->subscriptionStatusChanged(type, *activate != 0);
            }
            return true;
        case SEC_UNSOL_UICC_APPLICATIONS_ENABLEMENT_CHANGED:
            if (const auto enabled = responseInt(data, length)) {
                updateUiccApplicationsEnabled(*enabled != 0);
            }
            return false;
        default:
            LOG(DEBUG) << "Unhandled unsolicited " << id << " (" << length << " bytes)";
    }
    return false;
}

}  // namespace aidl::android::hardware::radio::samsung
