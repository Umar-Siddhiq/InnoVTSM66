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
// ENABLE_UNIFIED_FIRMWARE must be undefined for CDAC
#undef ENABLE_UNIFIED_FIRMWARE
// ENABLE_BATTERY_MONITOR must be undefined for CDAC trace stability
#undef ENABLE_BATTERY_MONITOR
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
