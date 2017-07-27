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

#ifndef FIMG_C210_H
#define FIMG_C210_H


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
//#include "FimgMem.h"

#include "sec_g2d.h"

//-----------------------------------------------------------------//

namespace android
{

//#define CHECK_FIMGC210_PERFORMANCE
//#define CHECK_FIMGC210_CRITICAL_PERFORMANCE
#define NUMBER_FIMG_LIST           (1)  // kcoolsw : because of pmem
//#define G2D_NONE_BLOCKING_MODE        // Not supported yet. because of sysMMU Page fault
#define GET_RECT_SIZE(rect)        ((rect->full_w) * (rect->h) * (rect->bytes_per_pixel))
#define GET_REAL_SIZE(rect)        ((rect->full_w) * (rect->h) * (rect->bytes_per_pixel))
#define GET_START_ADDR(rect)        (rect->virt_addr + ((rect->y * rect->full_w) * rect->bytes_per_pixel))


//---------------------------------------------------------------------------//
// class FimgC210 : public FimgBase
//---------------------------------------------------------------------------//
class FimgC210 : public FimgApi
{
private :
    int              m_g2dFd;

    unsigned char *  m_g2dVirtAddr;
    unsigned int     m_g2dSize;
    unsigned char *  m_g2dSrcVirtAddr;
    unsigned int     m_g2dSrcSize;
    unsigned char *  m_g2dDstVirtAddr;
    unsigned int     m_g2dDstSize;
    struct pollfd    m_g2dPoll;

    Mutex *          m_lock;

    static Mutex     m_instanceLock;
    static int       m_curFimgC210Index;
    static int       m_numOfInstance;

    static FimgApi * m_ptrFimgApiList[NUMBER_FIMG_LIST];


protected :
    FimgC210();
    virtual ~FimgC210();

public:
    static FimgApi * CreateInstance();
    static void      DestroyInstance(FimgApi * ptrFimgApi);
    static void      DestroyAllInstance(void);

protected:
    virtual bool     t_Create(void);
    virtual bool     t_Destroy(void);
    virtual bool     t_Stretch(FimgRect * src, FimgRect * dst, FimgClip * clip, FimgFlag * flag);
    virtual bool     t_Sync(void);
    virtual bool     t_Lock(void);
    virtual bool     t_UnLock(void);

private:
    bool             m_CreateG2D(void);
    bool             m_DestroyG2D(void);
    bool             SetClipRectl(FimgRect * dst, FimgClip * clip, FimgClip * clipTempMidRect);

    bool             m_DoG2D(FimgRect * src, FimgRect * dst, FimgClip * clip, FimgFlag * flag);

    inline bool      m_PollG2D(struct pollfd * events);

    inline bool      m_CleanG2D  (unsigned int addr, unsigned int size);
    inline bool      m_FlushG2D  (unsigned int addr, unsigned int size);

    inline int       m_ColorFormatFimgApi2FimgHw(int colorFormat);
    inline int       m_RotateValueFimgApi2FimgHw(int rotateValue);

    #ifdef CHECK_FIMGC210_PERFORMANCE
    void             m_PrintFimgC210Performance(FimgRect *   src,
                                                FimgRect *   dst,
                                                int          stopWatchIndex,
                                                const char * stopWatchName[],
                                                nsecs_t      stopWatchTime[]);
    #endif // CHECK_FIMGC210_PERFORMANCE
};

//---------------------------------------------------------------------------//
// class FimgApiAutoFreeThread : public Thread
//---------------------------------------------------------------------------//
class FimgApiAutoFreeThread;

static sp<FimgApiAutoFreeThread> fimgApiAutoFreeThread = 0;

class FimgApiAutoFreeThread : public Thread
{
    private:
        bool      mOneMoreSleep;
        bool      mDestroyed;

    public:
        FimgApiAutoFreeThread(void):
                    //Thread(true),
                    Thread(false),
                    mOneMoreSleep(true),
                    mDestroyed(false)
                    { }
        ~FimgApiAutoFreeThread(void)
        {
            if(mDestroyed == false)
            {
                FimgC210::DestroyAllInstance();
                mDestroyed = true;
            }
        }

        virtual void onFirstRef()
        {
            run("FimgApiAutoFreeThread", PRIORITY_BACKGROUND);
        }

        virtual bool threadLoop()
        {
            //#define SLEEP_TIME (10000000) // 10 sec
            #define SLEEP_TIME (3000000) // 3 sec
            //#define SLEEP_TIME (1000000) // 1 sec

            if(mOneMoreSleep == true)
            {
                mOneMoreSleep = false;
                usleep(SLEEP_TIME);

                return true;
            }
            else
            {
                if(mDestroyed == false)
                {
                    FimgC210::DestroyAllInstance();
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

#endif // FIMG_C210_H
