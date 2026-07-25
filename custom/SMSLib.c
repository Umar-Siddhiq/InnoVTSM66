#include "SMSlib.h"


ConSMSStruct g_asConSMSBuf[CON_SMS_BUF_MAX_CNT]={{0}};

static bool ConSMSBuf_IsIntact(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx,ST_RIL_SMS_Con *pCon);
static bool ConSMSBuf_AddSeg(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx,ST_RIL_SMS_Con *pCon,u8 *pData,u16 uLen);
static s8 ConSMSBuf_GetIndex(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,ST_RIL_SMS_Con *pCon);
static bool ConSMSBuf_ResetCtx(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx);



/*****************************************************************************
 * FUNCTION
 *  ConSMSBuf_GetIndex
 *
 * DESCRIPTION
 *  This function is used to get available index in <pCSBuf>
 *  
 * PARAMETERS
 *  <pCSBuf>     The SMS index in storage,it starts from 1
 *  <uCSMaxCnt>  TRUE: The module should reply a SMS to the sender; FALSE: The module only read this SMS.
 *  <pCon>       The pointer of 'ST_RIL_SMS_Con' data
 *
 * RETURNS
 *  -1:   FAIL! Can not get available index
 *  OTHER VALUES: SUCCESS.
 *
 * NOTE
 *  1. This is an internal function
 *****************************************************************************/
static s8 ConSMSBuf_GetIndex(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,ST_RIL_SMS_Con *pCon)
{
	u8 uIdx = 0;
	
    if(    (NULL == pCSBuf) || (0 == uCSMaxCnt) 
        || (NULL == pCon)
      )
    {
        LOGData(TAG_SMS,"Enter ConSMSBuf_GetIndex,FAIL! Parameter is INVALID. pCSBuf:%x,uCSMaxCnt:%d,pCon:%x\r\n",pCSBuf,uCSMaxCnt,pCon);
        return -1;
    }

    if((pCon->msgTot) > CON_SMS_MAX_SEG)
    {
        LOGData(TAG_SMS,"Enter ConSMSBuf_GetIndex,FAIL! msgTot:%d is larger than limit:%d\r\n",pCon->msgTot,CON_SMS_MAX_SEG);
        return -1;
    }
    
	for(uIdx = 0; uIdx < uCSMaxCnt; uIdx++)  //Match all exist records
	{
        if(    (pCon->msgRef == pCSBuf[uIdx].uMsgRef)
            && (pCon->msgTot == pCSBuf[uIdx].uMsgTot)
          )
        {
            return uIdx;
        }
	}

	for (uIdx = 0; uIdx < uCSMaxCnt; uIdx++)
	{
		if (0 == pCSBuf[uIdx].uMsgTot)  //Find the first unused record
		{
            pCSBuf[uIdx].uMsgTot = pCon->msgTot;
            pCSBuf[uIdx].uMsgRef = pCon->msgRef;
            
			return uIdx;
		}
	}

    LOGData(TAG_SMS,"Enter ConSMSBuf_GetIndex,FAIL! No avail index in ConSMSBuf,uCSMaxCnt:%d\r\n",uCSMaxCnt);
    
	return -1;
}

/*****************************************************************************
 * FUNCTION
 *  ConSMSBuf_AddSeg
 *
 * DESCRIPTION
 *  This function is used to add segment in <pCSBuf>
 *  
 * PARAMETERS
 *  <pCSBuf>     The SMS index in storage,it starts from 1
 *  <uCSMaxCnt>  TRUE: The module should reply a SMS to the sender; FALSE: The module only read this SMS.
 *  <uIdx>       Index of <pCSBuf> which will be stored
 *  <pCon>       The pointer of 'ST_RIL_SMS_Con' data
 *  <pData>      The pointer of CON-SMS-SEG data
 *  <uLen>       The length of CON-SMS-SEG data
 *
 * RETURNS
 *  FALSE:   FAIL!
 *  TRUE: SUCCESS.
 *
 * NOTE
 *  1. This is an internal function
 *****************************************************************************/
