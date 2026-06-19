
#include "Alert.h"
#include <string.h>



AlertTypedef VAlert[ALERT_COUNT];

StoreAlertTypedef StoredAlert;

#ifndef PROTO_CDAC
char alertACK[MAX_ACK_SIZE];
#endif

#ifdef PROTO_CDAC

void AlertInitStruct(void)			//AlertInitStruct
{
	uint16_t i;
	
	StoredAlert.TotalAlert=0;
	StoredAlert.AlertPosition=0;
	for(i=0;i<8;i++)
	{
		StoredAlert.Alerts[i]=0xFF;
	}
		
	for(i=0;i<ALERT_COUNT;i++)
	{
		VAlert[i].Enable=0;
		memset(VAlert[i].ACK,0x00,17);
		strcpy(VAlert[i].ID,VTAlertPKT[i]);
		VAlert[i].ContMode = VTContType[i];
		// Valid CDAC packet headers: EPB=Emergency, CRT=Critical, ALT=Alert, ACK=OTA Param Ack (spec 6.6)
		switch(VTAlertHeaderType[i]){
			case 0:
			strcpy(VAlert[i].Header,"EPB");
			break;
			case 1:
			strcpy(VAlert[i].Header,"CRT");
			break;
			case 2:
			strcpy(VAlert[i].Header,"ALT");
			break;
			case 3:
			strcpy(VAlert[i].Header,"ACK");
			break;
			default:
			strcpy(VAlert[i].Header,"INV");
			break;
		}
	}
	
	
}

void AddAlert(uint8_t alert)
{
	if(alert==0xff)
		return;
	for(int i=0;i<8;i++)
	{
		if(StoredAlert.Alerts[i]==alert)
			return;
	}
	VAlert[alert].Index=StoredAlert.TotalAlert;
	VAlert[alert].Enable=1;
	VAlert[alert].AlertSent=0;
	// StoredAlert.Alerts[StoredAlert.TotalAlert++]=alert;
	if(alert < 13)
	{
		IsPacketReady.IsCriticalPacket=1;
		IntervalTick.CriticalTick=0;
	}
	else
	{
		IsPacketReady.IsNormalPacket=1;
	}
	nwy_dbg_log("alert %d added",alert);
}

void RemoveAlert(uint8_t alert)
{
	//uint16_t i,fn=0,rm=0;
	VAlert[alert].Enable=0;
	VAlert[alert].WithACK=0;
	if(strlen(VAlert[alert].ACK)>0)
		memset(VAlert[alert].ACK,0x00,10);
	
	// for(i=0;i<StoredAlert.TotalAlert;i++)
	// {
	// 	if(StoredAlert.Alerts[i]==alert)
	// 	{
	// 		fn=i;
	// 		rm=1;
	// 		break;
	// 	}
	// }
	
	// if((StoredAlert.TotalAlert) && (rm))
	// {
	// 	for(i=fn;i<StoredAlert.TotalAlert;i++)
	// 	{
	// 		StoredAlert.Alerts[i]=StoredAlert.Alerts[i+1];
	// 	}
	// 	StoredAlert.Alerts[StoredAlert.TotalAlert]=0xFF;
	// 	StoredAlert.TotalAlert--;
	// }
	nwy_dbg_log("alert %d removed",alert);
}

