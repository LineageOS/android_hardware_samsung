/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (C) 2017 Christopher N. Hesse <raymanfx@gmail.com>
 * Copyright (C) 2017 Andreas Schneider <asn@cryptomilk.org>
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
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>
#include <dlfcn.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/atomic.h>
#include <cutils/sched_policy.h>
#include <cutils/properties.h>

#include <samsung_audio.h>

#include <hardware/audio_effect.h>
#include <system/thread_defs.h>
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_ns.h>
#include "audio_hw.h"
#include "compress_offload.h"
#include "voice.h"

#include "sound/compress_params.h"


/* TODO: the following PCM device profiles could be read from a config file */
static struct pcm_device_profile pcm_device_playback = {
    .config = {
        .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
        .rate = PLAYBACK_DEFAULT_SAMPLING_RATE,
        .period_size = PLAYBACK_PERIOD_SIZE,
        .period_count = PLAYBACK_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = PLAYBACK_START_THRESHOLD(PLAYBACK_PERIOD_SIZE, PLAYBACK_PERIOD_COUNT),
        .stop_threshold = PLAYBACK_STOP_THRESHOLD(PLAYBACK_PERIOD_SIZE, PLAYBACK_PERIOD_COUNT),
        .silence_threshold = 0,
        .silence_size = UINT_MAX,
        .avail_min = PLAYBACK_AVAILABLE_MIN,
    },
    .card = SOUND_CARD,
    .id = SOUND_PLAYBACK_DEVICE,
    .type = PCM_PLAYBACK,
    .devices = AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE|
               AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_EARPIECE,
};

static struct pcm_device_profile pcm_device_deep_buffer = {
    .config = {
        .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
        .rate = DEEP_BUFFER_OUTPUT_SAMPLING_RATE,
        .period_size = DEEP_BUFFER_OUTPUT_PERIOD_SIZE,
        .period_count = DEEP_BUFFER_OUTPUT_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = DEEP_BUFFER_OUTPUT_PERIOD_SIZE / 4,
        .stop_threshold = INT_MAX,
        .avail_min = DEEP_BUFFER_OUTPUT_PERIOD_SIZE / 4,
    },
    .card = SOUND_CARD,
    .id = SOUND_DEEP_BUFFER_DEVICE,
    .type = PCM_PLAYBACK,
    .devices = AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE|
               AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_EARPIECE,
};

static struct pcm_device_profile pcm_device_capture = {
    .config = {
        .channels = CAPTURE_DEFAULT_CHANNEL_COUNT,
        .rate = CAPTURE_DEFAULT_SAMPLING_RATE,
        .period_size = CAPTURE_PERIOD_SIZE,
        .period_count = CAPTURE_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = CAPTURE_START_THRESHOLD,
        .stop_threshold = 0,
        .silence_threshold = 0,
        .avail_min = 0,
    },
    .card = SOUND_CARD,
    .id = SOUND_CAPTURE_DEVICE,
    .type = PCM_CAPTURE,
    .devices = AUDIO_DEVICE_IN_BUILTIN_MIC|AUDIO_DEVICE_IN_WIRED_HEADSET|AUDIO_DEVICE_IN_BACK_MIC,
};

static struct pcm_device_profile pcm_device_capture_low_latency = {
    .config = {
        .channels = CAPTURE_DEFAULT_CHANNEL_COUNT,
        .rate = CAPTURE_DEFAULT_SAMPLING_RATE,
        .period_size = CAPTURE_PERIOD_SIZE_LOW_LATENCY,
        .period_count = CAPTURE_PERIOD_COUNT_LOW_LATENCY,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = CAPTURE_START_THRESHOLD,
        .stop_threshold = 0,
        .silence_threshold = 0,
        .avail_min = 0,
    },
    .card = SOUND_CARD,
    .id = SOUND_CAPTURE_DEVICE,
    .type = PCM_CAPTURE_LOW_LATENCY,
    .devices = AUDIO_DEVICE_IN_BUILTIN_MIC|AUDIO_DEVICE_IN_WIRED_HEADSET|AUDIO_DEVICE_IN_BACK_MIC,
};

#ifdef SOUND_CAPTURE_LOOPBACK_AEC_DEVICE
static struct pcm_device_profile pcm_device_capture_loopback_aec = {
    .config = {
        .channels = CAPTURE_DEFAULT_CHANNEL_COUNT,
        .rate = CAPTURE_DEFAULT_SAMPLING_RATE,
        .period_size = CAPTURE_PERIOD_SIZE,
        .period_count = CAPTURE_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = CAPTURE_START_THRESHOLD,
        .stop_threshold = 0,
        .silence_threshold = 0,
        .avail_min = 0,
    },
    .card = SOUND_CARD,
    .id = SOUND_CAPTURE_LOOPBACK_AEC_DEVICE,
    .type = PCM_CAPTURE,
    .devices = SND_DEVICE_IN_LOOPBACK_AEC,
};
#endif

static struct pcm_device_profile pcm_device_playback_sco = {
    .config = {
        .channels = SCO_DEFAULT_CHANNEL_COUNT,
        .rate = SCO_DEFAULT_SAMPLING_RATE,
        .period_size = SCO_PERIOD_SIZE,
        .period_count = SCO_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = SCO_START_THRESHOLD,
        .stop_threshold = SCO_STOP_THRESHOLD,
        .silence_threshold = 0,
        .avail_min = SCO_AVAILABLE_MIN,
    },
    .card = SOUND_CARD,
    .id = SOUND_PLAYBACK_SCO_DEVICE,
    .type = PCM_PLAYBACK,
    .devices =
            AUDIO_DEVICE_OUT_BLUETOOTH_SCO|AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET|
            AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT,
};

static struct pcm_device_profile pcm_device_capture_sco = {
    .config = {
        .channels = SCO_DEFAULT_CHANNEL_COUNT,
        .rate = SCO_DEFAULT_SAMPLING_RATE,
        .period_size = SCO_PERIOD_SIZE,
        .period_count = SCO_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = CAPTURE_START_THRESHOLD,
        .stop_threshold = 0,
        .silence_threshold = 0,
        .avail_min = 0,
    },
    .card = SOUND_CARD,
    .id = SOUND_CAPTURE_SCO_DEVICE,
    .type = PCM_CAPTURE,
    .devices = AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
};

#ifdef SOUND_CAPTURE_HOTWORD_DEVICE
static struct pcm_device_profile pcm_device_hotword_streaming = {
    .config = {
        .channels = 1,
        .rate = 16000,
        .period_size = CAPTURE_PERIOD_SIZE,
        .period_count = CAPTURE_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = CAPTURE_START_THRESHOLD,
        .stop_threshold = 0,
        .silence_threshold = 0,
        .avail_min = 0,
    },
    .card = SOUND_CARD,
    .id = SOUND_CAPTURE_HOTWORD_DEVICE,
    .type = PCM_HOTWORD_STREAMING,
    .devices = AUDIO_DEVICE_IN_BUILTIN_MIC|AUDIO_DEVICE_IN_WIRED_HEADSET|AUDIO_DEVICE_IN_BACK_MIC
};
#endif

static struct pcm_device_profile * const pcm_devices[] = {
    &pcm_device_playback,
    &pcm_device_capture,
    &pcm_device_capture_low_latency,
    &pcm_device_playback_sco,
    &pcm_device_capture_sco,
#ifdef SOUND_CAPTURE_LOOPBACK_AEC_DEVICE
    &pcm_device_capture_loopback_aec,
#endif
#ifdef SOUND_CAPTURE_HOTWORD_DEVICE
    &pcm_device_hotword_streaming,
#endif
    NULL,
};

static const char * const use_case_table[AUDIO_USECASE_MAX] = {
    [USECASE_AUDIO_PLAYBACK] = "playback",
    [USECASE_AUDIO_PLAYBACK_MULTI_CH] = "playback multi-channel",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD] = "compress-offload-playback",
    [USECASE_AUDIO_PLAYBACK_DEEP_BUFFER] = "playback deep-buffer",
    [USECASE_AUDIO_CAPTURE] = "capture",
    [USECASE_AUDIO_CAPTURE_HOTWORD] = "capture-hotword",
    [USECASE_VOICE_CALL] = "voice-call",
};

#define STRING_TO_ENUM(string) { #string, string }

static unsigned int audio_device_ref_count;

struct string_to_enum {
    const char *name;
    uint32_t value;
};

static const struct string_to_enum out_channels_name_to_enum_table[] = {
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_STEREO),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_5POINT1),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_7POINT1),
};

static struct audio_device *adev = NULL;

static amplifier_device_t * get_amplifier_device(void)
{
    if (adev)
        return adev->amp;

    return NULL;
}

static int amplifier_open(void)
{
    int rc;
    amplifier_module_t *module;

    rc = hw_get_module(AMPLIFIER_HARDWARE_MODULE_ID,
            (const hw_module_t **) &module);
    if (rc) {
        ALOGV("%s: Failed to obtain reference to amplifier module: %s\n",
                __func__, strerror(-rc));
        return -ENODEV;
    }

    rc = amplifier_device_open((const hw_module_t *) module, &adev->amp);
    if (rc) {
        ALOGV("%s: Failed to open amplifier hardware device: %s\n",
                __func__, strerror(-rc));
        return -ENODEV;
    }

    return 0;
}

static int amplifier_set_input_devices(uint32_t devices)
{
    amplifier_device_t *amp = get_amplifier_device();
    if (amp && amp->set_input_devices)
        return amp->set_input_devices(amp, devices);

    return 0;
}

static int amplifier_set_output_devices(uint32_t devices)
{
    amplifier_device_t *amp = get_amplifier_device();
    if (amp && amp->set_output_devices)
        return amp->set_output_devices(amp, devices);

    return 0;
}

static int amplifier_enable_devices(uint32_t devices, bool enable)
{
    amplifier_device_t *amp = get_amplifier_device();
    bool is_output = devices > SND_DEVICE_OUT_BEGIN &&
        devices < SND_DEVICE_OUT_END;

    if (amp && amp->enable_output_devices && is_output)
        return amp->enable_output_devices(amp, devices, enable);

    if (amp && amp->enable_input_devices && !is_output)
        return amp->enable_input_devices(amp, devices, enable);

    return 0;
}

static int amplifier_set_mode(audio_mode_t mode)
{
    amplifier_device_t *amp = get_amplifier_device();
    if (amp && amp->set_mode)
        return amp->set_mode(amp, mode);

    return 0;
}

static int amplifier_output_stream_start(struct audio_stream_out *stream,
        bool offload)
{
    amplifier_device_t *amp = get_amplifier_device();
    if (amp && amp->output_stream_start)
        return amp->output_stream_start(amp, stream, offload);

    return 0;
}

static int amplifier_input_stream_start(struct audio_stream_in *stream)
{
    amplifier_device_t *amp = get_amplifier_device();
    if (amp && amp->input_stream_start)
        return amp->input_stream_start(amp, stream);

    return 0;
}

static int amplifier_output_stream_standby(struct audio_stream_out *stream)
{
    amplifier_device_t *amp = get_amplifier_device();
    if (amp && amp->output_stream_standby)
        return amp->output_stream_standby(amp, stream);

    return 0;
}

static int amplifier_input_stream_standby(struct audio_stream_in *stream)
{
    amplifier_device_t *amp = get_amplifier_device();
    if (amp && amp->input_stream_standby)
        return amp->input_stream_standby(amp, stream);

    return 0;
}

static int amplifier_set_parameters(struct str_parms *parms)
{
    amplifier_device_t *amp = get_amplifier_device();
    if (amp && amp->set_parameters)
        return amp->set_parameters(amp, parms);

    return 0;
}

static int amplifier_close(void)
{
    amplifier_device_t *amp = get_amplifier_device();
    if (amp)
        amplifier_device_close(amp);

    return 0;
}

struct timespec time_spec_diff(struct timespec time1, struct timespec time0) {
    struct timespec ret;
    int xsec = 0;

    if (time0.tv_nsec > time1.tv_nsec) {
        xsec = (int) ((time0.tv_nsec - time1.tv_nsec) / (1E9 + 1));
        time0.tv_nsec -= (long int) (1E9 * xsec);
        time0.tv_sec += xsec;
    }

    if ((time1.tv_nsec - time0.tv_nsec) > 1E9) {
        xsec = (int) ((time1.tv_nsec - time0.tv_nsec) / 1E9);
        time0.tv_nsec += (long int) (1E9 * xsec);
        time0.tv_sec -= xsec;
    }

    ret.tv_sec = labs(time1.tv_sec - time0.tv_sec);
    ret.tv_nsec = labs(time1.tv_nsec - time0.tv_nsec);

    return ret;
}

static bool is_supported_format(audio_format_t format)
{
    if (format == AUDIO_FORMAT_MP3 ||
            ((format & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_AAC))
        return true;

    return false;
}

static int get_snd_codec_id(audio_format_t format)
{
    int id = 0;

    switch (format & AUDIO_FORMAT_MAIN_MASK) {
    case AUDIO_FORMAT_MP3:
        id = SND_AUDIOCODEC_MP3;
        break;
    case AUDIO_FORMAT_AAC:
        id = SND_AUDIOCODEC_AAC;
        break;
    default:
        ALOGE("%s: Unsupported audio format", __func__);
    }

    return id;
}

/* Array to store sound devices */
static const char * const device_table[SND_DEVICE_MAX] = {
    [SND_DEVICE_NONE] = "none",
    /* Playback sound devices */
    [SND_DEVICE_OUT_EARPIECE] = "earpiece",
    [SND_DEVICE_OUT_SPEAKER] = "speaker",
    [SND_DEVICE_OUT_HEADPHONES] = "headphones",
    [SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES] = "speaker-and-headphones",
    [SND_DEVICE_OUT_VOICE_EARPIECE] = "voice-earpiece",
    [SND_DEVICE_OUT_VOICE_EARPIECE_WB] = "voice-earpiece-wb",
    [SND_DEVICE_OUT_VOICE_SPEAKER] = "voice-speaker",
    [SND_DEVICE_OUT_VOICE_SPEAKER_WB] = "voice-speaker-wb",
    [SND_DEVICE_OUT_VOICE_HEADPHONES] = "voice-headphones",
    [SND_DEVICE_OUT_VOICE_HEADPHONES_WB] = "voice-headphones-wb",
    [SND_DEVICE_OUT_VOICE_BT_SCO] = "voice-bt-sco-headset",
    [SND_DEVICE_OUT_VOICE_BT_SCO_WB] = "voice-bt-sco-headset-wb",
    [SND_DEVICE_OUT_HDMI] = "hdmi",
    [SND_DEVICE_OUT_SPEAKER_AND_HDMI] = "speaker-and-hdmi",
    [SND_DEVICE_OUT_BT_SCO] = "bt-sco-headset",

    /* Capture sound devices */
    [SND_DEVICE_IN_EARPIECE_MIC] = "earpiece-mic",
    [SND_DEVICE_IN_SPEAKER_MIC] = "speaker-mic",
    [SND_DEVICE_IN_HEADSET_MIC] = "headset-mic",
    [SND_DEVICE_IN_EARPIECE_MIC_AEC] = "earpiece-mic",
    [SND_DEVICE_IN_SPEAKER_MIC_AEC] = "voice-speaker-mic",
    [SND_DEVICE_IN_HEADSET_MIC_AEC] = "headset-mic",
    [SND_DEVICE_IN_VOICE_MIC] = "voice-mic",
    [SND_DEVICE_IN_VOICE_EARPIECE_MIC] = "voice-earpiece-mic",
    [SND_DEVICE_IN_VOICE_EARPIECE_MIC_WB] = "voice-earpiece-mic-wb",
    [SND_DEVICE_IN_VOICE_SPEAKER_MIC] = "voice-speaker-mic",
    [SND_DEVICE_IN_VOICE_SPEAKER_MIC_WB] = "voice-speaker-mic-wb",
    [SND_DEVICE_IN_VOICE_HEADSET_MIC] = "voice-headset-mic",
    [SND_DEVICE_IN_VOICE_HEADSET_MIC_WB] = "voice-headset-mic-wb",
    [SND_DEVICE_IN_VOICE_BT_SCO_MIC] = "voice-bt-sco-mic",
    [SND_DEVICE_IN_VOICE_BT_SCO_MIC_WB] = "voice-bt-sco-mic-wb",
    [SND_DEVICE_IN_HDMI_MIC] = "hdmi-mic",
    [SND_DEVICE_IN_BT_SCO_MIC] = "bt-sco-mic",
    [SND_DEVICE_IN_CAMCORDER_MIC] = "camcorder-mic",
    [SND_DEVICE_IN_VOICE_REC_HEADSET_MIC] = "voice-rec-headset-mic",
    [SND_DEVICE_IN_VOICE_REC_MIC] = "voice-rec-mic",
    [SND_DEVICE_IN_LOOPBACK_AEC] = "loopback-aec",
};

static struct mixer_card *adev_get_mixer_for_card(struct audio_device *adev, int card)
{
    struct mixer_card *mixer_card;
    struct listnode *node;

    list_for_each(node, &adev->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, adev_list_node);
        if (mixer_card->card == card)
            return mixer_card;
    }
    return NULL;
}

static struct mixer_card *uc_get_mixer_for_card(struct audio_usecase *usecase, int card)
{
    struct mixer_card *mixer_card;
    struct listnode *node;

    list_for_each(node, &usecase->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, uc_list_node[usecase->id]);
        if (mixer_card->card == card)
            return mixer_card;
    }
    return NULL;
}

static void free_mixer_list(struct audio_device *adev)
{
    struct mixer_card *mixer_card;
    struct listnode *node;
    struct listnode *next;

    list_for_each_safe(node, next, &adev->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, adev_list_node);
        list_remove(node);
        audio_route_free(mixer_card->audio_route);
        free(mixer_card);
    }
}

