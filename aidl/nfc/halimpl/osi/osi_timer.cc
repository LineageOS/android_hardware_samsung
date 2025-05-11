/*
 *    Copyright (C) 2013 SAMSUNG S.LSI
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

/************************************************************************
** OS interface for task handling
*************************************************************************/
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include "osi.h"

/************************************************************************
** Internal function prototype
*************************************************************************/
void timer_thread(void);

/************************************************************************
** Public functions
*************************************************************************/
tOSI_TIMER_HANDLER OSI_timer_allocate(const char* timer_name) {
  tOSI_TIMER_HANDLER free_timer = NULL;
  int index;

  osi_lock();
  for (index = 0; index < OSI_MAX_TIMER; index++) {
    if (osi_info.timer[index].state == OSI_FREE) {
      if (free_timer == NULL)
        free_timer = (tOSI_TIMER_HANDLER)&osi_info.timer[index];
    } else {
      if ((char const*)osi_info.timer[index].name == NULL) continue;

      if (strcmp((char const*)osi_info.timer[index].name,
                 (char const*)timer_name) == 0) {
        OSI_loge("%s : %s timer is already allocated [%d]", __func__,
                 timer_name, index);
        free_timer = NULL;
        break;
      }
    }
  }

  if (free_timer == NULL) {
    OSI_loge("%s : Failed to find free timer(max: %d)", __func__,
             OSI_MAX_TIMER);
  } else {
    free_timer->timeout = 0;
    free_timer->name = timer_name;
    free_timer->callback = NULL;
    free_timer->callback_param = NULL;
    free_timer->state = OSI_ALLOCATED;
  }
  osi_unlock();

  return free_timer;
}

int OSI_timer_start(tOSI_TIMER_HANDLER timer, uint32_t timeout,
                    tOSI_TIMER_CALLBACK callback, void* param) {
  pthread_attr_t attr;
  int ret_th;

  if (timer == NULL) {
    OSI_loge("%s : Invalid parameters", __func__);
    return 0;
  } else if (timer->state == OSI_FREE) {
    OSI_loge("%s : The timer is not allocated", __func__);
    return 0;
  } else {
    osi_lock();
    OSI_logt("enter,osi_lock");
    timer->timeout = timeout;
    timer->exact_time = OSI_timer_get_current_time();
    timer->init_timeout = timeout - 10;
    timer->callback = callback;
    timer->callback_param = param;

    if (timer->state != OSI_RUN) {
      timer->state = OSI_RUN;

      /* start timer thread */
      if (osi_info.usingTimer < 1) {
        osi_info.timer_thread_flag |= OSI_TIMER_THREAD_FLAG_DETACH;
        //[START] S.LSI - To increase usingTimer prior to timer_thread
        osi_info.usingTimer++;
        //[END] S.LSI - To increase usingTimer prior to timer_thread
        ret_th = pthread_attr_init(&attr);
        if (ret_th != 0)
          OSI_loge("%s : Error pthread_attr_init! ,erron: %d", __func__,
                   ret_th);

        OSI_logt("before pthread_create for timer thread");
        ret_th = pthread_create(&osi_info.timer_thread, &attr,
                                (void* (*)(void*))timer_thread, NULL);
        OSI_logt("after pthread_create for timer thread");
        if (ret_th != 0)
          OSI_loge("%s : Error to create timer_thread! ,erron: %d", __func__,
                   ret_th);

        ret_th = pthread_attr_destroy(&attr);
        if (ret_th != 0)
          OSI_loge("%s : Error pthread_arrt_destroy ,erron: %d", __func__,
                   ret_th);
      } else
        osi_info.usingTimer++;
    }
    OSI_logt("before osi_unlock");
    osi_unlock();
    OSI_logt("exit");
    return timeout;
  }
}

