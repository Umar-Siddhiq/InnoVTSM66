#ifndef __LOG_DEBUG_H__
#define __LOG_DEBUG_H__

#include "custom_feature_def.h"
#include "ql_stdlib.h"
#include "ql_uart.h"
#include "ql_trace.h"

// Debug UART & buffer
#define DEBUG_UART UART_PORT2
#ifndef VTS_DEBUG_LOG_ENABLE
#define VTS_DEBUG_LOG_ENABLE 0
#endif

#if VTS_DEBUG_LOG_ENABLE
#define DBG_BUF_LEN   1024  // Increased from 512 to 1024 for larger log messages
extern char DBG_BUFFER[];
#endif

/*Debug TAG Definations */
#define TAG_MAIN				"MAIN"
#define TAG_GPRS				"GPRS"
#define TAG_GPS					"GPS"
#define TAG_BLE					"BLE"
#define TAG_TCP					"TCP"
#define TAG_MQTT				"MQTT"
#define TAG_HARDWARE			"HARDWARE"
#define TAG_SERVER				"SERVER"
#define TAG_OTA					"OTA"
#define TAG_SMS					"SMS"
#define TAG_HYPER				"HYPER"
#define TAG_SYSTIC				"SYSTIC"
#define TAG_FILE                "FILE"
#define TAG_MCU                 "MCU"
#define TAG_GEO                 "GEOFENCE"
#define TAG_ALERT               "ALERT"
#define TAG_BATCH               "BATCH"
#define TAG_BACKUP              "BACKUP"
#define TAG_SOS                 "EMG"
#define TAG_FTP                 "FTP"
#define TAG_EPO                 "EPO"
#define TAG_LED                 "LED"


#define QL_ECHO_LEN         512
#define LOG_SEND_SIZE_MAX   128

void print_long_string(const char* long_string);

#if VTS_DEBUG_LOG_ENABLE
#define LOGData(TAG, FORMAT, ...)                                 \
    do {                                                          \
        Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN);                    \
        Ql_snprintf(DBG_BUFFER, DBG_BUF_LEN - 1, "%s: " FORMAT "\n", TAG, ##__VA_ARGS__); \
        Ql_Debug_Trace(DBG_BUFFER);                               \
    } while (0)
#else
#define LOGData(TAG, FORMAT, ...) do { } while (0)
#endif

#endif // __LOG_DEBUG_H__
