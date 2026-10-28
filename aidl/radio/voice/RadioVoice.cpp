/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungVoice"

#include "voice/RadioVoice.h"

#include <samsung_ril/voice.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iterator>

#include "RadioSlot.h"
#include "voice/CallStatePublisher.h"

namespace aidl::android::hardware::radio::samsung {

using ::ndk::ScopedAStatus;

namespace {

std::vector<voice::UusInfo> toUusInfo(const RIL_UUS_Info* uus) {
    if (uus == nullptr) {
        return {};
    }
    return {{
            .uusType = static_cast<int32_t>(uus->uusType),
            .uusDcs = static_cast<int32_t>(uus->uusDcs),
            .uusData = uus->uusData != nullptr ? std::string(uus->uusData, uus->uusLength) : "",
    }};
}

voice::Call toCall(const SecCall& call) {
    return {
            .state = call.state,
            .index = call.index,
            .toa = call.toa,
            .isMpty = call.isMpty != 0,
            .isMT = call.isMT != 0,
            .als = static_cast<int8_t>(call.als),
            .isVoice = call.isVoice != 0,
            .isVoicePrivacy = call.isVoicePrivacy != 0,
            .number = toString(call.number),
            .numberPresentation = call.numberPresentation,
            .name = toString(call.name),
            .namePresentation = call.namePresentation,
            .uusInfo = toUusInfo(call.uusInfo),
            .audioQuality = static_cast<voice::AudioQuality>(call.audioQuality),
            .forwardedNumber = toString(call.forwardedNumber),
    };
}

voice::CallForwardInfo toCallForwardInfo(const RIL_CallForwardInfo& info) {
    return {
            .status = info.status,
            .reason = info.reason,
            .serviceClass = info.serviceClass,
            .toa = info.toa,
            .number = toString(info.number),
            .timeSeconds = info.timeSeconds,
    };
}

RIL_CallForwardInfo toRilCallForwardInfo(const voice::CallForwardInfo& info) {
    return {
            .status = info.status,
            .reason = info.reason,
            .serviceClass = info.serviceClass,
            .toa = info.toa,
            .number = cString(info.number),
            .timeSeconds = info.timeSeconds,
    };
}

struct DialRequest {
    explicit DialRequest(const voice::Dial& dialInfo)
        : dial{.address = cString(dialInfo.address), .clir = dialInfo.clir} {
        if (!dialInfo.uusInfo.empty()) {
            const auto& info = dialInfo.uusInfo[0];
            uus = {
                    .uusType = static_cast<RIL_UUS_Type>(info.uusType),
                    .uusDcs = static_cast<RIL_UUS_DCS>(info.uusDcs),
                    .uusData = cString(info.uusData),
                    .uusLength = static_cast<int>(info.uusData.size()),
            };
            dial.uusInfo = &uus;
        }
    }

