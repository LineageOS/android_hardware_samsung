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
//#define LOG_NDEBUG 0
#define LOG_TAG "CameraHardwareSec"
#include <utils/Log.h>

#include "SecCameraHWInterface.h"
#include <utils/threads.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <camera/Camera.h>
#include <media/hardware/MetadataBufferType.h>

#define VIDEO_COMMENT_MARKER_H          0xFFBE
#define VIDEO_COMMENT_MARKER_L          0xFFBF
#define VIDEO_COMMENT_MARKER_LENGTH     4
#define JPEG_EOI_MARKER                 0xFFD9
#define HIBYTE(x) (((x) >> 8) & 0xFF)
#define LOBYTE(x) ((x) & 0xFF)

#define BACK_CAMERA_AUTO_FOCUS_DISTANCES_STR       "0.10,1.20,Infinity"
#define BACK_CAMERA_MACRO_FOCUS_DISTANCES_STR      "0.10,0.20,Infinity"
#define BACK_CAMERA_INFINITY_FOCUS_DISTANCES_STR   "0.10,1.20,Infinity"
#define FRONT_CAMERA_FOCUS_DISTANCES_STR           "0.20,0.25,Infinity"
#define USE_EGL

// This hack does two things:
// -- it sets preview to NV21 (YUV420SP)
// -- it sets gralloc to YV12
//
// The reason being: the samsung encoder understands only yuv420sp, and gralloc
// does yv12 and rgb565.  So what we do is we break up the interleaved UV in
// separate V and U planes, which makes preview look good, and enabled the
// encoder as well.
//
// FIXME: Samsung needs to enable support for proper yv12 coming out of the
//        camera, and to fix their video encoder to work with yv12.
// FIXME: It also seems like either Samsung's YUV420SP (NV21) or img's YV12 has
//        the color planes switched.  We need to figure which side is doing it
//        wrong and have the respective party fix it.

namespace android {

struct addrs {
    uint32_t type;  // make sure that this is 4 byte.
    unsigned int addr_y;
    unsigned int addr_cbcr;
    unsigned int buf_index;
    unsigned int reserved;
};

struct addrs_cap {
    unsigned int addr_y;
    unsigned int width;
    unsigned int height;
};

static const int INITIAL_SKIP_FRAME = 3;
static const int EFFECT_SKIP_FRAME = 1;

gralloc_module_t const* CameraHardwareSec::mGrallocHal;

CameraHardwareSec::CameraHardwareSec(int cameraId, camera_device_t *dev)
        :
          mCaptureInProgress(false),
          mParameters(),
          mFrameSizeDelta(0),
          mCameraSensorName(NULL),
          mUseInternalISP(false),
          mSkipFrame(0),
          mNotifyCb(0),
          mDataCb(0),
          mDataCbTimestamp(0),
          mCallbackCookie(0),
          mMsgEnabled(CAMERA_MSG_RAW_IMAGE),
          mRecordRunning(false),
          mPostViewWidth(0),
          mPostViewHeight(0),
          mPostViewSize(0),
          mCapIndex(0),
          mRecordHint(false),
          mTouched(0),
          mHalDevice(dev)
{
    ALOGV("%s :", __func__);
    memset(&mCapBuffer, 0, sizeof(struct SecBuffer));
    int ret = 0;

    mPreviewWindow = NULL;
    mSecCamera = SecCamera::createInstance();

    mRawHeap = NULL;
    mPreviewHeap = NULL;
    for(int i = 0; i < BUFFER_COUNT_FOR_ARRAY; i++)
        mRecordHeap[i] = NULL;

    if (!mGrallocHal) {
        ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **)&mGrallocHal);
        if (ret)
            ALOGE("ERR(%s):Fail on loading gralloc HAL", __func__);
    }

    ret = mSecCamera->CreateCamera(cameraId);
    if (ret < 0) {
        ALOGE("ERR(%s):Fail on mSecCamera init", __func__);
        mSecCamera->DestroyCamera();
    }

    initDefaultParameters(cameraId);

    mExitAutoFocusThread = false;
    mExitPreviewThread = false;
    /* whether the PreviewThread is active in preview or stopped.  we
     * create the thread but it is initially in stopped state.
     */
    mPreviewRunning = false;
    mPreviewStartDeferred = false;
    mPreviewThread = new PreviewThread(this);
    mAutoFocusThread = new AutoFocusThread(this);
    mPictureThread = new PictureThread(this);
}

int CameraHardwareSec::getCameraId() const
{
    return mSecCamera->getCameraId();
}

void CameraHardwareSec::initDefaultParameters(int cameraId)
{
    if (mSecCamera == NULL) {
        ALOGE("ERR(%s):mSecCamera object is NULL", __func__);
        return;
    }

    CameraParameters p;
    CameraParameters ip;

    mCameraSensorName = mSecCamera->getCameraSensorName();
    if (mCameraSensorName == NULL) {
        ALOGE("ERR(%s):mCameraSensorName is NULL", __func__);
        return;
    }
    ALOGV("CameraSensorName: %s", mCameraSensorName);

    int preview_max_width   = 0;
    int preview_max_height  = 0;
    int snapshot_max_width  = 0;
    int snapshot_max_height = 0;

    mCameraID = cameraId;
    mUseInternalISP = mSecCamera->getUseInternalISP();

    if (cameraId == SecCamera::CAMERA_ID_BACK) {
        if (mUseInternalISP) {
            //3H2
            p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,
                  "720x480,640x384,640x360,640x480,320x240,528x432,176x144");
            p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES,
                  "3248x2436,3216x2144,3200x1920,3072x1728,2592x1944,1920x1080,1440x1080,1280x720,1232x1008,800x480,720x480,640x480");
            p.set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES,
                  "1920x1080,1280x720,640x480,176x144");
        } else {
            //M5MO
            p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,
                  "3264x2448,1920x1080,1280x720,800x480,720x480,640x480,320x240,528x432,176x144");
            p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES,
                  "3264x2448,3264x1968,2048x1536,2048x1232,800x480,640x480");
        }
    } else {
        if (mUseInternalISP) {
            //6A3
            p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,
                  "640x480,640x360,480x480,352x288,320x240,176x144");
            p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES,
                  "1392x1392,1280x960,1280x720,880x720,640x480");
            p.set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES,
                  "1280x720,640x480,176x144");
        }
    }

    p.getSupportedPreviewSizes(mSupportedPreviewSizes);

    String8 parameterString;

    // If these fail, then we are using an invalid cameraId and we'll leave the
    // sizes at zero to catch the error.
    if (mSecCamera->getPreviewMaxSize(&preview_max_width,
                                      &preview_max_height) < 0)
        ALOGE("getPreviewMaxSize fail (%d / %d)",
             preview_max_width, preview_max_height);
    if (mSecCamera->getSnapshotMaxSize(&snapshot_max_width,
                                       &snapshot_max_height) < 0)
        ALOGE("getSnapshotMaxSize fail (%d / %d)",
             snapshot_max_width, snapshot_max_height);

    parameterString = CameraParameters::PIXEL_FORMAT_YUV420P;
    parameterString.append(",");
    parameterString.append(CameraParameters::PIXEL_FORMAT_YUV420SP);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, parameterString);
    p.setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV420P);
    mFrameSizeDelta = 16;
    p.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420SP);
    p.setPreviewSize(preview_max_width, preview_max_height);

    p.setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);
    p.setPictureSize(snapshot_max_width, snapshot_max_height);
    p.set(CameraParameters::KEY_JPEG_QUALITY, "100"); // maximum quality
    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,
          CameraParameters::PIXEL_FORMAT_JPEG);

    p.set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, "1280x720");

#ifdef USE_FACE_DETECTION
    if (mUseInternalISP) {
        p.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW, "5");
    } else {
        p.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW, "0");
    }
#endif

    if (cameraId == SecCamera::CAMERA_ID_BACK) {
        parameterString = CameraParameters::FOCUS_MODE_AUTO;
        /* TODO : sensor will be support this mode */
        //parameterString.append(",");
        //parameterString.append(CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO);
        if (mUseInternalISP) {
            parameterString.append(",");
            parameterString.append(CameraParameters::FOCUS_MODE_INFINITY);
            parameterString.append(",");
            parameterString.append(CameraParameters::FOCUS_MODE_MACRO);
            parameterString.append(",");
            parameterString.append(CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE);
        }
        p.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES,
              parameterString.string());
        p.set(CameraParameters::KEY_FOCUS_MODE,
              CameraParameters::FOCUS_MODE_AUTO);
        p.set(CameraParameters::KEY_FOCUS_DISTANCES,
              BACK_CAMERA_AUTO_FOCUS_DISTANCES_STR);
#ifdef USE_TOUCH_AF
        if (mUseInternalISP)
            p.set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS, "1");
#endif
        p.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES,
              "320x240,0x0");
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, "320");
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, "240");
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, "7,15,30");
        p.setPreviewFrameRate(30);
    } else {
        p.set(CameraParameters::KEY_FOCUS_MODE, NULL);
        p.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES,
              "160x120,0x0");
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, "160");
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, "120");
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,
                    "7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,50,60");
        p.setPreviewFrameRate(30);
    }

    parameterString = CameraParameters::EFFECT_NONE;
    parameterString.append(",");
    parameterString.append(CameraParameters::EFFECT_MONO);
    parameterString.append(",");
    parameterString.append(CameraParameters::EFFECT_NEGATIVE);
    parameterString.append(",");
    parameterString.append(CameraParameters::EFFECT_SEPIA);
    p.set(CameraParameters::KEY_SUPPORTED_EFFECTS, parameterString.string());

    if (cameraId == SecCamera::CAMERA_ID_BACK) {
        parameterString = CameraParameters::FLASH_MODE_ON;
        parameterString.append(",");
        parameterString.append(CameraParameters::FLASH_MODE_OFF);
        parameterString.append(",");
        parameterString.append(CameraParameters::FLASH_MODE_AUTO);
        parameterString.append(",");
        parameterString.append(CameraParameters::FLASH_MODE_TORCH);
        p.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES,
              parameterString.string());
        p.set(CameraParameters::KEY_FLASH_MODE,
              CameraParameters::FLASH_MODE_OFF);

        /* we have two ranges, 4-30fps for night mode and
         * 15-30fps for all others
         */
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(15000,30000)");
        p.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "15000,30000");

        p.set(CameraParameters::KEY_FOCAL_LENGTH, "3.43");
    } else {
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(7500,30000)");
        p.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "7500,30000");

        p.set(CameraParameters::KEY_FOCAL_LENGTH, "0.9");
    }
    parameterString = CameraParameters::SCENE_MODE_AUTO;
    parameterString.append(",");
    parameterString.append(CameraParameters::SCENE_MODE_PORTRAIT);
    parameterString.append(",");
    parameterString.append(CameraParameters::SCENE_MODE_LANDSCAPE);
    parameterString.append(",");
    parameterString.append(CameraParameters::SCENE_MODE_BEACH);
    parameterString.append(",");
    parameterString.append(CameraParameters::SCENE_MODE_SNOW);
    parameterString.append(",");
    parameterString.append(CameraParameters::SCENE_MODE_FIREWORKS);
    parameterString.append(",");
    parameterString.append(CameraParameters::SCENE_MODE_SPORTS);
    parameterString.append(",");
    parameterString.append(CameraParameters::SCENE_MODE_PARTY);
    parameterString.append(",");
    parameterString.append(CameraParameters::SCENE_MODE_CANDLELIGHT);
    parameterString.append(",");
    parameterString.append(CameraParameters::SCENE_MODE_NIGHT);
    parameterString.append(",");
    parameterString.append(CameraParameters::SCENE_MODE_SUNSET);
    p.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES,
          parameterString.string());
    p.set(CameraParameters::KEY_SCENE_MODE,
          CameraParameters::SCENE_MODE_AUTO);

    parameterString = CameraParameters::WHITE_BALANCE_AUTO;
    parameterString.append(",");
    parameterString.append(CameraParameters::WHITE_BALANCE_INCANDESCENT);
    parameterString.append(",");
    parameterString.append(CameraParameters::WHITE_BALANCE_FLUORESCENT);
    parameterString.append(",");
    parameterString.append(CameraParameters::WHITE_BALANCE_DAYLIGHT);
    parameterString.append(",");
    parameterString.append(CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT);
    p.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE,
          parameterString.string());

    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "100");

    p.set(CameraParameters::KEY_ROTATION, 0);
    p.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);

    p.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_NONE);

    p.set("contrast", 0);
    p.set("iso", "auto");
    p.set("metering", "center");
    p.set("wdr", 0);

    ip.set("chk_dataline", 0);
    if (cameraId == SecCamera::CAMERA_ID_FRONT) {
        ip.set("vtmode", 0);
        ip.set("blur", 0);
    }

    p.set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, "51.2");
    p.set(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, "39.4");

    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, "0");
    p.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, "4");
    p.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, "-4");
    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, "1");

    p.set("brightness", 0);
    p.set("brightness-max", 2);
    p.set("brightness-min", -2);

    p.set("saturation", 0);
    p.set("saturation-max", 2);
    p.set("saturation-min", -2);

    p.set("sharpness", 0);
    p.set("sharpness-max", 2);
    p.set("sharpness-min", -2);

    p.set("hue", 0);
    p.set("hue-max", 2);
    p.set("hue-min", -2);

    parameterString = CameraParameters::ANTIBANDING_AUTO;
    parameterString.append(",");
    parameterString.append(CameraParameters::ANTIBANDING_50HZ);
    parameterString.append(",");
    parameterString.append(CameraParameters::ANTIBANDING_60HZ);
    parameterString.append(",");
    parameterString.append(CameraParameters::ANTIBANDING_OFF);
    p.set(CameraParameters::KEY_SUPPORTED_ANTIBANDING,
          parameterString.string());

    p.set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);

    if (mUseInternalISP) {
        p.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, "true");
        p.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK, "false");
    }

    if (mUseInternalISP) {
        p.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, "true");
        p.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, "false");
    }

    p.set(CameraParameters::KEY_RECORDING_HINT, "false");

#ifdef VIDEO_SNAPSHOT
    if (mUseInternalISP)
        p.set(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED, "true");
#endif

    if (!mUseInternalISP) {
        p.set(CameraParameters::KEY_ZOOM_SUPPORTED, "true");
        p.set(CameraParameters::KEY_MAX_ZOOM, ZOOM_LEVEL_MAX - 1);
        p.set(CameraParameters::KEY_ZOOM_RATIOS, "31,4.0");
    }

    mPreviewRunning = false;
    mParameters = p;
    mInternalParameters = ip;

    /* make sure mSecCamera has all the settings we do.  applications
     * aren't required to call setParameters themselves (only if they
     * want to change something.
     */
    setParameters(p);
}

CameraHardwareSec::~CameraHardwareSec()
{
    ALOGV("%s", __func__);
    mSecCamera->DestroyCamera();
}

