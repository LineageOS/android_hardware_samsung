/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "argos.h"

#define ARGOS_NET_THROUGHPUT_DEV "/dev/network_throughput"
#define ARGOS_PROC_NET_DEV "/proc/net/dev"

#define ARGOS_PROP_NODE "/net_boost,node"
#define ARGOS_PROP_LABEL "/net_boost,label"
#define ARGOS_PROP_SYSNODE "/net_boost,sysnode"

#define MS_PER_SECOND 1000
#define DEFAULT_INTERVAL_MS MS_PER_SECOND

#define NET_DEV_HEADER_LINES 2
#define NET_DEV_LINE_MAX 1024
#define IFACE_NAME_LEN 32

static const argos_platform_t kPlatformQcom = {
        .name = "qcom",
        .dt_path = "/proc/device-tree/soc/argos",
        .device_prefix = "/proc/device-tree/soc/argos/boot_device@",
        .interval_prop = "/net_boost,interval_idx",
        .log_throughput = true,
};

static const argos_platform_t kPlatformExynos = {
        .name = "exynos",
        .dt_path = "/proc/device-tree/argos",
        .device_prefix = "/proc/device-tree/argos/boot_device@",
        .interval_prop = "/net_boost,interval",
        .log_throughput = false,
};

static bool ends_with(const char* str, const char* suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    return str_len >= suffix_len && strcmp(str + str_len - suffix_len, suffix) == 0;
}

/* Reads the first line of a text file into buf, without the trailing newline. */
static bool read_line(const char* path, char* buf, size_t size) {
    buf[0] = '\0';

    FILE* fp = fopen(path, "re");
    if (!fp) return false;

    bool ok = fgets(buf, (int)size, fp) != NULL;
    fclose(fp);

    if (!ok) {
        buf[0] = '\0';
        return false;
    }

    buf[strcspn(buf, "\n")] = '\0';
    return true;
}

/*
 * Reads a device tree "interval index" cell (big endian u32) and maps it to
 * milliseconds. Missing or out-of-range values fall back to the default.
 */
static uint64_t read_interval_ms(const char* path) {
    static const uint64_t kIntervalsMs[] = {1000, 500, 250};

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return DEFAULT_INTERVAL_MS;

    uint32_t cell = 0;
    ssize_t n = read(fd, &cell, sizeof(cell));
    close(fd);

    if (n != (ssize_t)sizeof(cell)) return DEFAULT_INTERVAL_MS;

    uint32_t idx = ntohl(cell);
    if (idx >= sizeof(kIntervalsMs) / sizeof(kIntervalsMs[0])) return DEFAULT_INTERVAL_MS;

    return kIntervalsMs[idx];
}

/* True if `iface` is one of the space-separated names in `node`. */
static bool node_has_iface(const char* node, const char* iface) {
    size_t iface_len = strlen(iface);

    while (*node) {
        while (*node == ' ') node++;

        size_t token_len = strcspn(node, " ");
        if (token_len == iface_len && strncmp(node, iface, token_len) == 0) return true;
        node += token_len;
    }
    return false;
}

/* Parses one data line of /proc/net/dev, extracting the interface name and byte counters. */
static bool parse_net_dev_line(const char* line, char iface[IFACE_NAME_LEN], uint64_t* rx_bytes,
                               uint64_t* tx_bytes) {
    _Static_assert(IFACE_NAME_LEN == 32, "update the %31[^:] width in the sscanf format below");

    while (isspace((unsigned char)*line)) line++;

    unsigned long long rx = 0, tx = 0;
    /* Skip packets, errs, drop, fifo, frame, compressed and multicast between rx and tx bytes. */
    int matched = sscanf(line, "%31[^:]%*[:] %20llu %*s %*s %*s %*s %*s %*s %*s %20llu", iface,
                         &rx, &tx);
    if (matched != 3) return false;

    *rx_bytes = rx;
    *tx_bytes = tx;
    return true;
}

static void build_device_path(char* path, size_t size, const argos_platform_t* platform,
                              int dev_num, const char* prop) {
    snprintf(path, size, "%s%d%s", platform->device_prefix, dev_num, prop);
}

static const argos_platform_t* detect_platform(void) {
    return access(kPlatformQcom.dt_path, F_OK) == 0 ? &kPlatformQcom : &kPlatformExynos;
}

