/*
 * Copyright (C) 2020 The Android Open Source Project
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

#pragma once

#include <aidl/android/hardware/gnss/BnGnssCallback.h>
#include <aidl/android/hardware/gnss/BnGnssConfiguration.h>
#include <android/hardware/gnss/2.1/IGnssCallback.h>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace aidl::android::hardware::gnss {

struct BlocklistedSourceHash {
    inline int operator()(const BlocklistedSource& source) const {
        return int(source.constellation) * 1000 + int(source.svid);
    }
};

struct BlocklistedSourceEqual {
    inline bool operator()(const BlocklistedSource& s1, const BlocklistedSource& s2) const {
        return (s1.constellation == s2.constellation) && (s1.svid == s2.svid);
    }
};

using GnssSvInfoV2_1 = ::android::hardware::gnss::V2_1::IGnssCallback::GnssSvInfo;
using std::vector;
using BlocklistedSourceSet =
        std::unordered_set<BlocklistedSource, BlocklistedSourceHash, BlocklistedSourceEqual>;
using BlocklistedConstellationSet =
        std::unordered_set<android::hardware::gnss::GnssConstellationType>;

struct GnssConfiguration : public BnGnssConfiguration {
  public:
    ndk::ScopedAStatus setSuplVersion(int) override { return ndk::ScopedAStatus::ok(); }

    ndk::ScopedAStatus setSuplMode(int) override { return ndk::ScopedAStatus::ok(); }

    ndk::ScopedAStatus setLppProfile(int) override { return ndk::ScopedAStatus::ok(); }

    ndk::ScopedAStatus setGlonassPositioningProtocol(int) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus setEmergencySuplPdn(bool) override { return ndk::ScopedAStatus::ok(); }

    ndk::ScopedAStatus setEsExtensionSec(int) override { return ndk::ScopedAStatus::ok(); }

    ndk::ScopedAStatus setBlocklist(const vector<BlocklistedSource>& blocklist) override;

    bool isBlocklistedV2_1(const GnssSvInfoV2_1& gnssSvInfo) const;
    bool isBlocklisted(const IGnssCallback::GnssSvInfo& gnssSvInfo) const;

  private:
    BlocklistedSourceSet mBlocklistedSourceSet;
    BlocklistedConstellationSet mBlocklistedConstellationSet;
    mutable std::recursive_mutex mMutex;
};

}  // namespace aidl::android::hardware::gnss
