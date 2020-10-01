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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#if defined(BOARD_USE_V4L2)
#include "sec_utils_v4l2.h"
#include "s5p_tvout_v4l2.h"
#include "videodev2.h"
#else
#include "sec_utils.h"
#include "s5p_tvout.h"
#endif
#include "SecFimc.h"
#if defined(BOARD_USES_FIMGAPI)
#include "sec_g2d_4x.h"
#include "FimgApi.h"
#endif

#include "audio.h"
#include "video.h"
#include "../libhdmi/libsForhdmi/libedid/libedid.h"
#include "../libhdmi/libsForhdmi/libcec/libcec.h"

#include "SecHdmiCommon.h"
#include "SecHdmiV4L2Utils.h"

namespace android {

unsigned int output_type = V4L2_OUTPUT_TYPE_DIGITAL;
#if defined(BOARD_USE_V4L2)
unsigned int g_preset_id = V4L2_DV_1080P30;
#endif
v4l2_std_id t_std_id     = V4L2_STD_1080P_30;
int g_hpd_state   = HPD_CABLE_OUT;
unsigned int g_hdcp_en = 0;

int fp_tvout    = -1;
int fp_tvout_v  = -1;
int fp_tvout_g0 = -1;
int fp_tvout_g1 = -1;

struct vid_overlay_param vo_param;

#if defined(BOARD_USES_FIMGAPI)
unsigned int g2d_reserved_memory[HDMI_G2D_OUTPUT_BUF_NUM];
unsigned int g2d_reserved_memory_size   = 0;
unsigned int cur_g2d_address            = 0;
unsigned int g2d_buf_index              = 0;
#endif

void display_menu(void)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

    ALOGI("=============== HDMI Audio  =============\n");

    if (EDIDAudioModeSupport(&audio))
        ALOGI("=  2CH_PCM 44100Hz audio supported      =\n");

    ALOGI("========= HDMI Mode & Color Space =======\n");

    video.mode = HDMI;
    if (EDIDHDMIModeSupport(&video)) {
        video.colorSpace = HDMI_CS_YCBCR444;
        if (EDIDColorSpaceSupport(&video))
            ALOGI("=  1. HDMI(YCbCr)                       =\n");

        video.colorSpace = HDMI_CS_RGB;
        if (EDIDColorSpaceSupport(&video))
            ALOGI("=  2. HDMI(RGB)                         =\n");
    } else {
        video.mode = DVI;
        if (EDIDHDMIModeSupport(&video))
            ALOGI("=  3. DVI                               =\n");
    }

    ALOGI("===========    HDMI Rseolution   ========\n");

    /* 480P */
    video.resolution = v720x480p_60Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  4. 480P_60_16_9    (0x04000000)    =\n");

    video.resolution = v640x480p_60Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  5. 480P_60_4_3 (0x05000000)    =\n");

    /* 576P */
    video.resolution = v720x576p_50Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  6. 576P_50_16_9    (0x06000000)    =\n");

    video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  7. 576P_50_4_3 (0x07000000)    =\n");

    /* 720P 60 */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  8. 720P_60         (0x08000000)    =\n");

    /* 720P_50 */
    video.resolution = v1280x720p_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  9. 720P_50         (0x09000000)    =\n");

    /* 1080P_60 */
    video.resolution = v1920x1080p_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  a. 1080P_60        (0x0a000000)    =\n");

    /* 1080P_50 */
    video.resolution = v1920x1080p_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  b. 1080P_50        (0x0b000000)    =\n");

    /* 1080I_60 */
    video.resolution = v1920x1080i_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  c. 1080I_60        (0x0c000000)    =\n");

    /* 1080I_50 */
    video.resolution = v1920x1080i_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  d. 1080I_50        (0x0d000000)    =\n");

    /* 1080P_30 */
    video.resolution = v1920x1080p_30Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  e. 1080P_30        (0x12000000)    =\n");

    ALOGI("===========    HDMI 3D Format   ========\n");

    /* 720P_60_SBS_HALF */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  f. 720P_60_SBS_HALF    (0x13000000)    =\n");

    /* 720P_59_SBS_HALF */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  10. 720P_59_SBS_HALF    (0x14000000)    =\n");

    /* 720P_50_TB */
    video.resolution = v1280x720p_50Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  11. 720P_50_TB          (0x15000000)    =\n");

    /* 1080P_24_TB */
    video.resolution = v1920x1080p_24Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  12. 1080P_24_TB          (0x16000000)    =\n");

    /* 1080P_23_TB */
    video.resolution = v1920x1080p_24Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  13. 1080P_24_TB          (0x17000000)    =\n");
    ALOGI("=========================================\n");
}

int tvout_open(const char *fp_name)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int fp;

    fp = open(fp_name, O_RDWR);
    if (fp < 0)
        ALOGE("drv (%s) open failed!!\n", fp_name);

    return fp;
}
#if defined(BOARD_USE_V4L2)
int tvout_std_v4l2_init(int fd, unsigned int preset_id)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s:: preset_id = 0x%x", __func__, preset_id);
#endif

    int ret;
    struct v4l2_output output;
    struct v4l2_dv_preset preset;

    unsigned int matched = 0, i = 0;
    int output_index;

/*
    if (output_type >= V4L2_OUTPUT_TYPE_DIGITAL &&
        output_type <= V4L2_OUTPUT_TYPE_DVI)
        if (ioctl(fd_tvout, VIDIOC_HDCP_ENABLE, g_hdcp_en) < 0)
            ALOGE("%s::VIDIOC_HDCP_ENABLE failed %d", __func__, errno);
*/

    i = 0;

    do {
        output.index = i;
        ret = tvout_std_v4l2_enum_output(fd, &output);
        ALOGD("tvout_v4l2_enum_output():: output_type=%d output.index=%d output.name=%s", output.type, output.index, output.name);
        if (output.type == output_type) {
            matched = 1;
            break;
        }
        i++;
    } while (ret >=0);

    if (!matched) {
        ALOGE("%s::no matched output type [type=%d]", __func__, output_type);
//        return -1;
    }

    // set output
//    tvout_std_v4l2_s_output(fp_tvout, output.index);
//    output_index = 0;
//    tvout_std_v4l2_g_output(fp_tvout, &output_index);

//    if (output.capabilities & V4L2_OUT_CAP_PRESETS) {
        tvout_std_v4l2_enum_dv_presets(fd);
        preset.preset = preset_id;
        if (tvout_std_v4l2_s_dv_preset(fd, &preset) < 0 ) {
            ALOGE("%s::tvout_std_v4l2_s_dv_preset failed", __func__);
            return -1;
        }
//    }

    return 0;
}

int tvout_std_v4l2_querycap(int fd, char *node)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_capability v4l2cap;

    if (ioctl(fd, VIDIOC_QUERYCAP, &v4l2cap) < 0) {
        ALOGE("%s::VIDIOC_QUERYCAP failed", __func__);
        return -1;
    }

    if (!(v4l2cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("%s::%s is not support streaming", __func__, node);
        return -1;
    }

    if (!(v4l2cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)) {
        ALOGE("%s::%s is not support video output mplane", __func__, node);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_enum_dv_presets(int fd)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_dv_enum_preset enum_preset;
    int ret = -1;

    for (int index = 0; ; index++) {
        enum_preset.index = index;
        ret = ioctl(fd, VIDIOC_ENUM_DV_PRESETS, &enum_preset);

        if (ret < 0) {
            if (errno == EINVAL)
                break;
            ALOGE("%s::VIDIOC_ENUM_DV_PRESETS", __func__);
            return -1;
        }
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::index=%d, preset=0x%08x, name=%s, w=%d, h=%d",
              __func__, enum_preset.index, enum_preset.preset, enum_preset.name, enum_preset.width, enum_preset.height);
#endif
    }

    return 0;
}

int tvout_std_v4l2_s_dv_preset(int fd, struct v4l2_dv_preset *preset)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    if (ioctl(fd, VIDIOC_S_DV_PRESET, preset) < 0) {
        ALOGE("%s::VIDIOC_S_DV_PRESET failed", __func__);
        return -1;
    }

    return 0;
}

/*
   ioctl VIDIOC_ENUMOUTPUT
   To query the attributes of a video outputs applications initialize the index field of struct v4l2_output
   and call the VIDIOC_ENUMOUTPUT ioctl with a pointer to this structure. Drivers fill the rest of the
   structure or return an EINVAL error code when the index is out of bounds
   */
