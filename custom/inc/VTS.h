#ifndef _VTS_H
#define _VTS_H

#include "LOG.h"
#include "custom_feature_def.h"
#include "ql_adc.h"
#include "ql_error.h"
#include "ql_gprs.h"
#include "ql_socket.h"
#include "ql_stdlib.h"
#include "ql_system.h"
#include "ql_time.h"
#include "ql_timer.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ril.h"
#include "ril_network.h"
#include "ril_ntp.h"
#include "ril_sim.h"
#include "ril_system.h"
#include "ril_telephony.h"
#include "ril_util.h"

#include <stdint.h>
#include <string.h>
// #include "Geofence.h"

#ifdef PROTO_CDAC
#define IS_PROTO_CDAC() (1)
#else
#define IS_PROTO_CDAC() (0)
#endif

#ifdef PROTO_NIC1
#define IS_PROTO_NIC() (1)
#else
#define IS_PROTO_NIC() (0)
#endif

#ifdef PROTO_ODISA1
#define IS_PROTO_ODISHA() (1)
#else
#define IS_PROTO_ODISHA() (0)
#endif

#ifdef PROTO_MAHARASHTRA1
#define IS_PROTO_MH() (1)
#else
#define IS_PROTO_MH() (0)
#endif

#ifdef PROTO_OG
#define IS_PROTO_OG() (1)
#else
#define IS_PROTO_OG() (0)
#endif

#if !defined(PROTO_MAHARASHTRA1) && !defined(PROTO_NIC1) &&                    \
    !defined(PROTO_CDAC) && !defined(PROTO_ODISA1) && !defined(PROTO_OG)
#define PROTO_CDAC
#endif

// Disable debug printing for PROTO_OG to reduce binary size
// #ifndef PROTO_OG
#define ENABLE_RS232_PRINT
#ifdef ENABLE_RS232_PRINT
// #define ENABLE_RS232_FAST
#endif
// #endif
//

#ifndef PROTO_CDAC
// #define HISTORY_DISABLED
#endif
#define PRF_AUTOSWITCH
#define HISTORY_INTERNAL
//#define AUTO_SLEEP_ENABLE // Auto sleep after 2 min ignition off
extern char FirmVer[];

#ifdef AUTO_SLEEP_ENABLE
#warning "AUTO SLEEP ENABLED, disbale for production"
#endif
 
// #define AUTO_PROFILESWITCH_DISABLE
#ifdef AUTO_PROFILESWITCH_DISABLE
#warning                                                                       \
    "AUTO PROFILE SWITCH is disabled, device will be always in airtel profile, switch operator manually if needed"
#endif

#define FIRMWAREVERSION "01.05.09"
#define DevModel "02"
#define SDKFirm "01/V1.0"
#define PROTOVER "AIS140"

#define DEFAULT_VENDOR "APMG"

#define LOW_BAT_THRS_VOLT 3.5

#define LOW_BAT_THRS_PER 85

// #define SIMMAKE_APM
 #define SIMMAKE_IDEMIA_3P
// #define SIMMAKE_SENS
// #define SIMMAKE_GND
// #define SIMMAKE_TACHNOJACKS
// #define SIMMAKE_COLORPLAST

#ifdef SIMMAKE_APM
#define SIM_MAKE_STR "APM2P"
#define SIM_PROFILE_AIRTEL 1
#define SIM_PROFILE_BSNL 2
#elif defined(SIMMAKE_IDEMIA_3P)
#define SIM_MAKE_STR "ID3P"
#define SIM_PROFILE_AIRTEL 3
#define SIM_PROFILE_BSNL 2
#define SIM_PROFILE_VI 1
#elif defined(SIMMAKE_SENS)
#define SIM_MAKE_STR "SEN2P"
#define SIM_PROFILE_AIRTEL 1
#define SIM_PROFILE_BSNL 2
#elif defined(SIMMAKE_GND)
#define SIM_MAKE_STR "GND3P"
#define SIM_PROFILE_AIRTEL 2
#define SIM_PROFILE_BSNL 1
#define SIM_PROFILE_VI 3
#elif defined(SIMMAKE_TACHNOJACKS)
#define SIM_MAKE_STR "TAC3P"
#define SIM_PROFILE_AIRTEL 2
#define SIM_PROFILE_BSNL 1
#define SIM_PROFILE_VI 3
#elif defined(SIMMAKE_COLORPLAST)
#define SIM_MAKE_STR "COL3P"
#define SIM_PROFILE_AIRTEL 1
#define SIM_PROFILE_BSNL 2
#define SIM_PROFILE_VI 3
#else
#error "No SIM MAKE defined"
#endif

