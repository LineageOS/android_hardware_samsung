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

#define LOG_TAG "audio_hw_primary"
/*#define LOG_NDEBUG 0*/
/*#define VERY_VERY_VERBOSE_LOGGING*/
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/prctl.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/sched_policy.h>

#include <system/thread_defs.h>

#include <samsung_audio.h>

#include "audio_hw.h"
#include "sound/compress_params.h"

#define MIXER_CTL_COMPRESS_PLAYBACK_VOLUME "Compress Playback Volume"


/* Prototypes */
void lock_input_stream(struct stream_in *in);
void lock_output_stream(struct stream_out *out);
int disable_snd_device(struct audio_device *adev,
                              struct audio_usecase *uc_info,
                              snd_device_t snd_device,
                              bool update_mixer);
int enable_output_path_l(struct stream_out *out);
int disable_output_path_l(struct stream_out *out);

/* must be called with out->lock locked */
static int send_offload_cmd_l(struct stream_out* out, int command)
{
    struct offload_cmd *cmd = (struct offload_cmd *)calloc(1, sizeof(struct offload_cmd));

    ALOGVV("%s %d", __func__, command);

    cmd->cmd = command;
    list_add_tail(&out->offload_cmd_list, &cmd->node);
    pthread_cond_signal(&out->offload_cond);
    return 0;
}

/* must be called iwth out->lock locked */
void stop_compressed_output_l(struct stream_out *out)
{
    out->send_new_metadata = 1;
    if (out->compr != NULL) {
        compress_stop(out->compr);
        while (out->offload_thread_blocked) {
            pthread_cond_wait(&out->cond, &out->lock);
        }
    }
}

static void *offload_thread_loop(void *context)
{
    struct stream_out *out = (struct stream_out *) context;
    struct listnode *item;

    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);
    set_sched_policy(0, SP_FOREGROUND);
    prctl(PR_SET_NAME, (unsigned long)"Offload Callback", 0, 0, 0);

    ALOGV("%s", __func__);
    lock_output_stream(out);
    for (;;) {
        struct offload_cmd *cmd = NULL;
        stream_callback_event_t event;
        bool send_callback = false;

        ALOGVV("%s offload_cmd_list %d out->offload_state %d",
              __func__, list_empty(&out->offload_cmd_list),
              out->offload_state);
        if (list_empty(&out->offload_cmd_list)) {
            ALOGV("%s SLEEPING", __func__);
            pthread_cond_wait(&out->offload_cond, &out->lock);
            ALOGV("%s RUNNING", __func__);
            continue;
        }

        item = list_head(&out->offload_cmd_list);
        cmd = node_to_item(item, struct offload_cmd, node);
        list_remove(item);

        ALOGVV("%s STATE %d CMD %d out->compr %p",
               __func__, out->offload_state, cmd->cmd, out->compr);

        if (cmd->cmd == OFFLOAD_CMD_EXIT) {
            free(cmd);
            break;
        }

        if (out->compr == NULL) {
            ALOGE("%s: Compress handle is NULL", __func__);
            pthread_cond_signal(&out->cond);
            continue;
        }
        out->offload_thread_blocked = true;
        pthread_mutex_unlock(&out->lock);
        send_callback = false;
        switch(cmd->cmd) {
        case OFFLOAD_CMD_WAIT_FOR_BUFFER:
            compress_wait(out->compr, -1);
            send_callback = true;
            event = STREAM_CBK_EVENT_WRITE_READY;
            break;
        case OFFLOAD_CMD_PARTIAL_DRAIN:
            compress_next_track(out->compr);
            compress_partial_drain(out->compr);
            send_callback = true;
            event = STREAM_CBK_EVENT_DRAIN_READY;
            break;
        case OFFLOAD_CMD_DRAIN:
            compress_drain(out->compr);
            send_callback = true;
            event = STREAM_CBK_EVENT_DRAIN_READY;
            break;
        default:
            ALOGE("%s unknown command received: %d", __func__, cmd->cmd);
            break;
        }
        lock_output_stream(out);
        out->offload_thread_blocked = false;
        pthread_cond_signal(&out->cond);
        if (send_callback) {
            out->offload_callback(event, NULL, out->offload_cookie);
        }
        free(cmd);
    }

    pthread_cond_signal(&out->cond);
    while (!list_empty(&out->offload_cmd_list)) {
        item = list_head(&out->offload_cmd_list);
        list_remove(item);
        free(node_to_item(item, struct offload_cmd, node));
    }
    pthread_mutex_unlock(&out->lock);

    return NULL;
}

