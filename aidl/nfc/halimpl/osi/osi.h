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

#ifndef OSI_H
#define OSI_H

/************************************************************************
** OS Interface
*************************************************************************/
#include <osi_common.h>

#include <log/log.h>

/************************************************************************
** public functions
*************************************************************************/
/*
 * Function     OSI_init
 *
 * Description  This function is called to initialize OSI context.
 *
 * Return       OSI_FAIL if any problem
 *              OSI_OK if initialization is succeeded.
 */
OSI_STATE OSI_init(void);

/*
 * Function     OSI_deinit
 *
 * Description  This function is called to deinitialize OSI context.
 *
 */
void OSI_deinit(void);

/*
 * Function     OSI_delay
 *
 * Description  This function is called to delay.
 *
 * Parameter    timeout(input): ms
 *
 * Return
 *
 */
void OSI_delay(uint32_t timeout);

/***************
 *  OSI TASK
 ***************/
/*
 * Function     OSI_task_allocate
 *
 * Description  This function is called to create a new task.
 *
 * Parameter    task_name(input):
 *              task_entry(input): entry function.
 *
 * Return       OSI_TASK_HANDLE if allocate is succeeded,
 *              NULL if any problem.
 */
tOSI_TASK_HANDLER OSI_task_allocate(const char* task_name,
                                    tOSI_TASK_ENTRY task_entry);

/*
 * Function     OSI_task_run
 *
 * Description  This function is called to create a new task.
 *
 * Parameter    OSI_TASK_HANDLE(input): target task
 *
 * Return       OSI_OK if creadtion is succeeded,
 *              OSI_FAIL if any problem.
 */
OSI_STATE OSI_task_run(tOSI_TASK_HANDLER task_handler);

/*
 * Function     OSI_task_isRun
 *
 * Description  Check the task is running or not.
 *
 * Parameter    task_handler(input): Target task handler to check running
 *
 * Return       OSI_RUN, on run.
 *              OSI_FAIL, on error.
 */
OSI_STATE OSI_task_isRun(tOSI_TASK_HANDLER task_handler);

/*
 * Function     OSI_task_kill
 *
 * Description  This function is called to kill a task.
 *
 * Parameter    task_handler(input): Target task handler to kill.
 *
 * Return       OSI_OK, on success.
 *              other, on error.
 */
OSI_STATE OSI_task_kill(tOSI_TASK_HANDLER task_handler);

/*
 * Function     OSI_task_stop
 *
 * Description  This function is called to stop a task.
 *
 * Parameter    task_handler(input): Target task handler to kill.
 *
 * Return       OSI_OK, on success.
 *              other, on error.
 */
OSI_STATE OSI_task_stop(tOSI_TASK_HANDLER task_handler);

/*
 * Function     OSI_task_free
 *
 * Description  This function is called to free a task.
 *
 * Parameter    task_handler(input): Target task handler to kill.
 *
 * Return       OSI_OK, on success.
 *              other, on error.
 */
OSI_STATE OSI_task_free(tOSI_TASK_HANDLER task_handler);

/*
 * Function     OSI_task_get_handler
 *
 * Description  This function is called to get handler by task name.
 *
 * Parameter    name(input): Target name to get handler.
 *
 * Return       tOSI_TASK_HANDLER, on success.
 *              NULL, on error.
 */
tOSI_TASK_HANDLER OSI_task_get_handler(char* name);

/***************
 *  OSI MEMORY
 ***************/
/*
 * Function     OSI_mem_get
 *
 * Description  This function is called to get memeory.
 *
 * Parameter    size(input): it should be small than OSI_MEM_POLL_SIZE
 *
 * Return       Memory address if getting is succeeded,
 *              NULL if any problem.
 */
tOSI_MEM_HANDLER OSI_mem_get(size_t size);

/*
 * Function     OSI_mem_free
 *
 * Description  This function is called to free memeory.
 *
 * Parameter    target(input):
 *
 * Return
 */
void OSI_mem_free(tOSI_MEM_HANDLER target);

/** queue **/
/*
 * Function     OSI_queue_allocate
 *
 * Description  This function is called to get a free queue.
 *              Anyone using OSI can access this message que.
 *
 * Parameter    name(input): que_name
 *
 * Return       tOSI_QUEUE_HANDLER if init is succeeded.
 *              NULL if any problem.
 */
tOSI_QUEUE_HANDLER OSI_queue_allocate(const char* que_name);