int tvout_std_v4l2_enum_output(int fd, struct v4l2_output *output)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret;

    ret = ioctl(fd, VIDIOC_ENUMOUTPUT, output);

    if (ret >=0)
        ALOGV("tvout_v4l2_enum_output" "enum. output [index = %d] :: type : 0x%08x , name = %s\n",
                output->index,output->type,output->name);

    return ret;
}

/*
   ioctl VIDIOC_G_OUTPUT, VIDIOC_S_OUTPUT
   To query the current video output applications call the VIDIOC_G_OUTPUT ioctl with a pointer to an
   integer where the driver stores the number of the output, as in the struct v4l2_output index field.
   This ioctl will fail only when there are no video outputs, returning the EINVAL error code
   */
int tvout_std_v4l2_s_output(int fd, int index)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s:: index = 0x%x", __func__, index);
#endif

    int ret;

    ret = ioctl(fd, VIDIOC_S_OUTPUT, &index);
    if (ret < 0) {
        ALOGE("tvout_v4l2_s_output" "VIDIOC_S_OUTPUT failed %d\n", errno);
        return ret;
    }

    return ret;
}

int tvout_std_v4l2_g_output(int fd, int *index)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret;

    ret = ioctl(fd, VIDIOC_G_OUTPUT, index);
    if (ret < 0) {
        ALOGE("tvout_v4l2_g_output" "VIDIOC_G_OUTPUT failed %d\n", errno);
        return ret;
    } else {
        ALOGV("tvout_v4l2_g_output" "Current output index %d\n", *index);
    }

    return ret;
}

int tvout_std_v4l2_s_fmt(int fd, enum v4l2_buf_type type, enum v4l2_field field, int w, int h, int colorformat, int num_planes)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_format fmt;

    fmt.type = type;
//    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
//        ALOGE("%s::VIDIOC_G_FMT failed", __func__);
//        return -1;
//    }

    switch (fmt.type) {
    case V4L2_BUF_TYPE_VIDEO_OUTPUT:
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        fmt.fmt.pix.width       = w;
        fmt.fmt.pix.height      = h;
        fmt.fmt.pix.pixelformat = colorformat;
        fmt.fmt.pix.field       = field;
        break;
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        fmt.fmt.pix_mp.width       = w;
        fmt.fmt.pix_mp.height      = h;
        fmt.fmt.pix_mp.pixelformat = colorformat;
        fmt.fmt.pix_mp.field       = field;
        fmt.fmt.pix_mp.num_planes  = num_planes;
        break;
    default:
        ALOGE("%s::invalid buffer type", __func__);
        return -1;
        break;
    }

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        ALOGE("%s::VIDIOC_S_FMT failed", __func__);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_s_crop(int fd, enum v4l2_buf_type type, enum v4l2_field, int x, int y, int w, int h)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_crop crop;

    crop.type     = type;
    crop.c.left   = x;
    crop.c.top    = y;
    crop.c.width  = w;
    crop.c.height = h;

    if (ioctl(fd, VIDIOC_S_CROP, &crop) < 0) {
        ALOGE("%s::VIDIOC_S_CROP (x=%d, y=%d, w=%d, h=%d) failed",
            __func__, x, y, w, h);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_s_ctrl(int fd, int id, int value)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_control vc;

    vc.id    = id;
    vc.value = value;

    if (ioctl(fd, VIDIOC_S_CTRL, &vc) < 0) {
        ALOGE("%s::VIDIOC_S_CTRL (id=%d,value=%d) failed", __func__, id, value);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_reqbuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, unsigned int num_bufs)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_requestbuffers reqbuf;

    reqbuf.type   = type;
    reqbuf.memory = memory;
    reqbuf.count  = num_bufs;

    if (ioctl(fd, VIDIOC_REQBUFS, &reqbuf) < 0) {
        ALOGE("%s::VIDIOC_REQBUFS failed", __func__);
        return -1;
    }

    if (reqbuf.count < num_bufs) {
        ALOGE("%s::VIDIOC_REQBUFS failed ((reqbuf.count(%d) < num_bufs(%d))",
            __func__, reqbuf.count, num_bufs);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_querybuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, unsigned int buf_index, unsigned int num_planes, SecBuffer *secBuf)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_buffer buf;
    struct v4l2_plane  planes[MAX_PLANES_MIXER];

    memset(&buf, 0, sizeof(struct v4l2_buffer));

    for (int i = 0; i < MAX_PLANES_MIXER; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    if (MAX_BUFFERS_MIXER <= buf_index || MAX_PLANES_MIXER <= num_planes) {
        ALOGE("%s::exceed MAX! : buf_index=%d, num_plane=%d", __func__, buf_index, num_planes);
        return -1;
    }

    buf.type     = type;
    buf.memory   = V4L2_MEMORY_MMAP;
    buf.index    = buf_index;
    buf.length   = num_planes;
    buf.m.planes = planes;

    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
        ALOGE("%s::VIDIOC_QUERYBUF failed, plane_cnt=%d", __func__, buf.length);
        return -1;
    }

    for (unsigned int i = 0; i < num_planes; i++) {
        if ((secBuf->virt.extP[i] = (char *)mmap(0, buf.m.planes[i].length,
                 PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.planes[i].m.mem_offset)) < 0) {
            ALOGE("%s::mmap failed", __func__);
            ALOGE("%s::Offset = 0x%x", __func__, buf.m.planes[i].m.mem_offset);
            ALOGE("%s::Legnth = %d"  , __func__, buf.m.planes[i].length);
            ALOGE("%s::vaddr[%d][%d] = 0x%x", __func__, buf_index, i, (unsigned int)secBuf->virt.extP[i]);
            return -1;
        }
        secBuf->size.extS[i] = buf.m.planes[i].length;

#ifdef DEBUG_LIB_FIMC
    ALOGD("%s::vaddr[bufidx=%d][planeidx=%d] = 0x%x", __func__, buf_index, i, (unsigned int)secBuf->virt.extP[i]);
    ALOGD("%s::Legnth = %d"  , __func__, buf.m.planes[i].length);
#endif
    }

    return 0;
}

int tvout_std_v4l2_qbuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, int buf_index, int num_planes, SecBuffer *secBuf)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_buffer buf;
    struct v4l2_plane  planes[MAX_PLANES_MIXER];

    memset(&buf, 0, sizeof(struct v4l2_buffer));

    for (int i = 0; i < MAX_PLANES_MIXER; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    buf.type     = type;
    buf.memory   = memory;
    buf.length   = num_planes;
    buf.index    = buf_index;
    buf.m.planes = planes;

    for (unsigned int i = 0; i < buf.length; i++) {
        buf.m.planes[i].m.userptr = (unsigned long)secBuf->virt.extP[i];
        buf.m.planes[i].length    = secBuf->size.extS[i];
    }

    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        ALOGE("%s::VIDIOC_QBUF failed", __func__);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_dqbuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, int *buf_index, int num_planes)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_buffer buf;
    struct v4l2_plane  planes[MAX_PLANES_MIXER];

    memset(&buf, 0, sizeof(struct v4l2_buffer));

    for (int i = 0; i < MAX_PLANES_MIXER; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    buf.type     = type;
    buf.memory   = memory;
    buf.length   = num_planes;
    buf.m.planes = planes;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        ALOGE("%s::VIDIOC_DQBUF failed", __func__);
        return -1;
    }
    *buf_index = buf.index;

    return 0;
}