static int mixer_init(struct audio_device *adev)
{
    int i;
    int card;
    int retry_num;
    struct mixer *mixer;
    struct audio_route *audio_route;
    char mixer_path[PATH_MAX];
    struct mixer_card *mixer_card;
    int ret = 0;

    list_init(&adev->mixer_list);

    for (i = 0; pcm_devices[i] != NULL; i++) {
        card = pcm_devices[i]->card;
        if (adev_get_mixer_for_card(adev, card) == NULL) {
            retry_num = 0;
            do {
                mixer = mixer_open(card);
                if (mixer == NULL) {
                    if (++retry_num > RETRY_NUMBER) {
                        ALOGE("%s unable to open the mixer for--card %d, aborting.",
                              __func__, card);
                        ret = -ENODEV;
                        goto error;
                    }
                    usleep(RETRY_US);
                }
            } while (mixer == NULL);

            sprintf(mixer_path, "/system/etc/mixer_paths_%d.xml", card);
            audio_route = audio_route_init(card, mixer_path);
            if (!audio_route) {
                ALOGE("%s: Failed to init audio route controls for card %d, aborting.",
                      __func__, card);
                ret = -ENODEV;
                goto error;
            }
            mixer_card = calloc(1, sizeof(struct mixer_card));
            if (mixer_card == NULL) {
                ret = -ENOMEM;
                goto error;
            }
            mixer_card->card = card;
            mixer_card->mixer = mixer;
            mixer_card->audio_route = audio_route;

            /* Do not sleep on first enable_snd_device() */
            mixer_card->dsp_poweroff_time.tv_sec = 1;
            mixer_card->dsp_poweroff_time.tv_nsec = 0;

            list_add_tail(&adev->mixer_list, &mixer_card->adev_list_node);
        }
    }

    return 0;

error:
    free_mixer_list(adev);
    return ret;
}

static const char *get_snd_device_name(snd_device_t snd_device)
{
    const char *name = NULL;

    if (snd_device == SND_DEVICE_NONE ||
        (snd_device >= SND_DEVICE_MIN && snd_device < SND_DEVICE_MAX))
        name = device_table[snd_device];

    ALOGE_IF(name == NULL, "%s: invalid snd device %d", __func__, snd_device);

   return name;
}

static const char *get_snd_device_display_name(snd_device_t snd_device)
{
    const char *name = get_snd_device_name(snd_device);

    if (name == NULL)
        name = "SND DEVICE NOT FOUND";

    return name;
}

static struct pcm_device_profile *get_pcm_device(usecase_type_t uc_type, audio_devices_t devices)
{
    int i;

    devices &= ~AUDIO_DEVICE_BIT_IN;
    for (i = 0; pcm_devices[i] != NULL; i++) {
        if ((pcm_devices[i]->type == uc_type) &&
                (devices & pcm_devices[i]->devices))
            break;
    }
    return pcm_devices[i];
}

static struct audio_usecase *get_usecase_from_id(struct audio_device *adev,
                                                   audio_usecase_t uc_id)
{
    struct audio_usecase *usecase;
    struct listnode *node;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->id == uc_id)
            return usecase;
    }
    return NULL;
}

static struct audio_usecase *get_usecase_from_type(struct audio_device *adev,
                                                        usecase_type_t type)
{
    struct audio_usecase *usecase;
    struct listnode *node;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->type & type)
            return usecase;
    }
    return NULL;
}

/* always called with adev lock held */
static int set_voice_volume_l(struct audio_device *adev, float volume)
{
    int err = 0;

    if (adev->mode == AUDIO_MODE_IN_CALL) {
        set_voice_session_volume(adev->voice.session, volume);
    }

    return err;
}


static snd_device_t get_output_snd_device(struct audio_device *adev, audio_devices_t devices)
{

    audio_mode_t mode = adev->mode;
    snd_device_t snd_device = SND_DEVICE_NONE;

    ALOGV("%s: enter: output devices(%#x), mode(%d)", __func__, devices, mode);
    if (devices == AUDIO_DEVICE_NONE ||
        devices & AUDIO_DEVICE_BIT_IN) {
        ALOGV("%s: Invalid output devices (%#x)", __func__, devices);
        goto exit;
    }

    if (mode == AUDIO_MODE_IN_CALL) {
        if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
            devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_OUT_VOICE_HEADPHONES;
        } else if (devices & AUDIO_DEVICE_OUT_ALL_SCO) {
            snd_device = SND_DEVICE_OUT_VOICE_BT_SCO;
        } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_OUT_VOICE_SPEAKER;
        } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
            snd_device = SND_DEVICE_OUT_VOICE_EARPIECE;
        }

        if (voice_session_uses_wideband(adev->voice.session)) {
            if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                snd_device = SND_DEVICE_OUT_VOICE_HEADPHONES_WB;
            } else if (devices & AUDIO_DEVICE_OUT_ALL_SCO) {
                snd_device = SND_DEVICE_OUT_VOICE_BT_SCO_WB;
            } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
                snd_device = SND_DEVICE_OUT_VOICE_SPEAKER_WB;
            } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
                snd_device = SND_DEVICE_OUT_VOICE_EARPIECE_WB;
            }
        }

        if (snd_device != SND_DEVICE_NONE) {
            goto exit;
        }
    }

    if (popcount(devices) == 2) {
        if (devices == (AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
                        AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES;
        } else if (devices == (AUDIO_DEVICE_OUT_WIRED_HEADSET |
                               AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES;
        } else {
            ALOGE("%s: Invalid combo device(%#x)", __func__, devices);
            goto exit;
        }
        if (snd_device != SND_DEVICE_NONE) {
            goto exit;
        }
    }

    if (popcount(devices) != 1) {
        ALOGE("%s: Invalid output devices(%#x)", __func__, devices);
        goto exit;
    }

    if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
        devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
        snd_device = SND_DEVICE_OUT_HEADPHONES;
    } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
        snd_device = SND_DEVICE_OUT_SPEAKER;
    } else if (devices & AUDIO_DEVICE_OUT_ALL_SCO) {
        snd_device = SND_DEVICE_OUT_BT_SCO;
    } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
        snd_device = SND_DEVICE_OUT_EARPIECE;
    } else {
        ALOGE("%s: Unknown device(s) %#x", __func__, devices);
    }
exit:
    ALOGV("%s: exit: snd_device(%s)", __func__, device_table[snd_device]);
    return snd_device;
}

static snd_device_t get_input_snd_device(struct audio_device *adev, audio_devices_t out_device)
{
    audio_source_t  source;
    audio_mode_t    mode   = adev->mode;
    audio_devices_t in_device;
    audio_channel_mask_t channel_mask;
    snd_device_t snd_device = SND_DEVICE_NONE;
    struct stream_in *active_input = NULL;
    struct audio_usecase *usecase;

    usecase = get_usecase_from_type(adev, PCM_CAPTURE|VOICE_CALL);
    if (usecase != NULL) {
        active_input = (struct stream_in *)usecase->stream;
    }
    source = (active_input == NULL) ?
                                AUDIO_SOURCE_DEFAULT : active_input->source;

    in_device = (active_input == NULL) ?
                    AUDIO_DEVICE_NONE :
                    (active_input->devices & ~AUDIO_DEVICE_BIT_IN);
    channel_mask = (active_input == NULL) ?
                                AUDIO_CHANNEL_IN_MONO : active_input->main_channels;

    ALOGV("%s: enter: out_device(%#x) in_device(%#x)",
          __func__, out_device, in_device);
    if (mode == AUDIO_MODE_IN_CALL) {
        if (out_device == AUDIO_DEVICE_NONE) {
            ALOGE("%s: No output device set for voice call", __func__);
            goto exit;
        }

        snd_device = SND_DEVICE_IN_VOICE_MIC;
        if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_VOICE_HEADSET_MIC;
        }

        if (voice_session_uses_twomic(adev->voice.session)) {
            if (out_device & AUDIO_DEVICE_OUT_EARPIECE ||
                out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
                snd_device = SND_DEVICE_IN_VOICE_EARPIECE_MIC;
            } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
                snd_device = SND_DEVICE_IN_VOICE_SPEAKER_MIC;
            }
        }

        if (voice_session_uses_wideband(adev->voice.session)) {
            if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                snd_device = SND_DEVICE_IN_VOICE_HEADSET_MIC_WB;
            }

            if (voice_session_uses_twomic(adev->voice.session)) {
                if (out_device & AUDIO_DEVICE_OUT_EARPIECE ||
                    out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
                    snd_device = SND_DEVICE_IN_VOICE_EARPIECE_MIC_WB;
                } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
                    snd_device = SND_DEVICE_IN_VOICE_SPEAKER_MIC_WB;
                }
            }
        }

        /* BT SCO */
        if (out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
            snd_device = SND_DEVICE_IN_VOICE_MIC;

            if (out_device & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET) {
                if (voice_session_uses_wideband(adev->voice.session)) {
                    snd_device = SND_DEVICE_IN_VOICE_BT_SCO_MIC_WB;
                } else {
                    snd_device = SND_DEVICE_IN_VOICE_BT_SCO_MIC;
                }
            } else if (voice_session_uses_twomic(adev->voice.session)) {
                snd_device = SND_DEVICE_IN_VOICE_EARPIECE_MIC;
            }
        }
    } else if (source == AUDIO_SOURCE_CAMCORDER) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC ||
            in_device & AUDIO_DEVICE_IN_BACK_MIC) {
            snd_device = SND_DEVICE_IN_CAMCORDER_MIC;
        }
    } else if (source == AUDIO_SOURCE_VOICE_RECOGNITION) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
            if (snd_device == SND_DEVICE_NONE) {
                snd_device = SND_DEVICE_IN_VOICE_REC_MIC;
            }
        } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_VOICE_REC_HEADSET_MIC;
        }
    } else if (source == AUDIO_SOURCE_VOICE_COMMUNICATION || source == AUDIO_SOURCE_MIC) {
        if (out_device & AUDIO_DEVICE_OUT_SPEAKER)
            in_device = AUDIO_DEVICE_IN_BACK_MIC;
        if (active_input) {
            if (active_input->enable_aec) {
                if (in_device & AUDIO_DEVICE_IN_BACK_MIC) {
                    snd_device = SND_DEVICE_IN_SPEAKER_MIC_AEC;
                } else if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
                    if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
                        snd_device = SND_DEVICE_IN_SPEAKER_MIC_AEC;
                    } else {
                        snd_device = SND_DEVICE_IN_EARPIECE_MIC_AEC;
                    }
                } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
                    snd_device = SND_DEVICE_IN_HEADSET_MIC_AEC;
                }
            }
            /* TODO: set echo reference */
        }
    } else if (source == AUDIO_SOURCE_DEFAULT) {
        goto exit;
    }


    if (snd_device != SND_DEVICE_NONE) {
        goto exit;
    }

    if (in_device != AUDIO_DEVICE_NONE &&
            !(in_device & AUDIO_DEVICE_IN_VOICE_CALL) &&
            !(in_device & AUDIO_DEVICE_IN_COMMUNICATION)) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_BACK_MIC) {
            snd_device = SND_DEVICE_IN_SPEAKER_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_HEADSET_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
            snd_device = SND_DEVICE_IN_BT_SCO_MIC ;
        } else if (in_device & AUDIO_DEVICE_IN_AUX_DIGITAL) {
            snd_device = SND_DEVICE_IN_HDMI_MIC;
        } else {
            ALOGE("%s: Unknown input device(s) %#x", __func__, in_device);
            ALOGW("%s: Using default earpiece-mic", __func__);
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        }
    } else {
        if (out_device & AUDIO_DEVICE_OUT_EARPIECE) {
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_HEADSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_IN_SPEAKER_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET) {
            snd_device = SND_DEVICE_IN_BT_SCO_MIC;
        } else {
            ALOGE("%s: Unknown output device(s) %#x", __func__, out_device);
            ALOGW("%s: Using default earpiece-mic", __func__);
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        }
    }
exit:
    ALOGV("%s: exit: in_snd_device(%s)", __func__, device_table[snd_device]);
    return snd_device;
}

#if 0
static int set_hdmi_channels(struct audio_device *adev,  int channel_count)
{
    (void)adev;
    (void)channel_count;
    /* TODO */

    return 0;
}

static int edid_get_max_channels(struct audio_device *adev)
{
    int max_channels = 2;
    (void)adev;

    /* TODO */
    return max_channels;
}
#endif

/* Delay in Us */
static int64_t render_latency(audio_usecase_t usecase)
{
    (void)usecase;
    /* TODO */
    return 0;
}

static int enable_snd_device(struct audio_device *adev,
                             struct audio_usecase *uc_info,
                             snd_device_t snd_device,
                             bool update_mixer)
{
    struct mixer_card *mixer_card;
    struct listnode *node;
    const char *snd_device_name = get_snd_device_name(snd_device);
#ifdef DSP_POWEROFF_DELAY
    struct timespec activation_time;
    struct timespec elapsed_time;
#endif /* DSP_POWEROFF_DELAY */

    if (snd_device_name == NULL)
        return -EINVAL;

    if (snd_device == SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES) {
        ALOGV("Request to enable combo device: enable individual devices\n");
        enable_snd_device(adev, uc_info, SND_DEVICE_OUT_SPEAKER, update_mixer);
        enable_snd_device(adev, uc_info, SND_DEVICE_OUT_HEADPHONES, update_mixer);
        return 0;
    }
    adev->snd_dev_ref_cnt[snd_device]++;
    if (adev->snd_dev_ref_cnt[snd_device] > 1) {
        ALOGV("%s: snd_device(%d: %s) is already active",
              __func__, snd_device, snd_device_name);
        return 0;
    }

    ALOGV("%s: snd_device(%d: %s)", __func__,
          snd_device, snd_device_name);

    list_for_each(node, &uc_info->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, uc_list_node[uc_info->id]);

#ifdef DSP_POWEROFF_DELAY
        clock_gettime(CLOCK_MONOTONIC, &activation_time);

        elapsed_time = time_spec_diff(activation_time,
                                      mixer_card->dsp_poweroff_time);
        if (elapsed_time.tv_sec == 0) {
            long elapsed_usec = elapsed_time.tv_nsec / 1000;

            if (elapsed_usec < DSP_POWEROFF_DELAY) {
                usleep(DSP_POWEROFF_DELAY - elapsed_usec);
            }
        }
        update_mixer = true;
#endif /* DSP_POWEROFF_DELAY */

        amplifier_enable_devices(snd_device, true);

        audio_route_apply_path(mixer_card->audio_route, snd_device_name);
        if (update_mixer) {
            audio_route_update_mixer(mixer_card->audio_route);
        }
    }

    return 0;
}

int disable_snd_device(struct audio_device *adev,
                              struct audio_usecase *uc_info,
                              snd_device_t snd_device,
                              bool update_mixer)
{
    struct mixer_card *mixer_card;
    struct listnode *node;
    const char *snd_device_name = get_snd_device_name(snd_device);

    if (snd_device_name == NULL)
        return -EINVAL;

    if (snd_device == SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES) {
        ALOGV("Request to disable combo device: disable individual devices\n");
        disable_snd_device(adev, uc_info, SND_DEVICE_OUT_SPEAKER, update_mixer);
        disable_snd_device(adev, uc_info, SND_DEVICE_OUT_HEADPHONES, update_mixer);
        return 0;
    }

    if (adev->snd_dev_ref_cnt[snd_device] <= 0) {
        ALOGE("%s: device ref cnt is already 0", __func__);
        return -EINVAL;
    }
    adev->snd_dev_ref_cnt[snd_device]--;
    if (adev->snd_dev_ref_cnt[snd_device] == 0) {
        ALOGV("%s: snd_device(%d: %s)", __func__,
              snd_device, snd_device_name);
        list_for_each(node, &uc_info->mixer_list) {
            mixer_card = node_to_item(node, struct mixer_card, uc_list_node[uc_info->id]);
#ifdef DSP_POWEROFF_DELAY
            update_mixer = true;
#endif /* DSP_POWEROFF_DELAY */
            audio_route_reset_path(mixer_card->audio_route, snd_device_name);
            if (update_mixer) {
                audio_route_update_mixer(mixer_card->audio_route);
            }

            amplifier_enable_devices(snd_device, false);
#ifdef DSP_POWEROFF_DELAY
            clock_gettime(CLOCK_MONOTONIC, &(mixer_card->dsp_poweroff_time));
#endif /* DSP_POWEROFF_DELAY */
        }
    }
    return 0;
}