/* Returns the number of boot_device@N nodes, or a negative errno value. */
static int count_dt_devices(const argos_platform_t* platform) {
    DIR* dir = opendir(platform->dt_path);
    if (!dir) {
        int err = errno;
        ALOGE("%s: Fail to open %s: %s (%d)", __func__, platform->dt_path, strerror(err), err);
        return -err;
    }

    /* Only boot_device@N nodes are devices; the kernel ignores any other child node. */
    static const char kNodePrefix[] = "boot_device@";
    const size_t prefix_len = sizeof(kNodePrefix) - 1;

    int count = 0;
    long max_unit_addr = 0;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, kNodePrefix, prefix_len) != 0) continue;

        count++;
        long unit_addr = strtol(ent->d_name + prefix_len, NULL, 10);
        if (unit_addr > max_unit_addr) max_unit_addr = unit_addr;
    }
    closedir(dir);

    if (count < 1) {
        ALOGE("%s: No boot devices found in %s", __func__, platform->dt_path);
        return -ENODEV;
    }

    /*
     * The kernel numbers devices by their order in the device tree, while this daemon uses the
     * unit address, so the two only agree when the nodes are boot_device@1..@N in order.
     */
    if (max_unit_addr != count) {
        ALOGW("%s: boot_device unit addresses are not 1..%d; reports may be attributed to the "
              "wrong device",
              __func__, count);
    }
    return count;
}

static void init_device(const argos_platform_t* platform, int dev_num, argos_device_t* dev) {
    char path[ARGOS_STR_LEN];

    build_device_path(path, sizeof(path), platform, dev_num, ARGOS_PROP_NODE);
    read_line(path, dev->node, sizeof(dev->node));

    build_device_path(path, sizeof(path), platform, dev_num, ARGOS_PROP_LABEL);
    read_line(path, dev->label, sizeof(dev->label));

    build_device_path(path, sizeof(path), platform, dev_num, platform->interval_prop);
    dev->interval_ms = read_interval_ms(path);

    /* Devices without interface names are fed from a single sysfs counter instead. */
    if (dev->node[0] == '\0') {
        build_device_path(path, sizeof(path), platform, dev_num, ARGOS_PROP_SYSNODE);
        if (!read_line(path, dev->sysnode, sizeof(dev->sysnode))) {
            ALOGW("%s: Device %d has neither node nor sysnode", __func__, dev_num);
        }
    }

    dev->count_tx = !ends_with(dev->label, "RX");
    dev->count_rx = !ends_with(dev->label, "TX");

    ALOGD("%s: NODE:%s", __func__, dev->node);
    ALOGD("%s: LABEL:%s", __func__, dev->label);
    ALOGD("%s: INTERVAL:%" PRIu64, __func__, dev->interval_ms);
}

int argos_boot(argos_state_t** out) {
    const argos_platform_t* platform = detect_platform();

    int count = count_dt_devices(platform);
    if (count < 0) return count;

    if (count > ARGOS_MAX_DEVICES) {
        ALOGW("%s: %d boot devices found, only the first %d can be reported", __func__, count,
              ARGOS_MAX_DEVICES);
        count = ARGOS_MAX_DEVICES;
    }

    argos_state_t* state = calloc(1, sizeof(*state));
    argos_device_t* devices = calloc((size_t)count, sizeof(*devices));
    if (!state || !devices) {
        ALOGE("%s: Out of memory", __func__);
        free(state);
        free(devices);
        return -ENOMEM;
    }

    state->net_thru_fd = open(ARGOS_NET_THROUGHPUT_DEV, O_RDWR | O_CLOEXEC);
    if (state->net_thru_fd < 0) {
        int err = errno;
        ALOGE("%s: Device doesn't support qos: %s (%d)", __func__, strerror(err), err);
        free(state);
        free(devices);
        return -err;
    }

    state->platform = platform;
    state->count = count;
    state->devices = devices;
    state->tick_ms = MS_PER_SECOND;

    for (int i = 0; i < count; ++i) {
        init_device(platform, i + 1, &devices[i]);
        if (devices[i].interval_ms < state->tick_ms) state->tick_ms = devices[i].interval_ms;
    }

    ALOGD("%s: MIN INTERVAL:%" PRIu64, __func__, state->tick_ms);

    *out = state;
    return 0;
}

void argos_shutdown(argos_state_t* state) {
    if (!state) return;

    if (state->net_thru_fd >= 0) close(state->net_thru_fd);
    free(state->devices);
    free(state);
}

static void read_sysnode_counters(argos_state_t* state) {
    for (int i = 0; i < state->count; ++i) {
        argos_device_t* dev = &state->devices[i];
        char buf[ARGOS_STR_LEN];

        if (dev->sysnode[0] == '\0') continue;
        if (read_line(dev->sysnode, buf, sizeof(buf))) {
            dev->total_bytes += strtoull(buf, NULL, 10);
            dev->seen = true;
        }
    }
}

