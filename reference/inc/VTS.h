#ifndef							_VTS_H
#define							_VTS_H

#include "project.h"
#include <string.h>
#include "Geofence.h"
#define ENABLE_RS232_PRINT
#ifdef ENABLE_RS232_PRINT
	//#define ENABLE_RS232_FAST
#endif


//#define HISTORY_DISABLED
#define PRF_AUTOSWITCH
#define HISTORY_INTERNAL
extern char FirmVer[];


#define			FIRMWAREVERSION			"1.5.5"
#define 		DevModel				"02"
#define			SDKFirm					"01/V1.0"
#define 		PROTOVER				"AIS140"

#define 	DEFAULT_VENDOR				"APMG"

#define		LOW_BAT_THRS_VOLT				3.5

#define		LOW_BAT_THRS_PER				85

//#define 	AUTO_PROFILESWITCH_DISABLE
//#define _ONLINE_DBG_

#ifdef _ONLINE_DBG_
#warning "Online Debug Enabled, pls disable for production"
#endif

//#define SIMMAKE_APM
#define SIMMAKE_IDEMIA_3P
//#define SIMMAKE_SENS
//#define SIMMAKE_GND
//#define SIMMAKE_TACHNOJACKS


// BSNL PROTO MODE
#define BSNL_PROTO

#ifdef SIMMAKE_APM
	#define SIM_MAKE_STR	"APM2P"
	#define SIM_PROFILE_AIRTEL	1
	#define SIM_PROFILE_BSNL	2
#elif defined(SIMMAKE_IDEMIA_3P)
	#define SIM_MAKE_STR	"ID3P"
	#define SIM_PROFILE_AIRTEL	3
	#define SIM_PROFILE_BSNL	2
	#define SIM_PROFILE_VI		1
#elif defined(SIMMAKE_SENS)
	#define SIM_MAKE_STR	"SEN2P"
	#define SIM_PROFILE_AIRTEL	1
	#define SIM_PROFILE_BSNL	2
#elif defined(SIMMAKE_GND)
	#define SIM_MAKE_STR	"GND3P"
	#define SIM_PROFILE_AIRTEL	2
	#define SIM_PROFILE_BSNL	1
	#define SIM_PROFILE_VI		3
#elif defined(SIMMAKE_TACHNOJACKS)
	#define SIM_MAKE_STR	"TAC3P"
	#define SIM_PROFILE_AIRTEL	2
	#define SIM_PROFILE_BSNL	1
	#define SIM_PROFILE_VI		3
#else
	#error "No SIM MAKE defined"
#endif


//#define	PROTO_MAHARASHTRA1
#define 	PROTO_NIC1
//#define 	PROTO_CDAC
//#define 	PROTO_ODISA1
//#define 	PROTO_OG

#define MAX_BATT		4.0f
#define MIN_BATT		3.3f

#define EXTENDED_IPS
//#define MCU_HARSH_ALERTS


#if defined(PROTO_MAHARASHTRA1)
	#define PROTO_TAG	"MH1"
	//#define DBM_IN_CSQ
	#define PRF_AUTOSWITCH
	#define 	DEFAULT_IP1  	"data.vahanshakti.in"
	#define		DEFAULT_PORT1	"4030"
	#define 	DEFAULT_IP2  	"data.vahanshakti.in"
	#define		DEFAULT_PORT2	"4040" 	
	#define 	DEFAULT_IP3  	"devices.thelocate.in"
	#define		DEFAULT_PORT3	"4030"
	#define 	DEFAULT_IP4  	"13.234.160.106:8224"


	#define 	DEFAULT_INV_DATA	300
	#define 	DEFAULT_INV_IGN		10
	#define 	DEFAULT_INV_HEALTH	250
	#define 	DEFAULT_INV_SOS		5		
	#define 	DEFAULT_INV_STB		60
	#define 	DEFAULT_INV_STM		120

	#define 	DEFAULT_MOB0		"9600696008"
	#define 	DEFAULT_MOB1		"9655543732"

	#define		DEFAULT_VEHREG		"UNKNOWN"

	#define		DEFAULT_SPEED		70.0
	#define 	DEFAULT_HB			500
	#define 	DEFAULT_HA			1000
	#define		DEFAULT_OVERSPEED	70.0
	#define 	DEFAULT_RT			45
	#define 	DEFAULT_TL			42
	
