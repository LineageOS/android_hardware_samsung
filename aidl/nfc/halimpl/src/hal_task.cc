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
#include <malloc.h>
#include <string.h>

#include "device.h"
#include "hal.h"
#include "hal_msg.h"
#include "osi.h"
#include "util.h"

#include <cutils/properties.h>

uint32_t fw_update_state = 0;
int nfc_hal_nci_version = NCI_VER_1_0;         // 0x10 : NCI 1.0, 0x20 : NCI2.0

static void nfc_hal_state_switch(tNFC_HAL_MSG* msg, eHAL_STATE state) {
  tNFC_HAL_MSG* new_msg;

  new_msg = (tNFC_HAL_MSG*)OSI_mem_get(HAL_EVT_SIZE);
  if (!new_msg) {
    OSI_loge("Failed to memory allocate!");
    nfc_stack_cback(HAL_NFC_ERROR_EVT, HAL_NFC_STATUS_OK);
    return;
  }

  nfc_hal_info.state = state;
  memcpy(new_msg, msg, sizeof(HAL_EVT_SIZE));
  OSI_queue_put(nfc_hal_info.msg_q, (void*)new_msg);
}

void hal_sleep(__attribute__((unused)) void* param) {
  nfc_hal_info.flag &= ~HAL_FLAG_PROP_ONE_TIMER;
  nfc_hal_info.cfg.override_timeout = 0;
  device_sleep();
}

void hal_update_sleep_timer(void) {
  device_wakeup();

  /* workaround for double timer */
  if (nfc_hal_info.flag & HAL_FLAG_MASK_USING_TIMER) return;

  if (nfc_hal_info.flag & HAL_FLAG_PROP_ONE_TIMER)
    OSI_timer_start(nfc_hal_info.sleep_timer,
                    nfc_hal_info.cfg.override_timeout,
                    (tOSI_TIMER_CALLBACK)hal_sleep, NULL);
  else
    OSI_timer_start(nfc_hal_info.sleep_timer, nfc_hal_info.cfg.sleep_timeout,
                    (tOSI_TIMER_CALLBACK)hal_sleep, NULL);
}

int __send_to_device(uint8_t* data, size_t len) {
  hal_update_sleep_timer();
  if (nfc_hal_info.nci_last_pkt)
    memcpy((void*)nfc_hal_info.nci_last_pkt, (void*)data, len);

  return device_write(data, len);
}

int check_force_fw_update_mode() {
  uint8_t force_update_mode = 0;
  char check_update[PROPERTY_VALUE_MAX] = {0};

  if (property_get("nfc.fw.downloadmode_force", check_update, ""))
    sscanf(check_update, "%s", &force_update_mode);

  OSI_logd("force_update_mode : %d", force_update_mode);

  return (uint8_t)force_update_mode;
}

#ifdef NFC_SEC_ESE_COLDRESET
uint8_t* get_bootparam_for_coldreset() {
  extern int wakeup_delay;
  int ese_delay;
  int param;
  static uint8_t bootParam[7] = {0x00,};

  if (!get_config_int("ESE_DELAY", &ese_delay))
    ese_delay = wakeup_delay;
  bootParam[0] = ese_delay >> 8;
  bootParam[1] = ese_delay;
  if (!get_config_int("CP_TRIGGER_TYPE", &param))
    bootParam[2] = 0x01;
  else
    bootParam[2] = param;
  if (!get_config_int("CP_DEFAULT_TYPE", &param))
    bootParam[3] = 0x01;
  else
    bootParam[3] = param;
  if (!get_config_int("COLDRESET_SUPPORT", &param))
    bootParam[4] = 0x01;
  else
    bootParam[4] = param;
  if (!get_config_int("AP_COLDRESET_ENABLE", &param))
    bootParam[5] = 0x01;
  else
    bootParam[5] = param;
  if (!get_config_int("CP_COLDRESET_ENABLE", &param))
    bootParam[6] = 0x01;
  else
    bootParam[6] = param;

  return bootParam;
}

static bool isBootCmdSent = false;
#endif

void nfc_hal_open_sm(tNFC_HAL_MSG* msg) {
  tNFC_FW_PKT* fw_pkt = &msg->fw_packet;
  tNFC_HAL_FW_INFO* fw = &nfc_hal_info.fw_info;
  tNFC_HAL_FW_BL_INFO* bl = &fw->bl_info;
  tNFC_HAL_VS_INFO* vs = &nfc_hal_info.vs_info;
  size_t ret;
#ifdef NFC_SEC_ESE_COLDRESET
  uint8_t* bootParams;
#endif

  switch (msg->event) {
    case HAL_EVT_OPEN:
#ifndef NFC_HAL_DO_NOT_USE_BOOTLOADER
      device_set_mode(NFC_DEV_MODE_BOOTLOADER);
      OSI_logd("Get Bootloader information.");
#ifdef NFC_SEC_ESE_COLDRESET
      OSI_logd("Set eSE delay time for cold reset");
      bootParams = get_bootparam_for_coldreset();
      ret = nfc_fw_send_cmd(FW_CMD_GET_BOOTINFO, bootParams, 7);
#else
      ret = nfc_fw_send_cmd(FW_CMD_GET_BOOTINFO, NULL, 0);
#endif
      break;
    case HAL_EVT_READ:
#ifdef NFC_SEC_ESE_COLDRESET
      if (isBootCmdSent == false) {
#endif
        fw_read_payload(msg);
        if (fw_pkt->code != FW_RET_SUCCESS) {
          OSI_loge("Failed to get bootloader information. Return code: 0x%02X",
                   fw_pkt->code);
          nfc_stack_cback(HAL_NFC_OPEN_CPLT_EVT, HAL_NFC_STATUS_FAILED);
          break;
        }

        // store bootloader information from received data packet.
        memcpy(bl->version, fw_pkt->payload, sizeof(bl->version));
        FROM_LITTLE_ARRY(bl->sector_size, &fw_pkt->payload[4], 2);
        FROM_LITTLE_ARRY(bl->page_size, &fw_pkt->payload[6], 2);
        FROM_LITTLE_ARRY(bl->frame_max_size, &fw_pkt->payload[8], 2);
        FROM_LITTLE_ARRY(bl->hw_buffer_size, &fw_pkt->payload[10], 2);
        // search product information for using bootloader.
        bl->product = product_map(bl->version, &bl->base_address);
        OSI_logd(
            "Bootloader: Version %02x.%02x.%02x.%02x(N%d), Sector size: %d, "
            "Page size: %d",
            bl->version[0], bl->version[1], bl->version[2], bl->version[3],
            (bl->product >> 8), bl->sector_size, bl->page_size);

        if (bl->product != SNFC_UNKNOWN) {
          OSI_logd("bl->product 0x%03X!!: %s", bl->product,
                   get_product_name(bl->product));
        } else {
          OSI_logd("F/W Direct Mode!");

          // Get Base Address from libnfc-sec-hal.conf file.
          if (get_config_int(cfg_name_table[CFG_FW_BASE_ADDRESS],
                             (int *) &bl->base_address)) {
            OSI_logd("F/W Base Address : 0x%x", bl->base_address);
          } else {
            msg->event = HAL_EVT_COMPLETE_FAILED;
            nfc_hal_state_switch(msg, HAL_STATE_OPEN);
            OSI_loge("Doesn't support 'F/W Direct Mode'.");
            break;
          }
        }

        if (SNFC_N74 <= bl->version[0]) {
          FROM_LITTLE_ARRY(bl->base_address, &fw_pkt->payload[12], 2);
        }

        if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])) {
          vs->is_dual_option_supported = true;
        } else {
          vs->is_dual_option_supported = false;
        }

        ret = 0;
        if (get_config_int(cfg_name_table[CFG_UPDATE_MODE], (int *)&ret))
          OSI_logd("F/W Update mode is %d", (int)ret);

        /*force update for libnfc-sec-hal.conf */
        if ((ret >= FW_UPDATE_BY_FORCE_VER) ||
            (check_force_fw_update_mode() == 1)) {
          nfc_hal_info.state = HAL_STATE_POSTINIT;
          nfc_hal_info.flag &= ~HAL_FLAG_FORCE_FW_UPDATE;
          OSI_logd("NFC need to update FW!(force update)");
          fw_force_update(NULL);
          break;
        }
