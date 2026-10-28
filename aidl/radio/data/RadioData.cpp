/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungData"

#include "data/RadioData.h"

#include <android-base/strings.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "RadioSlot.h"
#include "data/DataConvert.h"

namespace aidl::android::hardware::radio::samsung {

using ::ndk::ScopedAStatus;

namespace {

// libsec-ril takes PdpProtocolType values, not "IP"/"IPV6".
std::string protocolString(data::PdpProtocolType protocol) {
    return std::to_string(static_cast<int>(protocol));
}

std::string boolString(bool value) {
    return value ? "1" : "0";
}

struct ProfileStrings {
    explicit ProfileStrings(const data::DataProfileInfo& profile)
        : apn(profile.apn),
          protocol(protocolString(profile.protocol)),
          roamingProtocol(protocolString(profile.roamingProtocol)),
          user(profile.user),
          password(profile.password) {}

    std::string apn;
    std::string protocol;
    std::string roamingProtocol;
    std::string user;
    std::string password;
};

SecDataProfile toSecDataProfile(const data::DataProfileInfo& profile,
                                const ProfileStrings& strings) {
    return {
            .profileId = profile.profileId,
            .apn = const_cast<char*>(strings.apn.c_str()),
            .protocol = const_cast<char*>(strings.protocol.c_str()),
            .roamingProtocol = const_cast<char*>(strings.roamingProtocol.c_str()),
            .authType = static_cast<int>(profile.authType),
            .user = cString(strings.user),
            .password = cString(strings.password),
            .type = profile.type,
            .maxConnsTime = profile.maxConnsTime,
            .maxConns = profile.maxConns,
            .waitTime = profile.waitTime,
            .enabled = profile.enabled,
            .supportedApnTypesBitmap = profile.supportedApnTypesBitmap,
            .bearerBitmap = profile.bearerBitmap,
            .mtuV4 = profile.mtuV4,
            .preferred = profile.preferred,
            .persistent = profile.persistent,
    };
}

data::KeepaliveStatus toKeepaliveStatus(const RIL_KeepaliveStatus& status) {
    return {
            .sessionHandle = static_cast<int32_t>(status.sessionHandle),
            .code = static_cast<int32_t>(status.code),
    };
}

}  // namespace

RadioData::RadioData(RadioSlot& slot) : mSlot(slot) {}

ScopedAStatus RadioData::setResponseFunctions(
        const std::shared_ptr<data::IRadioDataResponse>& response,
        const std::shared_ptr<data::IRadioDataIndication>& indication) {
    setCallbacks(response, indication);
    return ok();
}

ScopedAStatus RadioData::responseAcknowledgement() {
    return ok();
}

ScopedAStatus RadioData::allocatePduSessionId(int32_t serial) {
    LOG_CALL;
    mSlot.request(SEC_REQUEST_ALLOCATE_PDU_SESSION_ID,
                  [this, serial](auto e, auto data, auto length) {
                      const auto id = responseInt(data, length);
                      respond()->allocatePduSessionIdResponse(
                              responseInfo(serial, e, id.has_value()), id.value_or(0));
                  });
    return ok();
}

ScopedAStatus RadioData::cancelHandover(int32_t serial, int32_t callId) {
    LOG_CALL << ' ' << callId;
    mSlot.requestInts(SEC_REQUEST_CANCEL_HANDOVER, {callId},
                      statusResponse(serial, &data::IRadioDataResponse::cancelHandoverResponse));
    return ok();
}

ScopedAStatus RadioData::deactivateDataCall(int32_t serial, int32_t cid,
                                            data::DataRequestReason reason) {
    LOG_CALL << ' ' << cid << ' ' << toString(reason);
    mSlot.requestInts(
            RIL_REQUEST_DEACTIVATE_DATA_CALL, {cid, static_cast<int>(reason)},
            statusResponse(serial, &data::IRadioDataResponse::deactivateDataCallResponse));
    return ok();
}

ScopedAStatus RadioData::getDataCallList(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_DATA_CALL_LIST, [this, serial](auto e, auto data, auto length) {
        const auto calls = toDataCallList(data, length);
        respond()->getDataCallListResponse(
                responseInfo(serial, e, calls.has_value()),
                calls.value_or(std::vector<data::SetupDataCallResult>{}));
    });
    return ok();
}