#elif defined (PROTO_CDAC)

	#define CDAC_HIMACHAL

	#define PROTO_TAG  	"CD1"
	#define PRF_AUTOSWITCH
	#define HTTP_SIMULATE
	#define KEEP_ALIVE
	#define HTTP_QUEUE  // Enable queue-based HTTP for more robust transmission
	#define CONNTECTION_PRETIME	3  // Pre-connect 3 seconds before packet due (gives 2s buffer for 5s interval)
	
	// Controls non-critical alert behavior during CONTINUOUS CRITICAL states (Tilt, Overspeed)
	// NOTE: SOS/Emergency state ALWAYS stores alerts (mandatory per CDAC spec) - not affected by this
	// When defined: Non-critical alerts (Geofence, Harsh, etc.) are STORED during Tilt/Overspeed
	//               and sent later in batch
	// When NOT defined: Non-critical alerts are sent immediately even during Tilt/Overspeed
	//#define STORE_ALERTS_DURING_CONTINUOUS_CRITICAL

	#ifdef CDAC_HIMACHAL
	#ifdef 		HTTP_SIMULATE
	#define 	DEFAULT_IP1  	"vltdgw.hp.gov.in"
	#else
	#define 	DEFAULT_IP1  	"https://vltdgw.hp.gov.in"
	#endif
	#define		DEFAULT_PORT1	"80"
	// NO Support for IP2!
	//#define TEST_SERVER
	#ifdef TEST_SERVER
	#define 	DEFAULT_IP3  	"78.46.190.117"
	#define		DEFAULT_PORT3	"50002" 	
	#else
	#define 	DEFAULT_IP3  	"13.234.160.106"
	#define		DEFAULT_PORT3	"8224" 	
	#endif

	#else

	#ifdef 		HTTP_SIMULATE
	#define 	DEFAULT_IP1  	"surakshamitr.org/CDC"
	#else
	#define 	DEFAULT_IP1  	"http://surakshamitr.org/CDC"
	#endif
	#define		DEFAULT_PORT1	"80"
	// NO Support for IP2!
	#define TEST_SERVER
	#ifdef TEST_SERVER
	#define 	DEFAULT_IP3  	"78.46.190.117"
	#define		DEFAULT_PORT3	"50002" 	
	#else
	#define 	DEFAULT_IP3  	"13.234.160.106"
	#define		DEFAULT_PORT3	"8224" 	
	#endif
	#endif
	#define 	DEFAULT_INV_HALT	60
	#define 	DEFAULT_INV_MOTION	20
	#define 	DEFAULT_INV_HEALTH	60*60
	#define 	DEFAULT_INV_CRIT	5		
	#define 	DEFAULT_INV_SLEEP	60*60
	#define 	DEFAULT_INV_STM		30
	#define 	DEFAULT_INV_FULL	60*120

	#define 	DEFAULT_HALT_TIME	760
	#define 	DEFAULT_SLEEP_TIME	8800

	#define 	DEFAULT_MOB0		"7017034104"
	#define 	DEFAULT_MOB1		"9655543732"

	#define		DEFAULT_VEHREG		"UNKNOWN"

	#define		DEFAULT_SPEED		70.0
	#define 	DEFAULT_HB			500
	#define 	DEFAULT_HA			1000
	#define		DEFAULT_OVERSPEED	70.0
	#define 	DEFAULT_RT			45
	#define 	DEFAULT_TL			45