#ifdef NFC_SEC_ESE_COLDRESET
      } else { /* isBootCmdSent */
        fw_read_payload(msg);
      }
#endif

#ifdef NFC_SEC_ESE_COLDRESET
      if (isBootCmdSent == false) {
        isBootCmdSent = true;
        OSI_logd("Send coldreset disable command");
        ret = nfc_fw_send_cmd(FW_CMD_DISABLE_COMBO_COLDRESET, NULL, 0);
        OSI_timer_start(nfc_hal_info.nci_timer, 1000,
                        (tOSI_TIMER_CALLBACK)NULL, NULL);
        break;
      }
      isBootCmdSent = false;
#endif

      /*force update for libnfc-sec-hal.conf */
      device_set_mode(NFC_DEV_MODE_ON);

      if (bl->product < SNFC_N7) {
        hal_nci_send_reset();
        OSI_timer_start(nfc_hal_info.nci_timer, 1000,
                        (tOSI_TIMER_CALLBACK)fw_force_update, NULL);
      } else {
        nfc_hal_info.state = HAL_STATE_FW;
        nfc_hal_info.fw_info.state = FW_W4_NCI_PROP_FW_CFG;
        nfc_hal_info.flag |= HAL_FLAG_CLK_SET;

        /* Clock Rewrite support to N7. */
        hal_nci_send_prop_fw_cfg(bl->product);
        OSI_timer_start(nfc_hal_info.nci_timer, 1000,
                        (tOSI_TIMER_CALLBACK)fw_force_update, NULL);
      }
      break;

    case HAL_EVT_COMPLETE:
#endif
      device_set_mode(NFC_DEV_MODE_ON);
      nfc_hal_info.state = HAL_STATE_POSTINIT;
      nfc_stack_cback(HAL_NFC_OPEN_CPLT_EVT, HAL_NFC_STATUS_OK);
      break;

    case HAL_EVT_COMPLETE_FAILED:
      device_set_mode(NFC_DEV_MODE_OFF);
      nfc_stack_cback(HAL_NFC_OPEN_CPLT_EVT, HAL_NFC_STATUS_FAILED);
      break;

    case HAL_EVT_TERMINATE:
      // TODO: terminate
      break;
    default:
      break;
  }
}

