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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "device.h"
#include "hal.h"
#include "osi.h"
#include "sec_nfc.h"
#include "util.h"

int pw_driver, tr_driver;
pthread_mutex_t tr_lock;
int tr_closer;
bool isSleep;
int wakeup_delay;
bool log_ptr;
eNFC_DEV_MODE dev_state;
tOSI_TASK_HANDLER read_task;

// [Start] Workaround - i2c write fail(self wakeup)
bool first_wakeup;
// [End] Workaround - i2c write fail(self wakeup)
void read_thread(void);
void data_trace(const char* head, int len, uint8_t* p_data);

int device_init(int data_trace) {
  dev_state = NFC_DEV_MODE_OFF;
  log_ptr = data_trace;

  read_task = OSI_task_allocate("read_task", read_thread);
  if (!read_task) {
    OSI_loge("Failed to allocate task for read thread!!");
    return -1;
  }

  pthread_mutex_init(&tr_lock, NULL);

  return 0;
}

void device_deinit() {
  device_close();
  pthread_mutex_destroy(&tr_lock);
  OSI_task_free(read_task);
}

int device_open() {
  int ret;
  char pw_driver_name[64];
  char tr_driver_name[64];

  ret = get_config_string(cfg_name_table[CFG_POWER_DRIVER], pw_driver_name,
                          sizeof(pw_driver_name));
  if (ret == 0) return -EPERM;

  ret = get_config_string(cfg_name_table[CFG_TRANS_DRIVER], tr_driver_name,
                          sizeof(tr_driver_name));
  if (ret == 0) return -EPERM;

  pw_driver = open(pw_driver_name, O_RDWR | O_NOCTTY);
  if (pw_driver < 0) {
    OSI_loge("Failed to open device driver: %s, pw_driver : 0x%x, errno = %d",
             pw_driver_name, pw_driver, errno);
    return pw_driver;
  }

  tr_driver = pw_driver;

  OSI_loge("pw_driver: %d, tr_driver: %d", pw_driver, tr_driver);
  device_set_mode(NFC_DEV_MODE_BOOTLOADER);

  if (OSI_OK != OSI_task_run(read_task)) {
    OSI_loge("Failed to run read task!!");
    OSI_task_stop(read_task);
    close(tr_driver);
    close(pw_driver);
    return -1;
  }

  if (!get_config_int(cfg_name_table[CFG_WAKEUP_DELAY], &wakeup_delay))
    wakeup_delay = 10;

  return 0;
}

void device_close(void) {
  close(tr_driver);
  close(pw_driver);

  pthread_mutex_lock(&tr_lock);
  tr_driver = -1;
  pw_driver = -1;
  pthread_mutex_unlock(&tr_lock);

  if (tr_closer != 0) write(tr_closer, "x", 1);

  OSI_task_stop(read_task);
}

int device_set_mode(eNFC_DEV_MODE mode) {
  tNFC_HAL_FW_INFO* fw = &nfc_hal_info.fw_info;
  tNFC_HAL_FW_BL_INFO* bl = &fw->bl_info;
  int ret;

  OSI_logt("device mode chage: %d -> %d", dev_state, mode);
  ret = ioctl(pw_driver, SEC_NFC_SET_MODE, (int)mode);
  if (!ret) {
    if (mode == NFC_DEV_MODE_BOOTLOADER && mode != dev_state)
      nfc_hal_info.fw_info.seq_no = 0;
    else if (mode == NFC_DEV_MODE_ON)
      isSleep = true;
    dev_state = mode;
  }

  if (bl->product >= SNFC_SEN6) {
    OSI_logd("Wait 100ms for firmware boot for SEN6");
    usleep(100 * 1000);
  }

  return ret;
}

int device_sleep(void) {
  if (isSleep) return 0;

  isSleep = true;
  OSI_logt("NFC can be going to sleep");
  return ioctl(pw_driver, SEC_NFC_SLEEP, 0);
}

int device_wakeup(void) {
  int ret = 0;
  if (!isSleep) return 0;

  isSleep = false;
  // [Start] Workaround - i2c write fail(self wakeup)
  first_wakeup = true;
  // [End] Workaround - i2c write fail(self wakeup)
  ret = ioctl(pw_driver, SEC_NFC_WAKEUP, 0);

  /* START [H16031401] */
  if (nfc_hal_info.state == HAL_STATE_SERVICE &&
      nfc_hal_info.msg_event == HAL_EVT_READ)
    return ret;
  /* END [H16031401] */
  OSI_logt("Wakeup! in %d ms", wakeup_delay);
  /* wakeup delay */
  OSI_delay(wakeup_delay);
  OSI_logt("exit");

  return ret;
}

int device_write(uint8_t* data, size_t len) {
  OSI_logt("enter");
  int ret = 0;
  int total = 0;
  int retry = 1;

  while (len != 0) {
    OSI_logt("before system call");
    ret = write(tr_driver, data + total, len);
    OSI_logt("after system call");
    if (ret < 0) {
      OSI_loge("write error ret = %d, errno = %d, retry = %d", ret, errno,
               retry);
      if (retry++ < 3 && (nfc_hal_info.flag & HAL_FLAG_RETRY_TRNS)) {
        // [Start] Workaround - i2c write fail(self wakeup)
        if ((retry == 2) && (first_wakeup == true)) {
          ret = ioctl(pw_driver, SEC_NFC_SLEEP, 0);
          OSI_delay(1);
          ret = ioctl(pw_driver, SEC_NFC_WAKEUP, 0);

          OSI_delay(wakeup_delay);
          first_wakeup = false;
          continue;
        }
        // [End] Workaround - i2c write fail(self wakeup)
        else {
          OSI_delay(5);
          continue;
        }
      }
      break;
    }
    total += ret;
    len -= ret;
  }

  if (len == 0) data_trace("Send", total, data);

  OSI_logt("exit");
  return total;
}

