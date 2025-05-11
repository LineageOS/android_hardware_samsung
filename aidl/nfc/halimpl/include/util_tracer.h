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

#define TRC_MT_SHIFT 5
#define TRC_MT_MASK 0xE0
#define TRC_MT_DATA (0x0 << TRC_MT_SHIFT)
#define TRC_MT_CMD (0x1 << TRC_MT_SHIFT)
#define TRC_MT_RSP (0x2 << TRC_MT_SHIFT)
#define TRC_MT_NTF (0x3 << TRC_MT_SHIFT)

#define TRC_PBF_SHIFT 4
#define TRC_PBF_MASK 0x10
#define TRC_PBF_COMPLETE (0 << TRC_PBF_SHIFT)
#define TRC_PBF_SEGMENT (1 << TRC_PBF_SHIFT)

#define TRC_GID_SHIFT 0
#define TRC_GID_MASK 0xF
#define TRC_GID_NCICORE 0x0  // 0000b
#define TRC_GID_RF 0x1       // 0001b
#define TRC_GID_NFCEE 0x2    // 0010b
#define TRC_GID_PROP 0xF     // 1111b

enum TRC_OID_CORE {
  CORE_RESET,
  CORE_INIT,
  CORE_SET_CONFIG,
  CORE_GET_CONFIG,
  CORE_CONN_CREATE,
  CORE_CONN_CLOSE,
  CORE_CONN_CREDITS,
  CORE_GENERIC_ERROR,
  CORE_INTERFACE_ERROR,
  OID_CORE_MAX,
};

enum TRC_OID_RF {
  RF_DISCOVER_MAP,
  RF_SET_LISTEN_MODE_ROUTING,
  RF_GET_LISTEN_MODE_ROUTING,
  RF_DISCOVER,
  RF_DISCOVER_SELECT,
  RF_INTF_ACTIVATED,
  RF_DEACTIVATED,
  RF_FIELD_INFO,
  RF_T3T_POLLING,
  RF_NFCEE_ACTION,
  RF_NFCEE_DISCOVERY_REQ,
  RF_PARAMETER_UPDATE,
  OID_RF_MAX,
};

enum TRC_OID_NFCEE {
  NFCEE_DISCOVER,
  NFCEE_MODE_SET,
  OID_NFCEE_MAX,
};

enum TRC_OID_PROP {
  OID_PROP_MAX,
};

struct mapped_string {
  uint8_t param;
  char* str;
};

char* str_unknown = "Unkown value";
char* str_not_supported = "Not Supported";
char* str_value = "Value";
char* str_value_hex = "Value(Hex)";

char* _get_str(char* table[], uint8_t max, uint8_t value) {
  if (value < max) return table[value];
  return str_unknown;
}
#define get_str(x, y) _get_str(x, sizeof(x), y)

char* get_mstr(struct mapped_string* mstr, uint8_t value) {
  while (mstr->str != NULL) {
    if (mstr->param == value) {
      return mstr->str;
    }
    mstr++;
  }
  return "RFU";
}

/* string tables */

char* str_mt[] = {"_DATA", "_CMD", "_RSP", "_NTF"};

char* str_direction[] = {"DDDD", ">>>>", "<<<<", "<<<<"};

/* control packet */
char* str_oid_core[] = {
    "CORE_RESET",        "CORE_INIT",          "CORE_SET_CONFIG",
    "CORE_GET_CONFIG",   "CORE_CONN_CREATE",   "CORE_CONN_CLOSE",
    "CORE_CONN_CREDITS", "CORE_GENERIC_ERROR", "CORE_INTERFACE_ERROR"};

char* str_oid_rf[] = {"RF_DISCOVER_MAP",
                      "RF_SET_LISTEN_MODE_ROUTING",
                      "RF_GET_LISTEN_MODE_ROUTING",
                      "RF_DISCOVER",
                      "RF_DISCOVER_SELECT",
                      "RF_INTF_ACTIVATED",
                      "RF_DEACTIVATED",
                      "RF_FIELD_INFO",
                      "RF_T3T_POLLING",
                      "RF_NFCEE_ACTION",
                      "RF_NFCEE_DISCOVERY_REQ",
                      "RF_PARAMETER_UPDATE"};

