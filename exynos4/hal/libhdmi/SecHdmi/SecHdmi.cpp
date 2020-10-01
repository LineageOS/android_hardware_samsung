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

//#define LOG_NDEBUG 0
//#define LOG_TAG "libhdmi"
#include <cutils/log.h>

#if defined(BOARD_USE_V4L2_ION)
#include "ion.h"
#endif

#include "SecHdmi.h"
#include "SecHdmiV4L2Utils.h"

#define CHECK_GRAPHIC_LAYER_TIME (0)

namespace android {

extern unsigned int output_type;
#if defined(BOARD_USE_V4L2)
extern unsigned int g_preset_id;
#endif
extern v4l2_std_id t_std_id;
extern int g_hpd_state;
extern unsigned int g_hdcp_en;

#if !defined(BOARD_USE_V4L2)
extern int fp_tvout;
extern int fp_tvout_v;
extern int fp_tvout_g0;
extern int fp_tvout_g1;
#endif

#if defined(BOARD_USES_FIMGAPI)
extern unsigned int g2d_reserved_memory[HDMI_G2D_OUTPUT_BUF_NUM];
extern unsigned int g2d_reserved_memory_size;
extern unsigned int cur_g2d_address;
extern unsigned int g2d_buf_index;
#endif

#if defined(BOARD_USES_CEC)
SecHdmi::CECThread::~CECThread()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    mFlagRunning = false;
}

bool SecHdmi::CECThread::threadLoop()
{
    unsigned char buffer[CEC_MAX_FRAME_SIZE];
    int size;
    unsigned char lsrc, ldst, opcode;

    {
        Mutex::Autolock lock(mThreadLoopLock);
        mFlagRunning = true;

        size = CECReceiveMessage(buffer, CEC_MAX_FRAME_SIZE, 100000);

        if (!size) // no data available or ctrl-c
            return true;

        if (size == 1)
            return true; // "Polling Message"

        lsrc = buffer[0] >> 4;

        /* ignore messages with src address == mLaddr*/
        if (lsrc == mLaddr)
            return true;

        opcode = buffer[1];

        if (CECIgnoreMessage(opcode, lsrc)) {
            ALOGE("### ignore message coming from address 15 (unregistered)\n");
            return true;
        }

        if (!CECCheckMessageSize(opcode, size)) {
            ALOGE("### invalid message size: %d(opcode: 0x%x) ###\n", size, opcode);
            return true;
        }

        /* check if message broadcasted/directly addressed */
        if (!CECCheckMessageMode(opcode, (buffer[0] & 0x0F) == CEC_MSG_BROADCAST ? 1 : 0)) {
            ALOGE("### invalid message mode (directly addressed/broadcast) ###\n");
            return true;
        }

        ldst = lsrc;

        //TODO: macroses to extract src and dst logical addresses
        //TODO: macros to extract opcode

        switch (opcode) {
        case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
            /* responce with "Report Physical Address" */
            buffer[0] = (mLaddr << 4) | CEC_MSG_BROADCAST;
            buffer[1] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
            buffer[2] = (mPaddr >> 8) & 0xFF;
            buffer[3] = mPaddr & 0xFF;
            buffer[4] = mDevtype;
            size = 5;
            break;

        case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
            ALOGD("[CEC_OPCODE_REQUEST_ACTIVE_SOURCE]\n");
            /* responce with "Active Source" */
            buffer[0] = (mLaddr << 4) | CEC_MSG_BROADCAST;
            buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
            buffer[2] = (mPaddr >> 8) & 0xFF;
            buffer[3] = mPaddr & 0xFF;
            size = 4;
            ALOGD("Tx : [CEC_OPCODE_ACTIVE_SOURCE]\n");
            break;

        case CEC_OPCODE_ABORT:
        case CEC_OPCODE_FEATURE_ABORT:
        default:
            /* send "Feature Abort" */
            buffer[0] = (mLaddr << 4) | ldst;
            buffer[1] = CEC_OPCODE_FEATURE_ABORT;
            buffer[2] = CEC_OPCODE_ABORT;
            buffer[3] = 0x04; // "refused"
            size = 4;
            break;
        }

        if (CECSendMessage(buffer, size) != size)
            ALOGE("CECSendMessage() failed!!!\n");

    }
    return true;
}

bool SecHdmi::CECThread::start()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mThreadControlLock);
    if (exitPending()) {
        if (requestExitAndWait() == WOULD_BLOCK) {
            ALOGE("mCECThread.requestExitAndWait() == WOULD_BLOCK");
            return false;
        }
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("EDIDGetCECPhysicalAddress");
#endif
    /* set to not valid physical address */
    mPaddr = CEC_NOT_VALID_PHYSICAL_ADDRESS;

    if (!EDIDGetCECPhysicalAddress(&mPaddr)) {
        ALOGE("Error: EDIDGetCECPhysicalAddress() failed.\n");
        return false;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("CECOpen");
#endif
    if (!CECOpen()) {
        ALOGE("CECOpen() failed!!!\n");
        return false;
    }

    /* a logical address should only be allocated when a device \
       has a valid physical address, at all other times a device \
       should take the 'Unregistered' logical address (15)
       */

    /* if physical address is not valid device should take \
       the 'Unregistered' logical address (15)
       */

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("CECAllocLogicalAddress");
#endif
    mLaddr = CECAllocLogicalAddress(mPaddr, mDevtype);

    if (!mLaddr) {
        ALOGE("CECAllocLogicalAddress() failed!!!\n");
        if (!CECClose())
            ALOGE("CECClose() failed!\n");
        return false;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("request to run CECThread");
#endif

    status_t ret = run("SecHdmi::CECThread", PRIORITY_DISPLAY);
    if (ret != NO_ERROR) {
        ALOGE("%s fail to run thread", __func__);
        return false;
    }
    return true;
}

bool SecHdmi::CECThread::stop()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s request Exit", __func__);
#endif
    Mutex::Autolock lock(mThreadControlLock);
    if (requestExitAndWait() == WOULD_BLOCK) {
        ALOGE("mCECThread.requestExitAndWait() == WOULD_BLOCK");
        return false;
    }

    if (!CECClose())
        ALOGE("CECClose() failed!\n");

    mFlagRunning = false;
    return true;
}
#endif

SecHdmi::SecHdmi():
#if defined(BOARD_USES_CEC)
    mCECThread(NULL),
#endif
    mFlagCreate(false),
    mFlagConnected(false),
    mHdmiDstWidth(0),
    mHdmiDstHeight(0),
    mHdmiSrcYAddr(0),
    mHdmiSrcCbCrAddr(0),
    mHdmiOutputMode(DEFAULT_OUPUT_MODE),
    mHdmiResolutionValue(DEFAULT_HDMI_RESOLUTION_VALUE), // V4L2_STD_480P_60_4_3
    mCompositeStd(DEFAULT_COMPOSITE_STD),
    mHdcpMode(false),
    mAudioMode(2),
    mUIRotVal(0),
    mG2DUIRotVal(0),
    mCurrentHdmiOutputMode(-1),
    mCurrentHdmiResolutionValue(0), // 1080960
    mCurrentHdcpMode(false),
    mCurrentAudioMode(-1),
    mHdmiInfoChange(true),
    mFimcDstColorFormat(0),
    mFimcCurrentOutBufIndex(0),
    mFBaddr(NULL),
    mFBsize(0),
    mFBionfd(-1),
    mFBIndex(0),
    mDefaultFBFd(-1),
    mDisplayWidth(DEFALULT_DISPLAY_WIDTH),
    mDisplayHeight(DEFALULT_DISPLAY_HEIGHT)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    for (int i = 0; i < HDMI_LAYER_MAX; i++) {
        mFlagLayerEnable[i] = false;
        mFlagHdmiStart[i] = false;

        mSrcWidth      [i] = 0;
        mSrcHeight     [i] = 0;
        mSrcColorFormat[i] = 0;
        mHdmiResolutionWidth  [i] = 0;
        mHdmiResolutionHeight [i] = 0;
        mHdmiFd[i] = -1;
        mDstWidth  [i] = 0;
        mDstHeight [i] = 0;
        mPrevDstWidth  [i] = 0;
        mPrevDstHeight [i] = 0;
    }

    mHdmiPresetId = DEFAULT_HDMI_PRESET_ID;
    mHdmiStdId = DEFAULT_HDMI_STD_ID;

    //All layer is on
    mFlagLayerEnable[HDMI_LAYER_VIDEO] = true;
    mFlagLayerEnable[HDMI_LAYER_GRAPHIC_0] = true;
    mFlagLayerEnable[HDMI_LAYER_GRAPHIC_1] = true;

    mHdmiSizeOfResolutionValueList = 14;

    mHdmiResolutionValueList[0]  = 1080960;
    mHdmiResolutionValueList[1]  = 1080950;
    mHdmiResolutionValueList[2]  = 1080930;
    mHdmiResolutionValueList[3]  = 1080924;
    mHdmiResolutionValueList[4]  = 1080160;
    mHdmiResolutionValueList[5]  = 1080150;
    mHdmiResolutionValueList[6]  = 720960;
    mHdmiResolutionValueList[7]  = 7209601;
    mHdmiResolutionValueList[8]  = 720950;
    mHdmiResolutionValueList[9]  = 7209501;
    mHdmiResolutionValueList[10] = 5769501;
    mHdmiResolutionValueList[11] = 5769502;
    mHdmiResolutionValueList[12] = 4809601;
    mHdmiResolutionValueList[13] = 4809602;

#if defined(BOARD_USES_CEC)
    mCECThread = new CECThread(this);
#endif

    SecBuffer zeroBuf;
    for (int i = 0; i < HDMI_FIMC_OUTPUT_BUF_NUM; i++)
        mFimcReservedMem[i] = zeroBuf;
#if defined(BOARD_USE_V4L2)
    for (int i = 0; i < HDMI_LAYER_MAX; i++)
        for (int j = 0; j < MAX_BUFFERS_MIXER; j++)
            mMixerBuffer[i][j] = zeroBuf;
#endif

    memset(&mDstRect, 0 , sizeof(struct v4l2_rect));
}

