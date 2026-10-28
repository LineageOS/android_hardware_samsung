/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungNetwork"

#include "network/RadioNetwork.h"

#include <android-base/chrono_utils.h>

#include <chrono>

#include "RadioSlot.h"
#include "network/NetworkConvert.h"

namespace aidl::android::hardware::radio::samsung {

using ::ndk::ScopedAStatus;

namespace {

int64_t elapsedRealtimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                   ::android::base::boot_clock::now().time_since_epoch())
            .count();
}

}  // namespace

RadioNetwork::RadioNetwork(RadioSlot& slot) : mSlot(slot) {}

ScopedAStatus RadioNetwork::setResponseFunctions(
        const std::shared_ptr<network::IRadioNetworkResponse>& response,
        const std::shared_ptr<network::IRadioNetworkIndication>& indication) {
    setCallbacks(response, indication);

    int32_t version = 0;
    if (response != nullptr && response->getInterfaceVersion(&version).isOk()) {
        RadioSlot::VendorRil::get().setClientHalVersion(mSlot.socket(), version);
    }

    // Replay the last NITZ to a new client, like libril.
    std::lock_guard lock(mNitzMutex);
    if (indication != nullptr && mLastNitz.has_value()) {
        indication->nitzTimeReceived(kUnsolicited, mLastNitz->time, mLastNitz->receivedTimeMs, 0);
    }
    return ok();
}

ScopedAStatus RadioNetwork::responseAcknowledgement() {
    return ok();
}

ScopedAStatus RadioNetwork::getAllowedNetworkTypesBitmap(int32_t serial) {
    LOG_CALL;
    mSlot.request(SEC_REQUEST_GET_ALLOWED_NETWORK_TYPES_BITMAP, [this, serial](auto e, auto data,
                                                                               auto length) {
        const auto type = responseInt(data, length);
        const auto bitmap = type.has_value() ? toNetworkTypeBitmap(*type) : std::nullopt;
        respond()->getAllowedNetworkTypesBitmapResponse(responseInfo(serial, e, bitmap.has_value()),
                                                        bitmap.value_or(0));
    });
    return ok();
}

ScopedAStatus RadioNetwork::getAvailableBandModes(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE,
                  [this, serial](auto e, auto data, auto length) {
                      std::vector<network::RadioBandMode> modes;
                      for (const auto mode : responseInts(data, length)) {
                          modes.push_back(static_cast<network::RadioBandMode>(mode));
                      }
                      respond()->getAvailableBandModesResponse(responseInfo(serial, e), modes);
                  });
    return ok();
}

ScopedAStatus RadioNetwork::getAvailableNetworks(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            RIL_REQUEST_QUERY_AVAILABLE_NETWORKS, [this, serial](auto e, auto data, auto length) {
                constexpr size_t kStringsPerOperator = 4;
                constexpr size_t kOperatorSize = kStringsPerOperator * sizeof(char*);
                const bool valid = length % kOperatorSize == 0 && (data != nullptr || length == 0);
                std::vector<network::OperatorInfo> operators;
                const auto* strings = static_cast<char* const*>(data);
                for (size_t i = 0; valid && i < length / kOperatorSize; i++) {
                    const auto* op = strings + i * kStringsPerOperator;
                    operators.push_back({
                            .alphaLong = toString(op[0]),
                            .alphaShort = toString(op[1]),
                            .operatorNumeric = toString(op[2]),
                            .status = toOperatorStatus(toString(op[3])),
                    });
                }
                respond()->getAvailableNetworksResponse(responseInfo(serial, e, valid), operators);
            });
    return ok();
}