    RIL_Dial dial;
    RIL_UUS_Info uus = {};
};

bool isCallForwardQuery(const RIL_StkCcUnsolSsResponse& ss) {
    return ss.serviceType >= SS_CFU && ss.serviceType <= SS_CF_ALL_CONDITIONAL &&
           ss.requestType == SS_INTERROGATION;
}

voice::StkCcUnsolSsResult toSsResult(const RIL_StkCcUnsolSsResponse& ss) {
    voice::StkCcUnsolSsResult result = {
            .serviceType = ss.serviceType,
            .requestType = ss.requestType,
            .teleserviceType = ss.teleserviceType,
            .serviceClass = ss.serviceClass,
            .result = static_cast<RadioError>(ss.result),
    };
    if (isCallForwardQuery(ss)) {
        voice::CfData cfData;
        const auto count = std::clamp(ss.cfData.numValidIndexes, 0,
                                      static_cast<int>(std::size(ss.cfData.cfInfo)));
        for (int i = 0; i < count; i++) {
            cfData.cfInfo.push_back(toCallForwardInfo(ss.cfData.cfInfo[i]));
        }
        result.cfData.push_back(std::move(cfData));
    } else {
        result.ssInfo.push_back({.ssInfo = {std::begin(ss.ssInfo), std::end(ss.ssInfo)}});
    }
    return result;
}

std::vector<voice::EmergencyNumber> toEmergencyNumbers(const void* data, size_t length) {
    const auto* numbers = static_cast<const SecEmergencyNumber*>(data);
    std::vector<voice::EmergencyNumber> list;
    for (size_t i = 0; i < length / sizeof(SecEmergencyNumber); i++) {
        const auto& number = numbers[i];
        voice::EmergencyNumber out = {
                .number = fixedString(number.number),
                .mcc = fixedString(number.mcc),
                .mnc = fixedString(number.mnc),
                .categories = number.categories,
                .sources = number.sources,
        };
        if (auto urn = fixedString(number.urn); !urn.empty()) {
            out.urns.push_back(std::move(urn));
        }
        list.push_back(std::move(out));
    }
    return list;
}

}  // namespace

RadioVoice::RadioVoice(RadioSlot& slot) : mSlot(slot) {}

ScopedAStatus RadioVoice::setResponseFunctions(
        const std::shared_ptr<voice::IRadioVoiceResponse>& response,
        const std::shared_ptr<voice::IRadioVoiceIndication>& indication) {
    setCallbacks(response, indication);
    return ok();
}

ScopedAStatus RadioVoice::responseAcknowledgement() {
    return ok();
}

ScopedAStatus RadioVoice::acceptCall(int32_t serial) {
    LOG_CALL;
    CallStatePublisher::get().invalidate(mSlot.socket());
    mSlot.request(RIL_REQUEST_ANSWER,
                  statusResponse(serial, &voice::IRadioVoiceResponse::acceptCallResponse));
    return ok();
}

ScopedAStatus RadioVoice::cancelPendingUssd(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_CANCEL_USSD,
                  statusResponse(serial, &voice::IRadioVoiceResponse::cancelPendingUssdResponse));
    return ok();
}

ScopedAStatus RadioVoice::conference(int32_t serial) {
    LOG_CALL;
    CallStatePublisher::get().invalidate(mSlot.socket());
    mSlot.request(RIL_REQUEST_CONFERENCE,
                  statusResponse(serial, &voice::IRadioVoiceResponse::conferenceResponse));
    return ok();
}

ScopedAStatus RadioVoice::dial(int32_t serial, const voice::Dial& dialInfo) {
    LOG_CALL;
    CallStatePublisher::get().invalidate(mSlot.socket());
    DialRequest request(dialInfo);
    mSlot.request(RIL_REQUEST_DIAL, &request.dial, sizeof(request.dial),
                  statusResponse(serial, &voice::IRadioVoiceResponse::dialResponse));
    return ok();
}

ScopedAStatus RadioVoice::emergencyDial(int32_t serial, const voice::Dial& dialInfo,
                                        int32_t categories, const std::vector<std::string>& urns,
                                        voice::EmergencyCallRouting routing,
                                        bool hasKnownUserIntentEmergency, bool isTesting) {
    LOG_CALL << ' ' << categories << ' ' << toString(routing) << ' ' << isTesting;
    CallStatePublisher::get().invalidate(mSlot.socket());
    DialRequest request(dialInfo);
    // libsec-ril takes a single URN.
    SecEmergencyDial emergency = {
            .dialInfo = request.dial,
            .categories = categories,
            .routing = static_cast<int>(routing),
            .urn = urns.empty() ? nullptr : cString(urns[0]),
            .hasKnownUserIntentEmergency = hasKnownUserIntentEmergency,
            .isTesting = isTesting,
    };
    mSlot.request(SEC_REQUEST_EMERGENCY_DIAL, &emergency, sizeof(emergency),
                  statusResponse(serial, &voice::IRadioVoiceResponse::emergencyDialResponse));
    return ok();
}

ScopedAStatus RadioVoice::exitEmergencyCallbackMode(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE,
            statusResponse(serial, &voice::IRadioVoiceResponse::exitEmergencyCallbackModeResponse));
    return ok();
}

