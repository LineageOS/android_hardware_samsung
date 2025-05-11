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
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include "osi.h"

/************************************************************************
** Internal function prototype
*************************************************************************/

/************************************************************************
** Public functions
*************************************************************************/
tOSI_MEM_HANDLER OSI_mem_get(size_t size) {
  tOSI_MEM_HANDLER free_mem = NULL;
  int index, err_cnt = 3;

  if (size > OSI_MEM_POOL_SIZE) {
    OSI_loge("%s : memory getting failed. Max size=%d, Requested size=%d",
             __func__, OSI_MEM_POOL_SIZE, (int)size);
    return NULL;
  }

/* Try 3 times to get memory */
retry_getting:

  osi_lock();
  for (index = 0; index < osi_info.mem_max_cnt; index++) {
#ifdef OSI_USE_DYNAMIC_BUF
    if (osi_info.mem[index]->state == OSI_FREE)
      free_mem = (tOSI_MEM_HANDLER)osi_info.mem[index];
#else
    if (osi_info.mem[index].state == OSI_FREE)
      free_mem = (tOSI_MEM_HANDLER)&osi_info.mem[index];
#endif
  }

  if (free_mem == NULL) {
    /* Not found free memory handler */
    OSI_loge("%s : Failed to find free memory pool(max: %d)", __func__,
             osi_info.mem_max_cnt);
#ifdef OSI_USE_DYNAMIC_BUF
    /* get a new buffer */
    free_mem = osi_info.mem[index] =
        (tOSI_MEM_HANDLER)malloc(OSI_MEM_POOL_SIZE);
    if (osi_info.mem[index] != NULL) {
      osi_info.mem[index]->state = OSI_FREE;
      osi_info.mem_max_cnt++;
      OSI_loge("%s : get a new buffer (max: %d)", __func__,
               osi_info.mem_max_cnt);
    } else
#endif
    if (--err_cnt > 0) {
      OSI_loge("%s : try %d time(s) more!", __func__, err_cnt + 1);
      osi_unlock();
      sched_yield();
      OSI_delay(20);
      goto retry_getting;
    }
  } else {
    free_mem->state = OSI_ALLOCATED;
    memset(free_mem->buffer, 0, OSI_MEM_POOL_SIZE);
  }
  osi_unlock();

  return free_mem;
}

void OSI_mem_free(tOSI_MEM_HANDLER target) {
  if (!target) return;

  osi_lock();
  target->state = OSI_FREE;
  osi_unlock();
}

tOSI_QUEUE_HANDLER OSI_queue_allocate(const char* que_name) {
  tOSI_QUEUE_HANDLER free_que = NULL;
  int index;

  osi_lock();
  for (index = 0; index < osi_info.queue_max_cnt; index++) {
    if (osi_info.queue[index].state == OSI_FREE) {
      if (free_que == NULL)
        free_que = (tOSI_QUEUE_HANDLER)&osi_info.queue[index];
    } else {
      if (osi_info.queue[index].name == NULL) continue;

      if (strcmp((char const*)osi_info.queue[index].name,
                 (char const*)que_name) == 0) {
        OSI_loge("%s : %s queue is already allocated [%d]", __func__, que_name,
                 index);
        free_que = NULL;
        break;
      }
    }
  }

  if (free_que == NULL) {
    OSI_loge("%s : Failed to find free queue(max: %d)", __func__,
             OSI_MAX_QUEUE);
  } else {
    memset(free_que->queue, 0, OSI_QUEUE_SIZE);
    free_que->name = que_name;
    free_que->state = OSI_ALLOCATED;
    free_que->head = 0;
    free_que->tail = OSI_QUEUE_SIZE;
  }
  osi_unlock();

  return free_que;
}

int OSI_queue_put(tOSI_QUEUE_HANDLER queue, void* p_data) {
  int ret;

  osi_lock();

  if (!queue || queue->state != OSI_ALLOCATED) {
    OSI_loge("%s : queue is not allocated", __func__);
    return -1;
  }

  if (queue->head == queue->tail) {
    OSI_loge("%s : queue is overflower (max: %d)", __func__, OSI_QUEUE_SIZE);
  } else {
    queue->queue[queue->head++] = p_data;
    if (queue->head >= OSI_QUEUE_SIZE) queue->head = 0;

    // pthread_cond_broadcast(&queue->cond);
    pthread_cond_signal(&queue->cond);
  }

  ret = (queue->head) - (queue->tail);

  osi_unlock();

  if (ret < 0) ret += OSI_QUEUE_SIZE;

  return ret;
}

void* queue_get(tOSI_QUEUE_HANDLER queue) {
  void* data = NULL;

  if (!queue || queue->state != OSI_ALLOCATED) {
    OSI_loge("%s : queue is not allocated", __func__);
    return NULL;
  }

  if (queue->tail + 1 >= OSI_QUEUE_SIZE) {
    if (queue->head == 0) {
      // empty
      // OSI_loge("%s : queue is empty", __func__);
      return NULL;
    }
  } else {
    if (queue->tail + 1 == queue->head) {
      // empty
      // OSI_loge("%s : queue is empty", __func__);
      return NULL;
    }
  }

  queue->tail++;
  if (queue->tail >= OSI_QUEUE_SIZE) queue->tail = 0;
  data = queue->queue[queue->tail];

  return data;
}

void* OSI_queue_get(tOSI_QUEUE_HANDLER queue) {
  void* data = NULL;

  osi_lock();
  data = queue_get(queue);
  osi_unlock();

  return data;
}

void* OSI_queue_get_wait(tOSI_QUEUE_HANDLER queue) {
  void* ret;

  osi_lock();

  if (!queue || queue->state != OSI_ALLOCATED) {
    OSI_loge("%s : queue is not allocated", __func__);
    return NULL;
  }

  ret = queue_get(queue);
  if (ret == NULL) {
    pthread_cond_wait(&queue->cond, &osi_info.mutex);
    ret = queue_get(queue);
  }

  osi_unlock();

  return ret;
}

void OSI_queue_free(tOSI_QUEUE_HANDLER target) {
  if (target) {
    target->name = NULL;
    target->state = OSI_FREE;
  }
}

tOSI_QUEUE_HANDLER OSI_queue_get_handler(const char* name) {
  tOSI_QUEUE_HANDLER queue = NULL;
  int index;

  if (name == NULL) return NULL;

  osi_lock();
  for (index = 0; index < OSI_MAX_QUEUE; index++) {
    if (osi_info.queue[index].name == NULL) continue;

    if (strcmp((char const*)osi_info.queue[index].name,
        (char const*)name) == 0) {
      queue = (tOSI_QUEUE_HANDLER)&osi_info.queue[index];
      break;
    }
  }
  osi_unlock();

  return queue;
}
