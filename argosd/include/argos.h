/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define LOG_TAG "ARGOS"

#include <errno.h>
#include <log/log.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Qualcomm paths */
#define ARGOS_DT_PATH_QCOM "/proc/device-tree/soc/argos"
#define ARGOS_BOOT_DEVICE_PREFIX_QCOM "/proc/device-tree/soc/argos/boot_device@"
#define ARGOS_INTERVAL_PROP_QCOM "/net_boost,interval_idx"

/* Exynos paths */
#define ARGOS_DT_PATH_EXYNOS "/proc/device-tree/argos"
#define ARGOS_BOOT_DEVICE_PREFIX_EXYNOS "/proc/device-tree/argos/boot_device@"
#define ARGOS_INTERVAL_PROP_EXYNOS "/net_boost,interval"

#define ARGOS_NET_THROUGHPUT_DEV "/dev/network_throughput"
#define ARGOS_PROC_NET_DEV "/proc/net/dev"

#define ARGOS_STR_LEN 128

typedef struct argos_device_s {
    char node[ARGOS_STR_LEN];
    char label[ARGOS_STR_LEN];
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t sysnode_val;
    uint64_t delta_bytes;
    uint64_t prev_bytes;
    uint64_t total_bytes;
    uint64_t prev_mbps;
    uint64_t interval;
} argos_device_t;

/* Backward compatibility typedef */
typedef argos_device_t argos_data_t;

typedef struct argos_state_s {
    int count;
    int net_thru_fd;
    uint64_t min_interval;
    argos_device_t* devices;
    const char* prefix;
    int is_exynos;
} argos_state_t;

int argos_monitor(void);
argos_state_t* argos_boot(void);
void argos_shutdown(argos_state_t* state);
int argos_run(argos_state_t* state);