ScopedAStatus RadioVoice::explicitCallTransfer(int32_t serial) {
    LOG_CALL;
    CallStatePublisher::get().invalidate(mSlot.socket());
    mSlot.request(
            RIL_REQUEST_EXPLICIT_CALL_TRANSFER,
            statusResponse(serial, &voice::IRadioVoiceResponse::explicitCallTransferResponse));
    return ok();
}

ScopedAStatus RadioVoice::getCallForwardStatus(int32_t serial,
                                               const voice::CallForwardInfo& callInfo) {
    LOG_CALL << ' ' << callInfo.reason << ' ' << callInfo.serviceClass;
    auto request = toRilCallForwardInfo(callInfo);
    mSlot.request(RIL_REQUEST_QUERY_CALL_FORWARD_STATUS, &request, sizeof(request),
                  [this, serial](auto e, auto data, auto length) {
                      const bool valid = length % sizeof(RIL_CallForwardInfo*) == 0 &&
                                         (data != nullptr || length == 0);
                      std::vector<voice::CallForwardInfo> infos;
                      const auto* list = static_cast<RIL_CallForwardInfo* const*>(data);
                      for (size_t i = 0; valid && i < length / sizeof(RIL_CallForwardInfo*); i++) {
                          if (list[i] != nullptr) {
                              infos.push_back(toCallForwardInfo(*list[i]));
                          }
                      }
                      respond()->getCallForwardStatusResponse(responseInfo(serial, e, valid),
                                                              infos);
                  });
    return ok();
}

ScopedAStatus RadioVoice::getCallWaiting(int32_t serial, int32_t serviceClass) {
    LOG_CALL << ' ' << serviceClass;
    mSlot.requestInts(RIL_REQUEST_QUERY_CALL_WAITING, {serviceClass},
                      [this, serial](auto e, auto data, auto length) {
                          const auto ints = responseInts(data, length);
                          const bool valid = ints.size() == 2;
                          respond()->getCallWaitingResponse(responseInfo(serial, e, valid),
                                                            valid && ints[0] == 1,
                                                            valid ? ints[1] : 0);
                      });
    return ok();
}

ScopedAStatus RadioVoice::getClip(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            RIL_REQUEST_QUERY_CLIP,
            intResponse<voice::ClipStatus>(serial, &voice::IRadioVoiceResponse::getClipResponse));
    return ok();
}

ScopedAStatus RadioVoice::getClir(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_GET_CLIR, [this, serial](auto e, auto data, auto length) {
        const auto ints = responseInts(data, length);
        const bool valid = ints.size() == 2;
        respond()->getClirResponse(responseInfo(serial, e, valid), valid ? ints[0] : 0,
                                   valid ? ints[1] : 0);
    });
    return ok();
}

ScopedAStatus RadioVoice::getCurrentCalls(int32_t serial) {
    LOG_CALL;
    const auto query = CallStatePublisher::get().beginQuery(mSlot.socket());
    mSlot.request(RIL_REQUEST_GET_CURRENT_CALLS, [this, serial, query](auto e, auto data,
                                                                       auto length) {
        CallStatePublisher::get().completeQuery(mSlot.socket(), query, e, data, length);
        const bool valid = length % sizeof(SecCall*) == 0 && (data != nullptr || length == 0);
        std::vector<voice::Call> calls;
        const auto* list = static_cast<SecCall* const*>(data);
        for (size_t i = 0; valid && i < length / sizeof(SecCall*); i++) {
            if (list[i] != nullptr) {
                calls.push_back(toCall(*list[i]));
            }
        }
        respond()->getCurrentCallsResponse(responseInfo(serial, e, valid), calls);
    });
    return ok();
}

