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

#ifdef __LP64__
#define TFA_AMPLIFIER_LIBRARY_PATH "/system/lib64/libtfa98xx.so"
#else
#define TFA_AMPLIFIER_LIBRARY_PATH "/system/lib/libtfa98xx.so"
#endif

typedef struct {
    unsigned long a1[24];
    unsigned char a2[4];
    unsigned long a3[6];
    unsigned char a4[4];
    unsigned long a5[100]; // 100 is arbitrary here
} tfa_handle_t;

typedef struct {
    void*                   lib_handle;
    tfa_handle_t*           tfa_handle;
    pthread_mutex_t         tfa_lock;
    int                     (*tfa_device_open)(tfa_handle_t*, int);
    int                     (*tfa_enable)(tfa_handle_t*, int);
} amplifier_device_t;

int load_amplifier_lib();
int speaker_amp_device_open();
int speaker_amp_enable(int something);
int speaker_amp_disable();

#endif // AMPLIFIER_H
