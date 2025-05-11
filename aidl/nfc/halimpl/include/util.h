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
#ifndef __NFC_SEC_HALUTIL__
#define __NFC_SEC_HALUTIL__

#include "osi.h"

#ifndef __bool_true_false_are_defined
#define __bool_true_false_are_defined
typedef enum { false, true } bool;
#endif

#define HAL_UTIL_GET_INT_16 0x0001

bool get_config_int(const char* field, int* data);
int get_config_byteArry(const char* field, uint8_t* byteArry, size_t arrySize);
int get_config_string(const char* field, char* strBuffer, size_t bufferSize);
int get_config_count(const char* field);
uint8_t get_config_propnci_get_oid(int n);
int get_hw_rev();

#ifdef NFC_HAL_NCI_TRACE
#define util_nci_analyzer(x) sec_nci_analyzer(x)
#else
#define util_nci_analyzer(x)
#endif

#endif  //__NFC_SEC_HALUTIL__
