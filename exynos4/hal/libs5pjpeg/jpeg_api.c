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
#define LOG_TAG "Jpeg-api"

#include <utils/Log.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <math.h>
#include <sys/poll.h>

#ifdef S5P_VMEM
#include "s5p_vmem_api.h"
#endif
#include "jpeg_api.h"

static struct jpeg_lib  *jpeg_ctx = NULL;
#ifdef S5P_VMEM
static int mem_fp;
unsigned int cookie;
#endif /* S5P_VMEM */

static unsigned int get_yuv_size(enum jpeg_frame_format out_format,
                                unsigned int width, unsigned int height)
{
    switch (out_format) {
    case YUV_422 :
        if (width % 16 != 0)
            width += 16 - (width % 16);
        if (height % 8 != 0)
            height += 8 - (height % 8);
        break;

    case YUV_420 :
        if (width % 16 != 0)
            width += 16 - (width % 16);
        if (height % 16 != 0)
            height += 16 - (height % 16);
        break;

    default:
        ALOGV("get_yuv_size return fmt(%d)\n", out_format);
        return(0);
    }

    ALOGV("get_yuv_size width(%d) height(%d)\n", width, height);

    switch (out_format) {
    case YUV_422 :
        return(width*height*2);
    case YUV_420 :
        return((width*height) + (width*height >> 1));
    default :
        return(0);
    }
}

static int check_input_size(unsigned int width, unsigned int height)
{
    if ((width % 16) != 0 || (height % 8) != 0)
        return -1;

    return 0;
}

static void init_decode_param(void)
{
    jpeg_ctx = (struct jpeg_lib *)malloc(sizeof(struct jpeg_lib));
    memset(jpeg_ctx, 0x00, sizeof(struct jpeg_lib));

    jpeg_ctx->args.dec_param = (struct jpeg_dec_param *)malloc(sizeof(struct jpeg_dec_param));
    memset(jpeg_ctx->args.dec_param, 0x00, sizeof(struct jpeg_dec_param));
}

static void init_encode_param(void)
{
    jpeg_ctx = (struct jpeg_lib *)malloc(sizeof(struct jpeg_lib));
    memset(jpeg_ctx, 0x00, sizeof(struct jpeg_lib));

    jpeg_ctx->args.enc_param = (struct jpeg_enc_param *)malloc(sizeof(struct jpeg_enc_param));
    memset(jpeg_ctx->args.enc_param, 0x00, sizeof(struct jpeg_enc_param));
}

int api_jpeg_decode_init()
{
    init_decode_param();
    jpeg_ctx->jpeg_fd = open(JPEG_DRIVER_NAME, O_RDWR);

    if (jpeg_ctx->jpeg_fd < 0) {
        ALOGE("JPEG driver open failed\n");
        return -1;
    }

#ifdef S5P_VMEM
    mem_fp = s5p_vmem_open();
    ALOGV("s5p_vmem_open\n");
#else
    jpeg_ctx->args.mmapped_addr = (char *) mmap(0,
                        JPEG_TOTAL_BUF_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        jpeg_ctx->jpeg_fd, 0);

    if (jpeg_ctx->args.mmapped_addr == NULL) {
        ALOGE("JPEG mmap failed\n");
        return -1;
    }
    ALOGV("api_jpeg_decode_init jpeg_ctx->args.mmapped_addr 0x%08x\n",
                                                jpeg_ctx->args.mmapped_addr);
#endif /* S5P_VMEM */

    return jpeg_ctx->jpeg_fd;
}

int api_jpeg_encode_init()
{
    init_encode_param();
    jpeg_ctx->jpeg_fd = open(JPEG_DRIVER_NAME, O_RDWR);

    if (jpeg_ctx->jpeg_fd < 0) {
        ALOGE("JPEG driver open failed %d\n", jpeg_ctx->jpeg_fd);
        return -1;
    }

#ifdef S5P_VMEM
    mem_fp = s5p_vmem_open();
    ALOGI("s5p_vmem_open\n");
#else

    jpeg_ctx->args.mmapped_addr = (char *) mmap(0,
            JPEG_TOTAL_BUF_SIZE,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            jpeg_ctx->jpeg_fd, 0);

    if (jpeg_ctx->args.mmapped_addr == NULL) {
        ALOGE("JPEG mmap failed\n");
        return -1;
    }
    ALOGV("api_jpeg_encode_init jpeg_ctx->args.mmapped_addr 0x%08x\n",
                                                jpeg_ctx->args.mmapped_addr);
#endif /* S5P_VMEM */
    return jpeg_ctx->jpeg_fd;
}

