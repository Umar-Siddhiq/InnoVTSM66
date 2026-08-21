#ifndef __CUSTOM_FEATURE_DEF_H__
#define __CUSTOM_FEATURE_DEF_H__

#include "custom_proto_cfg.h"

/************************************************************************
 * RIL Function on/off
 ************************************************************************/
#define __OCPU_RIL_SUPPORT__
#define __OCPU_RIL_SMS_SUPPORT__
#define __OCPU_RIL_CALL_SUPPORT__
//#define __OCPU_RIL_QLBS_SUPPORT__
// Optional RIL features (enable only if you use them)
// #define __OCPU_RIL_AUDIO_SUPPORT__
// #define __OCPU_RIL_DTMF_SUPPORT__
// #define __OCPU_RIL_QCELLLOC_SUPPORT__
// #define __OCPU_RIL_ALARM_RING_SUPPORT__
// #define __OCPU_RIL_VOLTAGE_URC_SUPPORT__

/************************************************************************
 * 16MB Flash Storage Target Configuration
 * Enable FLASH_STORAGE_16MB for 16MB Flash hardware builds.
 * When enabled:
 *   - Automatically defines HISTORY_DISABLED (disables UFS history backup files)
 *   - Automatically sets VTS_DEBUG_LOG_ENABLE to 0 (disables UART debug trace logs)
 *   - RS232 communication & packets remain 100% active and untouched
 ************************************************************************/
 #define FLASH_STORAGE_16MB

#ifdef FLASH_STORAGE_16MB
#ifndef HISTORY_DISABLED
#define HISTORY_DISABLED
#endif
#undef VTS_DEBUG_LOG_ENABLE
#define VTS_DEBUG_LOG_ENABLE 0
#endif

/************************************************************************
 * Logging (disable to save flash)
 ************************************************************************/
#ifndef VTS_DEBUG_LOG_ENABLE
#define VTS_DEBUG_LOG_ENABLE 1
#endif

/************************************************************************
 * Bluetooth/BLE (disable to save flash)
 ************************************************************************/
#ifndef VTS_BLE_ENABLE
#define VTS_BLE_ENABLE 1
#endif

#if VTS_BLE_ENABLE
#define __OCPU_RIL_BT_SUPPORT__
#endif

/************************************************************************
 * FOTA Feature Definition
 ************************************************************************/
#define __OCPU_FOTA_APP__
#define __OCPU_FOTA_BY_FTP__
//#define __OCPU_FOTA_BY_HTTP__


/************************************************************************
 * GNSS / EPO Feature Definition
 ************************************************************************/
#define EOP_USE
/************************************************************************
 * Protocol Specific Feature Controls
 ************************************************************************/
#ifdef PROTO_CDAC
#ifndef HTTP_QUEUE
#define HTTP_QUEUE
#endif
#define SOS_NC_CIRCUIT
// ENABLE_BATTERY_MONITOR must be undefined for CDAC trace stability
#undef ENABLE_BATTERY_MONITOR
#endif

/************************************************************************
 * System Recovery & Watchdog
 ************************************************************************/
#ifndef SYSTEM_RECOVERY_ENABLE
#define SYSTEM_RECOVERY_ENABLE 1
#endif
#ifndef SYSTEM_WATCHDOG_ENABLE
#define SYSTEM_WATCHDOG_ENABLE 1
#endif
#ifndef SYSTEM_WATCHDOG_TIMEOUT_MS
#define SYSTEM_WATCHDOG_TIMEOUT_MS 5000
#endif
#ifndef SYSTEM_WATCHDOG_FEED_MS
#define SYSTEM_WATCHDOG_FEED_MS 500
#endif
#ifndef SYSTEM_WATCHDOG_SERVICE_INIT_ENABLE
#define SYSTEM_WATCHDOG_SERVICE_INIT_ENABLE 0
#endif
#ifndef SYSTEM_WATCHDOG_SERVICE_INIT_PIN
/* PINNAME_RXD_AUX is UART_PORT3 RX (GPS) — must not be used as WDT toggle */
#define SYSTEM_WATCHDOG_SERVICE_INIT_PIN PINNAME_END
#endif
#ifndef SYSTEM_WATCHDOG_SERVICE_INIT_MS
#define SYSTEM_WATCHDOG_SERVICE_INIT_MS 600
#endif
#ifndef SYSTEM_WATCHDOG_BOOT_DELAY_MS
#define SYSTEM_WATCHDOG_BOOT_DELAY_MS 0
#endif
#ifndef SYSTEM_RESET_DELAY_MS
#define SYSTEM_RESET_DELAY_MS 100
#endif
#ifndef SYSTEM_WATCHDOG_GPRS_STALL_MS
#define SYSTEM_WATCHDOG_GPRS_STALL_MS 180000
#endif
#ifndef SYSTEM_CONNECTION_WATCHDOG_TIMEOUT_MS
#define SYSTEM_CONNECTION_WATCHDOG_TIMEOUT_MS (2 * 60 * 60 * 1000ULL)
#endif

/************************************************************************
 * Minimal Build Toggle & Overrides
 ************************************************************************/
// #define SYSTEM_MINIMAL_FOTA_FORMATTER_BUILD

#ifdef SYSTEM_MINIMAL_FOTA_FORMATTER_BUILD
#undef VTS_BLE_ENABLE
#define VTS_BLE_ENABLE 0
#undef VTS_DEBUG_LOG_ENABLE
#define VTS_DEBUG_LOG_ENABLE 0
#undef SYSTEM_WATCHDOG_ENABLE
#define SYSTEM_WATCHDOG_ENABLE 0
#undef SYSTEM_RECOVERY_ENABLE
#define SYSTEM_RECOVERY_ENABLE 0
#undef ENABLE_GPS_RESET_RECOVERY
#define ENABLE_GPS_RESET_RECOVERY 0
#undef __OCPU_RIL_BT_SUPPORT__
#endif

#endif  //__CUSTOM_FEATURE_DEF_H__
