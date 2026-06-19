
#ifndef					_HARDWARE_H
#define					_HARDWARE_H

#include <stdio.h>
#include "project.h"


#define							LED_PIN						31
#define                         STATLED_ON                  nwy_gpio_set_value(LED_PIN,nwy_high)
#define                         STATLED_OFF                 nwy_gpio_set_value(LED_PIN,nwy_low)
#define                         STATLED_VAL                 nwy_gpio_get_value(LED_PIN);

#define							IP1_PIN						6
#define							IP1_VAL						nwy_gpio_get_value(IP1_PIN)
#define							IP2_PIN						33
#define							IP2_VAL						nwy_gpio_get_value(IP2_PIN)


#define							OP1_PIN						25
#define                         OP1_Set(x)                  nwy_gpio_set_value(OP1_PIN,x);PeriPheralVal.OP1=x
#define                         OP1_get                     nwy_gpio_get_value(OP1_PIN)

#define                         OP2_PIN                     29    
#define                         OP2_Set(x)                  nwy_gpio_set_value(OP2_PIN,x);PeriPheralVal.OP2=x
#define                         OP2_get                     nwy_gpio_get_value(OP2_PIN)

#define							BAT_PIN						NWY_ADC_CHANNEL0

#define							SYS100MS_PIN				12
#define 						SYS1S_PIN					11

#ifdef PROTO_CDAC

#define				IP1MASK						0x01
#define				IP2MASK						0x02
#define				IGNMASK						0x04
#define				MNSMASK						0x08
#define				OPNMASK						0x10

#define				OP1MASK						0x20
#define				OP2MASK						0x40

#endif

typedef struct
{
	uint8_t IP1;
    uint8_t IP2;
	uint8_t IGN;
	uint8_t OP1;
    uint8_t OP2;
	uint8_t IsMain;
    double MainsVolt;
	uint8_t IsCoverOpen;
	uint8_t IsMEMs;
	uint8_t IsFlash;
	double BattVolt;
	uint8_t BattPerc;
	double AN1;
	double AN2;
	uint8_t IsTilt;
	uint8_t PendingSMSAlert;  // SMS alert number pending to be sent by Server thread (non-blocking)
	
}PepheralTypedef;

extern PepheralTypedef PeriPheralVal;


#define RFID_MAX_DATALEN 100
extern uint8_t RFIDData[],PrevMain;
extern uint16_t RFIDDataCount;

//  ---------------------------------------------------------
// Prototype Declaration
//

extern uint8_t IsSleepMode;
void SleepModeON(void);
void SleepModeOFF(void);


void SYS_Init(void);
float batteryPercentageToVoltage(int percentage);
int batteryVoltageToPercentage(float voltage);
void PeripheralInit(void);
void ProcessPeripheral(void);
void EnableVat(void);
void RFIDRcv(const char *data, uint32 length);
#endif