static bool ConSMSBuf_AddSeg(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx,ST_RIL_SMS_Con *pCon,u8 *pData,u16 uLen)
{
    u8 uSeg = 1;
    
    if(    (NULL == pCSBuf) || (0 == uCSMaxCnt) 
        || (uIdx >= uCSMaxCnt)
        || (NULL == pCon)
        || (NULL == pData)
        || (uLen > (CON_SMS_SEG_MAX_CHAR * 4))
      )
    {
        LOGData(TAG_SMS,"Enter ConSMSBuf_AddSeg,FAIL! Parameter is INVALID. pCSBuf:%x,uCSMaxCnt:%d,uIdx:%d,pCon:%x,pData:%x,uLen:%d\r\n",pCSBuf,uCSMaxCnt,uIdx,pCon,pData,uLen);
        return FALSE;
    }

    if((pCon->msgTot) > CON_SMS_MAX_SEG)
    {
        LOGData(TAG_SMS,"Enter ConSMSBuf_GetIndex,FAIL! msgTot:%d is larger than limit:%d\r\n",pCon->msgTot,CON_SMS_MAX_SEG);
        return FALSE;
    }

    uSeg = pCon->msgSeg;
    pCSBuf[uIdx].abSegValid[uSeg-1] = TRUE;
    Ql_memcpy(pCSBuf[uIdx].asSeg[uSeg-1].aData,pData,uLen);
    pCSBuf[uIdx].asSeg[uSeg-1].uLen = uLen;
    
	return TRUE;
}

/*****************************************************************************
 * FUNCTION
 *  ConSMSBuf_IsIntact
 *
 * DESCRIPTION
 *  This function is used to check the CON-SMS is intact or not
 *  
 * PARAMETERS
 *  <pCSBuf>     The SMS index in storage,it starts from 1
 *  <uCSMaxCnt>  TRUE: The module should reply a SMS to the sender; FALSE: The module only read this SMS.
 *  <uIdx>       Index of <pCSBuf> which will be stored
 *  <pCon>       The pointer of 'ST_RIL_SMS_Con' data
 *
 * RETURNS
 *  FALSE:   FAIL!
 *  TRUE: SUCCESS.
 *
 * NOTE
 *  1. This is an internal function
 *****************************************************************************/
static bool ConSMSBuf_IsIntact(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx,ST_RIL_SMS_Con *pCon)
{
    u8 uSeg = 1;
	
    if(    (NULL == pCSBuf) 
        || (0 == uCSMaxCnt) 
        || (uIdx >= uCSMaxCnt)
        || (NULL == pCon)
      )
    {
        LOGData(TAG_SMS,"Enter ConSMSBuf_IsIntact,FAIL! Parameter is INVALID. pCSBuf:%x,uCSMaxCnt:%d,uIdx:%d,pCon:%x\r\n",pCSBuf,uCSMaxCnt,uIdx,pCon);
        return FALSE;
    }

    if((pCon->msgTot) > CON_SMS_MAX_SEG)
    {
        LOGData(TAG_SMS,"Enter ConSMSBuf_GetIndex,FAIL! msgTot:%d is larger than limit:%d\r\n",pCon->msgTot,CON_SMS_MAX_SEG);
        return FALSE;
    }
        
	for (uSeg = 1; uSeg <= (pCon->msgTot); uSeg++)
	{
        if(FALSE == pCSBuf[uIdx].abSegValid[uSeg-1])
        {
            LOGData(TAG_SMS,"Enter ConSMSBuf_IsIntact,FAIL! uSeg:%d has not received!\r\n",uSeg);
            return FALSE;
        }
	}
    
    return TRUE;
}
/*****************************************************************************
 * FUNCTION
 *  Hdlr_RecvNewSMS
 *
 * DESCRIPTION
 *  The handler function of new received SMS.
 *  
 * PARAMETERS
 *  <nIndex>     The SMS index in storage,it starts from 1
 *  <bAutoReply> TRUE: The module should reply a SMS to the sender; 
 *               FALSE: The module only read this SMS.
 *
 * RETURNS
 *  VOID
 *
 * NOTE
 *  1. This is an internal function
 *****************************************************************************/
