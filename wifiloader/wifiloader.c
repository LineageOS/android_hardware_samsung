/*
 * Copyright (c) 2015      Andreas Schneider <asn@cryptomilk.org>
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

#define LOG_TAG "wifiloader"
#define LOG_NDEBUG 0

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <cutils/log.h>

#define DEFERRED_INITCALLS "/proc/deferred_initcalls"

int main(void)
{
    char buf[8] = { '\0' };
    FILE *fp;
    size_t r;

    ALOGD("Trigger initcall of deferred modules\n");

    fp = fopen(DEFERRED_INITCALLS, "r");
    if (fp == NULL) {
        ALOGE("Failed to open %s - error: %s\n",
              DEFERRED_INITCALLS,
              strerror(errno));
        return -errno;
    }

    r = fread(buf, sizeof(buf), 1, fp);
    fclose(fp);

    ALOGV("%s=%s\n", DEFERRED_INITCALLS, buf);

    return 0;
}
