/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungSap"

#include "sap/RadioSap.h"

#include <hardware/ril/librilutils/proto/sap-api.pb.h>
#include <pb_decode.h>
#include <pb_encode.h>

#include <algorithm>
#include <cstdlib>
#include <memory>

#include "RadioSlot.h"

namespace aidl::android::hardware::radio::samsung {

using ::ndk::ScopedAStatus;
using sap::SapConnectRsp;
using sap::SapDisconnectType;
using sap::SapResultCode;
using sap::SapStatus;

namespace {

template <typename T>
std::vector<uint8_t> encode(const pb_field_t* fields, const T& message) {
    size_t size = 0;
    if (!pb_get_encoded_size(&size, fields, &message)) {
        return {};
    }
    std::vector<uint8_t> buffer(size);
    auto stream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    if (!pb_encode(&stream, fields, &message)) {
        LOG(ERROR) << "Failed to encode: " << PB_GET_ERROR(&stream);
        return {};
    }
    return buffer;
}

// Calls handle with the decoded message and returns whether it was decoded.
template <typename T, typename Handler>
bool decode(const pb_field_t* fields, const void* data, size_t length, Handler handle) {
    T message = {};
    auto stream = pb_istream_from_buffer(static_cast<const pb_byte_t*>(data), length);
    const bool decoded = data != nullptr && pb_decode(&stream, fields, &message);
    if (decoded) {
        handle(message);
    } else {
        LOG(ERROR) << "Failed to decode: " << PB_GET_ERROR(&stream);
    }
    pb_release(fields, &message);
    return decoded;
}

using Bytes = std::unique_ptr<pb_bytes_array_t, decltype(&free)>;

Bytes toBytes(const std::vector<uint8_t>& data) {
    Bytes bytes(static_cast<pb_bytes_array_t*>(malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(data.size()))),
                &free);
    bytes->size = data.size();
    std::copy(data.begin(), data.end(), bytes->bytes);
    return bytes;
}

std::vector<uint8_t> fromBytes(const pb_bytes_array_t* bytes) {
    return bytes != nullptr ? std::vector<uint8_t>(bytes->bytes, bytes->bytes + bytes->size)
                            : std::vector<uint8_t>();
}

SapResultCode toResultCode(RIL_SIM_SAP_APDU_RSP_Response response) {
    switch (response) {
        case RIL_SIM_SAP_APDU_RSP_Response_RIL_E_SUCCESS:
            return SapResultCode::SUCCESS;
        case RIL_SIM_SAP_APDU_RSP_Response_RIL_E_SIM_NOT_READY:
            return SapResultCode::CARD_NOT_ACCESSSIBLE;
        case RIL_SIM_SAP_APDU_RSP_Response_RIL_E_SIM_ALREADY_POWERED_OFF:
            return SapResultCode::CARD_ALREADY_POWERED_OFF;
        case RIL_SIM_SAP_APDU_RSP_Response_RIL_E_SIM_ABSENT:
            return SapResultCode::CARD_REMOVED;
        default:
            return SapResultCode::GENERIC_FAILURE;
    }
}

SapResultCode toResultCode(RIL_SIM_SAP_TRANSFER_ATR_RSP_Response response) {
    switch (response) {
        case RIL_SIM_SAP_TRANSFER_ATR_RSP_Response_RIL_E_SUCCESS:
            return SapResultCode::SUCCESS;
        case RIL_SIM_SAP_TRANSFER_ATR_RSP_Response_RIL_E_SIM_ALREADY_POWERED_OFF:
            return SapResultCode::CARD_ALREADY_POWERED_OFF;
        case RIL_SIM_SAP_TRANSFER_ATR_RSP_Response_RIL_E_SIM_ALREADY_POWERED_ON:
            return SapResultCode::CARD_ALREADY_POWERED_ON;
        case RIL_SIM_SAP_TRANSFER_ATR_RSP_Response_RIL_E_SIM_ABSENT:
            return SapResultCode::CARD_REMOVED;
        case RIL_SIM_SAP_TRANSFER_ATR_RSP_Response_RIL_E_SIM_DATA_NOT_AVAILABLE:
            return SapResultCode::DATA_NOT_AVAILABLE;
        default:
            return SapResultCode::GENERIC_FAILURE;
    }
}

SapResultCode toResultCode(RIL_SIM_SAP_POWER_RSP_Response response) {
    switch (response) {
        case RIL_SIM_SAP_POWER_RSP_Response_RIL_E_SUCCESS:
            return SapResultCode::SUCCESS;
        case RIL_SIM_SAP_POWER_RSP_Response_RIL_E_SIM_ABSENT:
            return SapResultCode::CARD_REMOVED;
        case RIL_SIM_SAP_POWER_RSP_Response_RIL_E_SIM_ALREADY_POWERED_OFF:
            return SapResultCode::CARD_ALREADY_POWERED_OFF;
        case RIL_SIM_SAP_POWER_RSP_Response_RIL_E_SIM_ALREADY_POWERED_ON:
            return SapResultCode::CARD_ALREADY_POWERED_ON;
        default:
            return SapResultCode::GENERIC_FAILURE;
    }
}

SapResultCode toResultCode(RIL_SIM_SAP_RESET_SIM_RSP_Response response) {
    switch (response) {
        case RIL_SIM_SAP_RESET_SIM_RSP_Response_RIL_E_SUCCESS:
            return SapResultCode::SUCCESS;
        case RIL_SIM_SAP_RESET_SIM_RSP_Response_RIL_E_SIM_ABSENT:
            return SapResultCode::CARD_REMOVED;
        case RIL_SIM_SAP_RESET_SIM_RSP_Response_RIL_E_SIM_NOT_READY:
            return SapResultCode::CARD_NOT_ACCESSSIBLE;
        case RIL_SIM_SAP_RESET_SIM_RSP_Response_RIL_E_SIM_ALREADY_POWERED_OFF:
            return SapResultCode::CARD_ALREADY_POWERED_OFF;
        default:
            return SapResultCode::GENERIC_FAILURE;
    }
}

SapResultCode toResultCode(RIL_SIM_SAP_TRANSFER_CARD_READER_STATUS_RSP_Response response) {
    switch (response) {
        case RIL_SIM_SAP_TRANSFER_CARD_READER_STATUS_RSP_Response_RIL_E_SUCCESS:
            return SapResultCode::SUCCESS;
        case RIL_SIM_SAP_TRANSFER_CARD_READER_STATUS_RSP_Response_RIL_E_SIM_DATA_NOT_AVAILABLE:
            return SapResultCode::DATA_NOT_AVAILABLE;
        default:
            return SapResultCode::GENERIC_FAILURE;
    }
}

}  // namespace