void Hdlr_RecvNewSMS(u32 nIndex, bool bAutoReply)
{
    s32 iResult = 0;
    u32 uMsgRef = 0;
    ST_RIL_SMS_TextInfo *pTextInfo = NULL;
    ST_RIL_SMS_DeliverParam *pDeliverTextInfo = NULL;
    char aPhNum[RIL_SMS_PHONE_NUMBER_MAX_LEN] = {0,};
    const char aReplyCon[] = {"Module has received SMS."};
    bool bResult = FALSE;
    
    pTextInfo = Ql_MEM_Alloc(sizeof(ST_RIL_SMS_TextInfo));
    if (NULL == pTextInfo)
    {
        LOGData(TAG_SMS,"%s/%d:Ql_MEM_Alloc FAIL! size:%u\r\n", sizeof(ST_RIL_SMS_TextInfo), __func__, __LINE__);
        return;
    }
    Ql_memset(pTextInfo, 0x00, sizeof(ST_RIL_SMS_TextInfo));
    iResult = RIL_SMS_ReadSMS_Text(nIndex, LIB_SMS_CHARSET_GSM, pTextInfo);
    if (iResult != RIL_AT_SUCCESS)
    {
        Ql_MEM_Free(pTextInfo);
        LOGData(TAG_SMS,"Fail to read text SMS[%d], cause:%d\r\n", nIndex, iResult);
        return;
    }        
    
    if ((LIB_SMS_PDU_TYPE_DELIVER != (pTextInfo->type)) || (RIL_SMS_STATUS_TYPE_INVALID == (pTextInfo->status)))
    {
        Ql_MEM_Free(pTextInfo);
        LOGData(TAG_SMS,"WARNING: NOT a new received SMS.\r\n");    
        return;
    }
    
    // Delete the SMS immediately to free SIM card memory slot
    RIL_SMS_DeleteSMS(nIndex, RIL_SMS_DEL_INDEXED_MSG);
    
    pDeliverTextInfo = &((pTextInfo->param).deliverParam);    

    if(TRUE == pDeliverTextInfo->conPres)  //Receive CON-SMS segment
    {
        s8 iBufIdx = 0;
        u8 uSeg = 0;
        u16 uConLen = 0;

        iBufIdx = ConSMSBuf_GetIndex(g_asConSMSBuf,CON_SMS_BUF_MAX_CNT,&(pDeliverTextInfo->con));
        if(-1 == iBufIdx)
        {
            LOGData(TAG_SMS,"Enter Hdlr_RecvNewSMS,WARNING! ConSMSBuf_GetIndex FAIL! Show this CON-SMS-SEG directly!\r\n");

            LOGData(TAG_SMS,
                "status:%u,type:%u,alpha:%u,sca:%s,oa:%s,scts:%s,data length:%u,cp:1,cy:%d,cr:%d,ct:%d,cs:%d\r\n",
                    (pTextInfo->status),
                    (pTextInfo->type),
                    (pDeliverTextInfo->alpha),
                    (pTextInfo->sca),
                    (pDeliverTextInfo->oa),
                    (pDeliverTextInfo->scts),
                    (pDeliverTextInfo->length),
                    pDeliverTextInfo->con.msgType,
                    pDeliverTextInfo->con.msgRef,
                    pDeliverTextInfo->con.msgTot,
                    pDeliverTextInfo->con.msgSeg
            );
            LOGData(TAG_SMS,"data = %s\r\n",(pDeliverTextInfo->data));
            Ql_strncpy(SMSData, (const char *)pDeliverTextInfo->data, MSGSIZE);
            SMSData[MSGSIZE - 1] = '\0';
            Ql_strncpy(SMSSender, pDeliverTextInfo->oa, RIL_SMS_PHONE_NUMBER_MAX_LEN);
            SMSSender[RIL_SMS_PHONE_NUMBER_MAX_LEN - 1] = '\0';
            IsSMS=1;
            Ql_MEM_Free(pTextInfo);
        
            return;
        }

        bResult = ConSMSBuf_AddSeg(
                    g_asConSMSBuf,
                    CON_SMS_BUF_MAX_CNT,
                    iBufIdx,
                    &(pDeliverTextInfo->con),
                    (pDeliverTextInfo->data),
                    (pDeliverTextInfo->length)
        );
        if(FALSE == bResult)
        {
            LOGData(TAG_SMS,"Enter Hdlr_RecvNewSMS,WARNING! ConSMSBuf_AddSeg FAIL! Show this CON-SMS-SEG directly!\r\n");

            LOGData(TAG_SMS,
                "status:%u,type:%u,alpha:%u,sca:%s,oa:%s,scts:%s,data length:%u,cp:1,cy:%d,cr:%d,ct:%d,cs:%d\r\n",
                (pTextInfo->status),
                (pTextInfo->type),
                (pDeliverTextInfo->alpha),
                (pTextInfo->sca),
                (pDeliverTextInfo->oa),
                (pDeliverTextInfo->scts),
                (pDeliverTextInfo->length),
                pDeliverTextInfo->con.msgType,
                pDeliverTextInfo->con.msgRef,
                pDeliverTextInfo->con.msgTot,
                pDeliverTextInfo->con.msgSeg
            );
            LOGData(TAG_SMS,"data = %s\r\n",(pDeliverTextInfo->data));
            Ql_strncpy(SMSData, (const char *)pDeliverTextInfo->data, MSGSIZE);
            SMSData[MSGSIZE - 1] = '\0';
            Ql_strncpy(SMSSender, pDeliverTextInfo->oa, RIL_SMS_PHONE_NUMBER_MAX_LEN);
            SMSSender[RIL_SMS_PHONE_NUMBER_MAX_LEN - 1] = '\0';
            IsSMS=1;
            Ql_MEM_Free(pTextInfo);
        
            return;
        }

        bResult = ConSMSBuf_IsIntact(
                    g_asConSMSBuf,
                    CON_SMS_BUF_MAX_CNT,
                    iBufIdx,
                    &(pDeliverTextInfo->con)
        );
        if(FALSE == bResult)
        {
            LOGData(TAG_SMS,
                "Enter Hdlr_RecvNewSMS,WARNING! ConSMSBuf_IsIntact FAIL! Waiting. cp:1,cy:%d,cr:%d,ct:%d,cs:%d\r\n",
                pDeliverTextInfo->con.msgType,
                pDeliverTextInfo->con.msgRef,
                pDeliverTextInfo->con.msgTot,
                pDeliverTextInfo->con.msgSeg
            );

            Ql_MEM_Free(pTextInfo);

            return;
        }

        // ========== FIXED SECTION: Assemble complete concatenated SMS ==========
        
        // Calculate total length of all segments
        uConLen = 0;
        for(uSeg = 1; uSeg <= pDeliverTextInfo->con.msgTot; uSeg++)
        {
            uConLen += g_asConSMSBuf[iBufIdx].asSeg[uSeg-1].uLen;
        }

        // Check if buffer is large enough
        if(uConLen >= MSGSIZE)
        {
            LOGData(TAG_SMS,"WARNING! CON-SMS too large (%u bytes), truncating to %u\r\n", 
                    uConLen, MSGSIZE-1);
            uConLen = MSGSIZE - 1;
        }

        // Copy all segments into SMSData
        u16 offset = 0;
        Ql_memset(SMSData, 0, MSGSIZE);  // Clear buffer first
        
        for(uSeg = 1; uSeg <= pDeliverTextInfo->con.msgTot; uSeg++)
        {
            u16 segLen = g_asConSMSBuf[iBufIdx].asSeg[uSeg-1].uLen;
            
            // Check remaining space
            if(offset + segLen >= MSGSIZE)
            {
                segLen = MSGSIZE - offset - 1;
            }
            
            // Copy this segment
            Ql_memcpy(SMSData + offset, 
                      g_asConSMSBuf[iBufIdx].asSeg[uSeg-1].aData, 
                      segLen);
            offset += segLen;
            
            if(offset >= MSGSIZE - 1) break;  // Buffer full
        }
        SMSData[offset] = '\0';  // Null terminate

        // Store sender phone number
        Ql_strncpy(SMSSender, pDeliverTextInfo->oa, RIL_SMS_PHONE_NUMBER_MAX_LEN);
        SMSSender[RIL_SMS_PHONE_NUMBER_MAX_LEN - 1] = '\0';

        // Set flag to notify application
        IsSMS = 1;

        // Log the complete message
        LOGData(TAG_SMS,"========== Complete CON-SMS Received ==========\r\n");
        LOGData(TAG_SMS,"status:%u, type:%u, alpha:%u\r\n",
            (pTextInfo->status),
            (pTextInfo->type),
            (pDeliverTextInfo->alpha));
        LOGData(TAG_SMS,"sca:%s, oa:%s, scts:%s\r\n",
            (pTextInfo->sca),
            (pDeliverTextInfo->oa),
            (pDeliverTextInfo->scts));
        LOGData(TAG_SMS,"Total segments:%u, Total length:%u bytes\r\n",
            pDeliverTextInfo->con.msgTot, offset);
        LOGData(TAG_SMS,"Complete data = %s\r\n", SMSData);
        LOGData(TAG_SMS,"===============================================\r\n");

        // Reset CON-SMS context
        bResult = ConSMSBuf_ResetCtx(g_asConSMSBuf, CON_SMS_BUF_MAX_CNT, iBufIdx);
        if(FALSE == bResult)
        {
            LOGData(TAG_SMS,"Enter Hdlr_RecvNewSMS,WARNING! ConSMSBuf_ResetCtx FAIL! iBufIdx:%d\r\n",iBufIdx);
        }

        Ql_MEM_Free(pTextInfo);
        
        return;
    }
    
    // Handle single (non-concatenated) SMS
    LOGData(TAG_SMS,"<-- RIL_SMS_ReadSMS_Text OK. eCharSet:LIB_SMS_CHARSET_GSM,nIndex:%u -->\r\n",nIndex);
    LOGData(TAG_SMS,"status:%u,type:%u,alpha:%u,sca:%s,oa:%s,scts:%s,data length:%u\r\n",
        pTextInfo->status,
        pTextInfo->type,
        pDeliverTextInfo->alpha,
        pTextInfo->sca,
        pDeliverTextInfo->oa,
        pDeliverTextInfo->scts,
        pDeliverTextInfo->length);
    LOGData(TAG_SMS,"data = %s\r\n",(pDeliverTextInfo->data));
    
    // Debug: Log raw data in hex to see if there are null bytes
    LOGData(TAG_SMS,"=== RAW SMS DATA (hex) ===\r\n");
    char hexBuf[200];
    int hexPos = 0;
    for(int i = 0; i < pDeliverTextInfo->length && i < 80; i++) {
        hexPos += Ql_sprintf(hexBuf + hexPos, "%02X ", pDeliverTextInfo->data[i]);
        if((i+1) % 16 == 0 || i == pDeliverTextInfo->length - 1) {
            LOGData(TAG_SMS,"%s\r\n", hexBuf);
            hexPos = 0;
            Ql_memset(hexBuf, 0, sizeof(hexBuf));
        }
    }
    LOGData(TAG_SMS,"=== END RAW DATA ===\r\n");
    
    // Copy SMS data directly - GSM 7-bit special chars now fixed in RIL layer
    Ql_memset(SMSData, 0, MSGSIZE);
    int copyLen = (pDeliverTextInfo->length < MSGSIZE - 1) ? pDeliverTextInfo->length : (MSGSIZE - 1);
    Ql_memcpy(SMSData, pDeliverTextInfo->data, copyLen);
    SMSData[copyLen] = '\0';

    Ql_strncpy(SMSSender, pDeliverTextInfo->oa, RIL_SMS_PHONE_NUMBER_MAX_LEN);
    SMSSender[RIL_SMS_PHONE_NUMBER_MAX_LEN - 1] = '\0';
    IsSMS=1;
    
    Ql_strcpy(aPhNum, pDeliverTextInfo->oa);
    Ql_MEM_Free(pTextInfo);
    
    if (bAutoReply)
    {
        if (!Ql_strstr(aPhNum, "10086"))  // Not reply SMS from operator
        {
            LOGData(TAG_SMS,"<-- Replying SMS... -->\r\n");
            iResult = RIL_SMS_SendSMS_Text(aPhNum, Ql_strlen(aPhNum),LIB_SMS_CHARSET_GSM,(u8*)aReplyCon,Ql_strlen(aReplyCon),&uMsgRef);
            if (iResult != RIL_AT_SUCCESS)
            {
                LOGData(TAG_SMS,"RIL_SMS_SendSMS_Text FAIL! iResult:%u\r\n",iResult);
                return;
            }
            LOGData(TAG_SMS,"<-- RIL_SMS_SendTextSMS OK. uMsgRef:%d -->\r\n", uMsgRef);
        }
    }
    return;
}

