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
 *
 */

#include "hal.h"
#include "osi.h"

int device_init(int data_trace);
void device_deinit(void);
int device_open();
void device_close(void);
int device_set_mode(eNFC_DEV_MODE mode);
int device_sleep(void);
int device_wakeup(void);
int device_write(uint8_t* data, size_t len);
int device_read(uint8_t* buffer, size_t len);
void data_trace(const char* head, int len, uint8_t* p_data);
#ifdef NFC_SEC_ESE_COLDRESET
void device_ese_coldreset(void);
void device_shutdown(void);
#endif
