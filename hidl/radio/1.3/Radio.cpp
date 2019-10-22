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

#include "Radio.h"

namespace android {
namespace hardware {
namespace radio {
namespace V1_3 {
namespace implementation {

Radio::Radio(const std::string& interfaceName) : interfaceName(interfaceName) {}

sp<::vendor::samsung::hardware::radio::V1_2::IRadio> Radio::getSecIRadio() {
    std::lock_guard<std::mutex> lock(secIRadioMutex);
    if (!secIRadio) {
        secIRadio = ::vendor::samsung::hardware::radio::V1_2::IRadio::getService(interfaceName);
    }
    return secIRadio;
}

// Methods from ::android::hardware::radio::V1_0::IRadio follow.
Return<void> Radio::setResponseFunctions(
    const sp<::android::hardware::radio::V1_0::IRadioResponse>& radioResponse,
    const sp<::android::hardware::radio::V1_0::IRadioIndication>& radioIndication) {
    sp<::vendor::samsung::hardware::radio::V1_2::IRadioResponse> secRadioResponse =
        new SecRadioResponse(
            interfaceName == RIL1_SERVICE_NAME ? 1 : 2,
            ::android::hardware::radio::V1_2::IRadioResponse::castFrom(radioResponse)
                .withDefault(nullptr));
    sp<::vendor::samsung::hardware::radio::V1_2::IRadioIndication> secRadioIndication =
        new SecRadioIndication(
            ::android::hardware::radio::V1_2::IRadioIndication::castFrom(radioIndication)
                .withDefault(nullptr));
    getSecIRadio()->setResponseFunctions(secRadioResponse, secRadioIndication);
    return Void();
}

Return<void> Radio::getIccCardStatus(int32_t serial) {
    getSecIRadio()->getIccCardStatus(serial);
    return Void();
}

Return<void> Radio::supplyIccPinForApp(int32_t serial, const hidl_string& pin,
                                       const hidl_string& aid) {
    getSecIRadio()->supplyIccPinForApp(serial, pin, aid);
    return Void();
}

Return<void> Radio::supplyIccPukForApp(int32_t serial, const hidl_string& puk,
                                       const hidl_string& pin, const hidl_string& aid) {
    getSecIRadio()->supplyIccPukForApp(serial, puk, pin, aid);
    return Void();
}

Return<void> Radio::supplyIccPin2ForApp(int32_t serial, const hidl_string& pin2,
                                        const hidl_string& aid) {
    getSecIRadio()->supplyIccPin2ForApp(serial, pin2, aid);
    return Void();
}

Return<void> Radio::supplyIccPuk2ForApp(int32_t serial, const hidl_string& puk2,
                                        const hidl_string& pin2, const hidl_string& aid) {
    getSecIRadio()->supplyIccPuk2ForApp(serial, puk2, pin2, aid);
    return Void();
}

Return<void> Radio::changeIccPinForApp(int32_t serial, const hidl_string& oldPin,
                                       const hidl_string& newPin, const hidl_string& aid) {
    getSecIRadio()->changeIccPinForApp(serial, oldPin, newPin, aid);
    return Void();
}

Return<void> Radio::changeIccPin2ForApp(int32_t serial, const hidl_string& oldPin2,
                                        const hidl_string& newPin2, const hidl_string& aid) {
    getSecIRadio()->changeIccPin2ForApp(serial, oldPin2, newPin2, aid);
    return Void();
}

Return<void> Radio::supplyNetworkDepersonalization(int32_t serial, const hidl_string& netPin) {
    getSecIRadio()->supplyNetworkDepersonalization(serial, netPin);
    return Void();
}

Return<void> Radio::getCurrentCalls(int32_t serial) {
    getSecIRadio()->getCurrentCalls(serial);
    return Void();
}

Return<void> Radio::dial(int32_t serial, const ::android::hardware::radio::V1_0::Dial& dialInfo) {
    getSecIRadio()->dial(serial, dialInfo);
    return Void();
}

Return<void> Radio::getImsiForApp(int32_t serial, const hidl_string& aid) {
    getSecIRadio()->getImsiForApp(serial, aid);
    return Void();
}

Return<void> Radio::hangup(int32_t serial, int32_t gsmIndex) {
    getSecIRadio()->hangup(serial, gsmIndex);
    return Void();
}

Return<void> Radio::hangupWaitingOrBackground(int32_t serial) {
    getSecIRadio()->hangupWaitingOrBackground(serial);
    return Void();
}

Return<void> Radio::hangupForegroundResumeBackground(int32_t serial) {
    getSecIRadio()->hangupForegroundResumeBackground(serial);
    return Void();
}

Return<void> Radio::switchWaitingOrHoldingAndActive(int32_t serial) {
    getSecIRadio()->switchWaitingOrHoldingAndActive(serial);
    return Void();
}

Return<void> Radio::conference(int32_t serial) {
    getSecIRadio()->conference(serial);
    return Void();
}

Return<void> Radio::rejectCall(int32_t serial) {
    getSecIRadio()->rejectCall(serial);
    return Void();
}

Return<void> Radio::getLastCallFailCause(int32_t serial) {
    getSecIRadio()->getLastCallFailCause(serial);
    return Void();
}

Return<void> Radio::getSignalStrength(int32_t serial) {
    getSecIRadio()->getSignalStrength(serial);
    return Void();
}

Return<void> Radio::getVoiceRegistrationState(int32_t serial) {
    getSecIRadio()->getVoiceRegistrationState(serial);
    return Void();
}

Return<void> Radio::getDataRegistrationState(int32_t serial) {
    getSecIRadio()->getDataRegistrationState(serial);
    return Void();
}

Return<void> Radio::getOperator(int32_t serial) {
    getSecIRadio()->getOperator(serial);
    return Void();
}

Return<void> Radio::setRadioPower(int32_t serial, bool on) {
    getSecIRadio()->setRadioPower(serial, on);
    return Void();
}

Return<void> Radio::sendDtmf(int32_t serial, const hidl_string& s) {
    getSecIRadio()->sendDtmf(serial, s);
    return Void();
}

Return<void> Radio::sendSms(int32_t serial,
                            const ::android::hardware::radio::V1_0::GsmSmsMessage& message) {
    getSecIRadio()->sendSms(serial, message);
    return Void();
}

Return<void> Radio::sendSMSExpectMore(
    int32_t serial, const ::android::hardware::radio::V1_0::GsmSmsMessage& message) {
    getSecIRadio()->sendSMSExpectMore(serial, message);
    return Void();
}

Return<void> Radio::setupDataCall(
    int32_t serial, ::android::hardware::radio::V1_0::RadioTechnology radioTechnology,
    const ::android::hardware::radio::V1_0::DataProfileInfo& dataProfileInfo, bool modemCognitive,
    bool roamingAllowed, bool isRoaming) {
    getSecIRadio()->setupDataCall(serial, radioTechnology, dataProfileInfo, modemCognitive,
                                  roamingAllowed, isRoaming);
    return Void();
}

Return<void> Radio::iccIOForApp(int32_t serial,
                                const ::android::hardware::radio::V1_0::IccIo& iccIo) {
    getSecIRadio()->iccIOForApp(serial, iccIo);
    return Void();
}

Return<void> Radio::sendUssd(int32_t serial, const hidl_string& ussd) {
    getSecIRadio()->sendUssd(serial, ussd);
    return Void();
}

Return<void> Radio::cancelPendingUssd(int32_t serial) {
    getSecIRadio()->cancelPendingUssd(serial);
    return Void();
}

Return<void> Radio::getClir(int32_t serial) {
    getSecIRadio()->getClir(serial);
    return Void();
}

Return<void> Radio::setClir(int32_t serial, int32_t status) {
    getSecIRadio()->setClir(serial, status);
    return Void();
}

Return<void> Radio::getCallForwardStatus(
    int32_t serial, const ::android::hardware::radio::V1_0::CallForwardInfo& callInfo) {
    getSecIRadio()->getCallForwardStatus(serial, callInfo);
    return Void();
}

Return<void> Radio::setCallForward(
    int32_t serial, const ::android::hardware::radio::V1_0::CallForwardInfo& callInfo) {
    getSecIRadio()->setCallForward(serial, callInfo);
    return Void();
}

Return<void> Radio::getCallWaiting(int32_t serial, int32_t serviceClass) {
    getSecIRadio()->getCallWaiting(serial, serviceClass);
    return Void();
}

Return<void> Radio::setCallWaiting(int32_t serial, bool enable, int32_t serviceClass) {
    getSecIRadio()->setCallWaiting(serial, enable, serviceClass);
    return Void();
}

Return<void> Radio::acknowledgeLastIncomingGsmSms(
    int32_t serial, bool success, ::android::hardware::radio::V1_0::SmsAcknowledgeFailCause cause) {
    getSecIRadio()->acknowledgeLastIncomingGsmSms(serial, success, cause);
    return Void();
}

Return<void> Radio::acceptCall(int32_t serial) {
    getSecIRadio()->acceptCall(serial);
    return Void();
}

Return<void> Radio::deactivateDataCall(int32_t serial, int32_t cid, bool reasonRadioShutDown) {
    getSecIRadio()->deactivateDataCall(serial, cid, reasonRadioShutDown);
    return Void();
}

Return<void> Radio::getFacilityLockForApp(int32_t serial, const hidl_string& facility,
                                          const hidl_string& password, int32_t serviceClass,
                                          const hidl_string& appId) {
    getSecIRadio()->getFacilityLockForApp(serial, facility, password, serviceClass, appId);
    return Void();
}

Return<void> Radio::setFacilityLockForApp(int32_t serial, const hidl_string& facility,
                                          bool lockState, const hidl_string& password,
                                          int32_t serviceClass, const hidl_string& appId) {
    getSecIRadio()->setFacilityLockForApp(serial, facility, lockState, password, serviceClass,
                                          appId);
    return Void();
}

Return<void> Radio::setBarringPassword(int32_t serial, const hidl_string& facility,
                                       const hidl_string& oldPassword,
                                       const hidl_string& newPassword) {
    getSecIRadio()->setBarringPassword(serial, facility, oldPassword, newPassword);
    return Void();
}

Return<void> Radio::getNetworkSelectionMode(int32_t serial) {
    getSecIRadio()->getNetworkSelectionMode(serial);
    return Void();
}

Return<void> Radio::setNetworkSelectionModeAutomatic(int32_t serial) {
    getSecIRadio()->setNetworkSelectionModeAutomatic(serial);
    return Void();
}

Return<void> Radio::setNetworkSelectionModeManual(int32_t serial,
                                                  const hidl_string& operatorNumeric) {
    getSecIRadio()->setNetworkSelectionModeManual(serial, operatorNumeric);
    return Void();
}

Return<void> Radio::getAvailableNetworks(int32_t serial) {
    getSecIRadio()->getAvailableNetworks(serial);
    return Void();
}

Return<void> Radio::startDtmf(int32_t serial, const hidl_string& s) {
    getSecIRadio()->startDtmf(serial, s);
    return Void();
}

Return<void> Radio::stopDtmf(int32_t serial) {
    getSecIRadio()->stopDtmf(serial);
    return Void();
}

Return<void> Radio::getBasebandVersion(int32_t serial) {
    getSecIRadio()->getBasebandVersion(serial);
    return Void();
}

Return<void> Radio::separateConnection(int32_t serial, int32_t gsmIndex) {
    getSecIRadio()->separateConnection(serial, gsmIndex);
    return Void();
}

Return<void> Radio::setMute(int32_t serial, bool enable) {
    getSecIRadio()->setMute(serial, enable);
    return Void();
}

Return<void> Radio::getMute(int32_t serial) {
    getSecIRadio()->getMute(serial);
    return Void();
}

Return<void> Radio::getClip(int32_t serial) {
    getSecIRadio()->getClip(serial);
    return Void();
}

Return<void> Radio::getDataCallList(int32_t serial) {
    getSecIRadio()->getDataCallList(serial);
    return Void();
}

Return<void> Radio::setSuppServiceNotifications(int32_t serial, bool enable) {
    getSecIRadio()->setSuppServiceNotifications(serial, enable);
    return Void();
}

Return<void> Radio::writeSmsToSim(
    int32_t serial, const ::android::hardware::radio::V1_0::SmsWriteArgs& smsWriteArgs) {
    getSecIRadio()->writeSmsToSim(serial, smsWriteArgs);
    return Void();
}

Return<void> Radio::deleteSmsOnSim(int32_t serial, int32_t index) {
    getSecIRadio()->deleteSmsOnSim(serial, index);
    return Void();
}

Return<void> Radio::setBandMode(int32_t serial,
                                ::android::hardware::radio::V1_0::RadioBandMode mode) {
    getSecIRadio()->setBandMode(serial, mode);
    return Void();
}

Return<void> Radio::getAvailableBandModes(int32_t serial) {
    getSecIRadio()->getAvailableBandModes(serial);
    return Void();
}

Return<void> Radio::sendEnvelope(int32_t serial, const hidl_string& command) {
    getSecIRadio()->sendEnvelope(serial, command);
    return Void();
}

Return<void> Radio::sendTerminalResponseToSim(int32_t serial, const hidl_string& commandResponse) {
    getSecIRadio()->sendTerminalResponseToSim(serial, commandResponse);
    return Void();
}

Return<void> Radio::handleStkCallSetupRequestFromSim(int32_t serial, bool accept) {
    getSecIRadio()->handleStkCallSetupRequestFromSim(serial, accept);
    return Void();
}

Return<void> Radio::explicitCallTransfer(int32_t serial) {
    getSecIRadio()->explicitCallTransfer(serial);
    return Void();
}

Return<void> Radio::setPreferredNetworkType(
    int32_t serial, ::android::hardware::radio::V1_0::PreferredNetworkType nwType) {
    getSecIRadio()->setPreferredNetworkType(serial, nwType);
    return Void();
}

Return<void> Radio::getPreferredNetworkType(int32_t serial) {
    getSecIRadio()->getPreferredNetworkType(serial);
    return Void();
}

Return<void> Radio::getNeighboringCids(int32_t serial) {
    getSecIRadio()->getNeighboringCids(serial);
    return Void();
}

Return<void> Radio::setLocationUpdates(int32_t serial, bool enable) {
    getSecIRadio()->setLocationUpdates(serial, enable);
    return Void();
}

Return<void> Radio::setCdmaSubscriptionSource(
    int32_t serial, ::android::hardware::radio::V1_0::CdmaSubscriptionSource cdmaSub) {
    getSecIRadio()->setCdmaSubscriptionSource(serial, cdmaSub);
    return Void();
}

Return<void> Radio::setCdmaRoamingPreference(int32_t serial,
                                             ::android::hardware::radio::V1_0::CdmaRoamingType type) {
    getSecIRadio()->setCdmaRoamingPreference(serial, type);
    return Void();
}

Return<void> Radio::getCdmaRoamingPreference(int32_t serial) {
    getSecIRadio()->getCdmaRoamingPreference(serial);
    return Void();
}

Return<void> Radio::setTTYMode(int32_t serial, ::android::hardware::radio::V1_0::TtyMode mode) {
    getSecIRadio()->setTTYMode(serial, mode);
    return Void();
}

Return<void> Radio::getTTYMode(int32_t serial) {
    getSecIRadio()->getTTYMode(serial);
    return Void();
}

Return<void> Radio::setPreferredVoicePrivacy(int32_t serial, bool enable) {
    getSecIRadio()->setPreferredVoicePrivacy(serial, enable);
    return Void();
}

Return<void> Radio::getPreferredVoicePrivacy(int32_t serial) {
    getSecIRadio()->getPreferredVoicePrivacy(serial);
    return Void();
}

Return<void> Radio::sendCDMAFeatureCode(int32_t serial, const hidl_string& featureCode) {
    getSecIRadio()->sendCDMAFeatureCode(serial, featureCode);
    return Void();
}

Return<void> Radio::sendBurstDtmf(int32_t serial, const hidl_string& dtmf, int32_t on, int32_t off) {
    getSecIRadio()->sendBurstDtmf(serial, dtmf, on, off);
    return Void();
}

Return<void> Radio::sendCdmaSms(int32_t serial,
                                const ::android::hardware::radio::V1_0::CdmaSmsMessage& sms) {
    getSecIRadio()->sendCdmaSms(serial, sms);
    return Void();
}

Return<void> Radio::acknowledgeLastIncomingCdmaSms(
    int32_t serial, const ::android::hardware::radio::V1_0::CdmaSmsAck& smsAck) {
    getSecIRadio()->acknowledgeLastIncomingCdmaSms(serial, smsAck);
    return Void();
}

Return<void> Radio::getGsmBroadcastConfig(int32_t serial) {
    getSecIRadio()->getGsmBroadcastConfig(serial);
    return Void();
}

Return<void> Radio::setGsmBroadcastConfig(
    int32_t serial,
    const hidl_vec<::android::hardware::radio::V1_0::GsmBroadcastSmsConfigInfo>& configInfo) {
    getSecIRadio()->setGsmBroadcastConfig(serial, configInfo);
    return Void();
}

Return<void> Radio::setGsmBroadcastActivation(int32_t serial, bool activate) {
    getSecIRadio()->setGsmBroadcastActivation(serial, activate);
    return Void();
}

Return<void> Radio::getCdmaBroadcastConfig(int32_t serial) {
    getSecIRadio()->getCdmaBroadcastConfig(serial);
    return Void();
}

Return<void> Radio::setCdmaBroadcastConfig(
    int32_t serial,
    const hidl_vec<::android::hardware::radio::V1_0::CdmaBroadcastSmsConfigInfo>& configInfo) {
    getSecIRadio()->setCdmaBroadcastConfig(serial, configInfo);
    return Void();
}

Return<void> Radio::setCdmaBroadcastActivation(int32_t serial, bool activate) {
    getSecIRadio()->setCdmaBroadcastActivation(serial, activate);
    return Void();
}

Return<void> Radio::getCDMASubscription(int32_t serial) {
    getSecIRadio()->getCDMASubscription(serial);
    return Void();
}

Return<void> Radio::writeSmsToRuim(
    int32_t serial, const ::android::hardware::radio::V1_0::CdmaSmsWriteArgs& cdmaSms) {
    getSecIRadio()->writeSmsToRuim(serial, cdmaSms);
    return Void();
}

Return<void> Radio::deleteSmsOnRuim(int32_t serial, int32_t index) {
    getSecIRadio()->deleteSmsOnRuim(serial, index);
    return Void();
}

Return<void> Radio::getDeviceIdentity(int32_t serial) {
    getSecIRadio()->getDeviceIdentity(serial);
    return Void();
}

Return<void> Radio::exitEmergencyCallbackMode(int32_t serial) {
    getSecIRadio()->exitEmergencyCallbackMode(serial);
    return Void();
}

Return<void> Radio::getSmscAddress(int32_t serial) {
    getSecIRadio()->getSmscAddress(serial);
    return Void();
}

Return<void> Radio::setSmscAddress(int32_t serial, const hidl_string& smsc) {
    getSecIRadio()->setSmscAddress(serial, smsc);
    return Void();
}

Return<void> Radio::reportSmsMemoryStatus(int32_t serial, bool available) {
    getSecIRadio()->reportSmsMemoryStatus(serial, available);
    return Void();
}

Return<void> Radio::reportStkServiceIsRunning(int32_t serial) {
    getSecIRadio()->reportStkServiceIsRunning(serial);
    return Void();
}

Return<void> Radio::getCdmaSubscriptionSource(int32_t serial) {
    getSecIRadio()->getCdmaSubscriptionSource(serial);
    return Void();
}

Return<void> Radio::requestIsimAuthentication(int32_t serial, const hidl_string& challenge) {
    getSecIRadio()->requestIsimAuthentication(serial, challenge);
    return Void();
}

Return<void> Radio::acknowledgeIncomingGsmSmsWithPdu(int32_t serial, bool success,
                                                     const hidl_string& ackPdu) {
    getSecIRadio()->acknowledgeIncomingGsmSmsWithPdu(serial, success, ackPdu);
    return Void();
}

Return<void> Radio::sendEnvelopeWithStatus(int32_t serial, const hidl_string& contents) {
    getSecIRadio()->sendEnvelopeWithStatus(serial, contents);
    return Void();
}

Return<void> Radio::getVoiceRadioTechnology(int32_t serial) {
    getSecIRadio()->getVoiceRadioTechnology(serial);
    return Void();
}

Return<void> Radio::getCellInfoList(int32_t serial) {
    getSecIRadio()->getCellInfoList(serial);
    return Void();
}

Return<void> Radio::setCellInfoListRate(int32_t serial, int32_t rate) {
    getSecIRadio()->setCellInfoListRate(serial, rate);
    return Void();
}

Return<void> Radio::setInitialAttachApn(
    int32_t serial, const ::android::hardware::radio::V1_0::DataProfileInfo& dataProfileInfo,
    bool modemCognitive, bool isRoaming) {
    getSecIRadio()->setInitialAttachApn(serial, dataProfileInfo, modemCognitive, isRoaming);
    return Void();
}

Return<void> Radio::getImsRegistrationState(int32_t serial) {
    getSecIRadio()->getImsRegistrationState(serial);
    return Void();
}

Return<void> Radio::sendImsSms(int32_t serial,
                               const ::android::hardware::radio::V1_0::ImsSmsMessage& message) {
    getSecIRadio()->sendImsSms(serial, message);
    return Void();
}

Return<void> Radio::iccTransmitApduBasicChannel(
    int32_t serial, const ::android::hardware::radio::V1_0::SimApdu& message) {
    getSecIRadio()->iccTransmitApduBasicChannel(serial, message);
    return Void();
}

Return<void> Radio::iccOpenLogicalChannel(int32_t serial, const hidl_string& aid, int32_t p2) {
    getSecIRadio()->iccOpenLogicalChannel(serial, aid, p2);
    return Void();
}

Return<void> Radio::iccCloseLogicalChannel(int32_t serial, int32_t channelId) {
    getSecIRadio()->iccCloseLogicalChannel(serial, channelId);
    return Void();
}

Return<void> Radio::iccTransmitApduLogicalChannel(
    int32_t serial, const ::android::hardware::radio::V1_0::SimApdu& message) {
    getSecIRadio()->iccTransmitApduLogicalChannel(serial, message);
    return Void();
}

Return<void> Radio::nvReadItem(int32_t serial, ::android::hardware::radio::V1_0::NvItem itemId) {
    getSecIRadio()->nvReadItem(serial, itemId);
    return Void();
}

Return<void> Radio::nvWriteItem(int32_t serial,
                                const ::android::hardware::radio::V1_0::NvWriteItem& item) {
    getSecIRadio()->nvWriteItem(serial, item);
    return Void();
}

Return<void> Radio::nvWriteCdmaPrl(int32_t serial, const hidl_vec<uint8_t>& prl) {
    getSecIRadio()->nvWriteCdmaPrl(serial, prl);
    return Void();
}

Return<void> Radio::nvResetConfig(int32_t serial,
                                  ::android::hardware::radio::V1_0::ResetNvType resetType) {
    getSecIRadio()->nvResetConfig(serial, resetType);
    return Void();
}

Return<void> Radio::setUiccSubscription(
    int32_t serial, const ::android::hardware::radio::V1_0::SelectUiccSub& uiccSub) {
    getSecIRadio()->setUiccSubscription(serial, uiccSub);
    return Void();
}

Return<void> Radio::setDataAllowed(int32_t serial, bool allow) {
    getSecIRadio()->setDataAllowed(serial, allow);
    return Void();
}

Return<void> Radio::getHardwareConfig(int32_t serial) {
    getSecIRadio()->getHardwareConfig(serial);
    return Void();
}

Return<void> Radio::requestIccSimAuthentication(int32_t serial, int32_t authContext,
                                                const hidl_string& authData,
                                                const hidl_string& aid) {
    getSecIRadio()->requestIccSimAuthentication(serial, authContext, authData, aid);
    return Void();
}

Return<void> Radio::setDataProfile(
    int32_t serial, const hidl_vec<::android::hardware::radio::V1_0::DataProfileInfo>& profiles,
    bool isRoaming) {
    getSecIRadio()->setDataProfile(serial, profiles, isRoaming);
    return Void();
}

Return<void> Radio::requestShutdown(int32_t serial) {
    getSecIRadio()->requestShutdown(serial);
    return Void();
}

Return<void> Radio::getRadioCapability(int32_t serial) {
    getSecIRadio()->getRadioCapability(serial);
    return Void();
}

Return<void> Radio::setRadioCapability(int32_t serial,
                                       const ::android::hardware::radio::V1_0::RadioCapability& rc) {
    getSecIRadio()->setRadioCapability(serial, rc);
    return Void();
}

Return<void> Radio::startLceService(int32_t serial, int32_t reportInterval, bool pullMode) {
    getSecIRadio()->startLceService(serial, reportInterval, pullMode);
    return Void();
}

Return<void> Radio::stopLceService(int32_t serial) {
    getSecIRadio()->stopLceService(serial);
    return Void();
}

Return<void> Radio::pullLceData(int32_t serial) {
    getSecIRadio()->pullLceData(serial);
    return Void();
}

Return<void> Radio::getModemActivityInfo(int32_t serial) {
    getSecIRadio()->getModemActivityInfo(serial);
    return Void();
}

Return<void> Radio::setAllowedCarriers(
    int32_t serial, bool allAllowed,
    const ::android::hardware::radio::V1_0::CarrierRestrictions& carriers) {
    getSecIRadio()->setAllowedCarriers(serial, allAllowed, carriers);
    return Void();
}

Return<void> Radio::getAllowedCarriers(int32_t serial) {
    getSecIRadio()->getAllowedCarriers(serial);
    return Void();
}

Return<void> Radio::sendDeviceState(
    int32_t serial, ::android::hardware::radio::V1_0::DeviceStateType deviceStateType, bool state) {
    getSecIRadio()->sendDeviceState(serial, deviceStateType, state);
    return Void();
}

Return<void> Radio::setIndicationFilter(
    int32_t serial,
    hidl_bitfield<::android::hardware::radio::V1_2::IndicationFilter> indicationFilter) {
    getSecIRadio()->setIndicationFilter(serial, indicationFilter);
    return Void();
}

Return<void> Radio::setSimCardPower(int32_t serial, bool powerUp) {
    getSecIRadio()->setSimCardPower(serial, powerUp);
    return Void();
}

Return<void> Radio::responseAcknowledgement() {
    getSecIRadio()->responseAcknowledgement();
    return Void();
}

// Methods from ::android::hardware::radio::V1_1::IRadio follow.
Return<void> Radio::setCarrierInfoForImsiEncryption(
    int32_t serial, const ::android::hardware::radio::V1_1::ImsiEncryptionInfo& imsiEncryptionInfo) {
    getSecIRadio()->setCarrierInfoForImsiEncryption(serial, imsiEncryptionInfo);
    return Void();
}

Return<void> Radio::setSimCardPower_1_1(int32_t serial,
                                        ::android::hardware::radio::V1_1::CardPowerState powerUp) {
    getSecIRadio()->setSimCardPower_1_1(serial, powerUp);
    return Void();
}

Return<void> Radio::startNetworkScan(
    int32_t serial, const ::android::hardware::radio::V1_1::NetworkScanRequest& request) {
    getSecIRadio()->startNetworkScan(serial, request);
    return Void();
}

Return<void> Radio::stopNetworkScan(int32_t serial) {
    getSecIRadio()->stopNetworkScan(serial);
    return Void();
}

Return<void> Radio::startKeepalive(
    int32_t serial, const ::android::hardware::radio::V1_1::KeepaliveRequest& keepalive) {
    getSecIRadio()->startKeepalive(serial, keepalive);
    return Void();
}

Return<void> Radio::stopKeepalive(int32_t serial, int32_t sessionHandle) {
    getSecIRadio()->stopKeepalive(serial, sessionHandle);
    return Void();
}

// Methods from ::android::hardware::radio::V1_2::IRadio follow.
Return<void> Radio::startNetworkScan_1_2(
    int32_t serial, const ::android::hardware::radio::V1_2::NetworkScanRequest& request) {
    getSecIRadio()->startNetworkScan_1_2(serial, request);
    return Void();
}

Return<void> Radio::setIndicationFilter_1_2(
    int32_t serial,
    hidl_bitfield<::android::hardware::radio::V1_2::IndicationFilter> indicationFilter) {
    getSecIRadio()->setIndicationFilter_1_2(serial, indicationFilter);
    return Void();
}

Return<void> Radio::setSignalStrengthReportingCriteria(
    int32_t serial, int32_t hysteresisMs, int32_t hysteresisDb,
    const hidl_vec<int32_t>& thresholdsDbm,
    ::android::hardware::radio::V1_2::AccessNetwork accessNetwork) {
    getSecIRadio()->setSignalStrengthReportingCriteria(serial, hysteresisMs, hysteresisDb,
                                                       thresholdsDbm, accessNetwork);
    return Void();
}

Return<void> Radio::setLinkCapacityReportingCriteria(
    int32_t serial, int32_t hysteresisMs, int32_t hysteresisDlKbps, int32_t hysteresisUlKbps,
    const hidl_vec<int32_t>& thresholdsDownlinkKbps, const hidl_vec<int32_t>& thresholdsUplinkKbps,
    ::android::hardware::radio::V1_2::AccessNetwork accessNetwork) {
    getSecIRadio()->setLinkCapacityReportingCriteria(serial, hysteresisMs, hysteresisDlKbps,
                                                     hysteresisUlKbps, thresholdsDownlinkKbps,
                                                     thresholdsUplinkKbps, accessNetwork);
    return Void();
}

Return<void> Radio::setupDataCall_1_2(
    int32_t serial, ::android::hardware::radio::V1_2::AccessNetwork accessNetwork,
    const ::android::hardware::radio::V1_0::DataProfileInfo& dataProfileInfo, bool modemCognitive,
    bool roamingAllowed, bool isRoaming, ::android::hardware::radio::V1_2::DataRequestReason reason,
    const hidl_vec<hidl_string>& addresses, const hidl_vec<hidl_string>& dnses) {
    getSecIRadio()->setupDataCall_1_2(serial, accessNetwork, dataProfileInfo, modemCognitive,
                                      roamingAllowed, isRoaming, reason, addresses, dnses);
    return Void();
}

Return<void> Radio::deactivateDataCall_1_2(
    int32_t serial, int32_t cid, ::android::hardware::radio::V1_2::DataRequestReason reason) {
    getSecIRadio()->deactivateDataCall_1_2(serial, cid, reason);
    return Void();
}

// Methods from ::android::hardware::radio::V1_3::IRadio follow.
Return<void> Radio::setSystemSelectionChannels(
    int32_t, bool, const hidl_vec<::android::hardware::radio::V1_1::RadioAccessSpecifier>&) {
    return Void();
}

Return<void> Radio::enableModem(int32_t, bool) {
    return Void();
}

Return<void> Radio::getModemStackStatus(int32_t) {
    return Void();
}

}  // namespace implementation
}  // namespace V1_3
}  // namespace radio
}  // namespace hardware
}  // namespace android
