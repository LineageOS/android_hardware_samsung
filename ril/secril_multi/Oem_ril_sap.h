/*
 * Copyright (C) 2009 Samsung Electronics, Co. Ltd.
 * All rights reserved.
 *
 * Oem_ril_sap.h
 *
 * author: young jin Park (lucky29.park@samsung.com)
 * data : 20100129 
*/

#ifndef __OEM_RIL_SAP_H__
#define __OEM_RIL_SAP_H__

/**
 * OEM request header data(for RIL_REQUEST_OEM_HOOK_RAW)

typedef struct _OemReqDataHdr
{
	char func_id;
	char sub_func_id;
	char len;
} __attribute__((packed)) OemReqDataHdr;


 * OEM request data(for RIL_REQUEST_OEM_HOOK_RAW)
 
typedef struct _OemReqData
{
	OemReqDataHdr hdr;
	char *payload;
} __attribute__((packed)) OemReqData;

**/

/* -- OEM RIL SAP Main Cmd --*/
#define OEM_FUNCTION_ID_SAP								0x14

/* -- OEM RIL SAP SUB FUNCTION ID -- */

#define OEM_SAP_CONNECT									0x01
/** OEM_SAP_CONNECT	
   Req - Format info
     1. MSG ID (1 byte)
     
   Res - 	Format info
     1. MSG ID (1 byte)
     2. CONNECTION STATUS (1 byte)
     3. MAX MSG SIZE (2 byte)

   Noti(Unsol) - Format info
     1. DISCONNECT TYPE (1 byte)   
**/    

#define OEM_SAP_STATUS									0x02
/** OEM_SAP_STATUS	
   Req - Format info
     non.
     
   Res - 	Format info
     1. SAP Status (1 byte)    

   Noti(Unsol) - Format info
     1. Card Status (1 byte)   
**/    

#define OEM_SAP_READER_STATUS							0x03
/** OEM_SAP_READER_STATUS	
   Req - Format info
     non.
     
   Res - 	Format info
     1. Result code (1 byte)   
     2. Card reader status (1 byte)

   Noti(Unsol) - Format info
    non.
**/    

#define OEM_SAP_SIM_POWER								0x04
/** OEM_SAP_SIM_POWER	
   Req - Format info
     1. MSG ID (1 byte)
     
   Res - 	Format info
     1. MSG ID (1 byte)
     2. Result code (1 byte)   
    
   Noti(Unsol) - Format info
    non.
**/  

#define OEM_SAP_TRANSFER_ATR							0x05
/** OEM_SAP_TRANSFER_ATR	
   Req - Format info
   non.
     
   Res - 	Format info
     1. Result code (1 byte)
     2. ATR length (2 byte)   
     3. ATR (variables) 
     
   Noti(Unsol) - Format info
    non.
**/  

#define OEM_SAP_TRANSFER_APDU							0x06
/** OEM_SAP_TRANSFER_APDU	
   Req - Format info
     1. APDU length(2 byte)
     2. commadn apdu or apdu_7816  (variables) 
     
   Res - 	Format info
     1. Result code (1 byte)
     2. Res APDU length (2 byte)   
     3. Res APDU (variables) 
     
   Noti(Unsol) - Format info
    non.
**/

#define OEM_SAP_SET_PROTOCOL							0x07
/** OEM_SAP_SET_PROTOCOL	
   Req - Format info
     1. Transport protocol (1 byte)
     
   Res - 	Format info
     1. Result code (1 byte)

   Noti(Unsol) - Format info
    non.
**/ 


/*MAX_MSG_SIZE */
#define MAX_MSG_SIZE									512 // 256->512	

/* MSG_ID Table */ 
#define OEM_SAP_CONNECT_REQ								0x00 /*Client -> Server*/
#define OEM_SAP_CONNECT_RESP							0x01 /*Server -> Client */
#define OEM_SAP_DISCONNECT_REQ							0x02 /*Client -> Server*/
#define OEM_SAP_DISCONNECT_RESP							0x03 /*Server -> Client */
#define OEM_SAP_DISCONNECT_IND							0x04 /*Server -> Client */
#define OEM_SAP_TRANSFER_APDU_REQ						0x05 /*Client -> Server*/
#define OEM_SAP_TRANSFER_APDU_RESP						0x06 /*Server -> Client */
#define OEM_SAP_TRANSFER_ATR_REQ						0x07 /*Client -> Server*/
#define OEM_SAP_TRANSFER_ATR_RESP						0x08 /*Server -> Client */
#define OEM_SAP_POWER_SIM_OFF_REQ						0x09 /*Client -> Server*/
#define OEM_SAP_POWER_SIM_OFF_RESP						0x0A /*Server -> Client */
#define OEM_SAP_POWER_SIM_ON_REQ						0x0B /*Client -> Server*/
#define OEM_SAP_POWER_SIM_ON_RESP						0x0C /*Server -> Client */
#define OEM_SAP_RESET_SIM_REQ							0x0D /*Client -> Server*/
#define OEM_SAP_RESET_SIM_RESP							0x0E /*Server -> Client */
#define OEM_SAP_TRANSFER_CARD_READER_STATUS_REQ			0x0F /*Client -> Server*/
#define OEM_SAP_TRANSFER_CARD_READER_STATUS_RESP		0x10 /*Server -> Client */
#define OEM_SAP_STATUS_IND								0x11 /*Client -> Server*/
#define OEM_SAP_ERROR_RESP								0x12 /*Server -> Client */
#define OEM_SAP_SET_TRANSPORT_PROTOCOL_REQ				0x13 /*Client -> Server*/
#define OEM_SAP_SET_TRANSPORT_PROTOCOL_RESP				0x14 /*Server -> Client */

