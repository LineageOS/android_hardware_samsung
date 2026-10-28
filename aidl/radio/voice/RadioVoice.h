/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/voice/BnRadioVoice.h>

#include "common.h"

namespace aidl::android::hardware::radio::samsung {

class RadioVoice : public voice::BnRadioVoice,
                   public RadioService<voice::IRadioVoiceResponse, voice::IRadioVoiceIndication> {
  public:
    explicit RadioVoice(RadioSlot& slot);

    ::ndk::ScopedAStatus setResponseFunctions(
            const std::shared_ptr<voice::IRadioVoiceResponse>& response,
            const std::shared_ptr<voice::IRadioVoiceIndication>& indication) override;
    ::ndk::ScopedAStatus responseAcknowledgement() override;

    ::ndk::ScopedAStatus acceptCall(int32_t serial) override;
    ::ndk::ScopedAStatus cancelPendingUssd(int32_t serial) override;
    ::ndk::ScopedAStatus conference(int32_t serial) override;
    ::ndk::ScopedAStatus dial(int32_t serial, const voice::Dial& dialInfo) override;
    ::ndk::ScopedAStatus emergencyDial(int32_t serial, const voice::Dial& dialInfo,
                                       int32_t categories, const std::vector<std::string>& urns,
                                       voice::EmergencyCallRouting routing,
                                       bool hasKnownUserIntentEmergency, bool isTesting) override;
    ::ndk::ScopedAStatus exitEmergencyCallbackMode(int32_t serial) override;
    ::ndk::ScopedAStatus explicitCallTransfer(int32_t serial) override;
    ::ndk::ScopedAStatus getCallForwardStatus(int32_t serial,
                                              const voice::CallForwardInfo& callInfo) override;
    ::ndk::ScopedAStatus getCallWaiting(int32_t serial, int32_t serviceClass) override;
    ::ndk::ScopedAStatus getClip(int32_t serial) override;
    ::ndk::ScopedAStatus getClir(int32_t serial) override;
    ::ndk::ScopedAStatus getCurrentCalls(int32_t serial) override;
    ::ndk::ScopedAStatus getLastCallFailCause(int32_t serial) override;
    ::ndk::ScopedAStatus getMute(int32_t serial) override;
    ::ndk::ScopedAStatus getPreferredVoicePrivacy(int32_t serial) override;
    ::ndk::ScopedAStatus getTtyMode(int32_t serial) override;
    ::ndk::ScopedAStatus handleStkCallSetupRequestFromSim(int32_t serial, bool accept) override;
    ::ndk::ScopedAStatus hangup(int32_t serial, int32_t gsmIndex) override;
    ::ndk::ScopedAStatus hangupForegroundResumeBackground(int32_t serial) override;
    ::ndk::ScopedAStatus hangupWaitingOrBackground(int32_t serial) override;
    ::ndk::ScopedAStatus isVoNrEnabled(int32_t serial) override;
    ::ndk::ScopedAStatus rejectCall(int32_t serial) override;
    ::ndk::ScopedAStatus sendBurstDtmf(int32_t serial, const std::string& dtmf, int32_t on,
                                       int32_t off) override;
    ::ndk::ScopedAStatus sendCdmaFeatureCode(int32_t serial,
                                             const std::string& featureCode) override;
    ::ndk::ScopedAStatus sendDtmf(int32_t serial, const std::string& s) override;
    ::ndk::ScopedAStatus sendUssd(int32_t serial, const std::string& ussd) override;
    ::ndk::ScopedAStatus separateConnection(int32_t serial, int32_t gsmIndex) override;
    ::ndk::ScopedAStatus setCallForward(int32_t serial,
                                        const voice::CallForwardInfo& callInfo) override;
    ::ndk::ScopedAStatus setCallWaiting(int32_t serial, bool enable, int32_t serviceClass) override;
    ::ndk::ScopedAStatus setClir(int32_t serial, int32_t status) override;
    ::ndk::ScopedAStatus setMute(int32_t serial, bool enable) override;
    ::ndk::ScopedAStatus setPreferredVoicePrivacy(int32_t serial, bool enable) override;
    ::ndk::ScopedAStatus setTtyMode(int32_t serial, voice::TtyMode mode) override;
    ::ndk::ScopedAStatus setVoNrEnabled(int32_t serial, bool enable) override;
    ::ndk::ScopedAStatus startDtmf(int32_t serial, const std::string& s) override;
    ::ndk::ScopedAStatus stopDtmf(int32_t serial) override;
    ::ndk::ScopedAStatus switchWaitingOrHoldingAndActive(int32_t serial) override;

    bool onUnsolicited(int id, const void* data, size_t length, RadioIndicationType type);

  private:
    RadioSlot& mSlot;
};

}  // namespace aidl::android::hardware::radio::samsung