void nfc_hal_postinit_sm(tNFC_HAL_MSG* msg) {
  tNFC_NCI_PKT* pkt = &msg->nci_packet;
  tNFC_HAL_FW_INFO* fw = &nfc_hal_info.fw_info;
  tNFC_HAL_FW_BL_INFO* bl = &fw->bl_info;
  tNFC_HAL_VS_INFO* vs = &nfc_hal_info.vs_info;

  switch (msg->event) {
    case HAL_EVT_CORE_INIT:
      if(!(nfc_hal_info.flag & HAL_FLAG_ALREADY_INIT)) {
        nfc_stack_cback(HAL_NFC_POST_INIT_CPLT_EVT, HAL_NFC_STATUS_FAILED);
        break;
      }
      //AOSP stack passes an altered init param, it loses the fw info.
      if (nfc_hal_nci_version != NCI_VER_2_0) {
       // Set FW version to system
        memcpy(&fw->fw_ver, NCI_MF_INFO(pkt), NCI_MF_INFO_SIZE);
        select_image_set_fw_version(NCI_MF_INFO(pkt), NCI_MF_INFO_SIZE - 1);
      }
      // Get RF version from the NFC chip
      if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])) {
        uint8_t sub_oid = NCI_PROP_DUAL_OPTION_SUB_OID_GET_VER;
        hal_vs_nci_send(NCI_PROP_DUAL_OPTION_OID , &sub_oid , 1);
      } else if (SNFC_N74 < bl->version[0] && SNFC_N84 > bl->version[0])
        hal_vs_nci_send(NCI_PROP_GET_OPTION_META_OID, NULL, 0);
      else
        hal_vs_nci_send(NCI_PROP_GET_RFREG_VER, NULL, 0);
      /* [201603] End */

      break;

    case HAL_EVT_WRITE:
      if (NCI_GID(pkt) == NCI_GID_CORE) {
        if (NCI_OID(pkt) == NCI_CORE_RESET && NCI_LEN(pkt) == 1) {
          if (nfc_hal_info.flag & HAL_FLAG_ALREADY_RESET) goto complete;

          nfc_hal_info.flag |= HAL_FLAG_W4_CORE_RESET_RSP;
          OSI_timer_start(nfc_hal_info.nci_timer, 1000,
                          (tOSI_TIMER_CALLBACK)fw_force_update, NULL);
          OSI_logd("set flag to 0x%06X", nfc_hal_info.flag);
        } else if (NCI_OID(pkt) == NCI_CORE_INIT &&
                   (NCI_LEN(pkt) == 0 || NCI_LEN(pkt) == 2)) {
          if (nfc_hal_info.flag & HAL_FLAG_ALREADY_INIT) goto complete;

          nfc_hal_info.flag |= HAL_FLAG_W4_CORE_INIT_RSP;
          OSI_timer_start(nfc_hal_info.nci_timer, 1000,
                          (tOSI_TIMER_CALLBACK)nci_init_timeout, NULL);
          OSI_logd("set flag to 0x%06X", nfc_hal_info.flag);
        }
      }
      hal_nci_send(&msg->nci_packet);
      break;

    case HAL_EVT_READ:
      nci_read_payload(msg);
      if (NCI_GID(pkt) == NCI_GID_CORE) {
        if (NCI_OID(pkt) == NCI_CORE_RESET) {
          OSI_logd("Respond CORE_RESET_RSP");
          nfc_hal_info.flag &= ~HAL_FLAG_W4_CORE_RESET_RSP;
          nfc_hal_info.flag |= HAL_FLAG_ALREADY_RESET;
          // nfc_hal_nci_version: 0x10 : NCI1.0, 0x20 : NCI2.0
          if ((NCI_LEN(pkt) == 0x03) && NCI_MT(pkt) == NCI_MT_RSP) {
            nfc_hal_nci_version = NCI_VER_1_0;
          } else {
            nfc_hal_nci_version = NCI_VER_2_0;
            memcpy(&fw->fw_ver, NCI_MF_INFO(pkt), NCI_MF_INFO_SIZE);
            select_image_set_fw_version(NCI_MF_INFO(pkt),
                                        NCI_MF_INFO_SIZE - 1);
          }
        } else if (NCI_OID(pkt) == NCI_CORE_INIT) {
          OSI_logd("Respond CORE_INIT_RSP");
          nfc_hal_info.flag &= ~HAL_FLAG_W4_CORE_INIT_RSP;
          nfc_hal_info.flag |= HAL_FLAG_ALREADY_INIT;
          if (nfc_hal_nci_version == NCI_VER_1_0) {
            memcpy(&fw->fw_ver, NCI_MF_INFO(pkt), NCI_MF_INFO_SIZE);
            select_image_set_fw_version(NCI_MF_INFO(pkt),
                                        NCI_MF_INFO_SIZE - 1);
          }
        }
        OSI_timer_stop(nfc_hal_info.nci_timer);
      }
      else if (NCI_GID(pkt) == NCI_GID_PROP) {
        if ((NCI_OID(pkt) == NCI_PROP_DUAL_OPTION_OID) ||
            ((SNFC_N74 <= bl->version[0]) &&
             (NCI_OID(pkt) == NCI_PROP_GET_OPTION_META_OID)) ||
            ((SNFC_N74 > bl->version[0]) &&
             (NCI_OID(pkt) == NCI_PROP_GET_RFREG_VER))) {
          // Set RF register version
          hal_vs_set_rfreg_version(vs, pkt);
          select_image_set_rf_version(vs->rfreg_version,
                                      RFREG_VERSION_SIZE - 2);  // User 6 bytes
          if (NCI_OID(pkt) == NCI_PROP_DUAL_OPTION_OID) {
            select_image_set_swreg_version(vs->swreg_version,
                                           RFREG_VERSION_SIZE - 2);
            select_image_set_chip_code((const char *)(NCI_PAYLOAD(pkt)
                                       + SWREG_VERSION_IDX + 12));
          } else if (NCI_OID(pkt) == NCI_PROP_GET_OPTION_META_OID) {
            select_image_set_chip_code((const char *)(NCI_PAYLOAD(pkt) + 12));
          } else if (NCI_OID(pkt) == NCI_PROP_GET_RFREG_VER) {
            select_image_set_chip_code((const char *)(NCI_PAYLOAD(pkt) + 6));
          }

#ifndef NFC_HAL_DO_NOT_USE_BOOTLOADER
          if (!(nfc_hal_info.flag & HAL_FLAG_FORCE_FW_UPDATE)) {
            if (nfc_fw_get_image(fw, nfc_hal_info.cfg.fw_file,
                                 bl->version[NFC_HAL_BL_VER_KEYINFO], false)) {
              // if (nfc_fw_force_update_check(fw))
              {
                OSI_logd("NFC need to update!!");
                select_image_set_chip_code("");
                device_set_mode(NFC_DEV_MODE_BOOTLOADER);
                nfc_hal_info.fw_info.state = FW_ENTER_UPDATE_MODE;
                nfc_hal_state_switch(msg, HAL_STATE_FW);
                break;
              }
              OSI_logd("Current F/W does not need to update!");
            }
          }
#endif
          nfc_hal_info.vs_info.state = VS_INIT;
          nfc_hal_state_switch(msg, HAL_STATE_VS);
          break;
        }
      }

      util_nci_analyzer(pkt);
      nfc_data_callback(&msg->nci_packet);
      break;

    case HAL_EVT_COMPLETE:
    complete:
      nfc_hal_info.flag |= HAL_FLAG_NTF_TRNS_ERROR | HAL_FLAG_RETRY_TRNS;
      nfc_hal_info.state = HAL_STATE_SERVICE;

      OSI_logd("Complete: CSC: %c%c%c", vs->rfreg_version[5],
               vs->rfreg_version[6], vs->rfreg_version[7]);
      OSI_logd("Complete: NFC FW Version: %02x.%02x.%02x", fw->fw_ver.major,
               fw->fw_ver.build1, fw->fw_ver.build2);
      OSI_logd("Complete: NFC RF Version: %d/%d/%d/%d.%d.%d",
               (vs->rfreg_version[0] >> 4) + 14, vs->rfreg_version[0] & 0xF,
               vs->rfreg_version[1], vs->rfreg_version[2],
               vs->rfreg_version[3], vs->rfreg_version[4]);
      if (SNFC_N74 <= bl->version[0]) {
        OSI_logd("Complete: NFC RF Number Version: %d.%d.%d(%d)",
                 vs->rfreg_number_version[0], vs->rfreg_number_version[1],
                 vs->rfreg_number_version[2], vs->rfreg_number_version[3]);
      }
      if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])){
        OSI_logd("Complete: NFC SW Version: %d/%d/%d/%d.%d.%d",
                 (vs->swreg_version[0] >> 4) + 14, vs->swreg_version[0] & 0xF,
                 vs->swreg_version[1], vs->swreg_version[2],
                 vs->swreg_version[3], vs->swreg_version[4]);
        OSI_logd("Complete: NFC SW Number Version: %d.%d.%d.%02X",
                 vs->swreg_number_version[0], vs->swreg_number_version[1],
                 vs->swreg_number_version[2], vs->swreg_number_version[3]);
      }

      nfc_stack_cback(HAL_NFC_POST_INIT_CPLT_EVT, HAL_NFC_STATUS_OK);
      break;
    case HAL_EVT_COMPLETE_FAILED:
      nfc_stack_cback(HAL_NFC_POST_INIT_CPLT_EVT, HAL_NFC_STATUS_FAILED);
      break;

    /* START - VTS */
    case HAL_EVT_POWER_CYCLE:
      OSI_logt("HAL_EVT_POWER_CYCLE");
      device_sleep();
      device_close();
      OSI_logt("HAL state change to POWERCYCLE");
      nfc_hal_state_switch(msg, HAL_STATE_POWERCYCLE);
      break;
      /* END - VTS */

    case HAL_EVT_TERMINATE:
      // TODO: terminate
      break;
    default:
      break;
  }
}