ScopedAStatus RadioData::getSlicingConfig(int32_t serial) {
    LOG_CALL;
    mSlot.request(SEC_REQUEST_GET_SLICING_CONFIG, [this, serial](auto e, auto data, auto length) {
        const auto config = e == RIL_E_SUCCESS ? toSlicingConfig(data, length) : std::nullopt;
        respond()->getSlicingConfigResponse(responseInfo(serial, e, config.has_value()),
                                            config.value_or(data::SlicingConfig{}));
    });
    return ok();
}

ScopedAStatus RadioData::releasePduSessionId(int32_t serial, int32_t id) {
    LOG_CALL << ' ' << id;
    mSlot.requestInts(
            SEC_REQUEST_RELEASE_PDU_SESSION_ID, {id},
            statusResponse(serial, &data::IRadioDataResponse::releasePduSessionIdResponse));
    return ok();
}

ScopedAStatus RadioData::setDataAllowed(int32_t serial, bool allow) {
    LOG_CALL << ' ' << allow;
    mSlot.requestInts(RIL_REQUEST_ALLOW_DATA, {allow},
                      statusResponse(serial, &data::IRadioDataResponse::setDataAllowedResponse));
    return ok();
}

ScopedAStatus RadioData::setDataProfile(int32_t serial,
                                        const std::vector<data::DataProfileInfo>& profiles) {
    LOG_CALL << ' ' << profiles.size();
    std::vector<ProfileStrings> strings;
    strings.reserve(profiles.size());
    std::vector<SecDataProfile> secProfiles;
    for (const auto& profile : profiles) {
        strings.emplace_back(profile);
        secProfiles.push_back(toSecDataProfile(profile, strings.back()));
    }
    std::vector<SecDataProfile*> pointers;
    for (auto& profile : secProfiles) {
        pointers.push_back(&profile);
    }
    mSlot.request(RIL_REQUEST_SET_DATA_PROFILE, pointers.data(),
                  pointers.size() * sizeof(SecDataProfile*),
                  statusResponse(serial, &data::IRadioDataResponse::setDataProfileResponse));
    return ok();
}

ScopedAStatus RadioData::setDataThrottling(int32_t serial,
                                           data::DataThrottlingAction dataThrottlingAction,
                                           int64_t completionDurationMillis) {
    LOG_CALL << ' ' << toString(dataThrottlingAction) << ' ' << completionDurationMillis;
    mSlot.requestStrings(
            SEC_REQUEST_SET_DATA_THROTTLING,
            {std::to_string(static_cast<int>(dataThrottlingAction)),
             std::to_string(completionDurationMillis)},
            statusResponse(serial, &data::IRadioDataResponse::setDataThrottlingResponse), false);
    return ok();
}

ScopedAStatus RadioData::setInitialAttachApn(
        int32_t serial, const std::optional<data::DataProfileInfo>& dataProfileInfo) {
    LOG_CALL << ' ' << (dataProfileInfo.has_value() ? dataProfileInfo->apn : "(none)");
    const auto profile = dataProfileInfo.value_or(data::DataProfileInfo{});
    const ProfileStrings strings(profile);
    SecInitialAttachApn apn = {
            .apn = const_cast<char*>(strings.apn.c_str()),
            .protocol = const_cast<char*>(strings.protocol.c_str()),
            .roamingProtocol = const_cast<char*>(strings.roamingProtocol.c_str()),
            .authType = static_cast<int>(profile.authType),
            .user = cString(strings.user),
            .password = cString(strings.password),
            .supportedApnTypesBitmap = profile.supportedApnTypesBitmap,
            .bearerBitmap = profile.bearerBitmap,
            .mtu = std::min(profile.mtuV4, profile.mtuV6),
            .enabled = profile.enabled,
            .maxConnsTime = profile.maxConnsTime,
            .maxConns = profile.maxConns,
            .waitTime = profile.waitTime,
            .preferred = profile.preferred,
            .persistent = profile.persistent,
            .mtuV4 = profile.mtuV4,
            .mtuV6 = profile.mtuV6,
    };
    mSlot.request(RIL_REQUEST_SET_INITIAL_ATTACH_APN, &apn, sizeof(apn),
                  statusResponse(serial, &data::IRadioDataResponse::setInitialAttachApnResponse));
    return ok();
}

