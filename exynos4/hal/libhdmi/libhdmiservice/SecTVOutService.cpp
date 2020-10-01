/*
**
** Copyright 2008, The Android Open Source Project
** Copyright 2010, Samsung Electronics Co. LTD
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
*/

/*
**
** @author  Taikyung, Yu(taikyung.yu@samsung.com)
** @date    2011-07-06
*/

#define LOG_TAG "SecTVOutService"

#include <binder/IServiceManager.h>
#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/Log.h>
#include "SecTVOutService.h"
#include <linux/fb.h>

namespace android {
#define DEFAULT_LCD_WIDTH               800
#define DEFAULT_LCD_HEIGHT              480

#define DIRECT_VIDEO_RENDERING          (1)
#define DIRECT_UI_RENDERING             (0)

    enum {
        SET_HDMI_STATUS = IBinder::FIRST_CALL_TRANSACTION,
        SET_HDMI_MODE,
        SET_HDMI_RESOLUTION,
        SET_HDMI_HDCP,
        SET_HDMI_ROTATE,
        SET_HDMI_HWCLAYER,
        BLIT_2_HDMI
    };

    int SecTVOutService::HdmiFlushThread()
    {
        while (!mExitHdmiFlushThread) {
            nsecs_t timeout = -1;
            sp<MessageBase> msg = mHdmiEventQueue.waitMessage(timeout);
        }

        return 0;
    }

    int SecTVOutService::instantiate()
    {
        ALOGD("SecTVOutService instantiate");
        int r = defaultServiceManager()->addService(String16( "SecTVOutService"), new SecTVOutService ());
        ALOGD("SecTVOutService r=%d", r);

        return r;
    }

    SecTVOutService::SecTVOutService () {
        ALOGV("SecTVOutService created");
        mHdmiCableInserted = false;
#ifdef SUPPORT_G2D_UI_MODE
        mUILayerMode = SecHdmi::HDMI_LAYER_GRAPHIC_1;
#else
        mUILayerMode = SecHdmi::HDMI_LAYER_VIDEO;
#endif
        mHwcLayer = 0;
        mExitHdmiFlushThread = false;

        setLCDsize();
        if (mSecHdmi.create(mLCD_width, mLCD_height) == false)
            ALOGE("%s::mSecHdmi.create() fail", __func__);
        else
            setHdmiStatus(1);

        mHdmiFlushThread = new HDMIFlushThread(this);
    }

    void SecTVOutService::setLCDsize(void) {
            char const * const device_template[] = {
                "/dev/graphics/fb%u",
                "/dev/fb%u",
                0 };

            int fd = -1;
            int i = 0;
            char name[64];

            while ((fd==-1) && device_template[i]) {
                snprintf(name, 64, device_template[i], 0);
                fd = open(name, O_RDWR, 0);
                i++;
            }
            if (fd > 0) {
                struct fb_var_screeninfo info;
                if (ioctl(fd, FBIOGET_VSCREENINFO, &info) != -1) {
                    mLCD_width  = info.xres;
                    mLCD_height = info.yres;
                } else {
                    mLCD_width  = DEFAULT_LCD_WIDTH;
                    mLCD_height = DEFAULT_LCD_HEIGHT;
                }
                close(fd);
            }
            return;
    }

    SecTVOutService::~SecTVOutService () {
        ALOGV ("SecTVOutService destroyed");

        if (mHdmiFlushThread != NULL) {
            mHdmiFlushThread->requestExit();
            mExitHdmiFlushThread = true;
            mHdmiFlushThread->requestExitAndWait();
            mHdmiFlushThread.clear();
        }
    }

    status_t SecTVOutService::onTransact(uint32_t code, const Parcel & data, Parcel * reply, uint32_t flags)
    {
        switch (code) {
        case SET_HDMI_STATUS: {
            int status = data.readInt32();
            setHdmiStatus(status);
        } break;

        case SET_HDMI_MODE: {
            int mode = data.readInt32();
            setHdmiMode(mode);
        } break;

        case SET_HDMI_RESOLUTION: {
            int resolution = data.readInt32();
            setHdmiResolution(resolution);
        } break;

        case SET_HDMI_HDCP: {
            int enHdcp = data.readInt32();
            setHdmiHdcp(enHdcp);
        } break;

        case SET_HDMI_ROTATE: {
            int rotVal = data.readInt32();
            int hwcLayer = data.readInt32();
            setHdmiRotate(rotVal, hwcLayer);
        } break;

        case SET_HDMI_HWCLAYER: {
            int hwcLayer = data.readInt32();
            setHdmiHwcLayer((uint32_t)hwcLayer);
        } break;

        case BLIT_2_HDMI: {
            uint32_t w = data.readInt32();
            uint32_t h = data.readInt32();
            uint32_t colorFormat = data.readInt32();
            uint32_t physYAddr  = data.readInt32();
            uint32_t physCbAddr = data.readInt32();
            uint32_t physCrAddr = data.readInt32();
            uint32_t dstX   = data.readInt32();
            uint32_t dstY   = data.readInt32();
            uint32_t hdmiLayer   = data.readInt32();
            uint32_t num_of_hwc_layer = data.readInt32();

            blit2Hdmi(w, h, colorFormat, physYAddr, physCbAddr, physCrAddr, dstX, dstY, hdmiLayer, num_of_hwc_layer);
        } break;

        default :
            ALOGE ( "onTransact::default");
            return BBinder::onTransact (code, data, reply, flags);
        }

        return NO_ERROR;
    }

