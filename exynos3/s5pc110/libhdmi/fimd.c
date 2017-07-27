/*
* Copyright@ Samsung Electronics Co. LTD
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/vt.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <cutils/log.h>

#include "fimd.h"

int fb_open(int win)
{
    char node[20];
    int fp = -1;

    sprintf(node, "%s%d", PFX_NODE_FB, win);

    fp = open(node, O_RDWR);
    if (fp < 0)
        ALOGE("%s: fb[%d] open failed", __func__, win);

    return fp;
}

int fb_close(int fp)
{
    if (fp)
        close(fp);
    else
        ALOGE("%s: fb is not allocated %d", __func__, fp);

    return 0;
}

int fb_get_fscreeninfo(int fp, struct fb_fix_screeninfo *fix)
{
    int ret = -1;

    ret = ioctl(fp, FBIOGET_FSCREENINFO, fix);
    if (ret)
        ALOGE("%s: FBIOGET_FSCREENINFO failed", __func__);

    return ret;
}

int fb_get_vscreeninfo(int fp, struct fb_var_screeninfo *var)
{
    int ret = -1;

    ret = ioctl(fp, FBIOGET_VSCREENINFO, var);
    if (ret)
        ALOGE("%s:: FBIOGET_VSCREENINFO failed", __func__);

    return ret;
}

int fb_put_vscreeninfo(int fp, struct fb_var_screeninfo *var)
{
    int ret = -1;

    ret = ioctl(fp, FBIOPUT_VSCREENINFO, var);
    if (ret)
        ALOGE("%s:: FBIOPUT_VSCREENINFO failed", __func__);

    return ret;
}

char* fb_mmap(int fp, __u32 size)
{
    char *buffer;

    buffer = (char *)mmap(0, size, PROT_READ | PROT_WRITE,
                  MAP_SHARED, fp, 0);
    if (!buffer) {
        ALOGE("%s:: mmap failed", __func__);
        return NULL;
    }

    return buffer;
}

int fb_ioctl(int fp, __u32 cmd, void *arg)
{
    int ret = -1;

    ret = ioctl(fp, cmd, arg);
    if (ret < 0)
        ALOGE("%s:: ioctl (%d) failed", __func__, cmd);

    return ret;
}

int fb_on(int fp)
{
    int ret = -1;

    ret = ioctl(fp, FBIOBLANK, FB_BLANK_UNBLANK);
    if (ret)
        ALOGE("%s:: FBIOBLANK failed", __func__);

    return ret;
}

int fb_off(int fp)
{
    int ret = -1;

    ret = ioctl(fp, FBIOBLANK, FB_BLANK_POWERDOWN);
    if (ret)
        ALOGE("%s:: FBIOBLANK failed", __func__);

    return ret;
}

int fb_off_all()
{
    int fp, i;

    for (i = 0; i < TOTAL_FB_NUM; i++) {
        fp = fb_open(i);
        if (fp < 0)
            return -1;

        if (ioctl(fp, FBIOBLANK, FB_BLANK_POWERDOWN) < 0)
            ALOGE("%s:: FBIOBLANK failed", __func__);

        fb_off(fp);
        fb_close(fp);
    }

    return 0;
}

char* fb_init_display(int fp, int width, int height, int left_x, int top_y, int bpp)
{
    struct fb_var_screeninfo var;
    struct s5ptvfb_user_window window;
    int fb_size;
    char *fb = NULL;

    var.xres = width;
    var.yres = height;
    var.bits_per_pixel = bpp;
    window.x = left_x;
    window.y = top_y;

    var.xres_virtual = var.xres;
    var.yres_virtual = var.yres;
    var.xoffset = 0;
    var.yoffset = 0;
    var.width = 0;
    var.height = 0;
    var.transp.length = 0;
    var.activate = FB_ACTIVATE_FORCE;
    fb_size = var.xres_virtual * var.yres_virtual * bpp / 8;

    /* FBIOPUT_VSCREENINFO should be first */
    fb_put_vscreeninfo(fp, &var);
    fb_ioctl(fp, S5PTVFB_WIN_POSITION, &window);

    /* draw image */
    fb = fb_mmap(fb_size, fp);
    memset(fb, 0x0, fb_size);

    return fb;
}

#if 0

static int get_bytes_per_pixel(int bits_per_pixel)
{
    return (bits_per_pixel == 24 || bits_per_pixel == 25 ||
        bits_per_pixel == 28) ? 4 : bits_per_pixel / 8;
}

int simple_draw(char *dest, const char *src, int img_width,
        struct fb_var_screeninfo *var)
{
    int bytes_per_pixel = get_bytes_per_pixel(var->bits_per_pixel);
    unsigned int y;

    for (y = 0; y < var->yres; y++)
        memcpy(dest + y * var->xres * bytes_per_pixel,
               src + y * img_width * bytes_per_pixel,
               var->xres * bytes_per_pixel);

    return 0;
}

int draw(char *dest, const char *src, int img_width,
     struct fb_var_screeninfo *var)
{
    int bytes_per_pixel = get_bytes_per_pixel(var->bits_per_pixel);
    unsigned int y;

    if (var->bits_per_pixel == 16) {
        memcpy(dest, src, var->xres * var->yres * 2);
    } else {
        for (y = 0; y < var->yres; y++)
            memcpy(dest + y * var->xres * bytes_per_pixel,
                   src + y * img_width * bytes_per_pixel,
                   var->xres * bytes_per_pixel);
    }

    return 0;
}
#endif