#define MAX_BATT 4.2f
#define MIN_BATT 3.5f

#define EXTENDED_IPS

#if defined(PROTO_MAHARASHTRA1)
#define PROTO_TAG "MH1"
#define PRF_AUTOSWITCH
#define DEFAULT_IP1 "data.vahanshakti.in"
#define DEFAULT_PORT1 "4030"
#define DEFAULT_IP2 "data.vahanshakti.in"
#define DEFAULT_PORT2 "4040"
#define DEFAULT_IP3 "devices.thelocate.in"
#define DEFAULT_PORT3 "4030"
#define DEFAULT_IP4 "13.234.160.106"
#define DEFAULT_PORT4 "8224"

#define DEFAULT_INV_DATA 300
#define DEFAULT_INV_IGN 10
#define DEFAULT_INV_HEALTH 250
#define DEFAULT_INV_SOS 5
#define DEFAULT_INV_STB 60
#define DEFAULT_INV_STM 120

#define DEFAULT_MOB0 "7017034104"
#define DEFAULT_MOB1 "9655543732"

#define DEFAULT_VEHREG "UNKNOWN"

#define DEFAULT_SPEED 70.0
#define DEFAULT_HB 500
#define DEFAULT_HA 1000
#define DEFAULT_OVERSPEED 70.0
#define DEFAULT_RT 45
#define DEFAULT_TL 42

#elif defined(PROTO_CDAC)
#define PROTO_TAG "CD1"
#define PRF_AUTOSWITCH
#ifndef HTTP_QUEUE
#define HTTP_QUEUE
#endif
#define KEEP_ALIVE
#define CONNTECTION_PRETIME 2

#define DEFAULT_IP1 "http://surakshamitr.org/CDC"
#define DEFAULT_PORT1 "443"
// NO Support for IP2!
// #define TEST_SERVER
#ifdef TEST_SERVER
#define DEFAULT_IP3 "NA"
#define DEFAULT_PORT3 "0"
#else
#define DEFAULT_IP3 "http://78.46.190.117/vlt"
#define DEFAULT_PORT3 "50024"
#endif

#define DEFAULT_INV_HALT 60
#define DEFAULT_INV_MOTION 10
#define DEFAULT_INV_HEALTH 60 * 60
#define DEFAULT_INV_CRIT 5
#define DEFAULT_INV_SLEEP 60 * 5
#define DEFAULT_INV_STM 1800
#define DEFAULT_INV_FULL ((uint16_t)(60 * 120))

#define DEFAULT_HALT_TIME 760
#define DEFAULT_SLEEP_TIME 8800

#define DEFAULT_MOB0 "8489607555"
#define DEFAULT_MOB1 "9600696008"

#define DEFAULT_VEHREG "UNKNOWN"

#define DEFAULT_SPEED 70.0
#define DEFAULT_HB 500
#define DEFAULT_HA 1000
#define DEFAULT_OVERSPEED 70.0
#define DEFAULT_RT 45
#define DEFAULT_TL 45

#elif defined(PROTO_NIC1)

// #define NIC_BIHAR
// #define NIC_PONDI
// #define NIC_UTTRA
// #define NIC_GOA
// #define NIC_MDP
// #define NIC_ISLAND
// #define NIC_TAMIL
// #define NIC_TRIPURA
// #define NIC_VAHAN
// #define NIC_HIMACHEL
#define NIC_RAJASTHAN

#define NO_PARAM
#define DBM_IN_CSQ

#if defined(NIC_PONDI)
#define PRF_AUTOSWITCH
#define PROTO_TAG "PD1"
#define DEFAULT_IP1 "164.100.64.227"
#define DEFAULT_PORT1 "9201"
#define DEFAULT_IP2 "164.100.64.227"
#define DEFAULT_PORT2 "9201"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"