static void nfc_hal_fw_send_fw_status(unsigned char status) {
  tNFC_NCI_PKT fw_status;

  fw_status.oct0 = 0x6f;
  fw_status.oid = 0xFF;
  fw_status.len = 1;
  fw_status.payload[0] = status;
  OSI_loge("nfc_hal_fw_send_fw_status, status: 0x%02X", status);
  fw_update_state = status;
  nfc_data_callback(&fw_status);
}

static void nfc_hal_fw_timeout( __attribute__((unused)) void* param) {
  OSI_loge("FW Update fail!!");
  nfc_hal_fw_send_fw_status(FW_UPDATE_STATUS_ERROR);
}

void nfc_hal_fw_sm(tNFC_HAL_MSG* msg) {
  tNFC_FW_PKT* fw_pkt = &msg->fw_packet;
  tNFC_NCI_PKT* pkt = &msg->nci_packet;
  tNFC_HAL_FW_INFO* fw = &nfc_hal_info.fw_info;
  tNFC_HAL_FW_IMAGE_INFO* img = &fw->image_info;
  tNFC_HAL_FW_BL_INFO* bl = &fw->bl_info;
  tNFC_HAL_VS_INFO* vs = &nfc_hal_info.vs_info;

  uint8_t param[10] = {0x00};
  unsigned int len;

  switch (msg->event) {
    case HAL_EVT_CORE_INIT:
      device_set_mode(NFC_DEV_MODE_BOOTLOADER);
      [[fallthrough]];
    case HAL_EVT_READ:
      OSI_timer_stop(nfc_hal_info.nci_timer);

      if (fw->state & FW_W4_FW_RSP) {
        OSI_timer_start(nfc_hal_info.nci_timer, 5000,
                        (tOSI_TIMER_CALLBACK)nfc_hal_fw_timeout, NULL);
        fw_read_payload(msg);
      }
      if (fw->state & FW_W4_NCI_RSP) {
        nci_read_payload(msg);
        util_nci_analyzer(pkt);
      }

      // responsed succeess?
      if (fw->state & FW_W4_FW_RSP && ((fw_pkt->code & 0x0F)
          != FW_RET_SUCCESS)) {
        OSI_loge(
            "F/W bootloader mode failed. FW state: %d, Return code: 0x%02X",
            fw->state, fw_pkt->code);
        nfc_fw_free_image(fw);
        nfc_hal_fw_send_fw_status(FW_UPDATE_STATUS_ERROR);
        break;
      }

      switch (fw->state) {
        case FW_ENTER_UPDATE_MODE:
          if (img->binary != NULL) {
            TO_LITTLE_ARRY(&param[0], img->hashCodeLen, 2);
            TO_LITTLE_ARRY(&param[2], img->signatureLen, 2);
            nfc_fw_send_cmd(FW_CMD_ENTER_UPDATEMODE, param, 4);
            fw->state = FW_W4_ENTER_UPDATEMODE_RSP;
          } else {
            OSI_loge("Cannot get the FW image!");
            // TODO: error handle
          }
          break;

        case FW_W4_ENTER_UPDATEMODE_RSP:
          OSI_logd("Enter F/W Updating mode");
          nfc_hal_fw_send_fw_status(FW_UPDATE_STATUS_START);
          OSI_logd("HASH check...");
          nfc_fw_send_data(img->hashCode, img->hashCodeLen);
          fw->state = FW_W4_HASH_DATA_RSP;
          break;

        case FW_W4_HASH_DATA_RSP:
          OSI_logd("HASH checking is succeed, signature check...");
          nfc_fw_send_data(img->signature, img->signatureLen);
          fw->state = FW_W4_SIGN_DATA_RSP;
          break;

        case FW_W4_SIGN_DATA_RSP:
          OSI_logd("signature checking is succeed");
          img->cur = 0;
          fw->target_sector = 0;
          TO_LITTLE_ARRY(param, bl->base_address, 4);
          OSI_logd("sector update");
          nfc_fw_send_cmd(FW_CMD_UPDATE_SECT, param, 4);
          fw->state = FW_W4_UPDATE_SECT_RSP;
          break;

        case FW_W4_UPDATE_SECT_RSP:
          OSI_logd("%d sector is selected", fw->target_sector);
          len = nfc_fw_cal_next_frame_length(fw, FW_DATA_PAYLOAD_MAX);
          img->cur += nfc_fw_send_data(img->rawData + img->cur, len);
          fw->state = FW_W4_FW_DATA_RSP;
          break;

        case FW_W4_FW_DATA_RSP:
          if (img->cur == img->rawDataLen) { // complete
            nfc_fw_send_cmd(FW_CMD_COMPLETE_UPDATE_MODE, NULL, 0);
            fw->state = FW_W4_COMPLETE_RSP;
          } else if (img->cur < (unsigned int)((fw->target_sector + 1)
                     * bl->sector_size)) { // next data
            len = nfc_fw_cal_next_frame_length(fw, FW_DATA_PAYLOAD_MAX);
            img->cur += nfc_fw_send_data(img->rawData + img->cur, len);
          } else { // next sector
            fw->target_sector++;
            TO_LITTLE_ARRY(
                param, bl->base_address
                + (fw->target_sector * bl->sector_size), 4);
            OSI_logd("sector update");
            nfc_fw_send_cmd(FW_CMD_UPDATE_SECT, param, 4);
            fw->state = FW_W4_UPDATE_SECT_RSP;

            OSI_logd("F/W Updating...%6d/%6d (%3.0f%%)", img->cur,
                     (int)img->rawDataLen,
                     ((double)img->cur / (double)img->rawDataLen * 100));
          }
          break;

        case FW_W4_COMPLETE_RSP:
          OSI_logd("F/W Update complete");
          nfc_fw_free_image(fw);
          device_set_mode(NFC_DEV_MODE_OFF);
          device_set_mode(NFC_DEV_MODE_ON);
          // nfc_hal_fw_send_fw_status(FW_UPDATE_STATUS_COMPLETED);
#ifdef NFC_SEC_ESE_COLDRESET
          if (nfc_hal_nci_version == NCI_VER_2_0) {
            OSI_logd("eSE cold reset!");
            device_ese_coldreset();
          }
#endif
          OSI_logd("Get RF clock source type from CFG file and send it!");
          hal_nci_send_prop_fw_cfg(bl->product);
          fw->state = FW_W4_NCI_PROP_FW_CFG;
          break;

        case FW_W4_NCI_PROP_FW_CFG:
          if (NCI_MT(pkt) != NCI_MT_RSP || NCI_GID(pkt) != NCI_GID_PROP ||
              NCI_OID(pkt) != NCI_PROP_FW_CFG) {
            OSI_logd("Not matched rsponse!! we expect NCI_PROP_FW_CFG_RSP");
            if (NCI_MT(pkt) == NCI_MT_NTF && NCI_GID(pkt) == NCI_GID_PROP &&
                NCI_OID(pkt) == 0x3F && NCI_PAYLOAD(pkt)[0] == 0xFF) {
              fw_force_update(NULL);
            }
          } else {
            if (NCI_STATUS(pkt) != NCI_STATUS_OK &&
                NCI_STATUS(pkt) != NCI_STATUS_E_SYNTAX &&
                NCI_STATUS(pkt) != NCI_CLOCK_STATUS_SYNTAX_ERROR &&
                NCI_STATUS(pkt) != NCI_CLOCK_STATUS_MISMATCHED &&
                NCI_STATUS(pkt) != NCI_CLOCK_STATUS_FULL) {
              OSI_loge("Failed to config FW, status: %d", NCI_STATUS(pkt));
              break;
            } else {
              switch (NCI_STATUS(pkt)) {
                case NCI_STATUS_OK:
                  OSI_logd("RF clock setup success!!");
                  break;

                case NCI_STATUS_E_SYNTAX:
                  OSI_loge(
                      "This version of bootloader does not support FW_CFG!!");
                  nfc_hal_fw_send_fw_status(
                      FW_UPDATE_STATUS_ERROR); /* [H15052702] */
                  break;

                case NCI_CLOCK_STATUS_SYNTAX_ERROR:
                  OSI_loge("Command Length Error!");
                  break;

                case NCI_CLOCK_STATUS_MISMATCHED:
                  OSI_loge("Clock Mismatch!");
                  break;

                case NCI_CLOCK_STATUS_FULL:
                  OSI_loge(
                      "Please, Check the Clock Value. Current Clock Val : %d",
                      pkt->payload[1]);
                  break;
              }
            }

            if (bl->product >= SNFC_N7) {
              if (NCI_STATUS(pkt) == NCI_CLOCK_STATUS_FULL) {
                fw_force_update(NULL);
                break;
              } else if (NCI_STATUS(pkt) == NCI_CLOCK_STATUS_MISMATCHED) {
                device_set_mode(NFC_DEV_MODE_OFF);
                device_set_mode(NFC_DEV_MODE_ON);

                nfc_hal_state_switch(msg, HAL_STATE_FW);
                fw->state = FW_W4_NCI_PROP_FW_CFG;
                hal_nci_send_prop_fw_cfg(bl->product);
                break;
              }

              if ((nfc_hal_info.flag & HAL_FLAG_CLK_SET) ||
                  ((nfc_hal_info.flag & HAL_FLAG_FORCE_FW_UPDATE) &&
                  !(nfc_hal_info.flag & (HAL_FLAG_W4_CORE_RESET_RSP |
                                         HAL_FLAG_W4_CORE_INIT_RSP)))) {
                msg->event = HAL_EVT_COMPLETE;
                nfc_hal_state_switch(msg, HAL_STATE_OPEN);
                nfc_hal_info.flag &= ~HAL_FLAG_CLK_SET;
              } else {
                hal_nci_send_reset();
                fw->state = FW_W4_NCI_RESET_RSP;
              }
              break;
            }

            if ((nfc_hal_info.flag & HAL_FLAG_FORCE_FW_UPDATE) &&
                !(nfc_hal_info.flag & (HAL_FLAG_W4_CORE_RESET_RSP |
                  HAL_FLAG_W4_CORE_INIT_RSP))) { /* F/W is updated before
                                                     sending CORE_RESET_CMD;
                                                     go back to open sm*/
              msg->event = HAL_EVT_COMPLETE;
              nfc_hal_state_switch(msg, HAL_STATE_OPEN);
            } else {
              /* F/W is updated after sending CORE_RESET_CMD */
              hal_nci_send_reset();
              fw->state = FW_W4_NCI_RESET_RSP;
            }
          }
          break;

        case FW_W4_NCI_RESET_RSP:
          if (((NCI_LEN(pkt) == 0x03) &&
              (NCI_MT(pkt) != NCI_MT_RSP || NCI_GID(pkt) != NCI_GID_CORE ||
               NCI_OID(pkt) != NCI_CORE_RESET || NCI_STATUS(pkt)
               != NCI_STATUS_OK)) ||
              ((NCI_LEN(pkt) > 0x03) &&
               (NCI_MT(pkt) != NCI_MT_NTF || NCI_GID(pkt) != NCI_GID_CORE ||
                NCI_OID(pkt) != NCI_CORE_RESET))) {
            OSI_logd("Failed NCI CORE_RESET after F/W updating.");
            nfc_stack_cback(HAL_NFC_POST_INIT_CPLT_EVT, HAL_NFC_STATUS_FAILED);
          } else if (NCI_LEN(pkt) > 0x01) {
            OSI_logd("OK! NCI CORE_RESET after F/W updating.");
            nfc_hal_info.flag &= ~HAL_FLAG_W4_CORE_RESET_RSP;

            if (nfc_hal_info.flag &
                HAL_FLAG_W4_CORE_RESET_RSP) { /* F/W is updated after sending
                                                 CORE_RESET_CMD */
              nfc_hal_info.state = HAL_STATE_POSTINIT;
              nfc_data_callback(pkt);
              break;
            }

            OSI_timer_stop(nfc_hal_info.nci_timer);
            fw->state = FW_W4_NCI_INIT_RSP;
            OSI_timer_start(nfc_hal_info.nci_timer, 1000,
                            (tOSI_TIMER_CALLBACK)nci_init_timeout, NULL);

            if((NCI_LEN(pkt) == 0x03) && NCI_MT(pkt) == NCI_MT_RSP) {
              nfc_hal_nci_version = NCI_VER_1_0;
            } else {
              nfc_hal_nci_version = NCI_VER_2_0;
            }
            if (nfc_hal_nci_version == NCI_VER_2_0) {
              memcpy(&fw->fw_ver, NCI_MF_INFO(pkt), NCI_MF_INFO_SIZE);
              select_image_set_fw_version(NCI_MF_INFO(pkt),
                                          NCI_MF_INFO_SIZE - 1);
            }
            nfc_data_callback(pkt);
            hal_nci_send_init(nfc_hal_nci_version);
          }
          break;

        case FW_W4_NCI_INIT_RSP:
          if (NCI_MT(pkt) != NCI_MT_RSP || NCI_GID(pkt) != NCI_GID_CORE ||
              NCI_OID(pkt) != NCI_CORE_INIT ||
              NCI_STATUS(pkt) != NCI_STATUS_OK) {
            OSI_logd("Failed NCI CORE_INIT after F/W updating.");
            nfc_stack_cback(HAL_NFC_POST_INIT_CPLT_EVT, HAL_NFC_STATUS_FAILED);
            break;
          }

          if (nfc_hal_nci_version != NCI_VER_2_0) {
            memcpy(&fw->fw_ver, NCI_MF_INFO(pkt), NCI_MF_INFO_SIZE);
            select_image_set_fw_version(NCI_MF_INFO(pkt),
                                        NCI_MF_INFO_SIZE - 1);
          }

          nfc_hal_info.flag &= ~HAL_FLAG_W4_CORE_INIT_RSP;
          OSI_logd("OK! NCI CORE_INIT after F/W updating.");

          OSI_logd("Reset rf version");
          hal_vs_reset_rfreg_version(vs);
          select_image_set_rf_version(vs->rfreg_version,
                                      RFREG_VERSION_SIZE - 2);
          if (NCI_OID(pkt) == NCI_PROP_DUAL_OPTION_OID) {
            select_image_set_swreg_version(vs->swreg_version,
                                           RFREG_VERSION_SIZE - 2);
          }

          nfc_data_callback(pkt);
          if (nfc_hal_info.flag & HAL_FLAG_W4_CORE_INIT_RSP) {
            nfc_hal_info.state = HAL_STATE_POSTINIT;
            break;
          }

          /* F/W is updated after CORE_INIT_RSP */
          fw->state = (eNFC_HAL_FW_STATE)0;
          nfc_hal_info.vs_info.state = VS_INIT;
          nfc_hal_state_switch(msg, HAL_STATE_VS);
          break;

        default:
          break;
      }
      break;
    default:
      OSI_loge("Unexpected event [%d]", msg->event);
      break;
  }
}