status_t CameraHardwareSec::setPreviewWindow(preview_stream_ops *w)
{
    int min_bufs;

    mPreviewWindow = w;
    ALOGV("%s: mPreviewWindow %p", __func__, mPreviewWindow);

    if (!w) {
        ALOGE("preview window is NULL!");
        return OK;
    }

    mPreviewLock.lock();

    if (mPreviewRunning && !mPreviewStartDeferred) {
        ALOGI("stop preview (window change)");
        stopPreviewInternal();
    }

    if (w->get_min_undequeued_buffer_count(w, &min_bufs)) {
        ALOGE("%s: could not retrieve min undequeued buffer count", __func__);
        return INVALID_OPERATION;
    }

    if (min_bufs >= BUFFER_COUNT_FOR_GRALLOC) {
        ALOGE("%s: min undequeued buffer count %d is too high (expecting at most %d)", __func__,
             min_bufs, BUFFER_COUNT_FOR_GRALLOC - 1);
    }

    ALOGV("%s: setting buffer count to %d", __func__, BUFFER_COUNT_FOR_GRALLOC);
    if (w->set_buffer_count(w, BUFFER_COUNT_FOR_GRALLOC)) {
        ALOGE("%s: could not set buffer count", __func__);
        return INVALID_OPERATION;
    }

    int preview_width;
    int preview_height;
    mParameters.getPreviewSize(&preview_width, &preview_height);

    int hal_pixel_format;

    const char *str_preview_format = mParameters.getPreviewFormat();
    ALOGV("%s: preview format %s", __func__, str_preview_format);
    mFrameSizeDelta = 16;

    hal_pixel_format = HAL_PIXEL_FORMAT_YV12; // default

    if (!strcmp(str_preview_format,
                CameraParameters::PIXEL_FORMAT_RGB565)) {
        hal_pixel_format = HAL_PIXEL_FORMAT_RGB_565;
        mFrameSizeDelta = 0;
    } else if (!strcmp(str_preview_format,
                     CameraParameters::PIXEL_FORMAT_RGBA8888)) {
        hal_pixel_format = HAL_PIXEL_FORMAT_RGBA_8888;
        mFrameSizeDelta = 0;
    } else if (!strcmp(str_preview_format,
                     CameraParameters::PIXEL_FORMAT_YUV420SP)) {
        hal_pixel_format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    } else if (!strcmp(str_preview_format,
                     CameraParameters::PIXEL_FORMAT_YUV420P))
        hal_pixel_format = HAL_PIXEL_FORMAT_YV12; // HACK

#ifdef USE_EGL
#ifdef BOARD_USE_V4L2_ION
    if (w->set_usage(w, GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_ION)) {
#else
    if (w->set_usage(w, GRALLOC_USAGE_SW_WRITE_OFTEN)) {
#endif
        ALOGE("%s: could not set usage on gralloc buffer", __func__);
        return INVALID_OPERATION;
    }
#else
#ifdef BOARD_USE_V4L2_ION
    if (w->set_usage(w, GRALLOC_USAGE_SW_WRITE_OFTEN
        | GRALLOC_USAGE_HWC_HWOVERLAY | GRALLOC_USAGE_HW_ION)) {
#else
    if (w->set_usage(w, GRALLOC_USAGE_SW_WRITE_OFTEN
        | GRALLOC_USAGE_HW_FIMC1 | GRALLOC_USAGE_HWC_HWOVERLAY)) {
#endif
        ALOGE("%s: could not set usage on gralloc buffer", __func__);
        return INVALID_OPERATION;
    }
#endif

    if (w->set_buffers_geometry(w,
                                preview_width, preview_height,
                                hal_pixel_format)) {
        ALOGE("%s: could not set buffers geometry to %s",
             __func__, str_preview_format);
        return INVALID_OPERATION;
    }

#ifdef BOARD_USE_V4L2_ION
    for(int i = 0; i < BUFFER_COUNT_FOR_ARRAY; i++)
        if (0 != mPreviewWindow->dequeue_buffer(mPreviewWindow, &mBufferHandle[i], &mStride[i])) {
            ALOGE("%s: Could not dequeue gralloc buffer[%d]!!", __func__, i);
            return INVALID_OPERATION;
        }
#endif

    if (mPreviewRunning && mPreviewStartDeferred) {
        ALOGV("start/resume preview");
        status_t ret = startPreviewInternal();
        if (ret == OK) {
            mPreviewStartDeferred = false;
            mPreviewCondition.signal();
        }
    }
    mPreviewLock.unlock();

    return OK;
}

void CameraHardwareSec::setCallbacks(camera_notify_callback notify_cb,
                                     camera_data_callback data_cb,
                                     camera_data_timestamp_callback data_cb_timestamp,
                                     camera_request_memory get_memory,
                                     void *user)
{
    mNotifyCb = notify_cb;
    mDataCb = data_cb;
    mDataCbTimestamp = data_cb_timestamp;
    mGetMemoryCb = get_memory;
    mCallbackCookie = user;
}

void CameraHardwareSec::enableMsgType(int32_t msgType)
{
    ALOGV("%s : msgType = 0x%x, mMsgEnabled before = 0x%x",
         __func__, msgType, mMsgEnabled);
    mMsgEnabled |= msgType;

    mPreviewLock.lock();
    if ((msgType & (CAMERA_MSG_PREVIEW_FRAME | CAMERA_MSG_VIDEO_FRAME)) &&
             mPreviewRunning && mPreviewStartDeferred) {
        ALOGV("%s: starting deferred preview", __func__);
        if (startPreviewInternal() == OK) {
            mPreviewStartDeferred = false;
            mPreviewCondition.signal();
        }
    }
    mPreviewLock.unlock();

    ALOGV("%s : mMsgEnabled = 0x%x", __func__, mMsgEnabled);
}

void CameraHardwareSec::disableMsgType(int32_t msgType)
{
    ALOGV("%s : msgType = 0x%x, mMsgEnabled before = 0x%x",
         __func__, msgType, mMsgEnabled);
    mMsgEnabled &= ~msgType;
    ALOGV("%s : mMsgEnabled = 0x%x", __func__, mMsgEnabled);
}

bool CameraHardwareSec::msgTypeEnabled(int32_t msgType)
{
    return (mMsgEnabled & msgType);
}

void CameraHardwareSec::setSkipFrame(int frame)
{
    Mutex::Autolock lock(mSkipFrameLock);
    if (frame < mSkipFrame)
        return;

    mSkipFrame = frame;
}

int CameraHardwareSec::previewThreadWrapper()
{
    ALOGI("%s: starting", __func__);
    while (1) {
        mPreviewLock.lock();
        while (!mPreviewRunning) {
            ALOGI("%s: calling mSecCamera->stopPreview() and waiting", __func__);
            mSecCamera->stopPreview();
            /* signal that we're stopping */
            mPreviewStoppedCondition.signal();
            mPreviewCondition.wait(mPreviewLock);
            ALOGI("%s: return from wait", __func__);
        }
        mPreviewLock.unlock();

        if (mExitPreviewThread) {
            ALOGI("%s: exiting", __func__);
            mSecCamera->stopPreview();
            return 0;
        }

        previewThread();
    }
}

int CameraHardwareSec::previewThread()
{
    int index;
    nsecs_t timestamp;
    SecBuffer previewAddr, recordAddr;
    static int numArray = 0;
    void *virAddr[3];
    camera_frame_metadata_t fdmeta;
    camera_face_t caface[5];

#ifdef BOARD_USE_V4L2_ION
    private_handle_t *hnd = NULL;
#else
    struct addrs *addrs;
#endif

    fdmeta.faces = caface;
    index = mSecCamera->getPreview(&fdmeta);

    mFaceData = &fdmeta;

    if (index < 0) {
        ALOGE("ERR(%s):Fail on SecCamera->getPreview()", __func__);
#ifdef BOARD_USE_V4L2_ION
        if (mSecCamera->getPreviewState()) {
            stopPreview();
            startPreview();
            mSecCamera->clearPreviewState();
        }
#endif
        return UNKNOWN_ERROR;
    }

#ifdef ZERO_SHUTTER_LAG
    if (mUseInternalISP && !mRecordHint) {
        mCapIndex = mSecCamera->getSnapshot();

        if (mCapIndex >= 0) {
            if (mSecCamera->setSnapshotFrame(mCapIndex) < 0) {
                ALOGE("%s: Fail qbuf, index(%d)", __func__, mCapIndex);
                return INVALID_OPERATION;
            }
        }
    }
#endif

    mSkipFrameLock.lock();
    if (mSkipFrame > 0) {
        mSkipFrame--;
        mSkipFrameLock.unlock();
        ALOGV("%s: index %d skipping frame", __func__, index);
        if (mSecCamera->setPreviewFrame(index) < 0) {
            ALOGE("%s: Could not qbuff[%d]!!", __func__, index);
            return UNKNOWN_ERROR;
        }
        return NO_ERROR;
    }
    mSkipFrameLock.unlock();

    timestamp = systemTime(SYSTEM_TIME_MONOTONIC);

    int width, height, frame_size, offset;

    mSecCamera->getPreviewSize(&width, &height, &frame_size);

    offset = frame_size * index;

    if (mPreviewWindow && mGrallocHal && mPreviewRunning) {
#ifdef BOARD_USE_V4L2_ION
        hnd = (private_handle_t*)*mBufferHandle[index];

        if (mPreviewHeap) {
            mPreviewHeap->release(mPreviewHeap);
            mPreviewHeap = 0;
        }

        mPreviewHeap = mGetMemoryCb(hnd->fd, frame_size, 1, 0);

        hnd = NULL;

        mGrallocHal->unlock(mGrallocHal, *mBufferHandle[index]);
        if (0 != mPreviewWindow->enqueue_buffer(mPreviewWindow, mBufferHandle[index])) {
            ALOGE("%s: Could not enqueue gralloc buffer[%d]!!", __func__, index);
            goto callbacks;
        } else {
            mBufferHandle[index] = NULL;
            mStride[index] = NULL;
        }

        numArray = index;
#endif

        if (0 != mPreviewWindow->dequeue_buffer(mPreviewWindow, &mBufferHandle[numArray], &mStride[numArray])) {
            ALOGE("%s: Could not dequeue gralloc buffer[%d]!!", __func__, numArray);
            goto callbacks;
        }

        if (!mGrallocHal->lock(mGrallocHal,
                               *mBufferHandle[numArray],
                               GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_YUV_ADDR,
                               0, 0, width, height, virAddr)) {
#ifdef BOARD_USE_V4L2
            mSecCamera->getPreviewAddr(index, &previewAddr);
            char *frame = (char *)previewAddr.virt.extP[0];
#else
            char *frame = ((char *)mPreviewHeap->data) + offset;
#endif

#ifdef BOARD_USE_V4L2_ION
            mSecCamera->setUserBufferAddr(virAddr, index, PREVIEW_MODE);
#else
            int total = frame_size + mFrameSizeDelta;
            int h = 0;
            char *src = frame;

            /* TODO : Need to fix size of planes for supported color fmt.
                      Currnetly we support only YV12(3 plane) and NV21(2 plane)*/
            // Y
            memcpy(virAddr[0],src, width * height);
            src += width * height;

            if (mPreviewFmtPlane == PREVIEW_FMT_2_PLANE) {
                memcpy(virAddr[1], src, width * height / 2);
            } else if (mPreviewFmtPlane == PREVIEW_FMT_3_PLANE) {
                // U
                memcpy(virAddr[1], src, width * height / 4);
                src += width * height / 4;

                // V
                memcpy(virAddr[2], src, width * height / 4);
            }

            mGrallocHal->unlock(mGrallocHal, **mBufferHandle);
#endif
        }
        else
            ALOGE("%s: could not obtain gralloc buffer", __func__);

        if (mSecCamera->setPreviewFrame(index) < 0) {
            ALOGE("%s: Fail qbuf, index(%d)", __func__, index);
            goto callbacks;
        }

        index = 0;
#ifndef BOARD_USE_V4L2_ION
        if (0 != mPreviewWindow->enqueue_buffer(mPreviewWindow, *mBufferHandle)) {
            ALOGE("Could not enqueue gralloc buffer!");
            goto callbacks;
        }
#endif
    }

callbacks:
    // Notify the client of a new frame.
    if (mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME && mPreviewRunning)
        mDataCb(CAMERA_MSG_PREVIEW_FRAME, mPreviewHeap, index, NULL, mCallbackCookie);

#ifdef USE_FACE_DETECTION
    if (mUseInternalISP && (mMsgEnabled & CAMERA_MSG_PREVIEW_METADATA) && mPreviewRunning)
        mDataCb(CAMERA_MSG_PREVIEW_METADATA, mFaceDataHeap, 0, mFaceData, mCallbackCookie);
#endif

    Mutex::Autolock lock(mRecordLock);
    if (mRecordRunning == true) {
        int recordingIndex = 0;

        index = mSecCamera->getRecordFrame();
        if (index < 0) {
            ALOGE("ERR(%s):Fail on SecCamera->getRecordFrame()", __func__);
            return UNKNOWN_ERROR;
        }

#ifdef VIDEO_SNAPSHOT
        if (mUseInternalISP && mRecordHint) {
            mCapIndex = mSecCamera->getSnapshot();

            if (mSecCamera->setSnapshotFrame(mCapIndex) < 0) {
                ALOGE("%s: Fail qbuf, index(%d)", __func__, mCapIndex);
                return INVALID_OPERATION;
            }
        }
#endif

#ifdef BOARD_USE_V4L2_ION
        numArray = index;
#else
        recordingIndex = index;
        mSecCamera->getRecordAddr(index, &recordAddr);

        ALOGV("record PhyY(0x%08x) phyC(0x%08x) ", recordAddr.phys.extP[0], recordAddr.phys.extP[1]);

        if (recordAddr.phys.extP[0] == 0xffffffff || recordAddr.phys.extP[1] == 0xffffffff) {
            ALOGE("ERR(%s):Fail on SecCamera getRectPhyAddr Y addr = %0x C addr = %0x", __func__,
                 recordAddr.phys.extP[0], recordAddr.phys.extP[1]);
            return UNKNOWN_ERROR;
        }

        addrs = (struct addrs *)(*mRecordHeap)->data;

        addrs[index].type   = kMetadataBufferTypeCameraSource;
        addrs[index].addr_y = recordAddr.phys.extP[0];
        addrs[index].addr_cbcr = recordAddr.phys.extP[1];
        addrs[index].buf_index = index;
#endif

        // Notify the client of a new frame.
        if (mMsgEnabled & CAMERA_MSG_VIDEO_FRAME)
            mDataCbTimestamp(timestamp, CAMERA_MSG_VIDEO_FRAME,
                             mRecordHeap[numArray], recordingIndex, mCallbackCookie);
        else
            mSecCamera->releaseRecordFrame(index);
    }

    return NO_ERROR;
}

status_t CameraHardwareSec::startPreview()
{
    int ret = 0;

    ALOGV("%s :", __func__);

    Mutex::Autolock lock(mStateLock);
    if (mCaptureInProgress) {
        ALOGE("%s : capture in progress, not allowed", __func__);
        return INVALID_OPERATION;
    }

    mPreviewLock.lock();
    if (mPreviewRunning) {
        // already running
        ALOGE("%s : preview thread already running", __func__);
        mPreviewLock.unlock();
        return INVALID_OPERATION;
    }

    mPreviewRunning = true;
    mPreviewStartDeferred = false;

    if (!mPreviewWindow &&
            !(mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) &&
            !(mMsgEnabled & CAMERA_MSG_VIDEO_FRAME)) {
        ALOGI("%s : deferring", __func__);
        mPreviewStartDeferred = true;
        mPreviewLock.unlock();
        return NO_ERROR;
    }

    ret = startPreviewInternal();
    if (ret == OK)
        mPreviewCondition.signal();

    mPreviewLock.unlock();
    return ret;
}

status_t CameraHardwareSec::startPreviewInternal()
{
    ALOGV("%s", __func__);
    int width, height, frame_size;

    mSecCamera->getPreviewSize(&width, &height, &frame_size);
    ALOGD("mPreviewHeap(fd(%d), size(%d), width(%d), height(%d))",
         mSecCamera->getCameraFd(SecCamera::PREVIEW), frame_size + mFrameSizeDelta, width, height);

#ifdef BOARD_USE_V4L2_ION
#ifdef ZERO_SHUTTER_LAG
/*TODO*/
    int mPostViewWidth, mPostViewHeight, mPostViewSize;
    mSecCamera->getPostViewConfig(&mPostViewWidth, &mPostViewHeight, &mPostViewSize);
    for(int i = 0; i < CAP_BUFFERS; i++) {
        mPostviewHeap[i] = new MemoryHeapBaseIon(mPostViewSize);
        mSecCamera->setUserBufferAddr(mPostviewHeap[i]->base(), i, CAPTURE_MODE);
    }
#endif
    void *vaddr[3];

    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (mBufferHandle[i] == NULL) {
            if (0 != mPreviewWindow->dequeue_buffer(mPreviewWindow, &mBufferHandle[i], &mStride[i])) {
                ALOGE("%s: Could not dequeue gralloc buffer[%d]!!", __func__, i);
                return INVALID_OPERATION;
            }
        }
        if (mGrallocHal->lock(mGrallocHal,
                          *mBufferHandle[i],
                          GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_YUV_ADDR,
                          0, 0, width, height, vaddr)) {
            ALOGE("ERR(%s): Could not get virtual address!!, index = %d", __func__, i);
            return UNKNOWN_ERROR;
        }
        mSecCamera->setUserBufferAddr(vaddr, i, PREVIEW_MODE);
    }
#endif

    int ret  = mSecCamera->startPreview();
    ALOGV("%s : mSecCamera->startPreview() returned %d", __func__, ret);

    if (ret < 0) {
        ALOGE("ERR(%s):Fail on mSecCamera->startPreview()", __func__);
        return UNKNOWN_ERROR;
    }

    setSkipFrame(INITIAL_SKIP_FRAME);

    if (mPreviewHeap) {
        mPreviewHeap->release(mPreviewHeap);
        mPreviewHeap = 0;
    }

    for(int i=0; i<BUFFER_COUNT_FOR_ARRAY; i++){
        if (mRecordHeap[i] != NULL) {
            mRecordHeap[i]->release(mRecordHeap[i]);
            mRecordHeap[i] = 0;
        }
    }

#ifndef BOARD_USE_V4L2
    mPreviewHeap = mGetMemoryCb((int)mSecCamera->getCameraFd(SecCamera::PREVIEW),
                                frame_size + mFrameSizeDelta,
                                MAX_BUFFERS,
                                0); // no cookie
#endif

    mFaceDataHeap = mGetMemoryCb(-1, 1, 1, 0);

    mSecCamera->getPostViewConfig(&mPostViewWidth, &mPostViewHeight, &mPostViewSize);
    ALOGV("CameraHardwareSec: mPostViewWidth = %d mPostViewHeight = %d mPostViewSize = %d",
         mPostViewWidth,mPostViewHeight,mPostViewSize);

    return NO_ERROR;
}

void CameraHardwareSec::stopPreviewInternal()
{
    ALOGV("%s :", __func__);

    /* request that the preview thread stop. */
    if (mPreviewRunning) {
        mPreviewRunning = false;
        if (!mPreviewStartDeferred) {
            mPreviewCondition.signal();
            /* wait until preview thread is stopped */
            mPreviewStoppedCondition.wait(mPreviewLock);

#ifdef BOARD_USE_V4L2_ION
            for (int i = 0; i < MAX_BUFFERS; i++) {
                if (mBufferHandle[i] != NULL) {
                    if (0 != mPreviewWindow->cancel_buffer(mPreviewWindow, mBufferHandle[i])) {
                        ALOGE("%s: Fail to cancel buffer[%d]", __func__, i);
                    } else {
                        mBufferHandle[i] = NULL;
                        mStride[i] = NULL;
                    }
                }
            }
#endif
        }
        else
            ALOGV("%s : preview running but deferred, doing nothing", __func__);
    } else
        ALOGI("%s : preview not running, doing nothing", __func__);
}

void CameraHardwareSec::stopPreview()
{
    ALOGV("%s :", __func__);

    /* request that the preview thread stop. */
    mPreviewLock.lock();
    stopPreviewInternal();
    mPreviewLock.unlock();
}

bool CameraHardwareSec::previewEnabled()
{
    Mutex::Autolock lock(mPreviewLock);
    ALOGV("%s : %d", __func__, mPreviewRunning);
    return mPreviewRunning;
}

status_t CameraHardwareSec::startRecording()
{
    ALOGV("%s :", __func__);

    Mutex::Autolock lock(mRecordLock);

    for(int i = 0; i<BUFFER_COUNT_FOR_ARRAY; i++){
        if (mRecordHeap[i] != NULL) {
            mRecordHeap[i]->release(mRecordHeap[i]);
            mRecordHeap[i] = 0;
        }

#ifdef BOARD_USE_V4L2_ION
        int width, height;

        mSecCamera->getRecordingSize(&width, &height);

        mRecordHeap[i] = mGetMemoryCb(-1, (ALIGN((ALIGN(width, 16) * ALIGN(height, 16)), 2048) + ALIGN((ALIGN(width, 16) * ALIGN(height >> 1, 8)), 2048)), 1, NULL);

        mSecCamera->setUserBufferAddr((void *)(mRecordHeap[i]->data), i, RECORD_MODE);
#else
        mRecordHeap[i] = mGetMemoryCb(-1, sizeof(struct addrs), MAX_BUFFERS, NULL);
#endif
        if (!mRecordHeap[i]) {
            ALOGE("ERR(%s): Record heap[%d] creation fail", __func__, i);
            return UNKNOWN_ERROR;
        }
    }

    ALOGV("mRecordHeaps alloc done");

    if (mRecordRunning == false) {
        if (mSecCamera->startRecord(mRecordHint) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->startRecord()", __func__);
            return UNKNOWN_ERROR;
        }
        mRecordRunning = true;
    }
    return NO_ERROR;
}

void CameraHardwareSec::stopRecording()
{
    ALOGV("%s :", __func__);

    Mutex::Autolock lock(mRecordLock);

    if (mRecordRunning == true) {
        if (mSecCamera->stopRecord() < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->stopRecord()", __func__);
            return;
        }
        mRecordRunning = false;
    }
}

bool CameraHardwareSec::recordingEnabled()
{
    ALOGV("%s :", __func__);
    ALOGV("%s : %d", __func__, mPreviewRunning);

    return mRecordRunning;
}

void CameraHardwareSec::releaseRecordingFrame(const void *opaque)
{
#ifdef BOARD_USE_V4L2_ION
    int i;
    for (i = 0; i < MAX_BUFFERS; i++)
        if ((char *)mRecordHeap[i]->data == (char *)opaque)
            break;

    mSecCamera->releaseRecordFrame(i);
#else
    struct addrs *addrs = (struct addrs *)opaque;
    mSecCamera->releaseRecordFrame(addrs->buf_index);
#endif
}

int CameraHardwareSec::autoFocusThread()
{
    int count =0;
    int af_status =0 ;

    ALOGV("%s : starting", __func__);

    /* block until we're told to start.  we don't want to use
     * a restartable thread and requestExitAndWait() in cancelAutoFocus()
     * because it would cause deadlock between our callbacks and the
     * caller of cancelAutoFocus() which both want to grab the same lock
     * in CameraServices layer.
     */
    mFocusLock.lock();
    /* check early exit request */
    if (mExitAutoFocusThread) {
        mFocusLock.unlock();
        ALOGV("%s : exiting on request0", __func__);
        return NO_ERROR;
    }
    mFocusCondition.wait(mFocusLock);
    /* check early exit request */
    if (mExitAutoFocusThread) {
        mFocusLock.unlock();
        ALOGV("%s : exiting on request1", __func__);
        return NO_ERROR;
    }
    mFocusLock.unlock();

    /* TODO : Currently only possible auto focus at BACK caemra
              We need to modify to check that sensor can support auto focus */
    if (mCameraID == SecCamera::CAMERA_ID_BACK) {
        ALOGV("%s : calling setAutoFocus", __func__);
        if (mTouched == 0) {
            if (mSecCamera->setAutofocus() < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setAutofocus()", __func__);
                return UNKNOWN_ERROR;
            }
        } else {
            if (mSecCamera->setTouchAF() < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setAutofocus()", __func__);
                return UNKNOWN_ERROR;
            }
        }
    }

    /* TODO */
    /* This is temperary implementation.
       When camera support AF blocking mode, this code will be removed
       Continous AutoFocus is not need to success */
    const char *focusModeStr = mParameters.get(CameraParameters::KEY_FOCUS_MODE);
    int isContinousAF = !strncmp(focusModeStr, CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO, 7);
    if (mUseInternalISP && !isContinousAF) {
        int i, err = -1;
        for (i = 0; i < 400; i++) {
           usleep(10000);

           af_status = mSecCamera->getAutoFocusResult();

           if ((af_status & 0x2)) {
               err = 0;
               break;
           }
        }
    } else {
        af_status = mSecCamera->getAutoFocusResult();
    }

    if (af_status == 0x01) {
        ALOGV("%s : AF Cancelled !!", __func__);
        if (mMsgEnabled & CAMERA_MSG_FOCUS)
            mNotifyCb(CAMERA_MSG_FOCUS, true, 0, mCallbackCookie);
    } else if (af_status == 0x02) {
        ALOGV("%s : AF Success !!", __func__);
        if (mMsgEnabled & CAMERA_MSG_FOCUS) {
            /* CAMERA_MSG_FOCUS only takes a bool.  true for
             * finished and false for failure.  cancel is still
             * considered a true result.
             */
            mNotifyCb(CAMERA_MSG_FOCUS, true, 0, mCallbackCookie);
        }
    } else {
        ALOGV("%s : AF Fail !!", __func__);
        ALOGV("%s : mMsgEnabled = 0x%x", __func__, mMsgEnabled);
        if (mMsgEnabled & CAMERA_MSG_FOCUS)
            mNotifyCb(CAMERA_MSG_FOCUS, false, 0, mCallbackCookie);
    }

    ALOGV("%s : exiting with no error", __func__);
    return NO_ERROR;
}

status_t CameraHardwareSec::autoFocus()
{
    ALOGV("%s :", __func__);
    /* signal autoFocusThread to run once */
    mFocusCondition.signal();
    return NO_ERROR;
}

status_t CameraHardwareSec::cancelAutoFocus()
{
    ALOGV("%s :", __func__);

    if (mSecCamera->cancelAutofocus() < 0) {
        ALOGE("ERR(%s):Fail on mSecCamera->cancelAutofocus()", __func__);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

int CameraHardwareSec::save_jpeg( unsigned char *real_jpeg, int jpeg_size)
{
    FILE *yuv_fp = NULL;
    char filename[100], *buffer = NULL;

    /* file create/open, note to "wb" */
    yuv_fp = fopen("/data/camera_dump.jpeg", "wb");
    if (yuv_fp == NULL) {
        ALOGE("Save jpeg file open error");
        return -1;
    }

    ALOGV("[BestIQ]  real_jpeg size ========>  %d", jpeg_size);
    buffer = (char *) malloc(jpeg_size);
    if (buffer == NULL) {
        ALOGE("Save YUV] buffer alloc failed");
        if (yuv_fp)
            fclose(yuv_fp);

        return -1;
    }

    memcpy(buffer, real_jpeg, jpeg_size);

    fflush(stdout);

    fwrite(buffer, 1, jpeg_size, yuv_fp);

    fflush(yuv_fp);

    if (yuv_fp)
            fclose(yuv_fp);
    if (buffer)
            free(buffer);

    return 0;
}

void CameraHardwareSec::save_postview(const char *fname, uint8_t *buf, uint32_t size)
{
    int nw;
    int cnt = 0;
    uint32_t written = 0;

    ALOGD("opening file [%s]", fname);
    int fd = open(fname, O_RDWR | O_CREAT);
    if (fd < 0) {
        ALOGE("failed to create file [%s]: %s", fname, strerror(errno));
        return;
    }

    ALOGD("writing %d bytes to file [%s]", size, fname);
    while (written < size) {
        nw = ::write(fd, buf + written, size - written);
        if (nw < 0) {
            ALOGE("failed to write to file %d [%s]: %s",written,fname, strerror(errno));
            break;
        }
        written += nw;
        cnt++;
    }
    ALOGD("done writing %d bytes to file [%s] in %d passes",size, fname, cnt);
    ::close(fd);
}

bool CameraHardwareSec::scaleDownYuv422(char *srcBuf, uint32_t srcWidth, uint32_t srcHeight,
                                        char *dstBuf, uint32_t dstWidth, uint32_t dstHeight)
{
    int32_t step_x, step_y;
    int32_t iXsrc, iXdst;
    int32_t x, y, src_y_start_pos, dst_pos, src_pos;

    if (dstWidth % 2 != 0 || dstHeight % 2 != 0) {
        ALOGE("scale_down_yuv422: invalid width, height for scaling");
        return false;
    }

    step_x = srcWidth / dstWidth;
    step_y = srcHeight / dstHeight;

    dst_pos = 0;
    for (uint32_t y = 0; y < dstHeight; y++) {
        src_y_start_pos = (y * step_y * (srcWidth * 2));

        for (uint32_t x = 0; x < dstWidth; x += 2) {
            src_pos = src_y_start_pos + (x * (step_x * 2));

            dstBuf[dst_pos++] = srcBuf[src_pos    ];
            dstBuf[dst_pos++] = srcBuf[src_pos + 1];
            dstBuf[dst_pos++] = srcBuf[src_pos + 2];
            dstBuf[dst_pos++] = srcBuf[src_pos + 3];
        }
    }

    return true;
}

bool CameraHardwareSec::YUY2toNV21(void *srcBuf, void *dstBuf, uint32_t srcWidth, uint32_t srcHeight)
{
    int32_t        x, y, src_y_start_pos, dst_cbcr_pos, dst_pos, src_pos;
    unsigned char *srcBufPointer = (unsigned char *)srcBuf;
    unsigned char *dstBufPointer = (unsigned char *)dstBuf;

    dst_pos = 0;
    dst_cbcr_pos = srcWidth*srcHeight;
    for (uint32_t y = 0; y < srcHeight; y++) {
        src_y_start_pos = (y * (srcWidth * 2));

        for (uint32_t x = 0; x < (srcWidth * 2); x += 2) {
            src_pos = src_y_start_pos + x;

            dstBufPointer[dst_pos++] = srcBufPointer[src_pos];
        }
    }
    for (uint32_t y = 0; y < srcHeight; y += 2) {
        src_y_start_pos = (y * (srcWidth * 2));

        for (uint32_t x = 0; x < (srcWidth * 2); x += 4) {
            src_pos = src_y_start_pos + x;

            dstBufPointer[dst_cbcr_pos++] = srcBufPointer[src_pos + 3];
            dstBufPointer[dst_cbcr_pos++] = srcBufPointer[src_pos + 1];
        }
    }

    return true;
}

int CameraHardwareSec::pictureThread()
{
    ALOGV("%s :", __func__);

    int jpeg_size = 0;
    int ret = NO_ERROR;
    unsigned char *jpeg_data = NULL;
    int postview_offset = 0;
    unsigned char *postview_data = NULL;

    unsigned char *addr = NULL;
    int mPostViewWidth, mPostViewHeight, mPostViewSize;
    int mThumbWidth, mThumbHeight, mThumbSize;
    int cap_width, cap_height, cap_frame_size;

    int JpegImageSize = 0;

    mSecCamera->getPostViewConfig(&mPostViewWidth, &mPostViewHeight, &mPostViewSize);
    mSecCamera->getThumbnailConfig(&mThumbWidth, &mThumbHeight, &mThumbSize);
    int postviewHeapSize = mPostViewSize;
    if (!mRecordRunning)
        mSecCamera->getSnapshotSize(&cap_width, &cap_height, &cap_frame_size);
    else
        mSecCamera->getVideosnapshotSize(&cap_width, &cap_height, &cap_frame_size);
    int mJpegHeapSize;
    if (!mUseInternalISP)
        mJpegHeapSize = cap_frame_size * SecCamera::getJpegRatio();
    else
        mJpegHeapSize = cap_frame_size;

    ALOGV("[5B] mPostViewWidth = %d mPostViewHeight = %d\n",mPostViewWidth,mPostViewHeight);

    camera_memory_t *JpegHeap = mGetMemoryCb(-1, mJpegHeapSize, 1, 0);
#ifdef BOARD_USE_V4L2_ION
#ifdef ZERO_SHUTTER_LAG
    mThumbnailHeap = new MemoryHeapBaseIon(mThumbSize);
#else
    mPostviewHeap[mCapIndex] = new MemoryHeapBaseIon(mPostViewSize);
    mThumbnailHeap = new MemoryHeapBaseIon(mThumbSize);
#endif
#else
    mThumbnailHeap = new MemoryHeapBase(mThumbSize);
#endif

    if (mMsgEnabled & CAMERA_MSG_RAW_IMAGE) {
        int picture_size, picture_width, picture_height;
        mSecCamera->getSnapshotSize(&picture_width, &picture_height, &picture_size);
        int picture_format = mSecCamera->getSnapshotPixelFormat();

        unsigned int thumb_addr, phyAddr;

        // Modified the shutter sound timing for Jpeg capture
        if (!mUseInternalISP) {
            mSecCamera->setSnapshotCmd();

            if (mMsgEnabled & CAMERA_MSG_SHUTTER)
                mNotifyCb(CAMERA_MSG_SHUTTER, 0, 0, mCallbackCookie);

            jpeg_data = mSecCamera->getJpeg(&JpegImageSize, &mThumbSize, &thumb_addr, &phyAddr);
            if (jpeg_data == NULL) {
                ALOGE("ERR(%s):Fail on SecCamera->getJpeg()", __func__);
                ret = UNKNOWN_ERROR;
            }

            memcpy((unsigned char *)mThumbnailHeap->base(), (unsigned char *)thumb_addr, mThumbSize);
            memcpy(JpegHeap->data, jpeg_data, JpegImageSize);
        } else {
            if (mMsgEnabled & CAMERA_MSG_SHUTTER)
                mNotifyCb(CAMERA_MSG_SHUTTER, 0, 0, mCallbackCookie);

#ifdef ZERO_SHUTTER_LAG
            mSecCamera->getCaptureAddr(mCapIndex, &mCapBuffer);

            if (mCapBuffer.virt.extP[0] == NULL) {
                ALOGE("ERR(%s):Fail on SecCamera getCaptureAddr = %0x ",
                     __func__, mCapBuffer.virt.extP[0]);
                return UNKNOWN_ERROR;
            }

            scaleDownYuv422((char *)mCapBuffer.virt.extP[0], cap_width, cap_height,
                            (char *)mThumbnailHeap->base(), mThumbWidth, mThumbHeight);
#else
#ifdef BOARD_USE_V4L2_ION
            mCapBuffer.virt.extP[0] = (char *)mPostviewHeap[mCapIndex]->base();
#endif
#endif

            if (mSecCamera->getSnapshotAndJpeg(&mCapBuffer, mCapIndex,
                    (unsigned char*)JpegHeap->data, &JpegImageSize) < 0) {
                mStateLock.lock();
                mCaptureInProgress = false;
                mStateLock.unlock();
                JpegHeap->release(JpegHeap);
                return UNKNOWN_ERROR;
            }
            ALOGI("snapshotandjpeg done");

#ifdef ZERO_SHUTTER_LAG
            if (!mRecordRunning)
                stopPreview();
            memset(&mCapBuffer, 0, sizeof(struct SecBuffer));
#else
            scaleDownYuv422((char *)mCapBuffer.virt.extP[0], cap_width, cap_height,
                            (char *)mThumbnailHeap->base(), mThumbWidth, mThumbHeight);
#endif
        }
    }

#ifndef BOARD_USE_V4L2_ION
    int rawHeapSize = cap_frame_size;
    ALOGV("mRawHeap : MemoryHeapBase(previewHeapSize(%d))", rawHeapSize);
#ifdef BOARD_USE_V4L2_ION
    mRawHeap = mGetMemoryCb(mPostviewHeap[mCapIndex]->getHeapID(), rawHeapSize, 1, 0);
#else
    mRawHeap = mGetMemoryCb((int)mSecCamera->getCameraFd(SecCamera::PICTURE), rawHeapSize, 1, 0);
#endif
    if (!mRawHeap)
        ALOGE("ERR(%s): Raw heap creation fail", __func__);

    if (mMsgEnabled & CAMERA_MSG_RAW_IMAGE)
        mDataCb(CAMERA_MSG_RAW_IMAGE, mRawHeap, 0, NULL, mCallbackCookie);
#endif
    mStateLock.lock();
    mCaptureInProgress = false;
    mStateLock.unlock();

    if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE) {
        camera_memory_t *ExifHeap =
            mGetMemoryCb(-1, EXIF_FILE_SIZE + mThumbSize, 1, 0);

        int JpegExifSize = mSecCamera->getExif((unsigned char *)ExifHeap->data,
                                           (unsigned char *)mThumbnailHeap->base(),
                                            mThumbSize);
        ALOGV("JpegExifSize=%d", JpegExifSize);

        if (JpegExifSize < 0) {
            ret = UNKNOWN_ERROR;
            goto out;
        }

        int mJpegHeapSize_out = JpegImageSize + JpegExifSize;
        camera_memory_t *JpegHeap_out = mGetMemoryCb(-1, mJpegHeapSize_out, 1, 0);

        unsigned char *ExifStart = (unsigned char *)JpegHeap_out->data + 2;
        unsigned char *ImageStart = ExifStart + JpegExifSize;

        memcpy(JpegHeap_out->data, JpegHeap->data, 2);
        memcpy(ExifStart, ExifHeap->data, JpegExifSize);
        memcpy(ImageStart, JpegHeap->data + 2, JpegImageSize - 2);

        mDataCb(CAMERA_MSG_COMPRESSED_IMAGE, JpegHeap_out, 0, NULL, mCallbackCookie);

        if (ExifHeap) {
            ExifHeap->release(ExifHeap);
            ExifHeap = 0;
        }

        if (JpegHeap_out) {
            JpegHeap_out->release(JpegHeap_out);
            JpegHeap_out = 0;
        }
    }

    ALOGV("%s : pictureThread end", __func__);

out:
    if (JpegHeap) {
        JpegHeap->release(JpegHeap);
        JpegHeap = 0;
    }

    if (mRawHeap) {
        mRawHeap->release(mRawHeap);
        mRawHeap = 0;
    }

    if (!mUseInternalISP && !mRecordRunning)
        mSecCamera->endSnapshot();

    return ret;
}

status_t CameraHardwareSec::takePicture()
{
    ALOGV("%s :", __func__);

#ifdef ZERO_SHUTTER_LAG
    if (!mUseInternalISP) {
        stopPreview();
    }
#else
    stopPreview();
#endif

    Mutex::Autolock lock(mStateLock);
    if (mCaptureInProgress) {
        ALOGE("%s : capture already in progress", __func__);
        return INVALID_OPERATION;
    }

    if (mPictureThread->run("CameraPictureThread", PRIORITY_DEFAULT) != NO_ERROR) {
        ALOGE("%s : couldn't run picture thread", __func__);
        return INVALID_OPERATION;
    }
    mCaptureInProgress = true;

    return NO_ERROR;
}

status_t CameraHardwareSec::cancelPicture()
{
    ALOGV("%s", __func__);

    if (mPictureThread.get()) {
        ALOGV("%s: waiting for picture thread to exit", __func__);
        mPictureThread->requestExitAndWait();
        ALOGV("%s: picture thread has exited", __func__);
    }

    return NO_ERROR;
}

bool CameraHardwareSec::CheckVideoStartMarker(unsigned char *pBuf)
{
    if (!pBuf) {
        ALOGE("CheckVideoStartMarker() => pBuf is NULL");
        return false;
    }

    if (HIBYTE(VIDEO_COMMENT_MARKER_H) == * pBuf      && LOBYTE(VIDEO_COMMENT_MARKER_H) == *(pBuf + 1) &&
        HIBYTE(VIDEO_COMMENT_MARKER_L) == *(pBuf + 2) && LOBYTE(VIDEO_COMMENT_MARKER_L) == *(pBuf + 3))
        return true;

    return false;
}

bool CameraHardwareSec::CheckEOIMarker(unsigned char *pBuf)
{
    if (!pBuf) {
        ALOGE("CheckEOIMarker() => pBuf is NULL");
        return false;
    }

    // EOI marker [FF D9]
    if (HIBYTE(JPEG_EOI_MARKER) == *pBuf && LOBYTE(JPEG_EOI_MARKER) == *(pBuf + 1))
        return true;

    return false;
}

bool CameraHardwareSec::FindEOIMarkerInJPEG(unsigned char *pBuf, int dwBufSize, int *pnJPEGsize)
{
    if (NULL == pBuf || 0 >= dwBufSize) {
        ALOGE("FindEOIMarkerInJPEG() => There is no contents.");
        return false;
    }

    unsigned char *pBufEnd = pBuf + dwBufSize;

    while (pBuf < pBufEnd) {
        if (CheckEOIMarker(pBuf++))
            return true;

        (*pnJPEGsize)++;
    }

    return false;
}

bool CameraHardwareSec::SplitFrame(unsigned char *pFrame, int dwSize,
                    int dwJPEGLineLength, int dwVideoLineLength, int dwVideoHeight,
                    void *pJPEG, int *pdwJPEGSize,
                    void *pVideo, int *pdwVideoSize)
{
    ALOGV("===========SplitFrame Start==============");

    if (NULL == pFrame || 0 >= dwSize) {
        ALOGE("There is no contents (pFrame=%p, dwSize=%d", pFrame, dwSize);
        return false;
    }

    if (0 == dwJPEGLineLength || 0 == dwVideoLineLength) {
        ALOGE("There in no input information for decoding interleaved jpeg");
        return false;
    }

    unsigned char *pSrc = pFrame;
    unsigned char *pSrcEnd = pFrame + dwSize;

    unsigned char *pJ = (unsigned char *)pJPEG;
    int dwJSize = 0;
    unsigned char *pV = (unsigned char *)pVideo;
    int dwVSize = 0;

    bool bRet = false;
    bool isFinishJpeg = false;

    while (pSrc < pSrcEnd) {
        // Check video start marker
        if (CheckVideoStartMarker(pSrc)) {
            int copyLength;

            if (pSrc + dwVideoLineLength <= pSrcEnd)
                copyLength = dwVideoLineLength;
            else
                copyLength = pSrcEnd - pSrc - VIDEO_COMMENT_MARKER_LENGTH;

            // Copy video data
            if (pV) {
                memcpy(pV, pSrc + VIDEO_COMMENT_MARKER_LENGTH, copyLength);
                pV += copyLength;
                dwVSize += copyLength;
            }

            pSrc += copyLength + VIDEO_COMMENT_MARKER_LENGTH;
        } else {
            // Copy pure JPEG data
            int size = 0;
            int dwCopyBufLen = dwJPEGLineLength <= pSrcEnd-pSrc ? dwJPEGLineLength : pSrcEnd - pSrc;

            if (FindEOIMarkerInJPEG((unsigned char *)pSrc, dwCopyBufLen, &size)) {
                isFinishJpeg = true;
                size += 2;  // to count EOF marker size
            } else {
                if ((dwCopyBufLen == 1) && (pJPEG < pJ)) {
                    unsigned char checkBuf[2] = { *(pJ - 1), *pSrc };

                    if (CheckEOIMarker(checkBuf))
                        isFinishJpeg = true;
                }
                size = dwCopyBufLen;
            }

            memcpy(pJ, pSrc, size);

            dwJSize += size;

            pJ += dwCopyBufLen;
            pSrc += dwCopyBufLen;
        }
        if (isFinishJpeg)
            break;
    }

    if (isFinishJpeg) {
        bRet = true;
        if (pdwJPEGSize)
            *pdwJPEGSize = dwJSize;
        if (pdwVideoSize)
            *pdwVideoSize = dwVSize;
    } else {
        ALOGE("DecodeInterleaveJPEG_WithOutDT() => Can not find EOI");
        bRet = false;
        if (pdwJPEGSize)
            *pdwJPEGSize = 0;
        if (pdwVideoSize)
            *pdwVideoSize = 0;
    }
    ALOGV("===========SplitFrame end==============");

    return bRet;
}

int CameraHardwareSec::decodeInterleaveData(unsigned char *pInterleaveData,
                                                 int interleaveDataSize,
                                                 int yuvWidth,
                                                 int yuvHeight,
                                                 int *pJpegSize,
                                                 void *pJpegData,
                                                 void *pYuvData)
{
    if (pInterleaveData == NULL)
        return false;

    bool ret = true;
    unsigned int *interleave_ptr = (unsigned int *)pInterleaveData;
    unsigned char *jpeg_ptr = (unsigned char *)pJpegData;
    unsigned char *yuv_ptr = (unsigned char *)pYuvData;
    unsigned char *p;
    int jpeg_size = 0;
    int yuv_size = 0;

    int i = 0;

    ALOGV("decodeInterleaveData Start~~~");
    while (i < interleaveDataSize) {
        if ((*interleave_ptr == 0xFFFFFFFF) || (*interleave_ptr == 0x02FFFFFF) ||
                (*interleave_ptr == 0xFF02FFFF)) {
            // Padding Data
            interleave_ptr++;
            i += 4;
        } else if ((*interleave_ptr & 0xFFFF) == 0x05FF) {
            // Start-code of YUV Data
            p = (unsigned char *)interleave_ptr;
            p += 2;
            i += 2;

            // Extract YUV Data
            if (pYuvData != NULL) {
                memcpy(yuv_ptr, p, yuvWidth * 2);
                yuv_ptr += yuvWidth * 2;
                yuv_size += yuvWidth * 2;
            }
            p += yuvWidth * 2;
            i += yuvWidth * 2;

            // Check End-code of YUV Data
            if ((*p == 0xFF) && (*(p + 1) == 0x06)) {
                interleave_ptr = (unsigned int *)(p + 2);
                i += 2;
            } else {
                ret = false;
                break;
            }
        } else {
            // Extract JPEG Data
            if (pJpegData != NULL) {
                memcpy(jpeg_ptr, interleave_ptr, 4);
                jpeg_ptr += 4;
                jpeg_size += 4;
            }
            interleave_ptr++;
            i += 4;
        }
    }
    if (ret) {
        if (pJpegData != NULL) {
            // Remove Padding after EOI
            for (i = 0; i < 3; i++) {
                if (*(--jpeg_ptr) != 0xFF) {
                    break;
                }
                jpeg_size--;
            }
            *pJpegSize = jpeg_size;

        }
        // Check YUV Data Size
        if (pYuvData != NULL) {
            if (yuv_size != (yuvWidth * yuvHeight * 2)) {
                ret = false;
            }
        }
    }
    ALOGV("decodeInterleaveData End~~~");
    return ret;
}

status_t CameraHardwareSec::dump(int fd) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    const Vector<String16> args;

    if (mSecCamera != 0) {
        mSecCamera->dump(fd);
        mParameters.dump(fd, args);
        mInternalParameters.dump(fd, args);
        snprintf(buffer, 255, " preview running(%s)\n", mPreviewRunning?"true": "false");
        result.append(buffer);
    } else
        result.append("No camera client yet.\n");
    write(fd, result.string(), result.size());
    return NO_ERROR;
}

bool CameraHardwareSec::isSupportedPreviewSize(const int width,
                                               const int height) const
{
    unsigned int i;

    for (i = 0; i < mSupportedPreviewSizes.size(); i++) {
        if (mSupportedPreviewSizes[i].width == width &&
                mSupportedPreviewSizes[i].height == height)
            return true;
    }

    return false;
}

bool CameraHardwareSec::getVideosnapshotSize(int *width, int *height)
{
    unsigned int i;
    Vector<Size> pictureSizes, videoSizes;
    int ratio = FRM_RATIO(*width, *height);

    mParameters.getSupportedPictureSizes(pictureSizes);
    mParameters.getSupportedVideoSizes(videoSizes);

    for (i = 0; i < pictureSizes.size(); i++) {
        if (FRM_RATIO(pictureSizes[i].width, pictureSizes[i].height) == ratio) {
            if (mRecordHint) {
                if (pictureSizes[i].width <= videoSizes[0].width) {
                    *width = pictureSizes[i].width;
                    *height = pictureSizes[i].height;
                    ALOGV("%s(width(%d), height(%d))", __func__, *width, *height);
                    return true;
                }
            } else {
                *width = pictureSizes[i].width;
                *height = pictureSizes[i].height;
                ALOGV("%s(width(%d), height(%d))", __func__, *width, *height);
                return true;
            }
        }
    }

    return false;
}

status_t CameraHardwareSec::setParameters(const CameraParameters& params)
{
    ALOGV("%s :", __func__);

    status_t ret = NO_ERROR;

    const char *new_record_hint_str = params.get(CameraParameters::KEY_RECORDING_HINT);
    const char *curr_record_hint_str = mParameters.get(CameraParameters::KEY_RECORDING_HINT);
    ALOGV("new_record_hint_str: %s", new_record_hint_str);

    if (new_record_hint_str) {
        if (strncmp(new_record_hint_str, curr_record_hint_str, 5)) {
            mRecordHint = !strncmp(new_record_hint_str, "true", 4);
            if (mSecCamera->setMode(mRecordHint) < 0) {
                ALOGE("ERR(%s):fail on mSecCamera->setMode(%d)", __func__, mRecordHint);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.set(CameraParameters::KEY_RECORDING_HINT, new_record_hint_str);
            }

            if (mUseInternalISP) {
                if (mSecCamera->initSetParams() < 0) {
                    ALOGE("ERR(%s):fail on mSecCamera->initSetParams()", __func__);
                    ret = UNKNOWN_ERROR;
                }
            }
        }
    }

    /* if someone calls us while picture thread is running, it could screw
     * up the sensor quite a bit so return error.  we can't wait because
     * that would cause deadlock with the callbacks
     */
    mStateLock.lock();
    if (mCaptureInProgress) {
        mStateLock.unlock();
        ALOGE("%s : capture in progress, not allowed", __func__);
        return UNKNOWN_ERROR;
    }
    mStateLock.unlock();

    // preview size
    int new_preview_width  = 0;
    int new_preview_height = 0;
    int new_preview_format = 0;

    params.getPreviewSize(&new_preview_width, &new_preview_height);

    if (mUseInternalISP) {
        int videosnapshot_width = new_preview_width;
        int videosnapshot_height = new_preview_height;

        if (!getVideosnapshotSize(&videosnapshot_width, &videosnapshot_height)) {
            ALOGE("ERR(%s):fail on getVideosnapshotSize(width(%d), height(%d))",
                    __func__, videosnapshot_width, videosnapshot_height);
            ret = UNKNOWN_ERROR;
        }

        if (mSecCamera->setVideosnapshotSize(videosnapshot_width, videosnapshot_height) < 0) {
            ALOGE("ERR(%s):fail on mSecCamera->setVideosnapshotSize(width(%d), height(%d))",
                    __func__, videosnapshot_width, videosnapshot_height);
            ret = UNKNOWN_ERROR;
        }
    }

    const char *new_str_preview_format = params.getPreviewFormat();
    ALOGV("%s : new_preview_width x new_preview_height = %dx%d, format = %s",
         __func__, new_preview_width, new_preview_height, new_str_preview_format);

    if (0 < new_preview_width && 0 < new_preview_height &&
            new_str_preview_format != NULL &&
            isSupportedPreviewSize(new_preview_width, new_preview_height)) {

        mFrameSizeDelta = 16;
        if (!strcmp(new_str_preview_format,
                    CameraParameters::PIXEL_FORMAT_RGB565)) {
            new_preview_format = V4L2_PIX_FMT_RGB565;
            mFrameSizeDelta = 0;
        }
        else if (!strcmp(new_str_preview_format,
                         CameraParameters::PIXEL_FORMAT_RGBA8888)) {
            new_preview_format = V4L2_PIX_FMT_RGB32;
            mFrameSizeDelta = 0;
        }
        else if (!strcmp(new_str_preview_format,
                         CameraParameters::PIXEL_FORMAT_YUV420SP)) {
            new_preview_format = V4L2_PIX_FMT_NV21;
            mPreviewFmtPlane = PREVIEW_FMT_2_PLANE;
        }
        else if (!strcmp(new_str_preview_format,
                         CameraParameters::PIXEL_FORMAT_YUV420P)) {
#ifdef BOARD_USE_V4L2_ION
            new_preview_format = V4L2_PIX_FMT_YVU420M;
#else
            new_preview_format = V4L2_PIX_FMT_YVU420;
#endif
            mPreviewFmtPlane = PREVIEW_FMT_3_PLANE;
        }
        else if (!strcmp(new_str_preview_format, "yuv420sp_custom"))
            new_preview_format = V4L2_PIX_FMT_NV12T;
        else if (!strcmp(new_str_preview_format, "yuv422i"))
            new_preview_format = V4L2_PIX_FMT_YUYV;
        else if (!strcmp(new_str_preview_format, "yuv422p"))
            new_preview_format = V4L2_PIX_FMT_YUV422P;
        else
            new_preview_format = V4L2_PIX_FMT_NV21; //for 3rd party

        int current_preview_width, current_preview_height, current_frame_size;
        mSecCamera->getPreviewSize(&current_preview_width,
                                   &current_preview_height,
                                   &current_frame_size);
        int current_pixel_format = mSecCamera->getPreviewPixelFormat();

        if (current_preview_width != new_preview_width ||
            current_preview_height != new_preview_height ||
            current_pixel_format != new_preview_format) {
            if (mSecCamera->setPreviewSize(new_preview_width, new_preview_height,
                                           new_preview_format) < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setPreviewSize(width(%d), height(%d), format(%d))",
                     __func__, new_preview_width, new_preview_height, new_preview_format);
                ret = UNKNOWN_ERROR;
            } else {
                if (mPreviewWindow) {
                    if (mPreviewRunning && !mPreviewStartDeferred) {
                        ALOGE("ERR(%s): preview is running, cannot change size and format!", __func__);
                        ret = INVALID_OPERATION;
                    }
                    ALOGV("%s: mPreviewWindow (%p) set_buffers_geometry", __func__, mPreviewWindow);
                    ALOGV("%s: mPreviewWindow->set_buffers_geometry (%p)", __func__,
                         mPreviewWindow->set_buffers_geometry);
                    mPreviewWindow->set_buffers_geometry(mPreviewWindow,
                                                         new_preview_width, new_preview_height,
                                                         V4L2_PIX_2_HAL_PIXEL_FORMAT(new_preview_format));
                    ALOGV("%s: DONE mPreviewWindow (%p) set_buffers_geometry", __func__, mPreviewWindow);
                }
                mParameters.setPreviewSize(new_preview_width, new_preview_height);
                mParameters.setPreviewFormat(new_str_preview_format);
            }
        }
    } else {
        ALOGE("%s: Invalid preview size(%dx%d)",
                __func__, new_preview_width, new_preview_height);

        ret = INVALID_OPERATION;
    }

    // picture size
    int new_picture_width  = 0;
    int new_picture_height = 0;

    params.getPictureSize(&new_picture_width, &new_picture_height);
    ALOGV("%s : new_picture_width x new_picture_height = %dx%d", __func__, new_picture_width, new_picture_height);

    int current_picture_width, current_picture_height, current_picture_size;
    mSecCamera->getSnapshotSize(&current_picture_width, &current_picture_height, &current_picture_size);

    if (new_picture_width != current_picture_width ||
        new_picture_height != current_picture_height) {
        if (mSecCamera->setSnapshotSize(new_picture_width, new_picture_height) < 0) {
            ALOGE("ERR(%s):fail on mSecCamera->setSnapshotSize(width(%d), height(%d))",
                    __func__, new_picture_width, new_picture_height);
            ret = UNKNOWN_ERROR;
        } else {
#ifdef ZERO_SHUTTER_LAG
            mSecCamera->stopSnapshot();
            if (mUseInternalISP && !mRecordHint && mPreviewRunning){
                mSecCamera->startSnapshot(NULL);
            }
#endif
            mParameters.setPictureSize(new_picture_width, new_picture_height);
        }
    }

    // picture format
    const char *new_str_picture_format = params.getPictureFormat();
    ALOGV("%s : new_str_picture_format %s", __func__, new_str_picture_format);
    if (new_str_picture_format != NULL) {
        int new_picture_format = 0;

        if (!strcmp(new_str_picture_format, CameraParameters::PIXEL_FORMAT_RGB565))
            new_picture_format = V4L2_PIX_FMT_RGB565;
        else if (!strcmp(new_str_picture_format, CameraParameters::PIXEL_FORMAT_RGBA8888))
            new_picture_format = V4L2_PIX_FMT_RGB32;
        else if (!strcmp(new_str_picture_format, CameraParameters::PIXEL_FORMAT_YUV420SP))
            new_picture_format = V4L2_PIX_FMT_NV21;
        else if (!strcmp(new_str_picture_format, "yuv420sp_custom"))
            new_picture_format = V4L2_PIX_FMT_NV12T;
        else if (!strcmp(new_str_picture_format, "yuv420p"))
            new_picture_format = V4L2_PIX_FMT_YUV420;
        else if (!strcmp(new_str_picture_format, "yuv422i"))
            new_picture_format = V4L2_PIX_FMT_YUYV;
        else if (!strcmp(new_str_picture_format, "uyv422i_custom")) //Zero copy UYVY format
            new_picture_format = V4L2_PIX_FMT_UYVY;
        else if (!strcmp(new_str_picture_format, "uyv422i")) //Non-zero copy UYVY format
            new_picture_format = V4L2_PIX_FMT_UYVY;
        else if (!strcmp(new_str_picture_format, CameraParameters::PIXEL_FORMAT_JPEG))
            new_picture_format = V4L2_PIX_FMT_YUYV;
        else if (!strcmp(new_str_picture_format, "yuv422p"))
            new_picture_format = V4L2_PIX_FMT_YUV422P;
        else
            new_picture_format = V4L2_PIX_FMT_NV21; //for 3rd party

        if (mSecCamera->setSnapshotPixelFormat(new_picture_format) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setSnapshotPixelFormat(format(%d))", __func__, new_picture_format);
            ret = UNKNOWN_ERROR;
        } else
            mParameters.setPictureFormat(new_str_picture_format);
    }

    // JPEG image quality
    int new_jpeg_quality = params.getInt(CameraParameters::KEY_JPEG_QUALITY);
    ALOGV("%s : new_jpeg_quality %d", __func__, new_jpeg_quality);
    /* we ignore bad values */
    if (new_jpeg_quality >=1 && new_jpeg_quality <= 100) {
        if (mSecCamera->setJpegQuality(new_jpeg_quality) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setJpegQuality(quality(%d))", __func__, new_jpeg_quality);
            ret = UNKNOWN_ERROR;
        } else
            mParameters.set(CameraParameters::KEY_JPEG_QUALITY, new_jpeg_quality);
    }

    // JPEG thumbnail size
    int new_jpeg_thumbnail_width = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    int new_jpeg_thumbnail_height= params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
    if (0 <= new_jpeg_thumbnail_width && 0 <= new_jpeg_thumbnail_height) {
        if (mSecCamera->setJpegThumbnailSize(new_jpeg_thumbnail_width, new_jpeg_thumbnail_height) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setJpegThumbnailSize(width(%d), height(%d))", __func__, new_jpeg_thumbnail_width, new_jpeg_thumbnail_height);
            ret = UNKNOWN_ERROR;
        } else {
            mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, new_jpeg_thumbnail_width);
            mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, new_jpeg_thumbnail_height);
        }
    }

    // JPEG thumbnail quality
    int new_jpeg_thumbnail_quality = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
    ALOGV("%s : new_jpeg_thumbnail_quality %d", __func__, new_jpeg_thumbnail_quality);
    /* we ignore bad values */
    if (new_jpeg_thumbnail_quality >=1 && new_jpeg_thumbnail_quality <= 100) {
        if (mSecCamera->setJpegThumbnailQuality(new_jpeg_thumbnail_quality) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setJpegThumbnailQuality(quality(%d))",
                                               __func__, new_jpeg_thumbnail_quality);
            ret = UNKNOWN_ERROR;
        } else
            mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, new_jpeg_thumbnail_quality);
    }

    // frame rate
    int new_frame_rate = params.getPreviewFrameRate();
    /* ignore any fps request, we're determine fps automatically based
     * on scene mode.  don't return an error because it causes CTS failure.
     */
    if (mRecordHint) {
        if (new_frame_rate) {
            if (mUseInternalISP && (mSecCamera->setFrameRate(new_frame_rate) < 0)){
                ALOGE("ERR(%s):Fail on mSecCamera->setFrameRate(%d)", __func__, new_frame_rate);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.setPreviewFrameRate(new_frame_rate);
            }
        }
    }

    // rotation
    int new_rotation = params.getInt(CameraParameters::KEY_ROTATION);
    ALOGV("%s : new_rotation %d", __func__, new_rotation);
    if (0 <= new_rotation) {
        ALOGV("%s : set orientation:%d", __func__, new_rotation);
        if (mSecCamera->setExifOrientationInfo(new_rotation) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setExifOrientationInfo(%d)", __func__, new_rotation);
            ret = UNKNOWN_ERROR;
        } else
            mParameters.set(CameraParameters::KEY_ROTATION, new_rotation);
    }

    // zoom
    int new_zoom = params.getInt(CameraParameters::KEY_ZOOM);
    int current_zoom = mParameters.getInt(CameraParameters::KEY_ZOOM);
    ALOGV("%s : new_zoom %d", __func__, new_zoom);
    if (0 <= new_zoom) {
        if (new_zoom != current_zoom) {
            if (mSecCamera->setZoom(new_zoom) < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setZoom(zoom(%d))", __func__, new_zoom);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.set(CameraParameters::KEY_ZOOM, new_zoom);
            }
        }
    }

    // brightness
    int new_brightness = params.getInt("brightness");
    int max_brightness = params.getInt("brightness-max");
    int min_brightness = params.getInt("brightness-min");
    ALOGV("%s : new_brightness %d", __func__, new_brightness);
    if ((min_brightness <= new_brightness) &&
        (max_brightness >= new_brightness)) {
        if (mSecCamera->setBrightness(new_brightness) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setBrightness(brightness(%d))", __func__, new_brightness);
            ret = UNKNOWN_ERROR;
        } else {
            mParameters.set("brightness", new_brightness);
        }
    }

    // saturation
    int new_saturation = params.getInt("saturation");
    int max_saturation = params.getInt("saturation-max");
    int min_saturation = params.getInt("saturation-min");
    ALOGV("%s : new_saturation %d", __func__, new_saturation);
    if ((min_saturation <= new_saturation) &&
        (max_saturation >= new_saturation)) {
        if (mSecCamera->setSaturation(new_saturation) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setSaturation(saturation(%d))", __func__, new_saturation);
            ret = UNKNOWN_ERROR;
        } else {
            mParameters.set("saturation", new_saturation);
        }
    }

    // sharpness
    int new_sharpness = params.getInt("sharpness");
    int max_sharpness = params.getInt("sharpness-max");
    int min_sharpness = params.getInt("sharpness-min");
    ALOGV("%s : new_sharpness %d", __func__, new_sharpness);
    if ((min_sharpness <= new_sharpness) &&
        (max_sharpness >= new_sharpness)) {
        if (mSecCamera->setSharpness(new_sharpness) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setSharpness(sharpness(%d))", __func__, new_sharpness);
            ret = UNKNOWN_ERROR;
        } else {
            mParameters.set("sharpness", new_sharpness);
        }
    }

    // hue
    int new_hue = params.getInt("hue");
    int max_hue = params.getInt("hue-max");
    int min_hue = params.getInt("hue-min");
    ALOGV("%s : new_hue %d", __func__, new_hue);
    if ((min_hue <= new_hue) &&
        (max_hue >= new_hue)) {
        if (mSecCamera->setHue(new_hue) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setHue(hue(%d))", __func__, new_hue);
            ret = UNKNOWN_ERROR;
        } else {
            mParameters.set("hue", new_hue);
        }
    }

    // exposure
    int new_exposure_compensation = params.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
    int max_exposure_compensation = params.getInt(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION);
    int min_exposure_compensation = params.getInt(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION);
    ALOGV("%s : new_exposure_compensation %d", __func__, new_exposure_compensation);
    if ((min_exposure_compensation <= new_exposure_compensation) &&
        (max_exposure_compensation >= new_exposure_compensation)) {
        if (mSecCamera->setExposure(new_exposure_compensation) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setExposure(exposure(%d))", __func__, new_exposure_compensation);
            ret = UNKNOWN_ERROR;
        } else {
            mParameters.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, new_exposure_compensation);
        }
    }

    const char *new_AE_lock = params.get(CameraParameters::KEY_AUTO_EXPOSURE_LOCK);
    const char *old_AE_lock = mParameters.get(CameraParameters::KEY_AUTO_EXPOSURE_LOCK);
    if ((new_AE_lock != NULL) && mUseInternalISP && mPreviewRunning) {
        if (strncmp(new_AE_lock, old_AE_lock, 4)) {
            int ae_value = !strncmp(new_AE_lock, "true", 4);
            if (mSecCamera->setAutoExposureLock(ae_value) < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setExposureLock", __func__);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK, new_AE_lock);
            }
        }
    }

    // ISO
    const char *new_iso_str = params.get("iso");
    ALOGV("%s : new_iso_str %s", __func__, new_iso_str);
    if (new_iso_str != NULL) {
        int new_iso = -1;

        if (!strcmp(new_iso_str, "auto")) {
            new_iso = ISO_AUTO;
        } else if (!strcmp(new_iso_str, "50")) {
            new_iso = ISO_50;
        } else if (!strcmp(new_iso_str, "100")) {
            new_iso = ISO_100;
        } else if (!strcmp(new_iso_str, "200")) {
            new_iso = ISO_200;
        } else if (!strcmp(new_iso_str, "400")) {
            new_iso = ISO_400;
        } else if (!strcmp(new_iso_str, "800")) {
            new_iso = ISO_800;
        } else if (!strcmp(new_iso_str, "1600")) {
            new_iso = ISO_1600;
        } else {
            ALOGE("ERR(%s):Invalid iso value(%s)", __func__, new_iso_str);
            ret = UNKNOWN_ERROR;
        }

        if (0 <= new_iso) {
            if (mSecCamera->setISO(new_iso) < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setISO(iso(%d))", __func__, new_iso);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.set("iso", new_iso_str);
            }
        }
    }

    // Metering
    const char *new_metering_str = params.get("metering");
    ALOGV("%s : new_metering_str %s", __func__, new_metering_str);
    if (new_metering_str != NULL) {
        int new_metering = -1;

        if (!strcmp(new_metering_str, "center")) {
            new_metering = METERING_CENTER;
        } else if (!strcmp(new_metering_str, "spot")) {
            new_metering = METERING_SPOT;
        } else if (!strcmp(new_metering_str, "matrix")) {
            new_metering = METERING_MATRIX;
        } else {
            ALOGE("ERR(%s):Invalid metering value(%s)", __func__, new_metering_str);
            ret = UNKNOWN_ERROR;
        }

        if (0 <= new_metering) {
            if (mSecCamera->setMetering(new_metering) < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setMetering(metering(%d))", __func__, new_metering);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.set("metering", new_metering_str);
            }
        }
    }

    // AFC
    const char *new_antibanding_str = params.get(CameraParameters::KEY_ANTIBANDING);
    ALOGV("%s : new_antibanding_str %s", __func__, new_antibanding_str);
    if (new_antibanding_str != NULL) {
        int new_antibanding = -1;

        if (!strcmp(new_antibanding_str, CameraParameters::ANTIBANDING_AUTO)) {
            if (mUseInternalISP)
                new_antibanding = IS_AFC_AUTO;
            else
                new_antibanding = ANTI_BANDING_AUTO;
        } else if (!strcmp(new_antibanding_str, CameraParameters::ANTIBANDING_50HZ)) {
            if (mUseInternalISP)
                new_antibanding = IS_AFC_MANUAL_50HZ;
            else
                new_antibanding = ANTI_BANDING_50HZ;
        } else if (!strcmp(new_antibanding_str, CameraParameters::ANTIBANDING_60HZ)) {
            if (mUseInternalISP)
                new_antibanding = IS_AFC_MANUAL_60HZ;
            else
                new_antibanding = ANTI_BANDING_60HZ;
        } else if (!strcmp(new_antibanding_str, CameraParameters::ANTIBANDING_OFF)) {
            if (mUseInternalISP)
                new_antibanding = IS_AFC_DISABLE;
            else
                new_antibanding = ANTI_BANDING_OFF;
        } else {
            ALOGE("ERR(%s):Invalid antibanding value(%s)", __func__, new_antibanding_str);
            ret = UNKNOWN_ERROR;
        }

        if (0 <= new_antibanding) {
            if (mSecCamera->setAntiBanding(new_antibanding) < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setAntiBanding(antibanding(%d))", __func__, new_antibanding);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.set(CameraParameters::KEY_ANTIBANDING, new_antibanding_str);
            }
        }
    }

    // scene mode
    const char *new_scene_mode_str = params.get(CameraParameters::KEY_SCENE_MODE);
    const char *current_scene_mode_str = mParameters.get(CameraParameters::KEY_SCENE_MODE);

    // fps range
    int new_min_fps = 0;
    int new_max_fps = 0;
    int current_min_fps, current_max_fps;
    params.getPreviewFpsRange(&new_min_fps, &new_max_fps);
    mParameters.getPreviewFpsRange(&current_min_fps, &current_max_fps);
    /* our fps range is determined by the sensor, reject any request
     * that isn't exactly what we're already at.
     * but the check is performed when requesting only changing fps range
     */
    if (new_scene_mode_str && current_scene_mode_str) {
        if (!strcmp(new_scene_mode_str, current_scene_mode_str)) {
            if ((new_min_fps != current_min_fps) || (new_max_fps != current_max_fps)) {
                ALOGW("%s : requested new_min_fps = %d, new_max_fps = %d not allowed",
                        __func__, new_min_fps, new_max_fps);
                /* TODO : We need policy for fps. */
                ALOGW("%s : current_min_fps = %d, current_max_fps = %d",
                        __func__, current_min_fps, current_max_fps);
                //ret = UNKNOWN_ERROR;
            }
        }
    } else {
        /* Check basic validation if scene mode is different */
        if ((new_min_fps > new_max_fps) ||
            (new_min_fps < 0) || (new_max_fps < 0))
        ret = UNKNOWN_ERROR;
    }

    const char *new_flash_mode_str = params.get(CameraParameters::KEY_FLASH_MODE);
    const char *new_focus_mode_str = params.get(CameraParameters::KEY_FOCUS_MODE);
    const char *new_white_str = params.get(CameraParameters::KEY_WHITE_BALANCE);

    // fps range is (15000,30000) by default.
    mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(15000,30000)");
    mParameters.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "15000,30000");

    if ((new_scene_mode_str != NULL) && (current_scene_mode_str != NULL) && strncmp(new_scene_mode_str, current_scene_mode_str, 5)) {
        int  new_scene_mode = -1;
        if (!strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_AUTO)) {
            new_scene_mode = SCENE_MODE_NONE;
        } else {
            // defaults for non-auto scene modes
            new_focus_mode_str = CameraParameters::FOCUS_MODE_AUTO;
            new_flash_mode_str = CameraParameters::FLASH_MODE_OFF;
            new_white_str = CameraParameters::WHITE_BALANCE_AUTO;
            mParameters.set(CameraParameters::KEY_WHITE_BALANCE, new_white_str);

            if (!strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_PORTRAIT)) {
                new_scene_mode = SCENE_MODE_PORTRAIT;
                if (mCameraID == SecCamera::CAMERA_ID_BACK)
                    new_flash_mode_str = CameraParameters::FLASH_MODE_AUTO;
            } else if (!strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_LANDSCAPE)) {
                new_scene_mode = SCENE_MODE_LANDSCAPE;
            } else if (!strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_SPORTS)) {
                new_scene_mode = SCENE_MODE_SPORTS;
            } else if (!strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_PARTY)) {
                new_scene_mode = SCENE_MODE_PARTY_INDOOR;
                if (mCameraID == SecCamera::CAMERA_ID_BACK)
                    new_flash_mode_str = CameraParameters::FLASH_MODE_AUTO;
            } else if ((!strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_BEACH)) ||
                        (!strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_SNOW))) {
                new_scene_mode = SCENE_MODE_BEACH_SNOW;
            } else if (!strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_SUNSET)) {
                new_scene_mode = SCENE_MODE_SUNSET;
            } else if (!strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_NIGHT)) {
                new_scene_mode = SCENE_MODE_NIGHTSHOT;
                mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(4000,30000)");
                mParameters.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "4000,30000");
            } else if (!strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_FIREWORKS)) {
                new_scene_mode = SCENE_MODE_FIREWORKS;
            } else if (!strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_CANDLELIGHT)) {
                new_scene_mode = SCENE_MODE_CANDLE_LIGHT;
            } else {
                ALOGE("%s::unmatched scene_mode(%s)",
                        __func__, new_scene_mode_str); //action, night-portrait, theatre, steadyphoto
                ret = UNKNOWN_ERROR;
            }
        }

        if (0 <= new_scene_mode) {
            if (mSecCamera->setSceneMode(new_scene_mode) < 0) {
                ALOGE("%s::mSecCamera->setSceneMode(%d) fail", __func__, new_scene_mode);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.set(CameraParameters::KEY_SCENE_MODE, new_scene_mode_str);
            }
        }
    }

    // focus mode
    /* TODO : currently only posible focus modes at BACK camera */
    if ((new_focus_mode_str != NULL) && (mCameraID == SecCamera::CAMERA_ID_BACK)) {
        int  new_focus_mode = -1;

        if (!strcmp(new_focus_mode_str,
                    CameraParameters::FOCUS_MODE_AUTO)) {
            new_focus_mode = FOCUS_MODE_AUTO;
            mParameters.set(CameraParameters::KEY_FOCUS_DISTANCES,
                            BACK_CAMERA_AUTO_FOCUS_DISTANCES_STR);
        } else if (!strcmp(new_focus_mode_str,
                         CameraParameters::FOCUS_MODE_MACRO)) {
            new_focus_mode = FOCUS_MODE_MACRO;
            mParameters.set(CameraParameters::KEY_FOCUS_DISTANCES,
                            BACK_CAMERA_MACRO_FOCUS_DISTANCES_STR);
        } else if (!strcmp(new_focus_mode_str,
                         CameraParameters::FOCUS_MODE_INFINITY)) {
            new_focus_mode = FOCUS_MODE_INFINITY;
            mParameters.set(CameraParameters::KEY_FOCUS_DISTANCES,
                            BACK_CAMERA_INFINITY_FOCUS_DISTANCES_STR);
        } else if (!strcmp(new_focus_mode_str,
                         CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO) ||
                   !strcmp(new_focus_mode_str,
                         CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE)) {
            new_focus_mode = FOCUS_MODE_CONTINOUS;
        } else {
            /* TODO */
            /* This is temperary implementation.
               When camera support all AF mode, this code will be changing */
            ALOGE("%s::unmatched focus_mode(%s)", __func__, new_focus_mode_str);
            ret = UNKNOWN_ERROR;
        }

        if (0 <= new_focus_mode) {
            if (mSecCamera->setFocusMode(new_focus_mode) < 0) {
                ALOGE("%s::mSecCamera->setFocusMode(%d) fail", __func__, new_focus_mode);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.set(CameraParameters::KEY_FOCUS_MODE, new_focus_mode_str);
            }
        }
    }

    // flash..
    if (new_flash_mode_str != NULL) {
        int  new_flash_mode = -1;

        if (!strcmp(new_flash_mode_str, CameraParameters::FLASH_MODE_OFF))
            new_flash_mode = FLASH_MODE_OFF;
        else if (!strcmp(new_flash_mode_str, CameraParameters::FLASH_MODE_AUTO))
            new_flash_mode = FLASH_MODE_AUTO;
        else if (!strcmp(new_flash_mode_str, CameraParameters::FLASH_MODE_ON))
            new_flash_mode = FLASH_MODE_ON;
        else if (!strcmp(new_flash_mode_str, CameraParameters::FLASH_MODE_TORCH))
            new_flash_mode = FLASH_MODE_TORCH;
        else {
            ALOGE("%s::unmatched flash_mode(%s)", __func__, new_flash_mode_str); //red-eye
            ret = UNKNOWN_ERROR;
        }
        if (0 <= new_flash_mode) {
            if (mSecCamera->setFlashMode(new_flash_mode) < 0) {
                ALOGE("%s::mSecCamera->setFlashMode(%d) fail", __func__, new_flash_mode);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.set(CameraParameters::KEY_FLASH_MODE, new_flash_mode_str);
            }
        }
    }

    // whitebalance
    ALOGV("%s : new_white_str %s", __func__, new_white_str);
    if ((new_scene_mode_str != NULL) && !strcmp(new_scene_mode_str, CameraParameters::SCENE_MODE_AUTO)) {
        if (new_white_str != NULL) {
            int new_white = -1;

            if (!strcmp(new_white_str, CameraParameters::WHITE_BALANCE_AUTO)) {
                new_white = WHITE_BALANCE_AUTO;
            } else if (!strcmp(new_white_str,
                             CameraParameters::WHITE_BALANCE_DAYLIGHT)) {
                new_white = WHITE_BALANCE_SUNNY;
            } else if (!strcmp(new_white_str,
                             CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT)) {
                new_white = WHITE_BALANCE_CLOUDY;
            } else if (!strcmp(new_white_str,
                             CameraParameters::WHITE_BALANCE_FLUORESCENT)) {
                new_white = WHITE_BALANCE_FLUORESCENT;
            } else if (!strcmp(new_white_str,
                             CameraParameters::WHITE_BALANCE_INCANDESCENT)) {
                new_white = WHITE_BALANCE_TUNGSTEN;
            } else {
                ALOGE("ERR(%s):Invalid white balance(%s)", __func__, new_white_str); //twilight, shade, warm_flourescent
                ret = UNKNOWN_ERROR;
            }

            if (0 <= new_white) {
                if (mSecCamera->setWhiteBalance(new_white) < 0) {
                    ALOGE("ERR(%s):Fail on mSecCamera->setWhiteBalance(white(%d))", __func__, new_white);
                    ret = UNKNOWN_ERROR;
                } else {
                    mParameters.set(CameraParameters::KEY_WHITE_BALANCE, new_white_str);
                }
            }
        }
    }

    const char *new_AWB_lock = params.get(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK);
    const char *old_AWB_lock = mParameters.get(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK);
    if (new_AWB_lock != NULL && mUseInternalISP && mPreviewRunning) {
        if (strncmp(new_AWB_lock, old_AWB_lock, 4)) {
            int awb_value = !strncmp(new_AWB_lock, "true", 4);
            if (mSecCamera->setAutoWhiteBalanceLock(awb_value) < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setoAutoWhiteBalanceLock()", __func__);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, new_AWB_lock);
            }
        }
    }

    const char *new_touch_rect_str = params.get(CameraParameters::KEY_FOCUS_AREAS);
    ALOGV("Touched rect is '%s'", new_touch_rect_str);

    if (new_touch_rect_str != NULL) {
        int left = 0, top = 0, right = 0, bottom = 0, touched = 0;
        int objx, objy;

        char *end;
        char delim = ',';
        left = (int)strtol(new_touch_rect_str+1, &end, 10);
        if (*end != delim) {
            ALOGE("Cannot find '%c' in str=%s", delim, new_touch_rect_str);
            return -1;
        }
        top = (int)strtol(end+1, &end, 10);
        if (*end != delim) {
            ALOGE("Cannot find '%c' in str=%s", delim, new_touch_rect_str);
            return -1;
        }
        right = (int)strtol(end+1, &end, 10);
        if (*end != delim) {
            ALOGE("Cannot find '%c' in str=%s", delim, new_touch_rect_str);
            return -1;
        }
        bottom = (int)strtol(end+1, &end, 10);
        if (*end != delim) {
            ALOGE("Cannot find '%c' in str=%s", delim, new_touch_rect_str);
            return -1;
        }
        touched = (int)strtol(end+1, &end, 10);
        if (*end != ')') {
            ALOGE("Cannot find ')' in str=%s", new_touch_rect_str);
            return -1;
        }

        /* TODO : Converting axis and  Calcurating center of rect. Because driver need (x, y) point. */
        objx = (int)((1023 * (left + 1000)) / 2000) + 97;
        objy = (int)((1023 * (top + 1000)) / 2000) + 128;

        mTouched = touched;
        mSecCamera->setObjectPosition(objx, objy);
    }

    // image effect
    const char *new_image_effect_str = params.get(CameraParameters::KEY_EFFECT);
    if (new_image_effect_str != NULL) {

        int  new_image_effect = -1;

        if (!strcmp(new_image_effect_str, CameraParameters::EFFECT_NONE)) {
            new_image_effect = IMAGE_EFFECT_NONE;
        } else if (!strcmp(new_image_effect_str, CameraParameters::EFFECT_MONO)) {
            new_image_effect = IMAGE_EFFECT_BNW;
        } else if (!strcmp(new_image_effect_str, CameraParameters::EFFECT_SEPIA)) {
            new_image_effect = IMAGE_EFFECT_SEPIA;
        } else if (!strcmp(new_image_effect_str, CameraParameters::EFFECT_AQUA))
            new_image_effect = IMAGE_EFFECT_AQUA;
        else if (!strcmp(new_image_effect_str, CameraParameters::EFFECT_NEGATIVE)) {
            new_image_effect = IMAGE_EFFECT_NEGATIVE;
        } else {
            //posterize, whiteboard, blackboard, solarize
            ALOGE("ERR(%s):Invalid effect(%s)", __func__, new_image_effect_str);
            ret = UNKNOWN_ERROR;
        }

        if (new_image_effect >= 0) {
            if (mSecCamera->setImageEffect(new_image_effect) < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setImageEffect(effect(%d))", __func__, new_image_effect);
                ret = UNKNOWN_ERROR;
            } else {
                const char *old_image_effect_str = mParameters.get(CameraParameters::KEY_EFFECT);

                if (old_image_effect_str) {
                    if (strcmp(old_image_effect_str, new_image_effect_str)) {
                        setSkipFrame(EFFECT_SKIP_FRAME);
                    }
                }

                mParameters.set(CameraParameters::KEY_EFFECT, new_image_effect_str);
            }
        }
    }

    //contrast
    const char *new_contrast_str = params.get("contrast");
    ALOGV("%s : new_contrast_str %s", __func__, new_contrast_str);
    if (new_contrast_str != NULL) {
        int new_contrast = -1;

        if (!strcmp(new_contrast_str, "auto")) {
            if (mUseInternalISP)
                new_contrast = IS_CONTRAST_AUTO;
            else
                ALOGW("WARN(%s):Invalid contrast value (%s)", __func__, new_contrast_str);
        } else if (!strcmp(new_contrast_str, "-2")) {
            if (mUseInternalISP)
                new_contrast = IS_CONTRAST_MINUS_2;
            else
                new_contrast = CONTRAST_MINUS_2;
        } else if (!strcmp(new_contrast_str, "-1")) {
            if (mUseInternalISP)
                new_contrast = IS_CONTRAST_MINUS_1;
            else
                new_contrast = CONTRAST_MINUS_1;
        } else if (!strcmp(new_contrast_str, "0")) {
            if (mUseInternalISP)
                new_contrast = IS_CONTRAST_DEFAULT;
            else
                new_contrast = CONTRAST_DEFAULT;
        } else if (!strcmp(new_contrast_str, "1")) {
            if (mUseInternalISP)
                new_contrast = IS_CONTRAST_PLUS_1;
            else
                new_contrast = CONTRAST_PLUS_1;
        } else if (!strcmp(new_contrast_str, "2")) {
            if (mUseInternalISP)
                new_contrast = IS_CONTRAST_PLUS_2;
            else
                new_contrast = CONTRAST_PLUS_2;
        } else {
            ALOGE("ERR(%s):Invalid contrast value(%s)", __func__, new_contrast_str);
            ret = UNKNOWN_ERROR;
        }

        if (0 <= new_contrast) {
            if (mSecCamera->setContrast(new_contrast) < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setContrast(contrast(%d))", __func__, new_contrast);
                ret = UNKNOWN_ERROR;
            } else {
                mParameters.set("contrast", new_contrast_str);
            }
        }
    }

    //WDR
    int new_wdr = params.getInt("wdr");
    ALOGV("%s : new_wdr %d", __func__, new_wdr);

    if (0 <= new_wdr) {
        if (mSecCamera->setWDR(new_wdr) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setWDR(%d)", __func__, new_wdr);
            ret = UNKNOWN_ERROR;
        }
    }

    //anti shake
    int new_anti_shake = mInternalParameters.getInt("anti-shake");

    if (0 <= new_anti_shake) {
        if (mSecCamera->setAntiShake(new_anti_shake) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setWDR(%d)", __func__, new_anti_shake);
            ret = UNKNOWN_ERROR;
        }
    }

    // gps latitude
    const char *new_gps_latitude_str = params.get(CameraParameters::KEY_GPS_LATITUDE);
    if (mSecCamera->setGPSLatitude(new_gps_latitude_str) < 0) {
        ALOGE("%s::mSecCamera->setGPSLatitude(%s) fail", __func__, new_gps_latitude_str);
        ret = UNKNOWN_ERROR;
    } else {
        if (new_gps_latitude_str) {
            mParameters.set(CameraParameters::KEY_GPS_LATITUDE, new_gps_latitude_str);
        } else {
            mParameters.remove(CameraParameters::KEY_GPS_LATITUDE);
        }
    }

    // gps longitude
    const char *new_gps_longitude_str = params.get(CameraParameters::KEY_GPS_LONGITUDE);

    if (mSecCamera->setGPSLongitude(new_gps_longitude_str) < 0) {
        ALOGE("%s::mSecCamera->setGPSLongitude(%s) fail", __func__, new_gps_longitude_str);
        ret = UNKNOWN_ERROR;
    } else {
        if (new_gps_longitude_str) {
            mParameters.set(CameraParameters::KEY_GPS_LONGITUDE, new_gps_longitude_str);
        } else {
            mParameters.remove(CameraParameters::KEY_GPS_LONGITUDE);
        }
    }

    // gps altitude
    const char *new_gps_altitude_str = params.get(CameraParameters::KEY_GPS_ALTITUDE);

    if (mSecCamera->setGPSAltitude(new_gps_altitude_str) < 0) {
        ALOGE("%s::mSecCamera->setGPSAltitude(%s) fail", __func__, new_gps_altitude_str);
        ret = UNKNOWN_ERROR;
    } else {
        if (new_gps_altitude_str) {
            mParameters.set(CameraParameters::KEY_GPS_ALTITUDE, new_gps_altitude_str);
        } else {
            mParameters.remove(CameraParameters::KEY_GPS_ALTITUDE);
        }
    }

    // gps timestamp
    const char *new_gps_timestamp_str = params.get(CameraParameters::KEY_GPS_TIMESTAMP);

    if (mSecCamera->setGPSTimeStamp(new_gps_timestamp_str) < 0) {
        ALOGE("%s::mSecCamera->setGPSTimeStamp(%s) fail", __func__, new_gps_timestamp_str);
        ret = UNKNOWN_ERROR;
    } else {
        if (new_gps_timestamp_str) {
            mParameters.set(CameraParameters::KEY_GPS_TIMESTAMP, new_gps_timestamp_str);
        } else {
            mParameters.remove(CameraParameters::KEY_GPS_TIMESTAMP);
        }
    }

    // gps processing method
    const char *new_gps_processing_method_str = params.get(CameraParameters::KEY_GPS_PROCESSING_METHOD);

    if (mSecCamera->setGPSProcessingMethod(new_gps_processing_method_str) < 0) {
        ALOGE("%s::mSecCamera->setGPSProcessingMethod(%s) fail", __func__, new_gps_processing_method_str);
        ret = UNKNOWN_ERROR;
    } else {
        if (new_gps_processing_method_str) {
            mParameters.set(CameraParameters::KEY_GPS_PROCESSING_METHOD, new_gps_processing_method_str);
        } else {
            mParameters.remove(CameraParameters::KEY_GPS_PROCESSING_METHOD);
        }
    }

    // Recording size
    /* TODO */
    /* GED application don't set different recording size before recording button is pushed */
    int new_recording_width  = 0;
    int new_recording_height = 0;
    params.getVideoSize(&new_recording_width, &new_recording_height);
    ALOGV("new_recording_width (%d) new_recording_height (%d)",
            new_recording_width, new_recording_height);

    int current_recording_width, current_recording_height;
    mParameters.getVideoSize(&current_recording_width, &current_recording_height);
    ALOGV("current_recording_width (%d) current_recording_height (%d)",
            current_recording_width, current_recording_height);

    if (current_recording_width != new_recording_width ||
        current_recording_height != new_recording_height) {
        if (0 < new_recording_width && 0 < new_recording_height) {
            if (mSecCamera->setRecordingSize(new_recording_width, new_recording_height) < 0) {
                ALOGE("ERR(%s):Fail on mSecCamera->setRecordingSize(width(%d), height(%d))",
                        __func__, new_recording_width, new_recording_height);
                ret = UNKNOWN_ERROR;
            }
            mParameters.setVideoSize(new_recording_width, new_recording_height);
        }
    }

    //gamma
    const char *new_gamma_str = mInternalParameters.get("video_recording_gamma");

    if (new_gamma_str != NULL) {
        int new_gamma = -1;
        if (!strcmp(new_gamma_str, "off"))
            new_gamma = GAMMA_OFF;
        else if (!strcmp(new_gamma_str, "on"))
            new_gamma = GAMMA_ON;
        else {
            ALOGE("%s::unmatched gamma(%s)", __func__, new_gamma_str);
            ret = UNKNOWN_ERROR;
        }

        if (0 <= new_gamma) {
            if (mSecCamera->setGamma(new_gamma) < 0) {
                ALOGE("%s::mSecCamera->setGamma(%d) fail", __func__, new_gamma);
                ret = UNKNOWN_ERROR;
            }
        }
    }

    //slow ae
    const char *new_slow_ae_str = mInternalParameters.get("slow_ae");

    if (new_slow_ae_str != NULL) {
        int new_slow_ae = -1;

        if (!strcmp(new_slow_ae_str, "off"))
            new_slow_ae = SLOW_AE_OFF;
        else if (!strcmp(new_slow_ae_str, "on"))
            new_slow_ae = SLOW_AE_ON;
        else {
            ALOGE("%s::unmatched slow_ae(%s)", __func__, new_slow_ae_str);
            ret = UNKNOWN_ERROR;
        }

        if (0 <= new_slow_ae) {
            if (mSecCamera->setSlowAE(new_slow_ae) < 0) {
                ALOGE("%s::mSecCamera->setSlowAE(%d) fail", __func__, new_slow_ae);
                ret = UNKNOWN_ERROR;
            }
        }
    }

    /*Camcorder fix fps*/
    int new_sensor_mode = mInternalParameters.getInt("cam_mode");

    if (0 <= new_sensor_mode) {
        if (mSecCamera->setSensorMode(new_sensor_mode) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setSensorMode(%d)", __func__, new_sensor_mode);
            ret = UNKNOWN_ERROR;
        }
    } else {
        new_sensor_mode=0;
    }

    /*Shot mode*/
    int new_shot_mode = mInternalParameters.getInt("shot_mode");

    if (0 <= new_shot_mode) {
        if (mSecCamera->setShotMode(new_shot_mode) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setShotMode(%d)", __func__, new_shot_mode);
            ret = UNKNOWN_ERROR;
        }
    } else {
        new_shot_mode=0;
    }

    // chk_dataline
    int new_dataline = mInternalParameters.getInt("chk_dataline");

    if (0 <= new_dataline) {
        if (mSecCamera->setDataLineCheck(new_dataline) < 0) {
            ALOGE("ERR(%s):Fail on mSecCamera->setDataLineCheck(%d)", __func__, new_dataline);
            ret = UNKNOWN_ERROR;
        }
    }
    ALOGV("%s return ret = %d", __func__, ret);

    return ret;
}

