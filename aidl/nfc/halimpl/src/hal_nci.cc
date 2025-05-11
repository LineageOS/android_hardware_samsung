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
#include <string.h>

#include "device.h"
#include "hal.h"
#include "hal_msg.h"
#include "osi.h"
#include "util.h"
#include "config.h"

int hal_nci_send(tNFC_NCI_PKT* pkt) {
  size_t len = (size_t)(pkt->len + NCI_HDR_SIZE);
  int ret;

  ret = __send_to_device((uint8_t*)pkt, len);
  if (ret != (int)len
      /* workaround for retry; F/W I2C issue */
      && (nfc_hal_info.flag & HAL_FLAG_NTF_TRNS_ERROR)) {
    OSI_loge("NCI message send failed");
    OSI_logd("set flag to 0x%06X", nfc_hal_info.flag);
  } else {
    util_nci_analyzer(pkt);
  }

  return ret;
}

void hal_nci_send_reset(void) {
  tNFC_NCI_PKT nci_pkt;

  memset(&nci_pkt, 0, sizeof(tNFC_NCI_PKT));
  nci_pkt.oct0 = NCI_MT_CMD | NCI_PBF_LAST | NCI_GID_CORE;
  nci_pkt.oid = NCI_CORE_RESET;
  nci_pkt.len = 0x01;
  nci_pkt.payload[0] = 0x01;  // Reset config

  hal_nci_send(&nci_pkt);
}

/* START [181106] Patch for supporting NCI v2.0 */
// [3. CORE_INIT Changes]
void hal_nci_send_init(int version) {
  /* END [181106] Patch for supporting NCI v2.0 */
  tNFC_NCI_PKT nci_pkt;

  /* START [181106] Patch for supporting NCI v2.0 */
  //[3. CORE_INIT Changes]
  memset(&nci_pkt, 0, sizeof(tNFC_NCI_PKT));
  nci_pkt.oct0 = NCI_MT_CMD | NCI_PBF_LAST | NCI_GID_CORE;
  nci_pkt.oid = NCI_CORE_INIT;
  nci_pkt.len = 0x00;

  if (version == NCI_VER_2_0) {
    nci_pkt.len = 0x02;
    nci_pkt.payload[0] = 0x00;
    nci_pkt.payload[1] = 0x00;
  }
  /* END [181106] Patch for supporting NCI v2.0 */

  hal_nci_send(&nci_pkt);
}

/* Workaround: Initialization flash of LMRT */
void hal_nci_send_clearLmrt(void) {
  tNFC_NCI_PKT nci_pkt;

  memset(&nci_pkt, 0, sizeof(tNFC_NCI_PKT));
  nci_pkt.oct0 = NCI_MT_CMD | NCI_PBF_LAST | NCI_GID_RF_MANAGE;
  nci_pkt.oid = 0x01;  // RF_SET_LMRT
  nci_pkt.len = 0x02;
  nci_pkt.payload[0] = 0x00;
  nci_pkt.payload[1] = 0x00;

  hal_nci_send(&nci_pkt);
}
/* END WA */

void hal_nci_send_setSWAPITrace(uint8_t* option, unsigned int len) {
  tNFC_NCI_PKT nci_pkt;
  int i = 0;

  memset(&nci_pkt, 0, sizeof(tNFC_NCI_PKT));
  nci_pkt.oct0 = NCI_MT_CMD | NCI_PBF_LAST | NCI_GID_PROP;
  nci_pkt.oid = 0x17;
  nci_pkt.len = len;

  for (i = 0; i < len; i++) {
    nci_pkt.payload[i] = option[i];
  }

  hal_nci_send(&nci_pkt);
}

void get_clock_info(int rev, int field_name, int* buffer) {
  char rev_field[50] = {
      '\0',
  };
  int isRevField = 0;

  sprintf(rev_field, "%s_REV%d", cfg_name_table[field_name], rev);
  isRevField = get_config_count(rev_field);
  if (rev < 0 || isRevField == 0) {
    if (!GetNumValue(cfg_name_table[field_name], buffer, 4))
            *buffer = 0;
  } else {
        if (!GetNumValue(rev_field, buffer, 4))
            *buffer = 0;
  }
}

