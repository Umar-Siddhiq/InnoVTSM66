#ifndef _MCU_H
#define _MCU_H

#include "project.h"

// Forward declare _RTC if not already defined (to handle circular dependency)
#ifndef _RTC_DEFINED
#define _RTC_DEFINED
typedef struct
{
  uint16_t Year;
  uint8_t Month;
  uint8_t Date;
  uint8_t DaysOfWeek;
  uint8_t Hour;
  uint8_t Min;
  uint8_t Sec;
} _RTC;
#endif

#define RSBufferSIZE 100
#define RSSendSIZE	200
#define OTABufferSIZE 256

extern char RSSend[];
extern char RSBuffer[];
extern char OTABuffer[];

extern uint8_t IsRsCmd, IsMCUTime;
extern uint8_t IsOTACmd;

typedef struct 
{
    uint32_t Address;
    uint8_t rtr;
    uint8_t dlc;
    uint8_t data[8];
}CANPKT;
extern CANPKT CanMsg;

#define MCUHEADER   "&"
#define MCUFOOTER   "\r\n"

/**
** MCU PACKET TYPES
*/
#define INPUTUPDATE     "#GIP"
#define CELLUPDATE      "#CEL"
#define RS485COM        "#RCM"
#define CANCOM          "#CCM"
#define ADCUPDATE       "#AUP"
#define GPSUPDATE       "#GPS"
#define DBGSTATE        "#DBG"
#define SOSLED          "#SOS"
#define ALIVE           "#ALV"
#define PROFCHANGE		"#PRF"
#define RESET			"#RST"
#define ATCMD			"#ATC"
#define OTACMD			"#OTA"
#define SMSCMD			"#SMS"
#define HANDLING		"#LSM"
#define BEHAVE			"#BHV"
#define ADDHISTORY		"#ADH"
#define GETHISTORY		"#GDH"
#define GETHCOUNT		"#GHC"
#define DELHISTORY		"#DHC"
#define MOTASTART		"#MTA"
#define MCUOK			"#OK"
#define MCUERR			"#ERR"
#define MCUVER			"#MVR"
#define FUELDATA		"#FLD"
#define GPSRESET		"#RGM"


/**********************MCU PROTOCOL*************************
** &#ALV\r\n
*! RCV : &#ALV\r\n
*? Modem Sends on regular interval to show its working
*  TODO: Reset Modem if Not receiving (after an delay)
************************************************************
** &#GIP,ign\r\n
*! Send : &#GIP,1\r\n
*? Ignition Input Status
************************************************************
** &#CEL,stat\r\n
*! Rcv : &#CEL,1\r\n
*? Battery ON/OFF
************************************************************
** &#SOS,state\r\n
*! Rcv : &#SOS,1\r\n
*? SOS Led Status
************************************************************
** &#RCM,message\r\n
*! Send : &#RCM,This is Example String\r\n
*? RS232 String Update
************************************************************
** &#CCM,id,type,dlc,d1,d2,d3,d4,d5,d6,d7,d8\r\n
*! Send : &#CCM,418382384,1,8,32,234,23,21,23,43,13,86\r\n
*? CAN Update , data parameters count can Vary depending on dlc value
************************************************************
** &#AUP,mains,ad1,ad2\r\n
*! Send : &#AUP,15350,2300,3300\r\n
*? Analog Input Update, Send Voltage not raw adc, unit mV
************************************************************
** &#GPS,state,lat,long,latDir,longDir,Alt,Speed,PDOP,HDOP,Heading,NoOfSats\r\n 
*! Send : &#GPS,1,28868493,76093842,N,E,30,45,0,0,0,3\r\n
*? GPS Update, Lat & long multiplied by 1000000, Alt * 10, Speed * 10, PDOP * 10, HDOP* 10, Heading * 10;
************************************************************
** &#DBG,state\r\n
*! Send : &#DBG,1\r\n
*? Macro to Turn on/off Debug Printf
************************************************************
** &#PRF,prof\r\n
*! Send : &#PRF,1\r\n
*? 1 : BSNL, 2: VI, 3: AIRTEL
************************************************************
** &#RST\r\n
*! Send : &#RST\r\n
*? Resets The Modem
************************************************************
** &#ATC,cmd\r\n
*! Send : &#ATC,AT+CIMI\r\n
*? Send Cmd to Virtual AT for Debugging
************************************************************
** &#OTA,cmd\r\n
*! Send : &#OTA,GETVINFO\r\n
*? Simulates a SMS Recieve with provided cmd and sends to registered mobile number 1
************************************************************
** &#SMS,number,msg\r\n
*! Send : &#SMS,7017044033,IP Updated\r\n
*? Sends SMS msg to provided number 
************************************************************
** &#LSM,n\r\n
*! Send : &#LSM,1\r\n
*? 1 = Harsh Accel, 2 = Harsh Brake, 3 = Rash Turn, 4 = Vehicle Tilt
************************************************************
** &#BHV,accel,break,turn,tilt\r\n
*! Send : &#BHV,1000,5000,45,42\r\n
*? Sends gyro setting data to MCU
************************************************************
** &#ADH,data\r\n
*! Send : &#ADH,$PVT,APMG,1.0.8,NR,01,L,861850060252893,TN14AJ5465,0,20072024,082201,0.0000000,N,0.0000000,E,000.0,000.00,00,0.0,0.0,0.0,airtel,0,1,12.0,3.7,0,C,12,404, 40,371E,7E57,44A5,371E,-87,7280,371E,-95,0,0,0,0,0,0,0001,00,000007,43DF30C3*\r\n
*? Sends Add history data to MCU
************************************************************
** &#GDH\r\n
*! RCV : &#GDH,$PVT,APMG,1.0.8,NR,01,L,861850060252893,TN14AJ5465,0,20072024,082201,0.0000000,N,0.0000000,E,000.0,000.00,00,0.0,0.0,0.0,airtel,0,1,12.0,3.7,0,C,12,404, 40,371E,7E57,44A5,371E,-87,7280,371E,-95,0,0,0,0,0,0,0001,00,000007,43DF30C3*\r\n
*? Sends Get last history data from MCU
************************************************************
** &#GHC\r\n
*! RCV : &#GHC,32\r\n
*? Sends Get history packet count from MCU
************************************************************
** &#DHC\r\n
*! RCV : &#OK\r\n
*? Sends Delete last history packet req to MCU
************************************************************
** &#RGM\r\n
*! RCV : &#OK\r\n
*? Resets the GPS Module
*/