ScopedAStatus RadioNetwork::getBarringInfo(int32_t serial) {
    LOG_CALL;
    mSlot.request(SEC_REQUEST_GET_BARRING_INFO, [this, serial](auto e, auto data, auto length) {
        const bool valid = data != nullptr && length >= sizeof(SecBarringInfo) &&
                           length % sizeof(SecBarringInfo) == 0;
        network::CellIdentity identity;
        std::vector<network::BarringInfo> infos;
        if (valid) {
            const auto* info = static_cast<const SecBarringInfo*>(data);
            identity = toCellIdentity(info->cellIdentity);
            infos = toBarringInfos(*info);
        }
        respond()->getBarringInfoResponse(responseInfo(serial, e, valid), identity, infos);
    });
    return ok();
}

ScopedAStatus RadioNetwork::getCdmaRoamingPreference(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE,
            intResponse<network::CdmaRoamingType>(
                    serial, &network::IRadioNetworkResponse::getCdmaRoamingPreferenceResponse));
    return ok();
}

ScopedAStatus RadioNetwork::getCellInfoList(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_GET_CELL_INFO_LIST, [this, serial](auto e, auto data, auto length) {
        const auto cells = toCellInfoList(data, length);
        respond()->getCellInfoListResponse(responseInfo(serial, e, cells.has_value()),
                                           cells.value_or(std::vector<network::CellInfo>{}));
    });
    return ok();
}

ScopedAStatus RadioNetwork::getDataRegistrationState(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            RIL_REQUEST_DATA_REGISTRATION_STATE, [this, serial](auto e, auto data, auto length) {
                const auto* state = responseAs<SecDataRegState>(data, length);
                respond()->getDataRegistrationStateResponse(
                        responseInfo(serial, e, state != nullptr),
                        state != nullptr ? toDataRegState(*state) : network::RegStateResult{});
            });
    return ok();
}

ScopedAStatus RadioNetwork::getImsRegistrationState(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_IMS_REGISTRATION_STATE,
                  [this, serial](auto e, auto data, auto length) {
                      const auto ints = responseInts(data, length);
                      const bool valid = ints.size() >= 2;
                      respond()->getImsRegistrationStateResponse(
                              responseInfo(serial, e, valid), valid && ints[0] != 0,
                              static_cast<RadioTechnologyFamily>(valid ? ints[1] : 0));
                  });
    return ok();
}

ScopedAStatus RadioNetwork::getNetworkSelectionMode(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE,
            intResponse<bool>(serial,
                              &network::IRadioNetworkResponse::getNetworkSelectionModeResponse));
    return ok();
}

ScopedAStatus RadioNetwork::getOperator(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_OPERATOR, [this, serial](auto e, auto data, auto length) {
        const auto* names = static_cast<char* const*>(data);
        const bool valid = names != nullptr && length == 3 * sizeof(char*);
        respond()->getOperatorResponse(
                responseInfo(serial, e, valid), valid ? toString(names[0]) : "",
                valid ? toString(names[1]) : "", valid ? toString(names[2]) : "");
    });
    return ok();
}

ScopedAStatus RadioNetwork::getSignalStrength(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_SIGNAL_STRENGTH, [this, serial](auto e, auto data, auto length) {
        const auto* signal = responseAs<SecSignalStrength>(data, length);
        respond()->getSignalStrengthResponse(
                responseInfo(serial, e, signal != nullptr),
                signal != nullptr ? toSignalStrength(*signal) : network::SignalStrength{});
    });
    return ok();
}

ScopedAStatus RadioNetwork::getSystemSelectionChannels(int32_t serial) {
    LOG_CALL;
    mSlot.request(SEC_REQUEST_GET_SYSTEM_SELECTION_CHANNELS,
                  [this, serial](auto e, auto data, auto length) {
                      const auto* channels = responseAs<SecSystemSelectionChannels>(data, length);
                      std::vector<network::RadioAccessSpecifier> specifiers;
                      if (channels != nullptr) {
                          const auto count = std::min<size_t>(channels->specifiersLength,
                                                              kSecMaxRadioAccessNetworks);
                          for (size_t i = 0; i < count; i++) {
                              specifiers.push_back(toRadioAccessSpecifier(channels->specifiers[i]));
                          }
                      }
                      respond()->getSystemSelectionChannelsResponse(
                              responseInfo(serial, e, channels != nullptr), specifiers);
                  });
    return ok();
}

