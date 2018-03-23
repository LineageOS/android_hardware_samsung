/*
 * Copyright 2016 The Android Open Source Project
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
#ifndef ANDROID_HARDWARE_GRALLOC1_ADAPTER_H
#define ANDROID_HARDWARE_GRALLOC1_ADAPTER_H

#include <hardware/hardware.h>

__BEGIN_DECLS

#define GRALLOC1_ADAPTER_MODULE_API_VERSION_1_0 \
    HARDWARE_MODULE_API_VERSION(1, 0)

enum {
    GRALLOC1_ADAPTER_PERFORM_FIRST = 10000,

    // void getRealModuleApiVersionMinor(..., int* outMinorVersion);
    GRALLOC1_ADAPTER_PERFORM_GET_REAL_MODULE_API_VERSION_MINOR =
        GRALLOC1_ADAPTER_PERFORM_FIRST,

    // void setUsages(..., buffer_handle_t buffer,
    //                     int producerUsage,
    //                     int consumerUsage);
    GRALLOC1_ADAPTER_PERFORM_SET_USAGES =
        GRALLOC1_ADAPTER_PERFORM_FIRST + 1,

    // void getDimensions(..., buffer_handle_t buffer,
    //                         int* outWidth,
    //                         int* outHeight);
    GRALLOC1_ADAPTER_PERFORM_GET_DIMENSIONS =
        GRALLOC1_ADAPTER_PERFORM_FIRST + 2,

    // void getFormat(..., buffer_handle_t buffer, int* outFormat);
    GRALLOC1_ADAPTER_PERFORM_GET_FORMAT =
        GRALLOC1_ADAPTER_PERFORM_FIRST + 3,

    // void getProducerUsage(..., buffer_handle_t buffer, int* outUsage);
    GRALLOC1_ADAPTER_PERFORM_GET_PRODUCER_USAGE =
        GRALLOC1_ADAPTER_PERFORM_FIRST + 4,

    // void getConsumerUsage(..., buffer_handle_t buffer, int* outUsage);
    GRALLOC1_ADAPTER_PERFORM_GET_CONSUMER_USAGE =
        GRALLOC1_ADAPTER_PERFORM_FIRST + 5,

    // void getBackingStore(..., buffer_handle_t buffer,
    //                           uint64_t* outBackingStore);
    GRALLOC1_ADAPTER_PERFORM_GET_BACKING_STORE =
        GRALLOC1_ADAPTER_PERFORM_FIRST + 6,

    // void getNumFlexPlanes(..., buffer_handle_t buffer,
    //                            int* outNumFlexPlanes);
    GRALLOC1_ADAPTER_PERFORM_GET_NUM_FLEX_PLANES =
        GRALLOC1_ADAPTER_PERFORM_FIRST + 7,

    // void getStride(..., buffer_handle_t buffer, int* outStride);
    GRALLOC1_ADAPTER_PERFORM_GET_STRIDE =
        GRALLOC1_ADAPTER_PERFORM_FIRST + 8,

    // void lockFlex(..., buffer_handle_t buffer,
    //                    int producerUsage,
    //                    int consumerUsage,
    //                    int left,
    //                    int top,
    //                    int width,
    //                    int height,
    //                    android_flex_layout* outLayout,
    //                    int acquireFence);
    GRALLOC1_ADAPTER_PERFORM_LOCK_FLEX =
        GRALLOC1_ADAPTER_PERFORM_FIRST + 9,
};

int gralloc1_adapter_device_open(const struct hw_module_t* module,
        const char* id, struct hw_device_t** device);

__END_DECLS

#endif /* ANDROID_HARDWARE_GRALLOC1_ADAPTER_H */
