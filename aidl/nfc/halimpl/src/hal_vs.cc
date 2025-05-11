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

#include <hardware/nfc.h>

#include <cutils/properties.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "hal.h"
#include "hal_msg.h"
#include "osi.h"
#include "util.h"

int hal_vs_nci_send(uint8_t oid, uint8_t* data, size_t size) {
  tNFC_NCI_PKT nci_pkt;
  nci_pkt.oct0 = NCI_MT_CMD | NCI_PBF_LAST | NCI_GID_PROP;
  nci_pkt.oid = oid;
  nci_pkt.len = size;

  if (data) memcpy(nci_pkt.payload, data, size);

  return hal_nci_send(&nci_pkt);
}

int hal_vs_get_prop(int n, tNFC_NCI_PKT* pkt) {
  char prop_field[15];

  pkt->oct0 = NCI_MT_CMD & NCI_GID_PROP & NCI_PBF_LAST;
  pkt->oid = get_config_propnci_get_oid(n);
  sprintf(prop_field, "NCI_PROP0x%02X", NCI_OID(pkt));
  pkt->len = get_config_byteArry(prop_field, pkt->payload, NCI_MAX_PAYLOAD);

  return (size_t)(pkt->len + NCI_HDR_SIZE);
}

bool hal_vs_get_rf_image(tNFC_HAL_VS_INFO* vs) {
  bool is_need_update = false;

  if (vs->binary != NULL) free(vs->binary);
  if (vs->is_dual_option_supported && vs->swreg_binary != NULL)
    free(vs->swreg_binary);

  if (check_force_fw_update_mode() == 3) {
    if (vs->is_dual_option_supported) {
      vs->binary = nfc_hal_get_update_image(IMAGE_TYPE_HW_REG,
                                            true, &is_need_update);
      vs->swreg_binary = nfc_hal_get_update_image(IMAGE_TYPE_SW_REG,
                                                  true, &is_need_update);
    } else {
      vs->binary = nfc_hal_get_update_image(IMAGE_TYPE_RF_REG,
                                            true, &is_need_update);
    }
  } else {
    if (vs->is_dual_option_supported) {
      vs->binary = nfc_hal_get_update_image(IMAGE_TYPE_HW_REG,
                                            false, &is_need_update);
      vs->swreg_binary = nfc_hal_get_update_image(IMAGE_TYPE_SW_REG,
                                                  false, &is_need_update);
    } else {
      vs->binary = nfc_hal_get_update_image(IMAGE_TYPE_RF_REG,
                                            false, &is_need_update);
    }
  }
  if ((!vs->is_dual_option_supported) && vs->binary == NULL) return false;

  if (vs->binary != NULL) {
    memcpy(&vs->rfreg_size, vs->binary + 1, vs->binary[0]);
    vs->rfreg = vs->binary + vs->binary[0] + 1;
  }

  if (vs->is_dual_option_supported) {
    if (vs->swreg_binary != NULL) {
      memcpy(&vs->swreg_size, vs->swreg_binary + 1, vs->swreg_binary[0]);
      vs->swreg = vs->swreg_binary + vs->swreg_binary[0] + 1;
    }
  }

  nfc_rf_getver_from_image(vs);

  return is_need_update;
}

void hal_vs_reset_rfreg_version(tNFC_HAL_VS_INFO* vs) {
  memset(vs->rfreg_version, 0, 8);
  memset(vs->swreg_version, 0, 8);
}

void hal_vs_set_rfreg_version(tNFC_HAL_VS_INFO* vs, tNFC_NCI_PKT* pkt) {
  tNFC_HAL_FW_BL_INFO* bl = &nfc_hal_info.fw_info.bl_info;
  if (SNFC_N74 <= bl->version[0]) {
    vs->rfreg_number_version[0] = NCI_PAYLOAD(pkt)[0] >> 4; // main version
    vs->rfreg_number_version[1] = NCI_PAYLOAD(pkt)[0] & (~(0xf << 4)); // sub version
    vs->rfreg_number_version[2] = NCI_PAYLOAD(pkt)[1]; // patch version
    vs->rfreg_number_version[3] = NCI_PAYLOAD(pkt)[11]; // minor version

    memcpy(vs->rfreg_version, NCI_PAYLOAD(pkt) + 5, 5);
    memcpy(vs->rfreg_version + 5, NCI_PAYLOAD(pkt) + 12, 3);

    if (vs->is_dual_option_supported) {
      vs->swreg_number_version[0] =
        NCI_PAYLOAD(pkt)[SWREG_VERSION_IDX + 0] >> 4; // main version
      vs->swreg_number_version[1] =
        NCI_PAYLOAD(pkt)[SWREG_VERSION_IDX + 0] & (~(0xf << 4)); // sub version
      vs->swreg_number_version[2] = NCI_PAYLOAD(pkt)[SWREG_VERSION_IDX + 1];
      vs->swreg_number_version[3] =
        NCI_PAYLOAD(pkt)[SWREG_VERSION_IDX + 11];  // minor version

      memcpy(vs->swreg_version, NCI_PAYLOAD(pkt) + SWREG_VERSION_IDX + 5, 5);
      memcpy(vs->swreg_version + 5, NCI_PAYLOAD(pkt)
             + SWREG_VERSION_IDX + 12, 3);
    }
  } else {
    memcpy(vs->rfreg_version, NCI_PAYLOAD(pkt) + 1, 8);
  }
}