void nfc_hal_vs_sm(tNFC_HAL_MSG* msg) {
  tNFC_HAL_VS_INFO* vs = &nfc_hal_info.vs_info;
  tNFC_HAL_FW_INFO* fw = &nfc_hal_info.fw_info;
  tNFC_HAL_FW_BL_INFO* bl = &fw->bl_info;
  tNFC_NCI_PKT* pkt = &msg->nci_packet;
  uint8_t check_sum[2];
  static int count = 0;

  if (msg->event != HAL_EVT_READ && msg->event != HAL_EVT_CORE_INIT) {
    OSI_loge("Unexpected event [%d]", msg->event);
    return;
  }
  /* Reque HAL_EVT_CORE_INIT to process in HAL_STATE_POSTINIT
   * nci_read_payload(msg) with the init param will try to read
   * unexpected len of response for NCI_PROP_START_RFREG
   * and hang in reading
   */
  if(msg->event == HAL_EVT_CORE_INIT){
     OSI_logd("%s", "Reque HAL_EVT_CORE_INIT...");
     OSI_queue_put(nfc_hal_info.msg_q, (void *)msg);
     return;
  }
  if (vs->state != VS_INIT) {
    nci_read_payload(msg);
    util_nci_analyzer(pkt);
  }

  switch (vs->state) {
    case VS_INIT:
      vs->rfregid = 0;
      vs->check_sum = 0;

      if (!hal_vs_check_rfreg_update(vs, pkt)) {
        OSI_logd("Do not need to update RF registers.");
        goto vs_work_around;
      }

      // merge HW,SW REG to RF REG
      if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])) {
        hal_vs_merge_rf_image(vs);
      }

      OSI_logd("Start setting RF register!!");
      if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])) {
        uint8_t sub_oid = NCI_PROP_DUAL_OPTION_SUB_OID_START_UPDATE;
        hal_vs_nci_send(NCI_PROP_DUAL_OPTION_OID, &sub_oid, 1);
      } else {
        hal_vs_nci_send(NCI_PROP_START_RFREG, NULL, 0);
      }
      vs->state = VS_W4_START_RFREG_RSP;
      break;

    case VS_W4_START_RFREG_RSP:
      if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])) {
        if (NCI_MT(pkt) != NCI_MT_RSP || NCI_GID(pkt) != NCI_GID_PROP ||
            NCI_OID(pkt) != NCI_PROP_DUAL_OPTION_OID ||
            NCI_STATUS(pkt) != NCI_STATUS_OK) {
          OSI_loge("Failed to start RF register!");
          goto vs_work_around;
        }
      } else {
        if (NCI_MT(pkt) != NCI_MT_RSP || NCI_GID(pkt) != NCI_GID_PROP ||
            NCI_OID(pkt) != NCI_PROP_START_RFREG ||
            NCI_STATUS(pkt) != NCI_STATUS_OK) {
          OSI_loge("Failed to start RF register!");
          goto vs_work_around;
        }
      }
      vs->state = VS_W4_FILE_RFREG_RSP;
      [[fallthrough]];

    case VS_W4_FILE_RFREG_RSP:
      if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])) {
        if (NCI_MT(pkt) != NCI_MT_RSP || NCI_GID(pkt) != NCI_GID_PROP ||
            NCI_OID(pkt) != NCI_PROP_DUAL_OPTION_OID ||
            NCI_STATUS(pkt) != NCI_STATUS_OK) {
          OSI_loge("Failed to set reg id: %d!", vs->rfregid - 1);
          uint8_t sub_oid = NCI_PROP_DUAL_OPTION_SUB_OID_STOP_UPDATE;
          hal_vs_nci_send(NCI_PROP_DUAL_OPTION_OID, &sub_oid, 1);
          vs->state = VS_W4_STOP_RFREG_RSP;
          break;
        }
      } else {
        if (NCI_MT(pkt) != NCI_MT_RSP || NCI_GID(pkt) != NCI_GID_PROP ||
            NCI_OID(pkt) != NCI_PROP_SET_RFREG ||
            NCI_STATUS(pkt) != NCI_STATUS_OK) {
          if (NCI_OID(pkt) != NCI_PROP_START_RFREG) {
            OSI_loge("Failed to set RF reg id: %d!", vs->rfregid - 1);
            hal_vs_nci_send(NCI_PROP_STOP_RFREG, NULL, 0);
            vs->state = VS_W4_STOP_RFREG_RSP;
            break;
          }
        }
      }

      if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])){
        if (hal_vs_rfreg_update_dual_option(vs)) // is continue?
          break;
      } else {
        if (hal_vs_rfreg_update(vs)) // is continue?
          break;
      }

      if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])) {
        uint8_t nci_payload[20];
        OSI_logd("Stop setting register!");
        OSI_logd("Register check sum is 0x%04X", vs->check_sum & 0xFFFF);
        TO_LITTLE_ARRY(check_sum, vs->check_sum & 0xFFFF, 2);

        uint8_t sub_oid = NCI_PROP_DUAL_OPTION_SUB_OID_STOP_UPDATE;
        nci_payload[0] = sub_oid;
        memcpy((uint8_t *)&nci_payload[1], check_sum, sizeof(check_sum));
        hal_vs_nci_send(NCI_PROP_DUAL_OPTION_OID, nci_payload,
                        sizeof(check_sum) + 1);
        vs->state = VS_W4_STOP_RFREG_RSP;
      } else {
        OSI_logd("Set RF register version");
        hal_vs_nci_send(NCI_PROP_SET_RFREG_VER, vs->rfreg_version, 8);
        vs->state = VS_W4_SET_RFREG_VER_RSP;
      }
      break;

    case VS_W4_SET_RFREG_VER_RSP:
      if (NCI_MT(pkt) != NCI_MT_RSP || NCI_GID(pkt) != NCI_GID_PROP ||
          NCI_OID(pkt) != NCI_PROP_SET_RFREG_VER ||
          NCI_STATUS(pkt) != NCI_STATUS_OK) {
        OSI_loge("Failed to set RF version!");
      }

      OSI_logd("Stop setting RF register!");
      OSI_logd("RF register check sum is 0x%04X", vs->check_sum & 0xFFFF);
      TO_LITTLE_ARRY(check_sum, vs->check_sum & 0xFFFF, 2);
      hal_vs_nci_send(NCI_PROP_STOP_RFREG, check_sum, 2);
      vs->state = VS_W4_STOP_RFREG_RSP;
      break;

    case VS_W4_STOP_RFREG_RSP:
      if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])) {
        if (NCI_MT(pkt) != NCI_MT_RSP || NCI_GID(pkt) != NCI_GID_PROP ||
            NCI_OID(pkt) != NCI_PROP_DUAL_OPTION_OID ||
            NCI_STATUS(pkt) != NCI_STATUS_OK) {
          OSI_loge("Failed to stop RF register!");
          if (count < 2) {
            vs->rfregid = 0;
            vs->check_sum = 0;
            OSI_logd("Restart setting register!!");
            uint8_t sub_oid = NCI_PROP_DUAL_OPTION_SUB_OID_START_UPDATE;
            hal_vs_nci_send(NCI_PROP_DUAL_OPTION_OID, &sub_oid, 1);
            vs->state = VS_W4_START_RFREG_RSP;
            count++;
            break;
          }
        } else {
          OSI_logd("Success RF register set!!");
          if (fw_update_state == FW_UPDATE_STATUS_START)
            nfc_hal_fw_send_fw_status(FW_UPDATE_STATUS_COMPLETED);
        }
      } else {
        if (NCI_STATUS(pkt) != NCI_STATUS_OK || NCI_MT(pkt) != NCI_MT_RSP ||
            NCI_GID(pkt) != NCI_GID_PROP || NCI_OID(pkt)
            != NCI_PROP_STOP_RFREG) {
          OSI_loge("Failed to stop RF register!");
          if (count < 2) {
            vs->rfregid = 0;
            vs->check_sum = 0;
            OSI_logd("Restart setting RF register!!");
            hal_vs_nci_send(NCI_PROP_START_RFREG, NULL, 0);
            vs->state = VS_W4_START_RFREG_RSP;
            count++;
            break;
          }
        } else {
          OSI_logd("Success RF register set!!");
          if (fw_update_state == FW_UPDATE_STATUS_START)
            nfc_hal_fw_send_fw_status(FW_UPDATE_STATUS_COMPLETED);
        }
      }

    vs_work_around:
      /* Workaround: Initialization flash of LMRT
       * TODO: Only for version 2.2.4
       */
      if (fw_update_state == FW_UPDATE_STATUS_START)
        nfc_hal_fw_send_fw_status(FW_UPDATE_STATUS_ERROR);
      {
        hal_nci_send_clearLmrt();
        if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])) {
          vs->state = VS_SET_SW_API_TRACE;
        } else {
          vs->state = VS_W4_COMPLETE;
        }
        break;
      }
  /* End WA */

    case VS_SET_SW_API_TRACE:
    {
      OSI_logd("Vendor: Set SW API Trace");
      char valueStr[PROPERTY_VALUE_MAX] = {0};
      int ret;

      ret = property_get("nfc.fw.sw_api_trace", valueStr, "");
      if (ret > 1) {
        OSI_logd("Invalid SW API Trace property.");
        vs->state = VS_W4_COMPLETE;
        [[fallthrough]];
      } else {
        hal_nci_send_setSWAPITrace((uint8_t*)valueStr, 1);
        vs->state = VS_W4_SET_SW_API_TRACE;
        break;
      }
    }
    case VS_W4_SET_SW_API_TRACE:
      vs->state = VS_W4_COMPLETE;
      [[fallthrough]];

    case VS_W4_COMPLETE:
      OSI_logd("Vendor Specific is complete.");

      if ((SNFC_N74 == bl->version[0]) || (SNFC_N84 <= bl->version[0])) {
        if (vs->swreg_binary != NULL) free(vs->swreg_binary);
      }
      msg->event = HAL_EVT_COMPLETE;
      nfc_hal_state_switch(msg, HAL_STATE_POSTINIT);
      break;
    default:
      OSI_loge("Unexpected event [%d]", msg->event);
      break;
  }
}