void OSI_timer_stop(tOSI_TIMER_HANDLER timer) {
  if (timer == NULL) {
    OSI_loge("%s : Invalid parameters", __func__);
  } else if (timer->state != OSI_RUN) {
    OSI_logd("%s : This timer is not running", __func__);
  } else {
    osi_lock();
    timer->state = OSI_STOP;
    osi_info.usingTimer--;
    if (osi_info.usingTimer <= 0) {
      /* Cancle pthread_detach */
      osi_info.timer_thread_flag &= ~OSI_TIMER_THREAD_FLAG_DETACH;
      osi_unlock();
      pthread_join(osi_info.timer_thread, NULL);
    } else {
      osi_unlock();
    }
  }
}

void OSI_timer_free(tOSI_TIMER_HANDLER timer) {
  if (timer) {
    osi_lock();

    if (timer->state == OSI_RUN) osi_info.usingTimer--;

    timer->state = OSI_FREE;
    timer->name = NULL;
    timer->callback = NULL;
    timer->callback_param = NULL;

    osi_unlock();
  }
}

tOSI_TIMER_HANDLER OSI_timer_get_handler(char* name) {
  tOSI_TIMER_HANDLER timer = NULL;
  int index;

  if (name == NULL) return NULL;

  osi_lock();
  for (index = 0; index < OSI_MAX_TIMER; index++) {
    if ((char const*)osi_info.timer[index].name == NULL) continue;

    if (strcmp((char const*)osi_info.timer[index].name,
        (char const*)name) == 0) {
      timer = &osi_info.timer[index];
      break;
    }
  }
  osi_unlock();

  return timer;
}

int32_t OSI_timer_get_current_time() {
  struct timeval sec;
  struct tm* now;
  time_t rawtime;

  gettimeofday(&sec, NULL);
  time(&rawtime);
  now = gmtime(&rawtime);

  return (((now->tm_hour * 3600) + (now->tm_min * 60) + (now->tm_sec)) * 1000)
          + (sec.tv_usec / 1000);
}

/************************************************************************
** Internal function
*************************************************************************/
void OSI_timer_update(int32_t tick) {
  int index;

  osi_lock();

  /* timer is not using */
  if (osi_info.usingTimer <= 0) {
    osi_unlock();
    return;
  }

  for (index = 0; index < OSI_MAX_TIMER; index++) {
    if (osi_info.timer[index].state == OSI_RUN) {
      osi_info.timer[index].timeout -= tick;

      if (osi_info.timer[index].timeout <= 0) {
        /* START [16051100] - RTCC Patch */
        if (((OSI_timer_get_current_time() - osi_info.timer[index].exact_time)
            > osi_info.timer[index].init_timeout)
            || (OSI_timer_get_current_time() < osi_info.timer[index].exact_time))
        /* END [16051100] - RTCC Patch */
        {
          osi_info.timer[index].state = OSI_STOP;
          osi_info.usingTimer--;

          if (osi_info.timer[index].callback != NULL) {
            osi_info.timer[index].callback(
                osi_info.timer[index].callback_param);
          }
        } else {
          osi_info.timer[index].timeout =
              osi_info.timer[index].init_timeout
              - (OSI_timer_get_current_time()
              - osi_info.timer[index].exact_time);
        }
      }
    }
  }
  osi_unlock();
}

void timer_thread(void) {
  struct timespec delay;
  int err;

  while (osi_info.usingTimer > 0) {
    /* delay */
    // OSI_delay(1);
    {
      /* 1ms sleep for nanosleep()*/
      delay.tv_sec = 0;
      delay.tv_nsec = 1000 * 1000;

      do {
        err = nanosleep(&delay, &delay);
        if (err < 0) OSI_loge("%s:Fail nanosleep", __func__);
      } while (err < 0 && errno == EINTR);
    }

    OSI_timer_update(1);
  }

  if (osi_info.timer_thread_flag & OSI_TIMER_THREAD_FLAG_DETACH)
    pthread_detach(pthread_self());

  pthread_exit(NULL);
}
