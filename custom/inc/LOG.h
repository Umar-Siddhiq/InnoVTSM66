#ifndef __LOG_DEBUG_H__
#define __LOG_DEBUG_H__

#include "custom_feature_def.h"
#include "ql_stdlib.h"
#include "ql_uart.h"
#include "ql_trace.h"
#include <stdio.h>

// Debug UART & buffer
#define DEBUG_UART UART_PORT2
#ifndef VTS_DEBUG_LOG_ENABLE
#define VTS_DEBUG_LOG_ENABLE 0
#endif

#if VTS_DEBUG_LOG_ENABLE
#define DBG_BUF_LEN   512  /* M66 Ql_Debug_Trace hard limit; see ql_trace.h. */
extern char DBG_BUFFER[];
void LogData_Init(void);
void LogData_Lock(void);
void LogData_Unlock(void);
#endif

/* -----------------------------------------------------------------------
 * DIAGNOSTIC LOGGING SYSTEM — TAG Definitions
 * Added : 2026-05-13
 * -----------------------------------------------------------------------
 * EXISTING TAGS (unchanged — listed for reference)
 * Every LOGData() call requires one of these as its first argument.
 * The tag appears as a prefix in the log line, e.g.:  GPRS: STATE -> GPRS INIT
 * ----------------------------------------------------------------------- */
#define TAG_MAIN                "MAIN"        /* main.c boot / message loop   */
#define TAG_GPRS                "GPRS"        /* GSM/GPRS state machine       */
#define TAG_GPS                 "GPS"         /* GPS parser / fix status      */
#define TAG_BLE                 "BLE"         /* Bluetooth LE                 */
#define TAG_TCP                 "TCP"         /* TCP socket layer             */
#define TAG_MQTT                "MQTT"        /* MQTT protocol                */
#define TAG_HARDWARE            "HARDWARE"    /* GPIO, ADC, battery, mains    */
#define TAG_SERVER              "SERVER"      /* server packet encode/send    */
#define TAG_OTA                 "OTA"         /* over-the-air firmware update */
#define TAG_SMS                 "SMS"         /* SMS command handler          */
#define TAG_HYPER               "HYPER"       /* OS thread scheduler          */
#define TAG_SYSTIC              "SYSTIC"      /* 1-second tick / intervals    */
#define TAG_FILE                "FILE"        /* flash read/write (UFS)       */
#define TAG_MCU                 "MCU"         /* external MCU UART bridge     */
#define TAG_GEO                 "GEOFENCE"    /* geofence polygon checks      */
#define TAG_ALERT               "ALERT"       /* alert packet generation      */
#define TAG_BATCH               "BATCH"       /* batch/history packet buffer  */
#define TAG_BACKUP              "BACKUP"      /* backup server logic          */
#define TAG_SOS                 "EMG"         /* SOS / emergency alerts       */
#define TAG_FTP                 "FTP"         /* FTP FOTA download            */
#define TAG_EPO                 "EPO"         /* GPS EPO orbit data           */
#define TAG_LED                 "LED"         /* LED state manager            */
#define TAG_RECOVERY            "RECOVERY"    /* watchdog / system recovery   */

/* -----------------------------------------------------------------------
 * NEW DIAGNOSTIC TAGS — Added for full diagnostic logging system
 * (Fix: Diagnostic Logging System — 2026-05-13)
 *
 * These tags were NOT present in the original firmware.
 * They provide dedicated log channels for each diagnostic area so that
 * field logs can be filtered by category without parsing unrelated lines.
 * ----------------------------------------------------------------------- */
#define TAG_PROFILE             "PROFILE"     /* profile switch: before/after, reason, fail count  */
#define TAG_WDT                 "WDT"         /* watchdog: start / feed-alive / stop / test-skip   */
#define TAG_DIAG                "DIAG"        /* event counters: boot count, gsm resets, gprs fails */
#define TAG_STATE               "STATE"       /* GSM state machine transitions with reason string   */
#define TAG_BOOT                "BOOT"        /* boot reason, firmware version, IMEI at startup     */


#define QL_ECHO_LEN         512
#define LOG_SEND_SIZE_MAX   128

void print_long_string(const char* long_string);

#if VTS_DEBUG_LOG_ENABLE
#define LOGData(TAG, FORMAT, ...)                                  \
    do {                                                           \
        LogData_Lock();                                            \
        Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN);                     \
        snprintf(DBG_BUFFER, DBG_BUF_LEN - 1, "%s: " FORMAT "\n", TAG, ##__VA_ARGS__); \
        Ql_Debug_Trace("%s", DBG_BUFFER);                         \
        LogData_Unlock();                                          \
    } while (0)
#else
#define LOGData(TAG, FORMAT, ...) do { } while (0)
#endif

#ifndef VTS_VERBOSE_LOG_ENABLE
#define VTS_VERBOSE_LOG_ENABLE 1
#endif

#if (VTS_DEBUG_LOG_ENABLE && VTS_VERBOSE_LOG_ENABLE)
#define LOGVerbose(TAG, FORMAT, ...) LOGData(TAG, FORMAT, ##__VA_ARGS__)
#else
#define LOGVerbose(TAG, FORMAT, ...) do { } while (0)
#endif

#endif // __LOG_DEBUG_H__
