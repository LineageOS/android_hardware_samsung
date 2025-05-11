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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "hal.h"
#include "osi.h"

#include <errno.h>
#include <time.h>

int osi_debug_level;
tOSI_INFO osi_info;

OSI_STATE OSI_init(void) {
  int32_t index;

  memset(&osi_info, 0, sizeof(osi_info));

  /* Initialize lock */
  pthread_mutex_init(&osi_info.mutex, NULL);

  /* Initialize task information */
  for (index = 0; index < OSI_MAX_TASK; index++) {
    osi_info.task[index].state = OSI_FREE;
  }

  /* Initialize memory information */
  osi_info.mem_max_cnt = OSI_MAX_MEM_POOL;
  for (index = 0; index < OSI_MAX_MEM_POOL; index++) {
#ifdef OSI_USE_DYNAMIC_BUF
    osi_info.mem[index] = (tOSI_MEM_HANDLER)malloc(OSI_MEM_POOL_SIZE);
    if (osi_info.mem[index] == NULL) {
      OSI_loge("%s : maximum conut of buffer is set to %d, (expected: %d)",
               index, OSI_MAX_MEM_POOL);
      osi_info.mem_max_cnt = index;
      break;
    }
    osi_info.mem[index]->state = OSI_FREE;
#else
    osi_info.mem[index].state = OSI_FREE;
#endif
  }

  /* Initialize queue information */
  osi_info.queue_max_cnt = OSI_MAX_QUEUE;
  for (index = 0; index < OSI_MAX_QUEUE; index++) {
    osi_info.queue[index].state = OSI_FREE;
    osi_info.queue[index].tail = OSI_QUEUE_SIZE;
  }

  /* Initialize timer information */
  for (index = 0; index < OSI_MAX_TIMER; index++) {
    osi_info.timer[index].state = OSI_FREE;
    osi_info.timer[index].name = NULL;
    osi_info.timer[index].callback = NULL;
    osi_info.timer[index].callback_param = NULL;
  }

  return OSI_OK;
}

void OSI_deinit() {
  int index;
#ifdef OSI_USE_DYNAMIC_BUF
  for (index = 0; index < osi_info.mem_max_cnt; index++) {
    if (osi_info.mem[index]) free(osi_info.mem[index]);
  }
#endif

  /* deinitialize timer */
  for (index = 0; index < OSI_MAX_TIMER; index++) {
    OSI_timer_free(&osi_info.timer[index]);
  }
  osi_info.usingTimer = 0;
}

void osi_lock() { pthread_mutex_lock(&osi_info.mutex); }

void osi_unlock() { pthread_mutex_unlock(&osi_info.mutex); }

void OSI_delay(uint32_t timeout) {
  struct timespec delay;
  int err;

  delay.tv_sec = timeout / 1000;
  delay.tv_nsec = 1000 * 1000 * (timeout % 1000);

  do {
    err = nanosleep(&delay, &delay);
  } while (err < 0 && errno == EINTR);
}
