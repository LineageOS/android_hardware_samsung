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

#define SEC_NFC_DRIVER_NAME "sec-nfc"

/* ioctl */
#define SEC_NFC_MAGIC 'S'
#define SEC_NFC_GET_MODE _IOW(SEC_NFC_MAGIC, 0, unsigned int)
#define SEC_NFC_SET_MODE _IOW(SEC_NFC_MAGIC, 1, unsigned int)
#define SEC_NFC_SLEEP _IOW(SEC_NFC_MAGIC, 2, unsigned int)
#define SEC_NFC_WAKEUP _IOW(SEC_NFC_MAGIC, 3, unsigned int)
#define SEC_NFC_SET_WPT_MODE _IOW(SEC_NFC_MAGIC, 4, unsigned int)
#ifdef NFC_SEC_ESE_COLDRESET
#define SEC_NFC_COLD_RESET _IOW(SEC_NFC_MAGIC, 5, unsigned int)
#define SEC_NFC_SHUTDOWN _IOW(SEC_NFC_MAGIC, 6, unsigned int)
#endif