void nfc_hal_service_sm(tNFC_HAL_MSG* msg) {
  tNFC_NCI_PKT* pkt = &msg->nci_packet;
  nfc_hal_info.msg_event = msg->event;

  switch (msg->event) {
    case HAL_EVT_CORE_INIT:
      nfc_hal_info.vs_info.state = VS_INIT;
      nfc_hal_state_switch(msg, HAL_STATE_VS);
      break;
    case HAL_EVT_WRITE:
      if (nfc_hal_prehandler(pkt)) hal_nci_send(pkt);
      break;
    case HAL_EVT_READ:
      nci_read_payload(msg);
      util_nci_analyzer(pkt);
      hal_update_sleep_timer();
      if (nfc_hal_prehandler(pkt)) nfc_data_callback(pkt);
      break;
    case HAL_EVT_CONTROL_GRANTED:
      nfc_hal_state_switch(msg, HAL_STATE_GRANTED);
      break;
    case HAL_EVT_TERMINATE:
      // TODO: terminate
      break;
    default:
      break;
  }
}

static void nfc_hal_grant_finish(void) {
  nfc_stack_cback(HAL_NFC_RELEASE_CONTROL_EVT, HAL_NFC_STATUS_OK);
  nfc_hal_info.state = HAL_STATE_SERVICE;
  nfc_hal_info.grant_cback = NULL;
}

