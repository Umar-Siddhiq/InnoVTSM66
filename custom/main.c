
/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2013
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   main.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This app demonstrates how to send AT command with RIL API, and transparently
 *   transfer the response through MAIN UART. And how to use UART port.
 *   Developer can program the application based on this example.
 * 
 ****************************************************************************/
#ifdef __CUSTOMER_CODE__
#include "LOG.h"
#include "custom_feature_def.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_telephony.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_system.h"
#include "GPRS.h"
#include "GPS.h"
#include "Systic.h"
#include "File.h"
#include "SMS.h"
#include "SMSlib.h"
#include "Sensors.h"

#if VTS_DEBUG_LOG_ENABLE
char DBG_BUFFER[DBG_BUF_LEN]={0};
#endif
char FirmVer[15]={0};

void print_long_string(const char* long_string) {
    size_t len = Ql_strlen(long_string);
    size_t chunk_size = 100; // Max bytes per log
    size_t total_chunks = (len + chunk_size - 1) / chunk_size; // Calculate total chunks

    for (size_t i = 0; i < total_chunks; i++) {
        // Calculate the start and end indices for the current chunk
        size_t start_index = i * chunk_size;
        size_t end_index = start_index + chunk_size;
        
        // Adjust end_index if it exceeds the length of the string
        if (end_index > len) {
            end_index = len;
        }

        // Create the chunk and the header
        char chunk[chunk_size + 50]; // Extra space for header
        Ql_snprintf(chunk, sizeof(chunk), "[%zu/%zu] %.*s\n", start_index + 1, len, (int)(end_index - start_index), long_string + start_index);
        
        // Print the chunk using nwy_dbg_log
        Ql_Debug_Trace(chunk);
    }
}



void debug_uart_init(void)
{
    s32 ret = Ql_UART_Register(UART_PORT2,NULL, NULL);
    if (ret < QL_RET_OK)
    {
        LOGData(TAG_MAIN,"Fail to register serial port[%d], ret=%d\r\n", UART_PORT2, ret);
    }
    ret = Ql_UART_Open(UART_PORT2, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        LOGData(TAG_MAIN,"Fail to open serial port[%d], ret=%d\r\n", UART_PORT2, ret);
    }
}

void system_init(void)
{
    //debug_uart_init();
    #ifndef PROTO_OG
    strcpy(FirmVer,FIRMWAREVERSION);
    #else
    strcpy(FirmVer,"V1.5.2");
    #endif
    LOGData(TAG_MAIN,"OpenCPU: APM %s Firware Version %s, State/Proto: %s\r\n", PROTOVER, FirmVer, PROTO_TAG);
    InitSystic();
    hw_init();
    LoadConfig();
    LoadState();
    #ifndef PROTO_CDAC
    InitSensors();
    LoadSensorConfigFromFlash();
    #endif
    SOSInit(VTSData.IntervalData.SOSTimeOut);
    #ifdef PROTO_CDAC
    VehicleState.PacketState = NORMAL;
    VehicleState.VehicleMode = HALT;
    VehicleMovingMode = 'H';
    VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.HaltInterval;
    AlertInitStruct();
    #else
    VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
    AlertInitStruct();
    #endif
}

typedef struct {
    char *buffer;
    u32 maxLen;
    u32 actualLen;
    s32 result;
    bool done;
} ATResponseContext;

static s32 Simple_ATResponse_Handler(char* line, u32 len, void* userData)
{
    ATResponseContext *ctx = (ATResponseContext *)userData;

    if (ctx->actualLen + len < ctx->maxLen) {
        Ql_memcpy(ctx->buffer + ctx->actualLen, line, len);
        ctx->actualLen += len;
    }

    if (Ql_RIL_FindLine(line, len, "OK")) {
        ctx->result = RIL_ATRSP_SUCCESS;
        ctx->done = true;
        return RIL_ATRSP_SUCCESS;
    } else if (Ql_RIL_FindLine(line, len, "ERROR") || 
               Ql_RIL_FindString(line, len, "+CME ERROR") || 
               Ql_RIL_FindString(line, len, "+CMS ERROR:")) {
        ctx->result = RIL_ATRSP_FAILED;
        ctx->done = true;
        return RIL_ATRSP_FAILED;
    }

    return RIL_ATRSP_CONTINUE;
}

s32 SendATCommandSimple(char *atCmd, char *responseBuf, u32 maxLen, u32 timeout)
{
    ATResponseContext ctx;
    Ql_memset(&ctx, 0, sizeof(ctx));
    ctx.buffer = responseBuf;
    ctx.maxLen = maxLen;

    s32 ret = Ql_RIL_SendATCmd(atCmd, strlen(atCmd), Simple_ATResponse_Handler, &ctx, timeout);
    if (ret != RIL_ATRSP_SUCCESS) {
        return ret; // command didn't send properly
    }

    // Wait for the handler to complete (in case of async processing)
    u32 elapsed = 0;
    while (!ctx.done && elapsed < timeout) {
        ThreadSleep(100); // delay 10ms
        elapsed += 100;
    }

    return ctx.result;
}