static int select_devices(struct audio_device *adev,
                          audio_usecase_t uc_id)
{
    snd_device_t out_snd_device = SND_DEVICE_NONE;
    snd_device_t in_snd_device = SND_DEVICE_NONE;
    struct audio_usecase *usecase = NULL;
    struct audio_usecase *vc_usecase = NULL;
    struct listnode *node;
    struct stream_in *active_input = NULL;
    struct stream_out *active_out;
    struct mixer_card *mixer_card;

    ALOGV("%s: usecase(%d)", __func__, uc_id);

    if (uc_id == USECASE_AUDIO_CAPTURE_HOTWORD)
        return 0;

    usecase = get_usecase_from_type(adev, PCM_CAPTURE|VOICE_CALL);
    if (usecase != NULL) {
        active_input = (struct stream_in *)usecase->stream;
    }

    usecase = get_usecase_from_id(adev, uc_id);
    if (usecase == NULL) {
        ALOGE("%s: Could not find the usecase(%d)", __func__, uc_id);
        return -EINVAL;
    }
    active_out = (struct stream_out *)usecase->stream;


    /*
     * If the voice call is active, use the sound devices of voice call usecase
     * so that it would not result any device switch. All the usecases will
     * be switched to new device when select_devices() is called for voice call
     * usecase.
     */
    if (usecase->type != VOICE_CALL && adev->voice.in_call) {
        vc_usecase = get_usecase_from_id(adev, USECASE_VOICE_CALL);
        if (vc_usecase == NULL) {
            ALOGE("%s: Could not find the voice call usecase", __func__);
        } else {
            ALOGV("%s: in call, reusing devices (rx: %s, tx: %s)", __func__,
                  get_snd_device_display_name(vc_usecase->out_snd_device),
                  get_snd_device_display_name(vc_usecase->in_snd_device));
            usecase->devices = vc_usecase->devices;
            return 0;
        }
    }

    if (usecase->type == VOICE_CALL) {
        usecase->devices = active_out->devices;
        prepare_voice_session(adev->voice.session, active_out->devices);
        out_snd_device = get_output_snd_device(adev, active_out->devices);
        in_snd_device = get_input_snd_device(adev, active_out->devices);
    } else if (usecase->type == PCM_PLAYBACK) {
        usecase->devices = active_out->devices;
        in_snd_device = SND_DEVICE_NONE;
        if (out_snd_device == SND_DEVICE_NONE) {
            out_snd_device = get_output_snd_device(adev, active_out->devices);
            if (active_out == adev->primary_output &&
                    active_input &&
                    active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION) {
                select_devices(adev, active_input->usecase);
            }
        }
    } else if (usecase->type == PCM_CAPTURE) {
        usecase->devices = ((struct stream_in *)usecase->stream)->devices;
        out_snd_device = SND_DEVICE_NONE;
        if (in_snd_device == SND_DEVICE_NONE) {
            if (active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION &&
                    adev->primary_output && !adev->primary_output->standby) {
                in_snd_device = get_input_snd_device(adev, adev->primary_output->devices);
            } else {
                in_snd_device = get_input_snd_device(adev, AUDIO_DEVICE_NONE);
            }
        }
    }

    if (out_snd_device == usecase->out_snd_device &&
        in_snd_device == usecase->in_snd_device) {
        return 0;
    }

    ALOGV("%s: out_snd_device(%d: %s) in_snd_device(%d: %s)", __func__,
          out_snd_device, get_snd_device_display_name(out_snd_device),
          in_snd_device,  get_snd_device_display_name(in_snd_device));


    /* Disable current sound devices */
    if (usecase->out_snd_device != SND_DEVICE_NONE) {
        disable_snd_device(adev, usecase, usecase->out_snd_device, false);
    }

    if (usecase->in_snd_device != SND_DEVICE_NONE) {
        disable_snd_device(adev, usecase, usecase->in_snd_device, false);
    }

    /* Enable new sound devices */
    if (out_snd_device != SND_DEVICE_NONE) {
        /* We need to update the audio path if we switch the out devices */
        if (adev->voice.in_call) {
            set_voice_session_audio_path(adev->voice.session);
        }

        enable_snd_device(adev, usecase, out_snd_device, false);
    }

    if (in_snd_device != SND_DEVICE_NONE) {
        enable_snd_device(adev, usecase, in_snd_device, false);
    }

    list_for_each(node, &usecase->mixer_list) {
         mixer_card = node_to_item(node, struct mixer_card, uc_list_node[usecase->id]);
         audio_route_update_mixer(mixer_card->audio_route);
    }

    usecase->in_snd_device = in_snd_device;
    usecase->out_snd_device = out_snd_device;

    /* Rely on amplifier_set_devices to distinguish between in/out devices */
    amplifier_set_input_devices(in_snd_device);
    amplifier_set_output_devices(out_snd_device);

    return 0;
}


static ssize_t read_frames(struct stream_in *in, void *buffer, ssize_t frames);
static int do_in_standby_l(struct stream_in *in);

#ifdef PREPROCESSING_ENABLED
static void get_capture_reference_delay(struct stream_in *in,
                              size_t frames __unused,
                              struct echo_reference_buffer *buffer)
{
    ALOGVV("%s: enter:)", __func__);

    /* read frames available in kernel driver buffer */
    unsigned int kernel_frames;
    struct timespec tstamp;
    long buf_delay;
    long kernel_delay;
    long delay_ns;
    struct pcm_device *ref_device;
    long rsmp_delay = 0;