#elif defined(NIC_UTTRA)
#define PRF_AUTOSWITCH
#define PROTO_TAG "UT1"
#define DEFAULT_IP1 "vlt.uk.gov.in"
#define DEFAULT_PORT1 "9999"
#define DEFAULT_IP2 "vlt.uk.gov.in"
#define DEFAULT_PORT2 "9999"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"

#elif defined(NIC_GOA)
#define PRF_AUTOSWITCH
#define PROTO_TAG "GA1"
#define DEFAULT_IP1 "gavltspvt.goa.gov.in"
#define DEFAULT_PORT1 "9031"
#define DEFAULT_IP2 "gavltsemg.goa.gov.in"
#define DEFAULT_PORT2 "9032"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"

#elif defined(NIC_MDP)
#define PRF_AUTOSWITCH
#define PROTO_TAG "MP1"
#define DEFAULT_IP1 "mpdevice.vltsecurity.com"
#define DEFAULT_PORT1 "61487"
#define DEFAULT_IP2 "mpdevice.vltsecurity.com"
#define DEFAULT_PORT2 "61488"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"

#elif defined(NIC_TRIPURA)
#define PRF_AUTOSWITCH
#define PROTO_TAG "TR1"
#define DEFAULT_IP1 "lsttr.vahanmitra.org"
#define DEFAULT_PORT1 "5200"
#define DEFAULT_IP2 "lsttr.vahanmitra.org"
#define DEFAULT_PORT2 "9049"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"

#elif defined(NIC_VAHAN)
#define PRF_AUTOSWITCH
#define PROTO_TAG "VB1"
#define DEFAULT_IP1 "commonlayer.parivahan.gov.in"
#define DEFAULT_PORT1 "9031"
#define DEFAULT_IP2 "commonlayer.parivahan.gov.in"
#define DEFAULT_PORT2 "9032"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"

#elif defined(NIC_HIMACHEL)
#define PRF_AUTOSWITCH
#define PROTO_TAG "HP1"
#define DEFAULT_IP1 "vltdgw.hp.gov.in"
#define DEFAULT_PORT1 "443"
#define DEFAULT_IP2 "NA"
#define DEFAULT_PORT2 "0"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"

#elif defined(NIC_RAJASTHAN)
#define PRF_AUTOSWITCH
#define PROTO_TAG "RJ1"
#define DEFAULT_IP1 "vltspvt.rajasthan.gov.in"
#define DEFAULT_PORT1 "9031"
#define DEFAULT_IP2 "vltsemg.rajasthan.gov.in"
#define DEFAULT_PORT2 "9032"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"

#elif defined(NIC_TAMIL)
#define PRF_AUTOSWITCH
#define PROTO_TAG "TN1"
#define DEFAULT_IP1 "stavltsgw.tn.gov.in"
#define DEFAULT_PORT1 "8080"
#define DEFAULT_IP2 "NA"
#define DEFAULT_PORT2 "0"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"


#define SOS_FULL_EA

#else
#define PROTO_TAG "NI1"
#define DEFAULT_IP1 "vltspvt.delhi.gov.in"
#define DEFAULT_PORT1 "9031"
#define DEFAULT_IP2 "vltsemg.delhi.gov.in"
#define DEFAULT_PORT2 "9032"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"

#endif

#define DEFAULT_INV_DATA 100
#define DEFAULT_INV_IGN 10
#define DEFAULT_INV_HEALTH 250
#define DEFAULT_INV_SOS 5
#define DEFAULT_INV_STB 60
// #define 	DEFAULT_INV_STM		1800 // 30 mins
#define DEFAULT_INV_STM 120 // 2 mins

#define DEFAULT_MOB0 "9600696008"
#define DEFAULT_MOB1 "8489607555"

#define DEFAULT_VEHREG "UNKNOWN"

#define DEFAULT_SPEED 70.0
#define DEFAULT_HB 500
#define DEFAULT_HA 1000
#define DEFAULT_OVERSPEED 70.0
#define DEFAULT_RT 45
#define DEFAULT_TL 42
#elif defined(PROTO_ODISA1)

