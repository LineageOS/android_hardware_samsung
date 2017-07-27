/* //device/libs/telephony/ril_commands.h
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
    {10000, NULL, NULL},
    {10001, NULL, NULL},
    {RIL_REQUEST_GET_CELL_BROADCAST_CONFIG, dispatchVoid, responseVoid},
    {10003, NULL, NULL},
    {10004, NULL, NULL},
    {RIL_REQUEST_SEND_ENCODED_USSD, dispatchVoid, responseVoid},
    {RIL_REQUEST_SET_PDA_MEMORY_STATUS, dispatchVoid, responseVoid},
    {RIL_REQUEST_GET_PHONEBOOK_STORAGE_INFO, dispatchVoid, responseVoid},
    {RIL_REQUEST_GET_PHONEBOOK_ENTRY, dispatchVoid, responseVoid},
    {RIL_REQUEST_ACCESS_PHONEBOOK_ENTRY, dispatchVoid, responseVoid},
    {RIL_REQUEST_DIAL_VIDEO_CALL, dispatchVoid, responseVoid},
    {RIL_REQUEST_CALL_DEFLECTION, dispatchVoid, responseVoid},
    {RIL_REQUEST_READ_SMS_FROM_SIM, dispatchVoid, responseVoid},
    {RIL_REQUEST_USIM_PB_CAPA, dispatchVoid, responseVoid},
    {RIL_REQUEST_LOCK_INFO, dispatchVoid, responseVoid},
    {10015, NULL, NULL},
    {RIL_REQUEST_DIAL_EMERGENCY, dispatchDial, responseVoid},
    {RIL_REQUEST_GET_STOREAD_MSG_COUNT, dispatchVoid, responseVoid},
    {RIL_REQUEST_STK_SIM_INIT_EVENT, dispatchVoid, responseVoid},
    {RIL_REQUEST_GET_LINE_ID, dispatchVoid, responseVoid},
    {RIL_REQUEST_SET_LINE_ID, dispatchVoid, responseVoid},
    {RIL_REQUEST_GET_SERIAL_NUMBER, dispatchVoid, responseVoid},
    {RIL_REQUEST_GET_MANUFACTURE_DATE_NUMBER, dispatchVoid, responseVoid},
    {RIL_REQUEST_GET_BARCODE_NUMBER, dispatchVoid, responseVoid},
    {RIL_REQUEST_UICC_GBA_AUTHENTICATE_BOOTSTRAP, dispatchVoid, responseVoid},
    {RIL_REQUEST_UICC_GBA_AUTHENTICATE_NAF, dispatchVoid, responseVoid},
    {RIL_REQUEST_SIM_TRANSMIT_BASIC, dispatchVoid, responseVoid},
    {RIL_REQUEST_SIM_OPEN_CHANNEL, dispatchVoid, responseVoid},
    {RIL_REQUEST_SIM_CLOSE_CHANNEL, dispatchVoid, responseVoid},
    {RIL_REQUEST_SIM_TRANSMIT_CHANNEL, dispatchVoid, responseVoid},
    {RIL_REQUEST_SIM_AUTH, dispatchVoid, responseVoid},
    {RIL_REQUEST_PS_ATTACH, dispatchVoid, responseVoid},
    {RIL_REQUEST_PS_DETACH, dispatchVoid, responseVoid},
    {RIL_REQUEST_ACTIVATE_DATA_CALL, dispatchVoid, responseVoid},
    {RIL_REQUEST_CHANGE_SIM_PERSO, dispatchVoid, responseVoid},
    {RIL_REQUEST_ENTER_SIM_PERSO, dispatchVoid, responseVoid},
    {RIL_REQUEST_GET_TIME_INFO, dispatchVoid, responseVoid},
    {RIL_REQUEST_OMADM_SETUP_SESSION, dispatchVoid, responseVoid},
    {RIL_REQUEST_OMADM_SERVER_START_SESSION, dispatchVoid, responseVoid},
    {RIL_REQUEST_OMADM_CLIENT_START_SESSION, dispatchVoid, responseVoid},
    {RIL_REQUEST_OMADM_SEND_DATA, dispatchVoid, responseVoid},
    {RIL_REQUEST_CDMA_GET_DATAPROFILE, dispatchVoid, responseVoid},
    {RIL_REQUEST_CDMA_SET_DATAPROFILE, dispatchVoid, responseVoid},
    {RIL_REQUEST_CDMA_GET_SYSTEMPROPERTIES, dispatchVoid, responseVoid},
    {RIL_REQUEST_CDMA_SET_SYSTEMPROPERTIES, dispatchVoid, responseVoid},
    {RIL_REQUEST_SEND_SMS_COUNT, dispatchVoid, responseVoid},
    {RIL_REQUEST_SEND_SMS_MSG, dispatchVoid, responseVoid},
    {RIL_REQUEST_SEND_SMS_MSG_READ_STATUS, dispatchVoid, responseVoid},
    {RIL_REQUEST_MODEM_HANGUP, dispatchVoid, responseVoid},
    {RIL_REQUEST_SET_SIM_POWER, dispatchVoid, responseVoid},
    {RIL_REQUEST_SET_PREFERRED_NETWORK_LIST, dispatchVoid, responseVoid},
    {RIL_REQUEST_GET_PREFERRED_NETWORK_LIST, dispatchVoid, responseVoid},
    {RIL_REQUEST_HANGUP_VT, dispatchVoid, responseVoid},