    void SecTVOutService::setHdmiStatus(uint32_t status)
    {

        ALOGD("%s HDMI cable status = %d", __func__, status);
        {
            Mutex::Autolock _l(mLock);

            bool hdmiCableInserted = (bool)status;

            if (mHdmiCableInserted == hdmiCableInserted)
                return;

            if (hdmiCableInserted == true) {
                if (mSecHdmi.connect() == false) {
                    ALOGE("%s::mSecHdmi.connect() fail", __func__);
                    hdmiCableInserted = false;
                }
            } else {
                if (mSecHdmi.disconnect() == false)
                    ALOGE("%s::mSecHdmi.disconnect() fail", __func__);
            }

            mHdmiCableInserted = hdmiCableInserted;
        }

        if (hdmiCableInserted() == true)
            this->blit2Hdmi(mLCD_width, mLCD_height, HAL_PIXEL_FORMAT_BGRA_8888, 0, 0, 0, 0, 0, HDMI_MODE_UI, 0);
    }

    void SecTVOutService::setHdmiMode(uint32_t mode)
    {
        ALOGD("%s TV mode = %d", __func__, mode);
        Mutex::Autolock _l(mLock);

        if ((hdmiCableInserted() == true) && (mSecHdmi.setHdmiOutputMode(mode)) == false) {
            ALOGE("%s::mSecHdmi.setHdmiOutputMode() fail", __func__);
            return;
        }
    }

    void SecTVOutService::setHdmiResolution(uint32_t resolution)
    {
        //ALOGD("%s TV resolution = %d", __func__, resolution);
        Mutex::Autolock _l(mLock);

        if ((hdmiCableInserted() == true) && (mSecHdmi.setHdmiResolution(resolution)) == false) {
            ALOGE("%s::mSecHdmi.setHdmiResolution() fail", __func__);
            return;
        }
    }

    void SecTVOutService::setHdmiHdcp(uint32_t hdcp_en)
    {
        ALOGD("%s TV HDCP = %d", __func__, hdcp_en);
        Mutex::Autolock _l(mLock);

        if ((hdmiCableInserted() == true) && (mSecHdmi.setHdcpMode(hdcp_en)) == false) {
            ALOGE("%s::mSecHdmi.setHdcpMode() fail", __func__);
            return;
        }
    }

    void SecTVOutService::setHdmiRotate(uint32_t rotVal, uint32_t hwcLayer)
    {
        //ALOGD("%s TV ROTATE = %d", __func__, rotVal);
        Mutex::Autolock _l(mLock);

        if ((hdmiCableInserted() == true) && (mSecHdmi.setUIRotation(rotVal, hwcLayer)) == false) {
            ALOGE("%s::mSecHdmi.setUIRotation() fail", __func__);
            return;
        }
    }

    void SecTVOutService::setHdmiHwcLayer(uint32_t hwcLayer)
    {
        //ALOGD("%s TV HWCLAYER = %d", __func__, hwcLayer);
        Mutex::Autolock _l(mLock);

        mHwcLayer = hwcLayer;
        return;
    }