/*
 * Function     OSI_queue_put
 *
 * Description  This function is called to put data to the queue.
 *
 * Parameter    que (input): queue handler.
 *              data (input): void * data to put the stack.
 *
 * Return       number of element in target queue
 *
 */
int OSI_queue_put(tOSI_QUEUE_HANDLER queue, void* p_data);

/*
 * Function     OSI_queue_get
 *
 * Description  This function is called to get data from the queue.
 *
 * Parameter    que (input): queue handler.
 *
 * Return       (void *) the first data in the queue.
 *              NULL if any problem.
 */
void* OSI_queue_get(tOSI_QUEUE_HANDLER queue);

/*
 * Function     OSI_queue_get_wait
 *
 * Description  This function is called to get data from the queue.
 *              If the queue is empty, this function is waiting for
 *              putting data.
 *
 * Parameter    que (input): queue handler.
 *
 * Return       (void *) the first data in the queue.
 *              NULL if any problem.
 */
void* OSI_queue_get_wait(tOSI_QUEUE_HANDLER target);

/*
 * Function     OSI_queue_free
 *
 * Description  This function is called to make que free.
 *
 * Parameter    que (input): queue handler.
 *
 * Return       void
 */
void OSI_queue_free(tOSI_QUEUE_HANDLER target);

/*
 * Function     OSI_queue_get_handler
 *
 * Description  This function is called to get handler by queue name.
 *
 * Parameter    name(input): Target name to get handler.
 *
 * Return       tOSI_QUEUE_HANDLER, on success.
 *              NULL, on error.
 */
tOSI_QUEUE_HANDLER OSI_queue_get_handler(const char* name);

/***************
 *  OSI TIMER
 ***************/
/*
 * Function     OSI_timer_allocate
 *
 * Description  This function is called to get a timer.
 *
 * Parameter    timer_name(input):
 *
 * Return       0 if any problem
 *              other if initialization is succeeded.
 */
tOSI_TIMER_HANDLER OSI_timer_allocate(const char* timer_name);

/*
 * Function     OSI_timer_start
 *
 * Description  This function is called to start a timer.
 *
 * Parameter    timer_handler (input)
 *              timeout (input): time out value. it is millisecond.
 *              callback (input): callback function.
 *
 * Return       0 if any problem
 *              other if initialization is succeeded.
 *
 */
int OSI_timer_start(tOSI_TIMER_HANDLER timer, uint32_t timeout,
                    tOSI_TIMER_CALLBACK callback, void* param);

/*
 * Function     OSI_timer_stop
 *
 * Description  This function is called to stop a timer.
 *
 * Parameter    timer_handler (input)
 *
 * Return
 *
 */
void OSI_timer_stop(tOSI_TIMER_HANDLER timer);

/*
 * Function     OSI_timer_free
 *
 * Description  This function is called to free a timer.
 *
 * Parameter    timer_handler (input)
 *
 * Return
 *
 */
void OSI_timer_free(tOSI_TIMER_HANDLER timer);

/*
 * Function     OSI_timer_get_handler
 *
 * Description  This function is called to get timer handler by name.
 *
 * Parameter    name(input): Target name to get handler.
 *
 * Return       tOSI_QUEUE_HANDLER, on success.
 *              NULL, on error.
 */
tOSI_TIMER_HANDLER OSI_timer_get_handler(char* name);

/***************
 *  OSI DEBUG
 ***************/
#define OSI_DEBUG
extern int osi_debug_level;
#define OSI_set_debug_level(xx) (osi_debug_level = xx)
#ifdef OSI_DEBUG
#define __osi_log(type, ...) (void)ALOG(type, "SecHAL", __VA_ARGS__)
#define OSI_logt(format, ...)                                      \
  do {                                                             \
    if (osi_debug_level >= 2)                                      \
      __osi_log(LOG_INFO, "%s: " format, __func__, ##__VA_ARGS__); \
  } while (0)
#define OSI_logd(format, ...)                                       \
  do {                                                              \
    if (osi_debug_level >= 1)                                       \
      __osi_log(LOG_DEBUG, "%s: " format, __func__, ##__VA_ARGS__); \
  } while (0)
#define OSI_loge(format, ...)                                       \
  do {                                                              \
    if (osi_debug_level >= 0)                                       \
      __osi_log(LOG_ERROR, "%s: " format, __func__, ##__VA_ARGS__); \
  } while (0)
#else
#define OSI_logt(...)
#define OSI_logd(...)
#define OSI_loge(...)
#endif

#endif /* OSI_H */
