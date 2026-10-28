/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <samsung_ril/structs.h>

// QoS filters are inline, so a data call response is about 1.3 MiB.

constexpr int kSecMaxQosSessions = 16;
constexpr int kSecMaxQosFilters = 16;
constexpr int kSecMaxQosFilterAddresses = 10;
constexpr int kSecMaxTrafficDescriptors = 10;
constexpr int kSecMaxUrspRules = 10;
constexpr int kSecMaxRouteSelectionDescriptors = 10;
constexpr int kSecMaxSliceInfos = 10;
constexpr int kSecMaxDnns = 10;
constexpr size_t kSecDnnLength = 101;

enum SecQosType {
    SEC_QOS_TYPE_EPS = 1,
    SEC_QOS_TYPE_NR = 2,
};

struct SecQosBandwidth {
    int maxBitrateKbps;
    int guaranteedBitrateKbps;
};

struct SecQos {
    int type;
    uint16_t qci;
    SecQosBandwidth downlink;
    SecQosBandwidth uplink;
    uint8_t qfi;
    uint8_t reserved;
    int8_t averagingWindowMs;
};

static_assert(offsetof(SecQos, downlink) == 0x8);
static_assert(offsetof(SecQos, qfi) == 0x18);
static_assert(offsetof(SecQos, averagingWindowMs) == 0x1a);
static_assert(sizeof(SecQos) == 0x1c);

struct SecPortRange {
    int start;
    int end;
};

struct SecQosFilter {
    size_t localAddressesLength;
    char localAddresses[kSecMaxQosFilterAddresses][256];
    size_t remoteAddressesLength;
    char remoteAddresses[kSecMaxQosFilterAddresses][256];
    uint8_t hasLocalPort;
    SecPortRange localPort;
    uint8_t hasRemotePort;
    SecPortRange remotePort;
    int protocol;
    uint8_t hasTos;
    uint8_t tos;
    uint8_t hasFlowLabel;
    int flowLabel;
    uint8_t hasSpi;
    int spi;
    int direction;
    int precedence;
};

static_assert(offsetof(SecQosFilter, remoteAddressesLength) == 0xa08);
static_assert(offsetof(SecQosFilter, hasLocalPort) == 0x1410);
static_assert(offsetof(SecQosFilter, localPort) == 0x1414);
static_assert(offsetof(SecQosFilter, remotePort) == 0x1420);
static_assert(offsetof(SecQosFilter, hasTos) == 0x142c);
static_assert(offsetof(SecQosFilter, flowLabel) == 0x1430);
static_assert(offsetof(SecQosFilter, spi) == 0x1438);
static_assert(offsetof(SecQosFilter, precedence) == 0x1440);
static_assert(sizeof(SecQosFilter) == 0x1448);

struct SecQosSession {
    int qosSessionId;
    SecQos qos;
    size_t qosFiltersLength;
    SecQosFilter qosFilters[kSecMaxQosFilters];
};

static_assert(offsetof(SecQosSession, qosFiltersLength) == 0x20);
static_assert(sizeof(SecQosSession) == 0x144a8);

struct SecSliceInfo {
    uint8_t sliceServiceType;
    int sliceDifferentiator;
    uint8_t mappedHplmnSst;
    int mappedHplmnSd;
    uint8_t status;
};

static_assert(sizeof(SecSliceInfo) == 0x14);

struct SecTrafficDescriptor {
    uint8_t hasDnn;
    char dnn[kSecDnnLength];
    uint8_t hasOsAppId;
    size_t osAppIdLength;
    uint8_t osAppId[0x118];
};

static_assert(offsetof(SecTrafficDescriptor, hasOsAppId) == 0x66);
static_assert(offsetof(SecTrafficDescriptor, osAppId) == 0x70);
static_assert(sizeof(SecTrafficDescriptor) == 0x188);

struct SecDataCallResponse {
    int status;
    int suggestedRetryTime;
    int cid;
    int active;
    char* type;
    char* ifname;
    char* addresses;
    char* dnses;
    char* gateways;
    char* pcscf;
    int mtu;
    int pduSessionId;
    int handoverFailureMode;
    int mtuV4;
    int mtuV6;
    SecQos defaultQos;
    size_t qosSessionsLength;
    SecQosSession qosSessions[kSecMaxQosSessions];
    SecSliceInfo sliceInfo;
    size_t trafficDescriptorsLength;
    SecTrafficDescriptor trafficDescriptors[kSecMaxTrafficDescriptors];
};

