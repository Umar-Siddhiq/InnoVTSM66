
#include "SOS.h"
#include "VTS.h"
#include "Hardware.h"
#include "SMS.h"
#include "TCP.h"
#include "Alert.h"
#ifdef PROTO_CDAC
#include "HTTP.h"
#endif
#include <stdlib.h>


#define		PUSH_MIN_DELAY				2
#define		PUSH_MAX_DELAY				20

#define		SOS_TAMPER_COUNT			21


SOSTypeDefStruct SOS={0};

uint16_t LedCount=0;

void SetSOSTimeOutSeconds(uint16_t seconds)
{
	SOS.SOSTimeOut=seconds;
	VTSData.IntervalData.SOSTimeOut=seconds;
}







void SOSInit(uint16_t timeOut)
{
	memset(&SOS, 0, sizeof(SOSTypeDefStruct));
	s32 ret = INPUT_SOS_INIT;
	if (QL_RET_OK!= ret)
	{
		LOGData(TAG_SOS,"SOS Input Pin Init Failed, ret=%d", ret);
	}
	else
	{
		LOGData(TAG_SOS,"SOS Input Pin Init Success");
	}
	SOS.IsSOS=0;
	SOS.IsSOSTamper=0;
	SOS.IsSOSSMS=0;
	SOS.SOSPushCount=0;
	SetSOSTimeOutSeconds(VTSData.IntervalData.SOSTimeOut);
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
		uint32_t timeoutSeconds=(uint32_t)j*60u;
		if(timeoutSeconds <= UINT16_MAX)
		{
			SetSOSTimeOutSeconds((uint16_t)timeoutSeconds);
		}
	}
}

#ifdef PROTO_CDAC
void ProcessSOS(void)
{
	if(VTSData.DisableSOS == 1)
	{
		if(SOS.IsSOS || SOS.IsSOSTamper)
		{
			SOS.IsSOS = 0;
			SOS.IsSOSTamper = 0;
			SOS.SOSPushCount = 0;
			SOS.SOSTimeLasped = 0;
			SLED_OFF;
			AddAlert(SOS_OFF_ALERT);
			RemoveAlert(SOS_ON_ALERT);
			RemoveAlert(SOS_TMP_ALERT);
			LOGData(TAG_SOS, "SOS disabled by config. Cleared active SOS/Tamper alerts.");
		}
		return;
	}
	uint16_t pinValue;
	
	pinValue=INPUT_SOS_VAL;
	if(pinValue==1)
	{
		SOS.SOSPushCount++;
		if(SOS.SOSPushCount > SOS_TAMPER_COUNT)
		{
			SOS.SOSPushCount=0;
			if(!SOS.IsSOSTamper)
			{
				LOGData(TAG_SOS,"********************SOS TAMPERED!! ***********************");
				SOS.IsSOSTamper=1;
				VAlert[SOS_TMP_ALERT].Enable=1;
				AddAlert(SOS_TMP_ALERT);
				SMSAlert(16);
			//	VTSData.IntervalData.CurrentInterval=VTSData.IntervalData.EnergencyInterval;
			}
			return;
		}		
	}
	if(!pinValue)
	{
		if((SOS.SOSPushCount > PUSH_MIN_DELAY) && (SOS.SOSPushCount < PUSH_MAX_DELAY))
		{
			if((SOS.IsSOS==0) && (!SOS.IsSOSTamper))
			{
				LOGData(TAG_SOS,"********************SOS ALERT ON ***********************");
				SOS.IsSOS=1;
				HTTPConnectFlag=1;
				VAlert[SOS_ON_ALERT].Enable=1;
				SOS.SOSTimeLasped=0;
				SLED_ON;
				AddAlert(SOS_ON_ALERT);
				IsPacketReady.IsCriticalPacket=1;
				SMSAlert(10);
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
			LOGData(TAG_SOS,"********************SOS ALERT OFF ***********************");
			SOS.SOSTimeLasped=0;
			SOS.IsSOS=0;
			SLED_OFF;
			AddAlert(SOS_OFF_ALERT);
			RemoveAlert(SOS_ON_ALERT);
			SMSAlert(11);
		}
	}
}
	

#else
void ProcessSOS(void)
{
	if(VTSData.DisableSOS == 1)
	{
		if(SOS.IsSOS || SOS.IsSOSTamper)
		{
			SOS.IsSOS = 0;
			SOS.IsSOSTamper = 0;
			SOS.SOSPushCount = 0;
			SOS.SOSTimeLasped = 0;
			SLED_OFF;
			AddAlert(SOS_OFF_ALERT);
			RemoveAlert(SOS_ON_ALERT);
			RemoveAlert(SOS_TMP_ALERT);
			VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
			LOGData(TAG_SOS, "SOS disabled by config. Cleared active SOS/Tamper alerts.");
		}
		return;
	}
	int pinValue=0;
	
	pinValue=INPUT_SOS_VAL;
	LOGData(TAG_SOS,"SOS Value : %d",pinValue);
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
					LOGData(TAG_SOS,"********************\nClear SOS Alert if ON********************\n");
					AddAlert(SOS_OFF_ALERT);
					RemoveAlert(SOS_ON_ALERT);
					VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
				}

				LOGData(TAG_SOS,"********************\nSOS Temper Alert ON********************\n");
				SOS.IsSOSTamper=1;
				AddAlert(SOS_TMP_ALERT);
				
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
				
				SOS.IsSOS=1;
				SLED_ON;
				/* OLD CODE - COMMENTED OUT AS REQUESTED:
				if(ServerSocket[1].SocketState != SOCKET_CONNECTED)
					SendSOSSMS(1);
				*/
				// NEW CODE: Dynamic SMS fallback based on Server 2 enabled state
				uint8_t isServer2Disabled = (VTSData.ServerData.IP2[0] == 'N' && VTSData.ServerData.IP2[1] == 'A');
				uint8_t isEmergencyConnected = isServer2Disabled ? 
					(ServerSocket[0].SocketState == SOCKET_CONNECTED) : 
					(ServerSocket[1].SocketState == SOCKET_CONNECTED);

				if (!isEmergencyConnected) {
					SendSOSSMS(1);
				}

				
				SOS.SOSTimeLasped=0;
				VAlert[SOS_ON_ALERT].Enable=1;

				LOGData(TAG_SOS,"********************\nSOS Alert ON\n***********************\n");
				AddAlert(SOS_ON_ALERT);
				VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.SOSInterval;
			}
		}
		SOS.SOSPushCount=0;
		if(SOS.IsSOSTamper)
		{
			SOS.IsSOSTamper=0;
			LOGData(TAG_SOS,"********************\nSOS Temper Alert OFF********************\n");
			RemoveAlert(SOS_TMP_ALERT);
		}
	}
	
	//    Clear SOS after given time - This must run regardless of pin state
	if(SOS.IsSOS)
	{
		SOS.SOSTimeLasped++;
		if(SOS.SOSTimeLasped >= SOS.SOSTimeOut)
		{
			SOS.SOSTimeLasped=0;
			SOS.IsSOS=0;
			SLED_OFF;
			LOGData(TAG_SOS,"********************\nSOS Alert OFF********************\n");
			AddAlert(SOS_OFF_ALERT);
			RemoveAlert(SOS_ON_ALERT);
			VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
		}
	}	
	
}
#endif