int api_jpeg_decode_deinit(int dev_fd)
{
    if (jpeg_ctx->args.mmapped_addr != NULL)
        munmap(jpeg_ctx->args.mmapped_addr, JPEG_TOTAL_BUF_SIZE);

#ifdef S5P_VMEM
    s5p_free_share(mem_fp, jpeg_ctx->args.in_cookie, jpeg_ctx->args.in_buf);
    s5p_free_share(mem_fp, jpeg_ctx->args.out_cookie, jpeg_ctx->args.out_buf);
    s5p_vmem_close(mem_fp);
#endif

    close(jpeg_ctx->jpeg_fd);

    if (jpeg_ctx->args.dec_param != NULL)
        free(jpeg_ctx->args.dec_param);

    free(jpeg_ctx);

    return JPEG_OK;
}

int api_jpeg_encode_deinit(int dev_fd)
{
    if (jpeg_ctx->args.mmapped_addr != NULL)
        munmap(jpeg_ctx->args.mmapped_addr, JPEG_TOTAL_BUF_SIZE);

#ifdef S5P_VMEM
    s5p_free_share(mem_fp, jpeg_ctx->args.in_cookie, jpeg_ctx->args.in_buf);
    s5p_free_share(mem_fp, jpeg_ctx->args.out_cookie, jpeg_ctx->args.out_buf);
    s5p_vmem_close(mem_fp);
#endif
    close(jpeg_ctx->jpeg_fd);

    if (jpeg_ctx->args.enc_param != NULL)
        free(jpeg_ctx->args.enc_param);

    free(jpeg_ctx);

    return JPEG_OK;
}

void *api_jpeg_get_decode_in_buf(int dev_fd, unsigned int size)
{
    if (size < 0 || size > MAX_JPEG_RES) {
        ALOGE("Invalid decode input buffer size\r\n");
        return NULL;
    }
#ifdef S5P_VMEM
    jpeg_ctx->args.in_cookie = (unsigned int)ioctl(jpeg_ctx->jpeg_fd,
                                IOCTL_GET_DEC_IN_BUF, size);
    jpeg_ctx->args.in_buf = s5p_malloc_share(mem_fp,
                                        jpeg_ctx->args.in_cookie,
                                        &jpeg_ctx->args.in_buf_size);
#else
    jpeg_ctx->args.in_buf = (char *)ioctl(jpeg_ctx->jpeg_fd,
                                IOCTL_GET_DEC_IN_BUF,
                                jpeg_ctx->args.mmapped_addr);
#endif /* S5P_VMEM */
    return (void *)(jpeg_ctx->args.in_buf);
}

void *api_jpeg_get_encode_in_buf(int dev_fd, unsigned int size)
{
#ifdef S5P_VMEM
    jpeg_ctx->args.in_cookie = (unsigned int)ioctl(jpeg_ctx->jpeg_fd,
                                IOCTL_GET_ENC_IN_BUF, (size*3));
    jpeg_ctx->args.in_buf = s5p_malloc_share(mem_fp,
                                        jpeg_ctx->args.in_cookie,
                                        &jpeg_ctx->args.in_buf_size);
#else
    jpeg_ctx->args.enc_param->size = size;
    jpeg_ctx->args.in_buf = (char *)ioctl(jpeg_ctx->jpeg_fd,
                                IOCTL_GET_ENC_IN_BUF,
                                jpeg_ctx->args.mmapped_addr);
#endif

    ALOGV("api_jpeg_get_encode_in_buf: 0x%x\n",
                        jpeg_ctx->args.in_buf);

    return (void *)(jpeg_ctx->args.in_buf);
}