#define ODISA_LD

#ifdef ODISA_LD
#define PROTO_TAG "LD1"
#define PRF_AUTOSWITCH
#define DEFAULT_IP1 "pvt.vltdladakh.in"
#define DEFAULT_PORT1 "60002"
#define DEFAULT_IP2 "emr.vltdladakh.in"
#define DEFAULT_PORT2 "61002"
#define DEFAULT_IP3 "103.143.84.2"
#define DEFAULT_PORT3 "8801"
#define DEFAULT_IP4 "13.234.160.106"
#define DEFAULT_PORT4 "8224"

#define DEFAULT_INV_DATA 300
#define DEFAULT_INV_IGN 10
#define DEFAULT_INV_HEALTH 250
#define DEFAULT_INV_SOS 5
#define DEFAULT_INV_STB 60
#define DEFAULT_INV_STM 600 // 10 mins

#else
#define PROTO_TAG "OD1"
// #define HISTORY_DISABLED
#define NO_TAMPER
#define DEFAULT_IP1 "pvtdevices.odishatransport.gov.in"
#define DEFAULT_PORT1 "8205"
#define DEFAULT_IP2 "emrdevices.odishatransport.gov.in"
#define DEFAULT_PORT2 "9202"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"
#define DEFAULT_IP4 "NA"
#define DEFAULT_PORT4 "0"

#define DEFAULT_INV_DATA 300
#define DEFAULT_INV_IGN 60
#define DEFAULT_INV_HEALTH 250
#define DEFAULT_INV_SOS 60
#define DEFAULT_INV_STB 300
#define DEFAULT_INV_STM 3600 // 1 hour
#endif

#define DEFAULT_MOB0 "7017034104"
#define DEFAULT_MOB1 "9655543732"

#define DEFAULT_VEHREG "UNKNOWN"

#define DEFAULT_SPEED 70.0
#define DEFAULT_HB 500
#define DEFAULT_HA 1000
#define DEFAULT_OVERSPEED 70.0
#define DEFAULT_RT 45
#define DEFAULT_TL 42

#elif defined(PROTO_OG)

#define PROTO_TAG "OG1"
#define PRF_AUTOSWITCH
#define DEFAULT_IP1 "78.46.190.117"
#define DEFAULT_PORT1 "50011"
#define DEFAULT_IP2 "78.46.190.117"
#define DEFAULT_PORT2 "50011"
#define DEFAULT_IP3 "13.234.160.106"
#define DEFAULT_PORT3 "8224"

#define DEFAULT_INV_DATA 30
#define DEFAULT_INV_IGN 10
#define DEFAULT_INV_HEALTH 250
#define DEFAULT_INV_SOS 5
#define DEFAULT_INV_STB 60
#define DEFAULT_INV_STM 120

#define DEFAULT_MOB0 "8489607555"
#define DEFAULT_MOB1 "9600696008"

#define DEFAULT_VEHREG "UNKNOWN"

#define DEFAULT_SPEED 70.0
#define DEFAULT_HB 500
#define DEFAULT_HA 1000
#define DEFAULT_OVERSPEED 70.0
#define DEFAULT_RT 45
#define DEFAULT_TL 42

#else

#error NO VALID PROTOCOL SELECTED !!!
#endif

#ifndef DEFAULT_IP4
#define DEFAULT_IP4 "NA"
#define DEFAULT_PORT4 "0"
#endif

// #define VIRTUAL_IMEI
#define VIMEI "861850061675811"

// #define VIRTUAL_IMSI
#define VIMSI "404844263390879"

// #define VIRTUAL_SIMCCID
#define VCID "89917350730001767711"

#ifdef VIRTUAL_IMEI
#ifdef VIMEI
#warning VIRTUAL IMEI Is Enabled, Disable it for PRODUCTION USE!!!!!
#else
#error VIMEI not Defined for Virtual Imei USE!!!!
#endif
#endif

#ifdef VIRTUAL_IMSI
#ifdef VIMSI
#warning VIRTUAL IMSI Is Enabled, Disable it for PRODUCTION USE!!!!!
#else
#error VIMSI not Defined for Virtual IMSI USE!!!!
#endif
#endif

