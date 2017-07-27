/*
 * Copyright 2011, Havlena Petr <havlenapetr@gmail.com>
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

#define LOG_NDEBUG 0
#include <cutils/log.h>

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <linux/fb.h>

#include "hardware/hdmi.h"

#include "SecHDMI.h"

using namespace android;

#define RETURN_EINVAL_IF(hw)                                            \
    if(!hw) {                                                           \
        ALOGE("%s: %i - Can't obtain hw driver!", __func__, __LINE__);  \
        return -EINVAL;                                                 \
    }

struct sec_hdmi_device_t {
    hdmi_device_t   base;
    /* Sec specific "private" data can go here (base.priv) */
    SecHDMI*        hw;
    int             lcd_width;
    int             lcd_height;
};

static SecHDMI* sec_obtain_hw(struct hdmi_device_t* device)
{
    if(!device) {
        ALOGE("Can't obtain hdmi base device!");
        return NULL;
    }

    struct sec_hdmi_device_t* dev =
            (struct sec_hdmi_device_t *) device;
    if(!dev) {
        ALOGE("Can't obtain SEC hdmi device!");
        return NULL;
    }

    return dev->hw;
}

static int hdmi_connect(struct hdmi_device_t* dev)
{
    ALOGV("connect is called");

    SecHDMI* hw = sec_obtain_hw(dev);
    RETURN_EINVAL_IF(hw);

    return hw->connect();
}

static int hdmi_disconnect(struct hdmi_device_t* dev)
{
    ALOGV("disconnect is called");

    SecHDMI* hw = sec_obtain_hw(dev);
    RETURN_EINVAL_IF(hw);

    return hw->disconnect();
}

static int hdmi_clear(struct hdmi_device_t* dev, int hdmiLayer)
{
    ALOGV("clear is called");

    SecHDMI* hw = sec_obtain_hw(dev);
    RETURN_EINVAL_IF(hw);

    return 0/*hw->clear(hdmiLayer) ? 0 : -1*/;
}

static int hdmi_blit(struct hdmi_device_t* dev, int srcW, int srcH, int srcColorFormat,
                        uint32_t srcYAddr, uint32_t srcCbAddr, uint32_t srcCrAddr,
                        int dstX, int dstY,
                        int layer,
                        int num_of_hwc_layer)
{
    ALOGV("blit is called");

    SecHDMI* hw = sec_obtain_hw(dev);
    RETURN_EINVAL_IF(hw);

    return hw->flush(srcW, srcH, srcColorFormat, srcYAddr, srcCbAddr, srcCrAddr,
                     dstX, dstY, layer, num_of_hwc_layer);
}

static int hdmi_close(struct hdmi_device_t *dev)
{
    ALOGV("close is called");

    if (!dev) {
        return 0;
    }

    SecHDMI* hw = sec_obtain_hw(dev);
    if(hw) {
        hw->destroy();
        delete hw;
    }

    free(dev);

    return 0;
}

static int hdmi_get_lcd_size(int* width, int* height)
{
    char const * const device_template[] = {
                "/dev/graphics/fb%u",
                "/dev/fb%u",
                0 };

    int fd = -1;
    char name[64];

    for(int i = 0; fd < 0 && device_template[i]; i++) {
        snprintf(name, 64, device_template[i], 0);
        fd = open(name, O_RDWR, 0);
    }

    if (fd < 0) {
        return -1;
    }

    struct fb_var_screeninfo info;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) < 0) {
        close(fd);
        return -2;
    }

    *width  = info.xres;
    *height = info.yres;
    
    close(fd);
    return 0;
}

static int hdmi_open(const struct hw_module_t *module, char const *name,
                         struct hw_device_t **device)
{
    int lcdWidth, lcdHeight;

    ALOGV("open: open with %s", name);

    if (strcmp("hdmi-test", name) &&
        strcmp("hdmi-service", name) &&
        strcmp("hdmi-composer", name)) {
        return -EINVAL;
    }

    if(hdmi_get_lcd_size(&lcdWidth, &lcdHeight) < 0) {
        return -EINVAL;
    }

    struct sec_hdmi_device_t *hdmi_dev =
           (struct sec_hdmi_device_t *) malloc(sizeof(struct sec_hdmi_device_t));
    if(!hdmi_dev) {
        return -ENOMEM;
    }
    memset(hdmi_dev, 0, sizeof(*hdmi_dev));

    hdmi_dev->base.common.tag = HARDWARE_DEVICE_TAG;
    hdmi_dev->base.common.version = 0;
    hdmi_dev->base.common.module = (struct hw_module_t *)module;
    hdmi_dev->base.common.close = (int (*)(struct hw_device_t *))hdmi_close;
    hdmi_dev->base.connect = hdmi_connect;
    hdmi_dev->base.disconnect = hdmi_disconnect;
    hdmi_dev->base.clear = hdmi_clear;
    hdmi_dev->base.blit = hdmi_blit;
    hdmi_dev->lcd_width = lcdWidth;
    hdmi_dev->lcd_height = lcdHeight;

    *device = &hdmi_dev->base.common;

    hdmi_dev->hw = new SecHDMI();
    if(hdmi_dev->hw->create(lcdWidth, lcdHeight) < 0) {
        hdmi_close((hdmi_device_t *)hdmi_dev);
        return -EINVAL;
    }

    ALOGI("initzialized for lcd size: %dx%d", lcdWidth, lcdHeight);

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    open: hdmi_open,
};

extern "C" {
    struct hw_module_t HAL_MODULE_INFO_SYM = {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: HDMI_HARDWARE_MODULE_ID,
        name: "Samsung S5PC11X hdmi module",
        author: "Havlena Petr <havlenapetr@gmail.com>",
        methods: &hal_module_methods,
    };
}
