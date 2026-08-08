#include "SOS.h"
#include "VTS.h"
#include "Hardware.h"
#include "SMS.h"
#include "TCP.h"
#include "Alert.h"
#if defined(PROTO_CDAC)
#include "HTTP.h"
#endif
#include <stdlib.h>


#define		PUSH_MIN_DELAY				2
#define		PUSH_MAX_DELAY				20

#define		SOS_TAMPER_COUNT			21


volatile SOSTypeDefStruct SOS={0};

uint16_t LedCount=0;

void SetSOSTimeOutSeconds(uint16_t seconds)
{
	SOS.SOSTimeOut=seconds;
	VTSData.IntervalData.SOSTimeOut=seconds;
}







void SOSInit(uint16_t timeOut)
{
	memset((void*)&SOS, 0, sizeof(SOSTypeDefStruct));
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
	/* For NC circuit: pin=LOW at rest means the button is correctly connected
	 * and at rest — this is the normal idle state, so RequireRelease must be 0
	 * on boot.  Pin=HIGH at boot with NC means the button is already active
	 * (disconnected or held), which we WANT to detect; do not latch it out.
	 * For NO circuit: pin=LOW at boot means the button was pressed during
	 * power-on (e.g., harness short); latch RequireRelease until pin goes HIGH. */
#ifdef SOS_NC_CIRCUIT
	SOS.RequireRelease = 0;
	if (INPUT_SOS_VAL == SOS_ACTIVE_LEVEL)
		LOGData(TAG_SOS, "SOS NC: pin active at boot (disconnected or pressed); tamper detection armed");
	else
		LOGData(TAG_SOS, "SOS NC: pin idle at boot (button connected and at rest)");
#else
	SOS.RequireRelease = (INPUT_SOS_VAL == SOS_ACTIVE_LEVEL) ? 1 : 0;
	if (SOS.RequireRelease)
		LOGData(TAG_SOS, "SOS NO: input active at boot; waiting for button release before arming");
#endif
	SetSOSTimeOutSeconds(VTSData.IntervalData.SOSTimeOut);
	SLED_OFF;
}