SecHdmi::~SecHdmi()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    if (mFlagCreate == true)
        ALOGE("%s::this is not Destroyed fail", __func__);
    else
        disconnect();
}

bool SecHdmi::create(int width, int height)
{
    Mutex::Autolock lock(mLock);
    unsigned int fimc_buf_size = 0;
    unsigned int gralloc_buf_size = 0;
    mFimcCurrentOutBufIndex = 0;
    int stride;
    int vstride;
    int BufNum = 0;
#if defined(BOARD_USE_V4L2_ION)
    int IonClient = -1;
    int IonFd = -1;
    void *ion_base_addr = NULL;
#endif

/*
 * Video plaback (I420): output buffer size of FIMC3 is (1920 x 1088 x 1.5)
 * Video plaback (NV12): FIMC3 is not used.
 * Camera preview (YV12): output buffer size of FIMC3 is (640 x 480 x 1.5)
 * UI mode (ARGB8888) : output buffer size of FIMC3 is (480 x 800 x 1.5)
 */
#ifndef SUPPORT_1080P_FIMC_OUT
    setDisplaySize(width, height);
#endif

    stride = ALIGN(HDMI_MAX_WIDTH, 16);
    vstride = ALIGN(HDMI_MAX_HEIGHT, 16);

    fimc_buf_size = stride * vstride * HDMI_FIMC_BUFFER_BPP_SIZE;
    gralloc_buf_size = GRALLOC_BUF_SIZE * SIZE_1K;
#if defined(BOARD_USES_FIMGAPI)
    g2d_reserved_memory_size = stride * vstride * HDMI_G2D_BUFFER_BPP_SIZE;
#endif

#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    if (mFlagCreate == true) {
        ALOGE("%s::Already Created fail", __func__);
        goto CREATE_FAIL;
    }

    if (mDefaultFBFd <= 0) {
        if ((mDefaultFBFd = fb_open(DEFAULT_FB)) < 0) {
            ALOGE("%s:Failed to open default FB", __func__);
            return false;
        }
    }

#ifdef BOARD_USE_V4L2
    BufNum = HDMI_FIMC_OUTPUT_BUF_NUM;
#else
    BufNum = 1;
#endif

    if (mSecFimc.create(SecFimc::DEV_3, SecFimc::MODE_SINGLE_BUF, BufNum) == false) {
        ALOGE("%s::SecFimc create() fail", __func__);
        goto CREATE_FAIL;
    }

#if defined(BOARD_USE_V4L2_ION)
    IonClient = ion_client_create();
    if (IonClient < 0) {
        ALOGE("%s::ion_client_create() failed", __func__);
        goto CREATE_FAIL;
    }
#if defined(BOARD_USES_FIMGAPI)
    IonFd = ion_alloc(IonClient, g2d_reserved_memory_size * HDMI_G2D_OUTPUT_BUF_NUM, 0, ION_HEAP_EXYNOS_MASK);

    if (IonFd < 0) {
        ALOGE("%s::ION memory allocation failed", __func__);
    } else {
        ion_base_addr = ion_map(IonFd, ALIGN(g2d_reserved_memory_size * HDMI_G2D_OUTPUT_BUF_NUM, PAGE_SIZE), 0);
        if (ion_base_addr == MAP_FAILED)
            ALOGE("%s::ION mmap failed", __func__);
    }

    for (int i = 0; i < HDMI_G2D_OUTPUT_BUF_NUM; i++)
        g2d_reserved_memory[i] = (unsigned int)ion_base_addr + (g2d_reserved_memory_size * i);
#endif
#else
#ifndef BOARD_USE_V4L2
    for (int i = 0; i < HDMI_FIMC_OUTPUT_BUF_NUM; i++)
        mFimcReservedMem[i].phys.p = mSecFimc.getMemAddr()->phys.p + gralloc_buf_size + (fimc_buf_size * i);
#endif

#if defined(BOARD_USES_FIMGAPI)
#if defined(BOARD_USES_HDMI_SUBTITLES)
    for (int i = 0; i < HDMI_G2D_OUTPUT_BUF_NUM; i++)
        g2d_reserved_memory[i] = mFimcReservedMem[HDMI_FIMC_OUTPUT_BUF_NUM - 1].phys.p + fimc_buf_size + (g2d_reserved_memory_size * i);
#else
    for (int i = 0; i < HDMI_G2D_OUTPUT_BUF_NUM; i++)
        g2d_reserved_memory[i] = mSecFimc.getMemAddr()->phys.p + gralloc_buf_size + (g2d_reserved_memory_size * i);
#endif
#endif
#endif

    v4l2_std_id std_id;

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::mHdmiOutputMode(%d) \n", __func__, mHdmiOutputMode);
#endif
    if (mHdmiOutputMode == COMPOSITE_OUTPUT_MODE) {
        std_id = composite_std_2_v4l2_std_id(mCompositeStd);
        if ((int)std_id < 0) {
            ALOGE("%s::composite_std_2_v4l2_std_id(%d) fail\n", __func__, mCompositeStd);
            goto CREATE_FAIL;
        }
        if (m_setCompositeResolution(mCompositeStd) == false) {
            ALOGE("%s::m_setCompositeResolution(%d) fail\n", __func__, mCompositeStd);
            goto CREATE_FAIL;
        }
    } else if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
#if defined(BOARD_USE_V4L2)
        unsigned int preset_id;

        if (hdmi_resolution_2_preset_id(mHdmiResolutionValue, &mHdmiDstWidth, &mHdmiDstHeight, &preset_id) < 0) {
            ALOGE("%s::hdmi_resolution_2_preset_id(%d) fail\n", __func__, mHdmiResolutionValue);
            goto CREATE_FAIL;
        }
#else
        if (hdmi_resolution_2_std_id(mHdmiResolutionValue, &mHdmiDstWidth, &mHdmiDstHeight, &std_id) < 0) {
            ALOGE("%s::hdmi_resolution_2_std_id(%d) fail\n", __func__, mHdmiResolutionValue);
            goto CREATE_FAIL;
        }
#endif
    }

    mFlagCreate = true;

    return true;

CREATE_FAIL :

    if (mSecFimc.flagCreate() == true &&
       mSecFimc.destroy()    == false)
        ALOGE("%s::fimc destory fail", __func__);

    return false;
}

