/*
 * Copyright (C) 2015 The NamelessRom Project
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

#include "hwcomposer.h"
#include "hwcomposer_vsync.h"

#include <cutils/iosched_policy.h>
#include <cutils/threads.h>

#include <utils/threads.h>

#include <sys/prctl.h>

/*****************************************************************************/

#define HWC_VSYNC_THREAD_NAME "hwcVsyncThread"

#define VSYNC_TIME_PATH "/sys/devices/platform/samsung-pd.2/s3cfb.0/vsync_time"

/*****************************************************************************/

static void *hwc_vsync_thread(void *data)
{
    static char buf[4096];
    fd_set exceptfds;
    int res;
    int64_t timestamp = 0;
    hwc_context_t* ctx = (hwc_context_t *)(data);

    // open the file descriptor for the vsync timestamp
    ctx->vsync_timestamp_fd = open(VSYNC_TIME_PATH, O_RDONLY);

    // set up the thread
    char thread_name[64] = HWC_VSYNC_THREAD_NAME;
    prctl(PR_SET_NAME, (unsigned long) &thread_name, 0, 0, 0);
    androidSetThreadPriority(0, HAL_PRIORITY_URGENT_DISPLAY);
    android_set_rt_ioprio(0, 1);

    memset(buf, 0, sizeof(buf));

    FD_ZERO(&exceptfds);
    FD_SET(ctx->vsync_timestamp_fd, &exceptfds);
    do {
        ssize_t len = read(ctx->vsync_timestamp_fd, buf, sizeof(buf));
        timestamp = strtoull(buf, NULL, 0);
        if (ctx->procs) {
            if (DEBUG_VSYNC) {
                ALOGD("%s: handling vsync, timestamp:%lld", __FUNCTION__, timestamp);
            }
            ctx->procs->vsync(ctx->procs, 0, timestamp);
        }
        select(ctx->vsync_timestamp_fd + 1, NULL, NULL, &exceptfds, NULL);
        lseek(ctx->vsync_timestamp_fd, 0, SEEK_SET);
    } while (1);

    return NULL;
}

void init_vsync_thread(hwc_context_t* ctx)
{
    int ret;

    ALOGD("Initializing VSYNC Thread: " HWC_VSYNC_THREAD_NAME);

    ret = pthread_create(&ctx->vsync_thread, NULL, hwc_vsync_thread, (void*) ctx);
    if (ret) {
        ALOGE("%s: failed to create %s: %s", __FUNCTION__,
              HWC_VSYNC_THREAD_NAME, strerror(ret));
    }
}

void close_vsync_thread(hwc_context_t* ctx)
{
    pthread_kill(ctx->vsync_thread, SIGTERM);
    pthread_join(ctx->vsync_thread, NULL);
    close(ctx->vsync_timestamp_fd);
}