CameraParameters CameraHardwareSec::getParameters() const
{
    ALOGV("%s :", __func__);
    return mParameters;
}

status_t CameraHardwareSec::sendCommand(int32_t command, int32_t arg1, int32_t arg2)
{
    /* TODO */
    /* CAMERA_CMD_START_FACE_DETECTION and CAMERA_CMD_STOP_FACE_DETECTION
       for Face Detection */
    if(command == CAMERA_CMD_START_FACE_DETECTION) {
        if (mSecCamera->setFaceDetect(FACE_DETECTION_ON) < 0) {
            ALOGE("ERR(%s): Fail on mSecCamera->startFaceDetection()");
            return BAD_VALUE;
        } else {
            return NO_ERROR;
        }
    }
    if(command == CAMERA_CMD_STOP_FACE_DETECTION) {
        if (mSecCamera->setFaceDetect(FACE_DETECTION_OFF) < 0) {
            ALOGE("ERR(%s): Fail on mSecCamera->stopFaceDetection()");
            return BAD_VALUE;
        } else {
            return NO_ERROR;
        }
    }

    return BAD_VALUE;
}

void CameraHardwareSec::release()
{
    ALOGV("%s", __func__);

    /* shut down any threads we have that might be running.  do it here
     * instead of the destructor.  we're guaranteed to be on another thread
     * than the ones below.  if we used the destructor, since the threads
     * have a reference to this object, we could wind up trying to wait
     * for ourself to exit, which is a deadlock.
     */
    if (mPreviewThread != NULL) {
        /* this thread is normally already in it's threadLoop but blocked
         * on the condition variable or running.  signal it so it wakes
         * up and can exit.
         */
        mPreviewThread->requestExit();
        mExitPreviewThread = true;
        mPreviewRunning = true; /* let it run so it can exit */
        mPreviewCondition.signal();
        mPreviewThread->requestExitAndWait();
        mPreviewThread.clear();
    }
    if (mAutoFocusThread != NULL) {
        /* this thread is normally already in it's threadLoop but blocked
         * on the condition variable.  signal it so it wakes up and can exit.
         */
        mFocusLock.lock();
        mAutoFocusThread->requestExit();
        mExitAutoFocusThread = true;
        mFocusCondition.signal();
        mFocusLock.unlock();
        mAutoFocusThread->requestExitAndWait();
        mAutoFocusThread.clear();
    }
    if (mPictureThread != NULL) {
        mPictureThread->requestExitAndWait();
        mPictureThread.clear();
    }

    if (mRawHeap) {
        mRawHeap->release(mRawHeap);
        mRawHeap = 0;
    }
    if (mPreviewHeap) {
        mPreviewHeap->release(mPreviewHeap);
        mPreviewHeap = 0;
    }
    for(int i = 0; i < BUFFER_COUNT_FOR_ARRAY; i++) {
        if (mRecordHeap[i]) {
            mRecordHeap[i]->release(mRecordHeap[i]);
            mRecordHeap[i] = 0;
        }
    }

     /* close after all the heaps are cleared since those
     * could have dup'd our file descriptor.
     */
    mSecCamera->DestroyCamera();
}