RadioSap::RadioSap(RadioSlot& slot) : mSlot(slot) {
    RadioSlot::VendorRil::get().setSapUnsolicitedCallback(
            mSlot.socket(),
            [this](int id, const void* data, size_t length) { onUnsolicited(id, data, length); });
}

ScopedAStatus RadioSap::setCallback(const std::shared_ptr<sap::ISapCallback>& callback) {
    std::lock_guard lock(mMutex);
    mCallback = callback;
    return ok();
}

std::shared_ptr<sap::ISapCallback> RadioSap::callback() {
    std::lock_guard lock(mMutex);
    return mCallback;
}

ScopedAStatus RadioSap::connectReq(int32_t serial, int32_t maxMsgSizeBytes) {
    LOG_CALL << ' ' << maxMsgSizeBytes;
    const RIL_SIM_SAP_CONNECT_REQ message = {.max_message_size = maxMsgSizeBytes};
    request(serial, MsgId_RIL_SIM_SAP_CONNECT, encode(RIL_SIM_SAP_CONNECT_REQ_fields, message));
    return ok();
}

ScopedAStatus RadioSap::disconnectReq(int32_t serial) {
    LOG_CALL;
    request(serial, MsgId_RIL_SIM_SAP_DISCONNECT, {});
    return ok();
}

ScopedAStatus RadioSap::apduReq(int32_t serial, sap::SapApduType type,
                                const std::vector<uint8_t>& command) {
    LOG_CALL << ' ' << toString(type) << ' ' << command.size();
    const auto bytes = toBytes(command);
    const RIL_SIM_SAP_APDU_REQ message = {
            .type = static_cast<RIL_SIM_SAP_APDU_REQ_Type>(type),
            .command = bytes.get(),
    };
    request(serial, MsgId_RIL_SIM_SAP_APDU, encode(RIL_SIM_SAP_APDU_REQ_fields, message));
    return ok();
}

