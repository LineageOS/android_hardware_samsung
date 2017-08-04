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

#define TFA_HANDLE_SIZE 50

typedef struct {
    void*                   lib_handle;
    int*                    tfa_handle;
    pthread_mutex_t         tfa_lock;
    int                     (*tfa_device_open)(int*, int);
    int                     (*tfa_enable)(int*, int);
} amplifier_device_t;

int load_amplifier_lib();
int speaker_amp_device_open();

#endif // AMPLIFIER_H