    ref_device = node_to_item(list_tail(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    if (pcm_get_htimestamp(ref_device->pcm, &kernel_frames, &tstamp) < 0) {
        buffer->time_stamp.tv_sec  = 0;
        buffer->time_stamp.tv_nsec = 0;
        buffer->delay_ns           = 0;
        ALOGW("read get_capture_reference_delay(): pcm_htimestamp error");
        return;
    }

    /* adjust render time stamp with delay added by current driver buffer.
    * Add the duration of current frame as we want the render time of the last
    * sample being written. */

    kernel_delay = (long)(((int64_t)kernel_frames * 1000000000) / ref_device->pcm_profile->config.rate);

    buffer->time_stamp = tstamp;
    buffer->delay_ns = kernel_delay;

    ALOGVV("get_capture_reference_delay_time_stamp Secs: [%10ld], nSecs: [%9ld], kernel_frames: [%5d],"
          " delay_ns: [%d] , frames:[%zd]",
           buffer->time_stamp.tv_sec , buffer->time_stamp.tv_nsec, kernel_frames, buffer->delay_ns, frames);
}

static void get_capture_delay(struct stream_in *in,
                              size_t frames __unused,
                              struct echo_reference_buffer *buffer)
{
    ALOGVV("%s: enter:)", __func__);
    /* read frames available in kernel driver buffer */
    unsigned int kernel_frames;
    struct timespec tstamp;
    long buf_delay;
    long rsmp_delay;
    long kernel_delay;
    long delay_ns;
    struct pcm_device *pcm_device;

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    if (pcm_get_htimestamp(pcm_device->pcm, &kernel_frames, &tstamp) < 0) {
        buffer->time_stamp.tv_sec  = 0;
        buffer->time_stamp.tv_nsec = 0;
        buffer->delay_ns           = 0;
        ALOGW("read get_capture_delay(): pcm_htimestamp error");
        return;
    }

    /* read frames available in audio HAL input buffer
     * add number of frames being read as we want the capture time of first sample
     * in current buffer */
    /* frames in in->read_buf are at driver sampling rate while frames in in->proc_buf are
     * at requested sampling rate */
    buf_delay = (long)(((int64_t)(in->read_buf_frames) * 1000000000) / in->config.rate +
                       ((int64_t)(in->proc_buf_frames) * 1000000000) / in->requested_rate );

    /* add delay introduced by resampler */
    rsmp_delay = 0;
    if (in->resampler) {
        rsmp_delay = in->resampler->delay_ns(in->resampler);
    }

    kernel_delay = (long)(((int64_t)kernel_frames * 1000000000) / in->config.rate);

    delay_ns = kernel_delay + buf_delay + rsmp_delay;

    buffer->time_stamp = tstamp;
    buffer->delay_ns   = delay_ns;
    ALOGVV("get_capture_delay_time_stamp Secs: [%10ld], nSecs: [%9ld], kernel_frames:[%5d],"
         " delay_ns: [%d], kernel_delay:[%ld], buf_delay:[%ld], rsmp_delay:[%ld],  "
         "in->read_buf_frames:[%zd], in->proc_buf_frames:[%zd], frames:[%zd]",
         buffer->time_stamp.tv_sec , buffer->time_stamp.tv_nsec, kernel_frames,
         buffer->delay_ns, kernel_delay, buf_delay, rsmp_delay,
         in->read_buf_frames, in->proc_buf_frames, frames);
}

static int32_t update_echo_reference(struct stream_in *in, size_t frames)
{
    ALOGVV("%s: enter:), in->config.channels(%d)", __func__,in->config.channels);
    struct echo_reference_buffer b;
    b.delay_ns = 0;
    struct pcm_device *pcm_device;

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    ALOGVV("update_echo_reference, in->config.channels(%d), frames = [%zd], in->ref_buf_frames = [%zd],  "
          "b.frame_count = [%zd]",
          in->config.channels, frames, in->ref_buf_frames, frames - in->ref_buf_frames);
    if (in->ref_buf_frames < frames) {
        if (in->ref_buf_size < frames) {
            in->ref_buf_size = frames;
            in->ref_buf = (int16_t *)realloc(in->ref_buf, pcm_frames_to_bytes(pcm_device->pcm, frames));
            ALOG_ASSERT((in->ref_buf != NULL),
                        "update_echo_reference() failed to reallocate ref_buf");
            ALOGVV("update_echo_reference(): ref_buf %p extended to %d bytes",
                      in->ref_buf, pcm_frames_to_bytes(pcm_device->pcm, frames));
        }
        b.frame_count = frames - in->ref_buf_frames;
        b.raw = (void *)(in->ref_buf + in->ref_buf_frames * in->config.channels);

        get_capture_delay(in, frames, &b);

        if (in->echo_reference->read(in->echo_reference, &b) == 0)
        {
            in->ref_buf_frames += b.frame_count;
            ALOGVV("update_echo_reference(): in->ref_buf_frames:[%zd], "
                    "in->ref_buf_size:[%zd], frames:[%zd], b.frame_count:[%zd]",
                 in->ref_buf_frames, in->ref_buf_size, frames, b.frame_count);
        }
    } else
        ALOGW("update_echo_reference(): NOT enough frames to read ref buffer");
    return b.delay_ns;
}

static int set_preprocessor_param(effect_handle_t handle,
                           effect_param_t *param)
{
    uint32_t size = sizeof(int);
    uint32_t psize = ((param->psize - 1) / sizeof(int) + 1) * sizeof(int) +
                        param->vsize;

    int status = (*handle)->command(handle,
                                   EFFECT_CMD_SET_PARAM,
                                   sizeof (effect_param_t) + psize,
                                   param,
                                   &size,
                                   &param->status);
    if (status == 0)
        status = param->status;

    return status;
}

static int set_preprocessor_echo_delay(effect_handle_t handle,
                                     int32_t delay_us)
{
    struct {
        effect_param_t  param;
        uint32_t        data_0;
        int32_t         data_1;
    } buf;
    memset(&buf, 0, sizeof(buf));

    buf.param.psize = sizeof(uint32_t);
    buf.param.vsize = sizeof(uint32_t);
    buf.data_0 = AEC_PARAM_ECHO_DELAY;
    buf.data_1 = delay_us;

    return set_preprocessor_param(handle, &buf.param);
}

static void push_echo_reference(struct stream_in *in, size_t frames)
{
    ALOGVV("%s: enter:)", __func__);
    /* read frames from echo reference buffer and update echo delay
     * in->ref_buf_frames is updated with frames available in in->ref_buf */

    int32_t delay_us = update_echo_reference(in, frames)/1000;
    int32_t size_in_bytes = 0;
    int i;
    audio_buffer_t buf;

    if (in->ref_buf_frames < frames)
        frames = in->ref_buf_frames;

    buf.frameCount = frames;
    buf.raw = in->ref_buf;

    for (i = 0; i < in->num_preprocessors; i++) {
        if ((*in->preprocessors[i].effect_itfe)->process_reverse == NULL)
            continue;
        ALOGVV("%s: effect_itfe)->process_reverse() BEGIN i=(%d) ", __func__, i);
        (*in->preprocessors[i].effect_itfe)->process_reverse(in->preprocessors[i].effect_itfe,
                                               &buf,
                                               NULL);
        ALOGVV("%s: effect_itfe)->process_reverse() END i=(%d) ", __func__, i);
        set_preprocessor_echo_delay(in->preprocessors[i].effect_itfe, delay_us);
    }

    in->ref_buf_frames -= buf.frameCount;
    ALOGVV("%s: in->ref_buf_frames(%zd), in->config.channels(%d) ",
           __func__, in->ref_buf_frames, in->config.channels);
    if (in->ref_buf_frames) {
        memcpy(in->ref_buf,
               in->ref_buf + buf.frameCount * in->config.channels,
               in->ref_buf_frames * in->config.channels * sizeof(int16_t));
    }
}

static void put_echo_reference(struct audio_device *adev,
                          struct echo_reference_itfe *reference)
{
    ALOGV("%s: enter:)", __func__);
    int32_t prev_generation = adev->echo_reference_generation;
    struct stream_out *out = adev->primary_output;

    if (adev->echo_reference != NULL &&
            reference == adev->echo_reference) {
        /* echo reference is taken from the low latency output stream used
         * for voice use cases */
        adev->echo_reference = NULL;
        android_atomic_inc(&adev->echo_reference_generation);
        if (out != NULL && out->usecase == USECASE_AUDIO_PLAYBACK) {
            // if the primary output is in standby or did not pick the echo reference yet
            // we can safely get rid of it here.
            // otherwise, out_write() or out_standby() will detect the change in echo reference
            // generation and release the echo reference owned by the stream.
            if ((out->echo_reference_generation != prev_generation) || out->standby)
                release_echo_reference(reference);
        } else {
            release_echo_reference(reference);
        }
        ALOGV("release_echo_reference");
    }
}

static struct echo_reference_itfe *get_echo_reference(struct audio_device *adev,
                                                      audio_format_t format __unused,
                                                      uint32_t channel_count,
                                                      uint32_t sampling_rate)
{
    ALOGV("%s: enter:)", __func__);
    put_echo_reference(adev, adev->echo_reference);
    /* echo reference is taken from the low latency output stream used
     * for voice use cases */
    if (adev->primary_output!= NULL && adev->primary_output->usecase == USECASE_AUDIO_PLAYBACK &&
            !adev->primary_output->standby) {
        struct audio_stream *stream =
                &adev->primary_output->stream.common;
        uint32_t wr_channel_count = audio_channel_count_from_out_mask(stream->get_channels(stream));
        uint32_t wr_sampling_rate = stream->get_sample_rate(stream);
        ALOGV("Calling create_echo_reference");
        int status = create_echo_reference(AUDIO_FORMAT_PCM_16_BIT,
                                           channel_count,
                                           sampling_rate,
                                           AUDIO_FORMAT_PCM_16_BIT,
                                           wr_channel_count,
                                           wr_sampling_rate,
                                           &adev->echo_reference);
        if (status == 0)
            android_atomic_inc(&adev->echo_reference_generation);
    }
    return adev->echo_reference;
}

#ifdef HW_AEC_LOOPBACK
static int get_hw_echo_reference(struct stream_in *in)
{
    struct pcm_device_profile *ref_pcm_profile;
    struct pcm_device *ref_device;
    struct audio_device *adev = in->dev;

    in->hw_echo_reference = false;

    if (adev->primary_output!= NULL &&
        !adev->primary_output->standby &&
        adev->primary_output->usecase == USECASE_AUDIO_PLAYBACK &&
        adev->primary_output->devices == AUDIO_DEVICE_OUT_SPEAKER) {
        struct audio_stream *stream = &adev->primary_output->stream.common;

        // TODO: currently there is no low latency mode for aec reference.
        ref_pcm_profile = get_pcm_device(PCM_CAPTURE, pcm_device_capture_loopback_aec.devices);
        if (ref_pcm_profile == NULL) {
            ALOGE("%s: Could not find PCM device id for the usecase(%d)",
                __func__, pcm_device_capture_loopback_aec.devices);
            return -EINVAL;
        }

        ref_device = (struct pcm_device *)calloc(1, sizeof(struct pcm_device));
        if (ref_device == NULL) {
            return -ENOMEM;
        }
        ref_device->pcm_profile = ref_pcm_profile;

        ALOGV("%s: ref_device rate:%d, ch:%d", __func__, ref_pcm_profile->config.rate, ref_pcm_profile->config.channels);
        ref_device->pcm = pcm_open(ref_device->pcm_profile->card, ref_device->pcm_profile->id, PCM_IN | PCM_MONOTONIC, &ref_device->pcm_profile->config);

        if (ref_device->pcm && !pcm_is_ready(ref_device->pcm)) {
           ALOGE("%s: %s", __func__, pcm_get_error(ref_device->pcm));
           pcm_close(ref_device->pcm);
          ref_device->pcm = NULL;
          return -EIO;
        }
        list_add_tail(&in->pcm_dev_list, &ref_device->stream_list_node);

        in->hw_echo_reference = true;

        ALOGV("%s: hw_echo_reference is true", __func__);
    }

    return 0;
}
#endif

static int get_playback_delay(struct stream_out *out,
                       size_t frames,
                       struct echo_reference_buffer *buffer)
{
    unsigned int kernel_frames;
    int status;
    int primary_pcm = 0;
    struct pcm_device *pcm_device;

    pcm_device = node_to_item(list_head(&out->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    status = pcm_get_htimestamp(pcm_device->pcm, &kernel_frames, &buffer->time_stamp);
    if (status < 0) {
        buffer->time_stamp.tv_sec  = 0;
        buffer->time_stamp.tv_nsec = 0;
        buffer->delay_ns           = 0;
        ALOGV("get_playback_delay(): pcm_get_htimestamp error,"
                "setting playbackTimestamp to 0");
        return status;
    }

    kernel_frames = pcm_get_buffer_size(pcm_device->pcm) - kernel_frames;

    /* adjust render time stamp with delay added by current driver buffer.
     * Add the duration of current frame as we want the render time of the last
     * sample being written. */
    buffer->delay_ns = (long)(((int64_t)(kernel_frames + frames)* 1000000000)/
                            out->config.rate);
    ALOGVV("get_playback_delay_time_stamp Secs: [%10ld], nSecs: [%9ld], kernel_frames: [%5u], delay_ns: [%d],",
         buffer->time_stamp.tv_sec, buffer->time_stamp.tv_nsec, kernel_frames, buffer->delay_ns);

    return 0;
}

#define GET_COMMAND_STATUS(status, fct_status, cmd_status) \
            do {                                           \
                if (fct_status != 0)                       \
                    status = fct_status;                   \
                else if (cmd_status != 0)                  \
                    status = cmd_status;                   \
            } while(0)

static int in_configure_reverse(struct stream_in *in)
{
    int32_t cmd_status;
    uint32_t size = sizeof(int);
    effect_config_t config;
    int32_t status = 0;
    int32_t fct_status = 0;
    int i;
    ALOGV("%s: enter: in->num_preprocessors(%d)", __func__, in->num_preprocessors);
    if (in->num_preprocessors > 0) {
        config.inputCfg.channels = in->main_channels;
        config.outputCfg.channels = in->main_channels;
        config.inputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
        config.outputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
        config.inputCfg.samplingRate = in->requested_rate;
        config.outputCfg.samplingRate = in->requested_rate;
        config.inputCfg.mask =
                ( EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS | EFFECT_CONFIG_FORMAT );
        config.outputCfg.mask =
                ( EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS | EFFECT_CONFIG_FORMAT );

        for (i = 0; i < in->num_preprocessors; i++)
        {
            if ((*in->preprocessors[i].effect_itfe)->process_reverse == NULL)
                continue;
            fct_status = (*(in->preprocessors[i].effect_itfe))->command(
                                                        in->preprocessors[i].effect_itfe,
                                                        EFFECT_CMD_SET_CONFIG_REVERSE,
                                                        sizeof(effect_config_t),
                                                        &config,
                                                        &size,
                                                        &cmd_status);
            ALOGV("%s: calling EFFECT_CMD_SET_CONFIG_REVERSE",__func__);
            GET_COMMAND_STATUS(status, fct_status, cmd_status);
        }
    }
    return status;
}

#define MAX_NUM_CHANNEL_CONFIGS 10

static void in_read_audio_effect_channel_configs(struct stream_in *in __unused,
                                                 struct effect_info_s *effect_info)
{
    /* size and format of the cmd are defined in hardware/audio_effect.h */
    effect_handle_t effect = effect_info->effect_itfe;
    uint32_t cmd_size = 2 * sizeof(uint32_t);
    uint32_t cmd[] = { EFFECT_FEATURE_AUX_CHANNELS, MAX_NUM_CHANNEL_CONFIGS };
    /* reply = status + number of configs (n) + n x channel_config_t */
    uint32_t reply_size =
            2 * sizeof(uint32_t) + (MAX_NUM_CHANNEL_CONFIGS * sizeof(channel_config_t));
    int32_t reply[reply_size];
    int32_t cmd_status;

    ALOG_ASSERT((effect_info->num_channel_configs == 0),
                "in_read_audio_effect_channel_configs() num_channel_configs not cleared");
    ALOG_ASSERT((effect_info->channel_configs == NULL),
                "in_read_audio_effect_channel_configs() channel_configs not cleared");

    /* if this command is not supported, then the effect is supposed to return -EINVAL.
     * This error will be interpreted as if the effect supports the main_channels but does not
     * support any aux_channels */
    cmd_status = (*effect)->command(effect,
                                EFFECT_CMD_GET_FEATURE_SUPPORTED_CONFIGS,
                                cmd_size,
                                (void*)&cmd,
                                &reply_size,
                                (void*)&reply);

    if (cmd_status != 0) {
        ALOGV("in_read_audio_effect_channel_configs(): "
                "fx->command returned %d", cmd_status);
        return;
    }

    if (reply[0] != 0) {
        ALOGW("in_read_audio_effect_channel_configs(): "
                "command EFFECT_CMD_GET_FEATURE_SUPPORTED_CONFIGS error %d num configs %d",
                reply[0], (reply[0] == -ENOMEM) ? reply[1] : MAX_NUM_CHANNEL_CONFIGS);
        return;
    }

    /* the feature is not supported */
    ALOGV("in_read_audio_effect_channel_configs()(): "
            "Feature supported and adding %d channel configs to the list", reply[1]);
    effect_info->num_channel_configs = reply[1];
    effect_info->channel_configs =
            (channel_config_t *) malloc(sizeof(channel_config_t) * reply[1]); /* n x configs */
    memcpy(effect_info->channel_configs, (reply + 2), sizeof(channel_config_t) * reply[1]);
}


#define NUM_IN_AUX_CNL_CONFIGS 2
static const channel_config_t in_aux_cnl_configs[NUM_IN_AUX_CNL_CONFIGS] = {
    { AUDIO_CHANNEL_IN_FRONT , AUDIO_CHANNEL_IN_BACK},
    { AUDIO_CHANNEL_IN_STEREO , AUDIO_CHANNEL_IN_RIGHT}
};
static uint32_t in_get_aux_channels(struct stream_in *in)
{
    int i;
    channel_config_t new_chcfg = {0, 0};

    if (in->num_preprocessors == 0)
        return 0;

    /* do not enable dual mic configurations when capturing from other microphones than
     * main or sub */
    if (!(in->devices & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC)))
        return 0;

    /* retain most complex aux channels configuration compatible with requested main channels and
     * supported by audio driver and all pre processors */
    for (i = 0; i < NUM_IN_AUX_CNL_CONFIGS; i++) {
        const channel_config_t *cur_chcfg = &in_aux_cnl_configs[i];
        if (cur_chcfg->main_channels == in->main_channels) {
            size_t match_cnt;
            size_t idx_preproc;
            for (idx_preproc = 0, match_cnt = 0;
                 /* no need to continue if at least one preprocessor doesn't match */
                 idx_preproc < (size_t)in->num_preprocessors && match_cnt == idx_preproc;
                 idx_preproc++) {
                struct effect_info_s *effect_info = &in->preprocessors[idx_preproc];
                size_t idx_chcfg;

                for (idx_chcfg = 0; idx_chcfg < effect_info->num_channel_configs; idx_chcfg++) {
                    if (memcmp(effect_info->channel_configs + idx_chcfg,
                               cur_chcfg,
                               sizeof(channel_config_t)) == 0) {
                        match_cnt++;
                        break;
                    }
                }
            }
            /* if all preprocessors match, we have a candidate */
            if (match_cnt == (size_t)in->num_preprocessors) {
                /* retain most complex aux channels configuration */
                if (audio_channel_count_from_in_mask(cur_chcfg->aux_channels) > audio_channel_count_from_in_mask(new_chcfg.aux_channels)) {
                    new_chcfg = *cur_chcfg;
                }
            }
        }
    }

    ALOGV("in_get_aux_channels(): return %04x", new_chcfg.aux_channels);

    return new_chcfg.aux_channels;
}

static int in_configure_effect_channels(effect_handle_t effect,
                                        channel_config_t *channel_config)
{
    int status = 0;
    int fct_status;
    int32_t cmd_status;
    uint32_t reply_size;
    effect_config_t config;
    uint32_t cmd[(sizeof(uint32_t) + sizeof(channel_config_t) - 1) / sizeof(uint32_t) + 1];

    ALOGV("in_configure_effect_channels(): configure effect with channels: [%04x][%04x]",
            channel_config->main_channels,
            channel_config->aux_channels);

    config.inputCfg.mask = EFFECT_CONFIG_CHANNELS;
    config.outputCfg.mask = EFFECT_CONFIG_CHANNELS;
    reply_size = sizeof(effect_config_t);
    fct_status = (*effect)->command(effect,
                                EFFECT_CMD_GET_CONFIG,
                                0,
                                NULL,
                                &reply_size,
                                &config);
    if (fct_status != 0) {
        ALOGE("in_configure_effect_channels(): EFFECT_CMD_GET_CONFIG failed");
        return fct_status;
    }

    config.inputCfg.channels = channel_config->main_channels | channel_config->aux_channels;
    config.outputCfg.channels = config.inputCfg.channels;
    reply_size = sizeof(uint32_t);
    fct_status = (*effect)->command(effect,
                                    EFFECT_CMD_SET_CONFIG,
                                    sizeof(effect_config_t),
                                    &config,
                                    &reply_size,
                                    &cmd_status);
    GET_COMMAND_STATUS(status, fct_status, cmd_status);

    cmd[0] = EFFECT_FEATURE_AUX_CHANNELS;
    memcpy(cmd + 1, channel_config, sizeof(channel_config_t));
    reply_size = sizeof(uint32_t);
    fct_status = (*effect)->command(effect,
                                EFFECT_CMD_SET_FEATURE_CONFIG,
                                sizeof(cmd), //sizeof(uint32_t) + sizeof(channel_config_t),
                                cmd,
                                &reply_size,
                                &cmd_status);
    GET_COMMAND_STATUS(status, fct_status, cmd_status);

    /* some implementations need to be re-enabled after a config change */
    reply_size = sizeof(uint32_t);
    fct_status = (*effect)->command(effect,
                                  EFFECT_CMD_ENABLE,
                                  0,
                                  NULL,
                                  &reply_size,
                                  &cmd_status);
    GET_COMMAND_STATUS(status, fct_status, cmd_status);

    return status;
}

static int in_reconfigure_channels(struct stream_in *in,
                                   effect_handle_t effect,
                                   channel_config_t *channel_config,
                                   bool config_changed) {

    int status = 0;

    ALOGV("in_reconfigure_channels(): config_changed %d effect %p",
          config_changed, effect);

    /* if config changed, reconfigure all previously added effects */
    if (config_changed) {
        int i;
        ALOGV("%s: config_changed (%d)", __func__, config_changed);
        for (i = 0; i < in->num_preprocessors; i++)
        {
            int cur_status = in_configure_effect_channels(in->preprocessors[i].effect_itfe,
                                                  channel_config);
            ALOGV("%s: in_configure_effect_channels i=(%d), [main_channel,aux_channel]=[%d|%d], status=%d",
                          __func__, i, channel_config->main_channels, channel_config->aux_channels, cur_status);
            if (cur_status != 0) {
                ALOGV("in_reconfigure_channels(): error %d configuring effect "
                        "%d with channels: [%04x][%04x]",
                        cur_status,
                        i,
                        channel_config->main_channels,
                        channel_config->aux_channels);
                status = cur_status;
            }
        }
    } else if (effect != NULL && channel_config->aux_channels) {
        /* if aux channels config did not change but aux channels are present,
         * we still need to configure the effect being added */
        status = in_configure_effect_channels(effect, channel_config);
    }
    return status;
}

static void in_update_aux_channels(struct stream_in *in,
                                   effect_handle_t effect)
{
    uint32_t aux_channels;
    channel_config_t channel_config;
    int status;

    aux_channels = in_get_aux_channels(in);

    channel_config.main_channels = in->main_channels;
    channel_config.aux_channels = aux_channels;
    status = in_reconfigure_channels(in,
                                     effect,
                                     &channel_config,
                                     (aux_channels != in->aux_channels));

    if (status != 0) {
        ALOGV("in_update_aux_channels(): in_reconfigure_channels error %d", status);
        /* resetting aux channels configuration */
        aux_channels = 0;
        channel_config.aux_channels = 0;
        in_reconfigure_channels(in, effect, &channel_config, true);
    }
    ALOGV("%s: aux_channels=%d, in->aux_channels_changed=%d", __func__, aux_channels, in->aux_channels_changed);
    if (in->aux_channels != aux_channels) {
        in->aux_channels_changed = true;
        in->aux_channels = aux_channels;
        do_in_standby_l(in);
    }
}
#endif

/* This function reads PCM data and:
 * - resample if needed
 * - process if pre-processors are attached
 * - discard unwanted channels
 */
static ssize_t read_and_process_frames(struct stream_in *in, void* buffer, ssize_t frames)
{
    ssize_t frames_wr = 0;
    size_t src_channels = in->config.channels;
    size_t dst_channels = audio_channel_count_from_in_mask(in->main_channels);
    int i;
    void *proc_buf_out;
    struct pcm_device *pcm_device;
    bool has_additional_channels = (dst_channels != src_channels) ? true : false;
#ifdef PREPROCESSING_ENABLED
    audio_buffer_t in_buf;
    audio_buffer_t out_buf;
    bool has_processing = (in->num_preprocessors != 0) ? true : false;
#endif

    /* Additional channels might be added on top of main_channels:
    * - aux_channels (by processing effects)
    * - extra channels due to HW limitations
    * In case of additional channels, we cannot work inplace
    */
    if (has_additional_channels)
        proc_buf_out = in->proc_buf_out;
    else
        proc_buf_out = buffer;

    if (list_empty(&in->pcm_dev_list)) {
        ALOGE("%s: pcm device list empty", __func__);
        return -EINVAL;
    }

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

#ifdef PREPROCESSING_ENABLED
    if (has_processing) {
        /* since all the processing below is done in frames and using the config.channels
         * as the number of channels, no changes is required in case aux_channels are present */
        while (frames_wr < frames) {
            /* first reload enough frames at the end of process input buffer */
            if (in->proc_buf_frames < (size_t)frames) {
                ssize_t frames_rd;
                if (in->proc_buf_size < (size_t)frames) {
                    size_t size_in_bytes = pcm_frames_to_bytes(pcm_device->pcm, frames);
                    in->proc_buf_size = (size_t)frames;
                    in->proc_buf_in = (int16_t *)realloc(in->proc_buf_in, size_in_bytes);
                    ALOG_ASSERT((in->proc_buf_in != NULL),
                                "process_frames() failed to reallocate proc_buf_in");
                    if (has_additional_channels) {
                        in->proc_buf_out = (int16_t *)realloc(in->proc_buf_out, size_in_bytes);
                        ALOG_ASSERT((in->proc_buf_out != NULL),
                                    "process_frames() failed to reallocate proc_buf_out");
                        proc_buf_out = in->proc_buf_out;
                    }
                }
                frames_rd = read_frames(in,
                                        in->proc_buf_in +
                                            in->proc_buf_frames * in->config.channels,
                                        frames - in->proc_buf_frames);
                  if (frames_rd < 0) {
                    /* Return error code */
                    frames_wr = frames_rd;
                    break;
                }
                in->proc_buf_frames += frames_rd;
            }

            if (in->echo_reference != NULL) {
                push_echo_reference(in, in->proc_buf_frames);
            }

             /* in_buf.frameCount and out_buf.frameCount indicate respectively
              * the maximum number of frames to be consumed and produced by process() */
            in_buf.frameCount = in->proc_buf_frames;
            in_buf.s16 = in->proc_buf_in;
            out_buf.frameCount = frames - frames_wr;
            out_buf.s16 = (int16_t *)proc_buf_out + frames_wr * in->config.channels;

            /* FIXME: this works because of current pre processing library implementation that
             * does the actual process only when the last enabled effect process is called.
             * The generic solution is to have an output buffer for each effect and pass it as
             * input to the next.
             */
            for (i = 0; i < in->num_preprocessors; i++) {
                (*in->preprocessors[i].effect_itfe)->process(in->preprocessors[i].effect_itfe,
                                                   &in_buf,
                                                   &out_buf);
            }

            /* process() has updated the number of frames consumed and produced in
             * in_buf.frameCount and out_buf.frameCount respectively
             * move remaining frames to the beginning of in->proc_buf_in */
            in->proc_buf_frames -= in_buf.frameCount;

            if (in->proc_buf_frames) {
                memcpy(in->proc_buf_in,
                       in->proc_buf_in + in_buf.frameCount * in->config.channels,
                       in->proc_buf_frames * in->config.channels * sizeof(int16_t));
            }

            /* if not enough frames were passed to process(), read more and retry. */
            if (out_buf.frameCount == 0) {
                ALOGW("No frames produced by preproc");
                continue;
            }

            if ((frames_wr + (ssize_t)out_buf.frameCount) <= frames) {
                frames_wr += out_buf.frameCount;
            } else {
                /* The effect does not comply to the API. In theory, we should never end up here! */
                ALOGE("preprocessing produced too many frames: %d + %zd  > %d !",
                      (unsigned int)frames_wr, out_buf.frameCount, (unsigned int)frames);
                frames_wr = frames;
            }
        }
    }
    else
#endif //PREPROCESSING_ENABLED
    {
        /* No processing effects attached */
        if (has_additional_channels) {
            /* With additional channels, we cannot use original buffer */
            if (in->proc_buf_size < (size_t)frames) {
                size_t size_in_bytes = pcm_frames_to_bytes(pcm_device->pcm, frames);
                in->proc_buf_size = (size_t)frames;
                in->proc_buf_out = (int16_t *)realloc(in->proc_buf_out, size_in_bytes);
                ALOG_ASSERT((in->proc_buf_out != NULL),
                            "process_frames() failed to reallocate proc_buf_out");
                proc_buf_out = in->proc_buf_out;
            }
        }
        frames_wr = read_frames(in, proc_buf_out, frames);
    }

    /* Remove all additional channels that have been added on top of main_channels:
     * - aux_channels
     * - extra channels from HW due to HW limitations
     * Assumption is made that the channels are interleaved and that the main
     * channels are first. */

    if (has_additional_channels)
    {
        int16_t* src_buffer = (int16_t *)proc_buf_out;
        int16_t* dst_buffer = (int16_t *)buffer;

        if (dst_channels == 1) {
            for (i = frames_wr; i > 0; i--)
            {
                *dst_buffer++ = *src_buffer;
                src_buffer += src_channels;
            }
        } else {
            for (i = frames_wr; i > 0; i--)
            {
                memcpy(dst_buffer, src_buffer, dst_channels*sizeof(int16_t));
                dst_buffer += dst_channels;
                src_buffer += src_channels;
            }
        }
    }

    return frames_wr;
}

static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                                   struct resampler_buffer* buffer)
{
    struct stream_in *in;
    struct pcm_device *pcm_device;

    if (buffer_provider == NULL || buffer == NULL)
        return -EINVAL;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    if (list_empty(&in->pcm_dev_list)) {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        in->read_status = -ENODEV;
        return -ENODEV;
    }

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    if (in->read_buf_frames == 0) {
        size_t size_in_bytes = pcm_frames_to_bytes(pcm_device->pcm, in->config.period_size);
        if (in->read_buf_size < in->config.period_size) {
            in->read_buf_size = in->config.period_size;
            in->read_buf = (int16_t *) realloc(in->read_buf, size_in_bytes);
            ALOG_ASSERT((in->read_buf != NULL),
                        "get_next_buffer() failed to reallocate read_buf");
        }

        in->read_status = pcm_read(pcm_device->pcm, (void*)in->read_buf, size_in_bytes);

        if (in->read_status != 0) {
            ALOGE("get_next_buffer() pcm_read error %d", in->read_status);
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in->read_status;
        }
        in->read_buf_frames = in->config.period_size;

#ifdef PREPROCESSING_ENABLED
#ifdef HW_AEC_LOOPBACK
        if (in->hw_echo_reference) {
            struct pcm_device *temp_device = NULL;
            struct pcm_device *ref_device = NULL;
            struct listnode *node = NULL;
            struct echo_reference_buffer b;
            size_t size_hw_ref_bytes;
            size_t size_hw_ref_frames;
            int read_status = 0;

            ref_device = node_to_item(list_tail(&in->pcm_dev_list),
                                      struct pcm_device, stream_list_node);
            list_for_each(node, &in->pcm_dev_list) {
                temp_device = node_to_item(node, struct pcm_device, stream_list_node);
                if (temp_device->pcm_profile->id == 1) {
                    ref_device = temp_device;
                    break;
                }
            }
            if (ref_device) {
                size_hw_ref_bytes = pcm_frames_to_bytes(ref_device->pcm, ref_device->pcm_profile->config.period_size);
                size_hw_ref_frames = ref_device->pcm_profile->config.period_size;
                if (in->hw_ref_buf_size < size_hw_ref_frames) {
                    in->hw_ref_buf_size = size_hw_ref_frames;
                    in->hw_ref_buf = (int16_t *) realloc(in->hw_ref_buf, size_hw_ref_bytes);
                    ALOG_ASSERT((in->hw_ref_buf != NULL),
                                "get_next_buffer() failed to reallocate hw_ref_buf");
                    ALOGV("get_next_buffer(): hw_ref_buf %p extended to %zd bytes",
                          in->hw_ref_buf, size_hw_ref_bytes);
                }

                read_status = pcm_read(ref_device->pcm, (void*)in->hw_ref_buf, size_hw_ref_bytes);
                if (read_status != 0) {
                    ALOGE("process_frames() pcm_read error for HW reference %d", read_status);
                    b.raw = NULL;
                    b.frame_count = 0;
                }
                else {
                    get_capture_reference_delay(in, size_hw_ref_frames, &b);
                    b.raw = (void *)in->hw_ref_buf;
                    b.frame_count = size_hw_ref_frames;
                    if (b.delay_ns != 0)
                        b.delay_ns = -b.delay_ns; // as this is capture delay, it needs to be subtracted from the microphone delay
                    in->echo_reference->write(in->echo_reference, &b);
                }
            }
        }
#endif // HW_AEC_LOOPBACK
#endif // PREPROCESSING_ENABLED
    }

    buffer->frame_count = (buffer->frame_count > in->read_buf_frames) ?
                                in->read_buf_frames : buffer->frame_count;
    buffer->i16 = in->read_buf + (in->config.period_size - in->read_buf_frames) *
                                                in->config.channels;
    return in->read_status;
}

static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                                  struct resampler_buffer* buffer)
{
    struct stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
        return;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    in->read_buf_frames -= buffer->frame_count;
}

/* read_frames() reads frames from kernel driver, down samples to capture rate
 * if necessary and output the number of frames requested to the buffer specified */
static ssize_t read_frames(struct stream_in *in, void *buffer, ssize_t frames)
{
    ssize_t frames_wr = 0;

    struct pcm_device *pcm_device;

    if (list_empty(&in->pcm_dev_list)) {
        ALOGE("%s: pcm device list empty", __func__);
        return -EINVAL;
    }

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    while (frames_wr < frames) {
        size_t frames_rd = frames - frames_wr;
        ALOGVV("%s: frames_rd: %zd, frames_wr: %zd, in->config.channels: %d",
               __func__,frames_rd,frames_wr,in->config.channels);
        if (in->resampler != NULL) {
            in->resampler->resample_from_provider(in->resampler,
                    (int16_t *)((char *)buffer +
                            pcm_frames_to_bytes(pcm_device->pcm, frames_wr)),
                    &frames_rd);
        } else {
            struct resampler_buffer buf = {
                    .raw = NULL,
                    .frame_count = frames_rd,
            };
            get_next_buffer(&in->buf_provider, &buf);
            if (buf.raw != NULL) {
                memcpy((char *)buffer +
                            pcm_frames_to_bytes(pcm_device->pcm, frames_wr),
                        buf.raw,
                        pcm_frames_to_bytes(pcm_device->pcm, buf.frame_count));
                frames_rd = buf.frame_count;
            }
            release_buffer(&in->buf_provider, &buf);
        }
        /* in->read_status is updated by getNextBuffer() also called by
         * in->resampler->resample_from_provider() */
        if (in->read_status != 0)
            return in->read_status;

        frames_wr += frames_rd;
    }
    return frames_wr;
}

static int in_release_pcm_devices(struct stream_in *in)
{
    struct pcm_device *pcm_device;
    struct listnode *node;
    struct listnode *next;

    list_for_each_safe(node, next, &in->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        list_remove(node);
        free(pcm_device);
    }

    return 0;
}

static int stop_input_stream(struct stream_in *in)
{
    struct audio_usecase *uc_info;
    struct audio_device *adev = in->dev;

    adev->active_input = NULL;
    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          in->usecase, use_case_table[in->usecase]);
    uc_info = get_usecase_from_id(adev, in->usecase);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
              __func__, in->usecase);
        return -EINVAL;
    }

    /* Disable the tx device */
    disable_snd_device(adev, uc_info, uc_info->in_snd_device, true);

    list_remove(&uc_info->adev_list_node);
    free(uc_info);

    if (list_empty(&in->pcm_dev_list)) {
        ALOGE("%s: pcm device list empty", __func__);
        return -EINVAL;
    }

    in_release_pcm_devices(in);
    list_init(&in->pcm_dev_list);

