#ifndef _GPRS_H
#define _GPRS_H

#include "VTS.h"




#define  DEF_SIM_SLOT   0

#define STK_TAISYS_SETUP_COUNT		2
#define STK_TAISYS_PROFILE_COUNT	2
#define STK_TAISYS_MENU			"810301250082028281830100"
#define	STK_TAISYS_ITEM			"D30782020181900101"
#define STK_TAISYS_PRIMARY		"810301240082028281830100900101"
#define STK_TAISYS_SECONDARY	"810301240082028281830100900102"
#define STK_TAISYS_THIRD		"810301240082028281830100900103"

#define STK_SENS_SETUP_COUNT			3
#define STK_SENS_PROFILE_COUNT			2
#define STK_SENS_MENU			"810301250082028281830100"
#define STK_SENS_ITEM			"D30782020181900180"
#define STK_SENS_NETWORK		"810301240402028281830100900102"
#define STK_SENS_PRIMARY		"810301240402028281830100900115"
#define STK_SENS_SECONDARY		"810301240402028281830100900116"


#define STK_GND_SETUP_COUNT			3
#define STK_GND_PROFILE_COUNT			3
#define STK_GND_MENU			"810301250082028281830100"
#define STK_GND_ITEM			"D30782020181900101"
#define STK_GND_NETWORK		"810301240082028281830100100101"
#define STK_GND_PRIMARY		"810301240082028281830100100101"
#define STK_GND_SECONDARY		"810301240082028281830100100102"
#define STK_GND_THIRD		"810301240082028281830100100103"

// COLORPLAST STK Commands (CORRECT FLOW per working notes)
#define STK_COLORPLAST_SETUP_COUNT        2  // MENU + ITEM only
#define STK_COLORPLAST_PROFILE_COUNT      3

// Step 1: Fetch STK Menu (Terminal Response)
#define STK_COLORPLAST_MENU               "810301250082028281830100"

// Step 2: Terminal profile / environment response (Envelope Command)
#define STK_COLORPLAST_ITEM               "D30782020181900180"

// Step 3: Account Selection with ID (XX = 01/02/03)
// DO NOT use NETWORK - profiles include full command with account ID
#define STK_COLORPLAST_PRIMARY            "810301240082028281830100900101"   // AIRTEL (01)
#define STK_COLORPLAST_SECONDARY          "810301240082028281830100900102"   // BSNL (02)
#define STK_COLORPLAST_THIRD              "810301240082028281830100900103"   // VIL (03)

// Step 4: Confirmation command (same for all profiles)
#define STK_COLORPLAST_CONFIRM            "810301010482028281830100"	
#define STK_SETUP_CMD_MAX	4
#define STK_PROFILE_CMD_MAX	3

typedef struct 
{
	uint8_t type;
	char data[40];	
}CommandDatatypedef;


typedef struct 
{
	uint8_t is_valid;
	uint8_t setup_cmd_count;
	CommandDatatypedef Setup[STK_SETUP_CMD_MAX];
	uint8_t profiles_supported;
	CommandDatatypedef Profile[STK_PROFILE_CMD_MAX];

}STKDatatypedef;

typedef struct
{
	int mnc;
	int mcc;
	char CellID[8];
	char LAC[8];
	char CellDB[8];
}NeigbourCellTypedef;

extern char sIMEI[];
/**
 * GSM Structure
 * @param GSMState - Current GSM State
 * @param SignalStrength - Current GSM Signal CSQ
 * @param MCC - Mobile Country Code
 * @param MNC - Mobile Network Code
 * @param LAC - Location area code
 * @param CellID - Tower Cell ID
*/
typedef struct
{
	enum {SIM_NOT_DETECTED=0,SIM_DETECTED,GPRS_INIT,GPRS_ACTIVE} GSMState;
	uint8_t SignalStrength;
	uint16_t IsModemOK;
	uint32_t ModemRespCount;
	uint16_t MCC;
	uint16_t MNC;
	char LAC[6];
	char CellID[8];
	NeigbourCellTypedef NeigbourCell[4];
	uint8_t IsTimeSet;
	uint8_t IsNeighbourCells;
	uint8_t IsRegDenied;  // Flag for registration denied state
}GSM_Typedef;


typedef enum {NONE,VI,BSNL,AIRTEL,JIO}Providertypedef;

/**
 * Network Structure
 * @param IMEI - Current Device IMEI
 * @param IMSI - Current Sim IMSI
 * @param Network - Network Vendor Name
 * @param SIMNo - Sim IMSI Number
 * @param APN - Access Point Name
*/
typedef struct 
{
	char IMEI[30];
	char IMSI[30];
	char Network[60];
	char SIMNo[30];
	char APN[50];
	Providertypedef Provider;
	
}NET_Typedef;

/**
 * RTC Date&Time Structure
*/
typedef struct _RTC
{
  uint16_t Year;
  uint8_t Month;
  uint8_t Date;
  uint8_t DaysOfWeek;
  uint8_t Hour;
  uint8_t Min;
  uint8_t Sec;
} _RTC;

extern _RTC CurrentDateTime;

#include "Utilities.h"



extern GSM_Typedef GSM;
extern NET_Typedef NetWork;

/**
 * GPRS Main Thread
 * @note Independent task, dont return;
 * @param none
*/
void GprsThreadEntry(void *param);

/**
 * Check GRPS State
 * @note Checks Current GPRS State
 * @param none
 * @return 1 if Connected, 0 if not Connected
*/
int CheckGPRSState(void);
uint8_t GetNextValidProfile(uint8_t currentProfile);
uint8_t SwitchProfile(uint8_t num);
uint8_t SendAtCmd(char* str, char* resp, char* grep);
uint8_t GetNeighbourCells(void);
void GetImei(void);
extern Providertypedef prfReq;
extern uint8_t PrfChanged;
s32 SetupAutoTimesync(void);

void InitGPRSThread(u32 taskId);
void GPRSThreadEntry(s32 taskId);
#endif