void *api_jpeg_get_decode_out_buf(int dev_fd)
{
#ifdef S5P_VMEM
    jpeg_ctx->args.out_cookie = (unsigned int)ioctl(jpeg_ctx->jpeg_fd,
                                IOCTL_GET_DEC_OUT_BUF, JPEG_FRAME_BUF_SIZE);
    jpeg_ctx->args.out_buf = s5p_malloc_share(mem_fp,
                                        jpeg_ctx->args.out_cookie,
                                        &jpeg_ctx->args.out_buf_size);
#else
    jpeg_ctx->args.out_buf = (char *)ioctl(jpeg_ctx->jpeg_fd,
                                IOCTL_GET_DEC_OUT_BUF,
                                jpeg_ctx->args.mmapped_addr);
#endif /* S5P_VMEM */
    /*
    ALOGV("api_jpeg_get_decode_out_buf: 0x%x\n",
                        jpeg_ctx->args.out_buf);
    */
    return (void *)(jpeg_ctx->args.out_buf);
}

void *api_jpeg_get_encode_out_buf(int dev_fd)
{
#ifdef S5P_VMEM
    jpeg_ctx->args.out_cookie = (unsigned int)ioctl(jpeg_ctx->jpeg_fd,
                                IOCTL_GET_ENC_OUT_BUF, JPEG_STREAM_BUF_SIZE);
    jpeg_ctx->args.out_buf = s5p_malloc_share(mem_fp,
                                        jpeg_ctx->args.out_cookie,
                                        &jpeg_ctx->args.out_buf_size);
#else
    jpeg_ctx->args.out_buf = (char *)ioctl(jpeg_ctx->jpeg_fd,
                                IOCTL_GET_ENC_OUT_BUF,
                                jpeg_ctx->args.mmapped_addr);
#endif /* S5P_VMEM */

    ALOGV("api_jpeg_get_encode_out_buf: 0x%x\n",
                        jpeg_ctx->args.out_buf);

    return (void *)(jpeg_ctx->args.out_buf);
}

void api_jpeg_set_decode_param(struct jpeg_dec_param *param)
{
    memcpy(jpeg_ctx->args.dec_param, param, sizeof(struct jpeg_dec_param));
    ioctl(jpeg_ctx->jpeg_fd, IOCTL_SET_DEC_PARAM, jpeg_ctx->args.dec_param);
}

void api_jpeg_set_encode_param(struct jpeg_enc_param *param)
{
    memcpy(jpeg_ctx->args.enc_param, param, sizeof(struct jpeg_enc_param));
    ioctl(jpeg_ctx->jpeg_fd, IOCTL_SET_ENC_PARAM, jpeg_ctx->args.enc_param);
}

enum jpeg_ret_type api_jpeg_decode_exe(int dev_fd,
                                    struct jpeg_dec_param *dec_param)
{
    struct jpeg_args *arg;

    arg = &(jpeg_ctx->args);

    ioctl(jpeg_ctx->jpeg_fd, IOCTL_JPEG_DEC_EXE, arg->dec_param);
    ALOGV("api_jpeg_decode_exe dec_param->out_fmt :%d \
                        dec_param->width : %d dec_param->height : %d\n",
                        arg->dec_param->out_fmt,
                        arg->dec_param->width,
                        arg->dec_param->height);
    dec_param->width = arg->dec_param->width;
    dec_param->height = arg->dec_param->height;
    dec_param->size = get_yuv_size(arg->dec_param->out_fmt,
                                arg->dec_param->width,
                                arg->dec_param->height);

    return JPEG_DECODE_OK;
}

enum jpeg_ret_type api_jpeg_encode_exe(int dev_fd,
                                        struct jpeg_enc_param *enc_param)
{
    struct jpeg_args     *arg;
    arg = &(jpeg_ctx->args);

    // check MCU validation width & height & sampling mode
    if (check_input_size(jpeg_ctx->args.enc_param->width,
                                jpeg_ctx->args.enc_param->height) < 0) {
        ALOGV("width/height doesn't match with MCU\r\n");
        return JPEG_FAIL;
    }

    ioctl(jpeg_ctx->jpeg_fd, IOCTL_JPEG_ENC_EXE, arg->enc_param);

    enc_param->size = arg->enc_param->size;

    return JPEG_ENCODE_OK;
}
