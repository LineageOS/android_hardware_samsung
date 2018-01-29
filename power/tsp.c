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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <log/log.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "util.h"
#include "tsp.h"

#define TSP_PATH "/sys/class/sec/tsp/"

#define TSP_CMD_EXEC_FILE "cmd"
#define TSP_CMD_LIST_FILE "cmd_list"
#define TSP_CMD_RES_FILE "cmd_result"
#define TSP_CMD_STATE_FILE "cmd_status"

#define TSP_CMD_LEN 64
/* cmd_result is of the format 'command:result' */
#define TSP_CMD_RES_LEN ((TSP_CMD_LEN)+8)
/* aod_enable <1|0> */
#define TSP_CMD_DT2W_CTL_PREFIX "aod_enable"
#define TSP_CMD_DT2W_CTL "aod_enable,%d"
/* set_aod_rect <width> <height> <x> <y> */
#define TSP_CMD_DT2W_RECT_PREFIX "set_aod_rect"
#define TSP_CMD_DT2W_RECT "set_aod_rect,%d,%d,%d,%d"
/* get_max_x */
#define TSP_CMD_MAX_X "get_max_x"
/* get_max_y */
#define TSP_CMD_MAX_Y "get_max_y"

#define CMD_STATE_LEN 16
#define CMD_STATE_NA "NOT_APPLICABLE"
#define CMD_STATE_WAITING "WAITING"
#define CMD_STATE_RUNNING "RUNNING"
#define CMD_STATE_OK "OK"
#define CMD_STATE_FAIL "FAIL"

/* We cache values from the TSP */
static int tsp_width = -1;
static int tsp_height = -1;

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
        if (strncmp(line, cmd, strlen(cmd)) == 0) {
            ALOGE("has cmd: %s", cmd);
            found = true;
            break;
        }
    }

    if (!found)
        ALOGE("not has cmd: %s", cmd);

    free(line);
    fclose(fp);
    return found;
}

static bool tsp_wait_for_cmd(const char *cmd) {
    char buf[CMD_STATE_LEN];
    int count = 0;

    while (count++ < 10) {
        if (sysfs_read(TSP_PATH TSP_CMD_STATE_FILE, buf, CMD_STATE_LEN) < 0) {
            ALOGE("%s: sysfs read failed", cmd);
            return false;
        }

        ALOGE("state: %s - want %s", buf, CMD_STATE_OK);
        /* TSP enters "ok" state once command result is ready to be read */
        if (!strncmp(buf, CMD_STATE_OK, strlen(CMD_STATE_OK)))
            return true;
        /* some commands skip the OK state and go straight to waiting. */
        if (!strncmp(buf, CMD_STATE_WAITING, strlen(CMD_STATE_WAITING)))
            return true;

        usleep(100);
    }

    ALOGE("%s: TSP timeout expired!", cmd);
    /* FIXME: if the TSP ends up in a weird state,
     * we should read the result file to cause it to
     * exit the state */
    return false;
}

char *tsp_get_result(void) {
    char inbuf[TSP_CMD_RES_LEN];
    char *res;
    char *retbuf;

    if (sysfs_read(TSP_PATH TSP_CMD_RES_FILE, inbuf, TSP_CMD_RES_LEN) < 0) {
        ALOGI("tsp get result failed");
        return NULL;
    }

    ALOGI("Got result: %s", inbuf);

    if ((res = strstr(inbuf, ":")) == NULL) {
        ALOGI("no strstr");
        return NULL;
    }

    retbuf = calloc(strlen(res)+1, sizeof(char));
    if (retbuf == NULL)
        return NULL;

    strcpy(retbuf, res+1);

    ALOGE("res: %s", retbuf);

    return retbuf;
}


bool tsp_doubletap_init(void)
{
    char *res;
    /* all of these commands must be present for dt2w to work */
    if (!check_tsp_has_cmd(TSP_CMD_DT2W_CTL_PREFIX) || !check_tsp_has_cmd(TSP_CMD_DT2W_RECT_PREFIX)
        || !check_tsp_has_cmd(TSP_CMD_MAX_X) || !check_tsp_has_cmd(TSP_CMD_MAX_Y)) {
        ALOGE("not has all cmds");
        return false;
    }

    sysfs_write(TSP_PATH TSP_CMD_EXEC_FILE, TSP_CMD_MAX_X);
    if (!tsp_wait_for_cmd(TSP_CMD_MAX_X)) {
        return false;
    }

    res = tsp_get_result();
    if (res == NULL)
        return false;

    tsp_width = atoi(res);
    free(res);
    if (!tsp_width)
        return false;

    ALOGE("read max Y");
    sysfs_write(TSP_PATH TSP_CMD_EXEC_FILE, TSP_CMD_MAX_Y);
    if (!tsp_wait_for_cmd(TSP_CMD_MAX_X))
        return false;

    res = tsp_get_result();
    if (res == NULL)
        return false;

    tsp_height = atoi(res);
    free(res);
    if (!tsp_height)
        return false;

    ALOGE("tsp height: %d, tsp width: %d", tsp_height, tsp_width);
    return true;
}

bool tsp_doubletap_set_region(void)
{
    char buf[TSP_CMD_LEN];
    /* set DT2W region */
    snprintf(buf, TSP_CMD_LEN, TSP_CMD_DT2W_RECT, tsp_width, tsp_height, 0, 0);
    sysfs_write(TSP_PATH TSP_CMD_EXEC_FILE, buf);
    if (!tsp_wait_for_cmd("set_aod_region"))
        return false;
    return true;
}

bool tsp_set_doubletap(bool state)
{
    char buf[TSP_CMD_LEN];
    /* set DT2W en/disable */
    snprintf(buf, TSP_CMD_LEN, TSP_CMD_DT2W_CTL, !!state);
    ALOGE("%s", buf);
    sysfs_write(TSP_PATH TSP_CMD_EXEC_FILE, buf);
    if (!tsp_wait_for_cmd("enable_aod"))
        return false;

    ALOGV("dt2w %sed", state ? "enabl" : "disabl");
    return true;
}
