/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <vector>

#include <hardware/audio_effect.h>

#include "effect-impl/EffectImpl.h"

namespace aidl::android::hardware::audio::effect {

/*
 * IEffect on top of an effect of a legacy effect library (one exporting
 * AUDIO_EFFECT_LIBRARY_INFO_SYM), like the HIDL android.hardware.audio.effect@7.0
 * default implementation.
 *
 * The parameters of effects without an AIDL definition are passed by the
 * framework as a vendor extension holding the legacy effect_param_t, which is
 * passed through as is.
 */
class EffectLegacy final : public EffectImpl {
  public:
    EffectLegacy(const audio_effect_library_t* library, const effect_uuid_t& uuid,
                 const Descriptor& descriptor);
    ~EffectLegacy();

    ndk::ScopedAStatus getDescriptor(Descriptor* desc) override;
    ndk::ScopedAStatus setParameterCommon(const Parameter& param) REQUIRES(mImplMutex) override;
    ndk::ScopedAStatus setParameterSpecific(const Parameter::Specific& specific)
            REQUIRES(mImplMutex) override;
    ndk::ScopedAStatus getParameterSpecific(const Parameter::Id& id, Parameter::Specific* specific)
            REQUIRES(mImplMutex) override;

    std::shared_ptr<EffectContext> createContext(const Parameter::Common& common)
            REQUIRES(mImplMutex) override;
    RetCode releaseContext() REQUIRES(mImplMutex) override;

    std::string getEffectName() override { return mDescriptor.common.name; }
    IEffect::Status effectProcessImpl(float* in, float* out, int samples)
            REQUIRES(mImplMutex) override;

  protected:
    ndk::ScopedAStatus commandImpl(CommandId id) REQUIRES(mImplMutex) override;

  private:
    int legacyCommand(uint32_t code, uint32_t size, void* data, uint32_t* replySize = nullptr,
                      void* replyData = nullptr) REQUIRES(mImplMutex);
    int legacyCommandWithStatus(uint32_t code, uint32_t size, void* data) REQUIRES(mImplMutex);
    bool setConfig(const Parameter::Common& common) REQUIRES(mImplMutex);

    const audio_effect_library_t* const mLibrary;
    const effect_uuid_t mUuid;
    const Descriptor mDescriptor;
    const bool mIsPreProcessing;

    effect_handle_t mHandle GUARDED_BY(mImplMutex) = nullptr;
    // Set when the legacy effect does not process float samples
    bool mUse16Bit GUARDED_BY(mImplMutex) = false;
    size_t mInputChannelCount GUARDED_BY(mImplMutex) = 0;
    size_t mOutputChannelCount GUARDED_BY(mImplMutex) = 0;
    std::vector<int16_t> mInput16 GUARDED_BY(mImplMutex);
    std::vector<int16_t> mOutput16 GUARDED_BY(mImplMutex);
};

/*
 * Registers the legacy library of an effect, so that effect libraries without
 * AIDL entry points can be loaded by the effect factory.
 */
void registerLegacyEffectLibrary(const ::aidl::android::media::audio::common::AudioUuid& impl,
                                 const audio_effect_library_t* library);

binder_exception_t createLegacyEffect(const ::aidl::android::media::audio::common::AudioUuid* uuid,
                                      std::shared_ptr<IEffect>* instance);
binder_exception_t destroyLegacyEffect(const std::shared_ptr<IEffect>& instance);
binder_exception_t queryLegacyEffect(const ::aidl::android::media::audio::common::AudioUuid* uuid,
                                     Descriptor* descriptor);

}  // namespace aidl::android::hardware::audio::effect
