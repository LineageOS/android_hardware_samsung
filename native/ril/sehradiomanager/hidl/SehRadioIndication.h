/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vendor/samsung/hardware/radio/2.2/ISehRadioIndication.h>
#include <vendor/samsung/hardware/radio/2.2/types.h>

namespace vendor {
namespace samsung {
namespace hardware {
namespace radio {
namespace V2_2 {
namespace implementation {

using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;

class SehRadioIndication : public ISehRadioIndication {
  public:
    Return<void> acbInfoChanged(int32_t type, const hidl_vec<int32_t>& acbInfo) override;
    Return<void> csFallback(int32_t type, int32_t state) override;
    Return<void> imsPreferenceChanged(int32_t type, const hidl_vec<int32_t>& imsPref) override;
    Return<void> voiceRadioBearerHandoverStatusChanged(int32_t type, int32_t state) override;
    Return<void> timerStatusChangedInd(int32_t type, const hidl_vec<int32_t>& eventNoti) override;
    Return<void> modemCapabilityIndication(int32_t type, const hidl_vec<int8_t>& data) override;
    Return<void> needTurnOnRadioIndication(int32_t type) override;
    Return<void> simPhonebookReadyIndication(int32_t type) override;
    Return<void> phonebookInitCompleteIndication(int32_t type) override;
    Return<void> deviceReadyNoti(int32_t type) override;
    Return<void> stkSmsSendResultIndication(int32_t type, int32_t result) override;
    Return<void> stkCallControlResultIndication(int32_t type, const hidl_string& cmd) override;
    Return<void> simSwapStateChangedIndication(int32_t type, int32_t state) override;
    Return<void> simCountMismatchedIndication(int32_t type, int32_t state) override;
    Return<void> simOnOffStateChangedNotify(int32_t type, int32_t mode) override;
    Return<void> releaseCompleteMessageIndication(
            int32_t type, const V2_0::SehSsReleaseComplete& result) override;
    Return<void> sapNotify(int32_t type, const hidl_vec<int8_t>& data) override;
    Return<void> nrBearerAllocationChanged(int32_t type, int32_t status) override;
    Return<void> nrNetworkTypeAdded(int32_t type, int32_t status) override;
    Return<void> rrcStateChanged(int32_t type, const V2_0::SehRrcStateInfo& state) override;
    Return<void> configModemCapabilityChangeNoti(
            int32_t type, const V2_0::SehConfigModemCapability& configModemCapa) override;
    Return<void> needApnProfileIndication(const hidl_string& select) override;
    Return<int32_t> needSettingValueIndication(const hidl_string& key,
                                               const hidl_string& table) override;
    Return<void> execute(int32_t type, const hidl_string& cmd) override;
    Return<void> signalLevelInfoChanged(int32_t type,
                                        const V2_0::SehSignalBar& signalBarInfo) override;
    Return<void> extendedRegistrationState(int32_t type,
                                           const V2_0::SehExtendedRegStateResult& state) override;
    Return<void> needPacketUsage(const hidl_string& iface, needPacketUsage_cb hidl_cb) override;
    Return<void> nrIconTypeChanged(uint32_t type, uint32_t nrIconType) override;
    Return<void> callDetailsChanged(uint32_t type,
                                    const hidl_vec<V2_0::SehCallDetails>& callDetails) override;
    Return<void> vendorConfigurationChanged(
            uint32_t type, const hidl_vec<SehVendorConfiguration>& configurations) override;
    Return<void> eriInfoReceived(uint32_t type, const SehEriInfo& eriInfo) override;
};

}  // namespace implementation
}  // namespace V2_2
}  // namespace radio
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
