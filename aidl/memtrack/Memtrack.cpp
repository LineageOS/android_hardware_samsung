#include <Memtrack.h>
#include <stdlib.h>

#include <sstream>
#include <string>
#include <vector>

#include "GpuSysfsReader.h"
#include "filesystem.h"

#undef LOG_TAG
#define LOG_TAG "memtrack-core"

namespace aidl {
namespace android {
namespace hardware {
namespace memtrack {

ndk::ScopedAStatus Memtrack::getMemory(int pid, MemtrackType type,
                                       std::vector<MemtrackRecord>* _aidl_return) {
    if (pid < 0)
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));

    if (type != MemtrackType::OTHER && type != MemtrackType::GL && type != MemtrackType::GRAPHICS &&
        type != MemtrackType::MULTIMEDIA && type != MemtrackType::CAMERA)
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));

    _aidl_return->clear();

    // Other types are retained only for backward compatibility
    if (type != MemtrackType::GL && type != MemtrackType::GRAPHICS)
        return ndk::ScopedAStatus::ok();

    // pid 0 is only supported for GL type to report total private memory
    if (pid == 0 && type != MemtrackType::GL)
        return ndk::ScopedAStatus::ok();

    uint64_t size = 0;
    switch (type) {
        case MemtrackType::GL:
            size = GpuSysfsReader::getPrivateGpuMem(pid);
            break;
        case MemtrackType::GRAPHICS:
            // TODO(b/194483693): This is not PSS as required by memtrack HAL
            // but complete dmabuf allocations. Reporting PSS requires reading
            // procfs. This HAL does not have that permission yet.
            size = GpuSysfsReader::getDmaBufGpuMem(pid);
            break;
        default:
            break;
    }

    MemtrackRecord record = {
            .flags = MemtrackRecord::FLAG_SMAPS_UNACCOUNTED,
            .sizeInBytes = static_cast<long>(size),
    };
    _aidl_return->emplace_back(record);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Memtrack::getGpuDeviceInfo(std::vector<DeviceInfo>* _aidl_return) {
    auto devPath = filesystem::path(GpuSysfsReader::kSysfsDevicePath);
    std::string devName = "default-gpu";
    if (filesystem::exists(devPath) && filesystem::is_symlink(devPath)) {
        devName = filesystem::read_symlink(devPath).filename().string();
    }

    DeviceInfo dev_info = {.id = 0, .name = devName};

    _aidl_return->clear();
    _aidl_return->emplace_back(dev_info);
    return ndk::ScopedAStatus::ok();
}

} // namespace memtrack
} // namespace hardware
} // namespace android
} // namespace aidl
