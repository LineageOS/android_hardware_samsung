/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/sim/BnRadioSim.h>

#include "common.h"

namespace aidl::android::hardware::radio::samsung {

class RadioSim : public sim::BnRadioSim,
                 public RadioService<sim::IRadioSimResponse, sim::IRadioSimIndication> {
  public:
    explicit RadioSim(RadioSlot& slot);

    ::ndk::ScopedAStatus setResponseFunctions(
            const std::shared_ptr<sim::IRadioSimResponse>& response,
            const std::shared_ptr<sim::IRadioSimIndication>& indication) override;
    ::ndk::ScopedAStatus responseAcknowledgement() override;

    ::ndk::ScopedAStatus areUiccApplicationsEnabled(int32_t serial) override;
    ::ndk::ScopedAStatus changeIccPin2ForApp(int32_t serial, const std::string& oldPin2,
                                             const std::string& newPin2,
                                             const std::string& aid) override;
    ::ndk::ScopedAStatus changeIccPinForApp(int32_t serial, const std::string& oldPin,
                                            const std::string& newPin,
                                            const std::string& aid) override;
    ::ndk::ScopedAStatus enableUiccApplications(int32_t serial, bool enable) override;
    ::ndk::ScopedAStatus getAllowedCarriers(int32_t serial) override;
    ::ndk::ScopedAStatus getCdmaSubscription(int32_t serial) override;
    ::ndk::ScopedAStatus getCdmaSubscriptionSource(int32_t serial) override;
    ::ndk::ScopedAStatus getFacilityLockForApp(int32_t serial, const std::string& facility,
                                               const std::string& password, int32_t serviceClass,
                                               const std::string& appId) override;
    ::ndk::ScopedAStatus getIccCardStatus(int32_t serial) override;
    ::ndk::ScopedAStatus getImsiForApp(int32_t serial, const std::string& aid) override;
    ::ndk::ScopedAStatus getSimPhonebookCapacity(int32_t serial) override;
    ::ndk::ScopedAStatus getSimPhonebookRecords(int32_t serial) override;
    ::ndk::ScopedAStatus iccCloseLogicalChannel(int32_t serial, int32_t channelId) override;
    ::ndk::ScopedAStatus iccIoForApp(int32_t serial, const sim::IccIo& iccIo) override;
    ::ndk::ScopedAStatus iccOpenLogicalChannel(int32_t serial, const std::string& aid,
                                               int32_t p2) override;
    ::ndk::ScopedAStatus iccTransmitApduBasicChannel(int32_t serial,
                                                     const sim::SimApdu& message) override;
    ::ndk::ScopedAStatus iccTransmitApduLogicalChannel(int32_t serial,
                                                       const sim::SimApdu& message) override;
    ::ndk::ScopedAStatus reportStkServiceIsRunning(int32_t serial) override;
    ::ndk::ScopedAStatus requestIccSimAuthentication(int32_t serial, int32_t authContext,
                                                     const std::string& authData,
                                                     const std::string& aid) override;
    ::ndk::ScopedAStatus sendEnvelope(int32_t serial, const std::string& contents) override;
    ::ndk::ScopedAStatus sendEnvelopeWithStatus(int32_t serial,
                                                const std::string& contents) override;
    ::ndk::ScopedAStatus sendTerminalResponseToSim(int32_t serial,
                                                   const std::string& contents) override;
    ::ndk::ScopedAStatus setAllowedCarriers(int32_t serial,
                                            const sim::CarrierRestrictions& carriers,
                                            sim::SimLockMultiSimPolicy multiSimPolicy) override;
    ::ndk::ScopedAStatus setCarrierInfoForImsiEncryption(
            int32_t serial, const sim::ImsiEncryptionInfo& imsiEncryptionInfo) override;
    ::ndk::ScopedAStatus setCdmaSubscriptionSource(int32_t serial,
                                                   sim::CdmaSubscriptionSource cdmaSub) override;
    ::ndk::ScopedAStatus setFacilityLockForApp(int32_t serial, const std::string& facility,
                                               bool lockState, const std::string& password,
                                               int32_t serviceClass,
                                               const std::string& appId) override;
    ::ndk::ScopedAStatus setSimCardPower(int32_t serial, sim::CardPowerState powerUp) override;
    ::ndk::ScopedAStatus setUiccSubscription(int32_t serial,
                                             const sim::SelectUiccSub& uiccSub) override;
    ::ndk::ScopedAStatus supplyIccPin2ForApp(int32_t serial, const std::string& pin2,
                                             const std::string& aid) override;
    ::ndk::ScopedAStatus supplyIccPinForApp(int32_t serial, const std::string& pin,
                                            const std::string& aid) override;
    ::ndk::ScopedAStatus supplyIccPuk2ForApp(int32_t serial, const std::string& puk2,
                                             const std::string& pin2,
                                             const std::string& aid) override;
    ::ndk::ScopedAStatus supplyIccPukForApp(int32_t serial, const std::string& puk,
                                            const std::string& pin,
                                            const std::string& aid) override;
    ::ndk::ScopedAStatus supplySimDepersonalization(int32_t serial, sim::PersoSubstate persoType,
                                                    const std::string& controlKey) override;
    ::ndk::ScopedAStatus updateSimPhonebookRecords(
            int32_t serial, const sim::PhonebookRecordInfo& recordInfo) override;
    ::ndk::ScopedAStatus iccCloseLogicalChannelWithSessionInfo(
            int32_t serial, const sim::SessionInfo& sessionInfo) override;

    bool onUnsolicited(int id, const void* data, size_t length, RadioIndicationType type);

  private:
    void transmitApdu(int32_t serial, int request, const sim::SimApdu& message,
                      const std::string& apduData);
    std::string addMepA1Port(const std::string& hex, int32_t& p3);
    bool convertCardStatus(const SecCardStatus& status, sim::CardStatus& out);
    static sim::IccIoResult convertIccIoResult(const RIL_SIM_IO_Response* result);
    void queryUiccApplicationsEnabled();
    void updateUiccApplicationsEnabled(bool enabled);

    RadioSlot& mSlot;

    std::mutex mUiccMutex;
    std::optional<bool> mUiccApplicationsEnabled;
};

}  // namespace aidl::android::hardware::radio::samsung
