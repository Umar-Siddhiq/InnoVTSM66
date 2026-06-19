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
// Enable to get verbose logs during EPO download/injection and binary ACK handling.
#define EPO_VERBOSE_LOG


#endif  //__CUSTOM_FEATURE_DEF_H__
