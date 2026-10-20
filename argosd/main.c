/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "argos.h"

int main(void) {
    ALOGD("%s: Argos Daemon Boot", __func__);

    int ret = argos_monitor();
    if (ret < 0) {
        ALOGE("%s: Fail to run traffic monitor: %s (%d)", __func__, strerror(-ret), ret);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
