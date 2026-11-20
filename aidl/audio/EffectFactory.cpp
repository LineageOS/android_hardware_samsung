/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dlfcn.h>
#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_set>
#define LOG_TAG "AHAL_EffectFactory"

#include <android-base/logging.h>
#include <android/binder_ibinder_platform.h>
#include <system/audio_aidl_utils.h>
#include <system/audio_effects/effect_uuid.h>
#include <system/thread_defs.h>

#include "effect-impl/EffectTypes.h"
#include "effectFactory-impl/EffectFactory.h"

using aidl::android::media::audio::common::AudioUuid;

namespace aidl::android::hardware::audio::effect {

Factory::Factory(const std::string& file) : mConfig(EffectConfig(file)) {
    LOG(DEBUG) << __func__ << " with config file: " << file;
    loadEffectLibs();
}

Factory::~Factory() {
    if (auto count = mEffectMap.size()) {
        LOG(WARNING) << __func__ << " remaining " << count
                     << " effect instances not destroyed indicating resource leak!";
        for (const auto& it : mEffectMap) {
            if (auto spEffect = it.first.lock()) {
                LOG(WARNING) << __func__ << " erase remaining instance UUID "
                             << ::android::audio::utils::toString(it.second.first);
                destroyEffectImpl_l(spEffect);
            }
        }
    }
}

ndk::ScopedAStatus Factory::getDescriptorWithUuid_l(const AudioUuid& uuid, Descriptor* desc) {
    RETURN_IF(!desc, EX_NULL_POINTER, "nullDescriptor");

    if (mEffectLibMap.count(uuid)) {
        auto& entry = mEffectLibMap[uuid];
        getDlSyms_l(entry);
        auto& libInterface = std::get<kMapEntryInterfaceIndex>(entry);
        RETURN_IF(!libInterface || !libInterface->queryEffectFunc, EX_NULL_POINTER,
                  "dlNullQueryEffectFunc");
        RETURN_IF_BINDER_EXCEPTION(libInterface->queryEffectFunc(&uuid, desc));
        return ndk::ScopedAStatus::ok();
    }

    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus Factory::queryEffects(const std::optional<AudioUuid>& in_type_uuid,
                                         const std::optional<AudioUuid>& in_impl_uuid,
                                         const std::optional<AudioUuid>& in_proxy_uuid,
                                         std::vector<Descriptor>* _aidl_return) {
    std::lock_guard lg(mMutex);
    // get the matching list
    std::vector<Descriptor::Identity> idList;
    std::copy_if(mIdentitySet.begin(), mIdentitySet.end(), std::back_inserter(idList),
                 [&](auto& id) {
                     return (!in_type_uuid.has_value() || in_type_uuid.value() == id.type) &&
                            (!in_impl_uuid.has_value() || in_impl_uuid.value() == id.uuid) &&
                            (!in_proxy_uuid.has_value() ||
                             (id.proxy.has_value() && in_proxy_uuid.value() == id.proxy.value()));
                 });
    // query through the matching list
    for (const auto& id : idList) {
        if (mEffectLibMap.count(id.uuid)) {
            Descriptor desc;
            RETURN_IF_ASTATUS_NOT_OK(getDescriptorWithUuid_l(id.uuid, &desc),
                                     "getDescriptorFailed");
            // update proxy UUID with information from config xml
            desc.common.id.proxy = id.proxy;
            _aidl_return->emplace_back(std::move(desc));
        }
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Factory::queryProcessing(const std::optional<Processing::Type>& in_type,
                                            std::vector<Processing>* _aidl_return) {
    std::lock_guard lg(mMutex);
    const auto& processings = mConfig.getProcessingMap();
    // Processing stream type
    for (const auto& procIter : processings) {
        if (!in_type.has_value() || in_type.value() == procIter.first) {
            Processing process = {.type = procIter.first /* Processing::Type */};
            for (const auto& libs : procIter.second /* std::vector<struct EffectLibraries> */) {
                for (const auto& lib : libs.libraries /* std::vector<struct Library> */) {
                    Descriptor desc;
                    if (libs.proxyLibrary.has_value()) {
                        desc.common.id.proxy = libs.proxyLibrary.value().uuid;
                    }
                    RETURN_IF_ASTATUS_NOT_OK(getDescriptorWithUuid_l(lib.uuid, &desc),
                                             "getDescriptorFailed");
                    process.ids.emplace_back(desc);
                }
            }
            _aidl_return->emplace_back(process);
        }
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Factory::createEffect(const AudioUuid& in_impl_uuid,
                                         std::shared_ptr<IEffect>* _aidl_return) {
    LOG(DEBUG) << __func__ << ": UUID " << ::android::audio::utils::toString(in_impl_uuid);
    std::lock_guard lg(mMutex);
    if (mEffectLibMap.count(in_impl_uuid)) {
        auto& entry = mEffectLibMap[in_impl_uuid];
        getDlSyms_l(entry);

        auto& libInterface = std::get<kMapEntryInterfaceIndex>(entry);
        RETURN_IF(!libInterface || !libInterface->createEffectFunc, EX_NULL_POINTER,
                  "dlNullcreateEffectFunc");
        std::shared_ptr<IEffect> effectSp;
        RETURN_IF_BINDER_EXCEPTION(libInterface->createEffectFunc(&in_impl_uuid, &effectSp));
        if (!effectSp) {
            LOG(WARNING) << __func__ << ": library created null instance without return error!";
            return ndk::ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
        }
        *_aidl_return = effectSp;
        ndk::SpAIBinder effectBinder = effectSp->asBinder();
        AIBinder_setMinSchedulerPolicy(effectBinder.get(), SCHED_NORMAL, ANDROID_PRIORITY_AUDIO);
        AIBinder_setInheritRt(effectBinder.get(), true);
        mEffectMap[std::weak_ptr<IEffect>(effectSp)] =
                std::make_pair(in_impl_uuid, std::move(effectBinder));
        return ndk::ScopedAStatus::ok();
    } else {
        LOG(ERROR) << __func__ << ": library doesn't exist";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Factory::destroyEffectImpl_l(const std::shared_ptr<IEffect>& in_handle) {
    std::weak_ptr<IEffect> wpHandle(in_handle);
    // find the effect entry with key (std::weak_ptr<IEffect>)
    if (auto effectIt = mEffectMap.find(wpHandle); effectIt != mEffectMap.end()) {
        auto& uuid = effectIt->second.first;
        // find implementation library with UUID
        if (auto libIt = mEffectLibMap.find(uuid); libIt != mEffectLibMap.end()) {
            auto& interface = std::get<kMapEntryInterfaceIndex>(libIt->second);
            RETURN_IF(!interface || !interface->destroyEffectFunc, EX_NULL_POINTER,
                      "dlNulldestroyEffectFunc");
            RETURN_IF_BINDER_EXCEPTION(interface->destroyEffectFunc(in_handle));
        } else {
            LOG(ERROR) << __func__ << ": UUID " << ::android::audio::utils::toString(uuid)
                       << " does not exist in libMap!";
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        mEffectMap.erase(effectIt);
        return ndk::ScopedAStatus::ok();
    } else {
        LOG(ERROR) << __func__ << ": instance " << in_handle << " does not exist!";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
}

// go over the map and cleanup all expired weak_ptrs.
void Factory::cleanupEffectMap_l() {
    for (auto it = mEffectMap.begin(); it != mEffectMap.end();) {
        if (nullptr == it->first.lock()) {
            it = mEffectMap.erase(it);
        } else {
            ++it;
        }
    }
}

ndk::ScopedAStatus Factory::destroyEffect(const std::shared_ptr<IEffect>& in_handle) {
    std::lock_guard lg(mMutex);
    ndk::ScopedAStatus status = destroyEffectImpl_l(in_handle);
    // always do the cleanup
    cleanupEffectMap_l();
    return status;
}

bool Factory::openEffectLibrary(const AudioUuid& impl,
                                const std::string& path) NO_THREAD_SAFETY_ANALYSIS {
    std::function<void(void*)> dlClose = [](void* handle) -> void {
        if (handle && dlclose(handle)) {
            LOG(ERROR) << "dlclose failed " << dlerror();
        }
    };

    auto libHandle =
            std::unique_ptr<void, decltype(dlClose)>{dlopen(path.c_str(), RTLD_LAZY), dlClose};
    if (!libHandle) {
        LOG(ERROR) << __func__ << ": dlopen failed, err: " << dlerror();
        return false;
    }

    LOG(DEBUG) << __func__ << " dlopen lib: " << path
               << "\nimpl:" << ::android::audio::utils::toString(impl) << "\nhandle:" << libHandle;
    auto interface = new effect_dl_interface_s{nullptr, nullptr, nullptr};
    mEffectLibMap.insert(
            {impl,
             std::make_tuple(std::move(libHandle),
                             std::unique_ptr<struct effect_dl_interface_s>(interface), path)});
    return true;
}

void Factory::createIdentityWithConfig(
        const EffectConfig::Library& configLib, const AudioUuid& typeUuid,
        const std::optional<AudioUuid> proxyUuid) NO_THREAD_SAFETY_ANALYSIS {
    static const auto& libMap = mConfig.getLibraryMap();
    const std::string& libName = configLib.name;
    if (auto path = libMap.find(libName); path != libMap.end()) {
        Descriptor::Identity id;
        id.type = typeUuid;
        id.uuid = configLib.uuid;
        id.proxy = proxyUuid;
        LOG(WARNING) << __func__ << " loading lib " << path->second << ": typeUuid "
                     << ::android::audio::utils::toString(id.type) << "\nimplUuid "
                     << ::android::audio::utils::toString(id.uuid) << " proxyUuid "
                     << (proxyUuid.has_value()
                                 ? ::android::audio::utils::toString(proxyUuid.value())
                                 : "null");
        if (openEffectLibrary(id.uuid, path->second)) {
            mIdentitySet.insert(std::move(id));
        }
    } else {
        LOG(ERROR) << __func__ << ": library " << libName << " not exist!";
        return;
    }
}

void Factory::loadEffectLibs() {
    const auto& configEffectsMap = mConfig.getEffectsMap();
    for (const auto& configEffects : configEffectsMap) {
        if (AudioUuid type; EffectConfig::findUuid(configEffects /* xml effect */, &type)) {
            const auto& configLibs = configEffects.second;
            std::optional<AudioUuid> proxyUuid;
            if (configLibs.proxyLibrary.has_value()) {
                const auto& proxyLib = configLibs.proxyLibrary.value();
                proxyUuid = proxyLib.uuid;
            }
            for (const auto& configLib : configLibs.libraries) {
                createIdentityWithConfig(configLib, type, proxyUuid);
            }
        } else {
            LOG(WARNING) << __func__ << ": can not find type UUID for effect "
                         << configEffects.first << " skipping!";
        }
    }
}

void Factory::getDlSyms_l(DlEntry& entry) {
    auto& dlHandle = std::get<kMapEntryHandleIndex>(entry);
    RETURN_VALUE_IF(!dlHandle, void(), "dlNullHandle");
    // Get the reference of the DL interfaces in library map tuple.
    auto& dlInterface = std::get<kMapEntryInterfaceIndex>(entry);
    // return if interface already exist
    if (!dlInterface->createEffectFunc) {
        dlInterface->createEffectFunc = (EffectCreateFunctor)dlsym(dlHandle.get(), "createEffect");
    }
    if (!dlInterface->queryEffectFunc) {
        dlInterface->queryEffectFunc = (EffectQueryFunctor)dlsym(dlHandle.get(), "queryEffect");
    }
    if (!dlInterface->destroyEffectFunc) {
        dlInterface->destroyEffectFunc =
                (EffectDestroyFunctor)dlsym(dlHandle.get(), "destroyEffect");
    }

    if (!dlInterface->createEffectFunc || !dlInterface->destroyEffectFunc ||
        !dlInterface->queryEffectFunc) {
        LOG(ERROR) << __func__ << ": create (" << dlInterface->createEffectFunc << "), query ("
                   << dlInterface->queryEffectFunc << "), or destroy ("
                   << dlInterface->destroyEffectFunc
                   << ") not exist in library: " << std::get<kMapEntryLibNameIndex>(entry)
                   << " handle: " << dlHandle << " with dlerror: " << dlerror();
        return;
    }
}

}  // namespace aidl::android::hardware::audio::effect
