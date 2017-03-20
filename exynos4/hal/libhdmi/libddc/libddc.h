/*
 * Copyright@ Samsung Electronics Co. LTD
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

#ifndef _LIBDDC_H_
#define _LIBDDC_H_

#ifdef __cplusplus
extern "C" {
#endif

int DDCOpen();
int DDCRead(unsigned char addr, unsigned char offset, unsigned int size, unsigned char* buffer);
int DDCWrite(unsigned char addr, unsigned char offset, unsigned int size, unsigned char* buffer);
int EDDCRead(unsigned char segpointer, unsigned char segment, unsigned char addr,
  unsigned char offset, unsigned int size, unsigned char* buffer);
int DDCClose();

#ifdef __cplusplus
}
#endif

#endif /* _LIBDDC_H_ */
