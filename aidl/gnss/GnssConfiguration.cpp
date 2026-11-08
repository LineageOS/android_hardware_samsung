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

#define LOG_TAG "GnssConfigurationAidl"

#include "GnssConfiguration.h"
#include <log/log.h>

namespace aidl::android::hardware::gnss {

ndk::ScopedAStatus GnssConfiguration::setBlocklist(const vector<BlocklistedSource>& sourceList) {
    ALOGD("GnssConfiguration::setBlocklist");
    std::unique_lock<std::recursive_mutex> lock(mMutex);
    mBlocklistedConstellationSet.clear();
    mBlocklistedSourceSet.clear();
    for (const auto& source : sourceList) {
        if (source.svid == 0) {
            // Wildcard blocklist, i.e., blocklist entire constellation.
            mBlocklistedConstellationSet.insert(source.constellation);
        } else {
            mBlocklistedSourceSet.insert(source);
        }
    }
    return ndk::ScopedAStatus::ok();
}

bool GnssConfiguration::isBlocklistedV2_1(const GnssSvInfoV2_1& gnssSvInfo) const {
    std::unique_lock<std::recursive_mutex> lock(mMutex);
    if (mBlocklistedConstellationSet.find(static_cast<GnssConstellationType>(
                gnssSvInfo.v2_0.constellation)) != mBlocklistedConstellationSet.end()) {
        return true;
    }
    BlocklistedSource source = {
            .constellation = static_cast<GnssConstellationType>(gnssSvInfo.v2_0.constellation),
            .svid = gnssSvInfo.v2_0.v1_0.svid};
    return (mBlocklistedSourceSet.find(source) != mBlocklistedSourceSet.end());
}

bool GnssConfiguration::isBlocklisted(const IGnssCallback::GnssSvInfo& gnssSvInfo) const {
    std::unique_lock<std::recursive_mutex> lock(mMutex);
    if (mBlocklistedConstellationSet.find(gnssSvInfo.constellation) !=
        mBlocklistedConstellationSet.end()) {
        return true;
    }
    BlocklistedSource source = {.constellation = gnssSvInfo.constellation, .svid = gnssSvInfo.svid};
    return (mBlocklistedSourceSet.find(source) != mBlocklistedSourceSet.end());
}

}  // namespace aidl::android::hardware::gnss