bool SecHdmi::destroy(void)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Already Destroyed fail \n", __func__);
        goto DESTROY_FAIL;
    }

    for (int layer = HDMI_LAYER_BASE + 1; layer <= HDMI_LAYER_GRAPHIC_0; layer++) {
        if (mFlagHdmiStart[layer] == true && m_stopHdmi(layer) == false) {
            ALOGE("%s::m_stopHdmi: layer[%d] fail \n", __func__, layer);
            goto DESTROY_FAIL;
        }

        if (hdmi_deinit_layer(layer) < 0) {
            ALOGE("%s::hdmi_deinit_layer(%d) fail \n", __func__, layer);
            goto DESTROY_FAIL;
        }
    }

#if !defined(BOARD_USE_V4L2)
    tvout_deinit();
#endif

    if (mSecFimc.flagCreate() == true && mSecFimc.destroy() == false) {
        ALOGE("%s::fimc destory fail \n", __func__);
        goto DESTROY_FAIL;
    }

#ifdef USE_LCD_ADDR_IN_HERE
    {
        if (0 < mDefaultFBFd) {
            close(mDefaultFBFd);
            mDefaultFBFd = -1;
        }
    }
#endif //USE_LCD_ADDR_IN_HERE

#if defined(BOARD_USE_V4L2_ION)
    if (mFBaddr != NULL)
        ion_unmap((void *)mFBaddr, ALIGN(mFBsize * 4 * 2, PAGE_SIZE));

    if (mFBionfd > 0)
        ion_free(mFBionfd);

    mFBaddr = NULL;
    mFBionfd = -1;
    mFBsize = 0;
#endif

#if defined(BOARD_USE_V4L2_ION) && defined(BOARD_USES_FIMGAPI)
    ion_unmap((void *)g2d_reserved_memory[0], ALIGN(g2d_reserved_memory_size * HDMI_G2D_OUTPUT_BUF_NUM, PAGE_SIZE));
#endif

    mFlagCreate = false;

    return true;

DESTROY_FAIL :

    return false;
}

bool SecHdmi::connect(void)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    {
        Mutex::Autolock lock(mLock);

        if (mFlagCreate == false) {
            ALOGE("%s::Not Yet Created \n", __func__);
            return false;
        }

        if (mFlagConnected == true) {
            ALOGD("%s::Already Connected.. \n", __func__);
            return true;
        }

        if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
                mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
            if (m_flagHWConnected() == false) {
                ALOGD("%s::m_flagHWConnected() fail \n", __func__);
                return false;
            }

#if defined(BOARD_USES_EDID)
            if (!EDIDOpen())
                ALOGE("EDIDInit() failed!\n");

            if (!EDIDRead()) {
                ALOGE("EDIDRead() failed!\n");
                if (!EDIDClose())
                    ALOGE("EDIDClose() failed!\n");
            }
#endif

#if defined(BOARD_USES_CEC)
            if (!(mCECThread->mFlagRunning))
                mCECThread->start();
#endif
        }
    }

    if (this->setHdmiOutputMode(mHdmiOutputMode, true) == false)
        ALOGE("%s::setHdmiOutputMode(%d) fail \n", __func__, mHdmiOutputMode);

    if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
        if (this->setHdmiResolution(mHdmiResolutionValue, true) == false)
            ALOGE("%s::setHdmiResolution(%d) fail \n", __func__, mHdmiResolutionValue);

        if (this->setHdcpMode(mHdcpMode, false) == false)
            ALOGE("%s::setHdcpMode(%d) fail \n", __func__, mHdcpMode);

        mHdmiInfoChange = true;
        mFlagConnected = true;

#if defined(BOARD_USES_EDID)
        // show display..
        display_menu();
#endif
    }

    return true;
}

bool SecHdmi::disconnect(void)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (mFlagConnected == false) {
        ALOGE("%s::Already Disconnected.. \n", __func__);
        return true;
    }

    if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
        mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
#if defined(BOARD_USES_CEC)
        if (mCECThread->mFlagRunning)
            mCECThread->stop();
#endif

#if defined(BOARD_USES_EDID)
        if (!EDIDClose()) {
            ALOGE("EDIDClose() failed!\n");
            return false;
        }
#endif
    }

    for (int layer = SecHdmi::HDMI_LAYER_BASE + 1; layer <= SecHdmi::HDMI_LAYER_GRAPHIC_0; layer++) {
        if (mFlagHdmiStart[layer] == true && m_stopHdmi(layer) == false) {
            ALOGE("%s::hdmiLayer(%d) layer fail \n", __func__, layer);
            return false;
        }
    }

#if defined(BOARD_USE_V4L2)
    for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
        if (hdmi_deinit_layer(layer) < 0)
            ALOGE("%s::hdmi_deinit_layer(%d) fail", __func__, layer);
    }
#else
    tvout_deinit();
#endif

    mFlagConnected = false;

    mHdmiOutputMode = DEFAULT_OUPUT_MODE;
    mHdmiResolutionValue = DEFAULT_HDMI_RESOLUTION_VALUE;
#if defined(BOARD_USE_V4L2)
    mHdmiPresetId = DEFAULT_HDMI_PRESET_ID;
#else
    mHdmiStdId = DEFAULT_HDMI_STD_ID;
#endif
    mCompositeStd = DEFAULT_COMPOSITE_STD;
    mAudioMode = 2;
    mCurrentHdmiOutputMode = -1;
    mCurrentHdmiResolutionValue = 0;
    mCurrentAudioMode = -1;
    mFimcCurrentOutBufIndex = 0;

    return true;
}

bool SecHdmi::flagConnected(void)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    return mFlagConnected;
}

bool SecHdmi::flush(int srcW, int srcH, int srcColorFormat,
        unsigned int srcYAddr, unsigned int srcCbAddr, unsigned int srcCrAddr,
        int dstX, int dstY,
        int hdmiLayer,
        int num_of_hwc_layer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s [srcW=%d, srcH=%d, srcColorFormat=0x%x, srcYAddr=0x%x, srcCbAddr=0x%x, srcCrAddr=0x%x, dstX=%d, dstY=%d, hdmiLayer=%d]",
            __func__, srcW, srcH, srcColorFormat, srcYAddr, srcCbAddr, srcCrAddr, dstX, dstY, hdmiLayer);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

#if defined(BOARD_USE_V4L2)
    if (hdmiLayer == HDMI_LAYER_VIDEO) {
        mDstWidth[hdmiLayer] = mHdmiDstWidth;
        mDstHeight[hdmiLayer] = mHdmiDstHeight;
   } else {
        if (num_of_hwc_layer == 0) {
            struct v4l2_rect rect;
            int tempSrcW, tempSrcH;

            if (mG2DUIRotVal == 0 || mG2DUIRotVal == 180) {
                tempSrcW = srcW;
                tempSrcH = srcH;
            } else {
                tempSrcW = srcH;
                tempSrcH = srcW;
            }

            hdmi_cal_rect(tempSrcW, tempSrcH, mHdmiDstWidth, mHdmiDstHeight, &rect);
            mDstWidth[hdmiLayer] = rect.width;
            mDstHeight[hdmiLayer] = rect.height;
            mDstWidth[HDMI_LAYER_VIDEO] = 0;
            mDstHeight[HDMI_LAYER_VIDEO] = 0;
        } else {
            mDstWidth[hdmiLayer] = mHdmiDstWidth;
            mDstHeight[hdmiLayer] = mHdmiDstHeight;
        }
   }
#ifdef DEBUG_MSG_ENABLE
    ALOGE("m_reset param(%d, %d, %d, %d)",
        mDstWidth[hdmiLayer], mDstHeight[hdmiLayer], \
        mPrevDstWidth[hdmiLayer], mPrevDstHeight[hdmiLayer]);
