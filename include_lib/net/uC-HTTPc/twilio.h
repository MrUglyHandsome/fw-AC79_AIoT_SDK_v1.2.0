/*
*********************************************************************************************************
*                                            EXAMPLE CODE
*
*               This file is provided as an example on how to use Micrium products.
*
*               Please feel free to use any application code labeled as 'EXAMPLE CODE' in
*               your application products.  Example code may be used as is, in whole or in
*               part, or may be used as a reference only. This file can be modified as
*               required to meet the end-product requirements.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                    HTTP CLIENT TEST SOURCE CODE
*
* Filename : twilio.h
* Version  : V3.01.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  TWILIO_MODULE_PRESENT
#define  TWILIO_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "http-c.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef   TWILIO_MODULE
#define  TWILIO_EXT
#else
#define  TWILIO_EXT  extern
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        DEFAULT CONFIGURATION
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  TWILIO_CFG_REQ_NBR_MAX                        2u

#define  TWILIO_CFG_CONN_NBR_MAX                       3u
#define  TWILIO_CFG_CONN_BUF_SIZE                   4096u

#define  TWILIO_CFG_HDR_NBR_MAX                       10u
#define  TWILIO_CFG_HDR_VAL_LEN_MAX                  300u

#define  TWILIO_CFG_FORM_BUF_SIZE                    256u
#define  TWILIO_CFG_FORM_FIELD_NBR_MAX                 3u
#define  TWILIO_CFG_FORM_FIELD_KEY_LEN_MAX           100u
#define  TWILIO_CFG_FORM_FIELD_VAL_LEN_MAX           200u
#define  TWILIO_CFG_FORM_MULTIPART_NAME_LEN_MAX      100u
#define  TWILIO_CFG_FORM_MULTIPART_FILENAME_LEN_MAX  100u

#define  TWILIO_URL_LEN_MAX                          100u

#define  TWILIO_API_HOSTNAME                   "api.twilio.com"

#define  TWILIO_URL_MESSAGES                   "/2010-04-01/Accounts/"

/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

TWILIO_EXT   CPU_CHAR             Twilio_Buf[1024];


TWILIO_EXT  CPU_CHAR              Twilio_FormBuf[TWILIO_CFG_FORM_BUF_SIZE];

TWILIO_EXT  HTTPc_KEY_VAL         Twilio_FormKeyValTbl[TWILIO_CFG_FORM_FIELD_NBR_MAX];
TWILIO_EXT  CPU_CHAR              Twilio_FormKeyStrTbl[2 * TWILIO_CFG_FORM_FIELD_NBR_MAX][TWILIO_CFG_FORM_FIELD_KEY_LEN_MAX];
TWILIO_EXT  CPU_CHAR              Twilio_FormValStrTbl[2 * TWILIO_CFG_FORM_FIELD_NBR_MAX][TWILIO_CFG_FORM_FIELD_VAL_LEN_MAX];
TWILIO_EXT  HTTPc_FORM_TBL_FIELD  Twilio_FormTbl[TWILIO_CFG_FORM_FIELD_NBR_MAX];


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/


CPU_BOOLEAN Twilio_SendSMS(CPU_CHAR           *p_from_phone_number,
                           CPU_CHAR           *p_to_phone_number,
                           CPU_CHAR           *p_message,
                           CPU_CHAR           *p_account_id,
                           CPU_CHAR           *p_auth_token);



void        Twilio_RespBodyHook(HTTPc_CONN_OBJ     *p_conn,
                                HTTPc_REQ_OBJ      *p_req,
                                HTTP_CONTENT_TYPE   content_type,
                                void               *p_data,
                                CPU_INT16U          data_len,
                                CPU_BOOLEAN         last_chunk);



/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* TWILIO_MODULE_PRESENT */