/*
 * Copyright (C) 2019, The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.1
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <vendor/samsung/hardware/radio/1.2/IRadioResponse.h>

namespace vendor {
namespace samsung {
namespace hardware {
namespace radio {
namespace V1_2 {
namespace implementation {

using ::android::sp;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;

struct SecRadioResponse : public IRadioResponse {
    int simSlot;
    sp<::android::hardware::radio::V1_2::IRadioResponse> radioResponse;

    SecRadioResponse(int simSlot,
                     const sp<::android::hardware::radio::V1_2::IRadioResponse>& radioResponse);

    // Methods from ::android::hardware::radio::V1_0::IRadioResponse follow.
    Return<void> getIccCardStatusResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::CardStatus& cardStatus) override;
    Return<void> supplyIccPinForAppResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t remainingRetries) override;
    Return<void> supplyIccPukForAppResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t remainingRetries) override;
    Return<void> supplyIccPin2ForAppResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t remainingRetries) override;
    Return<void> supplyIccPuk2ForAppResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t remainingRetries) override;
    Return<void> changeIccPinForAppResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t remainingRetries) override;
    Return<void> changeIccPin2ForAppResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t remainingRetries) override;
    Return<void> supplyNetworkDepersonalizationResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t remainingRetries) override;
    Return<void> getCurrentCallsResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_0::Call>& calls) override;
    Return<void> dialResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getIMSIForAppResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_string& imsi) override;
    Return<void> hangupConnectionResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> hangupWaitingOrBackgroundResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> hangupForegroundResumeBackgroundResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> switchWaitingOrHoldingAndActiveResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> conferenceResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> rejectCallResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getLastCallFailCauseResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::LastCallFailCauseInfo& failCauseinfo) override;
    Return<void> getSignalStrengthResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::SignalStrength& sigStrength) override;
    Return<void> getVoiceRegistrationStateResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::VoiceRegStateResult& voiceRegResponse) override;
    Return<void> getDataRegistrationStateResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::DataRegStateResult& dataRegResponse) override;
    Return<void> getOperatorResponse(const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
                                     const hidl_string& longName, const hidl_string& shortName,
                                     const hidl_string& numeric) override;
    Return<void> setRadioPowerResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> sendDtmfResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> sendSmsResponse(const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
                                 const ::android::hardware::radio::V1_0::SendSmsResult& sms) override;
    Return<void> sendSMSExpectMoreResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::SendSmsResult& sms) override;
    Return<void> setupDataCallResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::SetupDataCallResult& dcResponse) override;
    Return<void> iccIOForAppResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::IccIoResult& iccIo) override;
    Return<void> sendUssdResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> cancelPendingUssdResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getClirResponse(const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
                                 int32_t n, int32_t m) override;
    Return<void> setClirResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getCallForwardStatusResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_0::CallForwardInfo>& callForwardInfos) override;
    Return<void> setCallForwardResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getCallWaitingResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, bool enable,
        int32_t serviceClass) override;
    Return<void> setCallWaitingResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> acknowledgeLastIncomingGsmSmsResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> acceptCallResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> deactivateDataCallResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getFacilityLockForAppResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, int32_t response) override;
    Return<void> setFacilityLockForAppResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, int32_t retry) override;
    Return<void> setBarringPasswordResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getNetworkSelectionModeResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, bool manual) override;
    Return<void> setNetworkSelectionModeAutomaticResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setNetworkSelectionModeManualResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getAvailableNetworksResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_0::OperatorInfo>& networkInfos) override;
    Return<void> startDtmfResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> stopDtmfResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getBasebandVersionResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_string& version) override;
    Return<void> separateConnectionResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setMuteResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getMuteResponse(const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
                                 bool enable) override;
    Return<void> getClipResponse(const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
                                 ::android::hardware::radio::V1_0::ClipStatus status) override;
    Return<void> getDataCallListResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_0::SetupDataCallResult>& dcResponse) override;
    Return<void> setSuppServiceNotificationsResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> writeSmsToSimResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, int32_t index) override;
    Return<void> deleteSmsOnSimResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setBandModeResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getAvailableBandModesResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_0::RadioBandMode>& bandModes) override;
    Return<void> sendEnvelopeResponse(const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
                                      const hidl_string& commandResponse) override;
    Return<void> sendTerminalResponseToSimResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> handleStkCallSetupRequestFromSimResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> explicitCallTransferResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setPreferredNetworkTypeResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getPreferredNetworkTypeResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        ::android::hardware::radio::V1_0::PreferredNetworkType nwType) override;
    Return<void> getNeighboringCidsResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_0::NeighboringCell>& cells) override;
    Return<void> setLocationUpdatesResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setCdmaSubscriptionSourceResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setCdmaRoamingPreferenceResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getCdmaRoamingPreferenceResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        ::android::hardware::radio::V1_0::CdmaRoamingType type) override;
    Return<void> setTTYModeResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getTTYModeResponse(const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
                                    ::android::hardware::radio::V1_0::TtyMode mode) override;
    Return<void> setPreferredVoicePrivacyResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getPreferredVoicePrivacyResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, bool enable) override;
    Return<void> sendCDMAFeatureCodeResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> sendBurstDtmfResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> sendCdmaSmsResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::SendSmsResult& sms) override;
    Return<void> acknowledgeLastIncomingCdmaSmsResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getGsmBroadcastConfigResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_0::GsmBroadcastSmsConfigInfo>& configs) override;
    Return<void> setGsmBroadcastConfigResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setGsmBroadcastActivationResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getCdmaBroadcastConfigResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_0::CdmaBroadcastSmsConfigInfo>& configs)
        override;
    Return<void> setCdmaBroadcastConfigResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setCdmaBroadcastActivationResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getCDMASubscriptionResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, const hidl_string& mdn,
        const hidl_string& hSid, const hidl_string& hNid, const hidl_string& min,
        const hidl_string& prl) override;
    Return<void> writeSmsToRuimResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, uint32_t index) override;
    Return<void> deleteSmsOnRuimResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getDeviceIdentityResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, const hidl_string& imei,
        const hidl_string& imeisv, const hidl_string& esn, const hidl_string& meid) override;
    Return<void> exitEmergencyCallbackModeResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getSmscAddressResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_string& smsc) override;
    Return<void> setSmscAddressResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> reportSmsMemoryStatusResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> reportStkServiceIsRunningResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getCdmaSubscriptionSourceResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        ::android::hardware::radio::V1_0::CdmaSubscriptionSource source) override;
    Return<void> requestIsimAuthenticationResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_string& response) override;
    Return<void> acknowledgeIncomingGsmSmsWithPduResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> sendEnvelopeWithStatusResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::IccIoResult& iccIo) override;
    Return<void> getVoiceRadioTechnologyResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        ::android::hardware::radio::V1_0::RadioTechnology rat) override;
    Return<void> getCellInfoListResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_0::CellInfo>& cellInfo) override;
    Return<void> setCellInfoListRateResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setInitialAttachApnResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getImsRegistrationStateResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, bool isRegistered,
        ::android::hardware::radio::V1_0::RadioTechnologyFamily ratFamily) override;
    Return<void> sendImsSmsResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::SendSmsResult& sms) override;
    Return<void> iccTransmitApduBasicChannelResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::IccIoResult& result) override;
    Return<void> iccOpenLogicalChannelResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, int32_t channelId,
        const hidl_vec<int8_t>& selectResponse) override;
    Return<void> iccCloseLogicalChannelResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> iccTransmitApduLogicalChannelResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::IccIoResult& result) override;
    Return<void> nvReadItemResponse(const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
                                    const hidl_string& result) override;
    Return<void> nvWriteItemResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> nvWriteCdmaPrlResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> nvResetConfigResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setUiccSubscriptionResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setDataAllowedResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getHardwareConfigResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_0::HardwareConfig>& config) override;
    Return<void> requestIccSimAuthenticationResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::IccIoResult& result) override;
    Return<void> setDataProfileResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> requestShutdownResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getRadioCapabilityResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::RadioCapability& rc) override;
    Return<void> setRadioCapabilityResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::RadioCapability& rc) override;
    Return<void> startLceServiceResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::LceStatusInfo& statusInfo) override;
    Return<void> stopLceServiceResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::LceStatusInfo& statusInfo) override;
    Return<void> pullLceDataResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::LceDataInfo& lceInfo) override;
    Return<void> getModemActivityInfoResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::ActivityStatsInfo& activityInfo) override;
    Return<void> setAllowedCarriersResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t numAllowed) override;
    Return<void> getAllowedCarriersResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, bool allAllowed,
        const ::android::hardware::radio::V1_0::CarrierRestrictions& carriers) override;
    Return<void> sendDeviceStateResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setIndicationFilterResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setSimCardPowerResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> acknowledgeRequest(int32_t serial) override;

    // Methods from ::android::hardware::radio::V1_1::IRadioResponse follow.
    Return<void> setCarrierInfoForImsiEncryptionResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setSimCardPowerResponse_1_1(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> startNetworkScanResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> stopNetworkScanResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> startKeepaliveResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_1::KeepaliveStatus& status) override;
    Return<void> stopKeepaliveResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;

    // Methods from ::android::hardware::radio::V1_2::IRadioResponse follow.
    Return<void> getCellInfoListResponse_1_2(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_2::CellInfo>& cellInfo) override;
    Return<void> getIccCardStatusResponse_1_2(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_2::CardStatus& cardStatus) override;
    Return<void> setSignalStrengthReportingCriteriaResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> setLinkCapacityReportingCriteriaResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> getCurrentCallsResponse_1_2(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::android::hardware::radio::V1_2::Call>& calls) override;
    Return<void> getSignalStrengthResponse_1_2(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_2::SignalStrength& signalStrength) override;
    Return<void> getVoiceRegistrationStateResponse_1_2(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_2::VoiceRegStateResult& voiceRegResponse) override;
    Return<void> getDataRegistrationStateResponse_1_2(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_2::DataRegStateResult& dataRegResponse) override;

    // Methods from ::vendor::samsung::hardware::radio::V1_2::IRadioResponse follow.
    Return<void> secGetIccCardStatusReponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::SecCardStatus& cardStatus) override;
    Return<void> secSupplyNetworkDepersonalizationResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t remainingRetries) override;
    Return<void> secAcceptCallResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> secDialResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> secGetCurrentCallsResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::vendor::samsung::hardware::radio::V1_2::SecCall>& calls) override;
    Return<void> secGetSignalStrengthResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::SecSignalStrength& sigStrength) override;
    Return<void> secGetVoiceRegistrationStateResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::SecVoiceRegStateResult& voiceRegResponse)
        override;
    Return<void> secGetDataRegistrationStateResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::SecDataRegStateResult& dataRegResponse)
        override;
    Return<void> secExplicitCallTransferResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> secGetOperatorRespnse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_string& longName, const hidl_string& shortName, const hidl_string& plmn,
        const hidl_string& epdgname) override;
    Return<void> oemSetBarringPasswordResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> secgetImsRegistrationStateReponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<int32_t>& regState) override;
    Return<void> secGetAvailableNetworkResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::vendor::samsung::hardware::radio::V1_2::SecOperatorInfo>& networkInfo)
        override;
    Return<void> oemDialEmergencyCallResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemCallDeflectionResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemModifyCallInitiateResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::android::hardware::radio::V1_0::LastCallFailCauseInfo& failCauseInfo) override;
    Return<void> oemSetImsCallListResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemGetPreferredNetworkListResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::vendor::samsung::hardware::radio::V1_2::OemPreferredNetworkInfo>& infos)
        override;
    Return<void> oemSetPreferredNetworkListResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemSendEncodedUSSDResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemHoldCallResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemGetDisable2gResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, int32_t isDisable) override;
    Return<void> oemSetDisable2gResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oenGetAcbInfoResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<int32_t>& acbInfo) override;
    Return<void> oemSetTransferCallResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemGetICBarringResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_string& numberDateList) override;
    Return<void> oemSetICBarringResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemQueryCnapResponse(const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
                                      int32_t queryCNAP) override;
    Return<void> oemRefreshNitzTimeResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemEnableUnsolResponseResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemCancelTransferCallResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemAcknowledgeRilConnectedResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemGetPhoneBookStorageInfoResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<int32_t>& phoneBookInfo) override;
    Return<void> oemUsimPbCapaResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<int32_t>& usimPbCapa) override;
    Return<void> oemSetSimPowerResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, int32_t power) override;
    Return<void> oemSetSimOnOffResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemSetSimInitEventResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemGetSimLockInfoResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<int32_t>& simLockInfO) override;
    Return<void> oemSupplyIccPersoResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemChangeIccPersoResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemGetPhoneBookEntryResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::OemSimPBResponse& SimPBResponseInfo) override;
    Return<void> oemAccessPhoneBookEntryResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t SimPbAccessResp) override;
    Return<void> oemGetCellBroadcastConfigResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::OemCbConfigArgs& configs) override;
    Return<void> oemEmergencySearchResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t respEmergencySearch) override;
    Return<void> oemEmergencyControlResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> oemGetAtrResponse(const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
                                   const hidl_string& atr) override;
    Return<void> oemSendCdmaSmsExpectMoreResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::SecSendSmsResult& sms) override;
    Return<void> secSendSmsResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::SecSendSmsResult& sms) override;
    Return<void> secSendSMSExpectMoreResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::SecSendSmsResult& sms) override;
    Return<void> secSendCdmaSmsResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::SecSendSmsResult& sms) override;
    Return<void> secSendImsSmsResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::SecSendSmsResult& sms) override;
    Return<void> secSetDataAllowedResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> secGetCdmaRoamingPreferenceResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info, int32_t n,
        int32_t m) override;
    Return<void> secEnable5gResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> secDisable5gResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> secQuery5gStatusResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
    Return<void> secQueryNrDcParamResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::DcParam& endcDcnr) override;
    Return<void> secQueryNrBearerAllocationResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        int32_t bearerStatus) override;
    Return<void> secQueryNrSignalStrengthResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const ::vendor::samsung::hardware::radio::V1_2::NrSignalStrength& nrSignalStrength) override;
    Return<void> oemQueryCsgListResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info,
        const hidl_vec<::vendor::samsung::hardware::radio::V1_2::OemCsgInfo>& csgInfos) override;
    Return<void> oemSelectCsgManualResponse(
        const ::android::hardware::radio::V1_0::RadioResponseInfo& info) override;
};

}  // namespace implementation
}  // namespace V1_2
}  // namespace radio
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
