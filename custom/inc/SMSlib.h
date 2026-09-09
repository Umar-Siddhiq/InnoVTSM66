#ifndef __SMS_LIB_C__
#define __SMS_LIB_C__
#include "ril.h"
#include "ril_util.h"
#include "ril_sms.h"
#include "ril_telephony.h"
#include "ril_system.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_system.h"
#include "ql_memory.h"
#include "ql_timer.h"

#include "SMS.h"

/***********************************************************************
 * MACRO CONSTANT DEFINITIONS
************************************************************************/

#define CON_SMS_BUF_MAX_CNT   (1)
#define CON_SMS_SEG_MAX_CHAR  (160)
#define CON_SMS_SEG_MAX_BYTE  (4 * CON_SMS_SEG_MAX_CHAR)
#define CON_SMS_MAX_SEG       (7)

/***********************************************************************
 * STRUCT TYPE DEFINITIONS
************************************************************************/
typedef struct
{
    u8 aData[CON_SMS_SEG_MAX_BYTE];
    u16 uLen;
} ConSMSSegStruct;

typedef struct
{
    u16 uMsgRef;
    u8 uMsgTot;

    ConSMSSegStruct asSeg[CON_SMS_MAX_SEG];
    bool abSegValid[CON_SMS_MAX_SEG];
} ConSMSStruct;
extern uint8_t IsSMSInit;
/***********************************************************************
 * FUNCTION DECLARATIONS
************************************************************************/

void Hdlr_RecvNewSMS(u32 nIndex, bool bAutoReply);
bool SMS_Initialize(void);
void ResetSMSContext(void);
bool SMS_SendTextMessage(char* phoneNumber, const char* message, bool isUCS2);
#endif