ScopedAStatus RadioSap::transferAtrReq(int32_t serial) {
    LOG_CALL;
    request(serial, MsgId_RIL_SIM_SAP_TRANSFER_ATR, {});
    return ok();
}

ScopedAStatus RadioSap::powerReq(int32_t serial, bool powerOn) {
    LOG_CALL << ' ' << powerOn;
    const RIL_SIM_SAP_POWER_REQ message = {.state = powerOn};
    request(serial, MsgId_RIL_SIM_SAP_POWER, encode(RIL_SIM_SAP_POWER_REQ_fields, message));
    return ok();
}

ScopedAStatus RadioSap::resetSimReq(int32_t serial) {
    LOG_CALL;
    request(serial, MsgId_RIL_SIM_SAP_RESET_SIM, {});
    return ok();
}

ScopedAStatus RadioSap::transferCardReaderStatusReq(int32_t serial) {
    LOG_CALL;
    request(serial, MsgId_RIL_SIM_SAP_TRANSFER_CARD_READER_STATUS, {});
    return ok();
}

ScopedAStatus RadioSap::setTransferProtocolReq(int32_t serial,
                                               sap::SapTransferProtocol transferProtocol) {
    LOG_CALL << ' ' << toString(transferProtocol);
    const RIL_SIM_SAP_SET_TRANSFER_PROTOCOL_REQ message = {
            .protocol =
                    static_cast<RIL_SIM_SAP_SET_TRANSFER_PROTOCOL_REQ_Protocol>(transferProtocol),
    };
    request(serial, MsgId_RIL_SIM_SAP_SET_TRANSFER_PROTOCOL,
            encode(RIL_SIM_SAP_SET_TRANSFER_PROTOCOL_REQ_fields, message));
    return ok();
}

void RadioSap::request(int32_t serial, int id, const std::vector<uint8_t>& payload) {
    // Empty requests still pass a valid pointer, as libril_sem does.
    static const uint8_t kEmpty = 0;
    RadioSlot::VendorRil::get().sapRequest(
            mSlot.socket(), id, payload.empty() ? &kEmpty : payload.data(), payload.size(),
            [this, serial, id](RIL_Errno, const void* data, size_t length) {
                onResponse(serial, id, data, length);
            });
}

// Like libril_sem, the result comes from the payload alone; the RIL_Errno is not used.
void RadioSap::onResponse(int32_t serial, int id, const void* data, size_t length) {
    const auto cb = callback();
    if (cb == nullptr) {
        return;
    }
    bool decoded = true;
    switch (id) {
        case MsgId_RIL_SIM_SAP_CONNECT:
            decoded = decode<RIL_SIM_SAP_CONNECT_RSP>(
                    RIL_SIM_SAP_CONNECT_RSP_fields, data, length, [&](const auto& rsp) {
                        cb->connectResponse(serial, static_cast<SapConnectRsp>(rsp.response),
                                            rsp.has_max_message_size ? rsp.max_message_size : 0);
                        // SAP requires a card reset status after connecting, which the modem
                        // does not report. Bluetooth waits for it before using the connection.
                        if (rsp.response == RIL_SIM_SAP_CONNECT_RSP_Response_RIL_E_SUCCESS) {
                            cb->statusIndication(0, SapStatus::CARD_RESET);
                        }
                    });
            break;
        case MsgId_RIL_SIM_SAP_DISCONNECT:
            cb->disconnectResponse(serial);
            break;
        case MsgId_RIL_SIM_SAP_APDU:
            decoded = decode<RIL_SIM_SAP_APDU_RSP>(
                    RIL_SIM_SAP_APDU_RSP_fields, data, length, [&](const auto& rsp) {
                        cb->apduResponse(serial, toResultCode(rsp.response),
                                         fromBytes(rsp.apduResponse));
                    });
            break;
        case MsgId_RIL_SIM_SAP_TRANSFER_ATR:
            decoded = decode<RIL_SIM_SAP_TRANSFER_ATR_RSP>(
                    RIL_SIM_SAP_TRANSFER_ATR_RSP_fields, data, length, [&](const auto& rsp) {
                        cb->transferAtrResponse(serial, toResultCode(rsp.response),
                                                fromBytes(rsp.atr));
                    });
            break;
        case MsgId_RIL_SIM_SAP_POWER:
            decoded = decode<RIL_SIM_SAP_POWER_RSP>(
                    RIL_SIM_SAP_POWER_RSP_fields, data, length, [&](const auto& rsp) {
                        cb->powerResponse(serial, toResultCode(rsp.response));
                    });
            break;
        case MsgId_RIL_SIM_SAP_RESET_SIM:
            decoded = decode<RIL_SIM_SAP_RESET_SIM_RSP>(
                    RIL_SIM_SAP_RESET_SIM_RSP_fields, data, length, [&](const auto& rsp) {
                        cb->resetSimResponse(serial, toResultCode(rsp.response));
                    });
            break;
        case MsgId_RIL_SIM_SAP_TRANSFER_CARD_READER_STATUS:
            decoded = decode<RIL_SIM_SAP_TRANSFER_CARD_READER_STATUS_RSP>(
                    RIL_SIM_SAP_TRANSFER_CARD_READER_STATUS_RSP_fields, data, length,
                    [&](const auto& rsp) {
                        cb->transferCardReaderStatusResponse(
                                serial, toResultCode(rsp.response),
                                rsp.has_CardReaderStatus ? rsp.CardReaderStatus : 0);
                    });
            break;
        case MsgId_RIL_SIM_SAP_SET_TRANSFER_PROTOCOL:
            decoded = decode<RIL_SIM_SAP_SET_TRANSFER_PROTOCOL_RSP>(
                    RIL_SIM_SAP_SET_TRANSFER_PROTOCOL_RSP_fields, data, length,
                    [&](const auto& rsp) {
                        cb->transferProtocolResponse(
                                serial,
                                rsp.response == RIL_SIM_SAP_SET_TRANSFER_PROTOCOL_RSP_Response_RIL_E_SUCCESS
                                        ? SapResultCode::SUCCESS
                                        : SapResultCode::NOT_SUPPORTED);
                    });
            break;
    }
    if (!decoded) {
        onFailure(serial, id);
    }
}

