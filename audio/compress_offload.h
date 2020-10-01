/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef COMPRESS_OFFLOAD_H
#define COMPRESS_OFFLOAD_H

void stop_compressed_output_l(struct stream_out *out);

int create_offload_callback_thread(struct stream_out *out);

int destroy_offload_callback_thread(struct stream_out *out);

int parse_compress_metadata(struct stream_out *out, struct str_parms *parms);

int stop_output_offload_stream(struct stream_out *out, bool *disable);

int out_set_offload_parameters(struct audio_device *adev, struct audio_usecase *uc_info);

ssize_t out_write_offload(struct audio_stream_out *stream, const void *buffer,
                         size_t bytes);

int out_get_render_offload_position(struct stream_out *out,
                                   uint32_t *dsp_frames);

int out_get_presentation_offload_position(struct stream_out *out, uint64_t *frames,
                                   struct timespec *timestamp);

int out_pause_offload(struct stream_out *out);

int out_resume_offload(struct stream_out *out);

int out_drain_offload(struct stream_out *out, audio_drain_type_t type);

int out_flush_offload(struct stream_out *out);

int out_set_offload_volume(float left, float right);

#endif // COMPRESS_OFFLOAD_H