ScopedAStatus RadioData::setupDataCall(int32_t serial, AccessNetwork accessNetwork,
                                       const data::DataProfileInfo& dataProfileInfo,
                                       bool roamingAllowed, data::DataRequestReason reason,
                                       const std::vector<data::LinkAddress>& addresses,
                                       const std::vector<std::string>& dnses, int32_t pduSessionId,
                                       const std::optional<data::SliceInfo>& sliceInfo,
                                       bool matchAllRuleAllowed) {
    LOG_CALL << ' ' << toString(accessNetwork) << ' ' << dataProfileInfo.apn << ' '
             << toString(reason);
    std::vector<std::string> linkAddresses;
    for (const auto& address : addresses) {
        linkAddresses.push_back(address.address);
    }
    const auto& trafficDescriptor = dataProfileInfo.trafficDescriptor;
    // Raw OS app id bytes, followed by their length.
    std::string osAppId;
    if (trafficDescriptor.osAppId.has_value()) {
        const auto& bytes = trafficDescriptor.osAppId->osAppId;
        osAppId.assign(bytes.begin(), bytes.end());
    }
    const auto sliceValue = [&](auto field, int fallback) {
        return std::to_string(sliceInfo.has_value() ? static_cast<int>((*sliceInfo).*field)
                                                    : fallback);
    };
    const std::vector<std::string> strings = {
            "0",
            std::to_string(dataProfileInfo.profileId),
            dataProfileInfo.apn,
            dataProfileInfo.user,
            dataProfileInfo.password,
            std::to_string(static_cast<int>(dataProfileInfo.authType)),
            protocolString(dataProfileInfo.protocol),
            protocolString(dataProfileInfo.roamingProtocol),
            std::to_string(dataProfileInfo.supportedApnTypesBitmap),
            std::to_string(dataProfileInfo.bearerBitmap),
            "0",
            std::to_string(dataProfileInfo.mtuV4),
            "",
            "",
            boolString(roamingAllowed),
            std::to_string(static_cast<int>(accessNetwork)),
            std::to_string(static_cast<int>(reason)),
            ::android::base::Join(linkAddresses, ' '),
            ::android::base::Join(dnses, ' '),
            std::to_string(dataProfileInfo.mtuV6),
            std::to_string(pduSessionId),
            sliceValue(&data::SliceInfo::sliceServiceType, 0),
            sliceValue(&data::SliceInfo::mappedHplmnSst, 0),
            sliceValue(&data::SliceInfo::status, 0),
            sliceValue(&data::SliceInfo::sliceDifferentiator, -1),
            sliceValue(&data::SliceInfo::mappedHplmnSd, -1),
            trafficDescriptor.dnn.value_or(""),
            osAppId,
            std::to_string(osAppId.size()),
            boolString(matchAllRuleAllowed),
    };
    mSlot.requestStrings(
            RIL_REQUEST_SETUP_DATA_CALL, strings, [this, serial](auto e, auto data, auto length) {
                const auto* call = responseAs<SecDataCallResponse>(data, length);
                data::SetupDataCallResult result;
                if (call != nullptr) {
                    result = toSetupDataCallResult(*call);
                } else {
                    result.cause = data::DataCallFailCause::ERROR_UNSPECIFIED;
                    result.suggestedRetryTime = -1;
                }
                respond()->setupDataCallResponse(responseInfo(serial, e, call != nullptr), result);
            });
    return ok();
}

ScopedAStatus RadioData::startHandover(int32_t serial, int32_t callId) {
    LOG_CALL << ' ' << callId;
    mSlot.requestInts(SEC_REQUEST_START_HANDOVER, {callId},
                      statusResponse(serial, &data::IRadioDataResponse::startHandoverResponse));
    return ok();
}

