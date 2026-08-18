
#ifndef					_HARDWARE_H
#define					_HARDWARE_H

#include <stdio.h>
#include "VTS.h"
#include "LOG.h"


#define							LED_GSM_GPIO				PINNAME_DTR
#define                         LED_GSM_ON	                Ql_GPIO_SetLevel(LED_GSM_GPIO,PINLEVEL_HIGH)
#define                         LED_GSM_OFF                 Ql_GPIO_SetLevel(LED_GSM_GPIO,PINLEVEL_LOW)
#define                         LED_GSM_VAL                 Ql_GPIO_GetLevel(LED_GSM_GPIO)
#define 						LED_GSM_INIT				Ql_GPIO_Init(LED_GSM_GPIO,PINDIRECTION_OUT,PINLEVEL_LOW,PINPULLSEL_DISABLE)


#define							LED_GPS_GPIO				PINNAME_RI
#define                         LED_GPS_ON	                Ql_GPIO_SetLevel(LED_GPS_GPIO,PINLEVEL_HIGH)	
#define                         LED_GPS_OFF                 Ql_GPIO_SetLevel(LED_GPS_GPIO,PINLEVEL_LOW)
#define                         LED_GPS_VAL                 Ql_GPIO_GetLevel(LED_GPS_GPIO)
#define 						LED_GPS_INIT				Ql_GPIO_Init(LED_GPS_GPIO,PINDIRECTION_OUT,PINLEVEL_LOW,PINPULLSEL_DISABLE)

#define							LED_BATTERY_GPIO			PINNAME_DCD
#define                         LED_BATTERY_ON	            Ql_GPIO_SetLevel(LED_BATTERY_GPIO,PINLEVEL_HIGH)
#define                         LED_BATTERY_OFF             Ql_GPIO_SetLevel(LED_BATTERY_GPIO,PINLEVEL_LOW)
#define                         LED_BATTERY_VAL             Ql_GPIO_GetLevel(LED_BATTERY_GPIO)
#define 						LED_BATTERY_INIT			Ql_GPIO_Init(LED_BATTERY_GPIO,PINDIRECTION_OUT,PINLEVEL_LOW,PINPULLSEL_DISABLE)


#define 						LED_SOS_GPIO				PINNAME_RTS
#define                         LED_SOS_ON	                Ql_GPIO_SetLevel(LED_SOS_GPIO,PINLEVEL_HIGH)
#define                         LED_SOS_OFF                 Ql_GPIO_SetLevel(LED_SOS_GPIO,PINLEVEL_LOW)
#define                         LED_SOS_VAL                 Ql_GPIO_GetLevel(LED_SOS_GPIO)
#define 						LED_SOS_INIT				Ql_GPIO_Init(LED_SOS_GPIO,PINDIRECTION_OUT,PINLEVEL_LOW,PINPULLSEL_DISABLE)

#define						    OUTPUT_1_GPIO			    PINNAME_PCM_CLK
#define 					   	OUTPUT_1_ON	            	Ql_GPIO_SetLevel(OUTPUT_1_GPIO,PINLEVEL_HIGH)
#define 					   	OUTPUT_1_OFF            	Ql_GPIO_SetLevel(OUTPUT_1_GPIO,PINLEVEL_LOW)
#define 					   	OUTPUT_1_VAL            	Ql_GPIO_GetLevel(OUTPUT_1_GPIO)
#define 					   	OUTPUT_1_INIT				Ql_GPIO_Init(OUTPUT_1_GPIO,PINDIRECTION_OUT,PINLEVEL_LOW,PINPULLSEL_DISABLE)