int tvout_std_v4l2_streamon(int fd, enum v4l2_buf_type type)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        ALOGE("%s::VIDIOC_STREAMON failed", __func__);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_streamoff(int fd, enum v4l2_buf_type type)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        ALOGE("%s::VIDIOC_STREAMOFF failed", __func__);
        return -1;
    }

    return 0;
}
#else
int tvout_init(v4l2_std_id std_id)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s:: std_id = 0x%x", __func__, std_id);
#endif

    int ret;
    struct v4l2_output output;
    struct v4l2_standard std;
    v4l2_std_id std_g_id;
    struct tvout_param tv_g_param;

    unsigned int matched = 0, i = 0;
    int output_index;

    // It was initialized already
    if (fp_tvout <= 0) {
        fp_tvout = tvout_open(TVOUT_DEV);
        if (fp_tvout < 0) {
            ALOGE("tvout video drv open failed\n");
            return -1;
        }
    }

    if (output_type >= V4L2_OUTPUT_TYPE_DIGITAL &&
        output_type <= V4L2_OUTPUT_TYPE_DVI)
        if (ioctl(fp_tvout, VIDIOC_HDCP_ENABLE, g_hdcp_en) < 0)
            ALOGE("tvout_init" "VIDIOC_HDCP_ENABLE failed %d\n", errno);

    /* ============== query capability============== */
    tvout_v4l2_querycap(fp_tvout);

    tvout_v4l2_enum_std(fp_tvout, &std, std_id);

    // set std
    tvout_v4l2_s_std(fp_tvout, std_id);
    tvout_v4l2_g_std(fp_tvout, &std_g_id);

    i = 0;

    do {
        output.index = i;
        ret = tvout_v4l2_enum_output(fp_tvout, &output);
        if (output.type == output_type) {
            matched = 1;
            break;
        }
        i++;
    } while (ret >=0);

    if (!matched) {
        ALOGE("no matched output type [type : 0x%08x]\n", output_type);
        return -1;
    }

    // set output
    tvout_v4l2_s_output(fp_tvout, output.index);
    output_index = 0;
    tvout_v4l2_g_output(fp_tvout, &output_index);

    //set fmt param
    vo_param.src.base_y         = (void *)0x0;
    vo_param.src.base_c         = (void *)0x0;
    vo_param.src.pix_fmt.width  = 0;
    vo_param.src.pix_fmt.height = 0;
    vo_param.src.pix_fmt.field  = V4L2_FIELD_NONE;
    vo_param.src.pix_fmt.pixelformat = V4L2_PIX_FMT_NV12T;

    vo_param.src_crop.left    = 0;
    vo_param.src_crop.top     = 0;
    vo_param.src_crop.width   = 0;
    vo_param.src_crop.height  = 0;

    return fp_tvout;
}

int tvout_deinit()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    if (0 < fp_tvout) {
        close(fp_tvout);
        fp_tvout = -1;
    }
    return 0;
}

int tvout_v4l2_querycap(int fp)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s:: fp = 0x%x", __func__, fp);
#endif

    struct v4l2_capability cap;
    int ret;

    ret = ioctl(fp, VIDIOC_QUERYCAP, &cap);

    if (ret < 0) {
        ALOGE("tvout_v4l2_querycap" "VIDIOC_QUERYCAP failed %d\n", errno);
        return ret;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("tvout_v4l2_querycap" "DRIVER : %s, CARD : %s, CAP.: 0x%08x\n",
            cap.driver, cap.card, cap.capabilities);
#endif

    return ret;
}

/*
   ioctl VIDIOC_G_STD, VIDIOC_S_STD
   To query and select the current video standard applications use the VIDIOC_G_STD and
   VIDIOC_S_STD ioctls which take a pointer to a v4l2_std_id type as argument. VIDIOC_G_STD can
   return a single flag or a set of flags as in struct v4l2_standard field id
   */

int tvout_v4l2_g_std(int fp, v4l2_std_id *std_id)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret;

    ret = ioctl(fp, VIDIOC_G_STD, std_id);
    if (ret < 0) {
        ALOGE("tvout_v4l2_g_std" "VIDIOC_G_STD failed %d\n", errno);
        return ret;
    }

    return ret;
}

int tvout_v4l2_s_std(int fp, v4l2_std_id std_id)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s:: std_id = 0x%x", __func__, std_id);
#endif

    int ret;

    ret = ioctl(fp, VIDIOC_S_STD, &std_id);
    if (ret < 0) {
        ALOGE("tvout_v4l2_s_std" "VIDIOC_S_STD failed %d\n", errno);
        return ret;
    }

    return ret;
}

/*
   ioctl VIDIOC_ENUMSTD
   To query the attributes of a video standard, especially a custom (driver defined) one, applications
   initialize the index field of struct v4l2_standard and call the VIDIOC_ENUMSTD ioctl with a pointer
   to this structure. Drivers fill the rest of the structure or return an EINVAL error code when the index
   is out of bounds.
   */
int tvout_v4l2_enum_std(int fp, struct v4l2_standard *std, v4l2_std_id std_id)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    std->index = 0;
    while (0 == ioctl (fp, VIDIOC_ENUMSTD, std)) {
        if (std->id & std_id)
            ALOGV("tvout_v4l2_enum_std" "Current video standard: %s\n", std->name);

        std->index++;
    }

    return 0;
}

/*
   ioctl VIDIOC_ENUMOUTPUT
   To query the attributes of a video outputs applications initialize the index field of struct v4l2_output
   and call the VIDIOC_ENUMOUTPUT ioctl with a pointer to this structure. Drivers fill the rest of the
   structure or return an EINVAL error code when the index is out of bounds
   */
int tvout_v4l2_enum_output(int fp, struct v4l2_output *output)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret;

    ret = ioctl(fp, VIDIOC_ENUMOUTPUT, output);

    if (ret >=0)
        ALOGV("tvout_v4l2_enum_output" "enum. output [index = %d] :: type : 0x%08x , name = %s\n",
                output->index,output->type,output->name);

    return ret;
}

/*
   ioctl VIDIOC_G_OUTPUT, VIDIOC_S_OUTPUT
   To query the current video output applications call the VIDIOC_G_OUTPUT ioctl with a pointer to an
   integer where the driver stores the number of the output, as in the struct v4l2_output index field.
   This ioctl will fail only when there are no video outputs, returning the EINVAL error code
   */
int tvout_v4l2_s_output(int fp, int index)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s:: index = 0x%x", __func__, index);
#endif

    int ret;

    ret = ioctl(fp, VIDIOC_S_OUTPUT, &index);
    if (ret < 0) {
        ALOGE("tvout_v4l2_s_output" "VIDIOC_S_OUTPUT failed %d\n", errno);
        return ret;
    }

    return ret;
}

int tvout_v4l2_g_output(int fp, int *index)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret;

    ret = ioctl(fp, VIDIOC_G_OUTPUT, index);
    if (ret < 0) {
        ALOGE("tvout_v4l2_g_output" "VIDIOC_G_OUTPUT failed %d\n", errno);
        return ret;
    } else {
        ALOGV("tvout_v4l2_g_output" "Current output index %d\n", *index);
    }

    return ret;
}

/*
   ioctl VIDIOC_ENUM_FMT
   To enumerate image formats applications initialize the type and index field of struct v4l2_fmtdesc
   and call the VIDIOC_ENUM_FMT ioctl with a pointer to this structure. Drivers fill the rest of the
   structure or return an EINVAL error code. All formats are enumerable by beginning at index zero
   and incrementing by one until EINVAL is returned.
   */
int tvout_v4l2_enum_fmt(int fp, struct v4l2_fmtdesc *desc)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    desc->index = 0;
    while (0 == ioctl(fp, VIDIOC_ENUM_FMT, desc)) {
        ALOGV("tvout_v4l2_enum_fmt" "enum. fmt [id : 0x%08x] :: type = 0x%08x, name = %s, pxlfmt = 0x%08x\n",
                desc->index,
                desc->type,
                desc->description,
                desc->pixelformat);
        desc->index++;
    }

    return 0;
}

int tvout_v4l2_g_fmt(int fp, int buf_type, void* ptr)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret;
    struct v4l2_format format;
    struct v4l2_pix_format_s5p_tvout *fmt_param = (struct v4l2_pix_format_s5p_tvout*)ptr;

    format.type = (enum v4l2_buf_type)buf_type;

    ret = ioctl(fp, VIDIOC_G_FMT, &format);
    if (ret < 0) {
        ALOGE("tvout_v4l2_g_fmt" "type : %d, VIDIOC_G_FMT failed %d\n", buf_type, errno);
        return ret;
    } else {
        memcpy(fmt_param, format.fmt.raw_data, sizeof(struct v4l2_pix_format_s5p_tvout));
        ALOGV("tvout_v4l2_g_fmt" "get. fmt [base_c : 0x%08x], [base_y : 0x%08x] type = 0x%08x, width = %d, height = %d\n",
                fmt_param->base_c,
                fmt_param->base_y,
                fmt_param->pix_fmt.pixelformat,
                fmt_param->pix_fmt.width,
                fmt_param->pix_fmt.height);
    }

    return 0;
}