#elif defined(PROTO_NIC1)

	//#define NIC_BIHAR
	//#define NIC_PONDI
	//#define NIC_UTTRA 
	//#define NIC_GOA
	//#define NIC_MDP
	//#define NIC_ISLAND
	#define NIC_TAMIL

	#define NO_PARAM
	#define DBM_IN_CSQ
	

	#if defined(NIC_PONDI)
	#define PRF_AUTOSWITCH
	#define PROTO_TAG 	"PD1"
	#define 	DEFAULT_IP1  	"164.100.64.227"
	#define		DEFAULT_PORT1	"9201"
	#define 	DEFAULT_IP2  	"164.100.64.227"
	#define		DEFAULT_PORT2	"9201" 	
	#define 	DEFAULT_IP3  	"13.234.160.106"
	#define		DEFAULT_PORT3	"8224"

	#elif defined(NIC_UTTRA)
	#define PRF_AUTOSWITCH
	#define PROTO_TAG 	"UT1"
	#define 	DEFAULT_IP1  	"vlt.uk.gov.in"
	#define		DEFAULT_PORT1	"9999"
	#define 	DEFAULT_IP2  	"vlt.uk.gov.in"
	#define		DEFAULT_PORT2	"9999" 	
	#define 	DEFAULT_IP3  	"13.234.160.106"
	#define		DEFAULT_PORT3	"8224"	


	#elif defined(NIC_GOA)
	#define PRF_AUTOSWITCH
	#define PROTO_TAG 	"GA1"
	#define 	DEFAULT_IP1  	"gavltspvt.goa.gov.in"
	#define		DEFAULT_PORT1	"9031"
	#define 	DEFAULT_IP2  	"gavltsemg.goa.gov.in"
	#define		DEFAULT_PORT2	"9032" 	
	#define 	DEFAULT_IP3  	"13.234.160.106"
	#define		DEFAULT_PORT3	"8224"	

	#elif defined(NIC_MDP)
	#define PRF_AUTOSWITCH
	#define PROTO_TAG 	"MP1"
	#define 	DEFAULT_IP1  	"mpdevice.vltsecurity.com"
	#define		DEFAULT_PORT1	"61487"
	#define 	DEFAULT_IP2  	"mpdevice.vltsecurity.com"
	#define		DEFAULT_PORT2	"61488"
	#define 	DEFAULT_IP3  	"13.234.160.106"
	#define		DEFAULT_PORT3	"8224"

	#elif defined(NIC_BIHAR)
	#define PRF_AUTOSWITCH
	#define PROTO_TAG 	"BH1"
	#define 	DEFAULT_IP1  	"brvlts.parivahan.gov.in"
	#define		DEFAULT_PORT1	"9031"
	#define 	DEFAULT_IP2  	"brvlts.parivahan.gov.in"
	#define		DEFAULT_PORT2	"9032"
	#define 	DEFAULT_IP3  	"13.234.160.106"
	#define		DEFAULT_PORT3	"8224"

	#elif defined(NIC_TAMIL)

	#define PRF_AUTOSWITCH
	#define PROTO_TAG 	"TN1"
	#define 	DEFAULT_IP1  	"stavltsgw.tn.gov.in"
	#define		DEFAULT_PORT1	"8080"
	#define 	DEFAULT_IP2  	"NA"
	#define		DEFAULT_PORT2	"0"
	#define 	DEFAULT_IP3		"tracking.vlvprotect.com"
	#define 	DEFAULT_PORT3 	"8080"
	#define 	DEFAULT_IP4  	"13.234.160.106:8224"

	#define 	SOS_FULL_EA

	#else
	#define PROTO_TAG 	"NI1"
	#define 	DEFAULT_IP1  	"vltspvt.delhi.gov.in"
	#define		DEFAULT_PORT1	"9031"
	#define 	DEFAULT_IP2  	"vltsemg.delhi.gov.in"
	#define		DEFAULT_PORT2	"9032" 	
	#define 	DEFAULT_IP3  	"13.234.160.106"
	#define		DEFAULT_PORT3	"8224"

	#endif

	#define 	DEFAULT_INV_DATA	30
	#define 	DEFAULT_INV_IGN		10
	#define 	DEFAULT_INV_HEALTH	250
	#define 	DEFAULT_INV_SOS		5		
	#define 	DEFAULT_INV_STB		60
	#define 	DEFAULT_INV_STM		120

	#define 	DEFAULT_MOB0		"9600696008"
	#define 	DEFAULT_MOB1		"9655543732"

	#define		DEFAULT_VEHREG		"UNKNOWN"

	#define		DEFAULT_SPEED		70.0
	#define 	DEFAULT_HB			500
	#define 	DEFAULT_HA			1000
	#define		DEFAULT_OVERSPEED	70.0
	#define 	DEFAULT_RT			45
	#define 	DEFAULT_TL			42
