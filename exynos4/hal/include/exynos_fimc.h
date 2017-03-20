/*
 *
 * Copyright 2012 Samsung Electronics S.LSI Co. LTD
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

/*!
 * \file      exynos_fimcaler.h
 * \brief     header file for Gscaler HAL
 * \author    ShinWon Lee (shinwon.lee@samsung.com)
 * \date      2012/01/09
 *
 * <b>Revision History: </b>
 * - 2012/01/09 : ShinWon Lee(shinwon.lee@samsung.com) \n
 *   Create
 *
 * - 2012/02/07 : ShinWon Lee(shinwon.lee@samsung.com) \n
 *   Change file name to exynos_fimcaler.h
 *
 * - 2012/02/09 : Sangwoo, Parkk(sw5771.park@samsung.com) \n
 *   Use Multiple Gscaler by Multiple Process
 *
 * - 2012/02/20 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Add exynos_fimc_set_rotation() API
 *
 * - 2012/02/20 : ShinWon Lee(shinwon.lee@samsung.com) \n
 *   Add size constrain
 *
 */

/*!
 * \defgroup exynos_fimcaler
 * \brief API for fimcaler
 * \addtogroup Exynos
 */
#include "Exynos_log.h"

#ifndef EXYNOS_FIMCALER_H_
#define EXYNOS_FIMCALER_H_

#ifdef __cplusplus
extern "C" {
#endif

//#define EXYNOS_FIMC_TRACE 0
#ifdef EXYNOS_FIMC_TRACE
#define EXYNOS_FIMC_LOG_TAG "Exynos_fimcaler"
#define Exynos_fimc_In() Exynos_Log(EXYNOS_DEV_LOG_DEBUG, EXYNOS_FIMC_LOG_TAG, "%s In , Line: %d", __FUNCTION__, __LINE__)
#define Exynos_fimc_Out() Exynos_Log(EXYNOS_DEV_LOG_DEBUG, EXYNOS_FIMC_LOG_TAG, "%s Out , Line: %d", __FUNCTION__, __LINE__)
#else
#define Exynos_fimc_In() ((void *)0)
#define Exynos_fimc_Out() ((void *)0)
#endif

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t fw;
    uint32_t fh;
    uint32_t format;
    uint32_t yaddr;
    uint32_t uaddr;
    uint32_t vaddr;
    uint32_t rot;
    uint32_t cacheable;
    uint32_t drmMode;
    uint32_t narrowRgb;
    int      acquireFenceFd;
    int      releaseFenceFd;
} exynos_fimc_img;

/*
 * Create libfimcaler handle.
 * Gscaler dev_num is dynamically changed.
 *
 * \ingroup exynos_fimcaler
 *
 * \return
 *   libfimcaler handle
 */
void *exynos_fimc_create(
    void);

/*!
 * Create exclusive libfimcaler handle.
 * Other module can't use dev_num of Gscaler.
 *
 * \ingroup exynos_fimcaler
 *
 * \param dev_num
 *   fimcaler dev_num[in]
 * \param fimc_mode
 *It should be set to FIMC_M2M_MODE or FIMC_OUTPUT_MODE.
 *
 *\param out_mode
 *It should be set to FIMC_OUT_FIMD or FIMC_OUT_TV.
 *
 * \return
 *   libfimcaler handle
 */
void *exynos_fimc_create_exclusive(
    int dev_num,
    int fimc_mode,
    int out_mode,
    int allow_drm);

/*!
 * Destroy libfimcaler handle
 *
 * \ingroup exynos_fimcaler
 *
 * \param handle
 *   libfimcaler handle[in]
 */
void exynos_fimc_destroy(
    void *handle);

/*!
 * Set source format.
 *
 * \ingroup exynos_fimcaler
 *
 * \param handle
 *   libfimcaler handle[in]
 *
 * \param width
 *   image width[in]
 *
 * \param height
 *   image height[in]
 *
 * \param crop_left
 *   image left crop size[in]
 *
 * \param crop_top
 *   image top crop size[in]
 *
 * \param crop_width
 *   cropped image width[in]
 *
 * \param crop_height
 *   cropped image height[in]
 *
 * \param v4l2_colorformat
 *   color format[in]
 *
 * \param cacheable
 *   ccacheable[in]
 *
 * \param mode_drm
 *   mode_drm[in]
 *
 * \return
 *   error code
 */
int exynos_fimc_set_src_format(
    void        *handle,
    unsigned int width,
    unsigned int height,
    unsigned int crop_left,
    unsigned int crop_top,
    unsigned int crop_width,
    unsigned int crop_height,
    unsigned int v4l2_colorformat,
    unsigned int cacheable,
    unsigned int mode_drm);