bool hal_vs_check_rfreg_update(tNFC_HAL_VS_INFO* vs,
                               __attribute__((unused))tNFC_NCI_PKT* pkt) {
  tNFC_HAL_FW_BL_INFO* bl = &nfc_hal_info.fw_info.bl_info;
  char rfreg_date_version_buffer[30];
  char rfreg_num_version_buffer[10];
  char swreg_date_version_buffer[30];
  char swreg_num_version_buffer[10];
  char rfreg_csc_buffer[10];
  bool ret;
  // set rf register version to structure
  memset(rfreg_date_version_buffer, 0, 30);
  memset(rfreg_num_version_buffer, 0, 10);
  memset(swreg_date_version_buffer, 0, 30);
  memset(swreg_num_version_buffer, 0, 10);
  memset(rfreg_csc_buffer, 0, 10);

  sprintf(rfreg_date_version_buffer, "%d/%d/%d/%d.%d.%d",
          (vs->rfreg_version[0] >> 4) + 14, vs->rfreg_version[0] & 0xF,
          vs->rfreg_version[1], vs->rfreg_version[2], vs->rfreg_version[3],
          vs->rfreg_version[4]);
  if (vs->is_dual_option_supported) {
    OSI_logd("HW Version (F/W): %s", rfreg_date_version_buffer);
  } else {
    OSI_logd("RF Version (F/W): %s", rfreg_date_version_buffer);
  }

  if (vs->is_dual_option_supported) {
    sprintf(swreg_date_version_buffer, "%d/%d/%d/%d.%d.%d",
            (vs->swreg_version[0] >> 4) + 14, vs->swreg_version[0] & 0xF,
            vs->swreg_version[1], vs->swreg_version[2], vs->swreg_version[3],
            vs->swreg_version[4]);
    OSI_logd("SW Version (F/W) : %s", swreg_date_version_buffer);
  }

  ret = hal_vs_get_rf_image(vs);
  if (ret) {
    sprintf(rfreg_date_version_buffer, "%d/%d/%d/%d.%d.%d",
            (vs->rfreg_img_version[0] >> 4) + 14,
            vs->rfreg_img_version[0] & 0xF, vs->rfreg_img_version[1],
            vs->rfreg_img_version[2], vs->rfreg_img_version[3],
            vs->rfreg_img_version[4]);
    OSI_logd("RF Version (BIN): %s", rfreg_date_version_buffer);

    memcpy(vs->rfreg_version, vs->rfreg_img_version, 8);
    if (vs->is_dual_option_supported) {
      sprintf(swreg_date_version_buffer, "%d/%d/%d/%d.%d.%d",
              (vs->swreg_img_version[0] >> 4) + 14,
              vs->swreg_img_version[0] & 0xF, vs->swreg_img_version[1],
              vs->swreg_img_version[2], vs->swreg_img_version[3],
              vs->swreg_img_version[4]);
      OSI_logd("SW Version (BIN) : %s", swreg_date_version_buffer);
    }
  } else {
    OSI_loge("hal_vs_check_rfreg_update return false");
  }

  if (vs->is_dual_option_supported) {
    sprintf(rfreg_csc_buffer, "%c%c%c", vs->swreg_version[5],
            vs->swreg_version[6], vs->swreg_version[7]);
  } else {
    sprintf(rfreg_csc_buffer, "%c%c%c", vs->rfreg_version[5],
            vs->rfreg_version[6], vs->rfreg_version[7]);
  }

  // Set properties
  if (SNFC_N74 <= bl->version[0]) {
    if (ret)
      sprintf(rfreg_num_version_buffer, "%d", vs->rfreg_img_number_version[3]);
    else
      sprintf(rfreg_num_version_buffer, "%d", vs->rfreg_number_version[3]);

    if (vs->is_dual_option_supported) {
      if (ret)
        sprintf(swreg_num_version_buffer, "%d",
                vs->swreg_img_number_version[3]);
      else
        sprintf(swreg_num_version_buffer, "%d", vs->swreg_number_version[3]);
    }

    OSI_logd("RF Register Display Version : %s", rfreg_num_version_buffer);
    property_set("nfc.fw.rfreg_display_ver", rfreg_num_version_buffer);
  }
  OSI_logd("nfc.fw.rfreg_ver is %s", rfreg_date_version_buffer);
  property_set("nfc.fw.rfreg_ver", rfreg_date_version_buffer);

  if (vs->is_dual_option_supported) {
    OSI_logd("SW Register Display Version : %s", swreg_num_version_buffer);
    property_set("nfc.fw.swreg_display_ver", swreg_num_version_buffer);
    OSI_logd("nfc.fw.swreg_ver is %s", swreg_date_version_buffer);
    property_set("nfc.fw.swreg_ver", swreg_date_version_buffer);
  }

  property_set("nfc.fw.dfl_areacode", rfreg_csc_buffer);

  return ret;
}