void ResetSMSContext(void)
{
    s32 i;
    for(i = 0; i < CON_SMS_BUF_MAX_CNT; i++)
    {
        ConSMSBuf_ResetCtx(g_asConSMSBuf,CON_SMS_BUF_MAX_CNT,i);
    }
    LOGData(TAG_SMS,"<-- SMS context reset -->\r\n");
}

/*****************************************************************************
 * FUNCTION
 *  ConSMSBuf_ResetCtx
 *
 * DESCRIPTION
 *  This function is used to reset ConSMSBuf context
 *  
 * PARAMETERS
 *  <pCSBuf>     The SMS index in storage,it starts from 1
 *  <uCSMaxCnt>  TRUE: The module should reply a SMS to the sender; FALSE: The module only read this SMS.
 *  <uIdx>       Index of <pCSBuf> which will be stored
 *
 * RETURNS
 *  FALSE:   FAIL!
 *  TRUE: SUCCESS.
 *
 * NOTE
 *  1. This is an internal function
 *****************************************************************************/
static bool ConSMSBuf_ResetCtx(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx)
{
    if(    (NULL == pCSBuf) || (0 == uCSMaxCnt) 
        || (uIdx >= uCSMaxCnt)
      )
    {
        LOGData(TAG_SMS,"Enter ConSMSBuf_ResetCtx,FAIL! Parameter is INVALID. pCSBuf:%x,uCSMaxCnt:%d,uIdx:%d\r\n",pCSBuf,uCSMaxCnt,uIdx);
        return FALSE;
    }
    
    //Default reset
    Ql_memset(&pCSBuf[uIdx],0x00,sizeof(ConSMSStruct));

    //TODO: Add special reset here
    
    return TRUE;
}
uint8_t IsSMSInit=0;
/*****************************************************************************
 * FUNCTION
 *  SMS_Initialize
 *
 * DESCRIPTION
 *  Initialize SMS environment.
 *  
 * PARAMETERS
 *  VOID
 *
 * RETURNS
 *  TRUE:  This function works SUCCESS.
 *  FALSE: This function works FAIL!
 *****************************************************************************/