#ifdef HW_AEC_LOOPBACK
    if (in->hw_echo_reference)
    {
        in->hw_echo_reference = false;
    }
#endif

    ALOGV("%s: exit", __func__);
    return 0;
}

static int start_input_stream(struct stream_in *in)
{
    /* Enable output device and stream routing controls */
    int ret = 0;
    bool recreate_resampler = false;
    struct audio_usecase *uc_info;
    struct audio_device *adev = in->dev;
    struct pcm_device_profile *pcm_profile;
    struct pcm_device *pcm_device;

    ALOGV("%s: enter: usecase(%d)", __func__, in->usecase);
    adev->active_input = in;
    pcm_profile = get_pcm_device(in->usecase_type, in->devices);
    if (pcm_profile == NULL) {
        ALOGE("%s: Could not find PCM device id for the usecase(%d)",
              __func__, in->usecase);
        ret = -EINVAL;
        goto error_config;
    }

    uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    if (uc_info == NULL) {
        ret = -ENOMEM;
        goto error_config;
    }
    uc_info->id = in->usecase;
    uc_info->type = PCM_CAPTURE;
    uc_info->stream = (struct audio_stream *)in;
    uc_info->devices = in->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;

    pcm_device = (struct pcm_device *)calloc(1, sizeof(struct pcm_device));
    if (pcm_device == NULL) {
        free(uc_info);
        ret = -ENOMEM;
        goto error_config;
    }

    pcm_device->pcm_profile = pcm_profile;
    list_init(&in->pcm_dev_list);
    list_add_tail(&in->pcm_dev_list, &pcm_device->stream_list_node);

    list_init(&uc_info->mixer_list);
    list_add_tail(&uc_info->mixer_list,
                  &adev_get_mixer_for_card(adev,
                                       pcm_device->pcm_profile->card)->uc_list_node[uc_info->id]);

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);

    select_devices(adev, in->usecase);

    /* Config should be updated as profile can be changed between different calls
     * to this function:
     * - Trigger resampler creation
     * - Config needs to be updated */
    if (in->config.rate != pcm_profile->config.rate) {
        recreate_resampler = true;
    }
    in->config = pcm_profile->config;

#ifdef PREPROCESSING_ENABLED
    if (in->aux_channels_changed) {
        in->config.channels = audio_channel_count_from_in_mask(in->main_channels | in->aux_channels);
        recreate_resampler = true;
    }
#endif

    if (in->requested_rate != in->config.rate) {
        recreate_resampler = true;
    }

    if (recreate_resampler) {
        if (in->resampler) {
            release_resampler(in->resampler);
            in->resampler = NULL;
        }
        in->buf_provider.get_next_buffer = get_next_buffer;
        in->buf_provider.release_buffer = release_buffer;
        ret = create_resampler(in->config.rate,
                               in->requested_rate,
                               in->config.channels,
                               RESAMPLER_QUALITY_DEFAULT,
                               &in->buf_provider,
                               &in->resampler);
    }

#ifdef PREPROCESSING_ENABLED
    if (in->enable_aec && in->echo_reference == NULL) {
        in->echo_reference = get_echo_reference(adev,
                                                AUDIO_FORMAT_PCM_16_BIT,
                                                audio_channel_count_from_in_mask(in->main_channels),
                                                in->requested_rate
                                                );
    }

#ifdef HW_AEC_LOOPBACK
    if (in->enable_aec) {
        ret = get_hw_echo_reference(in);
        if (ret!=0)
            goto error_open;

        /* force ref buffer reallocation */
        in->hw_ref_buf_size = 0;
    }
#endif
#endif

    if (in->dev->voice.in_call) {
        ALOGV("%s: in_call, not handling PCMs", __func__);
        goto skip_pcm_handling;
    }

    /* Open the PCM device.
     * The HW is limited to support only the default pcm_profile settings.
     * As such a change in aux_channels will not have an effect.
     */
    ALOGV("%s: Opening PCM device card_id(%d) device_id(%d), channels %d, smp rate %d format %d, \
          period_size %d", __func__, pcm_device->pcm_profile->card, pcm_device->pcm_profile->id,
          pcm_device->pcm_profile->config.channels,pcm_device->pcm_profile->config.rate,
          pcm_device->pcm_profile->config.format, pcm_device->pcm_profile->config.period_size);

    if (pcm_profile->type == PCM_HOTWORD_STREAMING) {
        if (!adev->sound_trigger_open_for_streaming) {
            ALOGE("%s: No handle to sound trigger HAL", __func__);
            ret = -EIO;
            goto error_open;
        }
        pcm_device->pcm = NULL;
        pcm_device->sound_trigger_handle = adev->sound_trigger_open_for_streaming();
        if (pcm_device->sound_trigger_handle <= 0) {
            ALOGE("%s: Failed to open DSP for streaming", __func__);
            ret = -EIO;
            goto error_open;
        }
        ALOGV("Opened DSP successfully");
    } else {
        pcm_device->sound_trigger_handle = 0;
        pcm_device->pcm = pcm_open(pcm_device->pcm_profile->card, pcm_device->pcm_profile->id,
                                   PCM_IN | PCM_MONOTONIC, &pcm_device->pcm_profile->config);

        if (pcm_device->pcm && !pcm_is_ready(pcm_device->pcm)) {
            ALOGE("%s: %s", __func__, pcm_get_error(pcm_device->pcm));
            pcm_close(pcm_device->pcm);
            pcm_device->pcm = NULL;
            ret = -EIO;
            goto error_open;
        }
    }

skip_pcm_handling:
    /* force read and proc buffer reallocation in case of frame size or
     * channel count change */
    in->proc_buf_frames = 0;
    in->proc_buf_size = 0;
    in->read_buf_size = 0;
    in->read_buf_frames = 0;

    /* if no supported sample rate is available, use the resampler */
    if (in->resampler) {
        in->resampler->reset(in->resampler);
    }

    ALOGV("%s: exit", __func__);
    return ret;

error_open:
    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }
    stop_input_stream(in);

error_config:
    ALOGV("%s: exit: status(%d)", __func__, ret);
    adev->active_input = NULL;
    return ret;
}

void lock_input_stream(struct stream_in *in)
{
    pthread_mutex_lock(&in->pre_lock);
    pthread_mutex_lock(&in->lock);
    pthread_mutex_unlock(&in->pre_lock);
}

void lock_output_stream(struct stream_out *out)
{
    pthread_mutex_lock(&out->pre_lock);
    pthread_mutex_lock(&out->lock);
    pthread_mutex_unlock(&out->pre_lock);
}

static int uc_release_pcm_devices(struct audio_usecase *usecase)
{
    struct stream_out *out = (struct stream_out *)usecase->stream;
    struct pcm_device *pcm_device;
    struct listnode *node;
    struct listnode *next;

    list_for_each_safe(node, next, &out->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        list_remove(node);
        free(pcm_device);
    }
    list_init(&usecase->mixer_list);

    return 0;
}

static int uc_select_pcm_devices(struct audio_usecase *usecase)

{
    struct stream_out *out = (struct stream_out *)usecase->stream;
    struct pcm_device *pcm_device;
    struct pcm_device_profile *pcm_profile;
    struct mixer_card *mixer_card;
    audio_devices_t devices = usecase->devices;

    list_init(&usecase->mixer_list);
    list_init(&out->pcm_dev_list);

    while ((pcm_profile = get_pcm_device(usecase->type, devices)) != NULL) {
        pcm_device = calloc(1, sizeof(struct pcm_device));
        if (pcm_device == NULL) {
            return -ENOMEM;
        }
        pcm_device->pcm_profile = pcm_profile;
        list_add_tail(&out->pcm_dev_list, &pcm_device->stream_list_node);
        mixer_card = uc_get_mixer_for_card(usecase, pcm_profile->card);
        if (mixer_card == NULL) {
            mixer_card = adev_get_mixer_for_card(out->dev, pcm_profile->card);
            list_add_tail(&usecase->mixer_list, &mixer_card->uc_list_node[usecase->id]);
        }
        devices &= ~pcm_profile->devices;
    }

    return 0;
}

static int out_close_pcm_devices(struct stream_out *out)
{
    struct pcm_device *pcm_device;
    struct listnode *node;
    struct audio_device *adev = out->dev;

    list_for_each(node, &out->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        if (pcm_device->sound_trigger_handle > 0) {
            adev->sound_trigger_close_for_streaming(pcm_device->sound_trigger_handle);
            pcm_device->sound_trigger_handle = 0;
        }
        if (pcm_device->pcm) {
            pcm_close(pcm_device->pcm);
            pcm_device->pcm = NULL;
        }
        if (pcm_device->resampler) {
            release_resampler(pcm_device->resampler);
            pcm_device->resampler = NULL;
        }
        if (pcm_device->res_buffer) {
            free(pcm_device->res_buffer);
            pcm_device->res_buffer = NULL;
        }
    }

    return 0;
}

static int out_open_pcm_devices(struct stream_out *out)
{
    struct pcm_device *pcm_device;
    struct listnode *node;
    int ret = 0;
    int pcm_device_card;
    int pcm_device_id;

    list_for_each(node, &out->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        pcm_device_card = pcm_device->pcm_profile->card;
        pcm_device_id = pcm_device->pcm_profile->id;

        if (out->flags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER)
            pcm_device_id = pcm_device_deep_buffer.id;

        if (out->dev->voice.in_call) {
            ALOGV("%s: in_call, not opening PCMs", __func__);
            return ret;
        }

        ALOGV("%s: Opening PCM device card_id(%d) device_id(%d)",
              __func__, pcm_device_card, pcm_device_id);

        pcm_device->pcm = pcm_open(pcm_device_card, pcm_device_id,
                               PCM_OUT | PCM_MONOTONIC, &pcm_device->pcm_profile->config);

        if (pcm_device->pcm && !pcm_is_ready(pcm_device->pcm)) {
            ALOGE("%s: %s", __func__, pcm_get_error(pcm_device->pcm));
            pcm_device->pcm = NULL;
            ret = -EIO;
            goto error_open;
        }
        /*
        * If the stream rate differs from the PCM rate, we need to
        * create a resampler.
        */
        if (out->sample_rate != pcm_device->pcm_profile->config.rate) {
            ALOGV("%s: create_resampler(), pcm_device_card(%d), pcm_device_id(%d), \
                    out_rate(%d), device_rate(%d)",__func__,
                    pcm_device_card, pcm_device_id,
                    out->sample_rate, pcm_device->pcm_profile->config.rate);
            ret = create_resampler(out->sample_rate,
                    pcm_device->pcm_profile->config.rate,
                    audio_channel_count_from_out_mask(out->channel_mask),
                    RESAMPLER_QUALITY_DEFAULT,
                    NULL,
                    &pcm_device->resampler);
            pcm_device->res_byte_count = 0;
            pcm_device->res_buffer = NULL;
        }
    }
    return ret;

error_open:
    out_close_pcm_devices(out);
    return ret;
}

int disable_output_path_l(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    struct audio_usecase *uc_info;

    uc_info = get_usecase_from_id(adev, out->usecase);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
             __func__, out->usecase);
        return -EINVAL;
    }
    disable_snd_device(adev, uc_info, uc_info->out_snd_device, true);
    uc_release_pcm_devices(uc_info);
    list_remove(&uc_info->adev_list_node);
    free(uc_info);

    return 0;
}

int enable_output_path_l(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    struct audio_usecase *uc_info;

    uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    if (uc_info == NULL) {
        return -ENOMEM;
    }

    uc_info->id = out->usecase;
    uc_info->type = PCM_PLAYBACK;
    uc_info->stream = (struct audio_stream *)out;
    uc_info->devices = out->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;
    uc_select_pcm_devices(uc_info);

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);
    select_devices(adev, out->usecase);

    return 0;
}

static int stop_output_stream(struct stream_out *out)
{
    int ret = 0;
    bool do_disable = true;

    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          out->usecase, use_case_table[out->usecase]);

    stop_output_offload_stream(out, &do_disable);

    if (do_disable)
        ret = disable_output_path_l(out);

    ALOGV("%s: exit: status(%d)", __func__, ret);
    return ret;
}

static int start_output_stream(struct stream_out *out)
{
    int ret = 0;
    struct audio_device *adev = out->dev;

    ALOGV("%s: enter: usecase(%d: %s) devices(%#x) channels(%d)",
          __func__, out->usecase, use_case_table[out->usecase], out->devices, out->config.channels);

    ret = enable_output_path_l(out);
    if (ret != 0) {
        goto error_config;
    }

    if (out->usecase != USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        out->compr = NULL;
        ret = out_open_pcm_devices(out);
        if (ret != 0)
            goto error_open;
#ifdef PREPROCESSING_ENABLED
        out->echo_reference = NULL;
        out->echo_reference_generation = adev->echo_reference_generation;
        if (adev->echo_reference != NULL)
            out->echo_reference = adev->echo_reference;
#endif
    } else {
        out->compr = compress_open(COMPRESS_CARD, COMPRESS_DEVICE,
                                   COMPRESS_IN, &out->compr_config);
        if (out->compr && !is_compress_ready(out->compr)) {
            ALOGE("%s: %s", __func__, compress_get_error(out->compr));
            compress_close(out->compr);
            out->compr = NULL;
            ret = -EIO;
            goto error_open;
        }
        if (out->offload_callback)
            compress_nonblock(out->compr, out->non_blocking);

        if (adev->offload_fx_start_output != NULL)
            adev->offload_fx_start_output(out->handle);
    }
    ALOGV("%s: exit", __func__);
    return 0;
error_open:
    stop_output_stream(out);
error_config:
    return ret;
}