static CameraInfo sCameraInfo[] = {
    {
        CAMERA_FACING_BACK,
        90,  /* orientation */
    },
    {
        CAMERA_FACING_FRONT,
        90,  /* orientation */
    }
};

status_t CameraHardwareSec::storeMetaDataInBuffers(bool enable)
{
    // FIXME:
    // metadata buffer mode can be turned on or off.
    // Samsung needs to fix this.
    if (!enable) {
        ALOGE("Non-metadata buffer mode is not supported!");
        return INVALID_OPERATION;
    }
    return OK;
}

/** Close this device */

static camera_device_t *g_cam_device;

static int HAL_camera_device_close(struct hw_device_t* device)
{
    ALOGI("%s", __func__);
    if (device) {
        camera_device_t *cam_device = (camera_device_t *)device;
        delete static_cast<CameraHardwareSec *>(cam_device->priv);
        free(cam_device);
        g_cam_device = 0;
    }
    return 0;
}

static inline CameraHardwareSec *obj(struct camera_device *dev)
{
    return reinterpret_cast<CameraHardwareSec *>(dev->priv);
}

/** Set the preview_stream_ops to which preview frames are sent */
static int HAL_camera_device_set_preview_window(struct camera_device *dev,
                                                struct preview_stream_ops *buf)
{
    ALOGV("%s", __func__);
    return obj(dev)->setPreviewWindow(buf);
}

