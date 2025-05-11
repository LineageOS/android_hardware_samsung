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

#ifndef OSI_COMMON_H
#define OSI_COMMON_H

/************************************************************************
** OS Interface common component
*************************************************************************/
#include <pthread.h>
#ifdef ANDROID
#include <sys/times.h>
#endif

/************************************************************************
** Common definition and type
*************************************************************************/
/* OSI common */
// Maximum count of each obejct
#define OSI_MAX_TASK (3)  // main task, read task
#define OSI_MAX_MEM_POOL (10)
#define OSI_MAX_QUEUE (2)  // main queue, read queue
#define OSI_MAX_TIMER (2)  // fw download timer, nci timer

// Size of each object
#define OSI_MEM_POOL_SIZE \
  (259)  // for NFC (NCI_MAX_CTRL_SIZE + NCI_MSG_HDR_SIZE + NFC_HAL_EVT_SIZE)
#define OSI_QUEUE_SIZE (10)

// State
typedef uint8_t OSI_STATE;
#define OSI_FAIL 0
#define OSI_OK 1
#define OSI_FREE 2
#define OSI_ALLOCATED 3
#define OSI_RUN 4
#define OSI_STOP 5

#define OSI_TIMER_THREAD_FLAG_DETACH 0x01

/************************************************************************
** Common definition and type, union
*************************************************************************/
/* OSI task */
typedef void (*tOSI_TASK_ENTRY)(void);
typedef struct {
  pthread_t task;
  const char* name;
  OSI_STATE state;
  tOSI_TASK_ENTRY task_entry;
} sOSI_TASK;
typedef sOSI_TASK*(tOSI_TASK_HANDLER);

/* OSI memory */
typedef struct {
  uint8_t buffer[OSI_MEM_POOL_SIZE];
  OSI_STATE state;
} sOSI_MEM;
typedef sOSI_MEM*(tOSI_MEM_HANDLER);

/* OSI queue */
typedef struct {
  void* queue[OSI_QUEUE_SIZE];
  int head;
  int tail;
  const char* name;
  OSI_STATE state;
  pthread_cond_t cond;
} sOSI_QUEUE;
typedef sOSI_QUEUE*(tOSI_QUEUE_HANDLER);

/* OSI timer */
typedef void (*tOSI_TIMER_CALLBACK)(void* param);
typedef struct {
  int32_t exact_time;
  int32_t init_timeout;
  int32_t timeout;
  const char* name;
  tOSI_TIMER_CALLBACK callback;
  void* callback_param;
  OSI_STATE state;
} sOSI_TIMER;
typedef sOSI_TIMER*(tOSI_TIMER_HANDLER);

/* OSI Context */
typedef struct {
  /* main */
  pthread_mutex_t mutex;

  /* task */
  sOSI_TASK task[OSI_MAX_TASK];

/* memory */
#ifndef OSI_USE_DYNAMIC_BUF
  sOSI_MEM mem[OSI_MAX_MEM_POOL];
#else
  sOSI_MEM* mem[OSI_MAX_MEM_POOL];
#endif
  int32_t mem_max_cnt; /* Maximum number of allocated memory pool */

  /* queue */
  sOSI_QUEUE queue[OSI_MAX_QUEUE];
  int32_t queue_max_cnt; /* Maximum number of allocated queue */

  /* timer */
  sOSI_TIMER timer[OSI_MAX_TIMER];
  pthread_t timer_thread;
  int32_t usingTimer;
  unsigned char timer_thread_flag;

  /* log */
} tOSI_INFO;

/************************************************************************
** Global variable
*************************************************************************/
extern tOSI_INFO osi_info;

/************************************************************************
** Internal functions
*************************************************************************/
void osi_lock();
void osi_unlock();
void OSI_timer_update(int32_t tick);
int32_t OSI_timer_get_current_time();
#endif