int tvout_v4l2_s_fmt(int fp, int buf_type, void *ptr)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_format format;
    int ret;

    format.type = (enum v4l2_buf_type)buf_type;
    switch (buf_type) {
    case V4L2_BUF_TYPE_VIDEO_OVERLAY:
        format.fmt.win =  *((struct v4l2_window *) ptr);
        break;

    case V4L2_BUF_TYPE_PRIVATE: {
        struct v4l2_vid_overlay_src *fmt_param =
            (struct v4l2_vid_overlay_src *) ptr;

        memcpy(format.fmt.raw_data, fmt_param,
                sizeof(struct v4l2_vid_overlay_src));
        break;
    }
    case V4L2_BUF_TYPE_VIDEO_OUTPUT: {
        struct v4l2_pix_format_s5p_tvout *fmt_param =
            (struct v4l2_pix_format_s5p_tvout *)ptr;
        memcpy(format.fmt.raw_data, fmt_param,
                sizeof(struct v4l2_pix_format_s5p_tvout));
        break;
    }
    default:
        break;
    }

    ret = ioctl(fp, VIDIOC_S_FMT, &format);
    if (ret < 0) {
        ALOGE("tvout_v4l2_s_fmt [tvout_v4l2_s_fmt] : type : %d, VIDIOC_S_FMT failed %d\n",
                buf_type, errno);
        return ret;
    }
    return 0;

}

int tvout_v4l2_g_fbuf(int fp, struct v4l2_framebuffer *frame)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret;

    ret = ioctl(fp, VIDIOC_G_FBUF, frame);
    if (ret < 0) {
        ALOGE("tvout_v4l2_g_fbuf" "VIDIOC_STREAMON failed %d\n", errno);
        return ret;
    }

    ALOGV("tvout_v4l2_g_fbuf" "get. fbuf: base = 0x%08X, pixel format = %d\n",
            frame->base,
            frame->fmt.pixelformat);
    return 0;
}

int tvout_v4l2_s_fbuf(int fp, struct v4l2_framebuffer *frame)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret;

    ret = ioctl(fp, VIDIOC_S_FBUF, frame);
    if (ret < 0) {
        ALOGE("tvout_v4l2_s_fbuf" "VIDIOC_STREAMON failed %d\n", errno);
        return ret;
    }
    return 0;
}

int tvout_v4l2_s_baseaddr(int fp, void *base_addr)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret;

    ret = ioctl(fp, S5PTVFB_WIN_SET_ADDR, base_addr);
    if (ret < 0) {
        ALOGE("tvout_v4l2_baseaddr" "VIDIOC_S_BASEADDR failed %d\n", errno);
        return ret;
    }
    return 0;
}

int tvout_v4l2_g_crop(int fp, unsigned int type, struct v4l2_rect *rect)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret;
    struct v4l2_crop crop;
    crop.type = (enum v4l2_buf_type)type;
    ret = ioctl(fp, VIDIOC_G_CROP, &crop);
    if (ret < 0) {
        ALOGE("tvout_v4l2_g_crop" "VIDIOC_G_CROP failed %d\n", errno);
        return ret;
    }

    rect->left  = crop.c.left;
    rect->top   = crop.c.top;
    rect->width = crop.c.width;
    rect->height    = crop.c.height;

    ALOGV("tvout_v4l2_g_crop" "get. crop : left = %d, top = %d, width  = %d, height = %d\n",
            rect->left,
            rect->top,
            rect->width,
            rect->height);
    return 0;
}

int tvout_v4l2_s_crop(int fp, unsigned int type, struct v4l2_rect *rect)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_crop crop;
    int ret;

    crop.type   = (enum v4l2_buf_type)type;

    crop.c.left     = rect->left;
    crop.c.top      = rect->top;
    crop.c.width    = rect->width;
    crop.c.height   = rect->height;

    ret = ioctl(fp, VIDIOC_S_CROP, &crop);
    if (ret < 0) {
        ALOGE("tvout_v4l2_s_crop" "VIDIOC_S_CROP failed %d\n", errno);
        return ret;
    }

    return 0;
}

int tvout_v4l2_start_overlay(int fp)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret, start = 1;

    ret = ioctl(fp, VIDIOC_OVERLAY, &start);
    if (ret < 0) {
        ALOGE("tvout_v4l2_start_overlay" "VIDIOC_OVERLAY failed\n");
        return ret;
    }

    return ret;
}

int tvout_v4l2_stop_overlay(int fp)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int ret, stop =0;

    ret = ioctl(fp, VIDIOC_OVERLAY, &stop);
    if (ret < 0) {
        ALOGE("tvout_v4l2_stop_overlay" "VIDIOC_OVERLAY failed\n");
        return ret;
    }

    return ret;
}
#endif

int hdmi_init_layer(int layer)
{
    int fd = -1;
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s (layer = %d) called", __func__, layer);
#endif

    switch (layer) {
    case HDMI_LAYER_VIDEO :
        if (fp_tvout_v <= 0) {
            fp_tvout_v = tvout_open(TVOUT_DEV_V);
            if (fp_tvout_v < 0) {
                ALOGE("tvout video layer open failed\n");
                return -1;
            }
            fd = fp_tvout_v;
        }
        break;
    case HDMI_LAYER_GRAPHIC_0 :
        if (fp_tvout_g0 <= 0) {
#if defined(BOARD_USE_V4L2)
            fp_tvout_g0 = tvout_open(TVOUT_DEV_G0);
#else
            fp_tvout_g0 = fb_open(TVOUT_FB_G0);
#endif
            if (fp_tvout_g0 < 0) {
                ALOGE("tvout graphic layer 0 open failed\n");
                return -1;
            }
            fd = fp_tvout_g0;
        }
        break;
    case HDMI_LAYER_GRAPHIC_1 :
        if (fp_tvout_g1 <= 0) {
#if defined(BOARD_USE_V4L2)
            fp_tvout_g1 = tvout_open(TVOUT_DEV_G1);
#else
            fp_tvout_g1 = fb_open(TVOUT_FB_G1);
#endif
            if (fp_tvout_g1 < 0) {
                ALOGE("tvout graphic layer 1 open failed\n");
                return -1;
            }
            fd = fp_tvout_g1;
        }
        break;
    default :
        ALOGE("%s::unmathced layer(%d) fail", __func__, layer);
        fd = -1;
        break;
    }

    return fd;
}

int hdmi_deinit_layer(int layer)
{
    int ret = 0;
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s(layer = %d) called", __func__, layer);
#endif
    switch (layer) {
    case HDMI_LAYER_VIDEO :
        if (0 < fp_tvout_v) {
            close(fp_tvout_v);
            fp_tvout_v = -1;
        }
        break;
    case HDMI_LAYER_GRAPHIC_0 :
        if (0 < fp_tvout_g0) {
            close(fp_tvout_g0);
            fp_tvout_g0 = -1;
        }
        break;
    case HDMI_LAYER_GRAPHIC_1 :
        if (0 < fp_tvout_g1) {
            close(fp_tvout_g1);
            fp_tvout_g1 = -1;
        }
        break;
    default :
        ALOGE("%s::unmathced layer(%d) fail", __func__, layer);
        ret = -1;
        break;
    }

    return ret;
}

#define ROUND_UP(value, boundary) ((((uint32_t)(value)) + \
                                  (((uint32_t) boundary)-1)) & \
                                  (~(((uint32_t) boundary)-1)))

void hdmi_cal_rect(int src_w, int src_h, int dst_w, int dst_h, struct v4l2_rect *dst_rect)
{
    if (dst_w * src_h <= dst_h * src_w) {
        dst_rect->left   = 0;
        dst_rect->top    = (dst_h - ((dst_w * src_h) / src_w)) >> 1;
        dst_rect->width  = dst_w;
        dst_rect->height = ((dst_w * src_h) / src_w);
    } else {
        dst_rect->left   = (dst_w - ((dst_h * src_w) / src_h)) >> 1;
        dst_rect->top    = 0;
        dst_rect->width  = ((dst_h * src_w) / src_h);
        dst_rect->height = dst_h;
    }
}

#if defined(BOARD_USE_V4L2)
int hdmi_get_src_plane(int srcColorFormat, unsigned int *num_of_plane)
{
    int v4l2ColorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(srcColorFormat);

    switch (v4l2ColorFormat) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_RGB32:
    case V4L2_PIX_FMT_RGB565X:
        *num_of_plane = 1;
        break;
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV12MT:
    case V4L2_PIX_FMT_NV21M:
        *num_of_plane = 2;
        break;
    default:
        ALOGE("%s::invalid color type", __func__);
        return -1;
    }

    return 0;
}
#endif

#if defined(BOARD_USE_V4L2)
int hdmi_set_v_param(int fd, int layer,
                      int srcColorFormat,
                      int src_w, int src_h,
                      SecBuffer * dstBuffer,
                      int dst_x, int dst_y, int dst_w, int dst_h)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int v4l2ColorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(srcColorFormat);
    int round_up_src_w;
    int round_up_src_h;
    unsigned int num_of_plane;
    struct v4l2_rect rect;

    /* src_w, src_h round up to DWORD because of VP restriction */