/** Set the notification and data callbacks */
static void HAL_camera_device_set_callbacks(struct camera_device *dev,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void* user)
{
    ALOGV("%s", __func__);
    obj(dev)->setCallbacks(notify_cb, data_cb, data_cb_timestamp,
                           get_memory,
                           user);
}

/**
 * The following three functions all take a msg_type, which is a bitmask of
 * the messages defined in include/ui/Camera.h
 */

/**
 * Enable a message, or set of messages.
 */
static void HAL_camera_device_enable_msg_type(struct camera_device *dev, int32_t msg_type)
{
    ALOGV("%s", __func__);
    obj(dev)->enableMsgType(msg_type);
}

/**
 * Disable a message, or a set of messages.
 *
 * Once received a call to disableMsgType(CAMERA_MSG_VIDEO_FRAME), camera
 * HAL should not rely on its client to call releaseRecordingFrame() to
 * release video recording frames sent out by the cameral HAL before and
 * after the disableMsgType(CAMERA_MSG_VIDEO_FRAME) call. Camera HAL
 * clients must not modify/access any video recording frame after calling
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME).
 */
static void HAL_camera_device_disable_msg_type(struct camera_device *dev, int32_t msg_type)
{
    ALOGV("%s", __func__);
    obj(dev)->disableMsgType(msg_type);
}