char* str_oid_nfcee[] = {"NFCEE_DISCOVER", "NFCEE_MODE_SET"};

char** str_table_gid[] = {
    str_oid_core, str_oid_rf, str_oid_nfcee,
};

/* Numbered tables */
char* str_nci_version[] = {  // table 6
    "NCI Version 1.0"};

char* str_configuration_status[] = {  // table 7
    "NCI RF Configuration has been kept",
    "NCI RF Configuration has been reset"};

char* str_nfcc_features0[] = {  // table 9-0
    "- Discovery Frequency value is ignored and the value of 0x01 SHALL be "
    "used by the NFCC.\n"
    "- the DH is the only entity that configures the NFCC.",
    "- Discovery Frequency configuration in RF_DISCOVER_CMD supported.\n"
    "- the DH is the only entity that configures the NFCC.",
    "- Discovery Frequency value is ignored and the value of 0x01 SHALL be "
    "used by the NFCC.\n"
    "- the NFCC can receive configurations from the DH and other NFCEEs.",
    "- Discovery Frequency configuration in RF_DISCOVER_CMD supported.\n"
    "- the NFCC can receive configurations from the DH and other NFCEEs."};
char* str_nfcc_features1[] = {  // table 9-1
    "- routing is not Supported",
    NULL,
    "- Technology based Routing; Supported",
    NULL,
    "- Protocol based routing; Supported",
    NULL,
    "- Protocol based routing; Supported",
    NULL,
    "- Technology based Routing; Supported",
    NULL,
    "- AID based routing; Supported",
    NULL,
    "- AID based routing; Supported",
    NULL,
    "- Technology based Routing; Supported",
    NULL,
    "- AID based routing; Supported",
    NULL,
    "- Protocol based routing; Supported",
    NULL,
    "- AID based routing; Supported",
    NULL,
    "- Protocol based routing; Supported",
    NULL,
    "- Technology based Routing; Supported",
    NULL};
char* str_nfcc_features2[] = {  // table 9-2
    "- off state is not Supported", "- Battery Off state; Supported",
    "- Switched Off state; Supported", "- Switched Off state; Supported",
    "- Battery Off state; Supported"};
char** str_nfcc_features[] = {
    str_nfcc_features0, str_nfcc_features1, str_nfcc_features2,
};

char* str_destination_types[] = {
    // table 12
    "RFU", "NFCC - Loop-back", "Remote NFC Endpoint", "NFCEE",
};
char* cond_destination_types(uint8_t index) {
  if (index < 0x04)
    return get_str(str_destination_types, index);
  else if (index < 0xC1)
    return "RFU";
  else if (index < 0xFF)
    return "Proprietary";
  return "RFU";
}

char* str_destination_specific[] = {  // table 15
    "First octet: An RF Discovery ID (see Table 53).\n"
    "Second octet: RF Protocol (see Table 98).",
    "First octet: NFCEE ID as defined in Table 84. The value of 0x00 (DH-NFCEE"
    " ID) SHALL NOT be used.\n"
    "Second octet: NFCEE Interface Protocol. See Table 100."
    "RFU",
    "Proprietary"};

char* str_rf_field_status[] = {
    // table 21
    "0: No operating field generated by Remote NFC endpoint",
    "1: Operating field generated by Remote NFC endpoint",
};

char* str_value_field_for_mode[] = {
    // table 43
    "Poll mode", "Listen mode", "Poll & Listem mode",
};

char* str_more_field_values[] = {  // table 44
    "Last message", "More message to follow"};

char* str_value_field_for_power_state[] = {
    "Switched on [X], Switched off [X], Battery off [X]",
    "Switched on [O], Switched off [X], Battery off [X]",
    "Switched on [X], Switched off [O], Battery off [X]",
    "Switched on [O], Switched off [O], Battery off [X]",
    "Switched on [X], Switched off [X], Battery off [O]",
    "Switched on [O], Switched off [X], Battery off [O]",
    "Switched on [X], Switched off [O], Battery off [O]",
    "Switched on [O], Switched off [O], Battery off [O]",
};

char* str_deactivation_types[] = {
    // table 62
    "Idle Mode", "Sleep Mode", "Sleep AF Mode", "Discovery",
};

