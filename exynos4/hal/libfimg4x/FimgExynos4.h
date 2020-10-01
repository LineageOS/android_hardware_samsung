/*
**
** Copyright 2008, The Android Open Source Project
** Copyright 2009 Samsung Electronics Co, Ltd. All Rights Reserved.
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

#ifndef FIMG_EXYNOS4_H
#define FIMG_EXYNOS4_H

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

#include <utils/threads.h>
#include <utils/StopWatch.h>

#include "FimgApi.h"

#include "sec_g2d_4x.h"

namespace android
{

#define NUMBER_FIMG_LIST           (1)
#define GET_RECT_SIZE(rect)        ((rect->full_w) * (rect->h) * (rect->bytes_per_pixel))
#define GET_REAL_SIZE(rect)        ((rect->full_w) * (rect->h) * (rect->bytes_per_pixel))
#define GET_START_ADDR(rect)       (rect->virt_addr + ((rect->y * rect->full_w) * rect->bytes_per_pixel))
#define SLEEP_TIME                 (3000000) // 3 sec

//---------------------------------------------------------------------------//
// class FimgV4x : public FimgBase
//---------------------------------------------------------------------------//
class FimgV4x : public FimgApi
{
private :
    int             m_g2dFd;

    unsigned char  *m_g2dVirtAddr;
    unsigned int    m_g2dSize;
    unsigned char  *m_g2dSrcVirtAddr;
    unsigned int    m_g2dSrcSize;
    unsigned char  *m_g2dDstVirtAddr;
    unsigned int    m_g2dDstSize;
    struct pollfd   m_g2dPoll;

    Mutex          *m_lock;

    static Mutex    m_instanceLock;
    static unsigned m_curFimgV4xIndex;
    static int      m_numOfInstance;

    static FimgApi *m_ptrFimgApiList[NUMBER_FIMG_LIST];

protected :
    FimgV4x();
    virtual ~FimgV4x();

public:
    static FimgApi *CreateInstance();
    static void     DestroyInstance(FimgApi *ptrFimgApi);
    static void     DestroyAllInstance(void);

protected:
    virtual bool    t_Create(void);
    virtual bool    t_Destroy(void);
    virtual bool    t_Stretch(struct fimg2d_blit *cmd);
    virtual bool    t_Sync(void);
    virtual bool    t_Lock(void);
    virtual bool    t_UnLock(void);

private:
    bool            m_CreateG2D(void);
    bool            m_DestroyG2D(void);

    bool            m_DoG2D(struct fimg2d_blit *cmd);

    inline bool     m_PollG2D(struct pollfd *events);

    inline int      m_ColorFormatFimgApi2FimgHw(int colorFormat);
};

class FimgApiAutoFreeThread;

static sp<FimgApiAutoFreeThread> fimgApiAutoFreeThread = 0;

class FimgApiAutoFreeThread : public Thread
{
private:
    bool mOneMoreSleep;
    bool mDestroyed;

public:
    FimgApiAutoFreeThread(void):
                Thread(false),
                mOneMoreSleep(true),
                mDestroyed(false)
                { }
    ~FimgApiAutoFreeThread(void)
    {
        if (mDestroyed == false)
        {
            FimgV4x::DestroyAllInstance();
            mDestroyed = true;
        }
    }

    virtual void onFirstRef()
    {
        run("FimgApiAutoFreeThread", PRIORITY_BACKGROUND);
    }

    virtual bool threadLoop()
    {

        if (mOneMoreSleep == true)
        {
            mOneMoreSleep = false;
            usleep(SLEEP_TIME);

            return true;
        }
        else
        {
            if (mDestroyed == false)
            {
                FimgV4x::DestroyAllInstance();
                mDestroyed = true;
            }

            fimgApiAutoFreeThread = 0;

            return false;
        }
    }

    void SetOneMoreSleep(void)
    {
        mOneMoreSleep = true;
    }
};

}; // namespace android

#endif // FIMG_EXYNOS4_H