void nfc_hal_grant_sm(tNFC_HAL_MSG* msg) {
  tNFC_NCI_PKT* pkt = &msg->nci_packet;
  uint8_t cback_ret = HAL_GRANT_FINISH;

  /* Granted mode is not need to SLEEP.
   * hal should pend granted mode just few time */
  switch (msg->event) {
    case HAL_EVT_READ:
      nci_read_payload(msg);
      util_nci_analyzer(pkt);
      cback_ret = nfc_hal_info.grant_cback(pkt);
      if (cback_ret == HAL_GRANT_FINISH) nfc_hal_grant_finish();

      if (cback_ret != HAL_GRANT_SEND_NEXT) break;
      [[fallthrough]];
    case HAL_EVT_CONTROL_GRANTED:
      pkt = (tNFC_NCI_PKT*)OSI_queue_get(nfc_hal_info.nci_q);
      if (pkt) {
        // TODO: Should CLF respond?
        hal_nci_send(pkt);
        OSI_mem_free((tOSI_MEM_HANDLER)pkt);
      } else
        nfc_hal_grant_finish();

      break;

    case HAL_EVT_WRITE:
      OSI_loge("HAL is in granted mode!");
      break;
  }
}

void nfc_hal_power_sm(tNFC_HAL_MSG* msg) {
  switch (msg->event) {
    case HAL_EVT_POWER_CYCLE:
      // have to do is hal open
      OSI_logt("HAL_EVT_POWER_CYCLE");
      // nfc_hal_init();

      if (device_open()) return;

      msg->event = HAL_EVT_OPEN;
      nfc_hal_state_switch(msg, HAL_STATE_OPEN);
      break;
    default:
      break;
  }
}