ScopedAStatus RadioData::startKeepalive(int32_t serial, const data::KeepaliveRequest& keepalive) {
    LOG_CALL << ' ' << keepalive.type << ' ' << keepalive.cid;
    constexpr size_t kIpv4Length = 4;
    constexpr size_t kIpv6Length = 16;
    size_t addressLength = 0;
    if (keepalive.type == data::KeepaliveRequest::TYPE_NATT_IPV4) {
        addressLength = kIpv4Length;
    } else if (keepalive.type == data::KeepaliveRequest::TYPE_NATT_IPV6) {
        addressLength = kIpv6Length;
    }
    if (addressLength == 0 || keepalive.sourceAddress.size() != addressLength ||
        keepalive.destinationAddress.size() != addressLength) {
        respond()->startKeepaliveResponse(responseInfo(serial, RadioError::INVALID_ARGUMENTS), {});
        return ok();
    }
    RIL_KeepaliveRequest request = {
            .type = static_cast<RIL_KeepaliveType>(keepalive.type),
            .sourcePort = keepalive.sourcePort,
            .destinationPort = keepalive.destinationPort,
            .maxKeepaliveIntervalMillis = keepalive.maxKeepaliveIntervalMillis,
            .cid = keepalive.cid,
    };
    std::copy(keepalive.sourceAddress.begin(), keepalive.sourceAddress.end(),
              request.sourceAddress);
    std::copy(keepalive.destinationAddress.begin(), keepalive.destinationAddress.end(),
              request.destinationAddress);
    mSlot.request(RIL_REQUEST_START_KEEPALIVE, &request, sizeof(request),
                  [this, serial](auto e, auto data, auto length) {
                      const auto* status = responseAs<RIL_KeepaliveStatus>(data, length);
                      respond()->startKeepaliveResponse(responseInfo(serial, e, status != nullptr),
                                                        status != nullptr
                                                                ? toKeepaliveStatus(*status)
                                                                : data::KeepaliveStatus{});
                  });
    return ok();
}

ScopedAStatus RadioData::stopKeepalive(int32_t serial, int32_t sessionHandle) {
    LOG_CALL << ' ' << sessionHandle;
    mSlot.requestInts(RIL_REQUEST_STOP_KEEPALIVE, {sessionHandle},
                      statusResponse(serial, &data::IRadioDataResponse::stopKeepaliveResponse));
    return ok();
}

ScopedAStatus RadioData::setUserDataEnabled(int32_t serial, bool enabled) {
    LOG_NOT_SUPPORTED;
    respond()->setUserDataEnabledResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioData::setUserDataRoamingEnabled(int32_t serial, bool enabled) {
    LOG_NOT_SUPPORTED;
    respond()->setUserDataRoamingEnabledResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioData::notifyImsDataNetwork(int32_t serial,
                                              const data::ImsDataNetworkInfo& info) {
    LOG_NOT_SUPPORTED;
    respond()->notifyImsDataNetworkResponse(notSupported(serial));
    return ok();
}

bool RadioData::onUnsolicited(int id, const void* data, size_t length, RadioIndicationType type) {
    const auto indication = indicate();
    if (indication == nullptr) {
        return false;
    }
    switch (id) {
        case RIL_UNSOL_DATA_CALL_LIST_CHANGED:
            if (const auto calls = toDataCallList(data, length)) {
                indication->dataCallListChanged(type, *calls);
            }
            return true;
        case RIL_UNSOL_PCO_DATA:
            if (const auto* pco = responseAs<RIL_PCO_Data>(data, length)) {
                data::PcoDataInfo info = {
                        .cid = pco->cid,
                        .bearerProto = toString(pco->bearer_proto),
                        .pcoId = pco->pco_id,
                };
                if (pco->contents != nullptr && pco->contents_length > 0) {
                    const auto* contents = reinterpret_cast<const uint8_t*>(pco->contents);
                    info.contents.assign(contents, contents + pco->contents_length);
                }
                indication->pcoData(type, info);
            }
            return true;
        case RIL_UNSOL_KEEPALIVE_STATUS:
            if (const auto* status = responseAs<RIL_KeepaliveStatus>(data, length)) {
                indication->keepaliveStatus(type, toKeepaliveStatus(*status));
            }
            return true;
        case SEC_UNSOL_UNTHROTTLE_APN:
            indication->unthrottleApn(type, {});
            return true;
        default:
            LOG(DEBUG) << "Unhandled unsolicited " << id << " (" << length << " bytes)";
    }
    return false;
}

}  // namespace aidl::android::hardware::radio::samsung