#endif
#endif

    if (srcW != mSrcWidth[hdmiLayer] ||
        srcH != mSrcHeight[hdmiLayer] ||
        srcColorFormat != mSrcColorFormat[hdmiLayer] ||
        mHdmiDstWidth != mHdmiResolutionWidth[hdmiLayer] ||
        mHdmiDstHeight != mHdmiResolutionHeight[hdmiLayer] ||
#if defined(BOARD_USE_V4L2)
        mDstWidth[hdmiLayer] != mPrevDstWidth[hdmiLayer] ||
        mDstHeight[hdmiLayer] != mPrevDstHeight[hdmiLayer] ||
#endif
        mHdmiInfoChange == true) {
#ifdef DEBUG_MSG_ENABLE
        ALOGD("m_reset param(%d, %d, %d, %d, %d, %d, %d)",
            srcW, mSrcWidth[hdmiLayer], \
            srcH, mSrcHeight[hdmiLayer], \
            srcColorFormat,mSrcColorFormat[hdmiLayer], \
            hdmiLayer);
#endif

        if (m_reset(srcW, srcH, srcColorFormat, hdmiLayer, num_of_hwc_layer) == false) {
            ALOGE("%s::m_reset(%d, %d, %d, %d, %d) fail", __func__, srcW, srcH, srcColorFormat, hdmiLayer, num_of_hwc_layer);
            return false;
        }
    }

    if (srcYAddr == 0) {
#if defined(BOARD_USE_V4L2_ION)
        unsigned int FB_size = ALIGN(srcW, 16) * ALIGN(srcH, 16) * HDMI_FB_BPP_SIZE;
        void *virFBAddr = 0;
        struct s3c_fb_user_ion_client ion_handle;

        if (mFBaddr != NULL) {
            ion_unmap((void *)mFBaddr, ALIGN(mFBsize * 2, PAGE_SIZE));
            ion_free(mFBionfd);
        }

        // get framebuffer virtual address for LCD
        if (ioctl(mDefaultFBFd, S3CFB_GET_ION_USER_HANDLE, &ion_handle) < 0) {
            ALOGE("%s:ioctl(S3CFB_GET_ION_USER_HANDLE) fail", __func__);
            return false;
        }

        virFBAddr = ion_map(ion_handle.fd, ALIGN(FB_size * 2, PAGE_SIZE), 0);
        if (virFBAddr == MAP_FAILED) {
            ALOGE("%s::ion_map fail", __func__);
            ion_free(ion_handle.fd);
            mFBaddr = NULL;
            return false;
        }

        if ((mFBIndex % 2) == 0)
            srcYAddr = (unsigned int)virFBAddr;
        else
            srcYAddr = (unsigned int)virFBAddr + FB_size;

        srcCbAddr = srcYAddr;

        mFBIndex++;
        mFBaddr = virFBAddr;
        mFBsize = FB_size;
        mFBionfd = ion_handle.fd;
#else
        unsigned int phyFBAddr = 0;

        // get physical framebuffer address for LCD
        if (ioctl(mDefaultFBFd, S3CFB_GET_FB_PHY_ADDR, &phyFBAddr) == -1) {
            ALOGE("%s:ioctl(S3CFB_GET_FB_PHY__ADDR) fail", __func__);
            return false;
        }

        /*
         * when early suspend, FIMD IP off.
         * so physical framebuffer address for LCD is 0x00000000
         * so JUST RETURN.
         */
        if (phyFBAddr == 0) {
            ALOGE("%s::S3CFB_GET_FB_PHY_ADDR fail", __func__);
            return true;
        }
        srcYAddr = phyFBAddr;
        srcCbAddr = srcYAddr;
#endif
    }

    if (hdmiLayer == HDMI_LAYER_VIDEO) {
        if (srcColorFormat == HAL_PIXEL_FORMAT_YCbCr_420_SP ||
            srcColorFormat == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP) {
#if defined(BOARD_USE_V4L2)
            mMixerBuffer[hdmiLayer][0].virt.extP[0] = (char *)srcYAddr;
            mMixerBuffer[hdmiLayer][0].virt.extP[1] = (char *)srcCbAddr;
#else
            hdmi_set_v_param(hdmiLayer,
                    srcW, srcH, V4L2_PIX_FMT_NV12,
                    srcYAddr, srcCbAddr,
                    mHdmiDstWidth, mHdmiDstHeight);
#endif
        } else if (srcColorFormat == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED) {
#if defined(BOARD_USE_V4L2)
            mMixerBuffer[hdmiLayer][0].virt.extP[0] = (char *)srcYAddr;
            mMixerBuffer[hdmiLayer][0].virt.extP[1] = (char *)srcCbAddr;
#else
            hdmi_set_v_param(hdmiLayer,
                            srcW, srcH, V4L2_PIX_FMT_NV12T,
                            srcYAddr, srcCbAddr,
                            mHdmiDstWidth, mHdmiDstHeight);
#endif
        } else if (srcColorFormat == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
                 srcColorFormat == HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP) {
#if defined(BOARD_USE_V4L2)
            mMixerBuffer[hdmiLayer][0].virt.extP[0] = (char *)srcYAddr;
            mMixerBuffer[hdmiLayer][0].virt.extP[1] = (char *)srcCbAddr;
#else
            hdmi_set_v_param(hdmiLayer,
                    srcW, srcH, V4L2_PIX_FMT_NV21,
                    srcYAddr, srcCbAddr,
                    mHdmiDstWidth, mHdmiDstHeight);
#endif
        } else {
            if (mSecFimc.setSrcAddr(srcYAddr, srcCbAddr, srcCrAddr, srcColorFormat) == false) {
                ALOGE("%s::setSrcAddr(%d, %d, %d) fail",
                        __func__, srcYAddr, srcCbAddr, srcCrAddr);
                return false;
            }

            int  y_size = 0;
            if (mUIRotVal == 0 || mUIRotVal == 180)
                y_size =  ALIGN(ALIGN(srcW,128) * ALIGN(srcH, 32), SZ_8K);
            else
                y_size =  ALIGN(ALIGN(srcH,128) * ALIGN(srcW, 32), SZ_8K);

            mHdmiSrcYAddr    = mFimcReservedMem[mFimcCurrentOutBufIndex].phys.extP[0];
#ifdef BOARD_USE_V4L2
            mHdmiSrcCbCrAddr = mFimcReservedMem[mFimcCurrentOutBufIndex].phys.extP[1];
#else
            mHdmiSrcCbCrAddr = mFimcReservedMem[mFimcCurrentOutBufIndex].phys.extP[0] + y_size;
#endif
            if (mSecFimc.setDstAddr(mHdmiSrcYAddr, mHdmiSrcCbCrAddr, 0, mFimcCurrentOutBufIndex) == false) {
                ALOGE("%s::mSecFimc.setDstAddr(%d, %d) fail \n",
                        __func__, mHdmiSrcYAddr, mHdmiSrcCbCrAddr);
                return false;
            }

            if (mSecFimc.draw(0, mFimcCurrentOutBufIndex) == false) {
                ALOGE("%s::mSecFimc.draw() fail \n", __func__);
                return false;
            }
#if defined(BOARD_USE_V4L2)
            mMixerBuffer[hdmiLayer][0].virt.extP[0] = (char *)mHdmiSrcYAddr;
            mMixerBuffer[hdmiLayer][0].virt.extP[1] = (char *)mHdmiSrcCbCrAddr;
#else
            if (mUIRotVal == 0 || mUIRotVal == 180)
                hdmi_set_v_param(hdmiLayer,
                        srcW, srcH, V4L2_PIX_FMT_NV12T,
                        mHdmiSrcYAddr, mHdmiSrcCbCrAddr,
                        mHdmiDstWidth, mHdmiDstHeight);
            else
                hdmi_set_v_param(hdmiLayer,
                        srcH, srcW, V4L2_PIX_FMT_NV12T,
                        mHdmiSrcYAddr, mHdmiSrcCbCrAddr,
                        mHdmiDstWidth, mHdmiDstHeight);
#endif
            mFimcCurrentOutBufIndex++;
            if (mFimcCurrentOutBufIndex >= HDMI_FIMC_OUTPUT_BUF_NUM)
                mFimcCurrentOutBufIndex = 0;
        }

    } else {
        if (srcColorFormat != HAL_PIXEL_FORMAT_BGRA_8888 &&
            srcColorFormat != HAL_PIXEL_FORMAT_RGBA_8888 &&
            srcColorFormat != HAL_PIXEL_FORMAT_RGB_565) {
            if (mSecFimc.setSrcAddr(srcYAddr, srcCbAddr, srcCrAddr, srcColorFormat) == false) {
                ALOGE("%s::setSrcAddr(%d, %d, %d) fail",
                     __func__, srcYAddr, srcCbAddr, srcCrAddr);
                return false;
            }

            if (mSecFimc.draw(0, mFimcCurrentOutBufIndex) == false) {
                ALOGE("%s::mSecFimc.draw() failed", __func__);
                return false;
            }
#if defined(BOARD_USE_V4L2)
            if (hdmi_set_g_scaling(hdmiLayer,
                            HAL_PIXEL_FORMAT_BGRA_8888,
                            mDstRect.width, mDstRect.height,
                            mHdmiSrcYAddr, &mMixerBuffer[hdmiLayer][0],
                            mDstRect.left , mDstRect.top,
                            mHdmiDstWidth, mHdmiDstHeight,
                            mG2DUIRotVal,
                            num_of_hwc_layer) < 0)
                return false;
#else
            if (hdmi_gl_set_param(hdmiLayer,
                            HAL_PIXEL_FORMAT_BGRA_8888,
                            mDstRect.width, mDstRect.height,
                            mHdmiSrcYAddr, mHdmiSrcCbCrAddr,
                            mDstRect.left , mDstRect.top,
                            mHdmiDstWidth, mHdmiDstHeight,
                            mG2DUIRotVal) < 0)
#endif
                return false;
        } else {
#if CHECK_GRAPHIC_LAYER_TIME
            nsecs_t start, end;
            start = systemTime();
#endif
            if (num_of_hwc_layer == 0) { /* UI only mode */
                struct v4l2_rect rect;

                if (mG2DUIRotVal == 0 || mG2DUIRotVal == 180)
                    hdmi_cal_rect(srcW, srcH, mHdmiDstWidth, mHdmiDstHeight, &rect);
                else
                    hdmi_cal_rect(srcH, srcW, mHdmiDstWidth, mHdmiDstHeight, &rect);

                rect.left = ALIGN(rect.left, 16);

#if defined(BOARD_USE_V4L2)
                if (hdmi_set_g_scaling(hdmiLayer,
                                srcColorFormat,
                                srcW, srcH,
                                srcYAddr, &mMixerBuffer[hdmiLayer][0],
                                rect.left, rect.top,
                                rect.width, rect.height,
                                mG2DUIRotVal,
                                num_of_hwc_layer) < 0)
                    return false;
#else
                if (hdmi_gl_set_param(hdmiLayer,
                                srcColorFormat,
                                srcW, srcH,
                                srcYAddr, srcCbAddr,
                                rect.left, rect.top,
                                rect.width, rect.height,
                                mG2DUIRotVal) < 0)
                    return false;
#endif
            } else { /* Video Playback Mode */
#if defined(BOARD_USE_V4L2)
                if (hdmi_set_g_scaling(hdmiLayer,
                                srcColorFormat,
                                srcW, srcH,
                                srcYAddr, &mMixerBuffer[hdmiLayer][0],
                                dstX, dstY,
                                mHdmiDstWidth, mHdmiDstHeight,
                                mG2DUIRotVal,
                                num_of_hwc_layer) < 0)
                    return false;
#else
                if (hdmi_gl_set_param(hdmiLayer,
                                srcColorFormat,
                                srcW, srcH,
                                srcYAddr, srcCbAddr,
                                dstX, dstY,
                                mHdmiDstWidth, mHdmiDstHeight,
                                mG2DUIRotVal) < 0)
                    return false;
#endif
            }
#if CHECK_GRAPHIC_LAYER_TIME
            end = systemTime();
            ALOGD("[UI] hdmi_gl_set_param[end-start] = %ld ms", long(ns2ms(end)) - long(ns2ms(start)));
#endif
        }
    }

    if (mFlagConnected) {
#if defined(BOARD_USE_V4L2)
        unsigned int num_of_plane;

        if (hdmi_get_src_plane(srcColorFormat, &num_of_plane) < 0) {
            ALOGE("%s::hdmi_get_src_plane(%d) fail", __func__, srcColorFormat);
            return false;
        }

        if (mFlagHdmiStart[hdmiLayer] == false && m_startHdmi(hdmiLayer, num_of_plane) == false) {
            ALOGE("%s::hdmiLayer(%d) fail", __func__, hdmiLayer);
            return false;
        }
#else
        if (mFlagHdmiStart[hdmiLayer] == false && m_startHdmi(hdmiLayer) == false) {
            ALOGE("%s::hdmiLayer(%d) fail", __func__, hdmiLayer);
            return false;
        }
#endif
    }

    return true;
}

