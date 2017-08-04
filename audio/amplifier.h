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

#define TFA_AMPLIFIER_LIBRARY_PATH "/system/lib/libtfa98xx.so"

/* Forward declaration */
struct audio_device;


typedef enum {
    AMPLIFIER_ABSENT = -1,
    AMPLIFIER_OFFLINE = 0,
    AMPLIFIER_ONLINE
} amplifier_state_t;

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

typedef int (*tfa_device_open_t)(tfa_handle_t*, int);
typedef int (*tfa_enable_t)(tfa_handle_t*, int);

typedef struct {
    void *lib_handle;
    tfa_handle_t* tfa_handle;
    pthread_mutex_t tfa_lock;
    tfa_device_open_t tfa_device_open;
    tfa_enable_t tfa_enable;
} amplifier_device_t;

int speaker_amp_enable(amplifier_device_t *amp_dev);
int speaker_amp_disable(amplifier_device_t *amp_dev);
int speaker_amp_device_open(struct audio_device *adev);
int speaker_amp_device_close(struct audio_device *adev);

#endif // AMPLIFIER_H