int stop_voice_call(struct audio_device *adev)
{
    struct audio_usecase *uc_info;

    ALOGV("%s: enter", __func__);
    adev->voice.in_call = false;

    stop_voice_session(adev->voice.session);

    uc_info = get_usecase_from_id(adev, USECASE_VOICE_CALL);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
              __func__, USECASE_VOICE_CALL);
        return -EINVAL;
    }

    disable_snd_device(adev, uc_info, uc_info->out_snd_device, false);
    disable_snd_device(adev, uc_info, uc_info->in_snd_device, true);

    list_remove(&uc_info->adev_list_node);
    free(uc_info);

    ALOGV("%s: exit", __func__);
    return 0;
}

/* always called with adev lock held */
int start_voice_call(struct audio_device *adev)
{
    struct audio_usecase *uc_info;
    int ret = 0;

    ALOGV("%s: enter", __func__);

    uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    if (uc_info == NULL) {
        ret = -ENOMEM;
        goto exit;
    }
    /*
     * We set this early so that functions called after this is being set
     * can use it. It is e.g. needed in select_devices() to inform the RILD
     * which output device we use.
     */
    adev->voice.in_call = true;

    uc_info->id = USECASE_VOICE_CALL;
    uc_info->type = VOICE_CALL;
    uc_info->stream = (struct audio_stream *)adev->primary_output;
    uc_info->devices = adev->primary_output->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;

    list_init(&uc_info->mixer_list);
    list_add_tail(&uc_info->mixer_list,
                  &adev_get_mixer_for_card(adev, SOUND_CARD)->uc_list_node[uc_info->id]);

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);

    select_devices(adev, USECASE_VOICE_CALL);

    start_voice_session(adev->voice.session);

    /* set cached volume */
    set_voice_volume_l(adev, adev->voice.volume);

exit:
    ALOGV("%s: exit", __func__);
    return ret;
}

static int check_input_parameters(uint32_t sample_rate,
                                  audio_format_t format,
                                  int channel_count)
{
    if (format != AUDIO_FORMAT_PCM_16_BIT) return -EINVAL;

    if ((channel_count < 1) || (channel_count > 2)) return -EINVAL;

    switch (sample_rate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static size_t get_input_buffer_size(uint32_t sample_rate,
                                    audio_format_t format,
                                    int channel_count,
                                    usecase_type_t usecase_type,
                                    audio_devices_t devices)
{
    size_t size = 0;
    struct pcm_device_profile *pcm_profile;

    if (check_input_parameters(sample_rate, format, channel_count) != 0)
        return 0;

    pcm_profile = get_pcm_device(usecase_type, devices);
    if (pcm_profile == NULL)
        return 0;

    /*
     * take resampling into account and return the closest majoring
     * multiple of 16 frames, as audioflinger expects audio buffers to
     * be a multiple of 16 frames
     */
    size = (pcm_profile->config.period_size * sample_rate) / pcm_profile->config.rate;
    size = ((size + 15) / 16) * 16;

    return (size * channel_count * audio_bytes_per_sample(format));

}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    (void)stream;
    (void)rate;
    return -ENOSYS;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        return out->compr_config.fragment_size;
    }

    return out->config.period_size *
               audio_stream_out_frame_size((const struct audio_stream_out *)stream);
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    (void)stream;
    (void)format;
    return -ENOSYS;
}

static int do_out_standby_l(struct stream_out *out)
{
    int status = 0;

    out->standby = true;
    if (out->usecase != USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        out_close_pcm_devices(out);
#ifdef PREPROCESSING_ENABLED
        /* stop writing to echo reference */
        if (out->echo_reference != NULL) {
            out->echo_reference->write(out->echo_reference, NULL);
            if (out->echo_reference_generation != adev->echo_reference_generation) {
                ALOGV("%s: release_echo_reference %p", __func__, out->echo_reference);
                release_echo_reference(out->echo_reference);
                out->echo_reference_generation = adev->echo_reference_generation;
            }
            out->echo_reference = NULL;
        }
#endif
    } else {
        stop_compressed_output_l(out);
        out->gapless_mdata.encoder_delay = 0;
        out->gapless_mdata.encoder_padding = 0;
        if (out->compr != NULL) {
            compress_close(out->compr);
            out->compr = NULL;
        }
    }
    status = stop_output_stream(out);

    return status;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;

    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          out->usecase, use_case_table[out->usecase]);
    lock_output_stream(out);
    if (!out->standby) {
        pthread_mutex_lock(&adev->lock);
        amplifier_output_stream_standby((struct audio_stream_out *) stream);
        do_out_standby_l(out);
        pthread_mutex_unlock(&adev->lock);
    }
    pthread_mutex_unlock(&out->lock);
    ALOGV("%s: exit", __func__);

    // out->last_write_time_us = 0; unnecessary as a stale write time has same effect

    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    (void)stream;
    (void)fd;

    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    struct listnode *node;
    struct str_parms *parms;
    char value[32];
    int ret, val = 0;
    struct audio_usecase *uc_info;
    bool do_standby = false;
    struct pcm_device *pcm_device;
#ifdef PREPROCESSING_ENABLED
    struct stream_in *in = NULL;    /* if non-NULL, then force input to standby */
#endif

    ALOGV("%s: enter: usecase(%d: %s) kvpairs: %s out->devices(%#x) "
          "adev->mode(%#x)",
          __func__, out->usecase, use_case_table[out->usecase], kvpairs,
          out->devices, adev->mode);

    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);

        ALOGV("%s: routing: usecase(%d: %s) devices=(%#x) adev->mode(%#x)",
              __func__, out->usecase, use_case_table[out->usecase], val,
              adev->mode);

        pthread_mutex_lock(&adev->lock_inputs);
        lock_output_stream(out);
        pthread_mutex_lock(&adev->lock);
#ifdef PREPROCESSING_ENABLED
        if (((int)out->devices != val) && (val != 0) && (!out->standby) &&
            (out->usecase == USECASE_AUDIO_PLAYBACK)) {
            /* reset active input:
             *  - to attach the echo reference
             *  - because a change in output device may change mic settings */
            if (adev->active_input && (adev->active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION ||
                    adev->active_input->source == AUDIO_SOURCE_MIC)) {
                in = adev->active_input;
            }
        }
#endif
        if (val != SND_DEVICE_NONE) {
            bool bt_sco_active = false;

            if (out->devices & AUDIO_DEVICE_OUT_ALL_SCO) {
                bt_sco_active = true;
            }
            out->devices = val;

            if (!out->standby) {
                uc_info = get_usecase_from_id(adev, out->usecase);
                if (uc_info == NULL) {
                    ALOGE("%s: Could not find the usecase (%d) in the list",
                          __func__, out->usecase);
                } else {
                    list_for_each(node, &out->pcm_dev_list) {
                        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
                        if ((pcm_device->pcm_profile->devices & val) == 0)
                            do_standby = true;
                        val &= ~pcm_device->pcm_profile->devices;
                    }
                    if (val != 0)
                        do_standby = true;
                }
                if (do_standby)
                    do_out_standby_l(out);
                else {
                    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD)
                        out_set_offload_parameters(adev, uc_info);
                    select_devices(adev, out->usecase);
                }
            }

            if ((adev->mode == AUDIO_MODE_IN_CALL) && !adev->voice.in_call &&
                    (out == adev->primary_output)) {
                start_voice_call(adev);
            } else if ((adev->mode == AUDIO_MODE_IN_CALL) &&
                       adev->voice.in_call &&
                       (out == adev->primary_output)) {
                /* Turn on bluetooth if needed */
                if ((out->devices & AUDIO_DEVICE_OUT_ALL_SCO) && !bt_sco_active) {
                    select_devices(adev, USECASE_VOICE_CALL);
                    start_voice_session_bt_sco(adev->voice.session);
                } else {
                    /*
                     * When we select different devices we need to restart the
                     * voice call. The modem closes the stream on its end and
                     * we do not get any output.
                     */
                    stop_voice_call(adev);
                    start_voice_call(adev);
                }
            }
        }

        pthread_mutex_unlock(&adev->lock);
        pthread_mutex_unlock(&out->lock);
#ifdef PREPROCESSING_ENABLED
        if (in) {
            /* The lock on adev->lock_inputs prevents input stream from being closed */
            lock_input_stream(in);
            pthread_mutex_lock(&adev->lock);
            LOG_ALWAYS_FATAL_IF(in != adev->active_input);
            do_in_standby_l(in);
            pthread_mutex_unlock(&adev->lock);
            pthread_mutex_unlock(&in->lock);
        }
#endif
        pthread_mutex_unlock(&adev->lock_inputs);
    }

    amplifier_set_parameters(parms);

    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        parse_compress_metadata(out, parms);
    }

    str_parms_destroy(parms);

    if (ret > 0)
        ret = 0;
    ALOGV("%s: exit: code(%d)", __func__, ret);
    return ret;
}

static char* out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str;
    char value[256];
    struct str_parms *reply = str_parms_create();
    size_t i, j;
    int ret;
    bool first = true;
    ALOGV("%s: enter: keys - %s", __func__, keys);
    ret = str_parms_get_str(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value, sizeof(value));
    if (ret >= 0) {
        value[0] = '\0';
        i = 0;
        while (out->supported_channel_masks[i] != 0) {
            for (j = 0; j < ARRAY_SIZE(out_channels_name_to_enum_table); j++) {
                if (out_channels_name_to_enum_table[j].value == out->supported_channel_masks[i]) {
                    if (!first) {
                        strcat(value, "|");
                    }
                    strcat(value, out_channels_name_to_enum_table[j].name);
                    first = false;
                    break;
                }
            }
            i++;
        }
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value);
        str = str_parms_to_str(reply);
    } else {
        str = strdup(keys);
    }
    str_parms_destroy(query);
    str_parms_destroy(reply);
    ALOGV("%s: exit: returns - %s", __func__, str);
    return str;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD)
        return COMPRESS_OFFLOAD_PLAYBACK_LATENCY;

    return (out->config.period_count * out->config.period_size * 1000) /
           (out->config.rate);
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    struct stream_out *out = (struct stream_out *)stream;

    if (out->usecase == USECASE_AUDIO_PLAYBACK_MULTI_CH) {
        /* only take left channel into account: the API is for stereo anyway */
        out->muted = (left == 0.0f);
        return 0;
    } else if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        out_set_offload_volume(left, right);
    }

    return -ENOSYS;
}

#if SUPPORTS_IRQ_AFFINITY
static int fast_set_affinity(pid_t tid) {
    cpu_set_t cpu_set;
    int cpu_num;
    const char *irq_procfs = "/proc/asound/irq_affinity";
    FILE *fp;

    if ((fp = fopen(irq_procfs, "r")) == NULL) {
        ALOGW("Procfs node %s not found", irq_procfs);
        return -1;
    }

    if (fscanf(fp, "%d", &cpu_num) != 1) {
        ALOGW("Couldn't read CPU id from procfs node %s", irq_procfs);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    CPU_ZERO(&cpu_set);
    CPU_SET(cpu_num, &cpu_set);
    return sched_setaffinity(tid, sizeof(cpu_set), &cpu_set);
}
#endif

static ssize_t out_write(struct audio_stream_out *stream, const void *buffer,
                         size_t bytes)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    ssize_t ret = 0;
    struct pcm_device *pcm_device;
    struct listnode *node;
    size_t frame_size = audio_stream_out_frame_size(stream);
    size_t frames_wr = 0, frames_rq = 0;
#ifdef PREPROCESSING_ENABLED
    size_t in_frames = bytes / frame_size;
    size_t out_frames = in_frames;
    struct stream_in *in = NULL;
#endif

    lock_output_stream(out);

#if SUPPORTS_IRQ_AFFINITY
    if (out->usecase == USECASE_AUDIO_PLAYBACK && !out->is_fastmixer_affinity_set) {
        pid_t tid = gettid();
        int err;

        err = fast_set_affinity(tid);
        if (err < 0) {
            ALOGW("Couldn't set affinity for tid %d; error %d", tid, err);
        }
        out->is_fastmixer_affinity_set = true;
    }
#endif

    if (out->standby) {
#ifdef PREPROCESSING_ENABLED
        pthread_mutex_unlock(&out->lock);
        /* Prevent input stream from being closed */
        pthread_mutex_lock(&adev->lock_inputs);
        lock_output_stream(out);
        if (!out->standby) {
            pthread_mutex_unlock(&adev->lock_inputs);
            goto false_alarm;
        }
#endif
        pthread_mutex_lock(&adev->lock);
        ret = start_output_stream(out);
        if (ret == 0) {
            amplifier_output_stream_start(stream, out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD);
        }

        /* ToDo: If use case is compress offload should return 0 */
        if (ret != 0) {
            pthread_mutex_unlock(&adev->lock);
#ifdef PREPROCESSING_ENABLED
            pthread_mutex_unlock(&adev->lock_inputs);
#endif
            goto exit;
        }
        out->standby = false;

#ifdef PREPROCESSING_ENABLED
        /* A change in output device may change the microphone selection */
        if (adev->active_input &&
            (adev->active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION ||
                adev->active_input->source == AUDIO_SOURCE_MIC)) {
                    in = adev->active_input;
                    ALOGV("%s: enter:) force_input_standby true", __func__);
        }
#endif
        pthread_mutex_unlock(&adev->lock);
#ifdef PREPROCESSING_ENABLED
        if (!in) {
            /* Leave mutex locked iff in != NULL */
            pthread_mutex_unlock(&adev->lock_inputs);
        }
#endif
    }
#ifdef PREPROCESSING_ENABLED
false_alarm:
#endif

    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        ret = out_write_offload(stream, buffer, bytes);
        return ret;
    } else {
#ifdef PREPROCESSING_ENABLED
        if (android_atomic_acquire_load(&adev->echo_reference_generation)
                != out->echo_reference_generation) {
            pthread_mutex_lock(&adev->lock);
            if (out->echo_reference != NULL) {
                ALOGV("%s: release_echo_reference %p", __func__, out->echo_reference);
                release_echo_reference(out->echo_reference);
            }
            // note that adev->echo_reference_generation here can be different from the one
            // tested above but it doesn't matter as we now have the adev mutex and it is consistent
            // with what has been set by get_echo_reference() or put_echo_reference()
            out->echo_reference_generation = adev->echo_reference_generation;
            out->echo_reference = adev->echo_reference;
            ALOGV("%s: update echo reference generation %d", __func__,
                  out->echo_reference_generation);
            pthread_mutex_unlock(&adev->lock);
        }
#endif

        if (out->muted)
            memset((void *)buffer, 0, bytes);
        list_for_each(node, &out->pcm_dev_list) {
            pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
            if (pcm_device->resampler) {
                if (bytes * pcm_device->pcm_profile->config.rate / out->sample_rate + frame_size
                        > pcm_device->res_byte_count) {
                    pcm_device->res_byte_count =
                        bytes * pcm_device->pcm_profile->config.rate / out->sample_rate + frame_size;
                    pcm_device->res_buffer =
                        realloc(pcm_device->res_buffer, pcm_device->res_byte_count);
                    ALOGV("%s: resampler res_byte_count = %zu", __func__,
                        pcm_device->res_byte_count);
                }
                frames_rq = bytes / frame_size;
                frames_wr = pcm_device->res_byte_count / frame_size;
                ALOGVV("%s: resampler request frames = %d frame_size = %d",
                    __func__, frames_rq, frame_size);
                pcm_device->resampler->resample_from_input(pcm_device->resampler,
                    (int16_t *)buffer, &frames_rq, (int16_t *)pcm_device->res_buffer, &frames_wr);
                ALOGVV("%s: resampler output frames_= %d", __func__, frames_wr);
            }
            if (pcm_device->pcm) {
#ifdef PREPROCESSING_ENABLED
                if (out->echo_reference != NULL && pcm_device->pcm_profile->devices != SND_DEVICE_OUT_SPEAKER) {
                    struct echo_reference_buffer b;
                    b.raw = (void *)buffer;
                    b.frame_count = in_frames;

                    get_playback_delay(out, out_frames, &b);
                    out->echo_reference->write(out->echo_reference, &b);
                 }
#endif
                ALOGVV("%s: writing buffer (%d bytes) to pcm device", __func__, bytes);
                if (pcm_device->resampler && pcm_device->res_buffer)
                    pcm_device->status =
                        pcm_write(pcm_device->pcm, (void *)pcm_device->res_buffer,
                            frames_wr * frame_size);
                else
                    pcm_device->status = pcm_write(pcm_device->pcm, (void *)buffer, bytes);
                if (pcm_device->status != 0)
                    ret = pcm_device->status;
            }
        }
        if (ret == 0)
            out->written += bytes / (out->config.channels * sizeof(short));
    }

exit:
    pthread_mutex_unlock(&out->lock);

    if (ret != 0) {
        list_for_each(node, &out->pcm_dev_list) {
            pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
            if (pcm_device->pcm && pcm_device->status != 0)
                ALOGE("%s: error %zd - %s", __func__, ret, pcm_get_error(pcm_device->pcm));
        }
        out_standby(&out->stream.common);
        struct timespec t = { .tv_sec = 0, .tv_nsec = 0 };
        clock_gettime(CLOCK_MONOTONIC, &t);
        const int64_t now = (t.tv_sec * 1000000000LL + t.tv_nsec) / 1000;
        const int64_t elapsed_time_since_last_write = now - out->last_write_time_us;
        int64_t sleep_time = bytes * 1000000LL / audio_stream_out_frame_size(stream) /
                   out_get_sample_rate(&stream->common) - elapsed_time_since_last_write;
        if (sleep_time > 0) {
            usleep(sleep_time);
        } else {
            // we don't sleep when we exit standby (this is typical for a real alsa buffer).
            sleep_time = 0;
        }
        out->last_write_time_us = now + sleep_time;
        // last_write_time_us is an approximation of when the (simulated) alsa
        // buffer is believed completely full. The usleep above waits for more space
        // in the buffer, but by the end of the sleep the buffer is considered
        // topped-off.
        //
        // On the subsequent out_write(), we measure the elapsed time spent in
        // the mixer. This is subtracted from the sleep estimate based on frames,
        // thereby accounting for drain in the alsa buffer during mixing.
        // This is a crude approximation; we don't handle underruns precisely.
    }

