
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
#include "ql_power.h"
#include "GPRS.h"
#include "GPS.h"
#include "Systic.h"
#include "File.h"
#include "FTP.h"
#include "SMS.h"
#include "SMSlib.h"
#include "Alert.h"
#include "SystemRecovery.h"
#include "fota_main.h"

#if VTS_DEBUG_LOG_ENABLE
char DBG_BUFFER[DBG_BUF_LEN]={0};
static u32 s_logMutex = 0;

/* Ql_Debug_Trace is limited to 512 bytes and treats its first argument as a
 * printf format string.  All application tasks used to share DBG_BUFFER and
 * pass that mutable buffer as the format argument, so concurrent logs (or a
 * '%' in a payload) could make the trace engine read arbitrary memory. */
void LogData_Init(void)
{
    if (s_logMutex == 0)
        s_logMutex = Ql_OS_CreateMutex("VTS_LOG");
}

void LogData_Lock(void)
{
    if (s_logMutex != 0)
        Ql_OS_TakeMutex(s_logMutex);
}

void LogData_Unlock(void)
{
    if (s_logMutex != 0)
        Ql_OS_GiveMutex(s_logMutex);
}
#endif
char FirmVer[15]={0};

/*
 * Keep PWRKEY power-off under application control.  The board's PWRKEY line
 * must not be allowed to turn a brief low pulse into a module power cycle.
 * Intentional software resets continue to use their explicit Ql_Reset paths.
 */
static void PowerKeyIndication(s32 operation, s32 keyState)
{
    if (operation == POWER_OFF)
    {
        LOGData(TAG_MAIN,
                "PWRKEY power-off indication ignored: state=%d (automatic power-off disabled)",
                (int)keyState);
    }
    else
    {
        LOGData(TAG_MAIN, "PWRKEY indication: operation=%d state=%d",
                (int)operation, (int)keyState);
    }
}

static void RegisterPowerKeyProtection(void)
{
    s32 ret = Ql_PwrKey_Register(PowerKeyIndication);
    if (ret < QL_RET_OK)
    {
        LOGData(TAG_MAIN, "PWRKEY callback registration failed: ret=%d", ret);
    }
    else
    {
        LOGData(TAG_MAIN, "PWRKEY protection active: automatic power-off disabled");
    }
}

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
        /* LOG CLEANUP 2026-05-16: %zu not supported by Ql_vsnprintf on M66
         * → triggers "Ql_vsnprintf() is not supported" log spam every chunk.
         * Cast to int and use %d. OLD CODE:
         *   Ql_snprintf(chunk, sizeof(chunk), "[%zu/%zu] %.*s\n", start_index + 1, len, ...);
         */
        Ql_snprintf(chunk, sizeof(chunk), "[%d/%d] %.*s\n",
                    (int)(start_index + 1), (int)len,
                    (int)(end_index - start_index), long_string + start_index);

        /* Must pass chunk as an ARGUMENT, not as the format string: packet
         * payloads are attacker/server-influenced data and any '%' in them
         * would be parsed as a conversion specifier, making Ql_Debug_Trace
         * read non-existent varargs (garbage pointer deref on "%s"). */
        Ql_Debug_Trace("%s", chunk);
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
    AlertInitStruct();
    #ifndef PROTO_OG
    strcpy(FirmVer,FIRMWAREVERSION);
    #else
    Ql_sprintf(FirmVer,"%s",FIRMWAREVERSION);
    #endif
    LOGData(TAG_MAIN,"OpenCPU: APM %s Firware Version %s, State/Proto: %s\r\n", PROTOVER, FirmVer, PROTO_TAG);
    InitSystic();
    hw_init();
    LoadConfig();
    LoadState();
    LoadActiveProfile();
    
    extern uint8_t IsFTPReq;
    LoadFTPConfig(&DownloadReq);
    if (DownloadReq.IsValid == FOTA_REQ_VALID_CODE)
    {
        IsFTPReq = 1;

        /* Consume the persisted request NOW, so the auto-resume is ONE-SHOT.
         *
         * The resume runs inside the server thread (handleFTPRequests(), the
         * third call in its 100 ms loop) and FTPStart() begins with a silent
         * multi-hundred-operation UFS sweep before it logs anything. If that
         * stalls, the server thread never reaches any packet handling and the
         * device sends nothing but the single login it managed beforehand —
         * and because the request stayed valid in flash, the next boot armed
         * the very same stall again, forever. Invalidating flash here keeps
         * this boot's attempt (the in-RAM copy is still valid) while
         * guaranteeing a wedged or reset attempt cannot re-arm itself. */
        {
            download_req_info_s consumed = DownloadReq;
            consumed.IsValid = 0;
            UpdateFTPConfigInFlash(&consumed);
        }
        LOGData(TAG_MAIN, "Pending FOTA/MOTA request at boot: one-shot auto-resume armed, flash record consumed");
    }
    /* ------------------------------------------------------------------
     * DIAGNOSTIC LOGGING SYSTEM — Load persistent counters at boot
     * Added : 2026-05-13
     *
     * LoadDiag() reads Diag.bin from flash. On first boot after firmware
     * update (or if Diag.bin is missing/corrupted), LoadDefaultDiag()
     * is called automatically which zeros all counters and creates the file.
     * This call MUST come after LoadState() so VTSState.CurrentProfile
     * is valid when PushDiagEvent(BOOT) is called inside SystemRecovery_Init().
     *
     * OLD CODE: (LoadDiag did not exist)
     * ------------------------------------------------------------------ */
    LoadDiag();
    LOGData(TAG_BOOT, "=== BOOT COMPLETE === FW:%s SIM:%s Profile:%d BootCount:%lu ===",
            FirmVer, SIM_MAKE_STR, VTSState.CurrentProfile, DiagCounters.BootCount);
    SOSInit(VTSData.IntervalData.SOSTimeOut);
    #ifdef PROTO_CDAC
    VehicleState.PacketState = NORMAL;
    VehicleState.VehicleMode = HALT;
    VehicleMovingMode = 'H';
    VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.HaltInterval;
    #else
    VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
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

