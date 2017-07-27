/*
 * Copyright (C) 2009 The Android Open Source Project
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

/*
 * @file    csc_fimc.cpp
 *
 * @brief   csc_fimc use fimc1 to color space convertion
 *
 * @author  ShinWon Lee (shinwon.lee@samsung.com)
 *
 * @version 1.0
 *
 * @history
 *   2011.11.01 : Create
 */

#include <utils/Log.h>
#include <errno.h>
#include <dlfcn.h>

#include "SEC_OMX_Def.h"
#include "csc_fimc.h"
#include "HardwareConverter.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * create and open fimc handle
 *
 * @return
 *   fimc handle
 */
void *csc_fimc_open()
{
    HardwareConverter *hw_converter = NULL;

    hw_converter = new HardwareConverter;
    if (hw_converter->bHWconvert_flag == 0) {
        delete hw_converter;
        hw_converter = NULL;
        ALOGE("%s LINE = %d HardwareConverter failed", __func__, __LINE__);
    }

    return (void *)hw_converter;
}

/*
 * close and destroy fimc handle
 *
 * @param handle
 *   fimc handle[in]
 *
 * @return
 *   pass or fail
 */
CSC_FIMC_ERROR_CODE csc_fimc_close(void *handle)
{
    HardwareConverter *hw_converter = (HardwareConverter *)handle;

    if (hw_converter != NULL)
        delete hw_converter;

    return CSC_FIMC_RET_OK;
}

/*
 * convert color space nv12t to omxformat
 *
 * @param handle
 *   fimc handle[in]
 *
 * @param dst_addr
 *   y,u,v address of dst_addr[out]
 *
 * @param src_addr
 *   y,uv address of src_addr.Format is nv12t[in]
 *
 * @param width
 *   width of dst image[in]
 *
 * @param height
 *   height of dst image[in]
 *
 * @param omxformat
 *   omxformat of dst image[in]
 *
 * @return
 *   pass or fail
 */
CSC_FIMC_ERROR_CODE csc_fimc_convert_nv12t(
    void *handle,
    void **dst_addr,
    void **src_addr,
    unsigned int width,
    unsigned int height,
    OMX_COLOR_FORMATTYPE omxformat)
{
    CSC_FIMC_ERROR_CODE ret = CSC_FIMC_RET_OK;
    HardwareConverter *hw_converter = (HardwareConverter *)handle;

    if (hw_converter == NULL) {
        ret = CSC_FIMC_RET_FAIL;
        goto EXIT;
    }

    hw_converter->convert(
            (void *)src_addr, (void *)dst_addr,
            (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12TPhysicalAddress,
            width, height, omxformat);

    ret = CSC_FIMC_RET_OK;

EXIT:

    return ret;
}

#ifdef __cplusplus
}
#endif
