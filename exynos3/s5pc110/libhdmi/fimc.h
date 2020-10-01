/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef _FIMC_LIB_
#define _FIMC_LIB_

#include "s5p_fimc.h"
#include "sec_utils.h"

#define MAX_RESIZING_RATIO_LIMIT  (63)

enum {
    FIMC_MEM_TYPE_UNKNOWN = 0,
    FIMC_MEM_TYPE_PHYS,
    FIMC_MEM_TYPE_VIRT,
};

#ifdef __cplusplus
extern "C" {
#endif

struct sec_rect {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
};
    
struct sec_img {
    uint32_t w;
    uint32_t h;
    uint32_t format;
    uint32_t base;
    uint32_t offset;
    int      mem_id;
    int      mem_type;
};
    
inline int SEC_MIN(int x, int y) {
    return ((x < y) ? x : y);
}
    
inline int SEC_MAX(int x, int y) {
    return ((x > y) ? x : y);
}

int     fimc_open(s5p_fimc_t *fimc, const char* dev);

void    fimc_close(s5p_fimc_t *fimc);

int     fimc_flush(s5p_fimc_t *fimc,
                   struct sec_img *src_img,
                   struct sec_rect *src_rect,
                   struct sec_img *dst_img,
                   struct sec_rect *dst_rect,
                   unsigned int *phyAddr,
                   uint32_t transform);

#ifdef __cplusplus
}
#endif

#endif // end of _FIMC_LIB_
