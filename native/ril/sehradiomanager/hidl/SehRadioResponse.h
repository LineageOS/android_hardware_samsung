/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vendor/samsung/hardware/radio/2.2/ISehRadioResponse.h>
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
using ::android::hardware::radio::V1_0::RadioResponseInfo;

class SehRadioResponse : public ISehRadioResponse {
  public:
    Return<void> getIccCardStatusResponse() override;
    Return<void> supplyNetworkDepersonalizationResponse(const RadioResponseInfo& info) override;
    Return<void> dialResponse(const RadioResponseInfo& info) override;
    Return<void> getCurrentCallsResponse() override;
    Return<void> getImsRegistrationStateResponse() override;
    Return<void> setImsCallListResponse(const RadioResponseInfo& info) override;
    Return<void> getPreferredNetworkListResponse() override;
    Return<void> setPreferredNetworkListResponse(const RadioResponseInfo& info) override;
    Return<void> sendEncodedUssdResponse(const RadioResponseInfo& info) override;
    Return<void> getDisable2gResponse(const RadioResponseInfo& info, int32_t isDisable) override;
    Return<void> setDisable2gResponse(const RadioResponseInfo& info) override;
    Return<void> getCnapResponse(const RadioResponseInfo& info, int32_t m) override;
    Return<void> getPhonebookStorageInfoResponse() override;
    Return<void> getUsimPhonebookCapabilityResponse(
            const RadioResponseInfo& info, const hidl_vec<int32_t>& phonebookCapability) override;
    Return<void> setSimOnOffResponse(const RadioResponseInfo& info) override;
    Return<void> setSimInitEventResponse(const RadioResponseInfo& info) override;
    Return<void> getSimLockInfoResponse() override;
    Return<void> supplyIccPersonalizationResponse(const RadioResponseInfo& info) override;
    Return<void> changeIccPersonalizationResponse(const RadioResponseInfo& info) override;
    Return<void> getPhonebookEntryResponse() override;
    Return<void> accessPhonebookEntryResponse(const RadioResponseInfo& info,
                                              int32_t simPhonebookAccessResp) override;
    Return<void> getCellBroadcastConfigResponse() override;
    Return<void> emergencySearchResponse(const RadioResponseInfo& info,
                                         int32_t respEmergencySearch) override;
    Return<void> emergencyControlResponse(const RadioResponseInfo& info) override;
    Return<void> getAtrResponse(const RadioResponseInfo& info, const hidl_string& atr) override;
    Return<void> sendCdmaSmsExpectMoreResponse() override;
    Return<void> sendSmsResponse() override;
    Return<void> sendSMSExpectMoreResponse() override;
    Return<void> sendCdmaSmsResponse() override;
    Return<void> sendImsSmsResponse() override;
    Return<void> getStoredMsgCountFromSimResponse() override;
    Return<void> readSmsFromSimResponse() override;
    Return<void> writeSmsToSimResponse(const RadioResponseInfo& info, int32_t index) override;
    Return<void> setDataAllowedResponse(const RadioResponseInfo& info) override;
    Return<void> getCsgListResponse() override;
    Return<void> selectCsgManualResponse(const RadioResponseInfo& info) override;
    Return<void> setMobileDataSettingResponse(const RadioResponseInfo& info) override;
    Return<void> sendRequestRawResponse(const RadioResponseInfo& info,
                                        const hidl_vec<int8_t>& data) override;
    Return<void> sendRequestStringsResponse(const RadioResponseInfo& info,
                                            const hidl_vec<hidl_string>& data) override;
    Return<void> setNrModeResponse(const RadioResponseInfo& info) override;
    Return<void> getNrModeResponse(const RadioResponseInfo& info, uint32_t nrMode) override;
    Return<void> getNrIconResponse(const RadioResponseInfo& info, uint32_t nrIconType) override;
    Return<void> getIccCardStatusResponse_2_1(const RadioResponseInfo& info,
                                              const V2_1::SehCardStatus& cardStatus) override;
    Return<void> setNrModeResponse_2_2(const RadioResponseInfo& info) override;
    Return<void> getVendorSpecificConfigurationResponse(
            const RadioResponseInfo& info,
            const hidl_vec<SehVendorConfiguration>& configurations) override;
    Return<void> setVendorSpecificConfigurationResponse(const RadioResponseInfo& info) override;
};

}  // namespace implementation
}  // namespace V2_2
}  // namespace radio
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
