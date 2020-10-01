/*
**
** Copyright 2009 Samsung Electronics Co, Ltd.
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
**
*/

#ifndef FIMG_API_H
#define FIMG_API_H

#include <utils/Log.h>

#include "../include/sec_g2d.h"
//#include <sec_g2d.h>
#define REAL_DEBUG
#define ANDROID_LOG

#if defined(REAL_DEBUG)
#ifdef ANDROID_LOG
    #define PRINT  ALOGE
    #define PRINTD ALOGD
#else
    #define PRINT  printf
    #define PRINTD printf
#endif
#else
    void VOID_FUNC(const char* format, ...);

    #define PRINT  VOID_FUNC
    #define PRINTD VOID_FUNC
#endif

typedef g2d_rect FimgRect;
typedef g2d_flag FimgFlag;
typedef g2d_clip FimgClip;

#ifdef __cplusplus
class FimgApi
{
public:
#endif
    enum COLOR_FORMAT {
        COLOR_FORMAT_BASE = 0,

        COLOR_FORMAT_RGB_565   = G2D_RGB_565,

        COLOR_FORMAT_RGBA_8888 = G2D_RGBA_8888,
        COLOR_FORMAT_ARGB_8888 = G2D_ARGB_8888,
        COLOR_FORMAT_BGRA_8888 = G2D_BGRA_8888,
        COLOR_FORMAT_ABGR_8888 = G2D_ABGR_8888,

        COLOR_FORMAT_RGBX_8888 = G2D_RGBX_8888,
        COLOR_FORMAT_XRGB_8888 = G2D_XRGB_8888,
        COLOR_FORMAT_BGRX_8888 = G2D_BGRX_8888,
        COLOR_FORMAT_XBGR_8888 = G2D_XBGR_8888,

        COLOR_FORMAT_RGBA_5551 = G2D_RGBA_5551,
        COLOR_FORMAT_ARGB_1555 = G2D_ARGB_1555,
        COLOR_FORMAT_BGRA_5551 = G2D_BGRA_5551,
        COLOR_FORMAT_ABGR_1555 = G2D_ABGR_1555,

        COLOR_FORMAT_RGBX_5551 = G2D_RGBX_5551,
        COLOR_FORMAT_XRGB_1555 = G2D_XRGB_1555,
        COLOR_FORMAT_BGRX_5551 = G2D_BGRX_5551,
        COLOR_FORMAT_XBGR_1555 = G2D_XBGR_1555,

        COLOR_FORMAT_RGBA_4444 = G2D_RGBA_4444,
        COLOR_FORMAT_ARGB_4444 = G2D_ARGB_4444,
        COLOR_FORMAT_BGRA_4444 = G2D_BGRA_4444,
        COLOR_FORMAT_ABGR_4444 = G2D_ABGR_4444,

        COLOR_FORMAT_RGBX_4444 = G2D_RGBX_4444,
        COLOR_FORMAT_XRGB_4444 = G2D_XRGB_4444,
        COLOR_FORMAT_BGRX_4444 = G2D_BGRX_4444,
        COLOR_FORMAT_XBGR_4444 = G2D_XBGR_4444,

        COLOR_FORMAT_PACKED_RGB_888 = G2D_PACKED_RGB_888,
        COLOR_FORMAT_PACKED_BGR_888 = G2D_PACKED_BGR_888,
        COLOR_FORMAT_YUV_420SP,
        COLOR_FORMAT_YUV_420P,
        COLOR_FORMAT_YUV_420I,
        COLOR_FORMAT_YUV_422SP,
        COLOR_FORMAT_YUV_422P,
        COLOR_FORMAT_YUV_422I,
        COLOR_FORMAT_YUYV,

        COLOR_FORMAT_MAX,
    };

    enum ROTATE {
        ROTATE_BASE   = 0,
        ROTATE_0      = G2D_ROT_0,
        ROTATE_90     = G2D_ROT_90,
        ROTATE_180    = G2D_ROT_180,
        ROTATE_270    = G2D_ROT_270,
        ROTATE_X_FLIP = G2D_ROT_X_FLIP,
        ROTATE_Y_FLIP = G2D_ROT_Y_FLIP,
        ROTATE_MAX,
    };

    enum ALPHA_VALUE {
        ALPHA_MIN    = G2D_ALPHA_BLENDING_MIN, // wholly transparent
        ALPHA_MAX    = G2D_ALPHA_BLENDING_MAX, // 255
        ALPHA_OPAQUE = G2D_ALPHA_BLENDING_OPAQUE, // opaque
    };

    enum DITHER {
        DITHER_BASE   = 0,
        DITHER_OFF    = 0,
        DITHER_ON     = 1,
        DITHER_MAX,
    };
#ifdef __cplusplus
private :
    bool    m_flagCreate;

protected :
    FimgApi();
    FimgApi(const FimgApi& rhs) {}
    virtual ~FimgApi();

public:
    bool        Create(void);
    bool        Destroy(void);
    inline bool FlagCreate(void) { return m_flagCreate; }
    bool        Stretch(FimgRect * src, FimgRect * dst, FimgClip * clip, FimgFlag * flag);
    bool	    Sync(void);

protected:
    virtual bool t_Create(void);
    virtual bool t_Destroy(void);
    virtual bool t_Stretch(FimgRect * src, FimgRect * dst, FimgClip * clip, FimgFlag * flag);
    virtual bool t_Sync(void);
    virtual bool t_Lock(void);
    virtual bool t_UnLock(void);

};
#endif

/*---------------------------------------------------------------------------
 * user api extern function
 *---------------------------------------------------------------------------
 * usage 1
 * FimgApi * p = createFimgApi();
 * p->Stretch()
 * destroyFimgApi(p);
 *
 * usage 2
 * stretchFimgApi(src, dst, clip, flag);
 *-------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
#endif
struct FimgApi * createFimgApi();

#ifdef __cplusplus
extern "C"
#endif
void             destroyFimgApi(FimgApi * ptrFimgApi);

#ifdef __cplusplus
extern "C"
#endif
int              stretchFimgApi(FimgRect * src,
                                FimgRect * dst,
                                FimgClip * clip,
                                FimgFlag * flag);
#ifdef __cplusplus
extern "C"
#endif
int              SyncFimgApi(void);

#endif //FIMG_API_H
