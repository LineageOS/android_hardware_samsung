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

#ifndef ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_0_GRALLOC_MAPPER_H
#define ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_0_GRALLOC_MAPPER_H

#include <android/hardware/graphics/mapper/2.0/IMapper.h>
#include <cutils/native_handle.h>

#include <mutex>
#include <unordered_set>

namespace android {
namespace hardware {
namespace graphics {
namespace mapper {
namespace V2_0 {
namespace implementation {

class GrallocMapper : public IMapper {
   public:
    // IMapper interface
    Return<void> createDescriptor(const BufferDescriptorInfo& descriptorInfo,
                                  createDescriptor_cb hidl_cb) override;
    Return<void> importBuffer(const hidl_handle& rawHandle,
                              importBuffer_cb hidl_cb) override;
    Return<Error> freeBuffer(void* buffer) override;
    Return<void> lock(void* buffer, uint64_t cpuUsage,
                      const IMapper::Rect& accessRegion,
                      const hidl_handle& acquireFence,
                      lock_cb hidl_cb) override;
    Return<void> lockYCbCr(void* buffer, uint64_t cpuUsage,
                           const IMapper::Rect& accessRegion,
                           const hidl_handle& acquireFence,
                           lockYCbCr_cb hidl_cb) override;
    Return<void> unlock(void* buffer, unlock_cb hidl_cb) override;

   protected:
    static void waitFenceFd(int fenceFd, const char* logname);

    struct {
        bool highUsageBits;
        bool layeredBuffers;
        bool unregisterImplyDelete;
    } mCapabilities = {};

   private:
    virtual bool validateDescriptorInfo(
        const BufferDescriptorInfo& descriptorInfo) const;

    // Register a buffer.  The handle is already cloned by the caller.
    virtual Error registerBuffer(buffer_handle_t bufferHandle) = 0;

    // Unregister a buffer.  The handle is closed and deleted by the
    // callee if and only if mCapabilities.unregisterImplyDelete is set.
    virtual void unregisterBuffer(buffer_handle_t bufferHandle) = 0;

    // Lock a buffer.  The fence is owned by the caller.
    virtual Error lockBuffer(buffer_handle_t bufferHandle, uint64_t cpuUsage,
                             const IMapper::Rect& accessRegion, int fenceFd,
                             void** outData) = 0;
    virtual Error lockBuffer(buffer_handle_t bufferHandle, uint64_t cpuUsage,
                             const IMapper::Rect& accessRegion, int fenceFd,
                             YCbCrLayout* outLayout) = 0;

    // Unlock a buffer.  The returned fence is owned by the caller.
    virtual Error unlockBuffer(buffer_handle_t bufferHandle,
                               int* outFenceFd) = 0;

    static bool getFenceFd(const hidl_handle& fenceHandle, int* outFenceFd);
    static hidl_handle getFenceHandle(int fenceFd, char* handleStorage);
};

extern "C" IMapper* HIDL_FETCH_IMapper(const char* name);

} // namespace implementation
} // namespace V2_0
} // namespace mapper
} // namespace graphics
} // namespace hardware
} // namespace android

#endif // ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_0_GRALLOC_MAPPER_H
