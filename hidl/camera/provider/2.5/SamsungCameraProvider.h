/*
 * Copyright (C) 2021 The LineageOS Project
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

#ifndef SAMSUNG_CAMERA_PROVIDER_H

#include "LegacyCameraProviderImpl_2_5.h"

#define SAMSUNG_CAMERA_DEBUG

using ::android::hardware::camera::provider::V2_5::implementation::LegacyCameraProviderImpl_2_5;
using ::android::hardware::Return;

class SamsungCameraProvider : public LegacyCameraProviderImpl_2_5 {
public:
    SamsungCameraProvider();
    ~SamsungCameraProvider();

private:
    std::vector<int> mExtraIDs = {
#ifdef EXTRA_IDS
        EXTRA_IDS
#endif
    };
};

#endif // SAMSUNG_CAMERA_PROVIDER_H
