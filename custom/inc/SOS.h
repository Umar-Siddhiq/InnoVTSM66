
#ifndef					_SOS_H
#define					_SOS_H


#include "VTS.h"

//***********SOS************************************
#define							INPUT_SOS_PIN					PINNAME_PCM_SYNC
#define            				INPUT_SOS_INIT					Ql_GPIO_Init(INPUT_SOS_PIN,PINDIRECTION_IN,PINLEVEL_HIGH,PINPULLSEL_PULLUP);
#define							INPUT_SOS_VAL					Ql_GPIO_GetLevel(INPUT_SOS_PIN)

#ifdef SOS_NC_CIRCUIT
#define SOS_ACTIVE_LEVEL   1
#define SOS_IDLE_LEVEL     0
#else
#define SOS_ACTIVE_LEVEL   0
#define SOS_IDLE_LEVEL     1
#endif

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
	uint16_t SOSTamperTimeLapsed;
	uint8_t RequireRelease;   /* require the pin to return to idle before the next ON trigger */

}SOSTypeDefStruct;



extern volatile SOSTypeDefStruct SOS;


void SOSInit(uint16_t timeOut);
void ProcessSOS(void);
void ResetSOS(void);
void SetSOSTimeOutSeconds(uint16_t seconds);
void UpdateSOSTimeOut(char* value);


#endif					// _SOS_H



