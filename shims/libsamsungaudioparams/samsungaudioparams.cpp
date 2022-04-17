/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <android-base/logging.h>
#include <cutils/str_parms.h>

extern "C" {
int str_parms_get_mod(struct str_parms* str_parms, const char* key, char* val, int len) {
    if (!strcmp("g_sco_samplerate", key)) {
        char wb[4] = "off";
        if (str_parms_get_str(str_parms, "bt_wbs", wb, 4) != -ENOENT) {
            LOG(INFO) << __func__ << ": Overriding " << key << " based on bt_wbs=" << wb;
            return strlcpy(val, !strcmp("on", wb) ? "16000" : "8000", len);
        } else {
            return -ENOENT;
        }
    }

    return str_parms_get_str(str_parms, key, val, len);
}
}
