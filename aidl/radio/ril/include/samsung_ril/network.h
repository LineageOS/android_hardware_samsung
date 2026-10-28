/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <samsung_ril/structs.h>

// Samsung inlines strings and grows most structs, so none of these match ril.h.

constexpr size_t kSecOperatorNameLength = 0x200;

enum SecCellInfoType {
    SEC_CELL_INFO_TYPE_GSM = 1,
    SEC_CELL_INFO_TYPE_CDMA = 2,
    SEC_CELL_INFO_TYPE_LTE = 3,
    SEC_CELL_INFO_TYPE_WCDMA = 4,
    SEC_CELL_INFO_TYPE_TD_SCDMA = 5,
    SEC_CELL_INFO_TYPE_NR = 6,
    SEC_CELL_INFO_TYPE_LTE_ALT = 7,
};

struct SecCellIdentityGsm {
    char mcc[4];
    char mnc[4];
    int lac;
    int cid;
    int arfcn;
    uint8_t bsic;
    char alphaShort[kSecOperatorNameLength];
    char alphaLong[kSecOperatorNameLength];
};

struct SecCellIdentityWcdma {
    char mcc[4];
    char mnc[4];
    int lac;
    int cid;
    int psc;
    int uarfcn;
    char alphaShort[kSecOperatorNameLength];
    char alphaLong[kSecOperatorNameLength];
};

struct SecCellIdentityTdscdma {
    char mcc[4];
    char mnc[4];
    int lac;
    int cid;
    int cpid;
    char alphaShort[kSecOperatorNameLength];
    char alphaLong[kSecOperatorNameLength];
};

struct SecCellIdentityCdma {
    int networkId;
    int systemId;
    int basestationId;
    int longitude;
    int latitude;
    char alphaShort[kSecOperatorNameLength];
    char alphaLong[kSecOperatorNameLength];
};

struct SecCellIdentityLte {
    char mcc[4];
    char mnc[4];
    int ci;
    int pci;
    int tac;
    int earfcn;
    char alphaShort[kSecOperatorNameLength];
    char alphaLong[kSecOperatorNameLength];
    int bandwidth;
};

struct SecCellIdentityNr {
    char mcc[4];
    char mnc[4];
    int64_t nci;
    int pci;
    int tac;
    int nrarfcn;
    int band;
    char alphaShort[kSecOperatorNameLength];
    char alphaLong[kSecOperatorNameLength];
};

static_assert(offsetof(SecCellIdentityGsm, alphaShort) == 0x15);
static_assert(offsetof(SecCellIdentityGsm, alphaLong) == 0x215);
static_assert(offsetof(SecCellIdentityWcdma, alphaLong) == 0x218);
static_assert(offsetof(SecCellIdentityTdscdma, alphaLong) == 0x214);
static_assert(offsetof(SecCellIdentityCdma, alphaLong) == 0x214);
static_assert(offsetof(SecCellIdentityLte, bandwidth) == 0x418);
static_assert(offsetof(SecCellIdentityNr, alphaLong) == 0x220);
static_assert(sizeof(SecCellIdentityNr) == 0x420);

struct SecCellIdentity {
    int cellInfoType;
    union {
        SecCellIdentityGsm gsm;
        SecCellIdentityWcdma wcdma;
        SecCellIdentityTdscdma tdscdma;
        SecCellIdentityCdma cdma;
        SecCellIdentityLte lte;
        SecCellIdentityNr nr;
    };
};

static_assert(offsetof(SecCellIdentity, gsm) == 0x8);
static_assert(sizeof(SecCellIdentity) == 0x428);

struct SecVoiceRegState {
    int regState;
    int rat;
    int cssSupported;
    int roamingIndicator;
    int systemIsInPrl;
    int defaultRoamingIndicator;
    int reasonForDenial;
    SecCellIdentity cellIdentity;
    char registeredPlmn[7];
    uint8_t geranDtmSupported;
    int extended;
    int reserved;
    uint8_t extendedFlag;
};

static_assert(offsetof(SecVoiceRegState, cellIdentity) == 0x20);
static_assert(offsetof(SecVoiceRegState, registeredPlmn) == 0x448);
static_assert(offsetof(SecVoiceRegState, geranDtmSupported) == 0x44f);
static_assert(offsetof(SecVoiceRegState, extendedFlag) == 0x458);
static_assert(sizeof(SecVoiceRegState) == 0x460);