char* str_deactivation_reason[] = {
    // table 63
    "DH_Request", "Endpoint Request", "RF Link Loss", "NFC_b Bad AFI",
};

char* str_tlv_for_rf_nfcee_discovery_request[] = {
    // table 65
    "To be added to the list", "To be removed from the list",
};

struct mapped_string mstr_nfcee_action_notification[] = {
    // table 68
    {0x00, "SELECT command with an AID"},
    {0x01, "RF Protocol based routing decision"},
    {0x02, "RF Technology based routing decision"},
    {0x10, "Application initiation"},
};

char* str_tlv_rf_communication_parameter_id[] = {
    // table 71
    "RF Technology and Modd", "Transmit Bit Rate", "Receive Bit Rate",
    "NFC-B Data Exchange configuration",
};

char* str_tlv_nfcee_discovery[] = {
    // table 84
    "Hardware / Registration Identification", "ATR Bytes",
    "T3T Command Set Interface Supplementary Information",
};

char* str_state_codes[] = {  // table 92
    "STATUS_OK",
    "STATUS_REJECTED",
    "STATUS_RF_FRAME_CORRUPTED",
    "STATUS_FAILED",
    "STATUS_NOT_INITIALIZED",
    "STATUS_SYNTAX_ERROR",
    "STATUS_SEMANTIC_ERROR",
    "RFU",
    "RFU",
    "STATUS_INVALID_PARAM",
    "STATUS_MESSAGE_SIZE_EXCEEDED"};

char* str_rf_technologies[] = {
    // table 93
    "TECHNOLOGY A", "TECHNOLOGY B", "TECHNOLOGY F", "TECHNOLOGY 15693",
};
char* str_rf_technologies_prop[] = {
    "TECHNOLOGY PROP--",
};
char* cond_rf_technologies(uint8_t index) {
  if (index < 0x04)
    return get_str(str_rf_technologies, index);
  else if (index < 0x80)
    return "RFU";
  else if (index < 0xFF)
    return get_str(str_rf_technologies_prop, index - 0x80);
  return "RFU";
}

struct mapped_string mstr_rf_technology_and_mode[] = {
    // table 94
    {0x00, "NFC A PASSIVE POLL MODE"},
    {0x01, "NFC B PASSIVE POLL MODE"},
    {0x02, "NFC F PASSIVE POLL MODE"},
    {0x03, "NFC A ACTIVE POLL MODE (RFU)"},
    {0x04, "RFU"},
    {0x05, "NFC F ACTIVE_POLL_MODE (RFU)"},
    {0x06, "NFC 15693 PASSIVE POLL MODE (RFU)"},
    {0x80, "NFC A PASSIVE LISTEN MODE"},
    {0x81, "NFC B PASSIVE LISTEN MODE"},
    {0x82, "NFC F PASSIVE LISTEN MODE"},
    {0x83, "NFC A ACTIVE LISTEN MODE (RFU)"},
    {0x84, "RFU"},
    {0x85, "NFC F ACTIVE LISTEN MODE (RFU)"},
    {0x86, "NFC 15693 PASSIVE LISTEN MODE (RFU)"},
};
char* cond_rf_technology_and_mode(uint8_t index) {
  if (index < 0x07)
    return get_mstr(mstr_rf_technology_and_mode, index);
  else if (index < 0x70)
    return "RFU";
  else if (index < 0x80)
    return "Reserved for Proprietary Technologies in Poll Mode";
  else if (index < 0x87)
    return get_mstr(mstr_rf_technology_and_mode, index);
  else if (index < 0xF0)
    return "RFU";
  return "Reserved for Proprietary Technologies in Listen Mode";
}

char* str_bit_rate[] = {
    // table 95
    "NFC_BIT_RATE_106: 106 Kbit/s",   "NFC_BIT_RATE_212: 212 Kbit/s",
    "NFC_BIT_RATE_424: 424 Kbit/s",   "NFC_BIT_RATE_848: 848 Kbit/s",
    "NFC_BIT_RATE_1695: 1695 Kbit/s", "NFC_BIT_RATE_3390: 3390 Kbit/s",
    "NFC_BIT_RATE_6780: 6780 Kbit/s",
};
char* cond_bit_rate(uint8_t index) {
  if (index < 0x07)
    return get_str(str_bit_rate, index);
  else if (index < 0x80)
    return "RFU";
  else if (index < 0xFF)
    return "Proprietary use";
  return "RFU";
}

