/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/samsung/hardware/radio/network/BnSehRadioNetworkIndication.h>

namespace aidl {
namespace vendor {
namespace samsung {
namespace hardware {
namespace radio {
namespace network {
namespace implementation {

class SehRadioNetworkIndication : public BnSehRadioNetworkIndication {
  public:
    ndk::ScopedAStatus acbInfo(int32_t type, const std::vector<int32_t>& acbInfo) override;
    ndk::ScopedAStatus callDetailsChanged(int32_t type,
                                          const std::vector<SehCallDetails>& callDetails) override;
    ndk::ScopedAStatus csFallback(int32_t type, int32_t state) override;
    ndk::ScopedAStatus currentNetworkScanIsrequested(int32_t type, int8_t mode) override;
    ndk::ScopedAStatus eriInfoReceived(int32_t type, const SehEriInfo& eriInfo) override;
    ndk::ScopedAStatus execute(int32_t type, const std::string& cmd) override;
    ndk::ScopedAStatus extendedRegistrationState(int32_t type,
                                                 const SehExtendedRegStateResult& state) override;
    ndk::ScopedAStatus imsPreferenceChanged(int32_t type,
                                            const std::vector<int32_t>& imsPref) override;
    ndk::ScopedAStatus needTurnOnRadioIndication(int32_t type) override;
    ndk::ScopedAStatus nrBearerAllocationChanged(int32_t type, int32_t status) override;
    ndk::ScopedAStatus nrIconTypeChanged(int32_t type, int32_t nrIconType) override;
    ndk::ScopedAStatus nrNetworkTypeAdded(int32_t type, int32_t nrNetworkType) override;
    ndk::ScopedAStatus roamingNetworkScanIsRequested(int32_t type,
                                                     const std::vector<uint8_t>& scanData) override;
    ndk::ScopedAStatus signalLevelInfoChanged(int32_t type,
                                              const SehSignalBar& signalBarInfo) override;
    ndk::ScopedAStatus vendorConfigurationChanged(
            int32_t type, const std::vector<SehVendorConfiguration>& configurations) override;
};

}  // namespace implementation
}  // namespace network
}  // namespace radio
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