ScopedAStatus RadioVoice::getLastCallFailCause(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_LAST_CALL_FAIL_CAUSE, [this, serial](auto e, auto data, auto length) {
        voice::LastCallFailCauseInfo info;
        bool valid = data != nullptr;
        if (valid && length == sizeof(RIL_LastCallFailCauseInfo)) {
            const auto* cause = static_cast<const RIL_LastCallFailCauseInfo*>(data);
            info.causeCode = static_cast<voice::LastCallFailCause>(cause->cause_code);
            info.vendorCause = toString(cause->vendor_cause);
        } else if (valid && length == sizeof(int)) {
            info.causeCode = static_cast<voice::LastCallFailCause>(*static_cast<const int*>(data));
        } else {
            valid = false;
        }
        respond()->getLastCallFailCauseResponse(responseInfo(serial, e, valid), info);
    });
    return ok();
}

ScopedAStatus RadioVoice::getMute(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_GET_MUTE,
                  intResponse<bool>(serial, &voice::IRadioVoiceResponse::getMuteResponse));
    return ok();
}

ScopedAStatus RadioVoice::getPreferredVoicePrivacy(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE,
                  intResponse<bool>(serial,
                                    &voice::IRadioVoiceResponse::getPreferredVoicePrivacyResponse));
    return ok();
}

ScopedAStatus RadioVoice::getTtyMode(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            RIL_REQUEST_QUERY_TTY_MODE,
            intResponse<voice::TtyMode>(serial, &voice::IRadioVoiceResponse::getTtyModeResponse));
    return ok();
}

ScopedAStatus RadioVoice::handleStkCallSetupRequestFromSim(int32_t serial, bool accept) {
    LOG_CALL << ' ' << accept;
    mSlot.requestInts(
            RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM, {accept},
            statusResponse(serial,
                           &voice::IRadioVoiceResponse::handleStkCallSetupRequestFromSimResponse));
    return ok();
}

ScopedAStatus RadioVoice::hangup(int32_t serial, int32_t gsmIndex) {
    LOG_CALL << ' ' << gsmIndex;
    CallStatePublisher::get().invalidate(mSlot.socket());
    mSlot.requestInts(
            RIL_REQUEST_HANGUP, {gsmIndex},
            statusResponse(serial, &voice::IRadioVoiceResponse::hangupConnectionResponse));
    return ok();
}

ScopedAStatus RadioVoice::hangupForegroundResumeBackground(int32_t serial) {
    LOG_CALL;
    CallStatePublisher::get().invalidate(mSlot.socket());
    mSlot.request(
            RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND,
            statusResponse(serial,
                           &voice::IRadioVoiceResponse::hangupForegroundResumeBackgroundResponse));
    return ok();
}

ScopedAStatus RadioVoice::hangupWaitingOrBackground(int32_t serial) {
    LOG_CALL;
    CallStatePublisher::get().invalidate(mSlot.socket());
    mSlot.request(
            RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND,
            statusResponse(serial, &voice::IRadioVoiceResponse::hangupWaitingOrBackgroundResponse));
    return ok();
}

ScopedAStatus RadioVoice::isVoNrEnabled(int32_t serial) {
    LOG_CALL;
    mSlot.request(SEC_REQUEST_IS_VONR_ENABLED,
                  intResponse<bool>(serial, &voice::IRadioVoiceResponse::isVoNrEnabledResponse));
    return ok();
}

ScopedAStatus RadioVoice::rejectCall(int32_t serial) {
    LOG_CALL;
    CallStatePublisher::get().invalidate(mSlot.socket());
    mSlot.request(RIL_REQUEST_UDUB,
                  statusResponse(serial, &voice::IRadioVoiceResponse::rejectCallResponse));
    return ok();
}

ScopedAStatus RadioVoice::sendBurstDtmf(int32_t serial, const std::string& dtmf, int32_t on,
                                        int32_t off) {
    LOG_CALL << ' ' << on << ' ' << off;
    mSlot.requestStrings(
            RIL_REQUEST_CDMA_BURST_DTMF, {dtmf, std::to_string(on), std::to_string(off)},
            statusResponse(serial, &voice::IRadioVoiceResponse::sendBurstDtmfResponse), false);
    return ok();
}

ScopedAStatus RadioVoice::sendCdmaFeatureCode(int32_t serial, const std::string& featureCode) {
    LOG_CALL;
    mSlot.requestString(
            RIL_REQUEST_CDMA_FLASH, featureCode,
            statusResponse(serial, &voice::IRadioVoiceResponse::sendCdmaFeatureCodeResponse));
    return ok();
}

