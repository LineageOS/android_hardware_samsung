/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argos.h"

int main() {
    ALOGD("argos: %s: Argos Daemon Boot\n", __func__);

    int ret = argos_monitor();
    if (ret < 0) {
        ALOGE("argos: (ERR! %s) %s: Fail to run traffic monitor with(%d)\n", strerror(errno),
              __func__, ret);
    }

    return 0;
}