ScopedAStatus RadioNetwork::getVoiceRadioTechnology(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_VOICE_RADIO_TECH, [this, serial](auto e, auto data, auto length) {
        const auto rat = responseInt(data, length);
        respond()->getVoiceRadioTechnologyResponse(responseInfo(serial, e, rat.has_value()),
                                                   toRadioTechnology(rat.value_or(0)));
    });
    return ok();
}

ScopedAStatus RadioNetwork::getVoiceRegistrationState(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            RIL_REQUEST_VOICE_REGISTRATION_STATE, [this, serial](auto e, auto data, auto length) {
                const auto* state = responseAs<SecVoiceRegState>(data, length);
                respond()->getVoiceRegistrationStateResponse(
                        responseInfo(serial, e, state != nullptr),
                        state != nullptr ? toVoiceRegState(*state) : network::RegStateResult{});
            });
    return ok();
}

ScopedAStatus RadioNetwork::isNrDualConnectivityEnabled(int32_t serial) {
    LOG_CALL;
    mSlot.request(SEC_REQUEST_IS_NR_DUAL_CONNECTIVITY_ENABLED, [this, serial](auto e, auto data,
                                                                              auto length) {
        const auto enabled = responseInt(data, length);
        respond()->isNrDualConnectivityEnabledResponse(responseInfo(serial, e, enabled.has_value()),
                                                       enabled.value_or(0) == 1);
    });
    return ok();
}