#ifdef PREPROCESSING_ENABLED
    if (in) {
        /* The lock on adev->lock_inputs prevents input stream from being closed */
        lock_input_stream(in);
        pthread_mutex_lock(&adev->lock);
        LOG_ALWAYS_FATAL_IF(in != adev->active_input);
        do_in_standby_l(in);
        pthread_mutex_unlock(&adev->lock);
        pthread_mutex_unlock(&in->lock);
        /* This mutex was left locked iff in != NULL */
        pthread_mutex_unlock(&adev->lock_inputs);
    }
#endif

    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    struct stream_out *out = (struct stream_out *)stream;
    *dsp_frames = 0;
    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        return out_get_render_offload_position(out, dsp_frames);
    } else
        return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    (void)stream;
    (void)timestamp;
    return -EINVAL;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
                                   uint64_t *frames, struct timespec *timestamp)
{
    struct stream_out *out = (struct stream_out *)stream;
    int ret = -1;

    lock_output_stream(out);

    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        ret = out_get_presentation_offload_position(out, frames, timestamp);
    } else {
        if (out->dev->voice.in_call) {
            ALOGVV("%s: in_call, do not handle PCMs", __func__);
            ret = 0;
            goto done;
        }

        /* FIXME: which device to read from? */
        if (!list_empty(&out->pcm_dev_list)) {
            struct pcm_device *pcm_device;
            struct listnode *node;
            unsigned int avail;

            list_for_each(node, &out->pcm_dev_list) {
                pcm_device = node_to_item(node,
                                          struct pcm_device,
                                          stream_list_node);

                if (pcm_device->pcm != NULL) {
                    if (pcm_get_htimestamp(pcm_device->pcm, &avail, timestamp) == 0) {
                        size_t kernel_buffer_size = out->config.period_size * out->config.period_count;
                        int64_t signed_frames = out->written - kernel_buffer_size + avail;
                        /* This adjustment accounts for buffering after app processor.
                           It is based on estimated DSP latency per use case, rather than exact. */
                        signed_frames -=
                            (render_latency(out->usecase) * out->sample_rate / 1000000LL);

                        /* It would be unusual for this value to be negative, but check just in case ... */
                        if (signed_frames >= 0) {
                            *frames = signed_frames;
                            ret = 0;
                            goto done;
                        }
                        ret = -1;
                    }
                }
            }
        }
    }

done:
    pthread_mutex_unlock(&out->lock);

    return ret;
}

static int out_set_callback(struct audio_stream_out *stream,
            stream_callback_t callback, void *cookie)
{
    struct stream_out *out = (struct stream_out *)stream;

    ALOGV("%s", __func__);
    lock_output_stream(out);
    out->offload_callback = callback;
    out->offload_cookie = cookie;
    pthread_mutex_unlock(&out->lock);
    return 0;
}

static int out_pause(struct audio_stream_out* stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    int status = -ENOSYS;
    ALOGV("%s", __func__);
    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD)
        status = out_pause_offload(out);
    return status;
}

static int out_resume(struct audio_stream_out* stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    int status = -ENOSYS;
    ALOGV("%s", __func__);
    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD)
        status = out_resume_offload(out);
    return status;
}

static int out_drain(struct audio_stream_out* stream, audio_drain_type_t type )
{
    struct stream_out *out = (struct stream_out *)stream;
    int status = -ENOSYS;
    ALOGV("%s", __func__);
    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD)
        status = out_drain_offload(out, type);
    return status;
}

static int out_flush(struct audio_stream_out* stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    ALOGV("%s", __func__);
    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        return out_flush_offload(out);
    }
    return -ENOSYS;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return in->requested_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    (void)stream;
    (void)rate;
    return -ENOSYS;
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return in->main_channels;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    (void)stream;
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    (void)stream;
    (void)format;

    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return get_input_buffer_size(in->requested_rate,
                                 in_get_format(stream),
                                 audio_channel_count_from_in_mask(in->main_channels),
                                 in->usecase_type,
                                 in->devices);
}

static int in_close_pcm_devices(struct stream_in *in)
{
    struct pcm_device *pcm_device;
    struct listnode *node;
    struct audio_device *adev = in->dev;

    list_for_each(node, &in->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        if (pcm_device) {
            if (pcm_device->pcm)
                pcm_close(pcm_device->pcm);
            pcm_device->pcm = NULL;
            if (pcm_device->sound_trigger_handle > 0)
                adev->sound_trigger_close_for_streaming(pcm_device->sound_trigger_handle);
            pcm_device->sound_trigger_handle = 0;
        }
    }
    return 0;
}


/* must be called with stream and hw device mutex locked */
static int do_in_standby_l(struct stream_in *in)
{
    int status = 0;

#ifdef PREPROCESSING_ENABLED
    struct audio_device *adev = in->dev;
#endif
    if (!in->standby) {

        in_close_pcm_devices(in);

#ifdef PREPROCESSING_ENABLED
        if (in->echo_reference != NULL) {
            /* stop reading from echo reference */
            in->echo_reference->read(in->echo_reference, NULL);
            put_echo_reference(adev, in->echo_reference);
            in->echo_reference = NULL;
        }
#ifdef HW_AEC_LOOPBACK
        if (in->hw_echo_reference)
        {
            if (in->hw_ref_buf) {
                free(in->hw_ref_buf);
                in->hw_ref_buf = NULL;
            }
        }
#endif  // HW_AEC_LOOPBACK
#endif  // PREPROCESSING_ENABLED

        status = stop_input_stream(in);

        if (in->read_buf) {
            free(in->read_buf);
            in->read_buf = NULL;
        }

        in->standby = 1;
    }

    in->last_read_time_us = 0;

    return 0;
}

// called with adev->lock_inputs locked
static int in_standby_l(struct stream_in *in)
{
    struct audio_device *adev = in->dev;
    int status = 0;
    lock_input_stream(in);
    if (!in->standby) {
        pthread_mutex_lock(&adev->lock);
        amplifier_input_stream_standby((struct audio_stream_in *) in);
        status = do_in_standby_l(in);
        pthread_mutex_unlock(&adev->lock);
    }
    pthread_mutex_unlock(&in->lock);
    return status;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    int status;
    ALOGV("%s: enter", __func__);
    pthread_mutex_lock(&adev->lock_inputs);
    status = in_standby_l(in);
    pthread_mutex_unlock(&adev->lock_inputs);
    ALOGV("%s: exit:  status(%d)", __func__, status);
    return status;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    (void)stream;
    (void)fd;

    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    struct str_parms *parms;
    char value[32];
    int ret, val = 0;
    struct audio_usecase *uc_info;
    bool do_standby = false;
    struct pcm_device *pcm_device;

    ALOGV("%s: enter: kvpairs=%s", __func__, kvpairs);
    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE, value, sizeof(value));

    pthread_mutex_lock(&adev->lock_inputs);
    lock_input_stream(in);
    pthread_mutex_lock(&adev->lock);
    if (ret >= 0) {
        val = atoi(value);
        /* no audio source uses val == 0 */
        if (((int)in->source != val) && (val != 0)) {
            in->source = val;
        }
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        if (((int)in->devices != val) && (val != 0)) {
            in->devices = val;
            /* If recording is in progress, change the tx device to new device */
            if (!in->standby) {
                uc_info = get_usecase_from_id(adev, in->usecase);
                if (uc_info == NULL) {
                    ALOGE("%s: Could not find the usecase (%d) in the list",
                          __func__, in->usecase);
                } else {
                    if (list_empty(&in->pcm_dev_list))
                        ALOGE("%s: pcm device list empty", __func__);
                    else {
                        pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                                                  struct pcm_device, stream_list_node);
                        if ((pcm_device->pcm_profile->devices & val & ~AUDIO_DEVICE_BIT_IN) == 0) {
                            do_standby = true;
                        }
                    }
                }
                if (do_standby) {
                    ret = do_in_standby_l(in);
                } else
                    ret = select_devices(adev, in->usecase);
            }
        }
    }
    pthread_mutex_unlock(&adev->lock);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&adev->lock_inputs);
    str_parms_destroy(parms);

    if (ret > 0)
        ret = 0;

    ALOGV("%s: exit: status(%d)", __func__, ret);
    return ret;
}

static char* in_get_parameters(const struct audio_stream *stream,
                               const char *keys)
{
    (void)stream;
    (void)keys;

    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    (void)stream;
    (void)gain;

    return 0;
}

static ssize_t read_bytes_from_dsp(struct stream_in *in, void* buffer,
                                   size_t bytes)
{
    struct pcm_device *pcm_device;
    struct audio_device *adev = in->dev;

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    if (pcm_device->sound_trigger_handle > 0)
        return adev->sound_trigger_read_samples(pcm_device->sound_trigger_handle, buffer, bytes);
    else
        return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void *buffer,
                       size_t bytes)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    ssize_t frames = -1;
    int ret = -1;
    int read_and_process_successful = false;

    size_t frames_rq = bytes / audio_stream_in_frame_size(stream);

    /* no need to acquire adev->lock_inputs because API contract prevents a close */
    lock_input_stream(in);

#if SUPPORTS_IRQ_AFFINITY
    if (in->usecase == USECASE_AUDIO_CAPTURE && !in->is_fastcapture_affinity_set) {
        pid_t tid = gettid();
        int err;

        err = fast_set_affinity(tid);
        if (err < 0) {
            ALOGW("Couldn't set affinity for tid %d; error %d", tid, err);
        }
        in->is_fastcapture_affinity_set = true;
    }
#endif

    if (in->standby) {
        pthread_mutex_unlock(&in->lock);
        pthread_mutex_lock(&adev->lock_inputs);
        lock_input_stream(in);
        if (!in->standby) {
            pthread_mutex_unlock(&adev->lock_inputs);
            goto false_alarm;
        }
        pthread_mutex_lock(&adev->lock);
        ret = start_input_stream(in);
        if (ret == 0) {
            amplifier_input_stream_start(stream);
        }
        pthread_mutex_unlock(&adev->lock);
        pthread_mutex_unlock(&adev->lock_inputs);

        if (ret != 0) {
            goto exit;
        }
        in->standby = 0;
    }
false_alarm:

    if (!list_empty(&in->pcm_dev_list)) {
        if (in->usecase == USECASE_AUDIO_CAPTURE_HOTWORD) {
            bytes = read_bytes_from_dsp(in, buffer, bytes);
            if (bytes > 0)
                read_and_process_successful = true;
        } else {
            /*
             * Read PCM and:
             * - resample if needed
             * - process if pre-processors are attached
             * - discard unwanted channels
             */
            frames = read_and_process_frames(in, buffer, frames_rq);
            if (frames >= 0)
                read_and_process_successful = true;
        }
    }

    /*
     * Instead of writing zeroes here, we could trust the hardware
     * to always provide zeroes when muted.
     */
    if (read_and_process_successful == true && adev->mic_mute)
        memset(buffer, 0, bytes);

exit:
    pthread_mutex_unlock(&in->lock);

    if (read_and_process_successful == false) {
        in_standby(&in->stream.common);
        ALOGV("%s: read failed - sleeping for buffer duration", __func__);
        struct timespec t = { .tv_sec = 0, .tv_nsec = 0 };
        clock_gettime(CLOCK_MONOTONIC, &t);
        const int64_t now = (t.tv_sec * 1000000000LL + t.tv_nsec) / 1000;

        // we do a full sleep when exiting standby.
        const bool standby = in->last_read_time_us == 0;
        const int64_t elapsed_time_since_last_read = standby ?
                0 : now - in->last_read_time_us;
        int64_t sleep_time = bytes * 1000000LL / audio_stream_in_frame_size(stream) /
                in_get_sample_rate(&stream->common) - elapsed_time_since_last_read;
        if (sleep_time > 0) {
            usleep(sleep_time);
        } else {
            sleep_time = 0;
        }
        in->last_read_time_us = now + sleep_time;
        // last_read_time_us is an approximation of when the (simulated) alsa
        // buffer is drained by the read, and is empty.
        //
        // On the subsequent in_read(), we measure the elapsed time spent in
        // the recording thread. This is subtracted from the sleep estimate based on frames,
        // thereby accounting for fill in the alsa buffer during the interim.
        memset(buffer, 0, bytes);
    }

    if (bytes > 0) {
        in->frames_read += bytes / audio_stream_in_frame_size(stream);
    }

    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    (void)stream;

    return 0;
}

static int in_get_capture_position(const struct audio_stream_in *stream,
                                   int64_t *frames, int64_t *time)
{
    if (stream == NULL || frames == NULL || time == NULL) {
        return -EINVAL;
    }

    struct stream_in *in = (struct stream_in *)stream;
    struct pcm_device *pcm_device;
    int ret = -ENOSYS;

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    pthread_mutex_lock(&in->lock);
    if (pcm_device->pcm) {
        struct timespec timestamp;
        unsigned int avail;
        if (pcm_get_htimestamp(pcm_device->pcm, &avail, &timestamp) == 0) {
            *frames = in->frames_read + avail;
            *time = timestamp.tv_sec * 1000000000LL + timestamp.tv_nsec;
            ret = 0;
        }
    }

    pthread_mutex_unlock(&in->lock);
    return ret;
}

static int add_remove_audio_effect(const struct audio_stream *stream,
                                   effect_handle_t effect,
                                   bool enable)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    int status = 0;
    effect_descriptor_t desc;
#ifdef PREPROCESSING_ENABLED
    int i;
#endif
    status = (*effect)->get_descriptor(effect, &desc);
    if (status != 0)
        return status;

    ALOGI("add_remove_audio_effect(), effect type: %08x, enable: %d ", desc.type.timeLow, enable);

    pthread_mutex_lock(&adev->lock_inputs);
    lock_input_stream(in);
    pthread_mutex_lock(&in->dev->lock);
#ifndef PREPROCESSING_ENABLED
    if ((in->source == AUDIO_SOURCE_VOICE_COMMUNICATION) &&
            in->enable_aec != enable &&
            (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0)) {
        in->enable_aec = enable;
        if (!in->standby)
            select_devices(in->dev, in->usecase);
    }
#else
    if ( (in->num_preprocessors > MAX_PREPROCESSORS) && (enable == true) ) {
        status = -ENOSYS;
        goto exit;
    }
    if ( enable == true ) {
        in->preprocessors[in->num_preprocessors].effect_itfe = effect;
        /* add the supported channel of the effect in the channel_configs */
        in_read_audio_effect_channel_configs(in, &in->preprocessors[in->num_preprocessors]);
        in->num_preprocessors ++;
        /* check compatibility between main channel supported and possible auxiliary channels */
        in_update_aux_channels(in, effect);//wesley crash
        in->aux_channels_changed = true;
    } else {
        /* if ( enable == false ) */
        if (in->num_preprocessors <= 0) {
            status = -ENOSYS;
            goto exit;
        }
        status = -EINVAL;
        for (i=0; i < in->num_preprocessors; i++) {
            if (status == 0) { /* status == 0 means an effect was removed from a previous slot */
                in->preprocessors[i - 1].effect_itfe = in->preprocessors[i].effect_itfe;
                in->preprocessors[i - 1].channel_configs = in->preprocessors[i].channel_configs;
                in->preprocessors[i - 1].num_channel_configs =
                    in->preprocessors[i].num_channel_configs;
                ALOGV("add_remove_audio_effect moving fx from %d to %d", i, i-1);
                continue;
            }
            if ( in->preprocessors[i].effect_itfe == effect ) {
                ALOGV("add_remove_audio_effect found fx at index %d", i);
                free(in->preprocessors[i].channel_configs);
                status = 0;
            }
        }
        if (status != 0)
            goto exit;
        in->num_preprocessors--;
        /*  if we remove one effect, at least the last proproc should be reset */
        in->preprocessors[in->num_preprocessors].num_channel_configs = 0;
        in->preprocessors[in->num_preprocessors].effect_itfe = NULL;
        in->preprocessors[in->num_preprocessors].channel_configs = NULL;
        in->aux_channels_changed = false;
        ALOGV("%s: enable(%d), in->aux_channels_changed(%d)", __func__, enable, in->aux_channels_changed);
    }
    ALOGI("%s:  num_preprocessors = %d", __func__, in->num_preprocessors);

    if ( memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0) {
        in->enable_aec = enable;
        ALOGV("add_remove_audio_effect(), FX_IID_AEC, enable: %d", enable);
        if (!in->standby) {
            select_devices(in->dev, in->usecase);
            do_in_standby_l(in);
        }
        if (in->enable_aec == true) {
            in_configure_reverse(in);
        }
    }
exit:
#endif
    ALOGW_IF(status != 0, "add_remove_audio_effect() error %d", status);
    pthread_mutex_unlock(&in->dev->lock);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&adev->lock_inputs);
    return status;
}

static int in_add_audio_effect(const struct audio_stream *stream,
                               effect_handle_t effect)
{
    ALOGV("%s: effect %p", __func__, effect);
    return add_remove_audio_effect(stream, effect, true);
}

