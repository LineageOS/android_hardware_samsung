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

#ifndef ANDROID_HARDWARE_SEC_TV_H
#define ANDROID_HARDWARE_SEC_TV_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>

#include <linux/videodev2.h>
#include <s5p_tvout.h>

#include "fimc.h"

namespace android {

enum s5p_tv_standart {
    S5P_TV_STD_NTSC_M = 0,
    S5P_TV_STD_PAL_BDGHI,
    S5P_TV_STD_PAL_M,
    S5P_TV_STD_PAL_N,
    S5P_TV_STD_PAL_Nc,
    S5P_TV_STD_PAL_60,
    S5P_TV_STD_NTSC_443,
    S5P_TV_STD_480P_60_16_9,
    S5P_TV_STD_480P_60_4_3,
    S5P_TV_STD_576P_50_16_9,
    S5P_TV_STD_576P_50_4_3,
    S5P_TV_STD_720P_60,
    S5P_TV_STD_720P_50
};
    
// must match with s5p_tv_outputs in s5p_tv_v4l.c
enum s5p_tv_output {
    S5P_TV_OUTPUT_TYPE_COMPOSITE = 0,
    S5P_TV_OUTPUT_TYPE_SVIDEO,
    S5P_TV_OUTPUT_TYPE_YPBPR_INERLACED,
    S5P_TV_OUTPUT_TYPE_YPBPR_PROGRESSIVE,
    S5P_TV_OUTPUT_TYPE_RGB_PROGRESSIVE,
    S5P_TV_OUTPUT_TYPE_HDMI,
};
    
class SecHDMI {
public:
    SecHDMI();
    ~SecHDMI();

    static int      getCableStatus();

    int             create(int width, int height);
    int             destroy();

    int             connect();
    int             disconnect();

    int             flush(int srcW, int srcH, int srcColorFormat,
                          unsigned int srcYAddr, unsigned int srcCbAddr, unsigned int srcCrAddr,
                          int dstX, int dstY,
                          int layer,
                          int num_of_hwc_layer);

    const __u8*     getName(int index);
        
private:
    enum s5p_tv_layer {
        S5P_TV_LAYER_BASE = 0,
        S5P_TV_LAYER_VIDEO,
        S5P_TV_LAYER_GRAPHIC_0,
        S5P_TV_LAYER_GRAPHIC_1,
        S5P_TV_LAYER_MAX,
    };

    int             mTvOutFd;
    int             mTvOutVFd;
    int             mLcdFd;
    unsigned int    mHdcpEnabled;
    bool            mFlagConnected;
    bool            mFlagLayerEnable[S5P_TV_LAYER_MAX];

    s5p_fimc_t      mFimc;
    v4l2_streamparm mParams;

    int             startLayer(s5p_tv_layer layer);
    int             stopLayer(s5p_tv_layer layer);
};
    
}; // namespace android

#endif // ANDROID_HARDWARE_SEC_TV_H