#if defined(SAMSUNG_EXYNOS4x12)
    round_up_src_w = ROUND_UP(src_w, 16);
#else defined(SAMSUNG_EXYNOS4210)
    round_up_src_w = ROUND_UP(src_w, 8);
#endif
    round_up_src_h = ROUND_UP(src_h, 8);

    switch (v4l2ColorFormat) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
        dstBuffer->size.s = (round_up_src_w * round_up_src_h * 3) >> 1;
        num_of_plane = 1;
        break;
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV12MT:
    case V4L2_PIX_FMT_NV21M:
        dstBuffer->size.extS[0] = (round_up_src_w * round_up_src_h * 3) >> 1;
        dstBuffer->size.extS[1] = (round_up_src_w * round_up_src_h * 3) >> 2;
        num_of_plane = 2;
        break;
    default:
        ALOGE("%s::invalid color type", __func__);
        return false;
        break;
    }

    hdmi_cal_rect(src_w, src_h, dst_w, dst_h, &rect);
    rect.left = ALIGN(rect.left, 16);

    /* set format for VP input */
    if (tvout_std_v4l2_s_fmt(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_FIELD_ANY, round_up_src_w, round_up_src_h, v4l2ColorFormat, num_of_plane) < 0) {
        ALOGE("%s::tvout_std_v4l2_s_fmt()[video layer] failed", __func__);
        return -1;
    }

    /* set crop for VP input */
    if (tvout_std_v4l2_s_crop(fd, V4L2_BUF_TYPE_VIDEO_OVERLAY, V4L2_FIELD_ANY, 0, 0, src_w, src_h) < 0) {
        ALOGE("%s::tvout_std_v4l2_s_crop()[video layer] failed", __func__);
        return -1;
    }

    /* set crop for VP output */
    if (tvout_std_v4l2_s_crop(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_FIELD_ANY, rect.left, rect.top, rect.width, rect.height) < 0) {
        ALOGE("%s::tvout_std_v4l2_s_crop()[video layer] failed", __func__);
        return -1;
    }

    /* request buffer for VP input */
    if (tvout_std_v4l2_reqbuf(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR, HDMI_NUM_MIXER_BUF) < 0) {
        ALOGE("%s::tvout_std_v4l2_reqbuf(buf_num=%d)[video layer] failed", __func__, HDMI_NUM_MIXER_BUF);
        return -1;
    }

    return 0;
}

int hdmi_set_g_param(int fd, int layer,
                      int srcColorFormat,
                      int src_w, int src_h,
                      SecBuffer * dstBuffer,
                      int dst_x, int dst_y, int dst_w, int dst_h)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_rect rect;
    int v4l2ColorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(srcColorFormat);

    rect.left   = dst_x;
    rect.top    = dst_y;

#if defined(BOARD_USES_FIMGAPI)
    rect.width  = dst_w;
    rect.height = dst_h;
#else
    rect.width  = src_w;
    rect.height = src_h;
#endif

    switch (v4l2ColorFormat) {
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_RGB32:
        dstBuffer->size.s = rect.width * rect.height << 2;
        break;
    case V4L2_PIX_FMT_RGB565X:
        dstBuffer->size.s = rect.width * rect.height << 1;
        break;
    default:
        ALOGE("%s::invalid color type", __func__);
        return false;
        break;
    }

    /* set format for mixer graphic layer input device*/
    if (tvout_std_v4l2_s_fmt(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_FIELD_ANY, rect.width, rect.height, v4l2ColorFormat, 1) < 0) {
        ALOGE("%s::tvout_std_v4l2_s_fmt() [layer=%d] failed", __func__, layer);
        return -1;
    }

    /* set crop for mixer graphic layer input device*/
    if (tvout_std_v4l2_s_crop(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_FIELD_ANY, rect.left, rect.top, rect.width, rect.height) < 0) {
        ALOGE("%s::tvout_std_v4l2_s_crop() [layer=%d] failed", __func__, layer);
        return -1;
    }

    /* request buffer for mixer graphic layer input device */
    if (tvout_std_v4l2_reqbuf(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR, HDMI_NUM_MIXER_BUF) < 0) {
        ALOGE("%s::tvout_std_v4l2_reqbuf(buf_num=%d) [layer=%d] failed", __func__, HDMI_NUM_MIXER_BUF, layer);
        return -1;
    }

    /* enable alpha blending for mixer graphic layer */
    if (tvout_std_v4l2_s_ctrl(fd, V4L2_CID_TV_LAYER_BLEND_ENABLE, 1) < 0) {
        ALOGE("%s::tvout_std_v4l2_s_ctrl() [layer=%d] failed", __func__, layer);
        return -1;
    }

    /* enable per-pixel blending for mixer graphic layer */
    if (tvout_std_v4l2_s_ctrl(fd, V4L2_CID_TV_PIXEL_BLEND_ENABLE, 1) < 0) {
        ALOGE("%s::tvout_std_v4l2_s_ctrl [layer=%d] failed", __func__, layer);
            return false;
    }

    /* set global alpha value for mixer graphic layer */
    if (tvout_std_v4l2_s_ctrl(fd, V4L2_CID_TV_LAYER_BLEND_ALPHA, 255) < 0) {
        ALOGE("%s::tvout_std_v4l2_s_ctrl() [layer=%d] failed", __func__, layer);
        return -1;
    }

    return 0;
}

int hdmi_set_g_scaling(int layer,
        int srcColorFormat,
        int src_w, int src_h,
        unsigned int src_address, SecBuffer * dstBuffer,
        int dst_x, int dst_y, int dst_w, int dst_h,
        int rotVal, unsigned int hwc_layer)
{
#if defined(BOARD_USES_FIMGAPI)
    int             dst_color_format;
    int             dst_bpp;
    unsigned char   *dst_addr;
    fimg2d_blit     BlitParam;
    rotation        g2d_rotation;

    fimg2d_addr srcAddr;
    fimg2d_image srcImage;
    fimg2d_rect srcRect;

    fimg2d_addr dstAddr;
    fimg2d_image dstImage;
    fimg2d_rect dstRect;

    fimg2d_clip dstClip;
    fimg2d_scale Scaling;

    switch (g_preset_id) {
    case V4L2_DV_1080P60:
    case V4L2_DV_1080P30:
    case V4L2_DV_1080I60:
    case V4L2_DV_720P60_SB_HALF:
    case V4L2_DV_720P59_94_SB_HALF:
    case V4L2_DV_1080P24_TB:
    case V4L2_DV_1080P23_98_TB:
        dst_color_format = CF_ARGB_8888;
        dst_bpp = 4;
        break;
    case V4L2_DV_480P60:
    case V4L2_DV_576P50:
    case V4L2_DV_720P60:
    case V4L2_DV_720P50_TB:
    default:
        dst_color_format = CF_ARGB_4444;
        dst_bpp = 2;
        break;
    }

    static unsigned int prev_src_addr = 0;

    if ((cur_g2d_address == 0) || (src_address != prev_src_addr)) {
        dst_addr = (unsigned char *)g2d_reserved_memory[g2d_buf_index];

        g2d_buf_index++;
        if (g2d_buf_index >= HDMI_G2D_OUTPUT_BUF_NUM)
            g2d_buf_index = 0;

        cur_g2d_address = (unsigned int)dst_addr;
        prev_src_addr = src_address;

        srcAddr = {(addr_space)ADDR_USER, (unsigned long)src_address, src_w * src_h * 4, 1, 0};
        srcImage = {srcAddr, srcAddr, src_w, src_h, src_w*4, AX_RGB, CF_ARGB_8888};
        srcRect = {0, 0, src_w, src_h};

        dstAddr = {(addr_space)ADDR_USER, (unsigned long)dst_addr, dst_w * dst_h * dst_bpp, 1, 0};
        dstImage = {dstAddr, dstAddr, dst_w, dst_h, dst_w*dst_bpp, AX_RGB, (color_format)dst_color_format};
        dstRect = {0, 0, dst_w, dst_h};
        dstClip = {0, 0, 0, dst_w, dst_h};

        if (rotVal == 0 || rotVal == 180)
            Scaling = {SCALING_BILINEAR, SCALING_PIXELS, 0, 0, src_w, src_h, dst_w, dst_h};
        else
            Scaling = {SCALING_BILINEAR, SCALING_PIXELS, 0, 0, src_w, src_h, dst_h, dst_w};

        switch (rotVal) {
        case 0:
            g2d_rotation = ORIGIN;
            break;
        case 90:
            g2d_rotation = ROT_90;
            break;
        case 180:
            g2d_rotation = ROT_180;
            break;
        case 270:
            g2d_rotation = ROT_270;
            break;
        default:
            ALOGE("%s::invalid rotVal(%d) fail", __func__, rotVal);
            return -1;
            break;
        }

        BlitParam = {BLIT_OP_SRC, NON_PREMULTIPLIED, 0xff, 0, g2d_rotation, &Scaling, 0, 0, &dstClip, 0, &srcImage, &dstImage, NULL, &srcRect, &dstRect, NULL, 0};

        if (stretchFimgApi(&BlitParam) < 0) {
            ALOGE("%s::stretchFimgApi() fail", __func__);
            return -1;
        }

#ifdef DEBUG_MSG_ENABLE
    ALOGD("hdmi_set_g_scaling:: \n \\
                layer=%d,\n \\
                srcColorFormat=%d,\n \\
                src_w=%d, src_h=%d,\n\\
                src_address=0x%x, dst_addr=0x%x,\n\\
                dst_x=%d, dst_y=%d, dst_w=%d, dst_h=%d ",
                layer,
                srcColorFormat,
                src_w, src_h,
                src_address, dst_addr,
                dst_x, dst_y, dst_w, dst_h);
#endif
        dstBuffer->virt.p = (char *)dst_addr;
    }
#else
    dstBuffer->virt.p = (char *)src_address;
#endif

    return 0;
}
#else
int hdmi_set_v_param(int layer,
        int src_w, int src_h, int colorFormat,
        unsigned int src_y_address, unsigned int src_c_address,
        int dst_w, int dst_h)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int round_up_src_w;
    int round_up_src_h;
    if (fp_tvout_v <= 0) {
        ALOGE("fp_tvout is < 0 fail\n");
        return -1;
    }

    /* src_w, src_h round up to DWORD because of VP restriction */
