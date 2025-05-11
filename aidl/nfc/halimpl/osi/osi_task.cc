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
#include <signal.h>
#include <string.h>
#include "osi.h"

/************************************************************************
** Internal function prototype
*************************************************************************/

/************************************************************************
** Public functions
*************************************************************************/
void osi_task_entry(void* arg) {
  tOSI_TASK_ENTRY task_entry = (tOSI_TASK_ENTRY)arg;

  // pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

  task_entry();
  OSI_logt("%s : exit task", __func__);

  pthread_exit(NULL);
}

tOSI_TASK_HANDLER OSI_task_allocate(const char* task_name,
                                    tOSI_TASK_ENTRY task_entry) {
  tOSI_TASK_HANDLER free_task = NULL;
  int index;

  /* Find free task */
  osi_lock();
  for (index = 0; index < OSI_MAX_TASK; index++) {
    if (osi_info.task[index].state == OSI_FREE) {
      if (free_task == NULL)
        free_task = (tOSI_TASK_HANDLER)&osi_info.task[index];
    } else {
      if (osi_info.task[index].name == NULL) continue;

      /* User can't not make same name of task */
      if (strcmp((char const*)osi_info.task[index].name,
                 (char const*)task_name) == 0) {
        OSI_loge("%s : %s task is already allocated [%d]", __func__, task_name,
                 index);
        free_task = NULL;
        break;
      }
    }
  }

  if (free_task == NULL) {
    OSI_loge("%s : Failed to find free task(max: %d)", __func__, OSI_MAX_TASK);
  } else {
    free_task->state = OSI_ALLOCATED;
    free_task->name = task_name;
    free_task->task_entry = task_entry;
  }

  osi_unlock();
  return free_task;
}

OSI_STATE OSI_task_run(tOSI_TASK_HANDLER task_handler) {
  pthread_attr_t attr;
  int ret = OSI_FAIL;

  osi_lock();
  if (!task_handler) {
    OSI_loge("%s : task handler is not exist!!", __func__);
  } else if (task_handler->state != OSI_ALLOCATED) {
    OSI_loge("%s : task state is not ALLOCATED!! (%d)", __func__,
             task_handler->state);
  } else {
    /* Thread attr configuration */
    pthread_attr_init(&attr);
    if (!pthread_create(&(task_handler->task), &attr,
                        (void* (*)(void*))osi_task_entry,
                        (void*)(task_handler->task_entry))) {  //
      task_handler->state = OSI_RUN;
      ret = OSI_OK;
    } else {
      OSI_loge("%s : pthread_create failed(%d), %s", __func__, ret,
               task_handler->name);
    }
    pthread_attr_destroy(&attr);
  }

  osi_unlock();
  return ret;
}

OSI_STATE OSI_task_isRun(tOSI_TASK_HANDLER task_handler) {
  OSI_STATE ret = OSI_FAIL;
  osi_lock();
  if (task_handler && task_handler->state == OSI_RUN) ret = OSI_RUN;
  osi_unlock();
  return ret;
}

OSI_STATE OSI_task_stop(tOSI_TASK_HANDLER task_handler) {
  OSI_STATE ret = OSI_OK;
  if (!task_handler) return OSI_OK;

  osi_lock();
  if (task_handler->state == OSI_RUN) {
    osi_unlock();
    // ret = pthread_cancel(task_handler->task);
    ret = (OSI_STATE)pthread_join(task_handler->task, NULL);
    osi_lock();
  }

  task_handler->state = OSI_ALLOCATED;
  osi_unlock();

  return ret;
}

OSI_STATE OSI_task_free(tOSI_TASK_HANDLER task_handler) {
  OSI_STATE ret = OSI_OK;

  OSI_task_stop(task_handler);
  osi_lock();
  task_handler->name = NULL;
  task_handler->state = OSI_FREE;
  osi_unlock();

  return ret;
}

OSI_STATE OSI_task_kill(tOSI_TASK_HANDLER task_handler) {
  OSI_STATE ret = OSI_OK;
  if (!task_handler) return OSI_OK;

  osi_lock();
  if (task_handler->state == OSI_RUN) {
    osi_unlock();
    // ret = pthread_cancel(task_handler->task);
    ret = (OSI_STATE)pthread_join(task_handler->task, NULL);
    osi_lock();
  }

  task_handler->name = NULL;
  task_handler->state = OSI_FREE;
  osi_unlock();

  return ret;
}

tOSI_TASK_HANDLER OSI_task_get_handler(char* name) {
  tOSI_TASK_HANDLER task = NULL;
  int index;

  if (!name) return NULL;

  osi_lock();
  for (index = 0; index < OSI_MAX_TASK; index++) {
    if ((char const*)osi_info.task[index].name == NULL) continue;

    if (strcmp((char const*)osi_info.task[index].name,
               (char const*)name) == 0) {
      task = &osi_info.task[index];
      break;
    }
  }
  osi_unlock();

  return task;
}
