/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SehRadioNetworkResponse.h"

namespace aidl {
namespace vendor {
namespace samsung {
namespace hardware {
namespace radio {
namespace network {
namespace implementation {

ndk::ScopedAStatus SehRadioNetworkResponse::emergencyControlResponse(
        const SehRadioResponseInfo& info) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::emergencySearchResponse(
        const SehRadioResponseInfo& info, int respEmergencySearch) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::getAvailableNetworksResponse(
        const SehRadioResponseInfo& info, const std::vector<SehOperatorInfo>& networksInfo) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::getCnapResponse(const SehRadioResponseInfo& info,
                                                            int32_t m) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::getCsgListResponse(
        const SehRadioResponseInfo& info, const std::vector<SehCsgInfo>& csgInfos) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::getDisable2gResponse(const SehRadioResponseInfo& info,
                                                                 int32_t isDisable) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::getNrIconTypeResponse(const SehRadioResponseInfo& info,
                                                                  int32_t nrIconType) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::getNrModeResponse(const SehRadioResponseInfo& info,
                                                              int32_t nrMode) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::getPreferredNetworkListResponse(
        const SehRadioResponseInfo& info, const std::vector<SehPreferredNetworkInfo>& infos) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::getRoamingNetworkInfoViaBLEResponse(
        const SehRadioResponseInfo& info, const std::vector<SehRoamingNetworkInfo>& networkInfo) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::getVendorSpecificConfigurationResponse(
        const SehRadioResponseInfo& info, const SehVendorConfiguration& configurations) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::selectCsgManualResponse(
        const SehRadioResponseInfo& info) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::sendEncodedUssdResponse(
        const SehRadioResponseInfo& info) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::sendRequestRawResponse(
        const SehRadioResponseInfo& info, const std::vector<uint8_t>& data) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::sendRequestStringsResponse(
        const SehRadioResponseInfo& info, const std::vector<std::string>& data) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::setCurrentNetworkInfoViaBLEResponse(
        const SehRadioResponseInfo& info) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::setDisable2gResponse(const SehRadioResponseInfo& info) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::setImsCallListResponse(
        const SehRadioResponseInfo& info) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::setNrModeResponse(const SehRadioResponseInfo& info) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::setPreferredNetworkListResponse(
        const SehRadioResponseInfo& info) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::setRoamingNetworkInfoViaBLEResponse(
        const SehRadioResponseInfo& info) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::setScanResultViaBLEResponse(
        const SehRadioResponseInfo& info) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkResponse::setVendorSpecificConfigurationResponse(
        const SehRadioResponseInfo& info) {
    return ndk::ScopedAStatus::ok();
}

}  // namespace implementation
}  // namespace network
}  // namespace radio
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