#if defined(SAMSUNG_EXYNOS4x12)
    round_up_src_w = ROUND_UP(src_w, 16);
#else defined(SAMSUNG_EXYNOS4210)
    round_up_src_w = ROUND_UP(src_w, 8);
#endif
    round_up_src_h = ROUND_UP(src_h, 8);

    vo_param.src.base_y         = (void *)src_y_address;
    vo_param.src.base_c         = (void *)src_c_address;
    vo_param.src.pix_fmt.width  = round_up_src_w;
    vo_param.src.pix_fmt.height = round_up_src_h;
    vo_param.src.pix_fmt.field  = V4L2_FIELD_NONE;
    vo_param.src.pix_fmt.pixelformat = colorFormat;

    tvout_v4l2_s_fmt(fp_tvout_v, V4L2_BUF_TYPE_PRIVATE, &vo_param.src);

    vo_param.src_crop.width   = src_w;
    vo_param.src_crop.height  = src_h;

    tvout_v4l2_s_crop(fp_tvout_v, V4L2_BUF_TYPE_PRIVATE, &vo_param.src_crop);

    if (dst_w * src_h <= dst_h * src_w) {
        vo_param.dst_win.w.left   = 0;
        vo_param.dst_win.w.top    = (dst_h - ((dst_w * src_h) / src_w)) >> 1;
        vo_param.dst_win.w.width  = dst_w;
        vo_param.dst_win.w.height = ((dst_w * src_h) / src_w);
    } else {
        vo_param.dst_win.w.left   = (dst_w - ((dst_h * src_w) / src_h)) >> 1;
        vo_param.dst_win.w.top    = 0;
        vo_param.dst_win.w.width  = ((dst_h * src_w) / src_h);
        vo_param.dst_win.w.height = dst_h;
    }

    vo_param.dst.fmt.priv = 10;
    vo_param.dst_win.global_alpha = 255;
    tvout_v4l2_s_fbuf(fp_tvout_v, &vo_param.dst);
    tvout_v4l2_s_fmt(fp_tvout_v, V4L2_BUF_TYPE_VIDEO_OVERLAY, &vo_param.dst_win);

    return 0;
}

int hdmi_gl_set_param(int layer,
        int srcColorFormat,
        int src_w, int src_h,
        unsigned int src_y_address, unsigned int src_c_address,
        int dst_x, int dst_y, int dst_w, int dst_h,
        int rotVal)
{
#if defined(BOARD_USES_FIMGAPI)
    int             dst_color_format;
    int             dst_bpp;
    unsigned char   *dst_addr;
    fimg2d_blit     BlitParam;
    rotation        g2d_rotation;

    fimg2d_addr srcAddr;
    fimg2d_image srcImage;
    fimg2d_rect srcRect;

    fimg2d_addr dstAddr;
    fimg2d_image dstImage;
    fimg2d_rect dstRect;

    fimg2d_clip dstClip;
    fimg2d_scale Scaling;

    struct fb_var_screeninfo var;
    struct s5ptvfb_user_window window;

    int fp_tvout_g;

    if(layer == HDMI_LAYER_GRAPHIC_0)
        fp_tvout_g = fp_tvout_g0;
    else
        fp_tvout_g = fp_tvout_g1;

    switch (t_std_id) {
    case V4L2_STD_1080P_60:
    case V4L2_STD_1080P_30:
    case V4L2_STD_1080I_60:
    case V4L2_STD_TVOUT_720P_60_SBS_HALF:
    case V4L2_STD_TVOUT_720P_59_SBS_HALF:
    case V4L2_STD_TVOUT_1080P_24_TB:
    case V4L2_STD_TVOUT_1080P_23_TB:
        dst_color_format = CF_ARGB_8888;
        dst_bpp = 4;
        var.bits_per_pixel = 32;
        var.transp.length = 8;
        break;
    case V4L2_STD_480P_60_16_9:
    case V4L2_STD_576P_50_16_9:
    case V4L2_STD_720P_60:
    case V4L2_STD_TVOUT_720P_50_TB:
    default:
        dst_color_format = CF_ARGB_4444;
        dst_bpp = 2;
        var.bits_per_pixel = 16;
        var.transp.length = 4;
        break;
    }

    static unsigned int prev_src_addr = 0;

    if ((cur_g2d_address == 0) || (src_y_address != prev_src_addr)) {
        dst_addr = (unsigned char *)g2d_reserved_memory[g2d_buf_index];

        g2d_buf_index++;
        if (g2d_buf_index >= HDMI_G2D_OUTPUT_BUF_NUM)
            g2d_buf_index = 0;

        cur_g2d_address = (unsigned int)dst_addr;
        prev_src_addr = src_y_address;

        srcAddr = {(addr_space)ADDR_PHYS, (unsigned long)src_y_address, src_w*src_h*4, 1, 0};
        srcImage = {srcAddr, srcAddr, src_w, src_h, src_w*4, AX_RGB, CF_ARGB_8888};
        srcRect = {0, 0, src_w, src_h};

        dstAddr = {(addr_space)ADDR_PHYS, (unsigned long)dst_addr, dst_w*dst_h*dst_bpp, 1, 0};
        dstImage = {dstAddr, dstAddr, dst_w, dst_h, dst_w*dst_bpp, AX_RGB, (color_format)dst_color_format};
        dstRect = {0, 0, dst_w, dst_h};
        dstClip = {0, 0, 0, dst_w, dst_h};

        if (rotVal == 0 || rotVal == 180)
            Scaling = {SCALING_BILINEAR, SCALING_PIXELS, 0, 0, src_w, src_h, dst_w, dst_h};
        else
            Scaling = {SCALING_BILINEAR, SCALING_PIXELS, 0, 0, src_w, src_h, dst_h, dst_w};

        switch (rotVal) {
        case 0:
            g2d_rotation = ORIGIN;
            break;
        case 90:
            g2d_rotation = ROT_90;
            break;
        case 180:
            g2d_rotation = ROT_180;
            break;
        case 270:
            g2d_rotation = ROT_270;
            break;
        default:
            ALOGE("%s::invalid rotVal(%d) fail", __func__, rotVal);
            return -1;
            break;
        }

        BlitParam = {BLIT_OP_SRC, NON_PREMULTIPLIED, 0xff, 0, g2d_rotation, &Scaling, 0, 0, &dstClip, 0, &srcImage, &dstImage, NULL, &srcRect, &dstRect, NULL, 0};

        if (stretchFimgApi(&BlitParam) < 0) {
            ALOGE("%s::stretchFimgApi() fail", __func__);
            return -1;
        }

        var.xres = dst_w;
        var.yres = dst_h;

        var.xres_virtual = var.xres;
        var.yres_virtual = var.yres;
        var.xoffset = 0;
        var.yoffset = 0;
        var.width = 0;
        var.height = 0;
        var.activate = FB_ACTIVATE_FORCE;

        window.x = dst_x;
        window.y = dst_y;

        tvout_v4l2_s_baseaddr(fp_tvout_g, (void *)dst_addr);
        put_vscreeninfo(fp_tvout_g, &var);

        if (ioctl(fp_tvout_g, S5PTVFB_WIN_POSITION, &window) < 0) {
            ALOGE("%s::S5PTVFB_WIN_POSITION ioctl failed.", __func__);
            return -1;
        }
    }

    return 0;
#else
    struct fb_var_screeninfo var;
    struct s5ptvfb_user_window window;

    struct overlay_param ov_param;

    // set base address for grp layer0 of mixer
    int fp_tvout_g;

#ifdef DEBUG_MSG_ENABLE
    ALOGD("hdmi_gl_set_param:: \n \\
                layer=%d,\n \\
                srcColorFormat=%d,\n \\
                src_w=%d, src_h=%d,\n\\
                src_y_address=0x%x, src_c_address=0x%x,\n\\
                dst_x=%d, dst_y=%d, dst_w=%d, dst_h=%d ",
                layer,
                srcColorFormat,
                src_w, src_h,
                src_y_address, src_c_address,
                dst_x, dst_y, dst_w, dst_h);
#endif

    if (layer == HDMI_LAYER_GRAPHIC_0)
        fp_tvout_g = fp_tvout_g0;
    else
        fp_tvout_g = fp_tvout_g1;

    var.xres = src_w;
    var.yres = src_h;
    var.xres_virtual = var.xres;
    var.yres_virtual = var.yres;
    var.xoffset = 0;
    var.yoffset = 0;
    var.width = src_w;
    var.height = src_h;
    var.activate = FB_ACTIVATE_FORCE;
    if (srcColorFormat == HAL_PIXEL_FORMAT_RGB_565) {
        var.bits_per_pixel = 16;
        var.transp.length = 0;
    }
    else {
        var.bits_per_pixel = 32;
        var.transp.length = 8;
    }

    window.x = dst_x;
    window.y = dst_y;

    tvout_v4l2_s_baseaddr(fp_tvout_g, (void *)src_y_address);
    put_vscreeninfo(fp_tvout_g, &var);
    if (ioctl(fp_tvout_g, S5PTVFB_WIN_POSITION, &window) < 0) {
        ALOGE("%s:: S5PTVFB_WIN_POSITION ioctl failed.", __func__);
        return -1;
    }

    return 0;
#endif
}
#endif

