/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungMessaging"

#include "messaging/RadioMessaging.h"

#include <android-base/strings.h>

#include <cctype>
#include <cstring>

#include "RadioSlot.h"
#include "WakeLock.h"
#include "messaging/SmscAddress.h"

namespace aidl::android::hardware::radio::samsung {

using ::ndk::ScopedAStatus;

namespace {

std::vector<uint8_t> hexToBytes(const char* hex) {
    std::vector<uint8_t> bytes;
    if (hex == nullptr) {
        return bytes;
    }
    const size_t length = strlen(hex);
    const auto nibble = [](char c) {
        return std::isdigit(c) ? c - '0' : std::tolower(c) - 'a' + 10;
    };
    for (size_t i = 0; i + 1 < length; i += 2) {
        if (!std::isxdigit(hex[i]) || !std::isxdigit(hex[i + 1])) {
            LOG(ERROR) << "Invalid hex PDU";
            return {};
        }
        bytes.push_back(nibble(hex[i]) << 4 | nibble(hex[i + 1]));
    }
    return bytes;
}

messaging::SendSmsResult toSendSmsResult(const RIL_SMS_Response& response) {
    return {
            .messageRef = response.messageRef,
            .ackPDU = toString(response.ackPDU),
            .errorCode = response.errorCode,
    };
}

}  // namespace

RadioMessaging::RadioMessaging(RadioSlot& slot) : mSlot(slot) {}

ScopedAStatus RadioMessaging::setResponseFunctions(
        const std::shared_ptr<messaging::IRadioMessagingResponse>& response,
        const std::shared_ptr<messaging::IRadioMessagingIndication>& indication) {
    setCallbacks(response, indication);
    return ok();
}

ScopedAStatus RadioMessaging::responseAcknowledgement() {
    WakeLock::get().release();
    return ok();
}

void RadioMessaging::sendGsmSms(int id, const messaging::GsmSmsMessage& message,
                                SendSmsCallback callback) {
    // libsec-ril only takes a BCD SCA; NULL selects the SIM's default.
    std::string smsc;
    if (!message.smscPdu.empty() && !normalizeSmscPdu(message.smscPdu, &smsc)) {
        LOG(ERROR) << "Invalid SMSC " << message.smscPdu;
        callback(RIL_E_INVALID_ARGUMENTS, true, {});
        return;
    }
    const char* request[] = {cString(smsc), cString(message.pdu)};
    mSlot.request(id, request, sizeof(request), [callback](auto e, auto data, auto length) {
        const auto* response = responseAs<RIL_SMS_Response>(data, length);
        callback(e, response != nullptr,
                 response != nullptr ? toSendSmsResult(*response) : messaging::SendSmsResult{});
    });
}

ScopedAStatus RadioMessaging::acknowledgeIncomingGsmSmsWithPdu(int32_t serial, bool success,
                                                               const std::string& ackPdu) {
    LOG_CALL << ' ' << success;
    mSlot.requestStrings(
            RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU, {success ? "1" : "0", ackPdu},
            statusResponse(
                    serial,
                    &messaging::IRadioMessagingResponse::acknowledgeIncomingGsmSmsWithPduResponse),
            false);
    return ok();
}

ScopedAStatus RadioMessaging::acknowledgeLastIncomingCdmaSms(int32_t serial,
                                                             const messaging::CdmaSmsAck& smsAck) {
    LOG_NOT_SUPPORTED;
    respond()->acknowledgeLastIncomingCdmaSmsResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioMessaging::acknowledgeLastIncomingGsmSms(
        int32_t serial, bool success, messaging::SmsAcknowledgeFailCause cause) {
    LOG_CALL << ' ' << success << ' ' << toString(cause);
    mSlot.requestInts(
            RIL_REQUEST_SMS_ACKNOWLEDGE, {success, static_cast<int>(cause)},
            statusResponse(
                    serial,
                    &messaging::IRadioMessagingResponse::acknowledgeLastIncomingGsmSmsResponse));
    return ok();
}

ScopedAStatus RadioMessaging::deleteSmsOnRuim(int32_t serial, int32_t index) {
    LOG_NOT_SUPPORTED;
    respond()->deleteSmsOnRuimResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioMessaging::deleteSmsOnSim(int32_t serial, int32_t index) {
    LOG_CALL << ' ' << index;
    mSlot.requestInts(
            RIL_REQUEST_DELETE_SMS_ON_SIM, {index},
            statusResponse(serial, &messaging::IRadioMessagingResponse::deleteSmsOnSimResponse));
    return ok();
}

ScopedAStatus RadioMessaging::getCdmaBroadcastConfig(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getCdmaBroadcastConfigResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioMessaging::getGsmBroadcastConfig(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG, [this, serial](auto e, auto data,
                                                                           auto length) {
        const bool valid = length % sizeof(RIL_GSM_BroadcastSmsConfigInfo*) == 0 &&
                           (data != nullptr || length == 0);
        std::vector<messaging::GsmBroadcastSmsConfigInfo> configs;
        const auto* list = static_cast<RIL_GSM_BroadcastSmsConfigInfo* const*>(data);
        const size_t count = length / sizeof(RIL_GSM_BroadcastSmsConfigInfo*);
        for (size_t i = 0; valid && i < count; i++) {
            if (list[i] == nullptr) {
                continue;
            }
            configs.push_back({
                    .fromServiceId = list[i]->fromServiceId,
                    .toServiceId = list[i]->toServiceId,
                    .fromCodeScheme = list[i]->fromCodeScheme,
                    .toCodeScheme = list[i]->toCodeScheme,
                    .selected = list[i]->selected != 0,
            });
        }
        respond()->getGsmBroadcastConfigResponse(responseInfo(serial, e, valid), configs);
    });
    return ok();
}

ScopedAStatus RadioMessaging::getSmscAddress(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_GET_SMSC_ADDRESS, [this, serial](auto e, auto data, auto length) {
        // "+CSCA:" text, a raw SCA or digits without their '+'.
        constexpr size_t kMaxSmscLength = 128;
        std::string address;
        const auto* text = static_cast<const char*>(data);
        const bool valid = text != nullptr && length > 0 && length <= kMaxSmscLength &&
                           normalizeSmscAddress({text, strnlen(text, length)}, &address);
        respond()->getSmscAddressResponse(responseInfo(serial, e, valid), address);
    });
    return ok();
}

ScopedAStatus RadioMessaging::reportSmsMemoryStatus(int32_t serial, bool available) {
    LOG_CALL << ' ' << available;
    mSlot.requestInts(
            RIL_REQUEST_REPORT_SMS_MEMORY_STATUS, {available},
            statusResponse(serial,
                           &messaging::IRadioMessagingResponse::reportSmsMemoryStatusResponse));
    return ok();
}

ScopedAStatus RadioMessaging::sendCdmaSms(int32_t serial, const messaging::CdmaSmsMessage& sms) {
    LOG_NOT_SUPPORTED;
    respond()->sendCdmaSmsResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioMessaging::sendCdmaSmsExpectMore(int32_t serial,
                                                    const messaging::CdmaSmsMessage& sms) {
    LOG_NOT_SUPPORTED;
    respond()->sendCdmaSmsExpectMoreResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioMessaging::sendImsSms(int32_t serial, const messaging::ImsSmsMessage& message) {
    LOG_CALL << ' ' << toString(message.tech) << ' ' << message.retry;
    // The modem has no IMS registration, so send over CS.
    if (message.tech != RadioTechnologyFamily::THREE_GPP || message.gsmMessage.empty()) {
        respond()->sendImsSmsResponse(responseInfo(serial, RadioError::INVALID_ARGUMENTS), {});
        return ok();
    }
    sendGsmSms(RIL_REQUEST_SEND_SMS, message.gsmMessage[0],
               [this, serial](RIL_Errno e, bool valid, const messaging::SendSmsResult& result) {
                   respond()->sendImsSmsResponse(responseInfo(serial, e, valid), result);
               });
    return ok();
}

ScopedAStatus RadioMessaging::sendSms(int32_t serial, const messaging::GsmSmsMessage& message) {
    LOG_CALL;
    sendGsmSms(RIL_REQUEST_SEND_SMS, message,
               [this, serial](RIL_Errno e, bool valid, const messaging::SendSmsResult& result) {
                   respond()->sendSmsResponse(responseInfo(serial, e, valid), result);
               });
    return ok();
}

ScopedAStatus RadioMessaging::sendSmsExpectMore(int32_t serial,
                                                const messaging::GsmSmsMessage& message) {
    LOG_CALL;
    sendGsmSms(RIL_REQUEST_SEND_SMS_EXPECT_MORE, message,
               [this, serial](RIL_Errno e, bool valid, const messaging::SendSmsResult& result) {
                   respond()->sendSmsExpectMoreResponse(responseInfo(serial, e, valid), result);
               });
    return ok();
}

ScopedAStatus RadioMessaging::setCdmaBroadcastActivation(int32_t serial, bool activate) {
    LOG_NOT_SUPPORTED;
    respond()->setCdmaBroadcastActivationResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioMessaging::setCdmaBroadcastConfig(
        int32_t serial, const std::vector<messaging::CdmaBroadcastSmsConfigInfo>& configInfo) {
    LOG_NOT_SUPPORTED;
    respond()->setCdmaBroadcastConfigResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioMessaging::setGsmBroadcastActivation(int32_t serial, bool activate) {
    LOG_CALL << ' ' << activate;
    // RIL takes "0 = activate".
    mSlot.requestInts(
            RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION, {!activate},
            statusResponse(serial,
                           &messaging::IRadioMessagingResponse::setGsmBroadcastActivationResponse));
    return ok();
}

ScopedAStatus RadioMessaging::setGsmBroadcastConfig(
        int32_t serial, const std::vector<messaging::GsmBroadcastSmsConfigInfo>& configInfo) {
    LOG_CALL << ' ' << configInfo.size();
    std::vector<RIL_GSM_BroadcastSmsConfigInfo> configs;
    for (const auto& config : configInfo) {
        configs.push_back({
                .fromServiceId = config.fromServiceId,
                .toServiceId = config.toServiceId,
                .fromCodeScheme = config.fromCodeScheme,
                .toCodeScheme = config.toCodeScheme,
                .selected = config.selected,
        });
    }
    std::vector<RIL_GSM_BroadcastSmsConfigInfo*> pointers;
    for (auto& config : configs) {
        pointers.push_back(&config);
    }
    mSlot.request(
            RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG, pointers.data(),
            pointers.size() * sizeof(RIL_GSM_BroadcastSmsConfigInfo*),
            statusResponse(serial,
                           &messaging::IRadioMessagingResponse::setGsmBroadcastConfigResponse));
    return ok();
}

ScopedAStatus RadioMessaging::setSmscAddress(int32_t serial, const std::string& smsc) {
    LOG_CALL;
    mSlot.requestString(
            RIL_REQUEST_SET_SMSC_ADDRESS, smsc,
            statusResponse(serial, &messaging::IRadioMessagingResponse::setSmscAddressResponse));
    return ok();
}

ScopedAStatus RadioMessaging::writeSmsToRuim(int32_t serial,
                                             const messaging::CdmaSmsWriteArgs& cdmaSms) {
    LOG_NOT_SUPPORTED;
    respond()->writeSmsToRuimResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioMessaging::writeSmsToSim(int32_t serial,
                                            const messaging::SmsWriteArgs& smsWriteArgs) {
    LOG_CALL << ' ' << smsWriteArgs.status;
    RIL_SMS_WriteArgs args = {
            .status = smsWriteArgs.status,
            .pdu = cString(smsWriteArgs.pdu),
            .smsc = cString(smsWriteArgs.smsc),
    };
    mSlot.request(RIL_REQUEST_WRITE_SMS_TO_SIM, &args, sizeof(args),
                  [this, serial](auto e, auto data, auto length) {
                      const auto index = responseInt(data, length);
                      respond()->writeSmsToSimResponse(responseInfo(serial, e, index.has_value()),
                                                       index.value_or(-1));
                  });
    return ok();
}

bool RadioMessaging::onUnsolicited(int id, const void* data, size_t length,
                                   RadioIndicationType type) {
    const auto indication = indicate();
    if (indication == nullptr) {
        return false;
    }
    switch (id) {
        case RIL_UNSOL_RESPONSE_NEW_SMS:
            indication->newSms(type, hexToBytes(static_cast<const char*>(data)));
            return true;
        case RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT:
            indication->newSmsStatusReport(type, hexToBytes(static_cast<const char*>(data)));
            return true;
        case RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM:
            if (const auto record = responseInt(data, length)) {
                indication->newSmsOnSim(type, *record);
            }
            return true;
        case RIL_UNSOL_SIM_SMS_STORAGE_FULL:
            indication->simSmsStorageFull(type);
            return true;
        case RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS: {
            const auto* bytes = static_cast<const uint8_t*>(data);
            indication->newBroadcastSms(type, bytes != nullptr
                                                      ? std::vector<uint8_t>(bytes, bytes + length)
                                                      : std::vector<uint8_t>{});
            return true;
        }
        default:
            LOG(DEBUG) << "Unhandled unsolicited " << id << " (" << length << " bytes)";
    }
    return false;
}

}  // namespace aidl::android::hardware::radio::samsung
