/* Copyright (c) 2011 Samsung Electronics Co, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 *

 * Alternatively, Licensed under the Apache License, Version 2.0 (the "License");
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


#ifndef __SAMSUNG_SYSLSI_APDEV_FIMCLIB_H__
#define __SAMSUNG_SYSLSI_APDEV_FIMCLIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/fb.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <hardware/hardware.h>

#include "utils/Timers.h"

#include "s5p_fimc.h"
#include "sec_utils.h"
#include "sec_format.h"
#include "SecBuffer.h"
#include "SecRect.h"


#define PFX_NODE_FIMC   "/dev/video"

int fimc_switch_color_format(int color_space);

#ifdef __cplusplus
}

class SecFimc
{
public:
    enum FIMC_DEV
    {
        FIMC_DEV0   = 0,
        FIMC_DEV1,
        FIMC_DEV2,
        FIMC_DEV3,
    };
private:
    bool                        mFlagCreate;
    FIMC_DEV                    mFimcDev;
    fimc_overlay_mode           mFimcOvlyMode;
    struct v4l2_capability      mFimcCap;
    unsigned int                mFimcRrvedPhysMemAddr;
    unsigned int                mBufNum;
    unsigned int                mBufIndex;
    s5p_fimc_t                  mS5pFimc;
    unsigned int                mRotVal;
    bool                        mFlagGlobalAlpha;
    int                         mGlobalAlpha;
    bool                        mFlagLocalAlpha;
    bool                        mFlagColorKey;
    int                         mColorKey;
    bool                        mFlagSetSrcParam;
    bool                        mFlagSetDstParam;
    bool                        mFlagStreamOn;

public:
    SecFimc();
    virtual ~SecFimc();
    bool        create(FIMC_DEV fimc_dev, fimc_overlay_mode fimc_mode, unsigned int buf_num);
    bool        destroy(void);
    bool        flagCreate(void);

    int         getSecFimcFd();
//    bool        createWithFd(int fd, FIMC_DEV fimc_dev);
    int         getFimcRsrvedPhysMemAddr();
    int         getFimcVersion();

    bool        checkFimcSrcSize(unsigned int width, unsigned int height, 
            unsigned int cropX, unsigned int cropY, unsigned int* cropWidth, unsigned int* cropHeight, 
            int colorFormat, bool forceChange = false);

    bool        checkFimcDstSize(unsigned int width, unsigned int height, 
            unsigned int cropX, unsigned int cropY, unsigned int* cropWidth, unsigned int* cropHeight, 
            int colorFormat, int rotVal, bool forceChange = false);

    bool        setSrcParams(unsigned int width, unsigned int height, 
            unsigned int cropX, unsigned int cropY, unsigned int* cropWidth, unsigned int* cropHeight, 
            int colorFormat, bool forceChange = true);

    bool        getSrcParams(unsigned int* width, unsigned int* height, 
            unsigned int* cropX, unsigned int* cropY, unsigned int* cropWidth, unsigned int* cropHeight, 
            int* colorFormat);

    bool        setSrcPhyAddr(unsigned int physYAddr, unsigned int physCbAddr = 0, unsigned int physCrAddr = 0, int colorFormat = 0);

    bool        setDstParams(unsigned int width, unsigned int height, 
            unsigned int cropX, unsigned int cropY, unsigned int* cropWidth, unsigned int* cropHeight, 
            int colorFormat, bool forceChange = true);

    bool        getDstParams(unsigned int* width, unsigned int* height, 
            unsigned int* cropX, unsigned int* cropY, unsigned int* cropWidth, unsigned int* cropHeight, 
            int* colorFormat);

    bool        setDstPhyAddr(unsigned int physYAddr, unsigned int physCbAddr = 0, unsigned int physCrAddr = 0);

    bool        setRotVal(unsigned int rotVal);
    bool        setGlobalAlpha(bool enable = true, int alpha = 0xff);
    bool        setLocalAlpha(bool enable);
    bool        setColorKey(bool enable = true, int colorKey = 0xff);
    bool        streamOn();
    bool        streamOff();
    bool        queryBuffer(int index, struct v4l2_buffer *buf);
    bool        queueBuffer(int index);
    int         dequeueBuffer(int* index);
    bool        handleOneShot();

private:
    int         m_widthOfFimc(int fimc_color_format, int width);
    int         m_heightOfFimc(int fimc_color_format, int height);
    unsigned int m_get_yuv_bpp(unsigned int fmt);
    unsigned int m_get_yuv_planes(unsigned int fmt);
};
#endif

#endif //__SAMSUNG_SYSLSI_APDEV_FIMCLIB_H__