static_assert(offsetof(SecDataCallResponse, pcscf) == 0x38);
static_assert(offsetof(SecDataCallResponse, pduSessionId) == 0x44);
static_assert(offsetof(SecDataCallResponse, mtuV4) == 0x4c);
static_assert(offsetof(SecDataCallResponse, defaultQos) == 0x54);
static_assert(offsetof(SecDataCallResponse, qosSessionsLength) == 0x70);
static_assert(offsetof(SecDataCallResponse, qosSessions) == 0x78);
static_assert(offsetof(SecDataCallResponse, sliceInfo) == 0x144af8);
static_assert(offsetof(SecDataCallResponse, trafficDescriptorsLength) == 0x144b10);
static_assert(offsetof(SecDataCallResponse, trafficDescriptors) == 0x144b18);
static_assert(sizeof(SecDataCallResponse) == 0x145a68);

struct SecRouteSelectionDescriptor {
    uint8_t precedence;
    int sessionType;
    uint8_t sscMode;
    size_t sliceInfoLength;
    SecSliceInfo sliceInfo[kSecMaxSliceInfos];
    size_t dnnLength;
    char dnn[kSecMaxDnns][kSecDnnLength];
};

static_assert(offsetof(SecRouteSelectionDescriptor, sliceInfo) == 0x18);
static_assert(offsetof(SecRouteSelectionDescriptor, dnn) == 0xe8);
static_assert(sizeof(SecRouteSelectionDescriptor) == 0x4e0);

struct SecUrspRule {
    uint8_t precedence;
    size_t trafficDescriptorsLength;
    SecTrafficDescriptor trafficDescriptors[kSecMaxTrafficDescriptors];
    size_t routeSelectionDescriptorsLength;
    SecRouteSelectionDescriptor routeSelectionDescriptors[kSecMaxRouteSelectionDescriptors];
};

static_assert(offsetof(SecUrspRule, routeSelectionDescriptorsLength) == 0xf60);
static_assert(sizeof(SecUrspRule) == 0x4028);

// Followed by sliceInfoLength SecSliceInfo entries.
struct SecSlicingConfig {
    size_t urspRulesLength;
    SecUrspRule urspRules[kSecMaxUrspRules];
    size_t sliceInfoLength;
};

static_assert(sizeof(SecSlicingConfig) == 0x281a0);

struct SecInitialAttachApn {
    char* apn;
    char* protocol;
    char* roamingProtocol;
    int authType;
    char* user;
    char* password;
    int supportedApnTypesBitmap;
    int bearerBitmap;
    int modemCognitive;
    int mtu;
    char* mvnoType;
    char* mvnoMatchData;
    int enabled;
    int reserved0;
    int maxConnsTime;
    int maxConns;
    int waitTime;
    uint8_t preferred;
    int persistent;
    int mtuV4;
    int mtuV6;
    int reserved1;
};

static_assert(offsetof(SecInitialAttachApn, supportedApnTypesBitmap) == 0x30);
static_assert(offsetof(SecInitialAttachApn, enabled) == 0x50);
static_assert(offsetof(SecInitialAttachApn, preferred) == 0x64);
static_assert(offsetof(SecInitialAttachApn, mtuV6) == 0x70);
static_assert(sizeof(SecInitialAttachApn) == 0x78);

struct SecDataProfile {
    int profileId;
    char* apn;
    char* protocol;
    char* roamingProtocol;
    int authType;
    char* user;
    char* password;
    int type;
    int maxConnsTime;
    int maxConns;
    int waitTime;
    int enabled;
    int reserved0;
    int supportedApnTypesBitmap;
    int bearerBitmap;
    int mtuV4;
    char reserved1[0x14];
    int preferred;
    int persistent;
    char reserved2[8];
};

static_assert(offsetof(SecDataProfile, type) == 0x38);
static_assert(offsetof(SecDataProfile, supportedApnTypesBitmap) == 0x50);
static_assert(offsetof(SecDataProfile, mtuV4) == 0x58);
static_assert(offsetof(SecDataProfile, preferred) == 0x70);
static_assert(sizeof(SecDataProfile) == 0x80);