#define BT_EVTGRP_NAME      "BT_EVETNGRP"
s32 g_nEventGrpId = -1;


void proc_main_task(s32 taskId)
{
    ST_MSG msg;

    g_nEventGrpId = Ql_OS_CreateEvent(BT_EVTGRP_NAME);
   
    LOGData(TAG_MAIN,"OpenCPU: APM Open CPU VTS M66\r\n");
    
   // LOGData("MAIN", "OpenCPU: Customer Application %s\r\n","V0.1.0");
    // START MESSAGE LOOP OF THIS TASK
    while(TRUE)
    {
        Ql_memset(&msg, 0x0, sizeof(ST_MSG));
        Ql_OS_GetMessage(&msg);
        LOGData(TAG_MAIN, "Message: %d, param1: %d,  param2: %d\r\n", msg.message, msg.param1, msg.param2);
        switch(msg.message)
        {
        case MSG_ID_RIL_READY:
            LOGData(TAG_MAIN,"<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize();
            ResetSMSContext();
            system_init();
            break;
        case MSG_ID_URC_INDICATION:
            LOGData(TAG_MAIN,"<-- Received URC: type: %d, -->\r\n", msg.param1);
            switch (msg.param1)
            {
            case URC_SYS_INIT_STATE_IND:
                {
                    LOGData(TAG_MAIN,"<-- Sys Init Status %d -->\r\n", msg.param2);
                    if (SYS_STATE_SMSOK == msg.param2)
                    {
                        LOGData(TAG_MAIN,"\r\n<-- SMS module is ready -->\r\n");
                        LOGData(TAG_MAIN,"\r\n<-- Initialize SMS-related options -->\r\n");
                        bool iResult = SMS_Initialize();         
                        if (!iResult)
                        {
                            LOGData(TAG_MAIN,"Fail to initialize SMS\r\n");
                        }
                        
                    }
                    break;
                }
                break;
            case URC_SIM_CARD_STATE_IND:
                LOGData(TAG_MAIN,"<-- SIM Card Status:%d -->\r\n", msg.param2);
                break;
            case URC_GSM_NW_STATE_IND:
                LOGData(TAG_MAIN,"<-- GSM Network Status:%d -->\r\n", msg.param2);
                break;
            case URC_GPRS_NW_STATE_IND:
                LOGData(TAG_MAIN,"<-- GPRS Network Status:%d -->\r\n", msg.param2);
                break;
            case URC_CFUN_STATE_IND:
                LOGData(TAG_MAIN,"<-- CFUN Status:%d -->\r\n", msg.param2);
                break;
            case URC_COMING_CALL_IND:
                {
                    // Here, the program demonstrates how to hang up the incoming call and call back.
                    char printBuf[100] = {0};
                    s32 callState;
                    ST_ComingCall* pComingCall = (ST_ComingCall*)msg.param2;
                    //APP_DEBUG("<-- Coming call, number:%s, type:%d -->\r\n", pComingCall->phoneNumber, pComingCall->type);
                    Ql_sprintf(printBuf, "<-- Coming call, number:%s, type:%d -->\r\n", pComingCall->phoneNumber, pComingCall->type);
                    MCOMM_SendSerial(0, (uint8_t*)printBuf, strlen(printBuf));
                    callState = RIL_Telephony_Hangup();
                    if (RIL_AT_SUCCESS == callState)
                    {
                        LOGData(TAG_MAIN,"<-- Hang up the coming call -->\r\n");
                    } else {
                        LOGData(TAG_MAIN,"<-- Fail to hang up the coming call -->\r\n");
                    }
                    
                }
            case URC_CALL_STATE_IND:
                LOGData(TAG_MAIN,"<-- Call state:%d\r\n", msg.param2);
                break;
            case URC_NEW_SMS_IND:
                LOGData(TAG_MAIN,"******************************\n<-- New SMS Arrives: index=%d\r\n", msg.param2);
                Hdlr_RecvNewSMS((msg.param2), FALSE);
                if(IsSMS)
                {
                    LOGData(TAG_MAIN,"<-- SMS Received: %s\nSender: %s\r\n", SMSData, SMSSender);
                }
                break;
            case URC_MODULE_VOLTAGE_IND:
                LOGData(TAG_MAIN,"<-- VBatt Voltage Ind: type=%d\r\n", msg.param2);
                break;
            default:
                LOGData(TAG_MAIN,"<-- Other URC: type=%d\r\n", msg.param1);
                break;
            }
            break;
        default:
            break;
        }
    }
}

#endif // __CUSTOMER_CODE__