/**
 * Query whether a message, or a set of messages, is enabled.  Note that
 * this is operates as an AND, if any of the messages queried are off, this
 * will return false.
 */
static int HAL_camera_device_msg_type_enabled(struct camera_device *dev, int32_t msg_type)
{
    ALOGV("%s", __func__);
    return obj(dev)->msgTypeEnabled(msg_type);
}

/**
 * Start preview mode.
 */
static int HAL_camera_device_start_preview(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    return obj(dev)->startPreview();
}

/**
 * Stop a previously started preview.
 */
static void HAL_camera_device_stop_preview(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    obj(dev)->stopPreview();
}

/**
 * Returns true if preview is enabled.
 */
static int HAL_camera_device_preview_enabled(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    return obj(dev)->previewEnabled();
}

/**
 * Request the camera HAL to store meta data or real YUV data in the video
 * buffers sent out via CAMERA_MSG_VIDEO_FRAME for a recording session. If
 * it is not called, the default camera HAL behavior is to store real YUV
 * data in the video buffers.
 *
 * This method should be called before startRecording() in order to be
 * effective.
 *
 * If meta data is stored in the video buffers, it is up to the receiver of
 * the video buffers to interpret the contents and to find the actual frame
 * data with the help of the meta data in the buffer. How this is done is
 * outside of the scope of this method.
 *
 * Some camera HALs may not support storing meta data in the video buffers,
 * but all camera HALs should support storing real YUV data in the video
 * buffers. If the camera HAL does not support storing the meta data in the
 * video buffers when it is requested to do do, INVALID_OPERATION must be
 * returned. It is very useful for the camera HAL to pass meta data rather
 * than the actual frame data directly to the video encoder, since the
 * amount of the uncompressed frame data can be very large if video size is
 * large.
 *
 * @param enable if true to instruct the camera HAL to store
 *      meta data in the video buffers; false to instruct
 *      the camera HAL to store real YUV data in the video
 *      buffers.
 *
 * @return OK on success.
 */