ScopedAStatus RadioNetwork::setAllowedNetworkTypesBitmap(int32_t serial,
                                                         int32_t networkTypeBitmap) {
    LOG_CALL << " 0x" << std::hex << networkTypeBitmap;
    const auto type = toNetworkType(networkTypeBitmap);
    if (!type.has_value()) {
        LOG(ERROR) << "No network type matches bitmap 0x" << std::hex << networkTypeBitmap;
        respond()->setAllowedNetworkTypesBitmapResponse(
                responseInfo(serial, RadioError::INVALID_ARGUMENTS));
        return ok();
    }
    mSlot.requestInts(
            SEC_REQUEST_SET_ALLOWED_NETWORK_TYPES_BITMAP, {*type},
            statusResponse(serial,
                           &network::IRadioNetworkResponse::setAllowedNetworkTypesBitmapResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setBandMode(int32_t serial, network::RadioBandMode mode) {
    LOG_CALL << ' ' << toString(mode);
    mSlot.requestInts(RIL_REQUEST_SET_BAND_MODE, {static_cast<int>(mode)},
                      statusResponse(serial, &network::IRadioNetworkResponse::setBandModeResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setBarringPassword(int32_t serial, const std::string& facility,
                                               const std::string& oldPassword,
                                               const std::string& newPassword) {
    LOG_CALL << ' ' << facility;
    mSlot.requestStrings(
            RIL_REQUEST_CHANGE_BARRING_PASSWORD, {facility, oldPassword, newPassword},
            statusResponse(serial, &network::IRadioNetworkResponse::setBarringPasswordResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setCdmaRoamingPreference(int32_t serial,
                                                     network::CdmaRoamingType type) {
    LOG_CALL << ' ' << toString(type);
    mSlot.requestInts(
            RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE, {static_cast<int>(type)},
            statusResponse(serial,
                           &network::IRadioNetworkResponse::setCdmaRoamingPreferenceResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setCellInfoListRate(int32_t serial, int32_t rate) {
    LOG_CALL << ' ' << rate;
    mSlot.requestInts(
            RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE, {rate},
            statusResponse(serial, &network::IRadioNetworkResponse::setCellInfoListRateResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setIndicationFilter(int32_t serial, int32_t indicationFilter) {
    LOG_CALL << " 0x" << std::hex << indicationFilter;
    mSlot.requestInts(
            RIL_REQUEST_SET_UNSOLICITED_RESPONSE_FILTER, {indicationFilter},
            statusResponse(serial, &network::IRadioNetworkResponse::setIndicationFilterResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setLinkCapacityReportingCriteria(
        int32_t serial, int32_t hysteresisMs, int32_t hysteresisDlKbps, int32_t hysteresisUlKbps,
        const std::vector<int32_t>& thresholdsDownlinkKbps,
        const std::vector<int32_t>& thresholdsUplinkKbps, AccessNetwork accessNetwork) {
    LOG_CALL << ' ' << toString(accessNetwork);
    if (accessNetwork != AccessNetwork::UTRAN && accessNetwork != AccessNetwork::EUTRAN) {
        respond()->setLinkCapacityReportingCriteriaResponse(notSupported(serial));
        return ok();
    }
    std::vector<int> criteria = {hysteresisMs, hysteresisDlKbps, hysteresisUlKbps,
                                 static_cast<int>(accessNetwork),
                                 static_cast<int>(thresholdsDownlinkKbps.size())};
    criteria.insert(criteria.end(), thresholdsDownlinkKbps.begin(), thresholdsDownlinkKbps.end());
    criteria.push_back(thresholdsUplinkKbps.size());
    criteria.insert(criteria.end(), thresholdsUplinkKbps.begin(), thresholdsUplinkKbps.end());
    mSlot.requestInts(
            SEC_REQUEST_SET_LINK_CAPACITY_REPORTING_CRITERIA, std::move(criteria),
            statusResponse(
                    serial,
                    &network::IRadioNetworkResponse::setLinkCapacityReportingCriteriaResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setLocationUpdates(int32_t serial, bool enable) {
    LOG_CALL << ' ' << enable;
    mSlot.requestInts(
            RIL_REQUEST_SET_LOCATION_UPDATES, {enable},
            statusResponse(serial, &network::IRadioNetworkResponse::setLocationUpdatesResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setNetworkSelectionModeAutomatic(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC,
            statusResponse(
                    serial,
                    &network::IRadioNetworkResponse::setNetworkSelectionModeAutomaticResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setNetworkSelectionModeManual(int32_t serial,
                                                          const std::string& operatorNumeric,
                                                          AccessNetwork ran) {
    LOG_CALL << ' ' << operatorNumeric << ' ' << toString(ran);
    // libril_sem rejects the "123" test PLMN before it reaches the modem.
    if (operatorNumeric.starts_with("123")) {
        respond()->setNetworkSelectionModeManualResponse(
                responseInfo(serial, RadioError::INVALID_ARGUMENTS));
        return ok();
    }
    SecManualSelection selection = {.radioAccessNetwork = toRilRadioAccessNetwork(ran)};
    operatorNumeric.copy(selection.plmn, 6);
    mSlot.request(
            RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL, &selection, sizeof(selection),
            statusResponse(serial,
                           &network::IRadioNetworkResponse::setNetworkSelectionModeManualResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setNrDualConnectivityState(
        int32_t serial, network::NrDualConnectivityState nrDualConnectivityState) {
    LOG_CALL << ' ' << toString(nrDualConnectivityState);
    mSlot.requestInts(
            SEC_REQUEST_ENABLE_NR_DUAL_CONNECTIVITY, {static_cast<int>(nrDualConnectivityState)},
            statusResponse(serial,
                           &network::IRadioNetworkResponse::setNrDualConnectivityStateResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setSignalStrengthReportingCriteria(
        int32_t serial, const std::vector<network::SignalThresholdInfo>& signalThresholdInfos) {
    LOG_CALL << ' ' << signalThresholdInfos.size();
    if (signalThresholdInfos.empty() || signalThresholdInfos[0].hysteresisDb > 2) {
        respond()->setSignalStrengthReportingCriteriaResponse(
                responseInfo(serial, RadioError::INVALID_ARGUMENTS));
        return ok();
    }
    const auto& info = signalThresholdInfos[0];
    const bool knownMeasurement = info.signalMeasurement >= 1 && info.signalMeasurement <= 8;
    SecSignalThresholdInfo criteria = {
            .signalMeasurement = knownMeasurement ? info.signalMeasurement : 0,
            .hysteresisMs = info.hysteresisMs,
            .hysteresisDb = info.hysteresisDb,
            .rat = toRilRadioTechnology(info.ran),
            .thresholdsLength = static_cast<int>(info.thresholds.size()),
            .thresholds = info.thresholds.data(),
            .isEnabled = info.isEnabled,
    };
    mSlot.request(
            SEC_REQUEST_SET_SIGNAL_STRENGTH_REPORTING_CRITERIA, &criteria, sizeof(criteria),
            statusResponse(
                    serial,
                    &network::IRadioNetworkResponse::setSignalStrengthReportingCriteriaResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setSuppServiceNotifications(int32_t serial, bool enable) {
    LOG_CALL << ' ' << enable;
    mSlot.requestInts(
            RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION, {enable},
            statusResponse(serial,
                           &network::IRadioNetworkResponse::setSuppServiceNotificationsResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setSystemSelectionChannels(
        int32_t serial, bool specifyChannels,
        const std::vector<network::RadioAccessSpecifier>& specifiers) {
    LOG_CALL << ' ' << specifyChannels << ' ' << specifiers.size();
    SecSystemSelectionChannels channels = {};
    if (specifyChannels) {
        channels.specifiersLength = std::min<size_t>(specifiers.size(), kSecMaxRadioAccessNetworks);
        for (size_t i = 0; i < channels.specifiersLength; i++) {
            if (!toSecRadioAccessSpecifier(specifiers[i], channels.specifiers[i])) {
                respond()->setSystemSelectionChannelsResponse(
                        responseInfo(serial, RadioError::INVALID_ARGUMENTS));
                return ok();
            }
        }
    }
    mSlot.request(
            SEC_REQUEST_SET_SYSTEM_SELECTION_CHANNELS, &channels, sizeof(channels),
            statusResponse(serial,
                           &network::IRadioNetworkResponse::setSystemSelectionChannelsResponse));
    return ok();
}

ScopedAStatus RadioNetwork::startNetworkScan(int32_t serial,
                                             const network::NetworkScanRequest& request) {
    LOG_CALL << ' ' << request.type << ' ' << request.specifiers.size();
    constexpr int kMinInterval = 5;
    constexpr int kMaxInterval = 300;
    const bool valid = !request.specifiers.empty() &&
                       request.specifiers.size() <= kSecMaxRadioAccessNetworks &&
                       request.mccMncs.size() <= kSecMaxScanPlmns &&
                       request.interval >= kMinInterval && request.interval <= kMaxInterval;
    SecNetworkScanRequest scan = {
            // RIL_ScanType is ONE_SHOT = 1, PERIODIC = 2.
            .type = request.type + 1,
            .interval = request.interval,
            .specifiersLength = static_cast<unsigned int>(request.specifiers.size()),
            .maxSearchTime = request.maxSearchTime,
            .incrementalResults = request.incrementalResults,
            .incrementalResultsPeriodicity = request.incrementalResultsPeriodicity,
            .mccMncsLength = static_cast<unsigned int>(request.mccMncs.size()),
    };
    bool converted = valid;
    for (size_t i = 0; converted && i < request.specifiers.size(); i++) {
        converted = toSecRadioAccessSpecifier(request.specifiers[i], scan.specifiers[i]);
    }
    for (size_t i = 0; converted && i < request.mccMncs.size(); i++) {
        request.mccMncs[i].copy(scan.mccMncs[i], sizeof(scan.mccMncs[i]) - 1);
    }
    if (!converted) {
        respond()->startNetworkScanResponse(responseInfo(serial, RadioError::INVALID_ARGUMENTS));
        return ok();
    }
    mSlot.request(
            RIL_REQUEST_START_NETWORK_SCAN, &scan, sizeof(scan),
            statusResponse(serial, &network::IRadioNetworkResponse::startNetworkScanResponse));
    return ok();
}

ScopedAStatus RadioNetwork::stopNetworkScan(int32_t serial) {
    LOG_CALL;
    mSlot.request(RIL_REQUEST_STOP_NETWORK_SCAN,
                  statusResponse(serial, &network::IRadioNetworkResponse::stopNetworkScanResponse));
    return ok();
}

ScopedAStatus RadioNetwork::supplyNetworkDepersonalization(int32_t serial,
                                                           const std::string& netPin) {
    LOG_CALL;
    mSlot.requestStrings(RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION, {netPin},
                         [this, serial](auto e, auto data, auto length) {
                             respond()->supplyNetworkDepersonalizationResponse(
                                     responseInfo(serial, e),
                                     responseInt(data, length).value_or(-1));
                         });
    return ok();
}

ScopedAStatus RadioNetwork::setUsageSetting(int32_t serial, network::UsageSetting usageSetting) {
    LOG_CALL << ' ' << toString(usageSetting);
    mSlot.requestInts(
            SEC_REQUEST_SET_USAGE_SETTING, {static_cast<int>(usageSetting)},
            statusResponse(serial, &network::IRadioNetworkResponse::setUsageSettingResponse));
    return ok();
}

ScopedAStatus RadioNetwork::getUsageSetting(int32_t serial) {
    LOG_CALL;
    mSlot.request(SEC_REQUEST_GET_USAGE_SETTING,
                  intResponse<network::UsageSetting>(
                          serial, &network::IRadioNetworkResponse::getUsageSettingResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setEmergencyMode(int32_t serial, network::EmergencyMode emcModeType) {
    LOG_CALL << ' ' << toString(emcModeType);
    mSlot.requestInts(SEC_REQUEST_SET_EMERGENCY_MODE, {static_cast<int>(emcModeType)},
                      [this, serial](auto e, auto data, auto length) {
                          const auto* result = responseAs<SecEmergencyRegResult>(data, length);
                          respond()->setEmergencyModeResponse(
                                  responseInfo(serial, e, result != nullptr),
                                  result != nullptr ? toEmergencyRegResult(*result)
                                                    : network::EmergencyRegResult{});
                      });
    return ok();
}

ScopedAStatus RadioNetwork::triggerEmergencyNetworkScan(
        int32_t serial, const network::EmergencyNetworkScanTrigger& request) {
    LOG_CALL << ' ' << request.accessNetwork.size();
    SecEmergencyScanTrigger trigger = {
            .numRadioAccessNetworks = static_cast<int>(
                    std::min<size_t>(request.accessNetwork.size(), kSecMaxRadioAccessNetworks)),
            .scanType = static_cast<int>(request.scanType),
    };
    for (int i = 0; i < trigger.numRadioAccessNetworks; i++) {
        trigger.radioAccessNetworks[i] = toRilRadioAccessNetwork(request.accessNetwork[i]);
    }
    mSlot.request(
            SEC_REQUEST_TRIGGER_EMERGENCY_NETWORK_SCAN, &trigger, sizeof(trigger),
            statusResponse(serial,
                           &network::IRadioNetworkResponse::triggerEmergencyNetworkScanResponse));
    return ok();
}

ScopedAStatus RadioNetwork::cancelEmergencyNetworkScan(int32_t serial, bool resetScan) {
    LOG_CALL << ' ' << resetScan;
    mSlot.requestInts(
            SEC_REQUEST_CANCEL_EMERGENCY_NETWORK_SCAN, {resetScan},
            statusResponse(serial,
                           &network::IRadioNetworkResponse::cancelEmergencyNetworkScanResponse));
    return ok();
}

ScopedAStatus RadioNetwork::exitEmergencyMode(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            SEC_REQUEST_EXIT_EMERGENCY_MODE,
            statusResponse(serial, &network::IRadioNetworkResponse::exitEmergencyModeResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setNullCipherAndIntegrityEnabled(int32_t serial, bool enabled) {
    LOG_CALL << ' ' << enabled;
    mSlot.requestInts(
            SEC_REQUEST_SET_NULL_CIPHER_AND_INTEGRITY_ENABLED, {enabled},
            statusResponse(
                    serial,
                    &network::IRadioNetworkResponse::setNullCipherAndIntegrityEnabledResponse));
    return ok();
}

ScopedAStatus RadioNetwork::isNullCipherAndIntegrityEnabled(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            SEC_REQUEST_IS_NULL_CIPHER_AND_INTEGRITY_ENABLED,
            intResponse<bool>(
                    serial,
                    &network::IRadioNetworkResponse::isNullCipherAndIntegrityEnabledResponse));
    return ok();
}

ScopedAStatus RadioNetwork::isN1ModeEnabled(int32_t serial) {
    LOG_CALL;
    mSlot.request(
            SEC_REQUEST_IS_N1_MODE_ENABLED,
            intResponse<bool>(serial, &network::IRadioNetworkResponse::isN1ModeEnabledResponse));
    return ok();
}

ScopedAStatus RadioNetwork::setN1ModeEnabled(int32_t serial, bool enable) {
    LOG_CALL << ' ' << enable;
    mSlot.requestInts(
            SEC_REQUEST_SET_N1_MODE_ENABLED, {enable},
            statusResponse(serial, &network::IRadioNetworkResponse::setN1ModeEnabledResponse));
    return ok();
}

ScopedAStatus RadioNetwork::isCellularIdentifierTransparencyEnabled(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->isCellularIdentifierTransparencyEnabledResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioNetwork::setCellularIdentifierTransparencyEnabled(int32_t serial, bool enabled) {
    LOG_NOT_SUPPORTED;
    respond()->setCellularIdentifierTransparencyEnabledResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioNetwork::setSecurityAlgorithmsUpdatedEnabled(int32_t serial, bool enable) {
    LOG_NOT_SUPPORTED;
    respond()->setSecurityAlgorithmsUpdatedEnabledResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioNetwork::isSecurityAlgorithmsUpdatedEnabled(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->isSecurityAlgorithmsUpdatedEnabledResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioNetwork::setSatellitePlmn(
        int32_t serial, const std::vector<std::string>& carrierPlmnArray,
        const std::vector<std::string>& allSatellitePlmnArray) {
    LOG_NOT_SUPPORTED;
    respond()->setSatellitePlmnResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioNetwork::setSatelliteEnabledForCarrier(int32_t serial, bool satelliteEnabled) {
    LOG_NOT_SUPPORTED;
    respond()->setSatelliteEnabledForCarrierResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioNetwork::isSatelliteEnabledForCarrier(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->isSatelliteEnabledForCarrierResponse(notSupported(serial), {});
    return ok();
}

ScopedAStatus RadioNetwork::setSatelliteNetworkInfo(
        int32_t serial, const network::SatelliteNetworkInfo& satelliteNetworkInfo) {
    LOG_NOT_SUPPORTED;
    respond()->setSatelliteNetworkInfoResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioNetwork::enablePrioritizedNetworkScan(
        int32_t serial, const network::PrioritizedNetworkScanRequest& scanRequest) {
    LOG_NOT_SUPPORTED;
    respond()->enablePrioritizedNetworkScanResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioNetwork::disablePrioritizedNetworkScan(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->disablePrioritizedNetworkScanResponse(notSupported(serial));
    return ok();
}

ScopedAStatus RadioNetwork::getSupportedNetworkAlertCategories(int32_t serial) {
    LOG_NOT_SUPPORTED;
    respond()->getSupportedNetworkAlertCategoriesResponse(notSupported(serial), {});
    return ok();
}

bool RadioNetwork::onUnsolicited(int id, const void* data, size_t length,
                                 RadioIndicationType type) {
    if (id == RIL_UNSOL_NITZ_TIME_RECEIVED) {
        std::lock_guard lock(mNitzMutex);
        mLastNitz = Nitz{toString(static_cast<const char*>(data)), elapsedRealtimeMs()};
    }

    const auto indication = indicate();
    if (indication == nullptr) {
        return false;
    }
    switch (id) {
        case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED:
            indication->networkStateChanged(type);
            return true;
        case RIL_UNSOL_NITZ_TIME_RECEIVED: {
            std::lock_guard lock(mNitzMutex);
            indication->nitzTimeReceived(type, mLastNitz->time, mLastNitz->receivedTimeMs, 0);
            return true;
        }
        case RIL_UNSOL_SIGNAL_STRENGTH:
            if (const auto* signal = responseAs<SecSignalStrength>(data, length)) {
                indication->currentSignalStrength(type, toSignalStrength(*signal));
            }
            return true;
        case RIL_UNSOL_RESTRICTED_STATE_CHANGED:
            if (const auto state = responseInt(data, length)) {
                indication->restrictedStateChanged(
                        type, static_cast<network::PhoneRestrictedState>(*state));
            }
            return true;
        case RIL_UNSOL_VOICE_RADIO_TECH_CHANGED:
            if (const auto rat = responseInt(data, length)) {
                indication->voiceRadioTechChanged(type, toRadioTechnology(*rat));
            }
            return true;
        case RIL_UNSOL_CELL_INFO_LIST:
            if (const auto cells = toCellInfoList(data, length)) {
                indication->cellInfoList(type, *cells);
            }
            return true;
        case RIL_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED:
            indication->imsNetworkStateChanged(type);
            return true;
        case RIL_UNSOL_LCEDATA_RECV:
            if (const auto* lce = responseAs<SecLinkCapacityEstimate>(data, length)) {
                indication->currentLinkCapacityEstimate(type, toLinkCapacityEstimate(*lce));
            }
            return true;
        case RIL_UNSOL_NETWORK_SCAN_RESULT:
            if (const auto* result = responseAs<SecNetworkScanResult>(data, length)) {
                indication->networkScanResult(
                        type, {
                                      .status = result->status,
                                      .error = static_cast<RadioError>(result->error),
                                      .networkInfos = result->cells != nullptr
                                                              ? toCellInfoList(result->cells,
                                                                               result->numCells)
                                                              : std::vector<network::CellInfo>{},
                              });
            }
            return true;
        case SEC_UNSOL_PHYSICAL_CHANNEL_CONFIG:
            if (const auto configs = toPhysicalChannelConfigs(data, length)) {
                indication->currentPhysicalChannelConfigs(type, *configs);
            }
            return true;
        case SEC_UNSOL_REGISTRATION_FAILED:
            if (const auto* failure = responseAs<SecRegistrationFailed>(data, length)) {
                indication->registrationFailed(type, toCellIdentity(failure->cellIdentity),
                                               fixedString(failure->chosenPlmn), failure->domain,
                                               failure->causeCode, failure->additionalCauseCode);
            }
            return true;
        case SEC_UNSOL_BARRING_INFO_CHANGED:
            if (data != nullptr && length >= sizeof(SecBarringInfo)) {
                const auto* info = static_cast<const SecBarringInfo*>(data);
                indication->barringInfoChanged(type, toCellIdentity(info->cellIdentity),
                                               toBarringInfos(*info));
            }
            return true;
        case SEC_UNSOL_EMERGENCY_NETWORK_SCAN_RESULT:
            if (const auto* result = responseAs<SecEmergencyRegResult>(data, length)) {
                indication->emergencyNetworkScanResult(type, toEmergencyRegResult(*result));
            }
            return true;
        default:
            LOG(DEBUG) << "Unhandled unsolicited " << id << " (" << length << " bytes)";
    }
    return false;
}

}  // namespace aidl::android::hardware::radio::samsung
