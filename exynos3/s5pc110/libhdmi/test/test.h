/*
 * Copyright (C) 2012 Havlena Petr, <havlenapetr@gmail.com>
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

#ifndef _HDMI_TEST_H_
#define _HDMI_TEST_H_

#if LOG_TYPE == 1
#include <stdio.h>

#define LOGI(fmt, ...)                  \
    do {                                \
        printf(LOG_TAG"/I: "fmt"\n", __VA_ARGS__); \
    } while (0)

#define LOGE(fmt, ...)                  \
    do {                                \
        printf(LOG_TAG"/E: "fmt"\n", __VA_ARGS__); \
    } while (0)

#elif LOG_TYPE == 2
#include <utils/Log.h>

#define LOGI(fmt, ...)                  \
    do {                                \
        ALOGI(fmt, __VA_ARGS__);        \
    } while (0)

#define LOGE(fmt, ...)                  \
    do {                                \
        ALOGE(fmt, __VA_ARGS__);        \
    } while (0)

#endif

#endif // end of _HDMI_TEST_H_