int create_offload_callback_thread(struct stream_out *out)
{
    pthread_cond_init(&out->offload_cond, (const pthread_condattr_t *) NULL);
    list_init(&out->offload_cmd_list);
    pthread_create(&out->offload_thread, (const pthread_attr_t *) NULL,
                    offload_thread_loop, out);
    return 0;
}

int destroy_offload_callback_thread(struct stream_out *out)
{
    lock_output_stream(out);
    send_offload_cmd_l(out, OFFLOAD_CMD_EXIT);

    pthread_mutex_unlock(&out->lock);
    pthread_join(out->offload_thread, (void **) NULL);
    pthread_cond_destroy(&out->offload_cond);

    return 0;
}

int parse_compress_metadata(struct stream_out *out, struct str_parms *parms)
{
    int ret = 0;
    char value[32];
    struct compr_gapless_mdata tmp_mdata;

    if (!out || !parms) {
        return -EINVAL;
    }

    ret = str_parms_get_str(parms, AUDIO_OFFLOAD_CODEC_DELAY_SAMPLES, value, sizeof(value));
    if (ret >= 0) {
        tmp_mdata.encoder_delay = atoi(value); /* what is a good limit check? */
    } else {
        return -EINVAL;
    }

    ret = str_parms_get_str(parms, AUDIO_OFFLOAD_CODEC_PADDING_SAMPLES, value, sizeof(value));
    if (ret >= 0) {
        tmp_mdata.encoder_padding = atoi(value);
    } else {
        return -EINVAL;
    }

    out->gapless_mdata = tmp_mdata;
    out->send_new_metadata = 1;
    ALOGV("%s new encoder delay %u and padding %u", __func__,
          out->gapless_mdata.encoder_delay, out->gapless_mdata.encoder_padding);

    return 0;
}

int stop_output_offload_stream(struct stream_out *out, bool *disable)
{
    int ret = 0;
    struct audio_device *adev = out->dev;

    if (adev->offload_fx_stop_output != NULL) {
        adev->offload_fx_stop_output(out->handle);

        if (out->offload_state == OFFLOAD_STATE_PAUSED ||
                out->offload_state == OFFLOAD_STATE_PAUSED_FLUSHED)
            *disable = false;
        out->offload_state = OFFLOAD_STATE_IDLE;
    }

    return ret;
}

int out_set_offload_parameters(struct audio_device *adev, struct audio_usecase *uc_info)
{
    int ret = 0;

    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
                __func__, USECASE_AUDIO_PLAYBACK);
        ret = -1;
    }
    if (uc_info != NULL && uc_info->out_snd_device == SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES) {
        ALOGV("Out_set_param: spk+headset enabled\n");
        uc_info->out_snd_device = SND_DEVICE_OUT_HEADPHONES;
        disable_snd_device(adev, uc_info, SND_DEVICE_OUT_SPEAKER, true);
    }

    return ret;
}

ssize_t out_write_offload(struct audio_stream_out *stream, const void *buffer,
                         size_t bytes)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    ssize_t ret = 0;

    ALOGVV("%s: writing buffer (%d bytes) to compress device", __func__, bytes);

    if (out->offload_state == OFFLOAD_STATE_PAUSED_FLUSHED) {
        ALOGV("start offload write from pause state");
        pthread_mutex_lock(&adev->lock);
        ret = enable_output_path_l(out);
        pthread_mutex_unlock(&adev->lock);
        if (ret != 0) {
            return ret;
        }
    }

    if (out->send_new_metadata) {
        ALOGVV("send new gapless metadata");
        compress_set_gapless_metadata(out->compr, &out->gapless_mdata);
        out->send_new_metadata = 0;
    }

    ret = compress_write(out->compr, buffer, bytes);
    ALOGVV("%s: writing buffer (%d bytes) to compress device returned %d", __func__, bytes, ret);
    if (ret >= 0 && ret < (ssize_t)bytes) {
        send_offload_cmd_l(out, OFFLOAD_CMD_WAIT_FOR_BUFFER);
    }
    if (out->offload_state != OFFLOAD_STATE_PLAYING) {
        compress_start(out->compr);
        out->offload_state = OFFLOAD_STATE_PLAYING;
    }
    pthread_mutex_unlock(&out->lock);

    return ret;
}

