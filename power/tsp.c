/*
 * Copyright (C) 2018 The LineageOS Project
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
#define LOG_TAG "SamsungPowerHAL-TSP"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <log/log.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "util.h"
#include "tsp.h"

#define TSP_PATH "/sys/class/sec/tsp/"

#define TSP_CMD_EXEC_FILE "cmd"
#define TSP_CMD_LIST_FILE "cmd_list"
#define TSP_CMD_STATE_FILE "cmd_status"

#define TSP_CMD_LEN 64
/* aod_enable <1|0> */
#define TSP_CMD_DT2W_CTL "aod_enable %d"
/* set_aod_rect <width> <height> <x> <y> */
#define TSP_CMD_DT2W_RECT "set_aod_rect %d %d %d %d"

#define CMD_STATE_LEN 16
#define CMD_STATE_NA "NOT_APPLICABLE"
#define CMD_STATE_WAITING "WAITING"
#define CMD_STATE_RUNNING "RUNNING"
#define CMD_STATE_OK "OK"
#define CMD_STATE_FAIL "FAIL"

static bool check_tsp_has_cmd(const char *cmd)
{
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    bool found = false;
    FILE *fp = fopen(TSP_PATH TSP_CMD_LIST_FILE, "r");
    if (fp == NULL) {
        ALOGE("Failed to open TSP command list: %s", strerror(errno));
        return false;
    }

    while ((nread = getline(&line, &len, fp)) != -1) {
        if (strncmp(line, TSP_CMD_DT2W_CTL, strlen(TSP_CMD_DT2W_CTL)) == 0) {
            found = true;
            break;
        }
    }

    free(line);
    fclose(fp);
    return found;
}

static bool tsp_wait_for_cmd(const char *cmd) {
    char buf[CMD_STATE_LEN];
    int count = 0;

    while (count++ < 10) {
        if (sysfs_read(TSP_PATH TSP_CMD_STATE_FILE, buf, CMD_STATE_LEN) < 0) {
            return false;
        }

        if (strncmp(buf, CMD_STATE_RUNNING, strlen(CMD_STATE_RUNNING)) == 0 ||
                strncmp(buf, CMD_STATE_WAITING, strlen(CMD_STATE_WAITING)) == 0) {
            usleep(100);
        } else if (strncmp(buf, CMD_STATE_OK, strlen(CMD_STATE_OK))) {
            return true;
        } else if (strncmp(buf, CMD_STATE_FAIL, strlen(CMD_STATE_FAIL))) {
            ALOGE("%s: command failed!", cmd);
            return false;
        } else {
            ALOGE("%s: TSP has no pending command!", cmd);
            return false;
        }
    }

    ALOGE("%s: TSP timeout expired!", cmd);
    return false;
}

bool tsp_has_doubletap(void)
{
    return check_tsp_has_cmd(TSP_CMD_DT2W_CTL) && check_tsp_has_cmd(TSP_CMD_DT2W_RECT);
}

bool tsp_set_doubletap(int state, int width, int height, int x, int y)
{
    char buf[TSP_CMD_LEN];
    if (state) {
        /* set DT2W region */
        snprintf(buf, TSP_CMD_LEN, TSP_CMD_DT2W_RECT, width, height, x, y);
        sysfs_write(TSP_PATH TSP_CMD_EXEC_FILE, buf);
        if (!tsp_wait_for_cmd())
            return false;
    }

    /* set DT2W en/disable */
    snprintf(buf, TSP_CMD_LEN, TSP_CMD_DT2W_CTL, !!state);
    sysfs_write(TSP_PATH TSP_CMD_EXEC_FILE, buf);
    if (!tsp_wait_for_cmd())
        return false;

    ALOGV("dt2w %sed", state ? "enabl" : "disabl");
}