#elif defined (PROTO_ODISA1)
	#define DBM_IN_CSQ
	//#define ODISA_LD

	#ifdef ODISA_LD
	#define PROTO_TAG	"LD1"
	
	#define PRF_AUTOSWITCH
	#define 	DEFAULT_IP1  	"pvt.vltdladakh.in"
	#define		DEFAULT_PORT1	"60002"
	#define 	DEFAULT_IP2  	"emr.vltdladakh.in"
	#define		DEFAULT_PORT2	"61002" 	
	#define 	DEFAULT_IP3  	"103.143.84.2"
	#define		DEFAULT_PORT3	"8801"
	#define 	DEFAULT_IP4  	"13.234.160.106:8224"

	#define 	DEFAULT_INV_DATA	300
	#define 	DEFAULT_INV_IGN		10
	#define 	DEFAULT_INV_HEALTH	250
	#define 	DEFAULT_INV_SOS		5		
	#define 	DEFAULT_INV_STB		60
	#define 	DEFAULT_INV_STM		120
	
	#else
	#define PROTO_TAG	"OD1"
	#define HISTORY_DISABLED
	#define NO_TAMPER
	#define 	DEFAULT_IP1  	"pvtdevices.odishatransport.gov.in"
	#define		DEFAULT_PORT1	"8205"
	#define 	DEFAULT_IP2  	"emrdevices.odishatransport.gov.in"
	#define		DEFAULT_PORT2	"9202" 	
	#define 	DEFAULT_IP3  	"13.234.160.106"
	#define		DEFAULT_PORT3	"8224"

	#define 	DEFAULT_INV_DATA	60
	#define 	DEFAULT_INV_IGN		10
	#define 	DEFAULT_INV_HEALTH	250
	#define 	DEFAULT_INV_SOS		10		
	#define 	DEFAULT_INV_STB		300
	#define 	DEFAULT_INV_STM		3600
	#endif

	

	#define 	DEFAULT_MOB0		"9600696008"
	#define 	DEFAULT_MOB1		"9655543732"

	#define		DEFAULT_VEHREG		"UNKNOWN"

	#define		DEFAULT_SPEED		70.0
	#define 	DEFAULT_HB			500
	#define 	DEFAULT_HA			1000
	#define		DEFAULT_OVERSPEED	70.0
	#define 	DEFAULT_RT			45
	#define 	DEFAULT_TL			42

#elif defined (PROTO_OG)
	#define PROTO_TAG	"OG1"
	#define PRF_AUTOSWITCH
	#define 	DEFAULT_IP1  	"13.126.245.226"
	#define		DEFAULT_PORT1	"8080"
	#define 	DEFAULT_IP2  	"103.14.123.84"
	#define		DEFAULT_PORT2	"30002"
	#define 	DEFAULT_IP3  	"103.14.123.84"
	#define		DEFAULT_PORT3	"30002"


	#define 	DEFAULT_INV_DATA	30
	#define 	DEFAULT_INV_IGN		10
	#define 	DEFAULT_INV_HEALTH	250
	#define 	DEFAULT_INV_SOS		5		
	#define 	DEFAULT_INV_STB		60
	#define 	DEFAULT_INV_STM		120

	#define 	DEFAULT_MOB0		"7017034104"
	#define 	DEFAULT_MOB1		"9655543732"

	#define		DEFAULT_VEHREG		"UNKNOWN"

	#define		DEFAULT_SPEED		70.0
	#define 	DEFAULT_HB			500
	#define 	DEFAULT_HA			1000
	#define		DEFAULT_OVERSPEED	70.0
	#define 	DEFAULT_RT			45
	#define 	DEFAULT_TL			42

#else

	#error NO VALID PROTOCOL SELECTED !!!
#endif

#ifndef DEFAULT_IP4
	#define DEFAULT_IP4		"NA"
#endif


#ifndef PROTO_TAG
	#error "No PROTOCOL TAG Defined"
#endif

//#define VIRTUAL_IMEI
#define VIMEI  "861850061702714"

#ifdef VIRTUAL_IMEI
#ifdef VIMEI
	#warning VIRTUAL IMEI Is Enabled, Disable it for PRODUCTION USE!!!!!