static int read_net_dev_counters(argos_state_t* state) {
    FILE* fp = fopen(ARGOS_PROC_NET_DEV, "re");
    if (!fp) {
        int err = errno;
        ALOGE("%s: Fail to open %s: %s (%d)", __func__, ARGOS_PROC_NET_DEV, strerror(err), err);
        return -err;
    }

    char line[NET_DEV_LINE_MAX];
    for (int i = 0; i < NET_DEV_HEADER_LINES; ++i) {
        if (!fgets(line, sizeof(line), fp)) break;
    }

    while (fgets(line, sizeof(line), fp)) {
        char iface[IFACE_NAME_LEN];
        uint64_t rx_bytes, tx_bytes;

        if (!parse_net_dev_line(line, iface, &rx_bytes, &tx_bytes)) continue;

        for (int i = 0; i < state->count; ++i) {
            argos_device_t* dev = &state->devices[i];

            if (!node_has_iface(dev->node, iface)) continue;
            dev->seen = true;
            if (dev->count_tx) dev->total_bytes += tx_bytes;
            if (dev->count_rx) dev->total_bytes += rx_bytes;
        }
    }

    fclose(fp);
    return 0;
}

/* Refreshes total_bytes of every device. */
static int collect_counters(argos_state_t* state) {
    for (int i = 0; i < state->count; ++i) {
        state->devices[i].total_bytes = 0;
        state->devices[i].seen = false;
    }

    read_sysnode_counters(state);
    return read_net_dev_counters(state);
}

/*
 * Tells the kernel the throughput of a device. The payload is "0x<mbps><dev_num>\n" in hex, i.e.
 * the value (mbps << ARGOS_DEV_NUM_BITS) | dev_num that the kernel unpacks into a 1-based device
 * number and a speed in Mbps. It is written including its terminating NUL, as the driver expects.
 */
static void write_throughput(const argos_state_t* state, int dev_num, uint32_t mbps) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "0x%x%x\n", mbps, dev_num);

    if (write(state->net_thru_fd, buf, (size_t)len + 1) < 0) {
        ALOGW("%s: Fail to write throughput: %s (%d)", __func__, strerror(errno), errno);
    }
}

static void report_device(const argos_state_t* state, argos_device_t* dev, int dev_num,
                          uint64_t time_ms) {
    if (time_ms % dev->interval_ms != 0) return;

    /*
     * Counters are cumulative, so a rate needs a previous sample from the same source. It is
     * missing on the first sample (the counter holds everything since boot, which would show up
     * as a bogus burst) and whenever the source vanished or went backwards (interface removed,
     * re-created or reset).
     */
    if (dev->seen && dev->has_baseline && dev->total_bytes >= dev->prev_bytes) {
        uint64_t delta_bytes = dev->total_bytes - dev->prev_bytes;
        uint32_t mbps = (uint32_t)((delta_bytes * 8) / 1000000);

        /* Stay quiet while idle, but still report the drop back to zero. */
        if (mbps != 0 || dev->prev_mbps != 0) {
            write_throughput(state, dev_num, mbps);

            if (state->platform->log_throughput) {
                ALOGD("%s: %s - Mbps:%u prev_tput:%" PRIu64, __func__, dev->label, mbps,
                      dev->prev_mbps);
            }
        }

        dev->prev_mbps = mbps;
    } else if (dev->prev_mbps != 0) {
        /* No usable rate: don't leave the kernel boosted for traffic that no longer exists. */
        write_throughput(state, dev_num, 0);
        dev->prev_mbps = 0;
    }

    dev->has_baseline = dev->seen;
    dev->prev_bytes = dev->total_bytes;
}

int argos_run(argos_state_t* state) {
    if (!state) return -ENODEV;

    uint64_t time_ms = 0;

    for (;;) {
        int ret = collect_counters(state);
        if (ret < 0) return ret;

        for (int i = 0; i < state->count; ++i) {
            report_device(state, &state->devices[i], i + 1, time_ms);
        }

        time_ms = (time_ms + state->tick_ms) % MS_PER_SECOND;
        usleep((useconds_t)(state->tick_ms * 1000));
    }
}

int argos_monitor(void) {
    argos_state_t* state = NULL;

    int ret = argos_boot(&state);
    if (ret < 0) return ret;

    ret = argos_run(state);
    argos_shutdown(state);
    return ret;
}