bool SecHdmi::clear(int hdmiLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s || hdmiLayer = %d", __func__, hdmiLayer);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }
    if (mFlagHdmiStart[hdmiLayer] == true && m_stopHdmi(hdmiLayer) == false) {
        ALOGE("%s::m_stopHdmi: layer[%d] fail \n", __func__, hdmiLayer);
        return false;
    }
    return true;
}

bool SecHdmi::setHdmiOutputMode(int hdmiOutputMode, bool forceRun)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::hdmiOutputMode = %d, forceRun = %d", __func__, hdmiOutputMode, forceRun);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (forceRun == false && mHdmiOutputMode == hdmiOutputMode) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdmiOutputMode(%d) \n", __func__, hdmiOutputMode);
#endif
        return true;
    }

    int newHdmiOutputMode = hdmiOutputMode;

    int v4l2OutputType = hdmi_outputmode_2_v4l2_output_type(hdmiOutputMode);
    if (v4l2OutputType < 0) {
        ALOGD("%s::hdmi_outputmode_2_v4l2_output_type(%d) fail\n", __func__, hdmiOutputMode);
        return false;
    }

#if defined(BOARD_USES_EDID)
    int newV4l2OutputType = hdmi_check_output_mode(v4l2OutputType);
    if (newV4l2OutputType != v4l2OutputType) {
        newHdmiOutputMode = hdmi_v4l2_output_type_2_outputmode(newV4l2OutputType);
        if (newHdmiOutputMode < 0) {
            ALOGD("%s::hdmi_v4l2_output_type_2_outputmode(%d) fail\n", __func__, newV4l2OutputType);
            return false;
        }

        ALOGD("%s::calibration mode(%d -> %d)... \n", __func__, hdmiOutputMode, newHdmiOutputMode);
        mHdmiInfoChange = true;
    }
#endif

    if (mHdmiOutputMode != newHdmiOutputMode) {
        mHdmiOutputMode = newHdmiOutputMode;
        mHdmiInfoChange = true;
    }

    return true;
}

bool SecHdmi::setHdmiResolution(unsigned int hdmiResolutionValue, bool forceRun)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s:: hdmiResolutionValue = %d, forceRun = %d", __func__, hdmiResolutionValue, forceRun);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (forceRun == false && mHdmiResolutionValue == hdmiResolutionValue) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdmiResolutionValue(%d) \n", __func__, hdmiResolutionValue);
#endif
        return true;
    }

    unsigned int newHdmiResolutionValue = hdmiResolutionValue;
    int w = 0;
    int h = 0;

#if defined(BOARD_USES_EDID)
    // find perfect resolutions..
#if defined(BOARD_USE_V4L2)
    unsigned int preset_id;
    if (hdmi_resolution_2_preset_id(newHdmiResolutionValue, &w, &h, &preset_id) < 0 ||
        hdmi_check_resolution(preset_id) < 0) {
        bool flagFoundIndex = false;
        int resolutionValueIndex = m_resolutionValueIndex(newHdmiResolutionValue);

        for (int i = resolutionValueIndex + 1; i < mHdmiSizeOfResolutionValueList; i++) {
            if (hdmi_resolution_2_preset_id(mHdmiResolutionValueList[i], &w, &h, &preset_id) == 0 &&
                hdmi_check_resolution(preset_id) == 0) {
                newHdmiResolutionValue = mHdmiResolutionValueList[i];
                flagFoundIndex = true;
                break;
            }
        }

        if (flagFoundIndex == false) {
            ALOGE("%s::hdmi cannot control this resolution(%d) fail \n", __func__, hdmiResolutionValue);
            // Set resolution to 480P
            newHdmiResolutionValue = mHdmiResolutionValueList[mHdmiSizeOfResolutionValueList-2];
        } else {
            ALOGD("%s::HDMI resolutions size is calibrated(%d -> %d)..\n", __func__, hdmiResolutionValue, newHdmiResolutionValue);
        }
    }