#ifdef VIRTUAL_SIMCCID
#ifdef VCID
#warning VIRTUAL SIM CCID Is Enabled, Disable it for PRODUCTION USE!!!!!
#else
#error VCID not Defined for Virtual SIM CCID USE!!!!
#endif
#endif

typedef enum {
  UART2_MODE_LOG,
  UART2_MODE_RFID,
  UART2_MODE_MAX
} Uart2Modetypedef;
typedef enum { IP2_MODE_NORMAL, IP2_MODE_DHT11, IP2_MODE_MAX } IP2ModeTypedef;

typedef enum { TAISYS = 0, SENSORISE, GnD } SIMMakeTypedef;

typedef struct {
  Uart2Modetypedef Uart2Mode;
  IP2ModeTypedef IP2Mode;
  uint16_t IGNInterval;
  uint16_t OFFInterval;
} SensorSettingtypedef;

typedef enum { NORMAL, EMERGENCY, CRITICAL, ALERT, BATCH } PACKETSTATE;

typedef enum { HALT, MOTION, SLEEP } VEHICLEMODE;

typedef struct {
  PACKETSTATE PacketState;
  VEHICLEMODE VehicleMode;
} VehicleTypeDef;

#if defined(PROTO_CDAC)
extern VehicleTypeDef VehicleState;
extern volatile char VehicleMovingMode;
#endif

// typedef enum {LOG1_MODE_NONE,LOG1_MODE_RFID} LOG1UartMode;

typedef struct {
  int ID;
  uint8_t InOut;
  uint8_t AlertInOut;
  double Latitude[10];
  double Longitude[10];
} GEOPOINTTypeDef;

typedef struct {
  char Mob0[14];
  char Mob1[14];
  char Mob2[14];
  char Mob3[14];
  char Mob4[14];

} MobileNumTypeDefStruct;

#define MAX_IP_CONFIG 5
typedef struct {
  uint8_t IPConfig[MAX_IP_CONFIG]; // whether the ip is enabled or not
  char Url1[55];
  char Url2[55];
  char IP1[50];
  char IP2[50];
  char IP3[50];
  char IP4[50];
  char IP5[50];
  char Port1[7];
  char Port2[7];
  char Port3[7];
  char Port4[7];
  char Port5[7];

} ServerDataTypedef;

#ifndef PROTO_CDAC
typedef struct {
  uint16_t CurrentInterval;
  uint16_t DataInterval;
  uint16_t StandbyInterval;
  uint16_t SOSInterval;
  uint16_t IgnitionInterval;
  uint16_t IsDataAvail;
  uint16_t HealthInterval;
  uint16_t SleepTime;
  uint16_t HaltTime;
  uint16_t SOSTimeOut;
} IntervalTypeDef;
#else
typedef struct {
  uint16_t CurrentInterval;
  uint16_t HaltInterval;
  uint16_t MotionInterval;
  uint16_t SleepInterval;
  uint16_t EnergencyInterval;
  uint16_t IsDataAvail;
  uint16_t HealthInterval;
  uint16_t FullDataPacketInterval;
  uint16_t SOSTimeOut;
  uint16_t SleepTime;
  uint16_t HaltTime;

} IntervalTypeDef;
#define DataInterval                                                           \
  MotionInterval /* alias so shared code compiles under PROTO_CDAC */
#define SOSInterval EnergencyInterval
#define StandbyInterval SleepInterval
#define IgnitionInterval HaltInterval
#endif

typedef struct {
  uint16_t HarshBreak;
  uint16_t HarshAcc;
  uint16_t RashTurn;
  uint16_t TiltAngle;
  char VehicleRegNo[20];
  double OverSpeed;
  double DefaultSpeed;
} VehicleTypedef;