/*CONNECTIN STATUS */ 
#define OEM_SAP_CONNECT_OK								0x00
#define OEM_SAP_CONNECT_UNABLE_ESTABLISH				0x01
#define OEM_SAP_CONNECT_NOT_SUPPORT_MAX_SIZE			0x02
#define OEM_SAP_CONNECT_TOO_SMALL_MAX_SIZE				0x03

/*DISCONNECT TYPE */ 
#define OEM_SAP_DISCONNECT_TYPE_GRACEFUL				0x00
#define OEM_SAP_DISCONNECT_TYPE_IMMEDIATE				0x01

/*SAP STATUS */
#define OEM_SAP_STATUS_UNKNOWN							0x00
#define OEM_SAP_STATUS_NO_SIM							0x01
#define OEM_SAP_STATUS_NOT_READY						0x02
#define OEM_SAP_STATUS_READY							0x03
#define OEM_SAP_STATUS_CONNECTED						0x04

/*CARD STATUS */
#define OEM_SAP_CARD_STATUS_UNKNOWN						0x00
#define OEM_SAP_CARD_STATUS_RESET						0x01
#define OEM_SAP_CARD_STATUS_NOT_ACCESSIBLE				0x02
#define OEM_SAP_CARD_STATUS_REMOVED						0x03
#define OEM_SAP_CARD_STATUS_INSERTED					0x04
#define OEM_SAP_CARD_STATUS_RECOVERED					0x05

/*RESULT CODE */
#define OEM_SAP_RESULT_OK								0x00
#define OEM_SAP_RESULT_NO_REASON						0x01
#define OEM_SAP_RESULT_CARD_NOT_ACCESSIBLE				0x02
#define OEM_SAP_RESULT_CARD_ALREADY_POWER_OFF			0x03
#define OEM_SAP_RESULT_REMOVED							0x04
#define OEM_SAP_RESULT_ALREADY_POWER_ON					0x05
#define OEM_SAP_RESULT_DATA_NOT_AVAILABLE				0x06
#define OEM_SAP_RESULT_NOT_SUPPORT						0x07

/*TRANSPORT PROTOCOL*/
#define OEM_SAP_TRANSPORT_PROTOCOL_T_ZERO				0x00
#define OEM_SAP_TRANSPORT_PROTOCOL_T_ONE				0x01


typedef struct {
	uint8_t		func_id;
	uint8_t		cmd;
	uint16_t	len;
} __attribute__((packed)) oem_ril_sap_hdr;

typedef struct {    
    uint8_t 	msg_id;  
} __attribute__((packed)) ril_sap_req_sap_connect;


typedef struct {    
	uint16_t 	apdu_len;
	uint8_t 	apdu[MAX_MSG_SIZE];	
} __attribute__((packed)) ril_sap_req_transfer_apdu;

typedef struct {    
    uint8_t 	transport_protocol;  
} __attribute__((packed)) ril_sap_req_transport_protocol;


typedef struct {    
    uint8_t 	msg_id;  
} __attribute__((packed)) ril_sap_req_sim_power;


typedef struct {    
    uint8_t 	msg_id;
    uint8_t 	connection_status;
    uint16_t 	max_msg_size;
} __attribute__((packed)) ril_sap_res_connect;

typedef struct {    
    uint8_t 	sap_status;  
} __attribute__((packed)) ril_sap_res_sap_status;

typedef struct {    
    uint8_t 	result_code; 
	uint16_t 	atr_len;
	uint8_t atr[MAX_MSG_SIZE];	
} __attribute__((packed)) ril_sap_res_transfer_atr;

typedef struct {    
    uint8_t 	result_code; 
	uint16_t 	res_apdu_len;
	uint8_t res_apdu[MAX_MSG_SIZE];	
} __attribute__((packed)) ril_sap_res_transfer_apdu;

typedef struct {    
    uint8_t 	result_code;  
} __attribute__((packed)) ril_sap_res_transport_protocol;

typedef struct {    
	uint8_t 	msg_id;
    uint8_t 	result_code;  
} __attribute__((packed)) ril_sap_res_sim_power;

typedef struct {    	
    uint8_t 	result_code;  
	uint8_t 	card_reader_status;
} __attribute__((packed)) ril_sap_res_card_reader_status;

typedef struct {
	uint8_t		disconnect_type;
} __attribute__((packed)) unsol_sap_connect;

typedef struct {
	uint8_t		card_status;
} __attribute__((packed)) unsol_sap_status;

typedef union {
	unsol_sap_connect		connect;
	unsol_sap_status		status;	
} __attribute__((packed)) unsol_sap_parameters;

typedef struct {
	uint8_t sub_id;
	unsol_sap_parameters param;
} __attribute__((packed)) unsol_sap_notification;




#endif