int device_read(uint8_t* buffer, size_t len) {
  int ret = 0;
  int total = 0;
  int retry = 1;

  // [I2c] Handles exceptions when consecutive IRQ occurred
  if (len == 0) {
    ret = read(tr_driver, buffer + total, len);
    if(ret == 0) {
      OSI_logt("payload is 0");
      return total;
    }
  }

  while (len != 0) {
    ret = read(tr_driver, buffer + total, len);
    if (ret <= 0) {
      OSI_loge("Read error ret = %d, errno = %d", ret, errno);
      if (retry++ < 3 && (nfc_hal_info.flag & HAL_FLAG_RETRY_TRNS)) continue;
      break;
    }

    total += ret;
    len -= ret;
  }

  return total;
}

void read_thread(void) {
  tOSI_QUEUE_HANDLER msg_que = NULL;
  tNFC_HAL_MSG* msg = NULL;
  fd_set rfds;
  uint8_t header[NCI_HDR_SIZE];
  int close_pipe[2];
  int max_fd;
  struct timeval tv;
  struct timeval* ptv = NULL;
  int ret;

  OSI_logt("enter");
  /* get msg que */
  msg_que = OSI_queue_get_handler("msg_q");
  if (!msg_que) {
    OSI_loge("Not find %s queue!! exit read thread", "msg_q");
    return;
  }

  /* closer */
  if (pipe(close_pipe) < 0) {
    OSI_loge("pipe open error for closing read thread");
    close_pipe[0] = 0;
    close_pipe[1] = 0;
    ptv = &tv;
  }
  tr_closer = close_pipe[1];
  max_fd = (close_pipe[0] > tr_driver) ? close_pipe[0] : tr_driver;

  while (OSI_task_isRun(read_task) == OSI_RUN) {
    pthread_mutex_lock(&tr_lock);
    if (tr_driver < 0) {
      pthread_mutex_unlock(&tr_lock);
      break;
    }
    FD_ZERO(&rfds);
    FD_SET(tr_driver, &rfds);
    pthread_mutex_unlock(&tr_lock);

    if (close_pipe[0] > 0) {
      FD_SET(close_pipe[0], &rfds);
    } else {
      tv.tv_sec = 0;
      tv.tv_usec = 2000;
    }

    ret = select(max_fd + 1, &rfds, NULL, NULL, ptv);

    pthread_mutex_lock(&tr_lock);
    if (tr_driver < 0) {
      pthread_mutex_unlock(&tr_lock);
      break;
    }
    pthread_mutex_unlock(&tr_lock);

    if (ret == 0) /* timeout */
      continue;
    else if (ret < 0 && errno == EINTR) /* signal received */
      continue;
    else if (ret < 0) {
      OSI_loge("Polling error");
      nfc_stack_cback(HAL_NFC_ERROR_EVT, HAL_NFC_STATUS_OK);
      break;
    }

    /* read 3 bytes (header)*/
    ret = device_read(header, NCI_HDR_SIZE);
    if (ret == 0)
      continue;
    else if (ret != NCI_HDR_SIZE) {
      OSI_loge("Reading NCI header failed");
      continue;
    }

    msg = (tNFC_HAL_MSG*)OSI_mem_get(NCI_CTRL_SIZE);
    if (!msg) {
      OSI_loge("Failed to allocate memory!1");
      nfc_stack_cback(HAL_NFC_ERROR_EVT, HAL_NFC_STATUS_OK);
      break;
    }

    /* payload will read upper layer */
    msg->event = HAL_EVT_READ;
    memcpy((void*)msg->param, (void*)header, NCI_HDR_SIZE);

    ret = OSI_queue_put(msg_que, (void*)msg);
    OSI_logd("Sent message to HAL message task, remind que: %d", ret);
  }

  close(close_pipe[0]);
  close(close_pipe[1]);
  tr_closer = 0;

  osi_unlock();  // TODO: why?

  OSI_logt("end;");
}

#define TRACE_BUFFER_SIZE (NCI_CTRL_SIZE * 3 + 1)
void data_trace(const char* head, int len, uint8_t* p_data) {
  int i = 0, header;
  char trace_buffer[TRACE_BUFFER_SIZE + 2];

  header = (dev_state == NFC_DEV_MODE_BOOTLOADER) ? 4 : 3;
  while (len-- > 0 && i < NCI_CTRL_SIZE) {
    if (i < header)
      sprintf(trace_buffer + (i * 3), "%02x   ", p_data[i]);
    else
      sprintf(trace_buffer + (i * 3 + 2), "%02x ", p_data[i]);
    i++;
  }

  if (log_ptr) OSI_logd(" %s(%3d) %s", head, i, trace_buffer);
}

#ifdef NFC_SEC_ESE_COLDRESET
void device_ese_coldreset(void) {
  OSI_logt("enter");
  int ret = 0;
  ret = ioctl(pw_driver, SEC_NFC_COLD_RESET, 0);
  if (ret < 0) {
    OSI_loge("ese cold reset error = %d", ret);
  }
  OSI_logt("exit");
}

void device_shutdown(void) {
  OSI_logt("enter");
  int ret = 0;
  ret = ioctl(pw_driver, SEC_NFC_SHUTDOWN, 0);
  if (ret < 0) {
      OSI_loge("set shutdown error = %d", ret);
  }
  OSI_logt("exit");
}
#endif
