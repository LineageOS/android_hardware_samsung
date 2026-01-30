/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/samsung/hardware/radio/network/BnSehRadioNetworkResponse.h>

namespace aidl {
namespace vendor {
namespace samsung {
namespace hardware {
namespace radio {
namespace network {
namespace implementation {

class SehRadioNetworkResponse : public BnSehRadioNetworkResponse {
  public:
    ndk::ScopedAStatus emergencyControlResponse(const SehRadioResponseInfo& info) override;
    ndk::ScopedAStatus emergencySearchResponse(const SehRadioResponseInfo& info,
                                               int respEmergencySearch) override;
    ndk::ScopedAStatus getAvailableNetworksResponse(
            const SehRadioResponseInfo& info,
            const std::vector<SehOperatorInfo>& networksInfo) override;
    ndk::ScopedAStatus getCnapResponse(const SehRadioResponseInfo& info, int32_t m) override;
    ndk::ScopedAStatus getCsgListResponse(const SehRadioResponseInfo& info,
                                          const std::vector<SehCsgInfo>& csgInfos) override;
    ndk::ScopedAStatus getDisable2gResponse(const SehRadioResponseInfo& info,
                                            int32_t isDisable) override;
    ndk::ScopedAStatus getNrIconTypeResponse(const SehRadioResponseInfo& info,
                                             int32_t nrIconType) override;
    ndk::ScopedAStatus getNrModeResponse(const SehRadioResponseInfo& info, int32_t nrMode) override;
    ndk::ScopedAStatus getPreferredNetworkListResponse(
            const SehRadioResponseInfo& info,
            const std::vector<SehPreferredNetworkInfo>& infos) override;
    ndk::ScopedAStatus getRoamingNetworkInfoViaBLEResponse(
            const SehRadioResponseInfo& info,
            const std::vector<SehRoamingNetworkInfo>& networkInfo) override;
    ndk::ScopedAStatus getVendorSpecificConfigurationResponse(
            const SehRadioResponseInfo& info,
            const SehVendorConfiguration& configurations) override;
    ndk::ScopedAStatus selectCsgManualResponse(const SehRadioResponseInfo& info) override;
    ndk::ScopedAStatus sendEncodedUssdResponse(const SehRadioResponseInfo& info) override;
    ndk::ScopedAStatus sendRequestRawResponse(const SehRadioResponseInfo& info,
                                              const std::vector<uint8_t>& data) override;
    ndk::ScopedAStatus sendRequestStringsResponse(const SehRadioResponseInfo& info,
                                                  const std::vector<std::string>& data) override;
    ndk::ScopedAStatus setCurrentNetworkInfoViaBLEResponse(
            const SehRadioResponseInfo& info) override;
    ndk::ScopedAStatus setDisable2gResponse(const SehRadioResponseInfo& info) override;
    ndk::ScopedAStatus setImsCallListResponse(const SehRadioResponseInfo& info) override;
    ndk::ScopedAStatus setNrModeResponse(const SehRadioResponseInfo& info) override;
    ndk::ScopedAStatus setPreferredNetworkListResponse(const SehRadioResponseInfo& info) override;
    ndk::ScopedAStatus setRoamingNetworkInfoViaBLEResponse(
            const SehRadioResponseInfo& info) override;
    ndk::ScopedAStatus setScanResultViaBLEResponse(const SehRadioResponseInfo& info) override;
    ndk::ScopedAStatus setVendorSpecificConfigurationResponse(
            const SehRadioResponseInfo& info) override;
};

}  // namespace implementation
}  // namespace network
}  // namespace radio
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