int out_get_render_offload_position(struct stream_out *out,
                                   uint32_t *dsp_frames)
{
    *dsp_frames = 0;
    if (dsp_frames != NULL) {
        lock_output_stream(out);
        if (out->compr != NULL) {
            compress_get_tstamp(out->compr, (unsigned long *)dsp_frames,
                    &out->sample_rate);
            ALOGVV("%s rendered frames %d sample_rate %d",
                   __func__, *dsp_frames, out->sample_rate);
        }
        pthread_mutex_unlock(&out->lock);
        return 0;
    } else
        return -EINVAL;
}

int out_get_presentation_offload_position(struct stream_out *out, uint64_t *frames,
                                   struct timespec *timestamp)
{
    int ret = -1;
    unsigned long dsp_frames;

    if (out->compr != NULL) {
        compress_get_tstamp(out->compr, &dsp_frames,
                &out->sample_rate);
        ALOGVV("%s rendered frames %ld sample_rate %d",
                __func__, dsp_frames, out->sample_rate);
        *frames = dsp_frames;
        ret = 0;
        /* this is the best we can do */
        clock_gettime(CLOCK_MONOTONIC, timestamp);
    }

    return ret;
}

int out_pause_offload(struct stream_out *out)
{
    int status = -ENOSYS;

    lock_output_stream(out);
    if (out->compr != NULL && out->offload_state == OFFLOAD_STATE_PLAYING) {
        status = compress_pause(out->compr);
        out->offload_state = OFFLOAD_STATE_PAUSED;
        pthread_mutex_lock(&out->dev->lock);
        status = disable_output_path_l(out);
        pthread_mutex_unlock(&out->dev->lock);
    }
    pthread_mutex_unlock(&out->lock);

    return status;
}

int out_resume_offload(struct stream_out *out)
{
    int status = -ENOSYS;

    status = 0;
    lock_output_stream(out);
    if (out->compr != NULL && out->offload_state == OFFLOAD_STATE_PAUSED) {
        pthread_mutex_lock(&out->dev->lock);
        enable_output_path_l(out);
        pthread_mutex_unlock(&out->dev->lock);
        status = compress_resume(out->compr);
        out->offload_state = OFFLOAD_STATE_PLAYING;
    }
    pthread_mutex_unlock(&out->lock);

    return status;
}

int out_drain_offload(struct stream_out *out, audio_drain_type_t type)
{
    int status = -ENOSYS;

    lock_output_stream(out);
    if (type == AUDIO_DRAIN_EARLY_NOTIFY)
        status = send_offload_cmd_l(out, OFFLOAD_CMD_PARTIAL_DRAIN);
    else
        status = send_offload_cmd_l(out, OFFLOAD_CMD_DRAIN);
    pthread_mutex_unlock(&out->lock);

    return status;
}

int out_flush_offload(struct stream_out *out)
{
    lock_output_stream(out);
    if (out->offload_state == OFFLOAD_STATE_PLAYING) {
        ALOGE("out_flush() called in wrong state %d", out->offload_state);
        pthread_mutex_unlock(&out->lock);
        return -ENOSYS;
    }
    if (out->offload_state == OFFLOAD_STATE_PAUSED) {
        stop_compressed_output_l(out);
        out->offload_state = OFFLOAD_STATE_PAUSED_FLUSHED;
    }
    pthread_mutex_unlock(&out->lock);
    return 0;
}

int out_set_offload_volume(float left, float right)
{
    int offload_volume[2];//For stereo
    struct mixer_ctl *ctl;
    struct mixer *mixer = NULL;

    offload_volume[0] = (int)(left * COMPRESS_PLAYBACK_VOLUME_MAX);
    offload_volume[1] = (int)(right * COMPRESS_PLAYBACK_VOLUME_MAX);

    mixer = mixer_open(MIXER_CARD);
    if (!mixer) {
        ALOGE("%s unable to open the mixer for card %d, aborting.",
                __func__, MIXER_CARD);
        return -EINVAL;
    }
    ctl = mixer_get_ctl_by_name(mixer, MIXER_CTL_COMPRESS_PLAYBACK_VOLUME);
    if (!ctl) {
        ALOGE("%s: Could not get ctl for mixer cmd - %s",
                __func__, MIXER_CTL_COMPRESS_PLAYBACK_VOLUME);
        mixer_close(mixer);
        return -EINVAL;
    }
    ALOGV("out_set_volume set offload volume (%f, %f)", left, right);
    mixer_ctl_set_array(ctl, offload_volume,
                        sizeof(offload_volume)/sizeof(offload_volume[0]));
    mixer_close(mixer);
    return 0;
}
