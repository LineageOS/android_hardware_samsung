/* //device/libs/telephony/ril_unsol_commands.h
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
    {SAMSUNG_UNSOL_RESPONSE_BASE, NULL, WAKE_PARTIAL}, // SAMSUNG_UNSOL_RESPONSE_BASE should be defined as 11000 in ril.h
    {RIL_UNSOL_RELEASE_COMPLETE_MESSAGE, NULL, WAKE_PARTIAL}, // 11001
    {RIL_UNSOL_STK_SEND_SMS_RESULT, NULL, WAKE_PARTIAL}, // 11002
    {RIL_UNSOL_STK_CALL_CONTROL_RESULT, NULL, WAKE_PARTIAL}, // 11003
    {RIL_UNSOL_DUN_CALL_STATUS, NULL, WAKE_PARTIAL}, // 11004
    {11005, NULL, WAKE_PARTIAL},
    {11006, NULL, WAKE_PARTIAL},
    {RIL_UNSOL_O2_HOME_ZONE_INFO, NULL, WAKE_PARTIAL}, // 11007
    {RIL_UNSOL_DEVICE_READY_NOTI, NULL, WAKE_PARTIAL}, // 11008
    {RIL_UNSOL_GPS_NOTI, NULL, WAKE_PARTIAL}, // 11009
    {RIL_UNSOL_AM, NULL, WAKE_PARTIAL}, // 11010
    {RIL_UNSOL_DUN_PIN_CONTROL_SIGNAL, NULL, WAKE_PARTIAL}, // 11011
    {RIL_UNSOL_DATA_SUSPEND_RESUME, NULL, WAKE_PARTIAL}, // 11012
    {RIL_UNSOL_SAP, NULL, WAKE_PARTIAL}, // 11013
    {11014, NULL, WAKE_PARTIAL},
    {RIL_UNSOL_SIM_SMS_STORAGE_AVAILALE, NULL, WAKE_PARTIAL}, // 11015
    {RIL_UNSOL_HSDPA_STATE_CHANGED, NULL, WAKE_PARTIAL}, // 11016
    {RIL_UNSOL_WB_AMR_STATE, NULL, WAKE_PARTIAL}, // 11017
    {RIL_UNSOL_TWO_MIC_STATE, NULL, WAKE_PARTIAL}, // 11018
    {RIL_UNSOL_DHA_STATE, NULL, WAKE_PARTIAL}, // 11019
    {RIL_UNSOL_UART, NULL, WAKE_PARTIAL}, // 11020
    {RIL_UNSOL_RESPONSE_HANDOVER, NULL, WAKE_PARTIAL}, // 11021
    {RIL_UNSOL_IPV6_ADDR, NULL, WAKE_PARTIAL}, // 11022
    {RIL_UNSOL_NWK_INIT_DISC_REQUEST, NULL, WAKE_PARTIAL}, // 11023
    {RIL_UNSOL_RTS_INDICATION, NULL, WAKE_PARTIAL}, // 11024
    {RIL_UNSOL_OMADM_SEND_DATA, NULL, WAKE_PARTIAL}, // 11025
    {RIL_UNSOL_DUN, NULL, WAKE_PARTIAL}, // 11026
    {RIL_UNSOL_SYSTEM_REBOOT, NULL, WAKE_PARTIAL}, // 11027
    {RIL_UNSOL_VOICE_PRIVACY_CHANGED, NULL, WAKE_PARTIAL}, // 11028
    {RIL_UNSOL_UTS_GETSMSCOUNT, NULL, WAKE_PARTIAL}, // 11029
    {RIL_UNSOL_UTS_GETSMSMSG, NULL, WAKE_PARTIAL}, // 11030
    {RIL_UNSOL_UTS_GET_UNREAD_SMS_STATUS, NULL, WAKE_PARTIAL}, // 11031
    {RIL_UNSOL_MIP_CONNECT_STATUS, NULL, WAKE_PARTIAL}, // 11032
#ifdef RIL_UNSOL_SNDMGR_WB_AMR_REPORT
    {RIL_UNSOL_SNDMGR_WB_AMR_REPORT, NULL, WAKE_PARTIAL}, // 20017
#endif
#ifdef RIL_UNSOL_SNDMGR_CLOCK_CTRL
    {RIL_UNSOL_SNDMGR_CLOCK_CTRL, NULL, WAKE_PARTIAL}, // 20022
#endif