typedef struct {
  char VendorID[30];
  double BattThrs;
  ServerDataTypedef ServerData;
  IntervalTypeDef IntervalData;
  SensorSettingtypedef SensorSetting;
  VehicleTypedef VehicleData;
  GEOPOINTTypeDef GeoLatLng[10];
  MobileNumTypeDefStruct PhoneNumber;
  uint16_t DefID;
  uint8_t DefProfile;
  uint8_t AutoAPN;
  char mAPN[20];
  uint8_t IsCustomSPN;
  char mSPN[20];
  SIMMakeTypedef SIMMake;
  uint8_t DisableSOS;
  uint8_t SOSSmsEnabled;
  uint8_t DisableHistory;
  uint64_t SOSSmsConfigSignature;
  uint8_t EnableHTTPS;
  uint8_t DisableGPSFaultReset;
  uint8_t DisableSOSTamper;
} VTSTypedef;

#define DEFAULT_DISABLE_SOS_TAMPER 1

#ifdef PROTO_MAHARASHTRA1
#define SOS_SMS_FEATURE_ENABLED 1
// #define SOS_WIRECUT_SMS_ENABLED 1
#else
#define SOS_SMS_FEATURE_ENABLED 0
// #define SOS_WIRECUT_SMS_ENABLED 0
#endif
#define SOS_SMS_CONFIG_SIGNATURE 0x534F53534D533032ULL

extern VTSTypedef VTSData;

typedef struct {
  uint16_t DefVal;
  uint64_t OdoCount;
  uint8_t RegDeniedCount;
  uint8_t CurrentProfile;
  uint8_t ProfileFailCount[5]; // Tracks consecutive failures per profile (index
                               // 1-4)
  uint8_t IsPrevMain;
} VTSStateTypedef;

extern VTSStateTypedef VTSState;

/* =======================================================================
 * DIAGNOSTIC LOGGING SYSTEM — Structures
 * Added : 2026-05-13
 * =======================================================================
 * These structures form the persistent diagnostic layer.
 * They are stored in a SEPARATE flash file ("Diag.bin") and do NOT
 * share storage with VTSStateTypedef or VTSTypedef. This means adding
 * them here does NOT change the size of State.bin or UFSConfig.bin —
 * there is zero risk of triggering a config mismatch on existing devices.
 *
 * The ring buffer (DiagCounters.Ring[]) stores the last DIAG_RING_SIZE
 * events in circular fashion. When full it overwrites the oldest entry.
 * This gives a persistent "black box" of the last N events that survives
 * across reboots and can be read via RS232 after a field failure.
 * ======================================================================= */

/* -----------------------------------------------------------------------
 * DiagEventType — event type codes stored in the ring buffer
 *
 * HOW TO READ:
 *   Each entry in Diag.bin Ring[] has an EventType byte matching one of
 *   these codes. When you dump logs via RS232 you will see lines like:
 *     DIAG: EVT[3] uptime=1820 type=1(PRF_SWITCH) p1=1 p2=3
 * detail=[auto-timeout] p1 and p2 meaning depends on event type — see
 * PushDiagEvent() comments in File.c for the exact meaning of each parameter.
 * ----------------------------------------------------------------------- */
typedef enum {
  DIAG_EVT_BOOT = 0,         /* device powered on / restarted              */
  DIAG_EVT_PRF_SWITCH = 1,   /* profile switched: p1=from, p2=to           */
  DIAG_EVT_PRF_FAIL = 2,     /* profile switch failed: p1=profile, p2=count */
  DIAG_EVT_GSM_HANG = 3,     /* GSM modem hang detected: p1=CSQ, p2=fail   */
  DIAG_EVT_GSM_RESET = 4,    /* AT+CFUN=1,1 reset sent: p1=attempt count   */
  DIAG_EVT_WDT_RESET = 5,    /* boot reason was watchdog reset             */
  DIAG_EVT_GPRS_FAIL = 6,    /* GPRS activation failed: p1=profile, p2=cnt */
  DIAG_EVT_REG_DENIED = 7,   /* registration denied: p1=profile, p2=CSQ    */
  DIAG_EVT_STATE_CHG = 8,    /* GSM state changed: p1=old, p2=new          */
  DIAG_EVT_FLASH_ERR = 9,    /* flash read/write error detected            */
  DIAG_EVT_PRF_TIMEOUT = 10, /* 12-min PRF_AUTOSWITCH fired: p1=profile    */
  DIAG_EVT_SYS_RESET = 11,   /* SystemRecovery_RequestReset called         */
  DIAG_EVT_MAX = 12
} DiagEventType;