#define						    OUTPUT_2_GPIO			    PINNAME_NETLIGHT
#define 					   	OUTPUT_2_ON	            	Ql_GPIO_SetLevel(OUTPUT_2_GPIO,PINLEVEL_HIGH)
#define 					   	OUTPUT_2_OFF            	Ql_GPIO_SetLevel(OUTPUT_2_GPIO,PINLEVEL_LOW)
#define 					   	OUTPUT_2_VAL            	Ql_GPIO_GetLevel(OUTPUT_2_GPIO)
#define 					   	OUTPUT_2_INIT				Ql_GPIO_Init(OUTPUT_2_GPIO,PINDIRECTION_OUT,PINLEVEL_LOW,PINPULLSEL_DISABLE)



#define							INPUT_IGNITION_GPIO			PINNAME_PCM_IN
#define							INPUT_IGNITION_INIT			Ql_GPIO_Init(INPUT_IGNITION_GPIO,PINDIRECTION_IN,PINLEVEL_LOW,PINPULLSEL_DISABLE)
#define							INPUT_IGNITION_VAL			Ql_GPIO_GetLevel(INPUT_IGNITION_GPIO)



#define 						CONTROL_CHARGER_GPIO		PINNAME_PCM_OUT
#define 						CONTROL_CHARGER_INIT		Ql_GPIO_Init(CONTROL_CHARGER_GPIO,PINDIRECTION_OUT,PINLEVEL_LOW,PINPULLSEL_DISABLE)
#define 						CONTROL_CHARGER_ON			Ql_GPIO_SetLevel(CONTROL_CHARGER_GPIO,PINLEVEL_LOW)
#define 						CONTROL_CHARGER_OFF			Ql_GPIO_SetLevel(CONTROL_CHARGER_GPIO,PINLEVEL_HIGH)

#define 						ADC_VBAT_GPIO				PIN_ADC0







// #define							OP1_PIN						25
#define                         OP1_Set(x)                  Ql_GPIO_SetLevel(OUTPUT_1_GPIO,x);PeriPheralVal.OP1=x
#define                         OP1_get                     Ql_GPIO_GetLevel(OUTPUT_1_GPIO)

// #define                         OP2_PIN                     29    
#define                         OP2_Set(x)                  Ql_GPIO_SetLevel(OUTPUT_2_GPIO,x);PeriPheralVal.OP2=x
#define                         OP2_get                     Ql_GPIO_GetLevel(OUTPUT_2_GPIO)

// #define							BAT_PIN						NWY_ADC_CHANNEL0

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


/* UNKNOWN is appended deliberately — the existing values are reported in the
 * $BATT RS232 frame and must keep their numbering. */
typedef enum {BATTERY_STATUS_NOBATTERY=0, BATTERY_STATUS_CHARGING, BATTERY_STATUS_DISCHARGING, BATTERY_STATUS_FULL, BATTERY_STATUS_UNKNOWN} BatteryStatusTypedef;

/* --- Battery presence detection is NOT POSSIBLE on this hardware revision ---
 * Set to 1 only when a real presence signal is wired (see below).
 *
 * MEASURED 2026-08-12, no battery installed, charger GPIO pulsed OFF for 15 s
 * with the full rail curve logged once per second:
 *     baseline 4.20V
 *     t=0 4.18  t=1 4.16  t=2 4.14  t=3 4.12  t=4 4.10
 *     t=5..t=14  4.09 4.09 4.09 4.09 4.09 4.09 4.09 4.09 4.09 4.09
 *     => slope over the 10 s window = 2 mV, total step = 102 mV
 * The rail drops 110 mV and then HOLDS FLAT at 4.09 V for ten seconds with no
 * cell present, i.e. CONTROL_CHARGER_OFF does not open the VBAT path — the node
 * stays actively driven. The 102 mV is a regulation step, not a discharge.
 * A charged Li-ion cell's OCV (~4.05-4.15 V) is indistinguishable from it, so
 * NO threshold on this node can separate "cell" from "no cell".
 *
 * Every other candidate signal was checked against the same log and is dead:
 *   - MCU BattADC  : same node (BattADC/BattVolt constant ~528, and it tracks
 *                    the identical 2221->2170 step and plateau)
 *   - MCU ADCVal1  : flat 98-103 across the probe, unresponsive
 *   - MCU ADCVal2  : flat 3718-3757 across the probe, unresponsive
 *   - MCU IP1/IP2  : constant 1, not a charger STAT
 *   - M66 GPIOs    : all nine defined pins are allocated; no spare, no STAT in
 * To make detection possible, hardware must provide ONE of:
 *   (a) the charger IC's STAT/CHG open-drain output routed to an M66 GPIO or an
 *       MCU digital input, or
 *   (b) a charger EN that truly high-Zs the output so the rail can collapse, or
 *   (c) a sense divider on the BATTERY side of a blocking FET/diode, sampled
 *       while the charge path is open.
 * Until then the firmware must NOT assert that a battery is present. */