#if VTS_DEBUG_LOG_ENABLE
    LogData_Init();
#endif

    RegisterPowerKeyProtection();

    /* LOG CLEANUP 2026-05-16: Duplicate of line ~101 banner which already
     * shows FW version + protocol + tag. This line carried no extra info.
     * OLD CODE:
     *   LOGData(TAG_MAIN,"OpenCPU: APM Open CPU VTS M66\r\n");
     */

    /* ------------------------------------------------------------------
     * FIX: Start watchdog HERE — before Ql_OS_GetMessage is first called
     * DATE  : 2026-05-16
     *
     * ROOT CAUSE OF PREVIOUS FAILURE:
     *   Ql_WTD_Start() was called inside SystemRecovery_Init() which runs
     *   from case MSG_ID_RIL_READY: inside the message loop below. On the
     *   M66, Ql_WTD_Start() returns QL_RET_ERR_PARAM (-1) when invoked
     *   from inside a message handler — regardless of the interval value.
     *   Even Ql_WTD_Start(1000) failed with ret=-1 in that context.
     *   The retry-from-FeedWatchdog also ran in the wrong context (Systic
     *   thread after the loop started) and failed the same way.
     *
     * THE RULE (from Quectel example_watchdog.c):
     *   Ql_WTD_Start() MUST be called BEFORE the while(TRUE) message loop.
     *   After Ql_OS_GetMessage() is first called, the OS is in "message
     *   processing" state and WDT registration is no longer allowed.
     *
     * HOW THIS WORKS:
     *   SystemRecovery_EarlyWatchdogStart() calls Ql_WTD_Start() here,
     *   in the correct pre-loop context. It tries the full fallback table
     *   before Ql_OS_GetMessage() is entered and stores the returned ID in
     *   s_watchdogId inside SystemRecovery.c. SystemRecovery_Init() later
     *   only logs/persists status; FeedWatchdog() just feeds the active WDT.
     *
     * REVERT:
     *   Remove the SystemRecovery_EarlyWatchdogStart() call below.
     *   Restore SystemRecovery_StartWatchdog() call inside Init() (old code
     *   comment is preserved in SystemRecovery.c).
     *
     * OLD CODE: (no pre-loop WDT call — WDT was started from MSG_ID_RIL_READY)
     * ------------------------------------------------------------------ */
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_WATCHDOG_ENABLE
    SystemRecovery_EarlyWatchdogStart();
#endif

    // START MESSAGE LOOP OF THIS TASK
    while(TRUE)
    {
        Ql_memset(&msg, 0x0, sizeof(ST_MSG));
        Ql_OS_GetMessage(&msg);
        /* LOG CLEANUP 2026-05-16: This printed on EVERY OS message —
         * URC indications, RIL responses, timer ticks, heartbeats — tens
         * of thousands per minute. Each handler below has its own
         * meaningful log already. OLD CODE:
         *   LOGData(TAG_MAIN, "Message: %d, param1: %d,  param2: %d\r\n",
         *           msg.message, msg.param1, msg.param2);
         */
        switch(msg.message)
        {
        case MSG_ID_RIL_READY:
            LOGData(TAG_MAIN,"<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize();
            ResetSMSContext();
#ifdef SYSTEM_MINIMAL_FOTA_FORMATTER_BUILD
            LOGData(TAG_MAIN,"MINIMAL BUILD: Formatting UFS partition...");
            Ql_FS_Format(Ql_FS_UFS);
            LOGData(TAG_MAIN,"UFS partition formatted successfully!");
#endif
            system_init();
#if SYSTEM_RECOVERY_ENABLE
            SystemRecovery_Init();
#endif
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
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_VOLTAGE_RECOVERY_ENABLE
                SystemRecovery_OnVoltageInd(msg.param2);
#endif
                break;
            default:
                LOGData(TAG_MAIN,"<-- Other URC: type=%d\r\n", msg.param1);
                break;
            }
            break;
        case MSG_ID_RESET_MODULE_REQ:
            LOGData(TAG_MAIN, "<-- Reset request received: attempts=%d, ret=%d -->\r\n", msg.param1, msg.param2);
#if SYSTEM_RECOVERY_ENABLE
            SystemRecovery_RequestReset("FOTA FTP reset request");
#else
            LOGData(TAG_MAIN, "!!! Ql_Reset(0) IMMINENT - reason: FOTA FTP reset request !!!");
            Ql_Reset(0);
            ThreadSleep(2000);
#endif
            break;
        default:
            break;
        }
    }
}

#endif // __CUSTOMER_CODE__
