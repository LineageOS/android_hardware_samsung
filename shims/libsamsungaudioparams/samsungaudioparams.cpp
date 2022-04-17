/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <android-base/logging.h>
#include <cutils/str_parms.h>

extern "C" {
int str_parms_get_mod(struct str_parms* str_parms, const char* key, int* val) {
    if (!strcmp("g_sco_samplerate", key)) {
        char wb[4] = "off";
        if (str_parms_get_str(str_parms, "bt_wbs", wb, sizeof(wb)) != -ENOENT) {
            LOG(INFO) << __func__ << ": Overriding " << key << " based on bt_wbs=" << wb;
            *val = !strcmp("on", wb) ? 16000 : 8000;
            return 0;
        } else {
            return -ENOENT;
        }
    }
    return str_parms_get_int(str_parms, key, val);
}
}
