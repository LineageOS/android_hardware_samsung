/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define LOG_TAG "ARGOS"

#include <log/log.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARGOS_STR_LEN 128

#define ARGOS_DEV_NUM_BITS 4
#define ARGOS_MAX_DEVICES ((1 << ARGOS_DEV_NUM_BITS) - 1)

typedef struct argos_platform_s {
    const char* name;
    const char* dt_path;        /* node containing the boot_device@N children */
    const char* device_prefix;  /* path prefix of boot_device@; "N" is appended */
    const char* interval_prop;  /* "/<property>" below boot_device@N */
    bool log_throughput;        /* log every throughput update written to the kernel */
} argos_platform_t;

typedef struct argos_device_s {
    char node[ARGOS_STR_LEN];     /* space-separated interface names; empty => use sysnode */
    char label[ARGOS_STR_LEN];    /* human readable name, may end in "RX" or "TX" */
    char sysnode[ARGOS_STR_LEN];  /* sysfs counter file, only used when node is empty */
    bool count_rx;                /* include received bytes in the total */
    bool count_tx;                /* include transmitted bytes in the total */
    uint64_t interval_ms;         /* sampling interval */
    bool seen;                    /* a counter source was readable in the current sample */
    bool has_baseline;            /* prev_bytes comes from a valid earlier sample */
    uint64_t total_bytes;         /* byte counter observed in the current sample */
    uint64_t prev_bytes;          /* byte counter at the previous interval boundary */
    uint64_t prev_mbps;           /* throughput reported at the previous interval boundary */
} argos_device_t;

typedef struct argos_state_s {
    const argos_platform_t* platform;
    int net_thru_fd;
    int count;
    uint64_t tick_ms;  /* smallest device interval; the loop wakes up this often */
    argos_device_t* devices;
} argos_state_t;

int argos_monitor(void);
int argos_boot(argos_state_t** out);
void argos_shutdown(argos_state_t* state);
int argos_run(argos_state_t* state);