struct SecDataRegState {
    int regState;
    int rat;
    int reasonDataDenied;
    int maxDataCalls;
    SecCellIdentity cellIdentity;
    uint8_t isVopsSupported;
    uint8_t isEmcBearerSupported;
    uint8_t isEndcAvailable;
    uint8_t isDcNrRestricted;
    uint8_t isNrAvailable;
    int nrVopsSupported;
    int nrEmcSupported;
    int nrEmfSupported;
    char registeredPlmn[12];
    int extended[2];
    int dataRat;
    int extendedTail[2];
};

static_assert(offsetof(SecDataRegState, cellIdentity) == 0x10);
static_assert(offsetof(SecDataRegState, isVopsSupported) == 0x438);
static_assert(offsetof(SecDataRegState, nrVopsSupported) == 0x440);
static_assert(offsetof(SecDataRegState, registeredPlmn) == 0x44c);
static_assert(offsetof(SecDataRegState, dataRat) == 0x460);
static_assert(sizeof(SecDataRegState) == 0x470);

struct SecGsmSignalStrength {
    int signalStrength;
    int bitErrorRate;
    int timingAdvance;
};

struct SecCdmaSignalStrength {
    int dbm;
    int ecio;
};

struct SecEvdoSignalStrength {
    int dbm;
    int ecio;
    int signalNoiseRatio;
};

struct SecLteSignalStrength {
    int signalStrength;
    int rsrp;
    int rsrq;
    int rssnr;
    int cqi;
    int timingAdvance;
};

struct SecWcdmaSignalStrength {
    int signalStrength;
    int bitErrorRate;
    int rscp;
    int ecno;
};

// Followed by csiCqiReportSize report bytes, as many as fit in the enclosing struct.
struct SecNrSignalStrength {
    int ssRsrp;
    int ssRsrq;
    int ssSinr;
    int csiRsrp;
    int csiRsrq;
    int csiSinr;
    int csiCqiTableIndex;
    int csiCqiReportSize;
};

struct SecSignalStrength {
    SecGsmSignalStrength gsm;
    SecCdmaSignalStrength cdma;
    SecEvdoSignalStrength evdo;
    SecLteSignalStrength lte;
    int lteCqiTableIndex;
    int tdscdmaRscp;
    SecWcdmaSignalStrength wcdma;
    SecNrSignalStrength nr;
    uint8_t nrCsiCqiReport[0x30];
};

static_assert(offsetof(SecSignalStrength, lte) == 0x20);
static_assert(offsetof(SecSignalStrength, tdscdmaRscp) == 0x3c);
static_assert(offsetof(SecSignalStrength, wcdma) == 0x40);
static_assert(offsetof(SecSignalStrength, nr) == 0x50);
static_assert(sizeof(SecSignalStrength) == 0xa0);

struct SecCellInfo {
    int cellInfoType;
    int registered;
    int connectionStatus;
    union {
        struct {
            SecCellIdentityGsm identity;
            SecGsmSignalStrength signal;
        } gsm;
        struct {
            SecCellIdentityWcdma identity;
            SecWcdmaSignalStrength signal;
        } wcdma;
        struct {
            SecCellIdentityTdscdma identity;
            int rscp;
        } tdscdma;
        struct {
            SecCellIdentityCdma identity;
            SecCdmaSignalStrength cdma;
            SecEvdoSignalStrength evdo;
        } cdma;
        struct {
            SecCellIdentityLte identity;
            SecLteSignalStrength signal;
        } lte;
        struct {
            SecCellIdentityNr identity;
            SecNrSignalStrength signal;
            uint8_t csiCqiReport[0x18];
        } nr;
    };
};

static_assert(offsetof(SecCellInfo, gsm.identity) == 0x10);
static_assert(offsetof(SecCellInfo, gsm.signal) == 0x428);
static_assert(offsetof(SecCellInfo, wcdma.signal) == 0x428);
static_assert(offsetof(SecCellInfo, tdscdma.rscp) == 0x424);
static_assert(offsetof(SecCellInfo, cdma.evdo) == 0x42c);
static_assert(offsetof(SecCellInfo, lte.signal) == 0x42c);
static_assert(offsetof(SecCellInfo, nr.signal) == 0x430);
static_assert(sizeof(SecCellInfo) == 0x468);