#else
	#error VIMEI not Defined for Virtual Imei USE!!!!
#endif
#endif

typedef enum {UART2_MODE_LOG,UART2_MODE_RFID,UART2_MODE_MAX}Uart2Modetypedef;
typedef enum {IP2_MODE_NORMAL,IP2_MODE_DHT11,IP2_MODE_MAX}IP2ModeTypedef;

typedef enum {TAISYS=0, SENSORISE, GnD}SIMMakeTypedef;

typedef struct {
	Uart2Modetypedef Uart2Mode;
	IP2ModeTypedef IP2Mode;
	uint16_t IGNInterval;
	uint16_t OFFInterval;
}SensorSettingtypedef;

typedef enum {NORMAL, EMERGENCY,CRITICAL, ALERT, BATCH}PACKETSTATE;

typedef enum {HALT, MOTION, SLEEP}VEHICLEMODE;

typedef struct
{
	PACKETSTATE PacketState;
	VEHICLEMODE VehicleMode;
}VehicleTypeDef;

//typedef enum {LOG1_MODE_NONE,LOG1_MODE_RFID} LOG1UartMode;

typedef struct
{
	int ID;
	uint8_t InOut;
	uint8_t AlertInOut;
	double Latitude[10];
	double Longitude[10];
}GEOPOINTTypeDef;

typedef struct
{
	char Mob0[14];
	char Mob1[14];
	char Mob2[14];
	char Mob3[14];
	char Mob4[14];
	
}MobileNumTypeDefStruct;

typedef struct
{
	uint8_t IPConfig[3]; // whether the ip is enabled or not 
	char Url1[55];
	char Url2[55];
	char IP1[50];
	char IP2[50];
	char IP3[50];
	char Port1[7];
	char Port2[7];
	char Port3[7];
	
}ServerDataTypedef;

#ifndef PROTO_CDAC
typedef struct
{
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
}IntervalTypeDef;
#else
typedef struct
{
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
	
}IntervalTypeDef;

#endif

typedef struct
{
	uint16_t HarshBreak;
	uint16_t HarshAcc;
	uint16_t RashTurn;
	uint16_t TiltAngle;	
	char VehicleRegNo[20];
	double OverSpeed;
	double DefaultSpeed;
}VehicleTypedef;


typedef struct
{
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
	SIMMakeTypedef SIMMake;
}VTSTypedef;

extern VTSTypedef VTSData;


typedef struct FIMEITypedef
{
	uint8_t IsEnable;
	char Imei[18];
}FIMEITypeDef;

typedef struct 
{
	uint16_t DefVal;
	uint64_t OdoCount;
	uint8_t RegDeniedCount;
	uint8_t CurrentProfile;
	uint8_t IsPrevMain;
	FIMEITypeDef CustomImei;
	uint8_t ProfileFailCount[4];  // Track consecutive failures for each profile (0-3, where 0 is unused, 1-3 are profile numbers)
	uint8_t LastAttemptedProfile; // Track the last profile we attempted to switch to
}VTSStateTypedef;

extern VTSStateTypedef VTSState;


typedef struct
{
	uint8_t IsNormalPacket;
	uint8_t IsHealthPacket;
	uint8_t IsFullPacket;
	uint8_t IsCriticalPacket;
	uint8_t IsGSMParam;
	#ifndef PROTO_CDAC
	uint8_t IsHistoryPacket;
	#endif
	uint8_t IsSensPacket;
}PacketReadyTypedef;

extern PacketReadyTypedef IsPacketReady;

typedef struct
{
	volatile uint16_t NormalTick;
	volatile uint16_t HealthTick;
	volatile uint16_t FullTick;
	volatile uint16_t CriticalTick;
	volatile uint16_t ParamTick;
	#ifndef PROTO_CDAC
	volatile uint16_t HistoryTick;
	#endif
	volatile uint16_t SensTick;
}TickTypeDef;



 

extern uint8_t IsOverSpeed,PrevTamp;
extern volatile TickTypeDef IntervalTick;

// ZigTestMode: Manufacturing test mode flag
// When enabled (1), prevents automatic device resets
// Used for ZIG testing and manufacturing purposes
extern uint8_t ZigTestMode;



#endif							// _VTS_H