#else
    v4l2_std_id std_id;
    if (hdmi_resolution_2_std_id(newHdmiResolutionValue, &w, &h, &std_id) < 0 ||
        hdmi_check_resolution(std_id) < 0) {
        bool flagFoundIndex = false;
        int resolutionValueIndex = m_resolutionValueIndex(newHdmiResolutionValue);

        for (int i = resolutionValueIndex + 1; i < mHdmiSizeOfResolutionValueList; i++) {
            if (hdmi_resolution_2_std_id(mHdmiResolutionValueList[i], &w, &h, &std_id) == 0 &&
                hdmi_check_resolution(std_id) == 0) {
                newHdmiResolutionValue = mHdmiResolutionValueList[i];
                flagFoundIndex = true;
                break;
            }
        }

        if (flagFoundIndex == false) {
            ALOGE("%s::hdmi cannot control this resolution(%d) fail \n", __func__, hdmiResolutionValue);
            // Set resolution to 480P
            newHdmiResolutionValue = mHdmiResolutionValueList[mHdmiSizeOfResolutionValueList-2];
        } else {
            ALOGD("%s::HDMI resolutions size is calibrated(%d -> %d)..\n", __func__, hdmiResolutionValue, newHdmiResolutionValue);
        }
    }
#endif
    else {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::find resolutions(%d) at once\n", __func__, hdmiResolutionValue);
#endif
    }
#endif

    if (mHdmiResolutionValue != newHdmiResolutionValue) {
        mHdmiResolutionValue = newHdmiResolutionValue;
        mHdmiInfoChange = true;
    }

    return true;
}

bool SecHdmi::setHdcpMode(bool hdcpMode, bool forceRun)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (forceRun == false && mHdcpMode == hdcpMode) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdcpMode(%d) \n", __func__, hdcpMode);
#endif
        return true;
    }

    mHdcpMode = hdcpMode;
    mHdmiInfoChange = true;

    return true;
}

bool SecHdmi::setUIRotation(unsigned int rotVal, unsigned int hwcLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (rotVal % 90 != 0) {
        ALOGE("%s::Invalid rotation value(%d)", __func__, rotVal);
        return false;
    }

    /* G2D rotation */
    if (rotVal != mG2DUIRotVal) {
        mG2DUIRotVal = rotVal;
        mHdmiInfoChange = true;
    }

    /* FIMC rotation */
    if (hwcLayer == 0) { /* Rotate in UI only mode */
        if (rotVal != mUIRotVal) {
            mSecFimc.setRotVal(rotVal);
            mUIRotVal = rotVal;
            mHdmiInfoChange = true;
        }
    } else { /* Don't rotate video layer when video is played. */
        rotVal = 0;
        if (rotVal != mUIRotVal) {
            mSecFimc.setRotVal(rotVal);
            mUIRotVal = rotVal;
            mHdmiInfoChange = true;
        }
    }

    return true;
}

bool SecHdmi::setDisplaySize(int width, int height)
{
    mDisplayWidth = width;
    mDisplayHeight = height;

    return true;
}

bool SecHdmi::m_reset(int w, int h, int colorFormat, int hdmiLayer, int hwcLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called", __func__);
#endif
    v4l2_std_id std_id = 0;
    mFimcCurrentOutBufIndex = 0;

    int srcW = w;
    int srcH = h;

#if defined(BOARD_USE_V4L2)
    if (mFlagHdmiStart[hdmiLayer] == true && m_stopHdmi(hdmiLayer) == false) {
        ALOGE("%s::m_stopHdmi: layer[%d] fail", __func__, hdmiLayer);
        return false;
    }
#else
    // stop all..
    for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
        if (mFlagHdmiStart[layer] == true && m_stopHdmi(layer) == false) {
            ALOGE("%s::m_stopHdmi: layer[%d] fail", __func__, layer);
            return false;
        }
    }
#endif

#if defined(BOARD_USE_V4L2)
    if (hdmi_deinit_layer(hdmiLayer) < 0)
        ALOGE("%s::hdmi_deinit_layer(%d) fail", __func__, hdmiLayer);

    mHdmiFd[hdmiLayer] = hdmi_init_layer(hdmiLayer);
    if (mHdmiFd[hdmiLayer] < 0)
        ALOGE("%s::hdmi_init_layer(%d) fail", __func__, hdmiLayer);

    if (tvout_std_v4l2_init(mHdmiFd[hdmiLayer], mHdmiPresetId) < 0)
        ALOGE("%s::tvout_std_v4l2_init fail", __func__);
#endif

    if (w != mSrcWidth [hdmiLayer] ||
        h != mSrcHeight [hdmiLayer] ||
        mHdmiDstWidth != mHdmiResolutionWidth[hdmiLayer] ||
        mHdmiDstHeight != mHdmiResolutionHeight[hdmiLayer] ||
#if defined(BOARD_USE_V4L2)
        mDstWidth[hdmiLayer] != mPrevDstWidth[hdmiLayer] ||
        mDstHeight[hdmiLayer] != mPrevDstHeight[hdmiLayer] ||
#endif
        colorFormat != mSrcColorFormat[hdmiLayer]) {
        int preVideoSrcColorFormat = mSrcColorFormat[hdmiLayer];
        int videoSrcColorFormat = colorFormat;

        if (preVideoSrcColorFormat != HAL_PIXEL_FORMAT_YCbCr_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED) {
                ALOGI("%s: Unsupported preVideoSrcColorFormat = 0x%x\n", __func__, preVideoSrcColorFormat);
                preVideoSrcColorFormat = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED;
        }

        if (hdmiLayer == HDMI_LAYER_VIDEO) {
            if (colorFormat != HAL_PIXEL_FORMAT_YCbCr_420_SP &&
                colorFormat != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
                colorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP &&
                colorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP &&
                colorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED) {
#ifdef DEBUG_HDMI_HW_LEVEL
                ALOGD("### %s  call mSecFimc.setSrcParams\n", __func__);
#endif
                unsigned int full_wdith = ALIGN(w, 16);
                unsigned int full_height = ALIGN(h, 2);

                if (mSecFimc.setSrcParams(full_wdith, full_height, 0, 0,
                            (unsigned int*)&w, (unsigned int*)&h, colorFormat, true) == false) {
                    ALOGE("%s::mSecFimc.setSrcParams(%d, %d, %d) fail \n",
                            __func__, w, h, colorFormat);
                    return false;
                }

                mFimcDstColorFormat = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED;

#ifdef DEBUG_HDMI_HW_LEVEL
                ALOGD("### %s  call mSecFimc.setDstParams\n", __func__);
#endif
                if (mUIRotVal == 0 || mUIRotVal == 180) {
                    if (mSecFimc.setDstParams((unsigned int)w, (unsigned int)h, 0, 0,
                                (unsigned int*)&w, (unsigned int*)&h, mFimcDstColorFormat, true) == false) {
                        ALOGE("%s::mSecFimc.setDstParams(%d, %d, %d) fail \n",
                                __func__, w, h, mFimcDstColorFormat);
                        return false;
                    }
#if defined(BOARD_USE_V4L2)
                    hdmi_set_v_param(mHdmiFd[hdmiLayer], hdmiLayer,
                                    mFimcDstColorFormat, srcW, srcH,
                                    &mMixerBuffer[hdmiLayer][0],
                                    0, 0, mHdmiDstWidth, mHdmiDstHeight);
#endif
                } else {
                    if (mSecFimc.setDstParams((unsigned int)h, (unsigned int)w, 0, 0,
                                (unsigned int*)&h, (unsigned int*)&w, mFimcDstColorFormat, true) == false) {
                        ALOGE("%s::mSecFimc.setDstParams(%d, %d, %d) fail \n",
                                __func__, w, h, mFimcDstColorFormat);
                        return false;
                    }
#if defined(BOARD_USE_V4L2)
                    hdmi_set_v_param(mHdmiFd[hdmiLayer], hdmiLayer,
                                    mFimcDstColorFormat, srcH, srcW,
                                    &mMixerBuffer[hdmiLayer][0],
                                    0, 0, mHdmiDstWidth, mHdmiDstHeight);
#endif
                }
            }
#if defined(BOARD_USE_V4L2)
            else {
                hdmi_set_v_param(mHdmiFd[hdmiLayer], hdmiLayer,
                                colorFormat, srcW, srcH,
                                &mMixerBuffer[hdmiLayer][0],
                                0, 0, mHdmiDstWidth, mHdmiDstHeight);
            }
#endif
            mPrevDstWidth[hdmiLayer] = mHdmiDstWidth;
            mPrevDstHeight[hdmiLayer] = mHdmiDstHeight;
        } else {
#if defined(BOARD_USE_V4L2)
            struct v4l2_rect rect;
            int tempSrcW, tempSrcH;

            if (mG2DUIRotVal == 0 || mG2DUIRotVal == 180) {
                tempSrcW = srcW;
                tempSrcH = srcH;
            } else {
                tempSrcW = srcH;
                tempSrcH = srcW;
            }

            hdmi_cal_rect(tempSrcW, tempSrcH, mHdmiDstWidth, mHdmiDstHeight, &rect);
            rect.left = ALIGN(rect.left, 16);

            if (hwcLayer == 0) { /* UI only mode */
                hdmi_set_g_param(mHdmiFd[hdmiLayer], hdmiLayer,
                                colorFormat, srcW, srcH,
                                &mMixerBuffer[hdmiLayer][0],
                                rect.left, rect.top, rect.width, rect.height);
                mPrevDstWidth[hdmiLayer] = rect.width;
                mPrevDstHeight[hdmiLayer] = rect.height;
                mPrevDstWidth[HDMI_LAYER_VIDEO] = 0;
                mPrevDstHeight[HDMI_LAYER_VIDEO] = 0;
            } else { /* Video Playback + UI Mode */
                hdmi_set_g_param(mHdmiFd[hdmiLayer], hdmiLayer,
                                colorFormat, srcW, srcH,
                                &mMixerBuffer[hdmiLayer][0],
                                0, 0, mHdmiDstWidth, mHdmiDstHeight);
                mPrevDstWidth[hdmiLayer] = mHdmiDstWidth;
                mPrevDstHeight[hdmiLayer] = mHdmiDstHeight;
            }
#endif
        }

        if (preVideoSrcColorFormat != videoSrcColorFormat)
            mHdmiInfoChange = true;

        mSrcWidth[hdmiLayer] = srcW;
        mSrcHeight[hdmiLayer] = srcH;
        mSrcColorFormat[hdmiLayer] = colorFormat;

        mHdmiResolutionWidth[hdmiLayer] = mHdmiDstWidth;
        mHdmiResolutionHeight[hdmiLayer] = mHdmiDstHeight;

#ifdef DEBUG_MSG_ENABLE
        ALOGD("m_reset saved param(%d, %d, %d, %d, %d, %d, %d) \n",
            srcW, mSrcWidth[hdmiLayer], \
            srcH, mSrcHeight[hdmiLayer], \
            colorFormat,mSrcColorFormat[hdmiLayer], \
            hdmiLayer);
#endif
    }

    if (mHdmiInfoChange == true) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("mHdmiInfoChange: %d\n", mHdmiInfoChange);