bool hal_vs_merge_rf_image(tNFC_HAL_VS_INFO* vs) {
  uint8_t* merged_binary;
  merged_binary = (uint8_t *)malloc(vs->rfreg_size + vs->swreg_size);
  memcpy(merged_binary, vs->rfreg, vs->rfreg_size);
  memcpy(merged_binary + vs->rfreg_size, vs->swreg, vs->swreg_size);
  vs->merged_rfreg = merged_binary;
  vs->merged_rfreg_size = vs->rfreg_size + vs->swreg_size;
  return true;
}

#define RFREG_SECTION_SIZE 252
bool hal_vs_rfreg_update(tNFC_HAL_VS_INFO* vs) {
  uint8_t data[RFREG_SECTION_SIZE + 1];
  uint32_t* check_sum;
  size_t size;
  int total, next = vs->rfregid * RFREG_SECTION_SIZE;

  total = vs->rfreg_size;

  OSI_logd("Next / Total: %d / %d", next, total);

  if (total <= next) return false;

  if (total - next < RFREG_SECTION_SIZE)
    size = total - next;
  else
    size = RFREG_SECTION_SIZE;

  memcpy(data + 1, vs->rfreg + next, size);

  // checksum
  check_sum = (uint32_t *)(data + 1);
  for (int i = 1; i <= (int)size; i += 4) vs->check_sum += *check_sum++;

  data[0] = vs->rfregid;
  OSI_logd("Sent RF register #id: 0x%02X", vs->rfregid);
  vs->rfregid++;
  hal_vs_nci_send(NCI_PROP_SET_RFREG, data, size + 1);

  return true;
}

bool hal_vs_rfreg_update_dual_option(tNFC_HAL_VS_INFO* vs) {
  uint8_t data[RFREG_SECTION_SIZE + 2]; //sub oid, rfregid
  uint32_t* check_sum;
  size_t size;
  int total, next = vs->rfregid * RFREG_SECTION_SIZE;

  total = vs->merged_rfreg_size;

  OSI_logd("[Dual] Next / Total: %d / %d", next, total);

  if (total <= next) return false;

  if (total - next < RFREG_SECTION_SIZE)
    size = total - next;
  else
    size = RFREG_SECTION_SIZE;

  memcpy(data + 2, vs->merged_rfreg + next, size);

  // checksum
  check_sum = (uint32_t *)(data + 2);
  for (int i = 1; i <= (int)size; i += 4) vs->check_sum += *check_sum++;

  data[0] = NCI_PROP_DUAL_OPTION_SUB_OID_SET_OPTION;
  data[1] = vs->rfregid;
  OSI_logd("[Dual] Sent RF register #id: 0x%02X", vs->rfregid);
  vs->rfregid++;

  hal_vs_nci_send(NCI_PROP_DUAL_OPTION_OID, data, size + 1 + 1/*sub_oid*/);
  return true;
}

bool nfc_rf_getver_from_image(tNFC_HAL_VS_INFO* vs) {
  tNFC_HAL_FW_BL_INFO* bl = &nfc_hal_info.fw_info.bl_info;
  uint8_t value[16];
  int position = 16;  // Default position of version information

  memcpy(value, &vs->rfreg[vs->rfreg_size - position], 16);

  memcpy(vs->rfreg_img_version, value + 5, 5);    // Version information (time)
  if (!vs->is_dual_option_supported) {
    memcpy(vs->rfreg_img_version + 5, value + 12, 3);  // ID (csc code)
  }

  if (SNFC_N74 <= bl->version[0]) {
    nfc_hal_info.vs_info.rfreg_img_number_version[0] =
        value[0] >> 4;  // main version
    nfc_hal_info.vs_info.rfreg_img_number_version[1] =
        value[0] & (~(0xf << 4));  // sub version
    nfc_hal_info.vs_info.rfreg_img_number_version[2] =
        value[1];  // patch version
    nfc_hal_info.vs_info.rfreg_img_number_version[3] =
        value[11];  // minor version (number version)
  }

  if (vs->is_dual_option_supported) {
    memcpy(value, &vs->swreg[vs->swreg_size - position], 16);

    memcpy(vs->swreg_img_version, value + 5, 5);  // Version information (time)
    memcpy(vs->swreg_img_version + 5, value + 12, 3);  // ID (csc code)

    nfc_hal_info.vs_info.swreg_img_number_version[0] =
      value[0] >> 4;  // main version
    nfc_hal_info.vs_info.swreg_img_number_version[1] =
      value[0] & (~(0xf << 4));  // sub version
    nfc_hal_info.vs_info.swreg_img_number_version[2] =
      value[1];  // patch version
    nfc_hal_info.vs_info.swreg_img_number_version[3] =
      value[11];  // minor version (number version)
  }
  return true;
}