int hdmi_cable_status()
{
#if defined(BOARD_USE_V4L2)
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int cable_status = 0;
    int fd = 0;
    struct v4l2_control ctrl;

    fd = open(TVOUT_DEV_G0, O_RDWR);
    if (fd <= 0) {
        ALOGE("%s: graphic layer 0 drv open failed", __func__);
        return -1;
    }

    ctrl.id = V4L2_CID_TV_HPD_STATUS;

    if (ioctl(fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        ALOGE("Get HPD_STATUS fail");
        cable_status = -1;
    } else {
        cable_status = ctrl.value;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("HPD_STATUS = %d", cable_status);
#endif

    close(fd);

    return cable_status;
#else
    int cable_status = 0;
    int fp_hpd = 0;

    fp_hpd = open(HPD_DEV, O_RDWR);
    if (fp_hpd <= 0) {
        ALOGE("hpd drv open failed\n");
        return -1;
    }

    //Delay about 0.3s
    usleep(500000);
    if (ioctl(fp_hpd, HPD_GET_STATE, &cable_status) < 0) {
        ALOGE("hpd drv HPD_GET_STATE ioctl failed\n");
        cable_status = -1;
    }

    close(fp_hpd);

    return cable_status;
#endif
}

int hdmi_outputmode_2_v4l2_output_type(int output_mode)
{
    int v4l2_output_type = -1;

    switch (output_mode) {
    case HDMI_OUTPUT_MODE_YCBCR:
        v4l2_output_type = V4L2_OUTPUT_TYPE_DIGITAL;
        break;
    case HDMI_OUTPUT_MODE_RGB:
        v4l2_output_type = V4L2_OUTPUT_TYPE_HDMI_RGB;
        break;
    case HDMI_OUTPUT_MODE_DVI:
        v4l2_output_type = V4L2_OUTPUT_TYPE_DVI;
        break;
    case COMPOSITE_OUTPUT_MODE:
        v4l2_output_type = V4L2_OUTPUT_TYPE_COMPOSITE;
        break;
    default:
        ALOGE("%s::unmathced HDMI_mode(%d)", __func__, output_mode);
        v4l2_output_type = -1;
        break;
    }

    return v4l2_output_type;
}

int hdmi_v4l2_output_type_2_outputmode(int v4l2_output_type)
{
    int outputMode = -1;

    switch (v4l2_output_type) {
    case V4L2_OUTPUT_TYPE_DIGITAL:
        outputMode = HDMI_OUTPUT_MODE_YCBCR;
        break;
    case V4L2_OUTPUT_TYPE_HDMI_RGB:
        outputMode = HDMI_OUTPUT_MODE_RGB;
        break;
    case V4L2_OUTPUT_TYPE_DVI:
        outputMode = HDMI_OUTPUT_MODE_DVI;
        break;
    case V4L2_OUTPUT_TYPE_COMPOSITE:
        outputMode = COMPOSITE_OUTPUT_MODE;
        break;
    default:
        ALOGE("%s::unmathced v4l2_output_type(%d)", __func__, v4l2_output_type);
        outputMode = -1;
        break;
    }

    return outputMode;
}

int composite_std_2_v4l2_std_id(int std)
{
    int std_id = -1;

    switch (std) {
    case COMPOSITE_STD_NTSC_M:
        std_id = V4L2_STD_NTSC_M;
        break;
    case COMPOSITE_STD_NTSC_443:
        std_id = V4L2_STD_NTSC_443;
        break;
    case COMPOSITE_STD_PAL_BDGHI:
        std_id = V4L2_STD_PAL_BDGHI;
        break;
    case COMPOSITE_STD_PAL_M:
        std_id = V4L2_STD_PAL_M;
        break;
    case COMPOSITE_STD_PAL_N:
        std_id = V4L2_STD_PAL_N;
        break;
    case COMPOSITE_STD_PAL_Nc:
        std_id = V4L2_STD_PAL_Nc;
        break;
    case COMPOSITE_STD_PAL_60:
        std_id = V4L2_STD_PAL_60;
        break;
    default:
        ALOGE("%s::unmathced composite_std(%d)", __func__, std);
        break;
    }

    return std_id;
}

int hdmi_check_output_mode(int v4l2_output_type)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;
    int    calbirate_v4l2_mode = v4l2_output_type;

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

    switch (v4l2_output_type) {
    case V4L2_OUTPUT_TYPE_DIGITAL :
        video.mode = HDMI;
        if (!EDIDHDMIModeSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DVI;
            ALOGI("Change mode into DVI\n");
            break;
        }

        video.colorSpace = HDMI_CS_YCBCR444;
        if (!EDIDColorSpaceSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_HDMI_RGB;
            ALOGI("Change mode into HDMI_RGB\n");
        }
        break;

    case V4L2_OUTPUT_TYPE_HDMI_RGB:
        video.mode = HDMI;
        if (!EDIDHDMIModeSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DVI;
            ALOGI("Change mode into DVI\n");
            break;
        }

        video.colorSpace = HDMI_CS_RGB;
        if (!EDIDColorSpaceSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DIGITAL;
            ALOGI("Change mode into HDMI_YCBCR\n");
        }
        break;

    case V4L2_OUTPUT_TYPE_DVI:
        video.mode = DVI;
        if (!EDIDHDMIModeSupport(&video)) {
            video.colorSpace = HDMI_CS_YCBCR444;
            if (!EDIDColorSpaceSupport(&video)) {
                calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_HDMI_RGB;
                ALOGI("Change mode into HDMI_RGB\n");
            } else {
                calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DIGITAL;
                ALOGI("Change mode into HDMI_YCBCR\n");
            }
            break;
        }

        break;

    default:
        break;
    }
    return calbirate_v4l2_mode;
}

#if defined(BOARD_USE_V4L2)
int hdmi_check_resolution(unsigned int preset_id)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;

    switch (preset_id) {
    case V4L2_DV_480P60:
        video.resolution = v720x480p_60Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_576P50:
        video.resolution = v720x576p_50Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_720P60:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_720P50:
        video.resolution = v1280x720p_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080P60:
        video.resolution = v1920x1080p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080P50:
        video.resolution = v1920x1080p_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080I60:
        video.resolution = v1920x1080i_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080I50:
        video.resolution = v1920x1080i_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_480P59_94:
        video.resolution = v720x480p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_720P59_94:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080I59_94:
        video.resolution = v1920x1080i_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080P59_94:
        video.resolution = v1920x1080p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080P30:
        video.resolution = v1920x1080p_30Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_720P60_SB_HALF:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
        break;
    case V4L2_DV_720P59_94_SB_HALF:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
        break;
    case V4L2_DV_720P50_TB:
        video.resolution = v1280x720p_50Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    case V4L2_DV_1080P24_TB:
        video.resolution = v1920x1080p_24Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    case V4L2_DV_1080P23_98_TB:
        video.resolution = v1920x1080p_24Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    default:
        ALOGE("%s::unmathced preset_id(%d)", __func__, preset_id);
        return -1;
        break;
    }

    if (!EDIDVideoResolutionSupport(&video)) {
#ifdef DEBUG_MSG_ENABLE
        ALOGD("%s::EDIDVideoResolutionSupport(%d) fail (not suppoted preset_id) \n", __func__, preset_id);
#endif
        return -1;
    }

    return 0;
}