ScopedAStatus RadioVoice::sendDtmf(int32_t serial, const std::string& s) {
    LOG_CALL;
    mSlot.requestString(RIL_REQUEST_DTMF, s,
                        statusResponse(serial, &voice::IRadioVoiceResponse::sendDtmfResponse));
    return ok();
}

ScopedAStatus RadioVoice::sendUssd(int32_t serial, const std::string& ussd) {
    LOG_CALL;
    mSlot.requestString(RIL_REQUEST_SEND_USSD, ussd,
                        statusResponse(serial, &voice::IRadioVoiceResponse::sendUssdResponse));
    return ok();
}

ScopedAStatus RadioVoice::separateConnection(int32_t serial, int32_t gsmIndex) {
    LOG_CALL << ' ' << gsmIndex;
    CallStatePublisher::get().invalidate(mSlot.socket());
    mSlot.requestInts(
            RIL_REQUEST_SEPARATE_CONNECTION, {gsmIndex},
            statusResponse(serial, &voice::IRadioVoiceResponse::separateConnectionResponse));
    return ok();
}

ScopedAStatus RadioVoice::setCallForward(int32_t serial, const voice::CallForwardInfo& callInfo) {
    LOG_CALL << ' ' << callInfo.status << ' ' << callInfo.reason;
    auto request = toRilCallForwardInfo(callInfo);
    mSlot.request(RIL_REQUEST_SET_CALL_FORWARD, &request, sizeof(request),
                  statusResponse(serial, &voice::IRadioVoiceResponse::setCallForwardResponse));
    return ok();
}

ScopedAStatus RadioVoice::setCallWaiting(int32_t serial, bool enable, int32_t serviceClass) {
    LOG_CALL << ' ' << enable << ' ' << serviceClass;
    mSlot.requestInts(RIL_REQUEST_SET_CALL_WAITING, {enable, serviceClass},
                      statusResponse(serial, &voice::IRadioVoiceResponse::setCallWaitingResponse));
    return ok();
}

ScopedAStatus RadioVoice::setClir(int32_t serial, int32_t status) {
    LOG_CALL << ' ' << status;
    mSlot.requestInts(RIL_REQUEST_SET_CLIR, {status},
                      statusResponse(serial, &voice::IRadioVoiceResponse::setClirResponse));
    return ok();
}

ScopedAStatus RadioVoice::setMute(int32_t serial, bool enable) {
    LOG_CALL << ' ' << enable;
    mSlot.requestInts(RIL_REQUEST_SET_MUTE, {enable},
                      statusResponse(serial, &voice::IRadioVoiceResponse::setMuteResponse));
    return ok();
}

ScopedAStatus RadioVoice::setPreferredVoicePrivacy(int32_t serial, bool enable) {
    LOG_CALL << ' ' << enable;
    mSlot.requestInts(
            RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE, {enable},
            statusResponse(serial, &voice::IRadioVoiceResponse::setPreferredVoicePrivacyResponse));
    return ok();
}

ScopedAStatus RadioVoice::setTtyMode(int32_t serial, voice::TtyMode mode) {
    LOG_CALL << ' ' << toString(mode);
    mSlot.requestInts(RIL_REQUEST_SET_TTY_MODE, {static_cast<int>(mode)},
                      statusResponse(serial, &voice::IRadioVoiceResponse::setTtyModeResponse));
    return ok();
}

ScopedAStatus RadioVoice::setVoNrEnabled(int32_t serial, bool enable) {
    LOG_CALL << ' ' << enable;
    mSlot.requestInts(SEC_REQUEST_ENABLE_VONR, {enable},
                      statusResponse(serial, &voice::IRadioVoiceResponse::setVoNrEnabledResponse));
    return ok();
}

ScopedAStatus RadioVoice::startDtmf(int32_t serial, const std::string& s) {
    LOG_CALL;
    mSlot.requestString(RIL_REQUEST_DTMF_START, s,
                        statusResponse(serial, &voice::IRadioVoiceResponse::startDtmfResponse));
    return ok();
}