void nfc_hal_task(void) {
  tNFC_HAL_MSG* msg;
  eHAL_STATE old_st;

  OSI_logt("enter!");

  if (!nfc_hal_info.msg_task || !nfc_hal_info.nci_timer ||
      !nfc_hal_info.msg_q || !nfc_hal_info.nci_q) {
    OSI_loge("msg_task = %p, nci_timer = %p, msg_q = %p, nci_q = %p",
             nfc_hal_info.msg_task, nfc_hal_info.nci_timer, nfc_hal_info.msg_q,
             nfc_hal_info.nci_q);

    nfc_hal_deinit();
    OSI_loge("nfc_hal initialization is not succeeded.");
    nfc_stack_cback(HAL_NFC_ERROR_EVT, HAL_NFC_STATUS_FAILED);
    return;
  }

  while (OSI_task_isRun(nfc_hal_info.msg_task) == OSI_RUN) {
    msg = (tNFC_HAL_MSG*)OSI_queue_get_wait(nfc_hal_info.msg_q);
    if (!msg) continue;

    OSI_logd("Got a event: %s(%d)", event_to_string(msg->event), msg->event);
    if (msg->event == HAL_EVT_TERMINATE) break;

    OSI_logd("current state: %s", state_to_string(nfc_hal_info.state));
    old_st = nfc_hal_info.state;
    switch (nfc_hal_info.state) {
      case HAL_STATE_INIT:
      case HAL_STATE_DEINIT:
      case HAL_STATE_OPEN:
        nfc_hal_open_sm(msg);
        break;
      case HAL_STATE_FW:
        nfc_hal_fw_sm(msg);
        break;
      case HAL_STATE_VS:
        nfc_hal_vs_sm(msg);
        break;
      case HAL_STATE_POSTINIT:
        nfc_hal_postinit_sm(msg);
        break;
      case HAL_STATE_SERVICE:
        nfc_hal_service_sm(msg);
        break;
      case HAL_STATE_GRANTED:
        nfc_hal_grant_sm(msg);
        break;
      /* START - VTS */
      case HAL_STATE_POWERCYCLE:
        nfc_hal_power_sm(msg);
        break;
      /* END - VTS */
      default:
        break;
    }
    OSI_mem_free((tOSI_MEM_HANDLER)msg);

    if (old_st != nfc_hal_info.state) {
      OSI_logd("hal state is changed: %s -> %s", state_to_string(old_st),
               state_to_string(nfc_hal_info.state));
    }
  }
  OSI_logt("exit!");
}

const char* event_to_string(uint8_t event) {
  switch (event) {
    case HAL_EVT_OPEN:
      return "HAL_EVT_OPEN";
    case HAL_EVT_CORE_INIT:
      return "HAL_EVT_CORE_INIT";
    case HAL_EVT_WRITE:
      return "HAL_EVT_WRITE";
    case HAL_EVT_READ:
      return "HAL_EVT_READ";
    case HAL_EVT_CONTROL_GRANTED:
      return "HAL_EVT_CONTROL_GRANTED";
    case HAL_EVT_POWER_CYCLE:
      return "HAL_EVT_POWER_CYCLE";
    case HAL_EVT_TERMINATE:
      return "NFC_HAL_TERMINATE";
    case HAL_EVT_COMPLETE:
      return "NFC_HAL_COMPLETE";
    case HAL_EVT_COMPLETE_FAILED:
      return "NFC_HAL_COMPLETE_FAILED";
  }
  return "Unknown event.";
}

const char* state_to_string(eHAL_STATE state) {
  switch (state) {
    case HAL_STATE_INIT:
      return "INIT";
    case HAL_STATE_DEINIT:
      return "DEINIT";
    case HAL_STATE_OPEN:
      return "OPEN";
    case HAL_STATE_FW:
      return "FW_UPDATE";
    case HAL_STATE_VS:
      return "VENDOR_SPECIFIC";
    case HAL_STATE_POSTINIT:
      return "POST_INIT";
    case HAL_STATE_SERVICE:
      return "SERVICE";
    case HAL_STATE_GRANTED:
      return "GRANT";
    case HAL_STATE_POWERCYCLE:
      return "POWER_CYCLE";
    case HAL_STATE_CLOSE:
      return "CLOSE";
  }
  return "Unknown state.";
}