char* str_rf_protocols[] = {
    // table 96
    "PROTOCOL UNDETERMINED", "PROTOCOL T1T",     "PROTOCOL T2T",
    "PROTOCOL T3T",          "PROTOCOL ISO DEP", "PROTOCOL NFC DEP",
};
char* str_rf_protocols_prop[] = {"PROTOCOL PROP--"};
char* cond_rf_protocols(uint8_t index) {
  if (index < 0x06)
    return get_str(str_rf_protocols, index);
  else if (index < 0x80)
    return "RFU";
  else if (index < 0xFF)
    return get_str(str_rf_protocols_prop, index - 0x80);
  return "RFU";
}

char* str_nfcee_protocols[] = {
    // table 98
    "APDU", "HCI Access", "T3T Command set", "Transparent",
};
char * cond_nfcee_protocols(uint8_t index) {
  if (index < 0x04)
    return get_str(str_rf_protocols, index);
  else if (index < 0x80)
    return "RFU";
  else if (index < 0xFF)
    return "For proprietary use";
  return "RFU";
}

char* str_rf_interfaces[] = {
    // table 99
    "NFCEE Direct RF Interface", "Frame RF Interface", "ISO-DEP RF Interface",
    "NFC-DEP RF Interface",      "Proprietary",
};

struct mapped_string mstr_configuration_parameters[] = {
    {0x00, "TOTAL_DURATION"},
    {0x01, "CON_DEVICES_LIMIT"},
    {0x08, "PA_BAIL_OUT"},
    {0x10, "PB_AFI"},
    {0x11, "PB_BAIL_OUT"},
    {0x12, "PB_ATTRIB_PARAM1"},
    {0x13, "PB_SENSB_REQ_PARAM"},
    {0x18, "PF_BIT_RATE"},
    {0x19, "PF_RC_CODE"},
    {0x20, "PB_H_INFO"},
    {0x21, "PI_BIT_RATE"},
    {0x22, "PA_ADV_FEAT"},
    {0x28, "PN_NFC_DEP_SPEED"},
    {0x29, "PN_ATR_REQ_GEN_BYTES"},
    {0x2A, "PN_ATR_REQ_CONFIG"},
    {0x30, "LA_BIT_FRAME_SDD"},
    {0x31, "LA_PLATFORM_CONFIG"},
    {0x32, "LA_SEL_INFO"},
    {0x33, "LA_NFCID1"},
    {0x38, "LB_SENSB_INFO"},
    {0x39, "LB_NFCID0"},
    {0x3A, "LB_APPLICATION_DATA"},
    {0x3B, "LB_SFGI"},
    {0x3C, "LB_ADC_FO"},
    {0x40, "LF_T3T_IDENTIFIERS_1"},
    {0x41, "LF_T3T_IDENTIFIERS_2"},
    {0x42, "LF_T3T_IDENTIFIERS_3"},
    {0x43, "LF_T3T_IDENTIFIERS_4"},
    {0x44, "LF_T3T_IDENTIFIERS_5"},
    {0x45, "LF_T3T_IDENTIFIERS_6"},
    {0x46, "LF_T3T_IDENTIFIERS_7"},
    {0x47, "LF_T3T_IDENTIFIERS_8"},
    {0x48, "LF_T3T_IDENTIFIERS_9"},
    {0x49, "LF_T3T_IDENTIFIERS_10"},
    {0x4A, "LF_T3T_IDENTIFIERS_11"},
    {0x4B, "LF_T3T_IDENTIFIERS_12"},
    {0x4C, "LF_T3T_IDENTIFIERS_13"},
    {0x4D, "LF_T3T_IDENTIFIERS_14"},
    {0x4E, "LF_T3T_IDENTIFIERS_15"},
    {0x4F, "LF_T3T_IDENTIFIERS_16"},
    {0x50, "LF_PROTOCOL_TYPE"},
    {0x51, "LF_T3T_PMM"},
    {0x52, "LF_T3T_MAX"},
    {0x53, "LF_T3T_FLAGS"},
    {0x54, "LF_CON_BITR_F"},
    {0x55, "LF_ADV_FEAT"},
    {0x58, "LI_FWI"},
    {0x59, "LA_HIST_BY"},
    {0x5A, "LB_H_INFO_RESP"},
    {0x5B, "LI_BIT_RATE"},
    {0x60, "LN_WT"},
    {0x61, "LN_ATR_RES_GEN_BYTES"},
    {0x62, "LN_ATR_RES_CONFIG"},
    {0x80, "RF_FIELD_INFO"},
    {0x81, "RF_NFCEE_ACTION"},
    {0x82, "NFCDEP_OP"},
    {0xFF, NULL},
};

