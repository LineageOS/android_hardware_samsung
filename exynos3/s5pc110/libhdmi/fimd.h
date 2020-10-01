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

#ifndef __FIMD_H__
#define __FIMD_H__

#include <linux/fb.h>

#include <s5p_tvout.h>

#define TOTAL_FB_NUM 5

#ifdef __cplusplus
extern "C" {
#endif

int     fb_open(int win);
int     fb_close(int fp);
int     fb_on(int fp);
int     fb_off(int fp);
int     fb_off_all(void);
char*   fb_init_display(int fp, int width, int height,
            int left_x, int top_y, int bpp);
int     fb_ioctl(int fp, __u32 cmd, void *arg);
char*   fb_mmap(int fp, __u32 size);
int     fb_get_fscreeninfo(int fp, struct fb_fix_screeninfo *fix);
int     fb_get_vscreeninfo(int fp, struct fb_var_screeninfo *var);
int     fb_put_vscreeninfo(int fp, struct fb_var_screeninfo *var);

#if 0
int     simple_draw(char *dest, const char *src,
            int img_width, struct fb_var_screeninfo *var);
int     draw(char *dest, const char *src,\
            int img_width, struct fb_var_screeninfo *var);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __FIMD_H__ */