    void SecTVOutService::blit2Hdmi(uint32_t w, uint32_t h, uint32_t colorFormat, 
                                 uint32_t pPhyYAddr, uint32_t pPhyCbAddr, uint32_t pPhyCrAddr,
                                 uint32_t dstX, uint32_t dstY,
                                 uint32_t hdmiMode,
                                 uint32_t num_of_hwc_layer)
    {
        Mutex::Autolock _l(mLock);

        if (hdmiCableInserted() == false)
            return;

        int hdmiLayer = SecHdmi::HDMI_LAYER_VIDEO;
#if defined(CHECK_UI_TIME) || defined(CHECK_VIDEO_TIME)
        nsecs_t start, end;
#endif

        sp<MessageBase> msg;

        switch (hdmiMode) {
        case HDMI_MODE_UI :
            if (mHwcLayer >= 2)
                hdmiLayer = SecHdmi::HDMI_LAYER_GRAPHIC_0;
            else if (mHwcLayer == 1)
                hdmiLayer = SecHdmi::HDMI_LAYER_GRAPHIC_1;
            else
#ifdef SUPPORT_G2D_UI_MODE
                hdmiLayer = SecHdmi::HDMI_LAYER_GRAPHIC_1;
#else
                hdmiLayer = SecHdmi::HDMI_LAYER_VIDEO;
#endif

#ifdef SUPPORT_G2D_UI_MODE
            if (mHwcLayer == 0) {
                if (mSecHdmi.clear(SecHdmi::HDMI_LAYER_VIDEO) == false)
                    ALOGE("%s::mSecHdmi.clear(%d) fail", __func__, SecHdmi::HDMI_LAYER_VIDEO);
                if (mSecHdmi.clear(SecHdmi::HDMI_LAYER_GRAPHIC_0) == false)
                    ALOGE("%s::mSecHdmi.clear(%d) fail", __func__, SecHdmi::HDMI_LAYER_GRAPHIC_0);
            }
#endif

            if (mUILayerMode != hdmiLayer) {
                if (mSecHdmi.clear(mUILayerMode) == false)
                    ALOGE("%s::mSecHdmi.clear(%d) fail", __func__, mUILayerMode);
            }

            mUILayerMode = hdmiLayer;

#if !defined(BOARD_USES_HDMI_SUBTITLES)
            if (mHwcLayer == 0)
#endif
#if (DIRECT_UI_RENDERING == 1)
            {
#ifdef CHECK_UI_TIME
                start = systemTime();
#endif
                if (mSecHdmi.flush(w, h, colorFormat, pPhyYAddr, pPhyCbAddr, pPhyCrAddr, dstX, dstY,
                                    mUILayerMode, mHwcLayer) == false)
                    ALOGE("%s::mSecHdmi.flush() on HDMI_MODE_UI fail", __func__);
#ifdef CHECK_UI_TIME
                end = systemTime();
                ALOGD("[UI] mSecHdmi.flush[end-start] = %ld ms", long(ns2ms(end)) - long(ns2ms(start)));
#endif
            }
#else
            {
                msg = new SecHdmiEventMsg(&mSecHdmi, w, h, colorFormat, pPhyYAddr, pPhyCbAddr, pPhyCrAddr,
                                            dstX, dstY, mUILayerMode, mHwcLayer, HDMI_MODE_UI);

                /* post to HdmiEventQueue */
                mHdmiEventQueue.postMessage(msg, 0, 0);
            }
#endif
            break;

        case HDMI_MODE_VIDEO :
#if !defined(BOARD_USES_HDMI_SUBTITLES)
#ifdef SUPPORT_G2D_UI_MODE
            if (mSecHdmi.clear(SecHdmi::HDMI_LAYER_GRAPHIC_0) == false)
                ALOGE("%s::mSecHdmi.clear(%d) fail", __func__, SecHdmi::HDMI_LAYER_GRAPHIC_0);
            if (mSecHdmi.clear(SecHdmi::HDMI_LAYER_GRAPHIC_1) == false)
                ALOGE("%s::mSecHdmi.clear(%d) fail", __func__, SecHdmi::HDMI_LAYER_GRAPHIC_1);
#endif
#endif

#if (DIRECT_VIDEO_RENDERING == 1)
#ifdef CHECK_VIDEO_TIME
            start = systemTime();
#endif
            if (mSecHdmi.flush(w, h, colorFormat, pPhyYAddr, pPhyCbAddr, pPhyCrAddr, dstX, dstY,
                                SecHdmi::HDMI_LAYER_VIDEO, mHwcLayer) == false)
                ALOGE("%s::mSecHdmi.flush() on HDMI_MODE_VIDEO fail", __func__);
#ifdef CHECK_VIDEO_TIME
            end = systemTime();
            ALOGD("[Video] mSecHdmi.flush[end-start] = %ld ms", long(ns2ms(end)) - long(ns2ms(start)));
#endif
#else
            msg = new SecHdmiEventMsg(&mSecHdmi, w, h, colorFormat, pPhyYAddr, pPhyCbAddr, pPhyCrAddr,
                                        dstX, dstY, SecHdmi::HDMI_LAYER_VIDEO, mHwcLayer, HDMI_MODE_VIDEO);

            /* post to HdmiEventQueue */
            mHdmiEventQueue.postMessage(msg, 0, 0);
#endif
            break;

        default:
            ALOGE("unmatched HDMI_MODE : %d", hdmiMode);
            break;
        }

        return;
    }

    bool SecTVOutService::hdmiCableInserted(void)
    {
        return mHdmiCableInserted;
    }

}
