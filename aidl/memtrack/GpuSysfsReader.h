
#pragma once

#include <inttypes.h>
#include <sys/types.h>

namespace GpuSysfsReader {
uint64_t getDmaBufGpuMem(pid_t pid = 0);
uint64_t getGpuMemTotal(pid_t pid = 0);
uint64_t getPrivateGpuMem(pid_t pid = 0);

constexpr char kSysfsDevicePath[] = "/sys/class/misc/mali0/device";
constexpr char kProcessDir[] = "kprcs";
constexpr char kMappedDmaBufsDir[] = "dma_bufs";
constexpr char kTotalGpuMemNode[] = "total_gpu_mem";
constexpr char kDmaBufGpuMemNode[] = "dma_buf_gpu_mem";
} // namespace GpuSysfsReader