/*!
 * Set destination format.
 *
 * \ingroup exynos_fimcaler
 *
 * \param handle
 *   libfimcaler handle[in]
 *
 * \param width
 *   image width[in]
 *
 * \param height
 *   image height[in]
 *
 * \param crop_left
 *   image left crop size[in]
 *
 * \param crop_top
 *   image top crop size[in]
 *
 * \param crop_width
 *   cropped image width[in]
 *
 * \param crop_height
 *   cropped image height[in]
 *
 * \param v4l2_colorformat
 *   color format[in]
 *
 * \param cacheable
 *   ccacheable[in]
 *
 * \param mode_drm
 *   mode_drm[in]
 *
 * \param narrowRgb
 *   narrow RGB range[in]
 *
 * \return
 *   error code
 */
int exynos_fimc_set_dst_format(
    void        *handle,
    unsigned int width,
    unsigned int height,
    unsigned int crop_left,
    unsigned int crop_top,
    unsigned int crop_width,
    unsigned int crop_height,
    unsigned int v4l2_colorformat,
    unsigned int cacheable,
    unsigned int mode_drm,
    unsigned int narrowRgb);

/*!
 * Set rotation.
 *
 * \ingroup exynos_fimcaler
 *
 * \param handle
 *   libfimcaler handle[in]
 *
 * \param rotation
 *   image rotation. It should be multiple of 90[in]
 *
 * \param flip_horizontal
 *   image flip_horizontal[in]
 *
 * \param flip_vertical
 *   image flip_vertical[in]
 *
 * \return
 *   error code
 */
int exynos_fimc_set_rotation(
    void *handle,
    int   rotation,
    int   flip_horizontal,
    int   flip_vertical);

/*!
 * Set source buffer
 *
 * \ingroup exynos_fimcaler
 *
 * \param handle
 *   libfimcaler handle[in]
 *
 * \param addr
 *   buffer pointer array[in]
 *
 * \param acquireFenceFd
 *   acquire fence fd for the buffer or -1[in]
 *
 * \return
 *   error code
 */
int exynos_fimc_set_src_addr(
    void *handle,
    void *addr[3],
    int acquireFenceFd);

/*!
 * Set destination buffer
 *
 * \param handle
 *   libfimcaler handle[in]
 *
 * \param addr
 *   buffer pointer array[in]
 *
 * \param acquireFenceFd
 *   acquire fence fd for the buffer or -1[in]
 *
 * \return
 *   error code
 */
int exynos_fimc_set_dst_addr(
    void *handle,
    void *addr[3],
    int acquireFenceFd);

/*!
 * Convert color space with presetup color format
 *
 * \ingroup exynos_fimcaler
 *
 * \param handle
 *   libfimcaler handle[in]
 *
 * \return
 *   error code
 */
int exynos_fimc_convert(
    void *handle);

/*!
 * api for local path fimcaler. Not yet support.
 *
 * \ingroup exynos_fimcaler
 */
int exynos_fimc_connect(
    void *handle,
    void *hw);

/*!
 * api for local path fimcaler. Not yet support.
 *
 * \ingroup exynos_fimcaler
 */
int exynos_fimc_disconnect(
    void *handle,
    void *hw);

/*!
 * api for reserving a specific fimcaler.
 * This API could be used from any module that
 *wants to control the fimcalar privately. By calling this function any
 *module can let the libfimcaler know that FIMC is used privately.
 *
 * \ingroup exynos_fimc_reserve
 */
 void *exynos_fimc_reserve
    (int dev_num);


/*!
 * api for releasing the fimcaler that was reserved with
 *exynos_fimc_reserve.
 * \ingroup exynos_fimc_reserve
 */
void exynos_fimc_release
    (void *handle);


/*
*api for setting the FIMC config.
It configures the FIMC for given config
*/
int exynos_fimc_config_exclusive(
    void *handle,
    exynos_fimc_img *src_img,
    exynos_fimc_img *dst_img);

/*
*api for FIMC-OUT run.
It queues the srcBuf to FIMC and deques a buf from driver.
It should be called after configuring the FIMC.
*/
int exynos_fimc_run_exclusive(
    void *handle,
    exynos_fimc_img *src_img,
    exynos_fimc_img *dst_img);

/*
 * Blocks until the current frame is done processing.
 */
int exynos_fimc_wait_frame_done_exclusive
(void *handle);

/*
*api for FIMC stop.
It stops the FIMC OUT streaming.
*/
int exynos_fimc_stop_exclusive
(void *handle);

enum {
    FIMC_M2M_MODE = 0,
    FIMC_OUTPUT_MODE,
    FIMC_CAPTURE_MODE,
    FIMC_RESERVED_MODE,
};

/*flag info */
enum {
    FIMC_DUMMY = 0,
    FIMC_OUT_FIMD,
    FIMC_OUT_TV,
    FIMC_RESERVED,
};

enum {
    FIMC_DONE_CNG_CFG = 0,
    FIMC_NEED_CNG_CFG,
};

#ifdef __cplusplus
}
#endif

#endif /*EXYNOS_FIMCALER_H_*/