void hal_nci_send_prop_fw_cfg(uint8_t product) {
  tNFC_NCI_PKT nci_pkt;
  int rev = get_hw_rev();

  memset(&nci_pkt, 0, sizeof(tNFC_NCI_PKT));
  nci_pkt.oct0 = NCI_MT_CMD | NCI_PBF_LAST | NCI_GID_PROP;
  nci_pkt.oid = NCI_PROP_FW_CFG;

  if (product >= SNFC_N7) {
    nci_pkt.len = 0x01;
    get_clock_info(rev, CFG_FW_CLK_SPEED, (int *)&nci_pkt.payload[0]);
    if (nci_pkt.payload[0] == 0xff)
      OSI_loge("Set a different value! Current Clock Speed Value : 0x%x",
               nci_pkt.payload[0]);
  } else {
    nci_pkt.len = 0x03;
    get_clock_info(rev, CFG_FW_CLK_TYPE, (int *)&nci_pkt.payload[0]);
    get_clock_info(rev, CFG_FW_CLK_SPEED, (int *)&nci_pkt.payload[1]);
    get_clock_info(rev, CFG_FW_CLK_REQ, (int *)&nci_pkt.payload[2]);
  }
  hal_nci_send(&nci_pkt);
}

int nci_read_payload(tNFC_HAL_MSG* msg) {
  tNFC_NCI_PKT* pkt = &msg->nci_packet;
  int ret;

  ret = device_read(NCI_PAYLOAD(pkt), NCI_LEN(pkt));
  if (ret != (int)NCI_LEN(pkt)) {
    OSI_mem_free((tOSI_MEM_HANDLER)msg);
    OSI_loge("Failed to read payload");
    return ret;
  }

  data_trace("Recv", NCI_HDR_SIZE + ret, msg->param);
  return ret;
}

void nci_init_timeout(__attribute__((unused)) void* param) {
  tNFC_HAL_FW_INFO* fw = &nfc_hal_info.fw_info;
  tNFC_HAL_FW_BL_INFO* bl = &fw->bl_info;

  OSI_loge("NCI_INIT_RSP timeout!!");
  OSI_logd("Try send clk config!");
  device_set_mode(NFC_DEV_MODE_OFF);
  device_set_mode(NFC_DEV_MODE_ON);
  nfc_hal_info.state = HAL_STATE_FW;
  nfc_hal_info.fw_info.state = FW_W4_NCI_PROP_FW_CFG;
  hal_nci_send_prop_fw_cfg(bl->product);
}

bool nfc_hal_prehandler(tNFC_NCI_PKT* pkt) {
  if (NCI_MT(pkt) == NCI_MT_NTF) {
    if (NCI_GID(pkt) == NCI_GID_PROP) {
      /* Again procedure. only for N3 isN3group */
      if (NCI_OID(pkt) == NCI_PROP_AGAIN) {
        if (nfc_hal_info.nci_last_pkt) {
          OSI_logd("NFC requests sending last message again!");
          hal_update_sleep_timer();
          device_write((uint8_t*)nfc_hal_info.nci_last_pkt,
                       (size_t)(nfc_hal_info.nci_last_pkt->len
                       + NCI_HDR_SIZE));
          return false;
        }
      }
    }
  }

  if (NCI_MT(pkt) == NCI_MT_CMD) {
    if (NCI_GID(pkt) == NCI_GID_PROP) {
      if (NCI_OID(pkt) == NCI_PROP_WR_RESET) {
        hal_nci_send_reset();
        nfc_hal_info.flag |= HAL_FLAG_PROP_RESET;
        return false;
      }

      if (NCI_OID(pkt) == NCI_PROP_SET_SLEEP_TIME) {
        tNFC_NCI_PKT dummy_rsp;
        dummy_rsp.oct0 = NCI_MT_RSP | NCI_PBF_LAST | NCI_GID_PROP;
        dummy_rsp.oid = NCI_OID(pkt);
        dummy_rsp.len = 1;
        dummy_rsp.payload[0] = NCI_STATUS_OK;

        if (NCI_LEN(pkt) == 0) {
          setSleepTimeout(SET_SLEEP_TIME_CFG, 5000);
        } else {
          uint32_t timeout = NCI_PAYLOAD(pkt)[0] * 1000;  // sec
          int option = SET_SLEEP_TIME_ONCE;

          if (NCI_LEN(pkt) > 1)
            timeout += NCI_PAYLOAD(pkt)[1] * 1000 * 60;  // min

          if (NCI_LEN(pkt) > 2) option = NCI_PAYLOAD(pkt)[2];

          setSleepTimeout(option, timeout);
        }

        hal_update_sleep_timer();
        nfc_data_callback(&dummy_rsp);
        return false;
      }
    }
  }

  if (NCI_MT(pkt) == NCI_MT_RSP) {
    if (NCI_GID(pkt) == NCI_GID_CORE) {
      if (NCI_OID(pkt) == NCI_CORE_RESET) {
        pkt->oct0 = NCI_MT_RSP | NCI_PBF_LAST | NCI_GID_PROP;
        pkt->oid = NCI_PROP_WR_RESET;
      }
    }
  }
  return true;
}