#endif
        // stop all..
#if defined(BOARD_USES_CEC)
        if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
            if (mCECThread->mFlagRunning)
                mCECThread->stop();
        }
#endif

        if (m_setHdmiOutputMode(mHdmiOutputMode) == false) {
            ALOGE("%s::m_setHdmiOutputMode() fail \n", __func__);
            return false;
        }
        if (mHdmiOutputMode == COMPOSITE_OUTPUT_MODE) {
            std_id = composite_std_2_v4l2_std_id(mCompositeStd);
            if ((int)std_id < 0) {
                ALOGE("%s::composite_std_2_v4l2_std_id(%d) fail\n", __func__, mCompositeStd);
                return false;
            }
            if (m_setCompositeResolution(mCompositeStd) == false) {
                ALOGE("%s::m_setCompositeRsolution() fail \n", __func__);
                return false;
            }
        } else if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
                   mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
            if (m_setHdmiResolution(mHdmiResolutionValue) == false) {
                ALOGE("%s::m_setHdmiResolution() fail \n", __func__);
                return false;
            }

            if (m_setHdcpMode(mHdcpMode) == false) {
                ALOGE("%s::m_setHdcpMode() fail \n", __func__);
                return false;
            }
#if !defined(BOARD_USE_V4L2)
            std_id = mHdmiStdId;
#endif
        }

#if !defined(BOARD_USE_V4L2)
        fp_tvout = tvout_init(std_id);

        for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
            if (hdmi_deinit_layer(layer) < 0)
                ALOGE("%s::hdmi_init_layer(%d) fail \n", __func__, layer);
        }

        for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
            if (hdmi_init_layer(layer) < 0)
                ALOGE("%s::hdmi_init_layer(%d) fail \n", __func__, layer);
        }
#endif

        if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
#if defined(BOARD_USES_CEC)
            if (!(mCECThread->mFlagRunning))
                mCECThread->start();
#endif

            if (m_setAudioMode(mAudioMode) == false)
                ALOGE("%s::m_setAudioMode() fail \n", __func__);
        }

        mHdmiInfoChange = false;
#ifdef BOARD_USE_V4L2
        for (int i = 0; i < HDMI_FIMC_OUTPUT_BUF_NUM; i++)
            mFimcReservedMem[i] = *(mSecFimc.getMemAddr(i));
#endif
    }

    return true;
}