bool SMS_Initialize(void)
{
    s32 iResult = 0;
    u8  nCurrStorage = 0;
    u32 nUsed = 0;
    u32 nTotal = 0;
    
    // Set SMS storage:
    // By default, short message is stored into SIM card. You can change the storage to ME if needed, or
    // you can do it again to make sure the short message storage is SIM card.
    #if 1
    {
        iResult = RIL_SMS_SetStorage(RIL_SMS_STORAGE_TYPE_SM,&nUsed,&nTotal);
        if (RIL_ATRSP_SUCCESS != iResult)
        {
            LOGData(TAG_SMS,"Fail to set SMS storage, cause:%d\r\n", iResult);
            return FALSE;
        }
        LOGData(TAG_SMS,"<-- Set SMS storage to SM, nUsed:%u,nTotal:%u -->\r\n", nUsed, nTotal);

        iResult = RIL_SMS_GetStorage(&nCurrStorage, &nUsed ,&nTotal);
        if(RIL_ATRSP_SUCCESS != iResult)
        {
            LOGData(TAG_SMS,"Fail to get SMS storage, cause:%d\r\n", iResult);
            return FALSE;
        }
        LOGData(TAG_SMS,"<-- Check SMS storage: curMem=%d, used=%d, total=%d -->\r\n", nCurrStorage, nUsed, nTotal);
    }
    #endif

    // Enable new short message indication
    // By default, the auto-indication for new short message is enalbed. You can do it again to 
    // make sure that the option is open.
    #if 1
    {
        iResult = Ql_RIL_SendATCmd("AT+CNMI=2,1",Ql_strlen("AT+CNMI=2,1"),NULL,NULL,0);
        if (RIL_AT_SUCCESS != iResult)
        {
            LOGData(TAG_SMS,"Fail to send \"AT+CNMI=2,1\", cause:%d\r\n", iResult);
            return FALSE;
        }
        LOGData(TAG_SMS,"<-- Enable new SMS indication -->\r\n");
    }
    #endif

    // Delete all existed short messages (if needed)
    iResult = RIL_SMS_DeleteSMS(0, RIL_SMS_DEL_ALL_MSG);
    if (iResult != RIL_AT_SUCCESS)
    {
        LOGData(TAG_SMS,"Fail to delete all messages, iResult=%d,cause:%d\r\n", iResult, Ql_RIL_AT_GetErrCode());
        return FALSE;
    }
    LOGData(TAG_SMS,"Delete all existed messages\r\n");

    
    IsSMSInit=1;
    LOGData(TAG_SMS,"<-- SMS environment initialization OK -->\r\n");
    return TRUE;
}