void RadioSap::onFailure(int32_t serial, int id) {
    const auto cb = callback();
    if (cb == nullptr) {
        return;
    }
    switch (id) {
        case MsgId_RIL_SIM_SAP_CONNECT:
            cb->connectResponse(serial, SapConnectRsp::CONNECT_FAILURE, 0);
            break;
        case MsgId_RIL_SIM_SAP_APDU:
            cb->apduResponse(serial, SapResultCode::GENERIC_FAILURE, {});
            break;
        case MsgId_RIL_SIM_SAP_TRANSFER_ATR:
            cb->transferAtrResponse(serial, SapResultCode::GENERIC_FAILURE, {});
            break;
        case MsgId_RIL_SIM_SAP_POWER:
            cb->powerResponse(serial, SapResultCode::GENERIC_FAILURE);
            break;
        case MsgId_RIL_SIM_SAP_RESET_SIM:
            cb->resetSimResponse(serial, SapResultCode::GENERIC_FAILURE);
            break;
        case MsgId_RIL_SIM_SAP_TRANSFER_CARD_READER_STATUS:
            cb->transferCardReaderStatusResponse(serial, SapResultCode::GENERIC_FAILURE, 0);
            break;
        case MsgId_RIL_SIM_SAP_SET_TRANSFER_PROTOCOL:
            cb->transferProtocolResponse(serial, SapResultCode::NOT_SUPPORTED);
            break;
    }
}

void RadioSap::onUnsolicited(int id, const void* data, size_t length) {
    const auto cb = callback();
    if (cb == nullptr) {
        return;
    }
    switch (id) {
        case MsgId_RIL_SIM_SAP_DISCONNECT:
            decode<RIL_SIM_SAP_DISCONNECT_IND>(
                    RIL_SIM_SAP_DISCONNECT_IND_fields, data, length, [&](const auto& ind) {
                        cb->disconnectIndication(
                                0, static_cast<SapDisconnectType>(ind.disconnectType));
                    });
            break;
        case MsgId_RIL_SIM_SAP_STATUS:
            decode<RIL_SIM_SAP_STATUS_IND>(
                    RIL_SIM_SAP_STATUS_IND_fields, data, length, [&](const auto& ind) {
                        cb->statusIndication(0, static_cast<SapStatus>(ind.statusChange));
                    });
            break;
        default:
            LOG(DEBUG) << "Unhandled SAP unsolicited " << id;
    }
}

}  // namespace aidl::android::hardware::radio::samsung
