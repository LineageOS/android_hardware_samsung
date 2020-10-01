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

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include <fimc.h>

#include "test.h"

int main(int argc, char** argv) {
    int         ret;
    s5p_fimc_t  fimc;
    /* fimc src and dest objects */
    sec_img         src_img;
    sec_img         dst_img;
    sec_rect        src_rect;
    sec_rect        dst_rect;
    unsigned int    phyAddr[3];

    memset(&fimc, 0, sizeof(s5p_fimc_t));
    fimc.dev_fd = -1;
    ret = fimc_open(&fimc, "/dev/video2");
    if(ret < 0) {
        LOGE("%s:: Can't open fimc dev[%d]", __func__, ret);
        return ret;
    }

    memset(&src_img, 0, sizeof(src_img));
    memset(&dst_img, 0, sizeof(src_img));
    memset(&src_rect, 0, sizeof(src_rect));
    memset(&dst_rect, 0, sizeof(src_rect));
    memset(&phyAddr, 0, sizeof(int) * sizeof(phyAddr));

    phyAddr[0] = 0/*srcYAddr*/;
    phyAddr[1] = 0/*srcCbAddr*/;
    phyAddr[2] = 0/*srcCrAddr*/;

    src_img.w       = 600;
    src_img.h       = 1024;
    src_img.format  = HAL_PIXEL_FORMAT_YCbCr_420_SP;
    src_img.base    = 0;
    src_img.offset  = 0;
    src_img.mem_id  = 0;
    src_img.mem_type = FIMC_MEM_TYPE_PHYS;
    src_img.w       = (src_img.w + 15) & (~15);
    src_img.h       = (src_img.h + 1)  & (~1) ;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = src_img.w;
    src_rect.h = src_img.h;

    dst_img.w       = 600;
    dst_img.h       = 1024;
    dst_img.format  = HAL_PIXEL_FORMAT_YCbCr_420_SP;
    dst_img.base    = (unsigned int) fimc.out_buf.phys_addr;
    dst_img.offset  = 0;
    dst_img.mem_id  = 0;
    dst_img.mem_type = FIMC_MEM_TYPE_PHYS;

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.w = dst_img.w;
    dst_rect.h = dst_img.h;

    LOGI("%s::sr_x %d sr_y %d sr_w %d sr_h %d dr_x %d dr_y %d dr_w %d dr_h %d ",
          __func__, src_rect.x, src_rect.y, src_rect.w, src_rect.h,
          dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h);

    for(int i = 0; i < 5 && ret == 0; i++) {
        ret = fimc_flush(&fimc, &src_img, &src_rect, &dst_img, &dst_rect,
                         phyAddr, 0);
        if(ret < 0) {
            LOGE("%s:: Can't flush to fimc dev[%d]", __func__, ret);
        }
    }

    fimc_close(&fimc);
    return ret;
}