static int HAL_camera_device_store_meta_data_in_buffers(struct camera_device *dev, int enable)
{
    ALOGV("%s", __func__);
    return obj(dev)->storeMetaDataInBuffers(enable);
}

/**
 * Start record mode. When a record image is available, a
 * CAMERA_MSG_VIDEO_FRAME message is sent with the corresponding
 * frame. Every record frame must be released by a camera HAL client via
 * releaseRecordingFrame() before the client calls
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME). After the client calls
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
 * responsibility to manage the life-cycle of the video recording frames,
 * and the client must not modify/access any video recording frames.
 */
static int HAL_camera_device_start_recording(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    return obj(dev)->startRecording();
}

/**
 * Stop a previously started recording.
 */
static void HAL_camera_device_stop_recording(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    obj(dev)->stopRecording();
}

/**
 * Returns true if recording is enabled.
 */
static int HAL_camera_device_recording_enabled(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    return obj(dev)->recordingEnabled();
}

/**
 * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
 *
 * It is camera HAL client's responsibility to release video recording
 * frames sent out by the camera HAL before the camera HAL receives a call
 * to disableMsgType(CAMERA_MSG_VIDEO_FRAME). After it receives the call to
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
 * responsibility to manage the life-cycle of the video recording frames.
 */
static void HAL_camera_device_release_recording_frame(struct camera_device *dev,
                                const void *opaque)
{
    ALOGV("%s", __func__);
    obj(dev)->releaseRecordingFrame(opaque);
}

