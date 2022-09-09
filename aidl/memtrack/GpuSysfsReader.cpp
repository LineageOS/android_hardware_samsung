#include "GpuSysfsReader.h"

#include <log/log.h>

#include <fstream>
#include <sstream>

#include "filesystem.h"

#undef LOG_TAG
#define LOG_TAG "memtrack-gpusysfsreader"

using namespace GpuSysfsReader;

namespace {
uint64_t readNode(const std::string node, pid_t pid) {
    std::stringstream ss;
    if (pid)
        ss << kSysfsDevicePath << "/" << kProcessDir << "/" << pid << "/" << node;
    else
        ss << kSysfsDevicePath << "/" << node;
    const std::string path = ss.str();

    if (!filesystem::exists(filesystem::path(path))) {
        ALOGV("File not found: %s", path.c_str());
        return 0;
    }

    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        ALOGW("Failed to open %s path", path.c_str());
        return 0;
    }

    uint64_t out;
    file >> out;
    file.close();

    return out;
}
} // namespace

uint64_t GpuSysfsReader::getDmaBufGpuMem(pid_t pid) { return readNode(kDmaBufGpuMemNode, pid); }

uint64_t GpuSysfsReader::getGpuMemTotal(pid_t pid) { return readNode(kTotalGpuMemNode, pid); }

uint64_t GpuSysfsReader::getPrivateGpuMem(pid_t pid) {
    auto dma_buf_size = getDmaBufGpuMem(pid);
    auto gpu_total_size = getGpuMemTotal(pid);

    if (dma_buf_size > gpu_total_size) {
        ALOGE("Bug in reader, dma-buf size (%" PRIu64 ") is higher than total gpu size (%" PRIu64
              ")",
              dma_buf_size, gpu_total_size);
        return 0;
    }

    return gpu_total_size - dma_buf_size;
}
