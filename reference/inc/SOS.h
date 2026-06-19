
#ifndef					_SOS_H
#define					_SOS_H


#include "project.h"

//***********SOS************************************
#define				SOS_PIN					34
#define 			SOS_LED_PIN				BIT10

#define	LED_ON_TIME					200
#define	LED_OFF_TIME				800
// *************************************************

#define     SOS_STATE           nwy_gpio_get_value(SOS_PIN)

#define		SLED_ON				SOSLedSet(1);
#define		SLED_OFF			SOSLedSet(0);

typedef struct
{
	uint8_t IsSOS;
	uint8_t SOSPushCount;
	uint8_t IsSOSTamper;
	uint8_t IsSOSSMS;
	uint16_t SOSTimeOut;
	uint16_t SOSTimeLasped;
	
}SOSTypeDefStruct;



extern  SOSTypeDefStruct SOS;


void SOSInit(uint16_t timeOut);
void ProcessSOS(void);
void ResetSOS(void);
void UpdateSOSTimeOut(char* value);


#endif					// _SOS_H



