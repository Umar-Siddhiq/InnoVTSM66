
#ifndef							_ALERT_H
#define							_ALERT_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "VTS.h"

#define ACK_BUFFER_SIZE 200

static inline void SafeAppendACK(char *ack, const char *src)
{
	size_t currentLen;
	size_t srcLen;
	size_t spaceLeft;

	if(ack == NULL || src == NULL)
		return;

	currentLen = strlen(ack);
	srcLen = strlen(src);
	spaceLeft = ACK_BUFFER_SIZE - currentLen - 1;
	if(srcLen > spaceLeft)
		srcLen = spaceLeft;
	if(srcLen > 0)
		strncat(ack, src, srcLen);
}

#ifdef PROTO_CDAC

#define					ALERT_COUNT				19
#else
#define					ALERT_COUNT				21
#endif

#ifndef PROTO_CDAC
#define 	MAX_ACK_SIZE						128

#define		SOS_ON_ALERT						0
#define		SOS_OFF_ALERT						1
#define		SOS_TMP_ALERT						2
#define		MAINS_FAIL_ALERT				3
#define		TILT_ALERT							4
#define		TAMPER_ALERT						5
#define		OVER_SPEED_ALERT				6
#define		HARSH_BRK_ALERT					7
#define		HARSH_ACC_ALERT					8
#define		RASH_TURN_ALERT					9
#define		IMPACT_ALERT						10
#define		GFIN_OS_ALERT						11
#define		GFOUT_OS_ALERT					12
#define		GFIN_ALERT							13
#define		GFOUT_ALERT							14
#define		CONF_CHANGE_ALERT				15
#define		MAINS_RES_ALERT					16
#define		BATT_LOW_ALERT					17
#define		BATT_LOW_RES_ALERT			18
#define 	IGN_ON_ALERT				19
#define		IGN_OFF_ALERT				20

#else

static const uint8_t VTAlertHeaderType[ALERT_COUNT] = {0,0,1,1,1,2,1,2,2,2,1,1,1,2,2,3,2,2,2};	
static const char VTAlertPKT[ALERT_COUNT][3] ={"10","11","16","03","22","09","17","13","14","15","23","20","21","18","19","12","06","04","05"};
static const uint8_t VTContType[ALERT_COUNT]= {1,0,2,0,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0};

#define		SOS_ON_ALERT						0
#define		SOS_OFF_ALERT						1
#define		SOS_TMP_ALERT						2
#define		MAINS_FAIL_ALERT					3
#define		TILT_ALERT							4
#define		TAMPER_ALERT						5
#define		OVER_SPEED_ALERT					6
#define		HARSH_BRK_ALERT						7
#define		HARSH_ACC_ALERT						8
#define		RASH_TURN_ALERT						9
#define		IMPACT_ALERT						10
#define		GFIN_OS_ALERT						11
#define		GFOUT_OS_ALERT						12
#define		GFIN_ALERT							13
#define		GFOUT_ALERT							14
#define		CONF_CHANGE_ALERT					15
#define		MAINS_RES_ALERT						16
#define		BATT_LOW_ALERT						17
#define		BATT_LOW_RES_ALERT					18

#endif

// ***************ALERT NAME IN SMS *******************

#define		ALT_ID_10								"Emergency State ON"
#define		ALT_ID_11								"Emergency State OFF"
#define		ALT_ID_16								"Emergency wirecut"
#define		ALT_ID_17								"Overspeed"
#define		ALT_ID_03								"Disconnect from main battery"
#define		ALT_ID_20								"Overspeed+GF Entry"
#define		ALT_ID_21								"Overspeed+GF Exit"
#define		ALT_ID_22								"Tilt"
#define		ALT_ID_23								"Impact"

//*******************************************************
#ifndef PROTO_CDAC
typedef struct
{
	uint8_t TotalAlert;
//	uint8_t TotalCritical;
	uint8_t AlertPosition;
	uint8_t Alerts[10];
}StoreAlertTypedef;

extern StoreAlertTypedef StoredAlert;

typedef struct
{
	uint8_t Index;
	uint8_t Enable;
	uint8_t IsSMS;
	uint16_t SMSCounter;
	uint8_t IsCritical;
	uint8_t WithACK;
	char Header[4];
	char ID[3];	
}AlertTypedef;
#else
typedef struct
{
	uint8_t TotalAlert;
//	uint8_t TotalCritical;
	uint8_t AlertPosition;
	uint8_t Alerts[10];
}StoreAlertTypedef;

extern StoreAlertTypedef StoredAlert;

typedef struct
{
	uint8_t Index;
	uint8_t Enable;
	uint8_t IsCritical;
	uint8_t WithACK;
	uint8_t AlertSent;
	enum {ALT_CONT_NONE,ALT_CONT_URE,ALT_CONT_NORMAL}ContMode;
	char Header[8];
	char ID[10];
	char ACK[200];	
}AlertTypedef;
#endif
extern AlertTypedef VAlert[];


void AlertInitStruct(void);
void AddAlert(uint8_t alert);
void RemoveAlert(uint8_t alert);
void RemoveNonRepeatAlert(uint8_t pos);


#endif							// _ALERT_H