int hdmi_resolution_2_preset_id(unsigned int resolution, int * w, int * h, unsigned int *preset_id)
{
    int ret = 0;

    switch (resolution) {
    case 1080960:
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080P60;
        break;
    case 1080950:
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080P50;
        break;
    case 1080930:
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080P30;
        break;
    case 1080924:
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080P24_TB;
        break;
    case 1080160:
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080I60;
        break;
    case 1080150:
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080I50;
        break;
    case 720960:
        *w      = 1280;
        *h      = 720;
        *preset_id = V4L2_DV_720P60;
        break;
    case 7209601:
        *w      = 1280;
        *h      = 720;
        *preset_id = V4L2_DV_720P60_SB_HALF;
        break;
    case 720950:
        *w      = 1280;
        *h      = 720;
        *preset_id = V4L2_DV_720P50;
        break;
    case 7209501:
        *w      = 1280;
        *h      = 720;
        *preset_id = V4L2_DV_720P50_TB;
        break;
    case 5769501:
        *w      = 720;
        *h      = 576;
        *preset_id = V4L2_DV_576P50;
        break;
    case 5769502:
        *w      = 720;
        *h      = 576;
        *preset_id = V4L2_DV_576P50;
       break;
    case 4809601:
        *w      = 720;
        *h      = 480;
        *preset_id = V4L2_DV_480P60;
       break;
    case 4809602:
        *w     = 720;
        *h     = 480;
        *preset_id = V4L2_DV_480P60;
      break;
    default:
        ALOGE("%s::unmathced resolution(%d)", __func__, resolution);
        ret = -1;
        break;
    }

    return ret;
}
#else
int hdmi_check_resolution(v4l2_std_id std_id)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;

    switch (std_id) {
    case V4L2_STD_480P_60_16_9:
        video.resolution = v720x480p_60Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_480P_60_4_3:
        video.resolution = v640x480p_60Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_576P_50_16_9:
        video.resolution = v720x576p_50Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_576P_50_4_3:
        video.resolution = v720x576p_50Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_720P_60:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_720P_50:
        video.resolution = v1280x720p_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080P_60:
        video.resolution = v1920x1080p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080P_50:
        video.resolution = v1920x1080p_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080I_60:
        video.resolution = v1920x1080i_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080I_50:
        video.resolution = v1920x1080i_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_480P_59:
        video.resolution = v720x480p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_720P_59:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080I_59:
        video.resolution = v1920x1080i_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080P_59:
        video.resolution = v1920x1080p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080P_30:
        video.resolution = v1920x1080p_30Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_TVOUT_720P_60_SBS_HALF:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
        break;
    case V4L2_STD_TVOUT_720P_59_SBS_HALF:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
        break;
    case V4L2_STD_TVOUT_720P_50_TB:
        video.resolution = v1280x720p_50Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    case V4L2_STD_TVOUT_1080P_24_TB:
        video.resolution = v1920x1080p_24Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    case V4L2_STD_TVOUT_1080P_23_TB:
        video.resolution = v1920x1080p_24Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    default:
        ALOGE("%s::unmathced std_id(%lld)", __func__, std_id);
        return -1;
        break;
    }

    if (!EDIDVideoResolutionSupport(&video)) {
#ifdef DEBUG_MSG_ENABLE
        ALOGD("%s::EDIDVideoResolutionSupport(%llx) fail (not suppoted std_id) \n", __func__, std_id);
#endif
        return -1;
    }

    return 0;
}

int hdmi_resolution_2_std_id(unsigned int resolution, int * w, int * h, v4l2_std_id * std_id)
{
    int ret = 0;

    switch (resolution) {
    case 1080960:
        *std_id = V4L2_STD_1080P_60;
        *w      = 1920;
        *h      = 1080;
        break;
    case 1080950:
        *std_id = V4L2_STD_1080P_50;
        *w      = 1920;
        *h      = 1080;
        break;
    case 1080930:
        *std_id = V4L2_STD_1080P_30;
        *w      = 1920;
        *h      = 1080;
        break;
    case 1080924:
        *std_id = V4L2_STD_TVOUT_1080P_24_TB;
        *w      = 1920;
        *h      = 1080;
        break;
    case 1080160:
        *std_id = V4L2_STD_1080I_60;
        *w      = 1920;
        *h      = 1080;
        break;
    case 1080150:
        *std_id = V4L2_STD_1080I_50;
        *w      = 1920;
        *h      = 1080;
        break;
    case 720960:
        *std_id = V4L2_STD_720P_60;
        *w      = 1280;
        *h      = 720;
        break;
    case 7209601:
        *std_id = V4L2_STD_TVOUT_720P_60_SBS_HALF;
        *w      = 1280;
        *h      = 720;
        break;
    case 720950:
        *std_id = V4L2_STD_720P_50;
        *w      = 1280;
        *h      = 720;
        break;
    case 7209501:
        *std_id = V4L2_STD_TVOUT_720P_50_TB;
        *w      = 1280;
        *h      = 720;
        break;
    case 5769501:
        *std_id = V4L2_STD_576P_50_16_9;
        *w      = 720;
        *h      = 576;
        break;
    case 5769502:
        *std_id = V4L2_STD_576P_50_4_3;
        *w      = 720;
        *h      = 576;
        break;
    case 4809601:
        *std_id = V4L2_STD_480P_60_16_9;
        *w      = 720;
        *h      = 480;
        break;
    case 4809602:
        *std_id = V4L2_STD_480P_60_4_3;
        *w     = 720;
        *h     = 480;
        break;
    default:
        ALOGE("%s::unmathced resolution(%d)", __func__, resolution);
        ret = -1;
        break;
    }

    return ret;
}
#endif

int hdmi_enable_hdcp(unsigned int hdcp_en)
{
    if (ioctl(fp_tvout, VIDIOC_HDCP_ENABLE, hdcp_en) < 0) {
        ALOGD("%s::VIDIOC_HDCP_ENABLE(%d) fail \n", __func__, hdcp_en);
        return -1;
    }

    return 0;
}

int hdmi_check_audio(void)
{
    struct HDMIAudioParameter audio;
    enum state audio_state = ON;
    int ret = 0;

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

#if defined(BOARD_USES_EDID)
    if (!EDIDAudioModeSupport(&audio))
        audio_state = NOT_SUPPORT;
    else
        audio_state = ON;
#endif
    if (audio_state == ON) {
        if (ioctl(fp_tvout, VIDIOC_INIT_AUDIO, 1) < 0) {
            ALOGE("%s::VIDIOC_INIT_AUDIO(1) fail", __func__);
            ret = -1;
        }
    } else {
        if (ioctl(fp_tvout, VIDIOC_INIT_AUDIO, 0) < 0) {
            ALOGE("%s::VIDIOC_INIT_AUDIO(0) fail", __func__);
            ret = -1;
        }
    }

    return ret;
}

}