/* CORE */
char* str_core_reset_type[] = {
    "Keep ConfigurationReset the NFCC and keep the NCI RF Configuration (if "
    "supported).",
    "Reset ConfigurationReset the NFCC including the NCI RF Configuration."};

char* cond_core_reset_reason(uint8_t index) {
  if (index == 0)
    return "Unspecified reason";
  else if (index < 0xA0)
    return "RFU";
  return "For proprietary use";
}

/* RF */
char* str_notification_type[] = {
    "Last Notification",
    "Last Notification(due to NFCC reaching it's resource limit)",
    "More Notification to follow",
};

/* Tech and Mode parameter table */
#define IS_A_POLL(rf_tech) (rf_tech == 0x00 || rf_tech == 0x03)
#define TECH_A_POLL(rf_tech)                           \
  do {                                                 \
    PRINT("--(Poll A) SENS_RES Response1", str_value); \
    NEXT_ITEM();                                       \
    PRINT("           SENS_RES Response2", str_value); \
    NEXT_ITEM();                                       \
    PRINT(" NFCID1 Length", str_value);                \
    for (len_items = *p; len_items > 0; len_items--) { \
      NEXT_ITEM();                                     \
      PRINT("  NFCID1", str_value_hex);                \
    }                                                  \
    NEXT_ITEM();                                       \
    PRINT(" SEL_RES Response Length", str_value);      \
    for (len_items = *p; len_items > 0; len_items--) { \
      NEXT_ITEM();                                     \
      PRINT("  SEL_RES Response", str_value_hex);      \
    }                                                  \
  } while (0)

#define IS_B_POLL(rf_tech) (rf_tech == 0x01)
#define TECH_B_POLL(rf_tech)                                  \
  do {                                                        \
    PRINT("--(Poll B) SENSB_RES Response Length", str_value); \
    for (len_items = *p; len_items > 0; len_items--) {        \
      NEXT_ITEM();                                            \
      PRINT("  SENSB_RES Response", str_value_hex);           \
    }                                                         \
  } while (0)

#define IS_F_POLL(rf_tech) (rf_tech == 0x02 || rf_tech == 0x05)
#define TECH_F_POLL(rf_tech)                           \
  do {                                                 \
    if (*p == 0x01)                                    \
      PRINT("--(Poll F) Bit Rate", "212 kbps");        \
    else if (*p == 0x02)                               \
      PRINT("--(Poll F) Bit Rate", "424 kbps");        \
    else                                               \
      PRINT("--(Poll F) Bit Rate", "RFU");             \
    NEXT_ITEM();                                       \
    PRINT(" SENSF_RES Response Length", str_value);    \
    for (len_items = *p; len_items > 0; len_items--) { \
      NEXT_ITEM();                                     \
      PRINT("  SENSF_RES Response", str_value_hex);    \
    }                                                  \
  } while (0)

#define IS_A_LISTEN(rf_tech) (rf_tech == 0x80 || rf_tech == 0x83)
#define IS_B_LISTEN(rf_tech) (rf_tech == 0x81)

#define IS_F_LISTEN(rf_tech) (rf_tech == 0x82 || rf_tech == 0x85)
#define TECH_F_LISTEN(rf_tech)                            \
  do {                                                    \
    PRINT("--(Listen F) Local NFCID2 Length", str_value); \
    for (len_items = *p; len_items > 0; len_items--) {    \
      NEXT_ITEM();                                        \
      PRINT("  Local NFCID2", str_value_hex);             \
    }                                                     \
  } while (0)