//---------------------------------------------------------MOTA------------------------------------

extern uint8_t IsMOTAProcess;
extern char MCUVersion[];

#define MCUMOTAHEADER   '&'
#define MCUMOTAFOOTER   '~'

#define MCU_PACKET_HEADER_INDEX     0
#define MCU_PACKET_TYPE_INDEX       1 

#define MCU_RES_OK                              0x1
#define MCU_RES_ERR                             0x2
#define MCU_RES_INIT                            0x0

#define MCU_MOTAREPLY_LEN               7

#define MCU_REPLY_TIMEOUT 2500
#define BMS_OTA_LOOP_DATASIZE 256

#define MCU_PACKET_HEADER_AND_TYPE_OVERHEAD     0x2
#define MCU_MOTAREPLY_FOOTER_INDEX              4

typedef struct 
{
    uint16_t pktcount;
    uint16_t checksum;
}MOTAReplyTypedef;

typedef enum {MOTA_NONE,MOTA_WAIT,MOTA_REPLY}MOTAStateTypedef;

#define MOTA_MAX_PACKT_SIZE  256

typedef enum {CANMCU_ID_BMSDATA=1,CANMCU_ID_BAUDRATE,CANMCU_ID_CANMSG,CANMCU_ID_EXTRA,CANMCU_ID_CUSTOM,CANMCU_ID_BATCH,
                CANMCU_ID_VER,CANMCU_ID_MOTASTART,CANMCU_ID_MOTADATA,CANMCU_ID_MOTAREP,CANMCU_ID_MOTADONE}CanFrameTypedef;

	
#define BMS_OTA_LOOP_DATASIZE 256

uint8_t ProcessMCUOTA(char* Filename);

//---------------------------------------------------------------------------------------------------
 
//Structures 
/**
 * GPS Structure
*/
typedef struct
{
	uint8_t GPSFix;			// 1 or 0
	double Latitude;
	char LatDir;
	double Longitude;
	char LngDir;
	double Speed;
	double Heading;
	uint8_t NoOfSatalite;
	double Altitude;
	double PDOP;
	double HDOP;
	_RTC DateTime;
}GPS_Typedef;

extern char	sLatitude[];
extern char sLongitude[];
extern char sAltitude[];
extern char sSpeed[];
extern char sPDOP[];
extern char sHDOP[];
extern char sHeading[];


extern  GPS_Typedef GPS;
extern uint8_t IsMCU, IsFuelData, IsGPSFault;
extern char FuelData[];

/**
 * MCU Main Thread
 * @note Independent task, dont return;
 * @param none
*/
void MCUThreadEntry(void *param);

/**
 * Set or Reset SOS LED
 * @param state 1 for ON, 0 for OFF
*/
double calculateDistance(double lat1, double lon1, double lat2, double lon2);
void SOSLedSet(uint8_t state);
void SendGIPReq(void);
void SendCellCmd(uint8_t stat);
void SendAUPReq(char* str);
void SendAlivePacket(void);
void GetGPSState(void);
void DecodeATCommand(char *str);
void SendGyroSettingPacket(void);
void SendRS232String(char *str);
void ClearMcuSendBuffer(void);
void SendSleepReq(void);
void SendGPSResetReq(void);

#ifndef HISTORY_INTERNAL
uint8_t WriteHistoryData(char* data);
uint8_t GetHistoryCount(uint16_t *count);
uint8_t GetHistoryData(char *data);
uint8_t DeleteHistoryData(void);
#endif
void mcu_InitUart(void);
uint8_t GetMCUVersionReq(void);
#endif
