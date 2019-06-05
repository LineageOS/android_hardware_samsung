/*
**
** Copyright 2009 Samsung Electronics Co, Ltd.
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

#define LOG_NDEBUG 0
#define LOG_TAG "SKIA"
#include <utils/Log.h>

#include "FimgApi.h"

pthread_mutex_t fimg2d_mutex = PTHREAD_MUTEX_INITIALIZER;

struct blit_op_table optbl[] = {
    { (int)BLIT_OP_SOLID_FILL, "FILL" },
    { (int)BLIT_OP_CLR, "CLR" },
    { (int)BLIT_OP_SRC, "SRC" },
    { (int)BLIT_OP_DST, "DST" },
    { (int)BLIT_OP_SRC_OVER, "SRC_OVER" },
    { (int)BLIT_OP_DST_OVER, "DST_OVER" },
    { (int)BLIT_OP_SRC_IN, "SRC_IN" },
    { (int)BLIT_OP_DST_IN, "DST_IN" },
    { (int)BLIT_OP_SRC_OUT, "SRC_OUT" },
    { (int)BLIT_OP_DST_OUT, "DST_OUT" },
    { (int)BLIT_OP_SRC_ATOP, "SRC_ATOP" },
    { (int)BLIT_OP_DST_ATOP, "DST_ATOP" },
    { (int)BLIT_OP_XOR, "XOR" },
    { (int)BLIT_OP_ADD, "ADD" },
    { (int)BLIT_OP_MULTIPLY, "MULTIPLY" },
    { (int)BLIT_OP_SCREEN, "SCREEN" },
    { (int)BLIT_OP_DARKEN, "DARKEN" },
    { (int)BLIT_OP_LIGHTEN, "LIGHTEN" },
    { (int)BLIT_OP_DISJ_SRC_OVER, "DISJ_SRC_OVER" },
    { (int)BLIT_OP_DISJ_DST_OVER, "DISJ_DST_OVER" },
    { (int)BLIT_OP_DISJ_SRC_IN, "DISJ_SRC_IN" },
    { (int)BLIT_OP_DISJ_DST_IN, "DISJ_DST_IN" },
    { (int)BLIT_OP_DISJ_SRC_OUT, "DISJ_SRC_OUT" },
    { (int)BLIT_OP_DISJ_DST_OUT, "DISJ_DST_OUT" },
    { (int)BLIT_OP_DISJ_SRC_ATOP, "DISJ_SRC_ATOP" },
    { (int)BLIT_OP_DISJ_DST_ATOP, "DISJ_DST_ATOP" },
    { (int)BLIT_OP_DISJ_XOR, "DISJ_XOR" },
    { (int)BLIT_OP_CONJ_SRC_OVER, "CONJ_SRC_OVER" },
    { (int)BLIT_OP_CONJ_DST_OVER, "CONJ_DST_OVER" },
    { (int)BLIT_OP_CONJ_SRC_IN, "CONJ_SRC_IN" },
    { (int)BLIT_OP_CONJ_DST_IN, "CONJ_DST_IN" },
    { (int)BLIT_OP_CONJ_SRC_OUT, "CONJ_SRC_OUT" },
    { (int)BLIT_OP_CONJ_DST_OUT, "CONJ_DST_OUT" },
    { (int)BLIT_OP_CONJ_SRC_ATOP, "CONJ_SRC_ATOP" },
    { (int)BLIT_OP_CONJ_DST_ATOP, "CONJ_DST_ATOP" },
    { (int)BLIT_OP_CONJ_XOR, "CONJ_XOR" },
    { (int)BLIT_OP_USER_COEFF, "USER_COEFF" },
    { (int)BLIT_OP_END, "" },
};

#ifndef REAL_DEBUG
    void VOID_FUNC(const char *format, ...)
    {}
#endif

FimgApi::FimgApi()
{
    m_flagCreate = false;
}

FimgApi::~FimgApi()
{
    if (m_flagCreate == true)
        PRINT("%s::this is not Destroyed fail\n", __func__);
}

bool FimgApi::Create(void)
{
    bool ret = false;

    if (t_Lock() == false) {
        PRINT("%s::t_Lock() fail\n", __func__);
        goto CREATE_DONE;
    }

    if (m_flagCreate == true) {
        PRINT("%s::Already Created fail\n", __func__);
        goto CREATE_DONE;
    }

    if (t_Create() == false) {
        PRINT("%s::t_Create() fail\n", __func__);
        goto CREATE_DONE;
    }

    m_flagCreate = true;

    ret = true;

CREATE_DONE :

    t_UnLock();

    return ret;
}

bool FimgApi::Destroy(void)
{
    bool ret = false;

    if (t_Lock() == false) {
        PRINT("%s::t_Lock() fail\n", __func__);
        goto DESTROY_DONE;
    }

    if (m_flagCreate == false) {
        PRINT("%s::Already Destroyed fail\n", __func__);
        goto DESTROY_DONE;
    }

    if (t_Destroy() == false) {
        PRINT("%s::t_Destroy() fail\n", __func__);
        goto DESTROY_DONE;
    }

    m_flagCreate = false;

    ret = true;

DESTROY_DONE :

    t_UnLock();

    return ret;
}

bool FimgApi::Stretch(struct fimg2d_blit *cmd)
{
    bool ret = false;

    if (t_Lock() == false) {
        PRINT("%s::t_Lock() fail\n", __func__);
        goto STRETCH_DONE;
    }

    if (m_flagCreate == false) {
        PRINT("%s::This is not Created fail\n", __func__);
        goto STRETCH_DONE;
    }

    if (t_Stretch(cmd) == false) {
        goto STRETCH_DONE;
    }

    ret = true;

STRETCH_DONE :

    t_UnLock();

    return ret;
}

bool FimgApi::Sync(void)
{
    bool ret = false;

    if (m_flagCreate == false) {
        PRINT("%s::This is not Created fail\n", __func__);
        goto SYNC_DONE;
    }

    if (t_Sync() == false)
        goto SYNC_DONE;

    ret = true;

SYNC_DONE :

    return ret;
}

bool FimgApi::t_Create(void)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}

bool FimgApi::t_Destroy(void)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}

bool FimgApi::t_Stretch(struct fimg2d_blit *cmd)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}

bool FimgApi::t_Sync(void)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}

bool FimgApi::t_Lock(void)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}

bool FimgApi::t_UnLock(void)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}

//---------------------------------------------------------------------------//
// extern function
//---------------------------------------------------------------------------//
extern "C" int stretchFimgApi(struct fimg2d_blit *cmd)
{
    FimgApi * fimgApi = NULL;

    pthread_mutex_lock(&fimg2d_mutex);

    fimgApi = createFimgApi();

    if (fimgApi == NULL) {
        PRINT("%s::createFimgApi() fail\n", __func__);
        goto ERROR;
    }

    if (fimgApi->Stretch(cmd) == false) {
        if (fimgApi != NULL)
            destroyFimgApi(fimgApi);

        goto ERROR;
    }

    if (fimgApi != NULL)
        destroyFimgApi(fimgApi);

    pthread_mutex_unlock(&fimg2d_mutex);
    return 0;

ERROR:
    pthread_mutex_unlock(&fimg2d_mutex);
    return -1;
}

extern "C" int SyncFimgApi(void)
{
    FimgApi * fimgApi = NULL;

    pthread_mutex_lock(&fimg2d_mutex);

    fimgApi = createFimgApi();
    if (fimgApi == NULL) {
        PRINT("%s::createFimgApi() fail\n", __func__);
        goto ERROR;
    }

    if (fimgApi->Sync() == false) {
        if (fimgApi != NULL)
            destroyFimgApi(fimgApi);

        goto ERROR;
    }

    if (fimgApi != NULL)
        destroyFimgApi(fimgApi);

    pthread_mutex_unlock(&fimg2d_mutex);
    return 0;

ERROR:
    pthread_mutex_unlock(&fimg2d_mutex);
    return -1;
}

int comp_value[5][2] = {{511,582},{576,668},{768,924},{1152,1436},{1536,1948}};

extern "C" int FimgApiCheckBoostup(Fimg *curr, Fimg *prev)
{
    int i = 0;

    for (i = 0; i < 5; i++) {
        if ((curr->srcW == comp_value[i][0]) && (curr->srcH == comp_value[i][1]))
            break;
    }

    if (i == 5)
        return 0;

    if (curr->alpha != 255)
        return 0;

    if (curr->srcAddr != prev->srcAddr)
        return 0;

    if (curr->srcX != prev->srcX)
        return 0;

    if (curr->srcY != prev->srcY)
        return 0;

    if (curr->srcW != prev->srcW)
        return 0;

    if (curr->srcH != prev->srcH )
        return 0;

    if (curr->srcFWStride != prev->srcFWStride)
        return 0;

    if (curr->srcFH != prev->srcFH)
        return 0;

    if (curr->srcColorFormat != prev->srcColorFormat)
        return 0;

    if (curr->dstAddr != prev->dstAddr)
        return 0;

    if (curr->dstX != prev->dstX)
        return 0;

    if (curr->dstY != prev->dstY)
        return 0;

    if (curr->dstW != prev->dstW)
        return 0;

    if (curr->dstH != prev->dstH)
        return 0;

    if (curr->dstFWStride != prev->dstFWStride)
        return 0;

    if (curr->dstFH != prev->dstFH)
        return 0;

    if (curr->dstColorFormat != prev->dstColorFormat)
        return 0;

    if (curr->clipT != prev->clipT)
        return 0;

    if (curr->clipB != prev->clipB)
        return 0;

    if (curr->clipL != prev->clipL)
        return 0;

    if (curr->clipR != prev->clipR)
        return 0;

    if (curr->fillcolor != prev->fillcolor)
        return 0;

    if (curr->rotate != prev->rotate)
        return 0;

    if (prev->alpha != 255)
        return 0;

    if (curr->xfermode != prev->xfermode)
        return 0;

    if (curr->isDither != prev->isDither)
        return 0;

    if (curr->isFilter != prev->isFilter)
        return 0;

    if (curr->colorFilter != prev->colorFilter)
        return 0;

    if (curr->matrixType != prev->matrixType)
        return 0;

    if (curr->matrixSx != prev->matrixSx)
        return 0;

    if (curr->matrixSy != prev->matrixSy)
        return 0;

    PRINT("%s return 1", __func__);

    return 1;
}

void printDataBlit(char *title, struct fimg2d_blit *cmd)
{
    SLOGI("%s\n", title);

    SLOGI("    sequence_no. = %u\n", cmd->seq_no);
    SLOGI("    blit_op      = %d(%s)\n", cmd->op, optbl[cmd->op].str);
    SLOGI("    fill_color   = %X\n", cmd->param.solid_color);
    SLOGI("    global_alpha = %u\n", (unsigned int)cmd->param.g_alpha);
    SLOGI("    PREMULT      = %s\n", cmd->param.premult == PREMULTIPLIED ? "PREMULTIPLIED" : "NON-PREMULTIPLIED");
    SLOGI("    do_dither    = %s\n", cmd->param.dither == true ? "dither" : "no-dither");

    printDataBlitRotate(cmd->param.rotate);

    printDataBlitScale(&cmd->param.scaling);

    printDataBlitImage("SRC", cmd->src);
    printDataBlitImage("DST", cmd->dst);
    printDataBlitImage("MSK", cmd->msk);

    printDataBlitRect("SRC", &cmd->src->rect);
    printDataBlitRect("DST", &cmd->dst->rect);
    printDataBlitRect("MSK", &cmd->msk->rect);
}

void printDataBlitImage(char *title, struct fimg2d_image *image)
{
    if (NULL != image) {
    SLOGI("    Image_%s\n", title);
    SLOGI("        addr = %X\n", image->addr.start);
    SLOGI("        format = %d\n", image->fmt);
    } else
        SLOGI("    Image_%s : NULL\n", title);
}

void printDataBlitRect(char *title, struct fimg2d_rect *rect)
{
    if (NULL != rect) {
        SLOGI("    RECT_%s\n", title);
        SLOGI("        (x1, y1) = (%d, %d)\n", rect->x1, rect->y1);
        SLOGI("        (x2, y2) = (%d, %d)\n", rect->x2, rect->y2);
        SLOGI("        (width, height) = (%d, %d)\n", rect->x2 - rect->x1, rect->y2 - rect->y1);
    } else
        SLOGI("    RECT_%s : NULL\n", title);
}

void printDataBlitRotate(int rotate)
{
    SLOGI("    ROTATE : %d\n", rotate);
}

void printDataBlitScale(struct fimg2d_scale *scaling)
{
    SLOGI("    SCALING\n");
    SLOGI("        scale_mode : %s\n", scaling->mode == 0 ?
                                      "NO_SCALING" :
                          (scaling->mode == 1 ? "SCALING_NEAREST" : "SCALING_BILINEAR"));
    SLOGI("        src : (src_w, src_h) = (%d, %d)\n", scaling->src_w, scaling->src_h);
    SLOGI("        dst : (dst_w, dst_h) = (%d, %d)\n", scaling->dst_w, scaling->dst_h);
    SLOGI("        scaling_factor : (scale_w, scale_y) = (%3.2f, %3.2f)\n", (double)scaling->dst_w / scaling->src_w, (double)scaling->dst_h / scaling->src_h);
}
