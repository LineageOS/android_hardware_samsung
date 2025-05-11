/*
 *    Copyright (C) 2016 SAMSUNG S.LSI
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on n "S IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "hal.h"
#include "osi.h"
#include "util.h"

char MAGIC_NUMBER[] = {0x53, 0x52, 0x6F, 0x62};
// RF register - special information length
#define RFREG_META_FW_VERSION_LENGTH 2
#define RFREG_META_PROJECT_NO_LENGTH 1
#define RFREG_META_RF_VERSION_LENGTH 2
#define RFREG_META_BUILD_TIME_LENGTH 5
#define RFREG_META_BRANCH_NO_LENGTH 1
#define RFREG_META_MINOR_VERSION_LENGTH 1
#define RFREG_META_TARGET_INFOR_LENGTH 3
#define RFREG_META_RFU_LENGTH 1
#define RFREG_META_DATA_LENGTH 16

// RF register - special information address
#define RFREG_META_FW_VERSION 0
#define RFREG_META_PROJECT_NO \
  ((RFREG_META_FW_VERSION) + (RFREG_META_FW_VERSION_LENGTH))
#define RFREG_META_RF_VERSION \
  ((RFREG_META_PROJECT_NO) + (RFREG_META_PROJECT_NO_LENGTH))
#define RFREG_META_BUILDTIME \
  ((RFREG_META_RF_VERSION) + (RFREG_META_RF_VERSION_LENGTH))
#define RFREG_META_BRANCH_NO \
  ((RFREG_META_BUILDTIME) + (RFREG_META_BUILD_TIME_LENGTH))
#define RFREG_META_MINOR_VERSION \
  ((RFREG_META_BRANCH_NO) + (RFREG_META_BRANCH_NO_LENGTH))
#define RFREG_META_TARGET_INFOR \
  ((RFREG_META_MINOR_VERSION) + (RFREG_META_MINOR_VERSION_LENGTH))
#define RFREG_META_RFU \
  ((RFREG_META_TARGET_INFOR) + (RFREG_META_TARGET_INFOR_LENGTH))

// RF binary field's length
#define RF_MAGIC_NUMBER_LENGTH sizeof(MAGIC_NUMBER)
#define RF_FILE_SIZE_LENGTH 4
#define RF_VERSION_LENGTH 6
#define RF_OPTION_LENGTH 1
#define RF_BASE_REG_SIZE_LENGTH 2

// RF binary field's address
#define RF_MAGIC_NUMBER 0
#define RF_FILE_SIZE ((RF_MAGIC_NUMBER) + (RF_MAGIC_NUMBER_LENGTH))

typedef struct {
  // common fields
  FILE* file;
  int file_size;
  struct {
    uint8_t file_version[RF_VERSION_LENGTH];
    uint8_t file_options[RF_OPTION_LENGTH];
    int reg_size;
    int reg_start_addr;
  };
} tNFC_RF_BINARY;

// Register handle
static uint8_t* get_meta_data(uint8_t* reg, int size);
static void set_id_to_rf_reg(uint8_t* reg, int size, uint8_t* id);

// File operations
static FILE* find_and_open_rf_reg();
static tNFC_RF_BINARY get_binary_information(FILE* file);

// Temporary functions
uint8_t* tmp_reg_id = NULL;
uint8_t* nfc_hal_rf_get_reg_id_tmp(void) {
  OSI_logd("ID: %s", tmp_reg_id);
  return tmp_reg_id;
}

void nfc_hal_rf_set_reg_id_tmp(uint8_t* new_id) {
  tmp_reg_id = new_id;
  OSI_logd("ID: %s", tmp_reg_id);
}

char* tmp_file_name = NULL;
uint8_t tmp_file_option = 0;
char* nfc_hal_rf_get_file_name_tmp(void) {
  OSI_logd("file name: %s", tmp_file_name);
  return tmp_file_name;
}

void nfc_hal_rf_set_file_tmp(char* new_file_name, uint8_t option) {
  tmp_file_name = new_file_name;
  tmp_file_option = option;
  OSI_logd("file name: %s", tmp_file_name);
  OSI_logd("file option: %d", tmp_file_option);
}

void nfc_hal_rf_set_id(uint8_t* reg, int size, uint8_t* id) {
  set_id_to_rf_reg(reg, size, id);
}
// End temp

// APIs
int nfc_hal_rf_get_reg(uint8_t* reg_buffer,
                       __attribute__((unused)) int reg_buffer_size) {
  tNFC_RF_BINARY binary;
  FILE* file;

  if (reg_buffer == NULL) return -1;

  if ((file = find_and_open_rf_reg()) == NULL) {
    OSI_loge("Cannot open rf register file!!");
    return -1;
  }

  binary = get_binary_information(file);
  OSI_logd("file size: %d", binary.file_size);
  {
    OSI_logd("This is normal rf register file");
    if (fseek(file, 0, SEEK_SET) < 0) goto fail;

    if (fread(reg_buffer, 1, binary.file_size, file) !=
        (size_t)binary.file_size)
      goto fail;

    binary.reg_size = binary.file_size;
  }

  // Set ID to META data
  OSI_logd("Write ID to meta data");
  set_id_to_rf_reg(reg_buffer, binary.reg_size, nfc_hal_rf_get_reg_id_tmp());

  fclose(file);
  return binary.reg_size;
fail:
  OSI_loge("failed to get RF register set");
  fclose(file);
  return -1;
}

/****************************
 * Register handle
 ***************************/
static uint8_t* get_meta_data(uint8_t* reg, int size) {
  return reg + (size - RFREG_META_DATA_LENGTH);
}

static void set_id_to_rf_reg(uint8_t* reg, int size, uint8_t* id) {
  uint8_t* meta = NULL;
  if (reg == NULL || size < RFREG_META_DATA_LENGTH || id == NULL) return;

  meta = get_meta_data(reg, size);
  memcpy(meta + RFREG_META_TARGET_INFOR, id, RFREG_META_TARGET_INFOR_LENGTH);
}

/****************************
 * File operations
 ***************************/
static FILE* find_and_open_rf_reg() {
  FILE* file;
  struct stat attrib;
  // char file_name[256];
  char* file_name = NULL;

  // get rf reg
  /*
  if (!get_config_string(cfg_name_table[CFG_RFREG_FILE], file_name,
  sizeof(file_name)))
      return NULL;
  */
  file_name = nfc_hal_rf_get_file_name_tmp();

  // is exist
  if (stat(file_name, &attrib) != 0) return NULL;

  // Open RF binary
  if ((file = fopen(file_name, "rb")) == NULL) return NULL;

  return file;
}

static tNFC_RF_BINARY get_binary_information(FILE* file) {
  tNFC_RF_BINARY new_binary;
  uint8_t buffer[18];
  memset(&new_binary, 0, sizeof(new_binary));

  if (file == NULL) return new_binary;

  new_binary.file = file;

  // Get header information
  if (fseek(file, 0, SEEK_SET) < 0) return new_binary;

  if (fread(buffer, 1, 18, file) != 18)
    return new_binary;

  // Get size
  if (fseek(file, 0, SEEK_END) < 0) return new_binary;
  new_binary.file_size = (int)ftell(file);

  return new_binary;
}

