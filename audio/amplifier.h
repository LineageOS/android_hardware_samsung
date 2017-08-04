/*
 * Copyright (C) 2017 Christopher N. Hesse <raymanfx@gmail.com>
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

#ifndef AMPLIFIER_H
#define AMPLIFIER_H

#include <pthread.h>
#include <stdbool.h>

#include "audio_hw.h"

#define TFA_AMPLIFIER_LIBRARY_PATH "/system/lib/libtfa98xx.so"


/*
 * Amplifier audio modes for different usecases.
 */
typedef enum {
    Audio_Mode_None = -1,
    Audio_Mode_Music_Normal,
    Audio_Mode_Hfp_Client,
    Audio_Mode_Voice,
    Audio_Mode_Hs_Hfp,
    Audio_Mode_Max
} amplifier_mode_t;

#if 0
/*
 * Reverse engineered struct data from stock...
 */

typedef struct {
    int a1;
    int a2;
    int a3;
} inner_handle_t;

typedef struct {
    int a1;
    int a2;
    union {
        void *a3;
        unsigned long long pad1[2];
    };
    union {
        void *a4;
        unsigned long long pad2;
        short pad3;
    };
    union {
        void *a5;
        unsigned long long pad4[7];
        short pad5;
    };
    unsigned char a6[4];
    unsigned long long a7;
    unsigned long long a8;
    unsigned long long a9;
    unsigned char a10[4];
    short a11;
    int a12;
    inner_handle_t inner_handle;
    int a13;
    int a14;
} tfa_handle_t;
#endif

/*
 * It doesn't really matter what this is, apparently we just need a continuous
 * chunk of memory...
 */
typedef struct {
    int a1;
    int a2[200];
} tfa_handle_t;

/*
 * Vendor functions that we dlsym.
 */
typedef int (*tfa_device_open_t)(tfa_handle_t*, int);
typedef int (*tfa_enable_t)(tfa_handle_t*, int);

/*
 * Amplifier device abstraction.
 *
 * adev:             A reference to the corresponding audio HAL device
 * lib_handle:       The prebuilt vendor blob, loaded into memory
 * tfa_handle:       Misc data we need to pass to the vendor function calls
 * tfa_lock:         A mutex guarding amplifier enable/disable operations
 * tfa_device_open:  Vendor function for initializing the amplifier
 * tfa_enable:       Vendor function for enabling/disabling the amplifier
 * ref_cnt:          Keeps track of client streams which depend on the amplifier
 * route_cnt:        Keeps track of ALSA routes which depend on the amplifier
 * update_ref_cnt:
 */
typedef struct {
    struct audio_device *adev;
    void *lib_handle;
    tfa_handle_t* tfa_handle;
    pthread_mutex_t tfa_lock;
    tfa_device_open_t tfa_device_open;
    tfa_enable_t tfa_enable;
    int ref_cnt[Audio_Mode_Max];
    int route_cnt[Audio_Mode_Max];
    bool update_ref_cnt;
} amplifier_device_t;

/*
 * Public API for audio_hw.c to hook into.
 *
 * For documentation, look into amplifier.c
 */
int speaker_amp_enable();
int speaker_amp_disable(snd_device_t snd_device);
void speaker_amp_update();
int speaker_amp_device_open(struct audio_device *adev);
void speaker_amp_device_close();

#endif // AMPLIFIER_H