/* -----------------------------------------------------------------------
 * DiagEventEntry — one entry in the ring buffer
 * Size: 4 + 1 + 1 + 1 + 1 + 20 = 28 bytes per entry
 * ----------------------------------------------------------------------- */
#define DIAG_RING_SIZE 50 /* last 50 events kept; 50 * 28 = 1400 bytes */

typedef struct {
  uint32_t UptimeSec; /* seconds since last boot when event occurred */
  uint8_t EventType;  /* DiagEventType code                          */
  uint8_t Param1;     /* event-specific parameter 1 (see enum above) */
  uint8_t Param2;     /* event-specific parameter 2                  */
  uint8_t Reserved;   /* padding / future use                        */
  char Detail[20];    /* human-readable reason string, null-terminated */
} DiagEventEntry;

/* -----------------------------------------------------------------------
 * DiagCountersTypedef — top-level diagnostic structure stored in Diag.bin
 *
 * HOW TO READ AFTER A FIELD FAILURE:
 *   1. Connect RS232 to device
 *   2. Reboot device
 *   3. Search logs for "DIAG:" prefix
 *   4. BootCount tells you how many total reboots
 *   5. WatchdogResetCount tells you how many were WDT-triggered
 *   6. GsmHangCount tells you how many hang events occurred total
 *   7. Ring[] shows last DIAG_RING_SIZE events in time order
 * ----------------------------------------------------------------------- */
#define DIAG_MAGIC                                                             \
  0xD1A60001UL /* unique magic to detect Diag.bin corruption                   \
                */

typedef struct {
  uint32_t Magic;              /* must equal DIAG_MAGIC or defaults loaded  */
  uint32_t BootCount;          /* total device boots (any reason)           */
  uint32_t WatchdogResetCount; /* boots caused by watchdog timeout          */
  uint32_t ProfileSwitchCount; /* total successful profile switches         */
  uint32_t GsmResetCount;      /* total AT+CFUN=1,1 soft resets sent        */
  uint32_t RegDeniedTotal;     /* total registration denied events          */
  uint32_t Csq99Count;         /* total times CSQ read as 99 (no signal)    */
  uint32_t GprsFailCount;      /* total GPRS activation timeout events      */
  uint32_t GsmHangCount;       /* total GSM hang detections                 */
  uint8_t LastBootReason;      /* Ql_GetPowerOnReason() value at last boot  */
  uint8_t LastProfile;         /* active profile at last boot               */
  uint8_t RingHead;            /* next write index into Ring[] (0-based)    */
  uint8_t RingCount; /* how many entries are valid (0-DIAG_RING_SIZE) */
  DiagEventEntry Ring[DIAG_RING_SIZE]; /* circular event log */
} DiagCountersTypedef;

extern DiagCountersTypedef DiagCounters;
/* ======================================================================= */

typedef struct {
  uint8_t IsNormalPacket;
  uint8_t IsHealthPacket;
  uint8_t IsFullPacket;
  uint8_t IsCriticalPacket;
  uint8_t IsGSMNeighbour;
#ifndef PROTO_CDAC
  uint8_t IsHistoryPacket;
#endif
  uint8_t IsSensPacket;
} PacketReadyTypedef;

extern volatile PacketReadyTypedef IsPacketReady;

typedef struct {
  volatile uint16_t NormalTick;
  volatile uint16_t HealthTick;
  volatile uint16_t FullTick;
  volatile uint16_t CriticalTick;
  volatile uint16_t NeighbourTick;
#ifndef PROTO_CDAC
  volatile uint16_t HistoryTick;
#endif
  volatile uint16_t SensTick;
  volatile uint16_t ServerHangTimeout;
  volatile uint16_t ProfileChangeCount;
  volatile uint16_t NoSignalSec;
} TickTypeDef;

// ZigTestMode: Manufacturing test mode flag
// When enabled (1), prevents automatic device resets
// Used for ZIG testing and manufacturing purposes
extern uint8_t ZigTestMode;

extern uint8_t IsOverSpeed, PrevTamp;
extern volatile TickTypeDef IntervalTick;

const char *GetProfileName(uint8_t profile);

#endif // _VTS_H
