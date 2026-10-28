/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungData"

#include "data/DataConvert.h"

#include <android-base/strings.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "common.h"

namespace aidl::android::hardware::radio::samsung {

namespace {

using data::Qos;
using data::QosFilter;

// Larger values mean no slice differentiator.
constexpr int kMaxSliceDifferentiator = 0x7fffff;

std::vector<std::string> splitList(const char* list) {
    return ::android::base::Tokenize(toString(list), " ");
}

data::PdpProtocolType toPdpProtocolType(const char* type) {
    const std::string str = toString(type);
    if (str == "IP") {
        return data::PdpProtocolType::IP;
    }
    if (str == "IPV6") {
        return data::PdpProtocolType::IPV6;
    }
    if (str == "IPV4V6") {
        return data::PdpProtocolType::IPV4V6;
    }
    return data::PdpProtocolType::UNKNOWN;
}

int sliceDifferentiator(int sd) {
    return sd > kMaxSliceDifferentiator ? -1 : sd;
}

data::SliceInfo toSliceInfo(const SecSliceInfo& slice) {
    return {
            .sliceServiceType = static_cast<int8_t>(slice.sliceServiceType),
            .sliceDifferentiator = sliceDifferentiator(slice.sliceDifferentiator),
            .mappedHplmnSst = static_cast<int8_t>(slice.mappedHplmnSst),
            .mappedHplmnSd = sliceDifferentiator(slice.mappedHplmnSd),
            .status = static_cast<int8_t>(slice.status),
    };
}

Qos toQos(const SecQos& qos) {
    const data::QosBandwidth downlink = {qos.downlink.maxBitrateKbps,
                                         qos.downlink.guaranteedBitrateKbps};
    const data::QosBandwidth uplink = {qos.uplink.maxBitrateKbps, qos.uplink.guaranteedBitrateKbps};
    // Anything but NR, even no QoS, is reported as EPS.
    if (qos.type == SEC_QOS_TYPE_NR) {
        return Qos::make<Qos::nr>(data::NrQos{
                .fiveQi = qos.qci,
                .downlink = downlink,
                .uplink = uplink,
                .qfi = static_cast<int8_t>(qos.qfi),
                .averagingWindowMs = static_cast<char16_t>(qos.averagingWindowMs),
        });
    }
    return Qos::make<Qos::eps>(data::EpsQos{
            .qci = qos.qci,
            .downlink = downlink,
            .uplink = uplink,
    });
}

std::vector<std::string> toAddresses(const char (&addresses)[kSecMaxQosFilterAddresses][256],
                                     size_t length) {
    std::vector<std::string> list;
    for (size_t i = 0; i < std::min<size_t>(length, kSecMaxQosFilterAddresses); i++) {
        list.emplace_back(addresses[i], strnlen(addresses[i], sizeof(addresses[i])));
    }
    return list;
}

QosFilter toQosFilter(const SecQosFilter& filter) {
    QosFilter out = {
            .localAddresses = toAddresses(filter.localAddresses, filter.localAddressesLength),
            .remoteAddresses = toAddresses(filter.remoteAddresses, filter.remoteAddressesLength),
            .protocol = static_cast<int8_t>(filter.protocol),
            .direction = static_cast<int8_t>(filter.direction),
            .precedence = filter.precedence,
    };
    if (filter.hasLocalPort) {
        out.localPort = data::PortRange{filter.localPort.start, filter.localPort.end};
    }
    if (filter.hasRemotePort) {
        out.remotePort = data::PortRange{filter.remotePort.start, filter.remotePort.end};
    }
    if (filter.hasTos) {
        out.tos = data::QosFilterTypeOfService::make<data::QosFilterTypeOfService::value>(
                static_cast<int8_t>(filter.tos));
    }
    if (filter.hasFlowLabel) {
        out.flowLabel = data::QosFilterIpv6FlowLabel::make<data::QosFilterIpv6FlowLabel::value>(
                filter.flowLabel);
    }
    if (filter.hasSpi) {
        out.spi = data::QosFilterIpsecSpi::make<data::QosFilterIpsecSpi::value>(filter.spi);
    }
    return out;
}

data::QosSession toQosSession(const SecQosSession& session) {
    data::QosSession out = {
            .qosSessionId = session.qosSessionId,
            .qos = toQos(session.qos),
    };
    for (size_t i = 0; i < std::min<size_t>(session.qosFiltersLength, kSecMaxQosFilters); i++) {
        out.qosFilters.push_back(toQosFilter(session.qosFilters[i]));
    }
    return out;
}

data::TrafficDescriptor toTrafficDescriptor(const SecTrafficDescriptor& descriptor) {
    // 16 byte OS UUID, then the app id length and the app id.
    constexpr size_t kOsAppIdHeader = 0x11;
    data::TrafficDescriptor out;
    if (descriptor.hasDnn) {
        out.dnn = std::string(descriptor.dnn, strnlen(descriptor.dnn, sizeof(descriptor.dnn)));
    }
    if (descriptor.hasOsAppId) {
        const auto length =
                std::min(descriptor.osAppIdLength + kOsAppIdHeader, sizeof(descriptor.osAppId));
        out.osAppId = data::OsAppId{
                .osAppId = {descriptor.osAppId, descriptor.osAppId + length},
        };
    }
    return out;
}

std::vector<data::TrafficDescriptor> toTrafficDescriptors(
        const SecTrafficDescriptor (&descriptors)[kSecMaxTrafficDescriptors], size_t length) {
    std::vector<data::TrafficDescriptor> list;
    for (size_t i = 0; i < std::min<size_t>(length, kSecMaxTrafficDescriptors); i++) {
        list.push_back(toTrafficDescriptor(descriptors[i]));
    }
    return list;
}

data::RouteSelectionDescriptor toRouteSelectionDescriptor(
        const SecRouteSelectionDescriptor& descriptor) {
    data::RouteSelectionDescriptor out = {
            .precedence = static_cast<int8_t>(descriptor.precedence),
            .sessionType = static_cast<data::PdpProtocolType>(descriptor.sessionType),
            .sscMode = static_cast<int8_t>(descriptor.sscMode),
    };
    for (size_t i = 0; i < std::min<size_t>(descriptor.sliceInfoLength, kSecMaxSliceInfos); i++) {
        out.sliceInfo.push_back(toSliceInfo(descriptor.sliceInfo[i]));
    }
    for (size_t i = 0; i < std::min<size_t>(descriptor.dnnLength, kSecMaxDnns); i++) {
        out.dnn.emplace_back(descriptor.dnn[i], strnlen(descriptor.dnn[i], kSecDnnLength));
    }
    return out;
}

}  // namespace

data::SetupDataCallResult toSetupDataCallResult(const SecDataCallResponse& response) {
    data::SetupDataCallResult result = {
            .cause = static_cast<data::DataCallFailCause>(response.status),
            .suggestedRetryTime = response.suggestedRetryTime,
            .cid = response.cid,
            .active = response.active,
            .type = toPdpProtocolType(response.type),
            .ifname = toString(response.ifname),
            .dnses = splitList(response.dnses),
            .gateways = splitList(response.gateways),
            .pcscf = splitList(response.pcscf),
            .mtuV4 = response.mtuV4,
            .mtuV6 = response.mtuV6,
            .defaultQos = toQos(response.defaultQos),
            .handoverFailureMode = static_cast<int8_t>(response.handoverFailureMode),
            .pduSessionId = response.pduSessionId,
            .sliceInfo = toSliceInfo(response.sliceInfo),
            .trafficDescriptors = toTrafficDescriptors(response.trafficDescriptors,
                                                       response.trafficDescriptorsLength),
    };
    for (const auto& address : splitList(response.addresses)) {
        result.addresses.push_back({
                .address = address,
                .deprecationTime = -1,
                .expirationTime = -1,
        });
    }
    const auto sessions = std::min<size_t>(response.qosSessionsLength, kSecMaxQosSessions);
    for (size_t i = 0; i < sessions; i++) {
        result.qosSessions.push_back(toQosSession(response.qosSessions[i]));
    }
    return result;
}

std::optional<std::vector<data::SetupDataCallResult>> toDataCallList(const void* data,
                                                                     size_t length) {
    if (length % sizeof(SecDataCallResponse) != 0 || (data == nullptr && length != 0)) {
        LOG(ERROR) << "Invalid data call list length " << length;
        return std::nullopt;
    }
    const auto* calls = static_cast<const SecDataCallResponse*>(data);
    std::vector<data::SetupDataCallResult> list;
    for (size_t i = 0; i < length / sizeof(SecDataCallResponse); i++) {
        list.push_back(toSetupDataCallResult(calls[i]));
    }
    return list;
}

std::optional<data::SlicingConfig> toSlicingConfig(const void* data, size_t length) {
    if (data == nullptr || length < sizeof(SecSlicingConfig)) {
        LOG(ERROR) << "Invalid slicing config length " << length;
        return std::nullopt;
    }
    const auto* config = static_cast<const SecSlicingConfig*>(data);
    data::SlicingConfig out;
    for (size_t i = 0; i < std::min<size_t>(config->urspRulesLength, kSecMaxUrspRules); i++) {
        const auto& rule = config->urspRules[i];
        data::UrspRule urspRule = {
                .precedence = rule.precedence,
                .trafficDescriptors = toTrafficDescriptors(rule.trafficDescriptors,
                                                           rule.trafficDescriptorsLength),
        };
        const auto descriptors = std::min<size_t>(rule.routeSelectionDescriptorsLength,
                                                  kSecMaxRouteSelectionDescriptors);
        for (size_t j = 0; j < descriptors; j++) {
            urspRule.routeSelectionDescriptor.push_back(
                    toRouteSelectionDescriptor(rule.routeSelectionDescriptors[j]));
        }
        out.urspRules.push_back(std::move(urspRule));
    }
    const auto* slices = reinterpret_cast<const SecSliceInfo*>(config + 1);
    const size_t available = (length - sizeof(SecSlicingConfig)) / sizeof(SecSliceInfo);
    for (size_t i = 0; i < std::min(config->sliceInfoLength, available); i++) {
        out.sliceInfo.push_back(toSliceInfo(slices[i]));
    }
    return out;
}

}  // namespace aidl::android::hardware::radio::samsung