void RemoveNonRepeatAlert(uint8_t pos)
{
	switch(pos)
	{
		case MAINS_RES_ALERT:
			RemoveAlert(MAINS_RES_ALERT);
			break;
		case BATT_LOW_ALERT:
			RemoveAlert(BATT_LOW_ALERT);
			break;
		case BATT_LOW_RES_ALERT:
			RemoveAlert(BATT_LOW_RES_ALERT);
			break;
		case CONF_CHANGE_ALERT:
			RemoveAlert(CONF_CHANGE_ALERT);
			break;
		case GFIN_ALERT:
			RemoveAlert(GFIN_ALERT);
			break;
		case GFOUT_ALERT:
			RemoveAlert(GFOUT_ALERT);
			break;
		case SOS_OFF_ALERT:
			RemoveAlert(SOS_OFF_ALERT);
			break;
		case TAMPER_ALERT:
			RemoveAlert(TAMPER_ALERT);
			break;
			///
		case HARSH_ACC_ALERT:
			RemoveAlert(HARSH_ACC_ALERT);
			break;
		case HARSH_BRK_ALERT:
			RemoveAlert(HARSH_BRK_ALERT);
			break;
		case RASH_TURN_ALERT:
			RemoveAlert(RASH_TURN_ALERT);
			break;
		case IMPACT_ALERT:
			RemoveAlert(IMPACT_ALERT);
			break;
	}
}
#else
void AlertInitStruct(void)			//AlertInitStruct
{
	uint16_t i;
	const char VTAlertPKT[19][3] ={"10","11","16","03","22","09","17","13","14","15","23","20","21","18","19","12","06","04","05"};
		
	StoredAlert.TotalAlert=0;
	StoredAlert.AlertPosition=0;
	for(i=0;i<8;i++)
	{
		StoredAlert.Alerts[i]=0xFF;
	}
		
	for(i=0;i<ALERT_COUNT;i++)
	{
		VAlert[i].Enable=0;
		VAlert[i].Index=i;
		memset(alertACK,0x00,MAX_ACK_SIZE);
		strcpy(VAlert[i].ID,VTAlertPKT[i]);
		if(i<2)
		{
			strcpy(VAlert[i].Header,"EPB");
		}
		else if((i > 1) && (i<13))
		{
			if(i==TAMPER_ALERT)
				strcpy(VAlert[i].Header,"ALT");
			else
				strcpy(VAlert[i].Header,"CRT");
			
		}
		else
		{
			strcpy(VAlert[i].Header,"ALT");
		}
	}
	// ACK header for OTA Parameter Acknowledgment per CDAC spec 6.6
	strcpy(VAlert[CONF_CHANGE_ALERT].Header,"ACK");
}

void AddAlert(uint8_t alert)
{
	if(alert==0xff)
		return;
	for(int i=0;i<8;i++)
	{
		if(StoredAlert.Alerts[i]==alert)
			return;
	}
//	VAlert[alert].Index=StoredAlert.TotalAlert;
	VAlert[alert].Enable=1;
	if(!VAlert[alert].IsSMS)
	{
//		VAlert[alert].SMSCounter=__LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetMinute(RTC));
//		if(MessageAlertForContact(&VAlert[alert]))
//		{
//		//	SendSMS("9212685587",SMS.Msg);
//			VAlert[alert].IsSMS=1;
//		}
	}
//	StoredAlert.Alerts[StoredAlert.TotalAlert++]=alert;
//	IsPacketReady.IsCriticalPacket=1;
//	IntervalTick.CriticalTick=20;
}

void RemoveAlert(uint8_t alert)
{
	uint16_t i,fn=0,rm=0;
	VAlert[alert].Enable=0;
	VAlert[alert].WithACK=0;
	//VAlert[alert].IsSMS=0;
	//VAlert[alert].SMSCounter=0;
	for(i=0;i<StoredAlert.TotalAlert;i++)
	{
		if(StoredAlert.Alerts[i]==alert)
		{
			fn=i;
			rm=1;
			break;
		}
	}
	for(i=fn;i<StoredAlert.TotalAlert;i++)
	{
		StoredAlert.Alerts[i]=StoredAlert.Alerts[i+1];
	}
	StoredAlert.Alerts[StoredAlert.TotalAlert]=0xFF;
	if((StoredAlert.TotalAlert) && (rm))
		StoredAlert.TotalAlert--;
	
}

void RemoveNonRepeatAlert(uint8_t pos)
{
	switch(pos)
	{
		case MAINS_RES_ALERT:
			RemoveAlert(MAINS_RES_ALERT);
			break;
		case BATT_LOW_ALERT:
			RemoveAlert(BATT_LOW_ALERT);
			break;
		case BATT_LOW_RES_ALERT:
			RemoveAlert(BATT_LOW_RES_ALERT);
			break;
		case CONF_CHANGE_ALERT:
			RemoveAlert(CONF_CHANGE_ALERT);
			break;
		case GFIN_ALERT:
			RemoveAlert(GFIN_ALERT);
			break;
		case GFOUT_ALERT:
			RemoveAlert(GFOUT_ALERT);
			break;
		case SOS_OFF_ALERT:
			RemoveAlert(SOS_OFF_ALERT);
			break;
		case TAMPER_ALERT:
			RemoveAlert(TAMPER_ALERT);
			break;
	}
}
#endif