#define MAX_GSM_SEG_LEN     153
#define MAX_UCS2_SEG_LEN    67

void SMS_SendTextMessage(char* phoneNumber, const char* message, bool isUCS2)
{
    s32 iResult;
    u32 nMsgRef;
    static u8 g_msgRef = 1;
    u16 totalLen = Ql_strlen(message);
    u16 segLen;
    u8 totalSeg;
    u8 i;
    char segmentBuf[161] = {0};  // Ensure big enough buffer
    u8 charset = isUCS2 ? LIB_SMS_CHARSET_UCS2 : LIB_SMS_CHARSET_GSM;

    LOGData(TAG_SMS, "< SMS Send Begin,phoneNumber:%s, UCS2=%d, TotalLen=%d >\r\n",phoneNumber, isUCS2, totalLen);
    LOGData(TAG_SMS, "Message: %s\r\n", message);

    // Determine segment length
    segLen = isUCS2 ? MAX_UCS2_SEG_LEN : MAX_GSM_SEG_LEN;
    totalSeg = (totalLen + segLen - 1) / segLen;

    u8 msgRef = g_msgRef++ & 0xFF;
    if (g_msgRef == 0) g_msgRef = 1;

    for (i = 0; i < totalSeg; i++) {
        ST_RIL_SMS_SendExt sExt;
        Ql_memset(&sExt, 0x00, sizeof(sExt));

        u16 copyLen = (i == totalSeg - 1) ? (totalLen - i * segLen) : segLen;

        if (isUCS2 && (copyLen % 2 != 0)) {
            copyLen--;  // avoid half UCS2 character
        }

        Ql_memset(segmentBuf, 0, sizeof(segmentBuf));
        Ql_memcpy(segmentBuf, message + (i * segLen), copyLen);

        if (totalSeg > 1) {
            sExt.conPres = TRUE;
            sExt.con.msgType = LIB_SMS_UD_TYPE_CON_DEFAULT; // or _7_BYTE
            sExt.con.msgRef = msgRef;
            sExt.con.msgTot = totalSeg;
            sExt.con.msgSeg = i + 1;
        }

        iResult = RIL_SMS_SendSMS_Text_Ext(
            phoneNumber, Ql_strlen(phoneNumber),
            charset, (u8*)segmentBuf, copyLen,
            &nMsgRef, &sExt
        );

        if (iResult != RIL_AT_SUCCESS) {
            LOGData(TAG_SMS, "< SMS Seg %d/%d Failed, cause=%d >\r\n",
                i + 1, totalSeg, Ql_RIL_AT_GetErrCode());
            return;
        }

        LOGData(TAG_SMS, "< SMS Seg %d/%d Sent, MsgRef=%u >\r\n",
            i + 1, totalSeg, nMsgRef);

        Ql_Sleep(500); // optional wait
    }
}




