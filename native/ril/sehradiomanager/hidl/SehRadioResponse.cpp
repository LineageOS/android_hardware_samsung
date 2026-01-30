/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SehRadioResponse"

#include "SehRadioResponse.h"

namespace vendor {
namespace samsung {
namespace hardware {
namespace radio {
namespace V2_2 {
namespace implementation {

using ::android::hardware::Void;

Return<void> SehRadioResponse::getIccCardStatusResponse() {
    return Void();
}

Return<void> SehRadioResponse::supplyNetworkDepersonalizationResponse(
        const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::dialResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::getCurrentCallsResponse() {
    return Void();
}

Return<void> SehRadioResponse::getImsRegistrationStateResponse() {
    return Void();
}

Return<void> SehRadioResponse::setImsCallListResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::getPreferredNetworkListResponse() {
    return Void();
}

Return<void> SehRadioResponse::setPreferredNetworkListResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::sendEncodedUssdResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::getDisable2gResponse(const RadioResponseInfo& info,
                                                    int32_t isDisable) {
    return Void();
}

Return<void> SehRadioResponse::setDisable2gResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::getCnapResponse(const RadioResponseInfo& info, int32_t m) {
    return Void();
}

Return<void> SehRadioResponse::getPhonebookStorageInfoResponse() {
    return Void();
}

Return<void> SehRadioResponse::getUsimPhonebookCapabilityResponse(
        const RadioResponseInfo& info, const hidl_vec<int32_t>& phonebookCapability) {
    return Void();
}

Return<void> SehRadioResponse::setSimOnOffResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::setSimInitEventResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::getSimLockInfoResponse() {
    return Void();
}

Return<void> SehRadioResponse::supplyIccPersonalizationResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::changeIccPersonalizationResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::getPhonebookEntryResponse() {
    return Void();
}

Return<void> SehRadioResponse::accessPhonebookEntryResponse(const RadioResponseInfo& info,
                                                            int32_t simPhonebookAccessResp) {
    return Void();
}

Return<void> SehRadioResponse::getCellBroadcastConfigResponse() {
    return Void();
}

Return<void> SehRadioResponse::emergencySearchResponse(const RadioResponseInfo& info,
                                                       int32_t respEmergencySearch) {
    return Void();
}

Return<void> SehRadioResponse::emergencyControlResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::getAtrResponse(const RadioResponseInfo& info,
                                              const hidl_string& atr) {
    return Void();
}

Return<void> SehRadioResponse::sendCdmaSmsExpectMoreResponse() {
    return Void();
}

Return<void> SehRadioResponse::sendSmsResponse() {
    return Void();
}

Return<void> SehRadioResponse::sendSMSExpectMoreResponse() {
    return Void();
}

Return<void> SehRadioResponse::sendCdmaSmsResponse() {
    return Void();
}

Return<void> SehRadioResponse::sendImsSmsResponse() {
    return Void();
}

Return<void> SehRadioResponse::getStoredMsgCountFromSimResponse() {
    return Void();
}

Return<void> SehRadioResponse::readSmsFromSimResponse() {
    return Void();
}

Return<void> SehRadioResponse::writeSmsToSimResponse(const RadioResponseInfo& info, int32_t index) {
    return Void();
}

Return<void> SehRadioResponse::setDataAllowedResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::getCsgListResponse() {
    return Void();
}

Return<void> SehRadioResponse::selectCsgManualResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::setMobileDataSettingResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::sendRequestRawResponse(const RadioResponseInfo& info,
                                                      const hidl_vec<int8_t>& data) {
    return Void();
}

Return<void> SehRadioResponse::sendRequestStringsResponse(const RadioResponseInfo& info,
                                                          const hidl_vec<hidl_string>& data) {
    return Void();
}

Return<void> SehRadioResponse::setNrModeResponse(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::getNrModeResponse(const RadioResponseInfo& info, uint32_t nrMode) {
    return Void();
}

Return<void> SehRadioResponse::getNrIconResponse(const RadioResponseInfo& info,
                                                 uint32_t nrIconType) {
    return Void();
}

Return<void> SehRadioResponse::getIccCardStatusResponse_2_1(const RadioResponseInfo& info,
                                                            const V2_1::SehCardStatus& cardStatus) {
    return Void();
}

Return<void> SehRadioResponse::setNrModeResponse_2_2(const RadioResponseInfo& info) {
    return Void();
}

Return<void> SehRadioResponse::getVendorSpecificConfigurationResponse(
        const RadioResponseInfo& info, const hidl_vec<SehVendorConfiguration>& configurations) {
    return Void();
}

Return<void> SehRadioResponse::setVendorSpecificConfigurationResponse(
        const RadioResponseInfo& info) {
    return Void();
}

}  // namespace implementation
}  // namespace V2_2
}  // namespace radio
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