#if defined(BOARD_USE_V4L2)
bool SecHdmi::m_startHdmi(int hdmiLayer, unsigned int num_of_plane)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    bool ret = true;
    int buf_index = 0;

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s: hdmiLayer(%d) called\n", __func__, hdmiLayer);
#endif

    if (mFlagLayerEnable[hdmiLayer]) {
        static unsigned int index = 0;

        if (mFlagHdmiStart[hdmiLayer] == false) {
            index = 0;
            if (tvout_std_v4l2_qbuf(mHdmiFd[hdmiLayer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR,
                                    index, num_of_plane, &mMixerBuffer[hdmiLayer][0]) < 0) {
                ALOGE("%s::tvout_std_v4l2_qbuf(index : %d) (mSrcBufNum : %d) failed", __func__, index, HDMI_NUM_MIXER_BUF);
                return false;
            }
            index++;

            if (tvout_std_v4l2_streamon(mHdmiFd[hdmiLayer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0) {
                ALOGE("%s::tvout_std_v4l2_streamon() failed", __func__);
                return false;
            }

            mFlagHdmiStart[hdmiLayer] = true;
        } else {
            if (tvout_std_v4l2_qbuf(mHdmiFd[hdmiLayer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR,
                                    index, num_of_plane, &mMixerBuffer[hdmiLayer][0]) < 0) {
                ALOGE("%s::tvout_std_v4l2_qbuf() failed", __func__);
                return false;
            }

            if (tvout_std_v4l2_dqbuf(mHdmiFd[hdmiLayer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR, &buf_index, num_of_plane) < 0) {
                ALOGE("%s::tvout_std_v4l2_dqbuf() failed", __func__);
                return false;
            }
            index = buf_index;
        }
    }

    return true;
}
#else
bool SecHdmi::m_startHdmi(int hdmiLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    bool ret = true;
    int buf_index = 0;

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s: hdmiLayer(%d) called\n", __func__, hdmiLayer);
#endif

    switch (hdmiLayer) {
    case HDMI_LAYER_VIDEO:
        tvout_v4l2_start_overlay(fp_tvout_v);
        mFlagHdmiStart[hdmiLayer] = true;
        break;
    case HDMI_LAYER_GRAPHIC_0 :
        if (mFlagLayerEnable[hdmiLayer]) {
            if (ioctl(fp_tvout_g0, FBIOBLANK, (void *)FB_BLANK_UNBLANK) != -1)
                mFlagHdmiStart[hdmiLayer] = true;
        }
        break;
    case HDMI_LAYER_GRAPHIC_1 :
        if (mFlagLayerEnable[hdmiLayer]) {
            if (ioctl(fp_tvout_g1, FBIOBLANK, (void *)FB_BLANK_UNBLANK) != -1)
                mFlagHdmiStart[hdmiLayer] = true;
        }
        break;
    default :
        ALOGE("%s::unmathced layer(%d) fail", __func__, hdmiLayer);
        ret = false;
        break;
    }

    return true;
}
#endif

bool SecHdmi::m_stopHdmi(int hdmiLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    bool ret = true;
    if (mFlagHdmiStart[hdmiLayer] == false) {
        ALOGD("%s::already HDMI(%d layer) stopped.. \n", __func__, hdmiLayer);
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s : layer[%d] called\n", __func__, hdmiLayer);
#endif

#if defined(BOARD_USE_V4L2)
    int fd;

    switch (hdmiLayer) {
    case HDMI_LAYER_VIDEO:
        break;
    case HDMI_LAYER_GRAPHIC_0 :
        break;
    case HDMI_LAYER_GRAPHIC_1 :
#if defined(BOARD_USES_FIMGAPI)
        cur_g2d_address = 0;
        g2d_buf_index = 0;
#endif
        break;
    default :
        ALOGE("%s::unmathced layer(%d) fail", __func__, hdmiLayer);
        ret = false;
        break;
    }

    if (mFlagLayerEnable[hdmiLayer]) {
        if (tvout_std_v4l2_streamoff(mHdmiFd[hdmiLayer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0) {
            ALOGE("%s::tvout_std_v4l2_streamon layer(%d) failed", __func__, hdmiLayer);
            return false;
        }

        /* clear buffer */
        if (tvout_std_v4l2_reqbuf(mHdmiFd[hdmiLayer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR, 0) < 0) {
            ALOGE("%s::tvout_std_v4l2_reqbuf(buf_num=%d)[graphic layer] failed", __func__, 0);
            return -1;
        }

        mFlagHdmiStart[hdmiLayer] = false;
    }
#else
    switch (hdmiLayer) {
    case HDMI_LAYER_VIDEO:
        tvout_v4l2_stop_overlay(fp_tvout_v);
        mFlagHdmiStart[hdmiLayer] = false;
        break;
    case HDMI_LAYER_GRAPHIC_0 :
        if (mFlagLayerEnable[hdmiLayer]) {
            if (ioctl(fp_tvout_g0, FBIOBLANK, (void *)FB_BLANK_POWERDOWN) != -1)
                mFlagHdmiStart[hdmiLayer] = false;
        }
        break;
    case HDMI_LAYER_GRAPHIC_1 :
#if defined(BOARD_USES_FIMGAPI)
        cur_g2d_address = 0;
        g2d_buf_index = 0;
#endif
        if (mFlagLayerEnable[hdmiLayer]) {
            if (ioctl(fp_tvout_g1, FBIOBLANK, (void *)FB_BLANK_POWERDOWN) != -1)
                mFlagHdmiStart[hdmiLayer] = false;
        }
        break;
    default :
        ALOGE("%s::unmathced layer(%d) fail", __func__, hdmiLayer);
        ret = false;
        break;
    }
#endif

    return true;
}

bool SecHdmi::m_setHdmiOutputMode(int hdmiOutputMode)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    if (hdmiOutputMode == mCurrentHdmiOutputMode) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdmiOutputMode(%d) \n", __func__, hdmiOutputMode);
#endif
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    int v4l2OutputType = hdmi_outputmode_2_v4l2_output_type(hdmiOutputMode);
    if (v4l2OutputType < 0) {
        ALOGE("%s::hdmi_outputmode_2_v4l2_output_type(%d) fail\n", __func__, hdmiOutputMode);
        return false;
    }

    output_type = v4l2OutputType;

    mCurrentHdmiOutputMode = hdmiOutputMode;

    return true;
}

bool SecHdmi::m_setCompositeResolution(unsigned int compositeStdId)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    int w = 0;
    int h = 0;

    if (mHdmiOutputMode != COMPOSITE_OUTPUT_MODE) {
        ALOGE("%s:: not supported output type \n", __func__);
        return false;
    }

    switch (compositeStdId) {
    case COMPOSITE_STD_NTSC_M:
    case COMPOSITE_STD_NTSC_443:
        w = 704;
        h = 480;
        break;
    case COMPOSITE_STD_PAL_BDGHI:
    case COMPOSITE_STD_PAL_M:
    case COMPOSITE_STD_PAL_N:
    case COMPOSITE_STD_PAL_Nc:
    case COMPOSITE_STD_PAL_60:
        w = 704;
        h = 576;
        break;
    default:
        ALOGE("%s::unmathced composite_std(%d)", __func__, compositeStdId);
        return false;
    }

    t_std_id      = composite_std_2_v4l2_std_id(mCompositeStd);

    mHdmiDstWidth  = w;
    mHdmiDstHeight = h;

    mCurrentHdmiResolutionValue = -1;
    return true;
}

bool SecHdmi::m_setHdmiResolution(unsigned int hdmiResolutionValue)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    if (hdmiResolutionValue == mCurrentHdmiResolutionValue) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdmiResolutionValue(%d) \n", __func__, hdmiResolutionValue);
#endif
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    int w = 0;
    int h = 0;

#if defined(BOARD_USE_V4L2)
    unsigned int preset_id;
#else
    v4l2_std_id std_id;
#endif

    if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
        mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
#if defined(BOARD_USE_V4L2)
        if (hdmi_resolution_2_preset_id(hdmiResolutionValue, &w, &h, &preset_id) < 0) {
            ALOGE("%s::hdmi_resolution_2_std_id(%d) fail\n", __func__, hdmiResolutionValue);
            return false;
        }
        mHdmiPresetId    = preset_id;
#else
        if (hdmi_resolution_2_std_id(hdmiResolutionValue, &w, &h, &std_id) < 0) {
            ALOGE("%s::hdmi_resolution_2_std_id(%d) fail\n", __func__, hdmiResolutionValue);
            return false;
        }
        mHdmiStdId    = std_id;
#endif
    } else {
        ALOGE("%s:: not supported output type \n", __func__);
        return false;
    }

#if defined(BOARD_USE_V4L2)
    g_preset_id   = preset_id;
#else
    t_std_id      = std_id;
#endif

    mHdmiDstWidth  = w;
    mHdmiDstHeight = h;

    mCurrentHdmiResolutionValue = hdmiResolutionValue;

#ifdef DEBUG_HDMI_HW_LEVEL
#if defined(BOARD_USE_V4L2)
        ALOGD("%s:: mHdmiDstWidth = %d, mHdmiDstHeight = %d, mHdmiPresetId = 0x%x, hdmiResolutionValue = 0x%x\n",
                __func__,
                mHdmiDstWidth,
                mHdmiDstHeight,
                mHdmiPresetId,
                hdmiResolutionValue);
#else
        ALOGD("%s:: mHdmiDstWidth = %d, mHdmiDstHeight = %d, mHdmiStdId = 0x%x, hdmiResolutionValue = 0x%x\n",
                __func__,
                mHdmiDstWidth,
                mHdmiDstHeight,
                mHdmiStdId,
                hdmiResolutionValue);
#endif
#endif

    return true;
}

bool SecHdmi::m_setHdcpMode(bool hdcpMode)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    if (hdcpMode == mCurrentHdcpMode) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdcpMode(%d) \n", __func__, hdcpMode);
#endif

        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    if (hdcpMode == true)
        g_hdcp_en = 1;
    else
        g_hdcp_en = 0;

    mCurrentHdcpMode = hdcpMode;

    return true;
}

bool SecHdmi::m_setAudioMode(int audioMode)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    if (audioMode == mCurrentAudioMode) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same audioMode(%d) \n", __func__, audioMode);
#endif
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    if (hdmi_check_audio() < 0) {
        ALOGE("%s::hdmi_check_audio() fail \n", __func__);
        return false;
    }

    mCurrentAudioMode = audioMode;

    return true;
}

int SecHdmi::m_resolutionValueIndex(unsigned int ResolutionValue)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    int index = -1;

    for (int i = 0; i < mHdmiSizeOfResolutionValueList; i++) {
        if (mHdmiResolutionValueList[i] == ResolutionValue) {
            index = i;
            break;
        }
    }
    return index;
}

bool SecHdmi::m_flagHWConnected(void)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    bool ret = true;
    int hdmiStatus = hdmi_cable_status();

    if (hdmiStatus <= 0) {
#ifdef DEBUG_HDMI_HW_LEVEL
            ALOGD("%s::hdmi_cable_status() fail \n", __func__);
#endif
        ret = false;
    } else {
        ret = true;
    }

    return ret;
}

}; // namespace android
