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

#define LOG_TAG "audio_hw_voice"
#define LOG_NDEBUG 0
/*#define VERY_VERY_VERBOSE_LOGGING*/
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#include <stdlib.h>
#include <pthread.h>

#include "audio_hw.h"
#include "voice.h"

struct voice_session *voice_session_init(void)
{
    struct voice_session *session;
    int ret;

    session = calloc(1, sizeof(struct voice_session));
    if (session == NULL) {
        return NULL;
    }

    /* Do this as the last step so we do not have to close it on error */
    ret = ril_open(&session->ril);
    if (ret != 0) {
        free(session);
        return NULL;
    }

    return session;
}

void voice_session_deinit(struct voice_session *session)
{
    ril_close(&session->ril);
    free(session);
}
