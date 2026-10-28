/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungNetwork"

#include "network/NetworkConvert.h"

#include <aidl/android/hardware/radio/RadioConst.h>

#include <algorithm>
#include <array>
#include <climits>
#include <utility>

#include "common.h"

namespace aidl::android::hardware::radio::samsung {

namespace {

using network::CellIdentity;
using network::CellInfo;
using network::CellInfoRatSpecificInfo;

constexpr int kUnavailable = RadioConst::VALUE_UNAVAILABLE;

// Indexed by AccessNetwork - 1: GERAN, UTRAN, EUTRAN, CDMA2000, IWLAN, NGRAN.
constexpr std::array kRilRadioAccessNetworks = {1, 2, 3, 5, 6, 4};
constexpr std::array kRilRadioTechnologies = {2, 3, 14, 4, 18, 20};

constexpr std::array<int32_t, 34> kNetworkTypeBitmaps = {
        0x18e0e,  0x10006,  0x8e08,   0x18e0e,  0x31f0,   0x70,     0x3180,   0x1bffe,  0x71f0,
        0x1ce0e,  0x1fffe,  0x4000,   0xce08,   0x20000,  0x28e08,  0x24000,  0x30006,  0x34006,
        0x38e0e,  0x2ce08,  0x3ce0e,  0x3bffe,  0x3fffe,  0x100000, 0x104000, 0x1071f0, 0x11ce0e,
        0x11fffe, 0x10ce08, 0x124000, 0x134006, 0x12ce08, 0x13ce0e, 0x13fffe,
};

// LTE_CA counts as LTE.
constexpr std::array<std::pair<int32_t, int32_t>, 7> kNetworkTypeGroups = {{
        {0x10006, 0x10006},    // GSM, GPRS, EDGE
        {0x8e08, 0x8e08},      // UMTS, HSDPA, HSUPA, HSPA, HSPAP
        {0x70, 0x70},          // IS95A, IS95B, 1xRTT
        {0x3180, 0x3180},      // EVDO 0/A/B, EHRPD
        {0x20000, 0x20000},    // TD-SCDMA
        {0x84000, 0x4000},     // LTE, LTE_CA
        {0x100000, 0x100000},  // NR
}};

// Last EARFCN of each band, from 3GPP 36.101.
constexpr std::pair<int, int> kEarfcnBands[] = {
        {599, 1},    {1199, 2},   {1949, 3},   {2399, 4},   {2649, 5},   {2749, 6},   {3449, 7},
        {3799, 8},   {4149, 9},   {4749, 10},  {5009, 11},  {5179, 12},  {5279, 13},  {5379, 14},
        {5729, 0},   {5849, 17},  {6149, 18},  {6449, 20},  {6599, 21},  {7499, 22},  {7699, 23},
        {8039, 24},  {8689, 25},  {9039, 26},  {9209, 27},  {9659, 28},  {9769, 0},   {9869, 30},
        {9919, 31},  {36199, 0},  {36349, 34}, {36949, 35}, {37549, 36}, {37749, 37}, {38249, 38},
        {38649, 39}, {39649, 40}, {41589, 41}, {43589, 42}, {45589, 43}, {46589, 44}, {46789, 45},
        {54539, 46}, {55239, 47}, {56739, 48}, {58239, 49}, {59089, 50}, {59139, 51}, {60139, 52},
        {60254, 53}, {65535, 0},  {66435, 65}, {67365, 66}, {67535, 0},  {67835, 68}, {68335, 0},
        {68585, 70}, {68935, 71}, {68985, 72}, {69035, 73}, {69465, 74}, {70365, 0},  {70545, 85},
        {70595, 87}, {70645, 88},
};

int eutranBand(int earfcn) {
    if (earfcn < 0) {
        return 0;
    }
    for (const auto& [last, band] : kEarfcnBands) {
        if (earfcn <= last) {
            return band;
        }
    }
    return 0;
}

template <typename T>
std::vector<T> castBands(const SecRadioAccessSpecifier& specifier) {
    const auto length = std::min<size_t>(specifier.bandsLength, kSecMaxBands);
    std::vector<T> bands;
    for (size_t i = 0; i < length; i++) {
        bands.push_back(static_cast<T>(specifier.bands[i]));
    }
    return bands;
}

network::OperatorInfo operatorNames(const char (&alphaLong)[kSecOperatorNameLength],
                                    const char (&alphaShort)[kSecOperatorNameLength]) {
    return {
            .alphaLong = fixedString(alphaLong),
            .alphaShort = fixedString(alphaShort),
    };
}

network::CellIdentityGsm toGsm(const SecCellIdentityGsm& id) {
    return {
            .mcc = fixedString(id.mcc),
            .mnc = fixedString(id.mnc),
            .lac = id.lac,
            .cid = id.cid,
            .arfcn = id.arfcn,
            .bsic = static_cast<int8_t>(id.bsic),
            .operatorNames = operatorNames(id.alphaLong, id.alphaShort),
    };
}

network::CellIdentityWcdma toWcdma(const SecCellIdentityWcdma& id) {
    return {
            .mcc = fixedString(id.mcc),
            .mnc = fixedString(id.mnc),
            .lac = id.lac,
            .cid = id.cid,
            .psc = id.psc,
            .uarfcn = id.uarfcn,
            .operatorNames = operatorNames(id.alphaLong, id.alphaShort),
    };
}

network::CellIdentityTdscdma toTdscdma(const SecCellIdentityTdscdma& id) {
    return {
            .mcc = fixedString(id.mcc),
            .mnc = fixedString(id.mnc),
            .lac = id.lac,
            .cid = id.cid,
            .cpid = id.cpid,
            .uarfcn = kUnavailable,
            .operatorNames = operatorNames(id.alphaLong, id.alphaShort),
    };
}

network::CellIdentityCdma toCdma(const SecCellIdentityCdma& id) {
    return {
            .networkId = id.networkId,
            .systemId = id.systemId,
            .baseStationId = id.basestationId,
            .longitude = id.longitude,
            .latitude = id.latitude,
            .operatorNames = operatorNames(id.alphaLong, id.alphaShort),
    };
}

network::CellIdentityLte toLte(const SecCellIdentityLte& id) {
    network::CellIdentityLte lte = {
            .mcc = fixedString(id.mcc),
            .mnc = fixedString(id.mnc),
            .ci = id.ci,
            .pci = id.pci,
            .tac = id.tac,
            .earfcn = id.earfcn,
            .operatorNames = operatorNames(id.alphaLong, id.alphaShort),
            .bandwidth = id.bandwidth,
    };
    if (const int band = eutranBand(id.earfcn); band != 0) {
        lte.bands.push_back(static_cast<network::EutranBands>(band));
    }
    return lte;
}

network::CellIdentityNr toNr(const SecCellIdentityNr& id) {
    network::CellIdentityNr nr = {
            .mcc = fixedString(id.mcc),
            .mnc = fixedString(id.mnc),
            .nci = id.nci,
            .pci = id.pci,
            .tac = id.tac,
            .nrarfcn = id.nrarfcn,
            .operatorNames = operatorNames(id.alphaLong, id.alphaShort),
    };
    if (id.band > 0) {
        nr.bands.push_back(static_cast<network::NgranBands>(id.band));
    }
    return nr;
}

// libsec-ril reports dBm as negative values.
int negate(int value) {
    return value == kUnavailable || value == INT_MIN ? kUnavailable : -value;
}

network::NrSignalStrength toNrSignal(const SecNrSignalStrength& nr, const uint8_t* report,
                                     size_t maxReport) {
    const auto reportSize =
            std::min(static_cast<size_t>(std::max(nr.csiCqiReportSize, 0)), maxReport);
    return {
            .ssRsrp = negate(nr.ssRsrp),
            .ssRsrq = negate(nr.ssRsrq),
            .ssSinr = nr.ssSinr,
            .csiRsrp = negate(nr.csiRsrp),
            .csiRsrq = negate(nr.csiRsrq),
            .csiSinr = nr.csiSinr,
            .csiCqiTableIndex = nr.csiCqiTableIndex,
            .csiCqiReport = {report, report + reportSize},
    };
}

int wcdmaRscpAsu(int rscp) {
    if (rscp == 0xff) {
        return rscp;
    }
    return rscp < -119 ? 0 : rscp < -25 ? rscp + 120 : 96;
}

int wcdmaEcnoAsu(int ecno) {
    if (ecno == 0xff) {
        return ecno;
    }
    return ecno < -24 ? 0 : ecno < 0 ? ecno * 2 + 48 : 49;
}

network::WcdmaSignalStrength toWcdmaSignal(const SecWcdmaSignalStrength& wcdma) {
    return {
            .signalStrength = wcdma.signalStrength,
            .bitErrorRate = wcdma.bitErrorRate,
            .rscp = wcdmaRscpAsu(wcdma.rscp),
            .ecno = wcdmaEcnoAsu(wcdma.ecno),
    };
}

CellInfo toCellInfo(const SecCellInfo& cell) {
    CellInfo info = {
            .registered = cell.registered != 0,
            .connectionStatus = static_cast<network::CellConnectionStatus>(cell.connectionStatus),
    };
    switch (cell.cellInfoType) {
        case SEC_CELL_INFO_TYPE_GSM:
            info.ratSpecificInfo = CellInfoRatSpecificInfo::make<CellInfoRatSpecificInfo::gsm>(
                    network::CellInfoGsm{
                            .cellIdentityGsm = toGsm(cell.gsm.identity),
                            .signalStrengthGsm = {cell.gsm.signal.signalStrength,
                                                  cell.gsm.signal.bitErrorRate,
                                                  cell.gsm.signal.timingAdvance},
                    });
            break;
        case SEC_CELL_INFO_TYPE_CDMA:
            info.ratSpecificInfo = CellInfoRatSpecificInfo::make<CellInfoRatSpecificInfo::cdma>(
                    network::CellInfoCdma{
                            .cellIdentityCdma = toCdma(cell.cdma.identity),
                            .signalStrengthCdma = {cell.cdma.cdma.dbm, cell.cdma.cdma.ecio},
                            .signalStrengthEvdo = {cell.cdma.evdo.dbm, cell.cdma.evdo.ecio,
                                                   cell.cdma.evdo.signalNoiseRatio},
                    });
            break;
        case SEC_CELL_INFO_TYPE_LTE:
        case SEC_CELL_INFO_TYPE_LTE_ALT: {
            const auto& signal = cell.lte.signal;
            info.ratSpecificInfo = CellInfoRatSpecificInfo::make<CellInfoRatSpecificInfo::lte>(
                    network::CellInfoLte{
                            .cellIdentityLte = toLte(cell.lte.identity),
                            .signalStrengthLte = {signal.signalStrength, signal.rsrp, signal.rsrq,
                                                  signal.rssnr, signal.cqi, signal.timingAdvance,
                                                  kUnavailable},
                    });
            break;
        }
        case SEC_CELL_INFO_TYPE_WCDMA:
            info.ratSpecificInfo = CellInfoRatSpecificInfo::make<CellInfoRatSpecificInfo::wcdma>(
                    network::CellInfoWcdma{
                            .cellIdentityWcdma = toWcdma(cell.wcdma.identity),
                            .signalStrengthWcdma = toWcdmaSignal(cell.wcdma.signal),
                    });
            break;
        case SEC_CELL_INFO_TYPE_TD_SCDMA:
            info.ratSpecificInfo = CellInfoRatSpecificInfo::make<CellInfoRatSpecificInfo::tdscdma>(
                    network::CellInfoTdscdma{
                            .cellIdentityTdscdma = toTdscdma(cell.tdscdma.identity),
                            .signalStrengthTdscdma = {kUnavailable, kUnavailable,
                                                      cell.tdscdma.rscp},
                    });
            break;
        case SEC_CELL_INFO_TYPE_NR:
            info.ratSpecificInfo =
                    CellInfoRatSpecificInfo::make<CellInfoRatSpecificInfo::nr>(network::CellInfoNr{
                            .cellIdentityNr = toNr(cell.nr.identity),
                            .signalStrengthNr = toNrSignal(cell.nr.signal, cell.nr.csiCqiReport,
                                                           sizeof(cell.nr.csiCqiReport)),
                    });
            break;
        default:
            LOG(WARNING) << "Unknown cell info type " << cell.cellInfoType;
    }
    return info;
}

}  // namespace

int toRilRadioAccessNetwork(AccessNetwork accessNetwork) {
    const auto index = static_cast<size_t>(accessNetwork) - 1;
    return index < kRilRadioAccessNetworks.size() ? kRilRadioAccessNetworks[index] : 0;
}

AccessNetwork toAccessNetwork(int radioAccessNetwork) {
    const auto it = std::find(kRilRadioAccessNetworks.begin(), kRilRadioAccessNetworks.end(),
                              radioAccessNetwork);
    if (it == kRilRadioAccessNetworks.end()) {
        return AccessNetwork::UNKNOWN;
    }
    return static_cast<AccessNetwork>(it - kRilRadioAccessNetworks.begin() + 1);
}

int toRilRadioTechnology(AccessNetwork accessNetwork) {
    const auto index = static_cast<size_t>(accessNetwork) - 1;
    return index < kRilRadioTechnologies.size() ? kRilRadioTechnologies[index] : 0;
}

RadioTechnology toRadioTechnology(int rat) {
    // Samsung-only RAT.
    constexpr int kSecRatLteVariant = 21;
    if (rat == kSecRatLteVariant) {
        return RadioTechnology::LTE;
    }
    return rat >= 0 && rat <= static_cast<int>(RadioTechnology::NR)
                   ? static_cast<RadioTechnology>(rat)
                   : RadioTechnology::UNKNOWN;
}

int toOperatorStatus(const std::string& status) {
    constexpr std::array kStatuses = {"unknown", "available", "current", "forbidden"};
    const auto it = std::find(kStatuses.begin(), kStatuses.end(), status);
    return it != kStatuses.end() ? it - kStatuses.begin() : -1;
}

std::optional<int> toNetworkType(int32_t networkTypeBitmap) {
    int32_t adjusted = 0;
    for (const auto& [mask, group] : kNetworkTypeGroups) {
        if ((networkTypeBitmap & mask) != 0) {
            adjusted |= group;
        }
    }
    const auto it = std::find(kNetworkTypeBitmaps.begin(), kNetworkTypeBitmaps.end(), adjusted);
    if (it == kNetworkTypeBitmaps.end()) {
        return std::nullopt;
    }
    return it - kNetworkTypeBitmaps.begin();
}

std::optional<int32_t> toNetworkTypeBitmap(int networkType) {
    if (networkType < 0 || networkType >= static_cast<int>(kNetworkTypeBitmaps.size())) {
        return std::nullopt;
    }
    return kNetworkTypeBitmaps[networkType];
}

CellIdentity toCellIdentity(const SecCellIdentity& identity) {
    switch (identity.cellInfoType) {
        case SEC_CELL_INFO_TYPE_GSM:
            return CellIdentity::make<CellIdentity::gsm>(toGsm(identity.gsm));
        case SEC_CELL_INFO_TYPE_CDMA:
            return CellIdentity::make<CellIdentity::cdma>(toCdma(identity.cdma));
        case SEC_CELL_INFO_TYPE_LTE:
        case SEC_CELL_INFO_TYPE_LTE_ALT:
            return CellIdentity::make<CellIdentity::lte>(toLte(identity.lte));
        case SEC_CELL_INFO_TYPE_WCDMA:
            return CellIdentity::make<CellIdentity::wcdma>(toWcdma(identity.wcdma));
        case SEC_CELL_INFO_TYPE_TD_SCDMA:
            return CellIdentity::make<CellIdentity::tdscdma>(toTdscdma(identity.tdscdma));
        case SEC_CELL_INFO_TYPE_NR:
            return CellIdentity::make<CellIdentity::nr>(toNr(identity.nr));
    }
    return {};
}

network::RegStateResult toVoiceRegState(const SecVoiceRegState& state) {
    using Info = network::AccessTechnologySpecificInfo;
    network::RegStateResult result = {
            .regState = static_cast<network::RegState>(state.regState),
            .rat = toRadioTechnology(state.rat),
            .reasonForDenial = static_cast<network::RegistrationFailCause>(state.reasonForDenial),
            .cellIdentity = toCellIdentity(state.cellIdentity),
            .registeredPlmn = fixedString(state.registeredPlmn),
    };
    switch (result.rat) {
        case RadioTechnology::IS95A:
        case RadioTechnology::IS95B:
        case RadioTechnology::ONE_X_RTT:
        case RadioTechnology::EVDO_0:
        case RadioTechnology::EVDO_A:
            result.accessTechnologySpecificInfo =
                    Info::make<Info::cdmaInfo>(network::Cdma2000RegistrationInfo{
                            .cssSupported = state.cssSupported != 0,
                            .roamingIndicator = state.roamingIndicator,
                            .systemIsInPrl = state.systemIsInPrl,
                            .defaultRoamingIndicator = state.defaultRoamingIndicator,
                    });
            break;
        case RadioTechnology::GPRS:
        case RadioTechnology::EDGE:
        case RadioTechnology::GSM:
            result.accessTechnologySpecificInfo =
                    Info::make<Info::geranDtmSupported>(state.geranDtmSupported != 0);
            break;
        default:
            break;
    }
    return result;
}

network::RegStateResult toDataRegState(const SecDataRegState& state) {
    using Info = network::AccessTechnologySpecificInfo;
    network::RegStateResult result = {
            .regState = static_cast<network::RegState>(state.regState),
            .rat = toRadioTechnology(state.rat),
            .reasonForDenial = static_cast<network::RegistrationFailCause>(state.reasonDataDenied),
            .cellIdentity = toCellIdentity(state.cellIdentity),
            .registeredPlmn = fixedString(state.registeredPlmn),
    };
    switch (static_cast<RadioTechnology>(state.dataRat)) {
        case RadioTechnology::LTE:
        case RadioTechnology::LTE_CA:
            result.accessTechnologySpecificInfo =
                    Info::make<Info::eutranInfo>(network::EutranRegistrationInfo{
                            .lteVopsInfo = {state.isVopsSupported != 0,
                                            state.isEmcBearerSupported != 0},
                            .nrIndicators = {state.isEndcAvailable != 0,
                                             state.isDcNrRestricted != 0, state.isNrAvailable != 0},
                    });
            break;
        case RadioTechnology::NR:
            result.accessTechnologySpecificInfo =
                    Info::make<Info::ngranNrVopsInfo>(network::NrVopsInfo{
                            .vopsSupported = static_cast<int8_t>(state.nrVopsSupported),
                            .emcSupported = static_cast<int8_t>(state.nrEmcSupported),
                            .emfSupported = static_cast<int8_t>(state.nrEmfSupported),
                    });
            break;
        default:
            break;
    }
    return result;
}

network::SignalStrength toSignalStrength(const SecSignalStrength& signal) {
    const auto& lte = signal.lte;
    const auto orUnavailable = [](int value) { return value == -1 ? kUnavailable : value; };
    return {
            .gsm = {signal.gsm.signalStrength, signal.gsm.bitErrorRate, kUnavailable},
            .cdma = {signal.cdma.dbm, signal.cdma.ecio},
            .evdo = {signal.evdo.dbm, signal.evdo.ecio, signal.evdo.signalNoiseRatio},
            .lte = {lte.signalStrength == -1 ? 99 : lte.signalStrength,
                    lte.rsrp < -1 ? negate(lte.rsrp) : orUnavailable(lte.rsrp),
                    orUnavailable(lte.rsrq), lte.rssnr, orUnavailable(lte.cqi), lte.timingAdvance,
                    signal.lteCqiTableIndex},
            .tdscdma = {kUnavailable, kUnavailable, signal.tdscdmaRscp},
            .wcdma = toWcdmaSignal(signal.wcdma),
            .nr = toNrSignal(signal.nr, signal.nrCsiCqiReport, sizeof(signal.nrCsiCqiReport)),
    };
}

std::optional<std::vector<CellInfo>> toCellInfoList(const void* data, size_t length) {
    if (length % sizeof(SecCellInfo) != 0 || (data == nullptr && length != 0)) {
        LOG(ERROR) << "Invalid cell info list length " << length;
        return std::nullopt;
    }
    return toCellInfoList(static_cast<const SecCellInfo*>(data), length / sizeof(SecCellInfo));
}

std::vector<CellInfo> toCellInfoList(const SecCellInfo* cells, size_t count) {
    std::vector<CellInfo> list;
    for (size_t i = 0; i < count; i++) {
        list.push_back(toCellInfo(cells[i]));
    }
    return list;
}

std::optional<std::vector<network::PhysicalChannelConfig>> toPhysicalChannelConfigs(
        const void* data, size_t length) {
    using Band = network::PhysicalChannelConfigBand;
    if (length % sizeof(SecPhysicalChannelConfig) != 0 || (data == nullptr && length != 0)) {
        LOG(ERROR) << "Invalid physical channel config length " << length;
        return std::nullopt;
    }
    const auto* configs = static_cast<const SecPhysicalChannelConfig*>(data);
    std::vector<network::PhysicalChannelConfig> list;
    for (size_t i = 0; i < length / sizeof(SecPhysicalChannelConfig); i++) {
        const auto& config = configs[i];
        network::PhysicalChannelConfig out = {
                .status = static_cast<network::CellConnectionStatus>(config.status),
                .rat = toRadioTechnology(config.rat),
                .downlinkChannelNumber = config.downlinkChannelNumber,
                .uplinkChannelNumber = config.uplinkChannelNumber,
                .cellBandwidthDownlinkKhz = config.cellBandwidthDownlinkKhz,
                .cellBandwidthUplinkKhz = config.cellBandwidthUplinkKhz,
                .physicalCellId = config.physicalCellId,
        };
        if (config.contextIds != nullptr && config.numContextIds > 0) {
            out.contextIds.assign(config.contextIds, config.contextIds + config.numContextIds);
        }
        switch (out.rat) {
            case RadioTechnology::GPRS:
            case RadioTechnology::EDGE:
            case RadioTechnology::GSM:
                out.band =
                        Band::make<Band::geranBand>(static_cast<network::GeranBands>(config.band));
                break;
            case RadioTechnology::UMTS:
            case RadioTechnology::HSDPA:
            case RadioTechnology::HSUPA:
            case RadioTechnology::HSPA:
            case RadioTechnology::HSPAP:
            case RadioTechnology::TD_SCDMA:
                out.band =
                        Band::make<Band::utranBand>(static_cast<network::UtranBands>(config.band));
                break;
            case RadioTechnology::LTE:
            case RadioTechnology::LTE_CA:
                out.band = Band::make<Band::eutranBand>(
                        static_cast<network::EutranBands>(config.band));
                break;
            case RadioTechnology::NR:
                out.band =
                        Band::make<Band::ngranBand>(static_cast<network::NgranBands>(config.band));
                break;
            default:
                break;
        }
        list.push_back(std::move(out));
    }
    return list;
}

std::vector<network::BarringInfo> toBarringInfos(const SecBarringInfo& info) {
    constexpr int kConditionalBarring = 1;
    std::vector<network::BarringInfo> list;
    const int count =
            std::clamp(info.numBarringInfos, 0, static_cast<int>(std::size(info.barringInfos)));
    for (int i = 0; i < count; i++) {
        const auto& barring = info.barringInfos[i];
        network::BarringInfo out = {
                .serviceType = barring.serviceType,
                .barringType = barring.barringType,
        };
        if (barring.barringType == kConditionalBarring) {
            out.barringTypeSpecificInfo = network::BarringTypeSpecificInfo{
                    .factor = barring.factor,
                    .timeSeconds = barring.timeSeconds,
                    .isBarred = barring.isBarred != 0,
            };
        }
        list.push_back(std::move(out));
    }
    return list;
}

network::LinkCapacityEstimate toLinkCapacityEstimate(const SecLinkCapacityEstimate& lce) {
    return {
            .downlinkCapacityKbps = lce.downlinkCapacityKbps,
            .uplinkCapacityKbps = lce.uplinkCapacityKbps,
            .secondaryDownlinkCapacityKbps = -1,
            .secondaryUplinkCapacityKbps = -1,
    };
}

network::EmergencyRegResult toEmergencyRegResult(const SecEmergencyRegResult& result) {
    return {
            .accessNetwork = toAccessNetwork(result.accessNetwork),
            .regState = static_cast<network::RegState>(result.regState),
            .emcDomain = static_cast<network::Domain>(result.emcDomain),
            .isVopsSupported = result.isVopsSupported != 0,
            .isEmcBearerSupported = result.isEmcBearerSupported != 0,
            .nwProvidedEmc = result.nwProvidedEmc,
            .nwProvidedEmf = result.nwProvidedEmf,
            .mcc = fixedString(result.mcc),
            .mnc = fixedString(result.mnc),
    };
}

bool toSecRadioAccessSpecifier(const network::RadioAccessSpecifier& specifier,
                               SecRadioAccessSpecifier& out) {
    using Bands = network::RadioAccessSpecifierBands;
    out = {.radioAccessNetwork = toRilRadioAccessNetwork(specifier.accessNetwork)};

    std::vector<int> bands;
    switch (specifier.bands.getTag()) {
        case Bands::geranBands:
            for (const auto band : specifier.bands.get<Bands::geranBands>()) {
                bands.push_back(static_cast<int>(band));
            }
            break;
        case Bands::utranBands:
            for (const auto band : specifier.bands.get<Bands::utranBands>()) {
                bands.push_back(static_cast<int>(band));
            }
            break;
        case Bands::eutranBands:
            for (const auto band : specifier.bands.get<Bands::eutranBands>()) {
                bands.push_back(static_cast<int>(band));
            }
            break;
        case Bands::ngranBands:
            for (const auto band : specifier.bands.get<Bands::ngranBands>()) {
                bands.push_back(static_cast<int>(band));
            }
            break;
        default:
            break;
    }
    if (bands.size() > kSecMaxBands || specifier.channels.size() > kSecMaxChannels) {
        return false;
    }
    out.bandsLength = bands.size();
    std::copy(bands.begin(), bands.end(), out.bands);
    out.channelsLength = specifier.channels.size();
    std::copy(specifier.channels.begin(), specifier.channels.end(), out.channels);
    return true;
}

network::RadioAccessSpecifier toRadioAccessSpecifier(const SecRadioAccessSpecifier& specifier) {
    using Bands = network::RadioAccessSpecifierBands;
    network::RadioAccessSpecifier out = {
            .accessNetwork = toAccessNetwork(specifier.radioAccessNetwork),
    };
    const auto channelsLength = std::min<size_t>(specifier.channelsLength, kSecMaxChannels);
    switch (out.accessNetwork) {
        case AccessNetwork::GERAN:
            out.bands = Bands::make<Bands::geranBands>(castBands<network::GeranBands>(specifier));
            break;
        case AccessNetwork::UTRAN:
            out.bands = Bands::make<Bands::utranBands>(castBands<network::UtranBands>(specifier));
            break;
        case AccessNetwork::EUTRAN:
            out.bands = Bands::make<Bands::eutranBands>(castBands<network::EutranBands>(specifier));
            break;
        case AccessNetwork::NGRAN:
            out.bands = Bands::make<Bands::ngranBands>(castBands<network::NgranBands>(specifier));
            break;
        default:
            break;
    }
    out.channels.assign(specifier.channels, specifier.channels + channelsLength);
    return out;
}

}  // namespace aidl::android::hardware::radio::samsung