/**
 * Start auto focus, the notification callback routine is called with
 * CAMERA_MSG_FOCUS once when focusing is complete. autoFocus() will be
 * called again if another auto focus is needed.
 */
static int HAL_camera_device_auto_focus(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    return obj(dev)->autoFocus();
}

/**
 * Cancels auto-focus function. If the auto-focus is still in progress,
 * this function will cancel it. Whether the auto-focus is in progress or
 * not, this function will return the focus position to the default.  If
 * the camera does not support auto-focus, this is a no-op.
 */
static int HAL_camera_device_cancel_auto_focus(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    return obj(dev)->cancelAutoFocus();
}

/**
 * Take a picture.
 */
static int HAL_camera_device_take_picture(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    return obj(dev)->takePicture();
}

/**
 * Cancel a picture that was started with takePicture. Calling this method
 * when no picture is being taken is a no-op.
 */
static int HAL_camera_device_cancel_picture(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    return obj(dev)->cancelPicture();
}

/**
 * Set the camera parameters. This returns BAD_VALUE if any parameter is
 * invalid or not supported.
 */
static int HAL_camera_device_set_parameters(struct camera_device *dev,
                                            const char *parms)
{
    ALOGV("%s", __func__);
    String8 str(parms);
    CameraParameters p(str);
    return obj(dev)->setParameters(p);
}

/** Return the camera parameters. */
char *HAL_camera_device_get_parameters(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    String8 str;
    CameraParameters parms = obj(dev)->getParameters();
    str = parms.flatten();
    return strdup(str.string());
}

static void HAL_camera_device_put_parameters(struct camera_device *dev, char *parms)
{
    ALOGV("%s", __func__);
    free(parms);
}

/**
 * Send command to camera driver.
 */
static int HAL_camera_device_send_command(struct camera_device *dev,
                    int32_t cmd, int32_t arg1, int32_t arg2)
{
    ALOGV("%s", __func__);
    return obj(dev)->sendCommand(cmd, arg1, arg2);
}

/**
 * Release the hardware resources owned by this object.  Note that this is
 * *not* done in the destructor.
 */
static void HAL_camera_device_release(struct camera_device *dev)
{
    ALOGV("%s", __func__);
    obj(dev)->release();
}

/**
 * Dump state of the camera hardware
 */
static int HAL_camera_device_dump(struct camera_device *dev, int fd)
{
    ALOGV("%s", __func__);
    return obj(dev)->dump(fd);
}

static int HAL_getNumberOfCameras()
{
    ALOGV("%s", __func__);

    int cam_fd;
    static struct v4l2_input input;

    cam_fd = open(CAMERA_DEV_NAME, O_RDONLY);
    if (cam_fd < 0) {
        ALOGE("ERR(%s):Cannot open %s (error : %s)", __func__, CAMERA_DEV_NAME, strerror(errno));
        return -1;
    }

    input.index = 0;
    while (ioctl(cam_fd, VIDIOC_ENUMINPUT, &input) == 0) {
        ALOGI("Name of input channel[%d] is %s", input.index, input.name);
        input.index++;
    }

    close(cam_fd);

    return --input.index;
}

static int HAL_getCameraInfo(int cameraId, struct camera_info *cameraInfo)
{
    ALOGV("%s", __func__);
    memcpy(cameraInfo, &sCameraInfo[cameraId], sizeof(CameraInfo));
    return 0;
}

#define SET_METHOD(m) m : HAL_camera_device_##m

static camera_device_ops_t camera_device_ops = {
        SET_METHOD(set_preview_window),
        SET_METHOD(set_callbacks),
        SET_METHOD(enable_msg_type),
        SET_METHOD(disable_msg_type),
        SET_METHOD(msg_type_enabled),
        SET_METHOD(start_preview),
        SET_METHOD(stop_preview),
        SET_METHOD(preview_enabled),
        SET_METHOD(store_meta_data_in_buffers),
        SET_METHOD(start_recording),
        SET_METHOD(stop_recording),
        SET_METHOD(recording_enabled),
        SET_METHOD(release_recording_frame),
        SET_METHOD(auto_focus),
        SET_METHOD(cancel_auto_focus),
        SET_METHOD(take_picture),
        SET_METHOD(cancel_picture),
        SET_METHOD(set_parameters),
        SET_METHOD(get_parameters),
        SET_METHOD(put_parameters),
        SET_METHOD(send_command),
        SET_METHOD(release),
        SET_METHOD(dump),
};

#undef SET_METHOD

static int HAL_camera_device_open(const struct hw_module_t* module,
                                  const char *id,
                                  struct hw_device_t** device)
{
    ALOGV("%s", __func__);

    int cameraId = atoi(id);
    if (cameraId < 0 || cameraId >= HAL_getNumberOfCameras()) {
        ALOGE("Invalid camera ID %s", id);
        return -EINVAL;
    }

    if (g_cam_device) {
        if (obj(g_cam_device)->getCameraId() == cameraId) {
            ALOGV("returning existing camera ID %s", id);
            goto done;
        } else {
            ALOGE("Cannot open camera %d. camera %d is already running!",
                    cameraId, obj(g_cam_device)->getCameraId());
            return -ENOSYS;
        }
    }

    g_cam_device = (camera_device_t *)malloc(sizeof(camera_device_t));
    if (!g_cam_device)
        return -ENOMEM;

    g_cam_device->common.tag     = HARDWARE_DEVICE_TAG;
    g_cam_device->common.version = 1;
    g_cam_device->common.module  = const_cast<hw_module_t *>(module);
    g_cam_device->common.close   = HAL_camera_device_close;

    g_cam_device->ops = &camera_device_ops;

    ALOGI("%s: open camera %s", __func__, id);

    g_cam_device->priv = new CameraHardwareSec(cameraId, g_cam_device);

done:
    *device = (hw_device_t *)g_cam_device;
    ALOGI("%s: opened camera %s (%p)", __func__, id, *device);
    return 0;
}

static hw_module_methods_t camera_module_methods = {
            .open = HAL_camera_device_open
};

extern "C" {
    struct camera_module HAL_MODULE_INFO_SYM = {
      .common = {
          .tag           = HARDWARE_MODULE_TAG,
          .version_major = 1,
          .version_minor = 0,
          .id            = CAMERA_HARDWARE_MODULE_ID,
          .name          = "orion camera HAL",
          .author        = "Samsung Corporation",
          .methods       = &camera_module_methods,
      },
      .get_number_of_cameras = HAL_getNumberOfCameras,
      .get_camera_info       = HAL_getCameraInfo
    };
}

}; // namespace android
