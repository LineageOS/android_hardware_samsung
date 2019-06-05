/*
 * Copyright (C) 2010 Samsung Electronics Co.,LTD., for the ODROID code
 * Copyright (C) 2012 The Android Open Source Project, for the samsung_slsi/exynos5 code
 * Copyright (C) 2013 Paul Kocialkowski, for the V4L2 code
 * Copyright (C) 2017 The NamelessRom Project, for making it work
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

#ifndef ANDROID_HWCOMPOSER_WINDOW_H
#define ANDROID_HWCOMPOSER_WINDOW_H

int window_clear(struct hwc_context_t *ctx);

int window_buffer_allocate(struct hwc_context_t *ctx, struct hwc_win_info_t *win);
int window_buffer_deallocate(struct hwc_context_t *ctx, struct hwc_win_info_t *win);

void config_handle(struct hwc_context_t* ctx, const hwc_layer_1_t &layer, s3c_fb_win_config &cfg);

void dump_fb_win_cfg(struct s3c_fb_win_config_data &cfg);

#endif //ANDROID_HWCOMPOSER_WINDOW_H
