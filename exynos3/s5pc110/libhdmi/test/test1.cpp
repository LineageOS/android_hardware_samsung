/*
 * Copyright (C) 2012 Havlena Petr, <havlenapetr@gmail.com>
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

#include "hardware/hdmi.h"

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <linux/videodev2.h>

#include "sec_lcd.h"

#include "test.h"

static void dump_fbs(int count) {
    char name[64];
    char const * const fb_template = {
                "/dev/graphics/fb%u"};

    for(int i = 0; i < count; i++) {
        snprintf(name, 64, fb_template, i);
        int fd = open(name, O_RDWR, 0);
        if(fd < 0) {
            LOGE("%s:: Can't open %s", __func__, name);
            continue;
        }

        struct s3cfb_next_info fb_info;
        int ret = ioctl(fd, S3CFB_GET_CURR_FB_INFO, &fb_info);
        if (ret < 0) {
            LOGE("%s:: ioctl(S3CFB_GET_FB_PHY__ADDR) fail: %d for %s",
                    __func__, ret, name);
            goto close;
        }

        LOGI("%s:: %s addr=0x%08x", __func__, name, fb_info.phy_start_addr);

close:
        close(fd);
    }
}

int main(int argc, char** argv) {
    hw_module_t*   module;
    hdmi_device_t* hdmi;
    int            ret;

    ret = hw_get_module(HDMI_HARDWARE_MODULE_ID,
                (const hw_module_t**)&module);
    if(ret) {
        LOGE("%s:: Hdmi device not presented", __func__);
        goto fail;
    }

    ret = module->methods->open(module, "hdmi-test",
                (hw_device_t **)&hdmi);
    if(ret < 0) {
        LOGE("%s:: Can't open hdmi device", __func__);
        goto fail;
    }

    ret = hdmi->connect(hdmi);
    if(ret < 0) {
        LOGE("%s:: Can't connect hdmi device", __func__);
        goto close;
    }

#if 1
    dump_fbs(5);
#endif

    for(int i = 0; i < 5; i++) {
        LOGI("Blit cycle: %d", i);
        ret = hdmi->blit(hdmi,
                         600,                           /* default lcd width */
                         1024,                           /* default lcd height */
                         HAL_PIXEL_FORMAT_BGRA_8888,    /* our default pixel format */
                         0, 0, 0,                       /* use default frame buffer */
                         0, 0,
                         HDMI_MODE_UI,
                         0);
        if(ret < 0) {
            LOGE("%s:: Can't blit to hdmi device", __func__);
            break;
        }
    }

disconnect:
    if(hdmi->disconnect(hdmi) < 0) {
        LOGE("%s:: Can't disconnect hdmi device", __func__);
    }

close:
    if(hdmi->common.close(&hdmi->common) < 0) {
        LOGE("%s:: Can't close hdmi device", __func__);
    }

fail:
    LOGI("HDMI result: %d", ret);
    return ret;
}