#define BATT_PRESENCE_DETECT_SUPPORTED 0
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
	double BattVolt;
	BatteryStatusTypedef BatteryStatus;
	uint8_t BattPerc;
	double AN1;
	double AN2;
	uint8_t IsFlash;       // <- fixed
    uint8_t IsMems;        // <- fixed
    uint8_t IsTilt;        // <- fixed
    char MCUFirmwareVersion[10];
}PepheralTypedef;

extern PepheralTypedef PeriPheralVal;

typedef struct 
{
	uint8_t IsEnabled;
	uint32_t TimeRemaining;
	#ifdef AUTO_SLEEP_ENABLE
	uint32_t IgnOffTimer;  // Timer in seconds to track ignition off duration
	#endif
}SleepConfigTypedef;

extern SleepConfigTypedef SleepConfig;
uint8_t SleepConfig_IsEnabled(void);


#define RFID_MAX_DATALEN 100
extern uint8_t RFIDData[],PrevMain;
extern uint16_t RFIDDataCount;


#define MAX_LED_COUNT 5

typedef struct LEDState
{
    uint8_t IsEnabled;
    uint16_t ONTime;
    uint16_t TotalTime;
	uint16_t TimeCount;
	int PinName;
	uint16_t PhaseOffset; 
}LEDStateTypedef;

typedef struct LEDSystem
{
    uint8_t LEDCount;
    LEDStateTypedef LEDState[MAX_LED_COUNT];
} LEDSystemTypedef;

extern LEDSystemTypedef LEDSystem;

//  ---------------------------------------------------------
// Prototype Declaration
//
extern int GSMLED,GPSLED,BATTERYLED,SOSLED;
void hw_led_struct_init(void);
void hw_led_process(void);
int hw_led_add(int PinName, uint16_t ONTime, uint16_t TotalTime);
void hw_led_init(void);
uint8_t hw_led_state_set(int LED, uint8_t State, uint16_t ONTime, uint16_t TotalTime);
uint8_t hw_battery_connected(void);   /* 1 = battery present, 0 = not connected / faulty */


void hw_init(void);

extern uint8_t IsSleepMode;
uint8_t SleepModeON(uint32_t SleepTime);
uint8_t SleepModeOFF(void);


void SYS_Init(void);
float batteryPercentageToVoltage(int percentage);
int batteryVoltageToPercentage(float voltage);
void PeripheralInit(void);
void ProcessPeripheral(void);
void EnableVat(void);
void RFIDRcv(const char *data, uint32_t length);
void CheckGPSAlerts(void);
s32 SendATCommandSimple(char *atCmd, char *responseBuf, u32 maxLen, u32 timeout);
uint8_t SendCFUNAT(uint8_t mode, uint8_t reset);
void HardwareThreadEntry(s32 taskId);
void hardware_thread_init(u32 taskId);

// Battery Functions
uint8_t GetBatteryPercentage(float voltage);

// RS232 System Status Functions
void SendRS232String(const char* message);
void SystemInfoSend(void);
void CellTowerInfoSend(void);
void PeripheralInfoSend(void);
void GPSDataSend(void);

// RS232 Response Buffering Functions
void QueueRS232Response(const char* response);
void SendBufferedRS232Response(void);

#endif