static int in_remove_audio_effect(const struct audio_stream *stream,
                                  effect_handle_t effect)
{
    ALOGV("%s: effect %p", __func__, effect);
    return add_remove_audio_effect(stream, effect, false);
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address __unused)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_out *out;
    int ret = 0;
    struct pcm_device_profile *pcm_profile;

    ALOGV("%s: enter: sample_rate(%d) channel_mask(%#x) devices(%#x) flags(%#x)",
          __func__, config->sample_rate, config->channel_mask, devices, flags);
    *stream_out = NULL;
    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (out == NULL) {
        ret = -ENOMEM;
        goto error_config;
    }

    if (devices == AUDIO_DEVICE_NONE)
        devices = AUDIO_DEVICE_OUT_SPEAKER;

    out->flags = flags;
    out->devices = devices;
    out->dev = adev;
    out->format = config->format;
    out->sample_rate = config->sample_rate;
    out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    out->supported_channel_masks[0] = AUDIO_CHANNEL_OUT_STEREO;
    out->handle = handle;

    pcm_profile = get_pcm_device(PCM_PLAYBACK, devices);
    if (pcm_profile == NULL) {
        ret = -EINVAL;
        goto error_open;
    }
    out->config = pcm_profile->config;

    /* Init use case and pcm_config */
    if (out->flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
        if (config->offload_info.version != AUDIO_INFO_INITIALIZER.version ||
            config->offload_info.size != AUDIO_INFO_INITIALIZER.size) {
            ALOGE("%s: Unsupported Offload information", __func__);
            ret = -EINVAL;
            goto error_open;
        }
        if (!is_supported_format(config->offload_info.format)) {
            ALOGE("%s: Unsupported audio format", __func__);
            ret = -EINVAL;
            goto error_open;
        }

        out->compr_config.codec = (struct snd_codec *)
                                    calloc(1, sizeof(struct snd_codec));
        if (out->compr_config.codec == NULL) {
            ret = -ENOMEM;
            goto error_open;
        }

        out->usecase = USECASE_AUDIO_PLAYBACK_OFFLOAD;
        if (config->offload_info.channel_mask)
            out->channel_mask = config->offload_info.channel_mask;
        else if (config->channel_mask)
            out->channel_mask = config->channel_mask;
        out->format = config->offload_info.format;
        out->sample_rate = config->offload_info.sample_rate;

        out->stream.set_callback = out_set_callback;
        out->stream.pause = out_pause;
        out->stream.resume = out_resume;
        out->stream.drain = out_drain;
        out->stream.flush = out_flush;

        out->compr_config.codec->id =
                get_snd_codec_id(config->offload_info.format);
        out->compr_config.fragment_size = COMPRESS_OFFLOAD_FRAGMENT_SIZE;
        out->compr_config.fragments = COMPRESS_OFFLOAD_NUM_FRAGMENTS;
        out->compr_config.codec->sample_rate = config->offload_info.sample_rate;
        out->compr_config.codec->bit_rate =
                    config->offload_info.bit_rate;
        out->compr_config.codec->ch_in =
                audio_channel_count_from_out_mask(config->channel_mask);
        out->compr_config.codec->ch_out = out->compr_config.codec->ch_in;

        if (flags & AUDIO_OUTPUT_FLAG_NON_BLOCKING)
            out->non_blocking = 1;

        out->send_new_metadata = 1;
        create_offload_callback_thread(out);
        out->offload_state = OFFLOAD_STATE_IDLE;

        ALOGV("%s: offloaded output offload_info version %04x bit rate %d",
                __func__, config->offload_info.version,
                config->offload_info.bit_rate);
    } else if (out->flags & (AUDIO_OUTPUT_FLAG_DEEP_BUFFER)) {
        out->usecase = USECASE_AUDIO_PLAYBACK_DEEP_BUFFER;
        out->config = pcm_device_deep_buffer.config;
        out->sample_rate = out->config.rate;
        ALOGV("%s: use AUDIO_PLAYBACK_DEEP_BUFFER",__func__);
    } else {
        out->usecase = USECASE_AUDIO_PLAYBACK;
        out->sample_rate = out->config.rate;
    }

    if (flags & AUDIO_OUTPUT_FLAG_PRIMARY) {
        if (adev->primary_output == NULL)
            adev->primary_output = out;
        else {
            ALOGE("%s: Primary output is already opened", __func__);
            ret = -EEXIST;
            goto error_open;
        }
    }

    /* Check if this usecase is already existing */
    pthread_mutex_lock(&adev->lock);
    if (get_usecase_from_id(adev, out->usecase) != NULL) {
        ALOGE("%s: Usecase (%d) is already present", __func__, out->usecase);
        pthread_mutex_unlock(&adev->lock);
        ret = -EEXIST;
        goto error_open;
    }
    pthread_mutex_unlock(&adev->lock);

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
    out->stream.get_presentation_position = out_get_presentation_position;

    out->standby = 1;
    /* out->muted = false; by calloc() */
    /* out->written = 0; by calloc() */

    pthread_mutex_init(&out->lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&out->pre_lock, (const pthread_mutexattr_t *) NULL);
    pthread_cond_init(&out->cond, (const pthread_condattr_t *) NULL);

    config->format = out->stream.common.get_format(&out->stream.common);
    config->channel_mask = out->stream.common.get_channels(&out->stream.common);
    config->sample_rate = out->stream.common.get_sample_rate(&out->stream.common);

    out->is_fastmixer_affinity_set = false;

    *stream_out = &out->stream;
    ALOGV("%s: exit", __func__);
    return 0;

error_open:
    free(out);
    *stream_out = NULL;
error_config:
    ALOGV("%s: exit: ret %d", __func__, ret);
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    (void)dev;

    ALOGV("%s: enter", __func__);
    out_standby(&stream->common);
    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        destroy_offload_callback_thread(out);

        if (out->compr_config.codec != NULL)
            free(out->compr_config.codec);
    }
    pthread_cond_destroy(&out->cond);
    pthread_mutex_destroy(&out->lock);
    free(stream);
    ALOGV("%s: exit", __func__);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct str_parms *parms;
    char value[32];
#if SWAP_SPEAKER_ON_SCREEN_ROTATION
    int val;
#endif
    int ret;

    ALOGV("%s: enter: %s", __func__, kvpairs);

    parms = str_parms_create_str(kvpairs);

    /******************************************************
     *** BT SCO
     ******************************************************/
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_BT_NREC, value, sizeof(value));
    if (ret >= 0) {
        /* When set to false, HAL should disable EC and NS
         * But it is currently not supported.
         */
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev->voice.bluetooth_nrec = true;
        else
            adev->voice.bluetooth_nrec = false;
    }

    ret = str_parms_get_str(parms,
                            AUDIO_PARAMETER_KEY_BT_SCO_WB,
                            value,
                            sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0) {
            adev->voice.bluetooth_wb = true;
        } else {
            adev->voice.bluetooth_wb = false;
        }
    }

    ret = str_parms_get_str(parms, "screen_state", value, sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev->screen_off = false;
        else
            adev->screen_off = true;
    }

#if SWAP_SPEAKER_ON_SCREEN_ROTATION
    ret = str_parms_get_int(parms, "rotation", &val);
    if (ret >= 0) {
        bool reverse_speakers = false;
        switch(val) {
        /* FIXME: note that the code below assumes that the speakers are in the correct placement
             relative to the user when the device is rotated 90deg from its default rotation. This
             assumption is device-specific, not platform-specific like this code. */
        case 270:
            reverse_speakers = true;
            break;
        case 0:
        case 90:
        case 180:
            break;
        default:
            ALOGE("%s: unexpected rotation of %d", __func__, val);
        }
        pthread_mutex_lock(&adev->lock);
        if (adev->speaker_lr_swap != reverse_speakers) {
            adev->speaker_lr_swap = reverse_speakers;
            /* only update the selected device if there is active pcm playback */
            struct audio_usecase *usecase;
            struct listnode *node;
            list_for_each(node, &adev->usecase_list) {
                usecase = node_to_item(node, struct audio_usecase, adev_list_node);
                if (usecase->type == PCM_PLAYBACK) {
                    select_devices(adev, usecase->id);
                    break;
                }
            }
        }
        pthread_mutex_unlock(&adev->lock);
    }
#endif /* SWAP_SPEAKER_ON_SCREEN_ROTATION */

    str_parms_destroy(parms);

    if (ret > 0)
        ret = 0;

    ALOGV("%s: exit with code(%d)", __func__, ret);
    return ret;
}

static char* adev_get_parameters(const struct audio_hw_device *dev,
                                 const char *keys)
{
    (void)dev;
    (void)keys;

    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    (void)dev;

    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    int ret = 0;
    struct audio_device *adev = (struct audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    /* cache volume */
    adev->voice.volume = volume;
    ret = set_voice_volume_l(adev, adev->voice.volume);
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    (void)dev;
    (void)volume;

    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev,
                                  float *volume)
{
    (void)dev;
    (void)volume;

    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted)
{
    (void)dev;
    (void)muted;

    return -ENOSYS;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted)
{
    (void)dev;
    (void)muted;

    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    struct audio_device *adev = (struct audio_device *)dev;

    pthread_mutex_lock(&adev->lock);
    if (adev->mode != mode) {
        ALOGI("%s mode = %d", __func__, mode);
        if (amplifier_set_mode(mode) != 0) {
            ALOGE("Failed setting amplifier mode");
        }
        adev->mode = mode;

        if ((mode == AUDIO_MODE_NORMAL) && adev->voice.in_call) {
            stop_voice_call(adev);
        }
    }
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    struct audio_device *adev = (struct audio_device *)dev;
    int err = 0;

    pthread_mutex_lock(&adev->lock);
    adev->mic_mute = state;

    if (adev->mode == AUDIO_MODE_IN_CALL) {
        set_voice_session_mic_mute(adev->voice.session, state);
    }

    pthread_mutex_unlock(&adev->lock);
    return err;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    *state = adev->mic_mute;

    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    (void)dev;

    /* NOTE: we default to built in mic which may cause a mismatch between what we
     * report here and the actual buffer size
     */
    return get_input_buffer_size(config->sample_rate,
                                 config->format,
                                 audio_channel_count_from_in_mask(config->channel_mask),
                                 PCM_CAPTURE /* usecase_type */,
                                 AUDIO_DEVICE_IN_BUILTIN_MIC);
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle __unused,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags,
                                  const char *address __unused,
                                  audio_source_t source)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in;
    struct pcm_device_profile *pcm_profile;

    ALOGV("%s: enter", __func__);

    *stream_in = NULL;
    if (check_input_parameters(config->sample_rate, config->format,
                               audio_channel_count_from_in_mask(config->channel_mask)) != 0)
        return -EINVAL;

    usecase_type_t usecase_type = source == AUDIO_SOURCE_HOTWORD ?
                PCM_HOTWORD_STREAMING : flags & AUDIO_INPUT_FLAG_FAST ?
                        PCM_CAPTURE_LOW_LATENCY : PCM_CAPTURE;
    pcm_profile = get_pcm_device(usecase_type, devices);
    if (pcm_profile == NULL && usecase_type == PCM_CAPTURE_LOW_LATENCY) {
        // a low latency profile may not exist for that device, fall back
        // to regular capture. the MixerThread automatically changes
        // to non-fast capture based on the buffer size.
        flags &= ~AUDIO_INPUT_FLAG_FAST;
        usecase_type = PCM_CAPTURE;
        pcm_profile = get_pcm_device(usecase_type, devices);
    }
    if (pcm_profile == NULL)
        return -EINVAL;

    in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    if (in == NULL) {
        return -ENOMEM;
    }

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;
    in->stream.get_capture_position = in_get_capture_position;

    in->devices = devices;
    in->source = source;
    in->dev = adev;
    in->standby = 1;
    in->main_channels = config->channel_mask;
    in->requested_rate = config->sample_rate;
    if (config->sample_rate != CAPTURE_DEFAULT_SAMPLING_RATE)
        flags = flags & ~AUDIO_INPUT_FLAG_FAST;
    in->input_flags = flags;
    // in->frames_read = 0;
    /* HW codec is limited to default channels. No need to update with
     * requested channels */
    in->config = pcm_profile->config;

    /* Update config params with the requested sample rate and channels */
    if (source == AUDIO_SOURCE_HOTWORD) {
        in->usecase = USECASE_AUDIO_CAPTURE_HOTWORD;
    } else {
        in->usecase = USECASE_AUDIO_CAPTURE;
    }
    in->usecase_type = usecase_type;

    pthread_mutex_init(&in->lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&in->pre_lock, (const pthread_mutexattr_t *) NULL);

    in->is_fastcapture_affinity_set = false;

    *stream_in = &in->stream;
    ALOGV("%s: exit", __func__);
    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                    struct audio_stream_in *stream)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in = (struct stream_in*)stream;
    ALOGV("%s", __func__);

    /* prevent concurrent out_set_parameters, or out_write from standby */
    pthread_mutex_lock(&adev->lock_inputs);

    if (in->read_buf) {
        free(in->read_buf);
        in->read_buf = NULL;
    }

    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }

#ifdef PREPROCESSING_ENABLED
    int i;

    for (i=0; i<in->num_preprocessors; i++) {
        free(in->preprocessors[i].channel_configs);
    }

    if (in->proc_buf_in) {
        free(in->proc_buf_in);
        in->proc_buf_in = NULL;
    }

    if (in->proc_buf_out) {
        free(in->proc_buf_out);
        in->proc_buf_out = NULL;
    }

    if (in->ref_buf) {
        free(in->ref_buf);
        in->ref_buf = NULL;
    }

#endif

    in_standby_l(in);
    free(stream);

    pthread_mutex_unlock(&adev->lock_inputs);

    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    (void)device;
    (void)fd;

    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct audio_device *adev = (struct audio_device *)device;
    voice_session_deinit(adev->voice.session);
    audio_device_ref_count--;
    if (audio_device_ref_count == 0) {
        if (amplifier_close() != 0) {
            ALOGE("Amplifier close failed");
        }
    }
    free(adev->snd_dev_ref_cnt);
    free_mixer_list(adev);
    free(device);

    adev = NULL;

    return 0;
}

/* This returns true if the input parameter looks at all plausible as a low latency period size,
 * or false otherwise.  A return value of true doesn't mean the value is guaranteed to work,
 * just that it _might_ work.
 */
static bool period_size_is_plausible_for_low_latency(int period_size)
{
    switch (period_size) {
    case 64:
    case 96:
    case 128:
    case 192:
    case 256:
        return true;
    default:
        return false;
    }
}

static int adev_open(const hw_module_t *module, const char *name,
                     hw_device_t **device)
{
    ALOGV("%s: enter", __func__);
    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) return -EINVAL;

    *device = NULL;

    adev = calloc(1, sizeof(struct audio_device));
    if (adev == NULL) {
        return -ENOMEM;
    }

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->device.common.module = (struct hw_module_t *)module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.get_master_volume = adev_get_master_volume;
    adev->device.set_master_mute = adev_set_master_mute;
    adev->device.get_master_mute = adev_get_master_mute;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    /* Set the default route before the PCM stream is opened */
    adev->mode = AUDIO_MODE_NORMAL;
    adev->active_input = NULL;
    adev->primary_output = NULL;

    adev->voice.volume = 1.0f;
    adev->voice.bluetooth_nrec = true;
    adev->voice.in_call = false;
    adev->voice.bluetooth_wb = false;

    /* adev->cur_hdmi_channels = 0;  by calloc() */
    adev->snd_dev_ref_cnt = calloc(SND_DEVICE_MAX, sizeof(int));
    if (adev->snd_dev_ref_cnt == NULL) {
        free(adev);
        return -ENOMEM;
    }

    adev->ns_in_voice_rec = false;

    list_init(&adev->usecase_list);

    if (mixer_init(adev) != 0) {
        free(adev->snd_dev_ref_cnt);
        free(adev);
        ALOGE("%s: Failed to init, aborting.", __func__);
        *device = NULL;
        return -EINVAL;
    }

    if (access(OFFLOAD_FX_LIBRARY_PATH, R_OK) == 0) {
        adev->offload_fx_lib = dlopen(OFFLOAD_FX_LIBRARY_PATH, RTLD_NOW);
        if (adev->offload_fx_lib == NULL) {
            ALOGE("%s: DLOPEN failed for %s", __func__, OFFLOAD_FX_LIBRARY_PATH);
        } else {
            ALOGV("%s: DLOPEN successful for %s", __func__, OFFLOAD_FX_LIBRARY_PATH);
            adev->offload_fx_start_output =
                        (int (*)(audio_io_handle_t))dlsym(adev->offload_fx_lib,
                                                        "visualizer_hal_start_output");
            adev->offload_fx_stop_output =
                        (int (*)(audio_io_handle_t))dlsym(adev->offload_fx_lib,
                                                        "visualizer_hal_stop_output");
        }
    }

    if (access(SOUND_TRIGGER_HAL_LIBRARY_PATH, R_OK) == 0) {
        adev->sound_trigger_lib = dlopen(SOUND_TRIGGER_HAL_LIBRARY_PATH, RTLD_NOW);
        if (adev->sound_trigger_lib == NULL) {
            ALOGE("%s: DLOPEN failed for %s", __func__, SOUND_TRIGGER_HAL_LIBRARY_PATH);
        } else {
            ALOGV("%s: DLOPEN successful for %s", __func__, SOUND_TRIGGER_HAL_LIBRARY_PATH);
            adev->sound_trigger_open_for_streaming =
                        (int (*)(void))dlsym(adev->sound_trigger_lib,
                                                        "sound_trigger_open_for_streaming");
            adev->sound_trigger_read_samples =
                        (size_t (*)(int, void *, size_t))dlsym(adev->sound_trigger_lib,
                                                        "sound_trigger_read_samples");
            adev->sound_trigger_close_for_streaming =
                        (int (*)(int))dlsym(adev->sound_trigger_lib,
                                                        "sound_trigger_close_for_streaming");
            if (!adev->sound_trigger_open_for_streaming ||
                !adev->sound_trigger_read_samples ||
                !adev->sound_trigger_close_for_streaming) {

                ALOGE("%s: Error grabbing functions in %s", __func__, SOUND_TRIGGER_HAL_LIBRARY_PATH);
                adev->sound_trigger_open_for_streaming = 0;
                adev->sound_trigger_read_samples = 0;
                adev->sound_trigger_close_for_streaming = 0;
            }
        }
    }

    adev->voice.session = voice_session_init(adev);
    if (adev->voice.session == NULL) {
        ALOGE("%s: Failed to initialize voice session data", __func__);

        free(adev->snd_dev_ref_cnt);
        free(adev);

        *device = NULL;
        return -EINVAL;
    }

    if (amplifier_open() != 0) {
        ALOGE("Amplifier initialization failed");
    }

    *device = &adev->device.common;

    audio_device_ref_count++;

    char value[PROPERTY_VALUE_MAX];
    if (property_get("audio_hal.period_size", value, NULL) > 0) {
        int trial = atoi(value);
        if (period_size_is_plausible_for_low_latency(trial)) {

            pcm_device_playback.config.period_size = trial;
            pcm_device_playback.config.start_threshold =
                    PLAYBACK_START_THRESHOLD(trial, PLAYBACK_PERIOD_COUNT);
            pcm_device_playback.config.stop_threshold =
                    PLAYBACK_STOP_THRESHOLD(trial, PLAYBACK_PERIOD_COUNT);

            pcm_device_capture_low_latency.config.period_size = trial;
        }
    }

    ALOGV("%s: exit", __func__);
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Samsung Audio HAL",
        .author = "The LineageOS Project",
        .methods = &hal_module_methods,
    },
};