struct SecBarringData {
    int serviceType;
    int barringType;
    int factor;
    int timeSeconds;
    int isBarred;
};

struct SecBarringInfo {
    SecCellIdentity cellIdentity;
    int numBarringInfos;
    SecBarringData barringInfos[50];
};

static_assert(offsetof(SecBarringInfo, barringInfos) == 0x42c);
static_assert(sizeof(SecBarringInfo) == 0x818);

struct SecPhysicalChannelConfig {
    int status;
    int rat;
    int downlinkChannelNumber;
    int uplinkChannelNumber;
    int cellBandwidthDownlinkKhz;
    int cellBandwidthUplinkKhz;
    int numContextIds;
    int* contextIds;
    int physicalCellId;
    int reserved;
    int band;
};

static_assert(offsetof(SecPhysicalChannelConfig, contextIds) == 0x20);
static_assert(offsetof(SecPhysicalChannelConfig, band) == 0x30);
static_assert(sizeof(SecPhysicalChannelConfig) == 0x38);

struct SecLinkCapacityEstimate {
    int downlinkCapacityKbps;
    int reserved0[2];
    int uplinkCapacityKbps;
    int reserved1[2];
};

static_assert(sizeof(SecLinkCapacityEstimate) == 0x18);

struct SecNetworkScanResult {
    int status;
    int error;
    unsigned int numCells;
    SecCellInfo* cells;
};

static_assert(sizeof(SecNetworkScanResult) == 0x18);

struct SecRegistrationFailed {
    int domain;
    char chosenPlmn[8];
    int causeCode;
    int additionalCauseCode;
    SecCellIdentity cellIdentity;
};

static_assert(offsetof(SecRegistrationFailed, cellIdentity) == 0x18);
static_assert(sizeof(SecRegistrationFailed) == 0x440);

struct SecEmergencyRegResult {
    int accessNetwork;
    int regState;
    int emcDomain;
    uint8_t isVopsSupported;
    uint8_t isEmcBearerSupported;
    int8_t nwProvidedEmc;
    int8_t nwProvidedEmf;
    char mcc[4];
    char mnc[4];
};

static_assert(sizeof(SecEmergencyRegResult) == 0x18);

constexpr int kSecMaxBands = 8;
constexpr int kSecMaxChannels = 32;
constexpr int kSecMaxRadioAccessNetworks = 8;
constexpr int kSecMaxScanPlmns = 20;

struct SecRadioAccessSpecifier {
    int radioAccessNetwork;
    unsigned int bandsLength;
    unsigned int bands[kSecMaxBands];
    unsigned int channelsLength;
    unsigned int channels[kSecMaxChannels];
};

static_assert(sizeof(SecRadioAccessSpecifier) == 0xac);

struct SecNetworkScanRequest {
    int type;
    int interval;
    unsigned int specifiersLength;
    SecRadioAccessSpecifier specifiers[kSecMaxRadioAccessNetworks];
    int maxSearchTime;
    int incrementalResults;
    int incrementalResultsPeriodicity;
    unsigned int mccMncsLength;
    char mccMncs[kSecMaxScanPlmns][7];
};

static_assert(offsetof(SecNetworkScanRequest, maxSearchTime) == 0x56c);
static_assert(sizeof(SecNetworkScanRequest) == 0x608);

struct SecSystemSelectionChannels {
    unsigned int specifiersLength;
    SecRadioAccessSpecifier specifiers[kSecMaxRadioAccessNetworks];
};

static_assert(sizeof(SecSystemSelectionChannels) == 0x564);

struct SecSignalThresholdInfo {
    int signalMeasurement;
    int hysteresisMs;
    int hysteresisDb;
    int rat;
    int thresholdsLength;
    const int* thresholds;
    int isEnabled;
};

static_assert(offsetof(SecSignalThresholdInfo, thresholds) == 0x18);
static_assert(sizeof(SecSignalThresholdInfo) == 0x28);

struct SecManualSelection {
    int radioAccessNetwork;
    char plmn[8];
};

static_assert(sizeof(SecManualSelection) == 0xc);

struct SecEmergencyScanTrigger {
    int numRadioAccessNetworks;
    int radioAccessNetworks[kSecMaxRadioAccessNetworks];
    int scanType;
};

static_assert(sizeof(SecEmergencyScanTrigger) == 0x28);
