
#ifndef					_SOS_H
#define					_SOS_H


#include "VTS.h"

//***********SOS************************************
#define							INPUT_SOS_PIN					PINNAME_PCM_SYNC
#define            				INPUT_SOS_INIT					Ql_GPIO_Init(INPUT_SOS_PIN,PINDIRECTION_IN,PINLEVEL_LOW,PINPULLSEL_DISABLE);
#define							INPUT_SOS_VAL					Ql_GPIO_GetLevel(INPUT_SOS_PIN)





//in unit of 100ms
#define	SOS_OFF_LED_ON_TIME					2
#define	SOS_OFF_LED_OFF_TIME				8
#define	SOS_ON_LED_ON_TIME					1
#define	SOS_ON_LED_OFF_TIME				    4
// *************************************************


#define		SLED_ON				hw_led_state_set(SOSLED,1,SOS_ON_LED_ON_TIME,(SOS_ON_LED_ON_TIME+SOS_ON_LED_OFF_TIME))
#define		SLED_OFF			hw_led_state_set(SOSLED,1,SOS_OFF_LED_ON_TIME,(SOS_OFF_LED_ON_TIME+SOS_OFF_LED_OFF_TIME))

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
void SetSOSTimeOutSeconds(uint16_t seconds);
void UpdateSOSTimeOut(char* value);


#endif					// _SOS_H



