
#include "SOS.h"
#include "VTS.h"
#include "Alert.h"
#include "GPRS.h"
#include <stdlib.h>


#define		PUSH_MIN_DELAY				2
#define		PUSH_MAX_DELAY				20

#define		SOS_TAMPER_COUNT			21


SOSTypeDefStruct SOS;

uint16_t LedCount;


void BlinkLED(void);





void SOSInit(uint16_t timeOut)
{
	//GPIO_SetMode(SOS_PORT, SOS_PIN, GPIO_MODE_INPUT);	
	//GPIO_SetMode(SOS_LED_PORT, SOS_LED_PIN, GPIO_MODE_OUTPUT);	
    nwy_gpio_set_direction(SOS_PIN,nwy_input);
	SOS.IsSOS=0;
	SOS.IsSOSTamper=0;
	SOS.IsSOSSMS=0;
	SOS.SOSPushCount=0;
	SOS.SOSTimeOut=VTSData.IntervalData.SOSTimeOut;
	SLED_OFF;
}

void ResetSOS(void)
{
	if(SOS.IsSOS)
		SOS.SOSTimeLasped=SOS.SOSTimeOut;
}

void UpdateSOSTimeOut(char* value)
{
	uint16_t j=atoi(value);
	if((j >1) && (j < 400))
	{
		SOS.SOSTimeOut=j*60;
		VTSData.IntervalData.SOSTimeOut=SOS.SOSTimeOut;
	}
}

#ifdef PROTO_CDAC
void ProcessSOS(void)
{
	uint16_t pinValue;
	
	pinValue=SOS_STATE;
	if(pinValue==1)
	{
		SOS.SOSPushCount++;
		if(SOS.SOSPushCount > SOS_TAMPER_COUNT)
		{
			SOS.SOSPushCount=0;
			if(!SOS.IsSOSTamper)
			{
				nwy_dbg_log("********************SOS TAMPERED!! ***********************");
				SOS.IsSOSTamper=1;
				VAlert[SOS_TMP_ALERT].Enable=1;
				AddAlert(SOS_TMP_ALERT);
				// CDAC spec 4.iii req 6 & 7: Server thread handles SMS
				// Sets flag, Server thread sends after packet or immediately if no GPRS
				SOS.IsSOSSMS = 16;
			//	VTSData.IntervalData.CurrentInterval=VTSData.IntervalData.EnergencyInterval;
			}
			return;
		}		
	}
	if(!pinValue)
	{
		if((SOS.SOSPushCount >= PUSH_MIN_DELAY) && (SOS.SOSPushCount < PUSH_MAX_DELAY))
		{
			if((SOS.IsSOS==0) && (!SOS.IsSOSTamper))
			{
				nwy_dbg_log("********************SOS ALERT ON ***********************");
				SOS.IsSOS=1;
				HTTPConnectFlag=1;
				VAlert[SOS_ON_ALERT].Enable=1;
				SOS.SOSTimeLasped=0;
				SLED_ON;
				AddAlert(SOS_ON_ALERT);
				IsPacketReady.IsCriticalPacket=1;
				// CDAC spec 4.iii req 6 & 7: Server thread handles SMS
				// Sets flag, Server thread sends after packet or immediately if no GPRS
				SOS.IsSOSSMS = 10;
			}
			
		}
		SOS.SOSPushCount=0;
		if(SOS.IsSOSTamper)
		{
			SOS.IsSOSTamper=0;
			RemoveAlert(SOS_TMP_ALERT);
		}
	}
	//    Clear SOS after given time 	
	if(SOS.IsSOS)
	{
		SOS.SOSTimeLasped++;
		if(SOS.SOSTimeLasped >= SOS.SOSTimeOut)
		{
			nwy_dbg_log("********************SOS ALERT OFF ***********************");
			SOS.SOSTimeLasped=0;
			SOS.IsSOS=0;
			SLED_OFF;
			AddAlert(SOS_OFF_ALERT);
			RemoveAlert(SOS_ON_ALERT);
			// CDAC spec 4.iii req 6 & 7: Server thread handles SMS
			// Sets flag, Server thread sends after packet or immediately if no GPRS
			SOS.IsSOSSMS = 11;
		}
	}
}
	

#else
void ProcessSOS(void)
{
	uint16_t pinValue;
	
	pinValue=SOS_STATE;
	//nwy_dbg_log("SOS Value : %d",pinValue);
	if(pinValue==1)
	{
		SOS.SOSPushCount++;
		if(SOS.SOSPushCount > SOS_TAMPER_COUNT)
		{
			SOS.SOSPushCount=0;
			
			if(!SOS.IsSOSTamper)
			{
				if(SOS.IsSOS)
				{
					SOS.IsSOS=0;
					SLED_OFF;
					nwy_dbg_log("********************\nClear SOS Alert if ON********************\n");
					AddAlert(SOS_OFF_ALERT);
					RemoveAlert(SOS_ON_ALERT);
					VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
				}

				nwy_dbg_log("********************\nSOS Temper Alert ON********************\n");
				SOS.IsSOSTamper=1;
				AddAlert(SOS_TMP_ALERT);
				//SLED_ON;
				
			}
			return;
		}		
	}
	if(!pinValue)
	{
		if((SOS.SOSPushCount >= PUSH_MIN_DELAY) && (SOS.SOSPushCount < PUSH_MAX_DELAY))
		{
			if((SOS.IsSOS==0) && (!SOS.IsSOSTamper))
			{
				if(IsSleepMode)
				{
            		SleepModeON();
					nwy_sleep(5000);
				}
				SOS.IsSOS=1;
				if(ServerSocket[1].SocketState != SOCKET_CONNECTED)
					SendSOSSMS(1);

				SLED_ON;
				SOS.SOSTimeLasped=0;
				VAlert[SOS_ON_ALERT].Enable=1;
//				if(!LL_GPIO_IsOutputPinSet(SOS_LED_PORT,SOS_LED_PIN))
//					SLED_ON;
				nwy_dbg_log("********************\nSOS Alert ON\n***********************\n");
				AddAlert(SOS_ON_ALERT);
				VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.SOSInterval;
			}
		}
		SOS.SOSPushCount=0;
		if(SOS.IsSOSTamper)
		{
			SOS.IsSOSTamper=0;
			nwy_dbg_log("********************\nSOS Temper Alert OFF********************\n");
			RemoveAlert(SOS_TMP_ALERT);
			//SLED_OFF;
		}
		
	}	
	//    Clear SOS after given time regardless of pin state
	if(SOS.IsSOS)
	{
		BlinkLED();
		SOS.SOSTimeLasped++;
		if(SOS.SOSTimeLasped >= SOS.SOSTimeOut)
		{
			SOS.SOSTimeLasped=0;
			SOS.IsSOS=0;
			SLED_OFF;
//				GPO1_OFF;
			nwy_dbg_log("********************\nSOS Alert OFF********************\n");
			AddAlert(SOS_OFF_ALERT);
			RemoveAlert(SOS_ON_ALERT);
			VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
		}
	}
}
#endif

void BlinkLED(void)
{
	if(!SOS.IsSOS)
		return;
//	if(LL_GPIO_IsOutputPinSet(SOS_LED_PORT,SOS_LED_PIN))
//	{
//		if(LedCount > LED_ON_TIME)
//		{
//			SLED_OFF;
//			LedCount=0;
//		}
//	}
//	else
//	{
//		if(LedCount > LED_OFF_TIME)
//		{
//			SLED_ON;
//			LedCount=0;
//		}
//	}
//	LedCount++;
}