ScopedAStatus RadioVoice::stopDtmf(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_DTMF_STOP,
                  statusResponse(serial, &voice::IRadioVoiceResponse::stopDtmfResponse));
    return ok();
}

ScopedAStatus RadioVoice::switchWaitingOrHoldingAndActive(int32_t serial) {
    LOG_CALL;
    CallStatePublisher::get().invalidate(mSlot.socket());
    mSlot.request(
            RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE,
            statusResponse(serial,
                           &voice::IRadioVoiceResponse::switchWaitingOrHoldingAndActiveResponse));
    return ok();
}

bool RadioVoice::onUnsolicited(int id, const void* data, size_t length, RadioIndicationType type) {
    if (id == RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED) {
        CallStatePublisher::get().invalidate(mSlot.socket());
    }
    const auto indication = indicate();
    if (indication == nullptr) {
        return false;
    }
    switch (id) {
        case RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED:
            indication->callStateChanged(type);
            return true;
        case RIL_UNSOL_ON_USSD:
        case RIL_UNSOL_ON_USSD_REQUEST: {
            const auto* strings = static_cast<char* const*>(data);
            if (strings != nullptr && length == 2 * sizeof(char*)) {
                indication->onUssd(type,
                                   static_cast<voice::UssdModeType>(
                                           strings[0] != nullptr ? atoi(strings[0]) : 0),
                                   toString(strings[1]));
            }
            return true;
        }
        case RIL_UNSOL_STK_CALL_SETUP:
            if (const auto timeout = responseInt(data, length)) {
                indication->stkCallSetup(type, *timeout);
            }
            return true;
        case RIL_UNSOL_CALL_RING: {
            const auto* record = responseAs<RIL_CDMA_SignalInfoRecord>(data, length);
            voice::CdmaSignalInfoRecord info;
            if (record != nullptr) {
                info = {
                        .isPresent = record->isPresent != 0,
                        .signalType = static_cast<int8_t>(record->signalType),
                        .alertPitch = static_cast<int8_t>(record->alertPitch),
                        .signal = static_cast<int8_t>(record->signal),
                };
            }
            indication->callRing(type, record == nullptr, info);
            return true;
        }
        case RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE:
            indication->enterEmergencyCallbackMode(type);
            return true;
        case RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE:
            indication->exitEmergencyCallbackMode(type);
            return true;
        case RIL_UNSOL_RINGBACK_TONE:
            if (const auto start = responseInt(data, length)) {
                indication->indicateRingbackTone(type, *start != 0);
            }
            return true;
        case RIL_UNSOL_RESEND_INCALL_MUTE:
            indication->resendIncallMute(type);
            return true;
        case RIL_UNSOL_SRVCC_STATE_NOTIFY:
            if (const auto state = responseInt(data, length)) {
                indication->srvccStateNotify(type, static_cast<voice::SrvccState>(*state));
            }
            return true;
        case RIL_UNSOL_ON_SS:
            if (const auto* ss = responseAs<RIL_StkCcUnsolSsResponse>(data, length)) {
                indication->onSupplementaryServiceIndication(type, toSsResult(*ss));
            }
            return true;
        case RIL_UNSOL_STK_CC_ALPHA_NOTIFY:
            indication->stkCallControlAlphaNotify(type, toString(static_cast<const char*>(data)));
            return true;
        case SEC_UNSOL_EMERGENCY_NUMBER_LIST:
            if (data == nullptr) {
                return true;
            }
            if (length % sizeof(SecEmergencyNumber) != 0) {
                LOG(ERROR) << "Invalid emergency number list length " << length;
                return true;
            }
            indication->currentEmergencyNumberList(type, toEmergencyNumbers(data, length));
            return true;
        default:
            LOG(DEBUG) << "Unhandled unsolicited " << id << " (" << length << " bytes)";
    }
    return false;
}

}  // namespace aidl::android::hardware::radio::samsung