void ResetSOS(void)
{
	if(SOS.IsSOS)
	{
		SOS.IsSOS = 0;
		SOS.SOSTimeLasped = 0;
		SOS.SOSPushCount = 0;
		SOS.RequireRelease = 1; // Require pin release before next trigger
		SLED_OFF;
		AddAlert(SOS_OFF_ALERT);
		RemoveAlert(SOS_ON_ALERT);
		IsPacketReady.IsCriticalPacket = 1;
		VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
		#if defined(PROTO_CDAC)
		SMSAlert(11);
		#endif
	}
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

#if defined(PROTO_CDAC)
#ifdef ENABLE_UNIFIED_FIRMWARE
void ProcessSOSCDAC(void)
#else
void ProcessSOS(void)
#endif
{
	uint16_t pinValue;
	
	if (VTSData.DisableSOS)
	{
		if (SOS.IsSOS || SOS.IsSOSTamper)
		{
			SOS.IsSOS = 0;
			SOS.IsSOSTamper = 0;
			SLED_OFF;
			LOGData(TAG_SOS, "DisableSOS active, clearing active CDAC SOS/Tamper Alert");
			AddAlert(SOS_OFF_ALERT);
			RemoveAlert(SOS_ON_ALERT);
			RemoveAlert(SOS_TMP_ALERT);
			IsPacketReady.IsCriticalPacket = 1;
		}
		SOS.SOSPushCount = 0;
		return;
	}
	
	pinValue = INPUT_SOS_VAL;
	/* SOS_ACTIVE_LEVEL = button pressed (NO: 0) or pressed/disconnected (NC: 1)
	 * SOS_IDLE_LEVEL   = button at rest  (NO: 1) or connected+resting    (NC: 0)
	 * With NC wiring, a disconnected SOS wire holds the pin at SOS_ACTIVE_LEVEL
	 * permanently, so the tamper threshold is crossed exactly like a stuck button. */
	if(pinValue == SOS_ACTIVE_LEVEL)
	{
		/* Count always increments while button is held — moved outside the
		 * RequireRelease gate.  Previously, SOS ON set RequireRelease=1 and
		 * reset count=0, so the tamper check (count > SOS_TAMPER_COUNT) could
		 * never fire because counting stopped at 0.  Counting unconditionally
		 * lets a prolonged hold accumulate past the tamper threshold even after
		 * SOS has already fired.  With NC circuit, also counts when wire is cut. */
		SOS.SOSPushCount++;

		/* Tamper: button held longer than SOS_TAMPER_COUNT ticks (~2.1s) or wire cut.
		 * If SOS ON was activated during count, clear SOS ON so removal only sends Alert 16. */
		if(SOS.SOSPushCount > SOS_TAMPER_COUNT && !SOS.IsSOSTamper)
		{
			SOS.SOSPushCount=0;
			LOGData(TAG_SOS,"********************SOS TAMPERED / WIRECUT!! (Code 02) ***********************");
			SOS.IsSOSTamper=1;
			SOS.RequireRelease=1; // Latch until button returns to idle level to prevent false triggers
			VAlert[SOS_TMP_ALERT].Enable=1;
			AddAlert(SOS_TMP_ALERT);
			IsPacketReady.IsCriticalPacket=1;
			SMSAlert(16);
			return;
		}
	}
	// Button at rest / NC path connected (idle level)
	if(pinValue == SOS_IDLE_LEVEL)
	{
		if(SOS.SOSPushCount > 0)
		{
			if(!SOS.RequireRelease && !SOS.IsSOSTamper)
			{
				// Second press while SOS active → Emergency OFF (Code 11)
				if(SOS.SOSPushCount >= PUSH_MIN_DELAY && SOS.IsSOS)
				{
					LOGData(TAG_SOS,"********************SOS ALERT OFF (Code 11) via button ***********************");
					ResetSOS();
				}
				// First press → Emergency ON (Code 01)
				else if(SOS.SOSPushCount >= PUSH_MIN_DELAY && !SOS.IsSOS)
				{
					LOGData(TAG_SOS,"********************SOS ALERT ON (Code 01) ***********************");
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
		}
		
		SOS.RequireRelease=0; // Button/wire returned to idle — ready for next trigger
		if(SOS.IsSOSTamper)
		{
			SOS.IsSOSTamper=0;
			LOGData(TAG_SOS, "SOS Tamper Alert OFF (Wire Reconnected)");
			RemoveAlert(SOS_TMP_ALERT);
			IsPacketReady.IsCriticalPacket = 1;
		}
	}
}


#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || !defined(PROTO_CDAC)
#ifdef ENABLE_UNIFIED_FIRMWARE
void ProcessSOSStd(void)
#else
void ProcessSOS(void)
#endif
{
	int pinValue=0;
	
	if (VTSData.DisableSOS)
	{
		if (SOS.IsSOS || SOS.IsSOSTamper)
		{
			SOS.IsSOS = 0;
			SOS.IsSOSTamper = 0;
			SLED_OFF;
			LOGData(TAG_SOS, "DisableSOS active, clearing active SOS/Tamper Alert");
			AddAlert(SOS_OFF_ALERT);
			RemoveAlert(SOS_ON_ALERT);
			RemoveAlert(SOS_TMP_ALERT);
			IsPacketReady.IsCriticalPacket = 1;
			VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
		}
		SOS.SOSPushCount = 0;
		return;
	}
	
	pinValue = INPUT_SOS_VAL;
	/* See ProcessSOSCDAC for full explanation of SOS_ACTIVE_LEVEL / SOS_IDLE_LEVEL.
	 * With SOS_NC_CIRCUIT defined, a disconnected wire = permanent ACTIVE level
	 * → SOSPushCount crosses SOS_TAMPER_COUNT → tamper fires without hardware change. */
	if(pinValue == SOS_ACTIVE_LEVEL)
	{
		/* Same fix as ProcessSOSCDAC: count unconditionally so that a prolonged
		 * hold reaches SOS_TAMPER_COUNT even after SOS has already fired and
		 * set RequireRelease=1.  With NC circuit also counts when wire is cut. */
		SOS.SOSPushCount++;

		if(SOS.SOSPushCount > SOS_TAMPER_COUNT && !SOS.IsSOSTamper)
		{
			SOS.SOSPushCount=0;
			LOGData(TAG_SOS,"********************\nSOS Temper Alert ON********************\n");
			SOS.IsSOSTamper=1;
			SOS.RequireRelease=1; // Latch until button returns to idle level to prevent false triggers
			AddAlert(SOS_TMP_ALERT);
			IsPacketReady.IsCriticalPacket=1;
			return;
		}
	}
	// Button at rest / NC path connected (idle level)
	if(pinValue == SOS_IDLE_LEVEL)
	{
		if(SOS.SOSPushCount > 0)
		{
			if(!SOS.RequireRelease && !SOS.IsSOSTamper)
			{
				if((SOS.SOSPushCount >= PUSH_MIN_DELAY) && !SOS.IsSOS)
				{
					SOS.IsSOS=1;
					uint8_t isServer2Disabled = (VTSData.ServerData.IP2[0] == 'N' && VTSData.ServerData.IP2[1] == 'A');
					uint8_t isEmergencyConnected = isServer2Disabled ?
						(ServerSocket[0].SocketState == SOCKET_CONNECTED) :
						(ServerSocket[1].SocketState == SOCKET_CONNECTED);

					if (!isEmergencyConnected) {
						SendSOSSMS(1);
					}

					SOS.SOSTimeLasped=0;
					VAlert[SOS_ON_ALERT].Enable=1;
					SLED_ON;

					LOGData(TAG_SOS,"********************\nSOS Alert ON\n***********************\n");
					AddAlert(SOS_ON_ALERT);
					VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.SOSInterval;
				}
			}
			SOS.SOSPushCount=0;
		}
		
		SOS.RequireRelease=0; // Button/wire returned to idle — ready for next trigger
		if(SOS.IsSOSTamper)
		{
			SOS.IsSOSTamper=0;
			LOGData(TAG_SOS,"********************\nSOS Temper Alert OFF********************\n");
			RemoveAlert(SOS_TMP_ALERT);
			IsPacketReady.IsCriticalPacket = 1;
		}
	}
}
#endif

#ifdef ENABLE_UNIFIED_FIRMWARE
void ProcessSOS(void)
{
	ProcessSOSStd();
}
#endif
