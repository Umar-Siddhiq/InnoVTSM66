#include "SMS.h"
#include <stdlib.h>
#include <string.h>
#include "FTP.h"
#include "Geofence.h"
#include "Server.h"
#include "VTS.h"
#include "Sensors.h"
#ifdef PROTO_CDAC
#include "Batch.h"
#endif


//http://maps.google.com/maps?z=12&t=m&q=loc:38.9419+-78.3020

#define MAPSLINK "http://maps.google.com/maps?z=12&t=m&q=loc:"

nwy_sms_recv_info_type_t SMSRCV = {{0}};

char RcvIndex[30];
char SMSData[500];
char SMSSender[18];
char SimData[MSGSIZE];
uint8_t ACTMsg;

// ZigTestMode: Manufacturing test mode flag (0=disabled, 1=enabled)
uint8_t ZigTestMode = 0;

#ifdef PROTO_MAHARASHTRA1
uint8_t IsSRCMD;
#endif

#define CMDBUFFSIZE 250
char CMD_Buff[CMDBUFFSIZE];

extern uint8_t SendLogin1,SendLogin2;
uint16_t IsSMS;
extern uint32_t FrameNumber;

#ifdef PROTO_CDAC
extern char VehicleMovingMode;
#endif

extern void LoadDefault();
void SMS_rcvcb(void *data, size_t size)
{
    memset(RcvIndex,0,30);
    nwy_sms_recv_info_type_t sms_data = {{0}};
    memset(&sms_data, 0, sizeof(sms_data));
    nwy_sms_recv_message(&sms_data);
	nwy_dbg_log("******SsMS CALLBAK*******");
    if(sms_data.cnmi_mt == 1)
    {
        sprintf(RcvIndex,"%d",sms_data.nIndex);
        ReadSMS(RcvIndex);
    }
	else if (sms_data.cnmi_mt == 2)
	{

		strncpy(SMSData,sms_data.pData,50);
		IsSMS=1;	
	}

/*
	// memset(&sms_data, 0, sizeof(sms_data));
    // nwy_sms_recv_message(&sms_data);

    // nwy_dbg_log("nwy recv sms sms_data.cnmi_mt=%d\r\n", sms_data.cnmi_mt);
    // if (sms_data.cnmi_mt == 1) {
    //     nwy_dbg_log("nwy recv sms sms_data.nIndex=%d, mt=%d\r\n", sms_data.nIndex, sms_data.cnmi_mt);
    //     nwy_dbg_log("nwy recv sms sms_data.nStorageId=%d\r\n", sms_data.nStorageId);
    // } else if (sms_data.cnmi_mt == 2) {
    //     nwy_dbg_log("nwy recv sms oa len =%d,sms_data.oa=%s, dcs=%d, mt=%d, time %02d-%02d-%02d %02d:%02d:%02d+%d\r\n", \
    //         sms_data.oa_size, sms_data.oa, sms_data.dcs, sms_data.cnmi_mt,sms_data.scts.uYear, sms_data.scts.uMonth, sms_data.scts.uDay, \
    //         sms_data.scts.uHour, sms_data.scts.uMinute, sms_data.scts.uSecond, sms_data.scts.iZone);
    //     nwy_dbg_log("nwy recv sms pdata len =%d,sms_data.pdata=%s\r\n", sms_data.nDataLen, sms_data.pData);
    // } else {
    //     nwy_dbg_log("nwy recv sms sms_data.cnmi_mt=%d invalid\r\n", sms_data.cnmi_mt);
    // }

    // nwy_dbg_log("=SMS= end to test recv sms\n");
    */
}
#ifndef PROTO_CDAC
void SendSOSSMS(uint8_t isFall)
{
	nwy_dbg_log("SOS SMS Fallback triggered !!!!!!!!!!!!!");
	// char Link[200];
	// memset(SimData,0x00,MSGSIZE);
	// strcpy(SimData,"SOS Triggered\n");

	// sprintf(Link,"%s%s,%s",MAPSLINK,sLastitude,sLongitude);
	// strcat(SimData,Link);

	// SendSMS(VTSData.PhoneNumber.Mob0,SimData);
	// SendSMS(VTSData.PhoneNumber.Mob1,SimData);
	if(isFall)
		sprintf(SimData,"SOSFB,");
	else
		sprintf(SimData,"SOS,");

	StringAdd(SimData,"%s\nLat:%s,%c\nLng:%s,%c,fix:%d, Speed:%s\nCID:%s,LAC:%s\n",NetWork.IMEI,sLatitude,GPS.LatDir,sLongitude,GPS.LngDir,GPS.GPSFix,sSpeed,GSM.CellID,GSM.LAC);
	InsertCurrentDateTime(SimData,0);
	InsertChar(SimData,',');
	InsertCurrentDateTime(SimData,1);

	SendSMS(VTSData.PhoneNumber.Mob0,SimData);
	SendSMS(VTSData.PhoneNumber.Mob1,SimData);

}

void SendGeoData(uint8_t index ,uint8_t OTASource)
{
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	sprintf(dataBuffer,"$GFR,");

	if(VTSData.GeoLatLng[index].InOut!=0)
	{
		StringAdd(dataBuffer,"Geo[%d]: ",index);
		StringAdd(dataBuffer,"ID : %d,",VTSData.GeoLatLng[index].ID);
		StringAdd(dataBuffer,"Mask: %d,",VTSData.GeoLatLng[index].InOut);
		for(int j=0; j < 10;j++)
		{
			if(VTSData.GeoLatLng[index].Latitude[j] != 0)
			{
				StringAdd(dataBuffer,"LAT[%d]:%.6f,",j,VTSData.GeoLatLng[index].Latitude[j]);
				StringAdd(dataBuffer,"LNG[%d]:%.6f,",j,VTSData.GeoLatLng[index].Longitude[j]);
			}
		}
		dataBuffer[strlen(dataBuffer)-1] = '*';
	}
	else
		strcat(dataBuffer,"NC*");

	
	nwy_dbg_log("geo res : %s",dataBuffer);
	if(OTASource==OTA_SRC_SCK_1)
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
	else if(OTASource==OTA_SRC_SCK_2)
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
	#ifdef EXTENDED_IPS
	else if(OTASource==OTA_SRC_SCK_3)	
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
	#endif
}
#endif

void InitSMS(void)
{
	nwy_set_report_option(2,1,0,0,0); 
    DeleteAllSMS();
}

void SendSMS(char* ph,char* msg)
{
    nwy_sms_info_type_t sms_data = {{0}};
    int ret = 0;
    nwy_dbg_log("SMS Sending!");
    memcpy(sms_data.phone_num, ph, strlen(ph));
    sms_data.msg_context_len = strlen(msg);
    memcpy(sms_data.msg_contxet, msg, strlen(msg));
    sms_data.msg_format = (nwy_sms_mag_dcs_type_e)0;
    ret = nwy_sms_send_message(&sms_data);
    if(ret != NWY_SUCCESS)
        nwy_dbg_log("******************************\nSMS Sent Failed!, ret: %d,  ph: %s\n cntx:",ret,ph);
    else
        nwy_dbg_log("******************************\nSMS Sent Success, ph: %s\n cntx: ",ph);
    nwy_dbg_log("%s",msg);
    nwy_dbg_log("\n******************************\n");
}

void ReadSMS(char* num)
{
	nwy_sms_recv_info_type_t sms_data = {{0}};
    int ret = 0;
    unsigned int nb;
    if((num[1] > 0x39) || (num[1] < 0x30))
			num[1] = 0;

    nb = atoi(num);
    ret = nwy_sms_read_message(nb, &sms_data);
    if (NWY_SUCESS != ret)
    {
        nwy_dbg_log("nwy read sms fail!\r\n");
        DeleteAllSMS();
        return;
    }
    
    strcpy(SMSSender,sms_data.oa);
	if(strlen(SMSSender)>10);
		strcpy(SMSSender,sms_data.oa+2);
    strcpy(SMSData,sms_data.pData);
    nwy_dbg_log("******************************\nSMS RECEIVED!,  ph: %s\n cntx:",SMSSender);
    nwy_dbg_log("%s",SMSData);
    nwy_dbg_log("\n******************************\n");
	IsSMS=1;
    //DecodeSMS(SMSData,0);
    //DeleteSMS(num);
}

void DeleteSMS(char* msgNum)
{
    int ret;
    int num = atoi(msgNum);

    ret = nwy_sms_delete_message(num);
    if (NWY_SUCESS != ret)
        nwy_dbg_log("nwy del sms fail!\r\n");
    else
        nwy_dbg_log("nwy del sms success!\r\n");
}


void DeleteAllReadSMS(void)
{	
   nwy_sms_delete_message_by_type(1);
}

void DeleteAllSMS(void)
{	
    nwy_sms_delete_message_by_type(4);
}


int GetValueFromData(char* data, char* cmd,char delim1, int delimPos, char delim2, char* value)
{
	char* fn;
	char* ls;
	int ln;
	ln=strlen(cmd);
	fn=strstr(data,cmd);
	if(fn)
	{
		fn=fn+ln;
		fn++;
		if(delimPos==0)
		{
			ls=strchr(fn,delim2);
			if(ls)
			{
				ln=ls-fn;
				strncpy(value,fn,ln);
				value[ln]=0;
				// Strip trailing \r and \n characters
				while(ln > 0 && (value[ln-1] == '\r' || value[ln-1] == '\n')) {
					value[ln-1] = '\0';
					ln--;
				}
				return 1;
			}
		}
		while(delimPos)
		{
			delimPos--;
			ls=strchr(fn,delim1);
			fn=ls+1;
		}
		if(ls)
		{
			ls++;
			fn=strchr(ls,delim2);
			ln=fn-ls;
			if((ln > 0) && (ln < 60))
			{
				memset(value,0x00,ln+1);
				strncpy(value,ls,ln);
				// Strip trailing \r and \n characters
				while(ln > 0 && (value[ln-1] == '\r' || value[ln-1] == '\n')) {
					value[ln-1] = '\0';
					ln--;
				}
				return 1;
			}
		}
	}
	return 0;
}
extern void FOTAStart(void);
void LoadDefaultFOTAParams(char *Sender, uint8_t OTASource)
{
	memset((void*)&DownloadReq,0x00,sizeof(download_req_info_s));
	strcpy(DownloadReq.IP,"124.123.18.16");
	strcpy(DownloadReq.User,"CCMSV1");
	strcpy(DownloadReq.Pass,"CCMS@123");
	strcpy(DownloadReq.Sender,Sender);
	strcpy(DownloadReq.FilePath,"app_fota.bin");
	strcpy(DownloadReq.InternalFilePath,SERVER_FOTA_FILEPATH);
	DownloadReq.Port = 21;
	DownloadReq.IsServer = OTASource;
	DownloadReq.AttemptCount=3;
	DownloadReq.RequestType=FTP_REQ_TYPE_FOTA;
	DownloadReq.IsValid = FOTA_REQ_VALID_CODE;
	UpdateFTPConfigInFlash(&DownloadReq);
	SendResponce(Sender,"Attemping FTP for FOTA...",OTASource,1);
	FTPStart(&DownloadReq);
	nwy_sleep(3000);
	//nwy_power_off(2);
}

uint8_t ParseFOTAPacket(char *buf,char *IP, uint16_t *port, char *User, char* pass, char* Filepath)
{
	char *ln, *fn;

	fn = strtok(buf," ");
	if(!fn)
		return 0;
	ln = strtok(NULL,",");
	if(!ln)
		return 0;
	strcpy(IP,ln);
	ln = strtok(NULL,",");
	if(!ln)
		return 0;
	*port = atoi(ln);
	ln = strtok(NULL,",");
	if(!ln)
		return 0;
	strcpy(User,ln);
	ln = strtok(NULL,",");
	if(!ln)
		return 0;
	strcpy(pass,ln);
	ln = strtok(NULL,",");
	if(!ln)
		return 0;
	strcpy(Filepath,ln);
	return 1;
	
}

void MOTAPacket(char *buf,char *sender, uint8_t OTASource)
{
	memset((void*)&DownloadReq,0x00,sizeof(download_req_info_s));
	if(!ParseFOTAPacket(buf,DownloadReq.IP,&DownloadReq.Port,DownloadReq.User,DownloadReq.Pass,DownloadReq.FilePath))
	{
		SendResponce(sender,"MOTA INVALID COMMAND",OTASource,0);
		return;
	}
	if(!strstr(DownloadReq.FilePath,".bin"))
	{
		SendResponce(sender,"MOTA file invalid format, only bin file is supported!",OTASource,0);
		return;
	}
	strcpy(DownloadReq.Sender,sender);
	DownloadReq.IsServer=OTASource;
	DownloadReq.RequestType=FTP_REQ_TYPE_CONFIG;
	DownloadReq.AttemptCount=3;
	DownloadReq.IsValid = FOTA_REQ_VALID_CODE;
	strcpy(DownloadReq.InternalFilePath,SERVER_MOTA_FILEPATH);
	UpdateFTPConfigInFlash(&DownloadReq);
	SendResponce(sender,"Attemping FTP for MOTA...",OTASource,1);
	FTPStart(&DownloadReq);
	nwy_sleep(3000);
	//nwy_power_off(2);
	
}


void FOTAPacket(char *buf,char *sender, uint8_t OTASource)
{
	memset((void*)&DownloadReq,0x00,sizeof(download_req_info_s));
	if(!ParseFOTAPacket(buf,DownloadReq.IP,&DownloadReq.Port,DownloadReq.User,DownloadReq.Pass,DownloadReq.FilePath))
	{
		SendResponce(sender,"FOTA INVALID COMMAND",OTASource,0);
		return;
	}
	if(!strstr(DownloadReq.FilePath,".bin"))
	{
		SendResponce(sender,"FOTA file invalid format, only bin file is supported!",OTASource,0);
		return;
	}
	strcpy(DownloadReq.Sender,sender);
	DownloadReq.IsServer=OTASource;
	DownloadReq.RequestType=FTP_REQ_TYPE_FOTA;
	DownloadReq.AttemptCount=3;
	DownloadReq.IsValid = FOTA_REQ_VALID_CODE;
	strcpy(DownloadReq.InternalFilePath,SERVER_FOTA_FILEPATH);
	UpdateFTPConfigInFlash(&DownloadReq);
	SendResponce(sender,"Attemping FTP for FOTA...",OTASource,1);
	FTPStart(&DownloadReq);
	nwy_sleep(3000);
	//nwy_power_off(2);
	
}
#ifdef PROTO_CDAC
extern void InsertIntValue(uint16_t value, uint16_t position, uint16_t length);
#endif
void MakeACTMessage(uint8_t mode, char* code)
{
	char ss[15];
	memset(SimData,0x00,MSGSIZE);
	#if defined(PROTO_MAHARASHTRA1) 
	if(mode==0)
		strcpy(SimData,"$ACTVR,");
	else
		strcpy(SimData,"$HCHKR,");
	#else
	if(mode==0)
		strcpy(SimData,"ACTVR,");
	else
		strcpy(SimData,"HCHKR,");
	#endif
	strncat(SimData, code,6);
	InsertChar(SimData,',');
	#ifdef PROTO_CDAC
	strcat(SimData,"APMG");
	#else
	strcat(SimData,VTSData.VendorID);
	#endif
	InsertChar(SimData,',');
	#if !defined(PROTO_CDAC) && !defined(BSNL_PROTO)
	InsertChar(SimData,'V');
	#endif
	strcat(SimData,FirmVer);
	InsertChar(SimData,',');
	strncat(SimData,NetWork.IMEI,15);
	strcat(SimData,",01,");
	//strcat(SimData,sLatitude);
	StringAdd(SimData,"%012.8f",GPS.Latitude); //012.12345678
	strcat(SimData,",N,");
	//strcat(SimData,sLongitude);
	StringAdd(SimData,"%012.8f",GPS.Longitude);
	strcat(SimData,",E,");
	InsertChar(SimData,GPS.GPSFix + '0');
	InsertChar(SimData,',');
	sprintf(ss,"%02d%02d20%02d ",CurrentDateTime.Date,CurrentDateTime.Month,CurrentDateTime.Year);
	strcat(SimData,ss);
	//InsertChar(SimData,',');
	sprintf(ss,"%02d%02d%02d",CurrentDateTime.Hour ,CurrentDateTime.Min , CurrentDateTime.Sec);
	strcat(SimData,ss);
	InsertChar(SimData,',');
	strcat(SimData,sHeading);
	InsertChar(SimData,',');

	//strcat(SimData,sSpeed);
	StringAdd(SimData,"%04.1f",GPS.Speed);
	InsertChar(SimData,',');
	#ifdef PROTO_CDAC
	InsertIntValue_OLD(SimData,GSM.SignalStrength,"%02d");
	InsertChar(SimData,',');
	InsertIntValue_OLD(SimData,GSM.MCC,"%03d");
	InsertChar(SimData,',');
	InsertIntValue_OLD(SimData,GSM.MNC,"%04d");
	#else
	InsertIntValue(SimData,GSM.SignalStrength,"%02d");
	InsertChar(SimData,',');
	InsertIntValue(SimData,GSM.MCC,"%03d");
	InsertChar(SimData,',');
	InsertIntValue(SimData,GSM.MNC,"%04d");
	#endif
	InsertChar(SimData,',');
	strcat(SimData,GSM.LAC);
	InsertChar(SimData,',');
	InsertChar(SimData,PeriPheralVal.IsMain + '0');
	InsertChar(SimData,',');
	InsertChar(SimData,PeriPheralVal.IGN + '0');
	InsertChar(SimData,',');
	sprintf(ss,"%04.1f",(PeriPheralVal.BattVolt));
	strcat(SimData,ss);
	InsertChar(SimData,',');
	sprintf(ss,"%06d",FrameNumber);
	strcat(SimData,ss);
	#ifdef PROTO_CDAC
	InsertChar(SimData,',');
	InsertChar(SimData,'0');
	InsertChar(SimData,VehicleMovingMode);
	#else
	strcat(SimData,",NR");
	#endif
//	SendSMS(phone,ModemCMD);
	
}


void SendFuelData(char* sender,uint8_t OTASource)
{
	if(!IsFuelData)
	{
		SendResponce(sender,"No Fuel Data",OTASource,0);
		return;
	}
	char *p, *n;
	p = strchr(FuelData,'*');

	if(!p)
		goto PRS_ERR;
	p++;
	n = strchr(p,'#');
	if(!n)
		goto PRS_ERR;
	*n = 0;
	if(strlen(p) > 120)
		goto PRS_ERR;

	memset(SimData,0x00,MSGSIZE);
	sprintf(SimData,"Fuel: %s",p);

	SendResponce(sender,SimData,OTASource,0);
	return;
	PRS_ERR:
	sprintf(SimData,"invalid Fuel Format: %s",FuelData);
	SendResponce(sender,SimData,OTASource,0);
	return;
	
}
#ifdef PROTO_MAHARASHTRA1
void SRDecode(char* msg, uint8_t OTASource)
{
	char *fn;
	char ss[40];
	int i ;
	uint8_t type = 0;;
	if(strstr(msg,"GET"))
		type = 1;
	else if (strstr(msg,"SET"))
		type = 2;
	else if (strstr(msg,"CLR"))
		type = 3;
	
	nwy_dbg_log("SR parse Type : %d",type);
	fn = strstr(msg,"GIP#");
	if(fn)
	{
		nwy_dbg_log("SR GIP cmd");
		if(type == 0)
			return;
		if(type == 1)
		{
			sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP1,VTSData.ServerData.Port1);
			SendResponce(SMSSender,"GET:GIP",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"GIP",'#',0,',',ss);//SET:GIP#13.234.160.106,8224;
			if(!i)
				return;
			if((strlen(ss) > 49) || (strlen(ss) < 5))
				return;
			char ip[50]={0};
			strcpy(ip,ss);


			i = GetValueFromData(fn,"GIP",',',1,';',ss);
			if(!i)
				return;

			strcpy(VTSData.ServerData.IP1,ip);
			strcpy(VTSData.ServerData.Port1,ss);
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP1,VTSData.ServerData.Port1);
			SendResponce(SMSSender,"SET:GIP",OTASource,1);
			InitSockets();
			return;
		}
		if(type == 3)
		{
			strcpy(VTSData.ServerData.IP1,DEFAULT_IP1);
			strcpy(VTSData.ServerData.Port1,DEFAULT_PORT1);
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:GIP",OTASource,1);
			InitSockets();
			return;
		}
	}
	fn = strstr(msg,"APN#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{
			if(VTSData.AutoAPN)
				sprintf(CMD_Buff,"AUTO:%s",NetWork.APN);
			else
				sprintf(CMD_Buff,"%s",NetWork.APN);

			SendResponce(SMSSender,"GET:APN",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"APN",'#',0,';',ss);
			if(!i)
				return;
			if((strlen(ss) > 19) || (strlen(ss) < 5))
				return;
			if(strstr(ss,"AUTO"))
			{
				VTSData.AutoAPN=1;
				sprintf(CMD_Buff,"AUTO");
			}
			else 
			{
				VTSData.AutoAPN = 0;
				strcpy(VTSData.mAPN,ss);
				sprintf(CMD_Buff,"%s",VTSData.mAPN);
			}
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:APN",OTASource,1);
			nwy_sleep(2000);
			nwy_power_off(2);
			nwy_sleep(3000);
		}
		if(type == 3)
		{
			VTSData.AutoAPN=1;
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:APN",OTASource,1);
			nwy_sleep(2000);
			nwy_power_off(2);
			nwy_sleep(3000);
		}
	}
	fn = strstr(msg,"SOS#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{
			
			sprintf(CMD_Buff,"%s",VTSData.PhoneNumber.Mob0);
			SendResponce(SMSSender,"GET:SOS",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"SOS",'#',0,';',ss);
			if(!i)
				return;
			if((strlen(ss) > 13) || (strlen(ss) < 5))
				return;
			
			strcpy(VTSData.PhoneNumber.Mob0,ss);
			sprintf(CMD_Buff,"%s",VTSData.PhoneNumber.Mob0);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:SOS",OTASource,1);
			return;
		
		}
		if(type == 3)
		{
			strcpy(VTSData.PhoneNumber.Mob0,DEFAULT_MOB1);
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:SOS",OTASource,1);
			return;
		}
	}
	fn = strstr(msg,"EIP#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{
			sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP2,VTSData.ServerData.Port2);
			SendResponce(SMSSender,"GET:EIP",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"EIP",'#',0,',',ss);
			if(!i)
				return;
			if((strlen(ss) > 49) || (strlen(ss) < 5))
				return;
			char ip[50];
			strcpy(ip,ss);
			i = GetValueFromData(fn,"EIP",',',1,';',ss);
			if(!i)
				return;
			strcpy(VTSData.ServerData.IP2,ip);
			strcpy(VTSData.ServerData.Port2,ss);
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP2,VTSData.ServerData.Port2);
			SendResponce(SMSSender,"SET:EIP",OTASource,1);
			InitSockets();
			return;
		}
		if(type == 3)
		{
			strcpy(VTSData.ServerData.IP2,DEFAULT_IP2);
			strcpy(VTSData.ServerData.Port2,DEFAULT_PORT2);
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:EIP",OTASource,1);
			InitSockets();
			return;
		}
	}
	fn = strstr(msg,"PIP#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{
			sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP3,VTSData.ServerData.Port3);
			SendResponce(SMSSender,"GET:PIP",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"PIP",'#',0,',',ss);
			if(!i)
				return;
			if((strlen(ss) > 49) || (strlen(ss) < 5))
				return;
			char ip[50];
			strcpy(ip,ss);
			i = GetValueFromData(fn,"PIP",',',1,';',ss);
			if(!i)
				return;
			strcpy(VTSData.ServerData.IP3,ip);
			strcpy(VTSData.ServerData.Port3,ss);
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP3,VTSData.ServerData.Port3);
			SendResponce(SMSSender,"SET:PIP",OTASource,1);
			InitSockets();
			return;
		}
		if(type == 3)
		{
			strcpy(VTSData.ServerData.IP3,DEFAULT_IP3);
			strcpy(VTSData.ServerData.Port3,DEFAULT_PORT3);
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:PIP",OTASource,1);
			InitSockets();
			return;
		}
	}
	fn = strstr(msg,"VRN#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			sprintf(CMD_Buff,"%s",VTSData.VehicleData.VehicleRegNo);
			SendResponce(SMSSender,"GET:VRN",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"VRN",'#',0,';',ss);
			if(!i)
				return;
			if((strlen(ss) > 19) || (strlen(ss) < 3))
				return;
			
			strcpy(VTSData.VehicleData.VehicleRegNo,ss);
			sprintf(CMD_Buff,"%s",VTSData.VehicleData.VehicleRegNo);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:VRN",OTASource,1);
			return;
		
		}
		if(type == 3)
		{
			strcpy(VTSData.VehicleData.VehicleRegNo,DEFAULT_VEHREG);
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:VRN",OTASource,1);
			return;
		}
	}
	fn = strstr(msg,"LOGS#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			sprintf(CMD_Buff,"%d",VTSData.IntervalData.IgnitionInterval);
			SendResponce(SMSSender,"GET:LOGS",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"LOGS",'#',0,';',ss);
			if(!i)
				return;
			int val = atoi(ss);
			if(val < 1 || val > MAX_UINT16)
				return;
			VTSData.IntervalData.IgnitionInterval = val;
			sprintf(CMD_Buff,"%d",VTSData.IntervalData.IgnitionInterval);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:LOGS",OTASource,1);
			return;
		
		}
		if(type == 3)
		{
			VTSData.IntervalData.IgnitionInterval = DEFAULT_INV_IGN;
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:LOGS",OTASource,1);
			return;
		}
	}
	fn = strstr(msg,"LOG2#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			sprintf(CMD_Buff,"%d",VTSData.IntervalData.DataInterval);
			SendResponce(SMSSender,"GET:LOG2",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"LOG2",'#',0,';',ss);
			if(!i)
				return;
			int val = atoi(ss);
			if(val < 1 || val > MAX_UINT16)
				return;
			VTSData.IntervalData.DataInterval = val;
			sprintf(CMD_Buff,"%d",VTSData.IntervalData.DataInterval);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:LOG2",OTASource,1);
			return;
		
		}
		if(type == 3)
		{
			VTSData.IntervalData.DataInterval = DEFAULT_INV_DATA;
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:LOG2",OTASource,1);
			return;
		}
	}
	fn = strstr(msg,"HPTI#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			sprintf(CMD_Buff,"%d",VTSData.IntervalData.HealthInterval);
			SendResponce(SMSSender,"GET:HPTI",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"HPTI",'#',0,';',ss);
			if(!i)
				return;
			int val = atoi(ss);
			if(val < 1 || val > MAX_UINT16)
				return;
			VTSData.IntervalData.HealthInterval = val;
			sprintf(CMD_Buff,"%d",VTSData.IntervalData.HealthInterval);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:HPTI",OTASource,1);
			return;
		
		}
		if(type == 3)
		{
			VTSData.IntervalData.HealthInterval = DEFAULT_INV_HEALTH;
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:HPTI",OTASource,1);
			return;
		}
	}
	fn = strstr(msg,"EPTI#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			sprintf(CMD_Buff,"%d",VTSData.IntervalData.SOSInterval);
			SendResponce(SMSSender,"GET:EPTI",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"EPTI",'#',0,';',ss);
			if(!i)
				return;
			int val = atoi(ss);
			if(val < 1 || val > MAX_UINT16)
				return;
			VTSData.IntervalData.SOSInterval = val;
			sprintf(CMD_Buff,"%d",VTSData.IntervalData.SOSInterval);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:EPTI",OTASource,1);
			return;
		
		}
		if(type == 3)
		{
			VTSData.IntervalData.SOSInterval = DEFAULT_INV_SOS;
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:EPTI",OTASource,1);
			return;
		}
	}
	fn = strstr(msg,"EMTD#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			sprintf(CMD_Buff,"%d",VTSData.IntervalData.SOSTimeOut);
			SendResponce(SMSSender,"GET:EMTD",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"EMTD",'#',0,';',ss);
			if(!i)
				return;
			int val = atoi(ss);
			if(val < 1 || val > MAX_UINT16)
				return;
			VTSData.IntervalData.SOSTimeOut = val;
			sprintf(CMD_Buff,"%d",VTSData.IntervalData.SOSTimeOut);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:EMTD",OTASource,1);
			return;
		
		}
		if(type == 3)
		{
			VTSData.IntervalData.SOSTimeOut = DEFAULT_INV_STM;
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:EMTD",OTASource,1);
			return;
		}
	}
	fn = strstr(msg,"IMON#");
	if(fn)
	{
		if(type == 0)
		{
			OP1_Set(1);
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"IMON",OTASource,1);
		}
		return;
	}
	fn = strstr(msg,"IMOFF#");
	if(fn)
	{
		if(type == 0)
		{
			OP1_Set(0);
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"IMOFF",OTASource,1);
		}
		return;
	}
	fn = strstr(msg,"RST#");
	if(fn)
	{
		if(type == 0)
		{
			if(fn[4] == '1')
				sprintf(CMD_Buff,"1");
			else
				sprintf(CMD_Buff,"0");
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"RST",OTASource,1);
			nwy_sleep(1500);
			nwy_power_off(2);
			nwy_sleep(3000);
		}
		return;
	}
	fn = strstr(msg,"FOTA#");
	if(fn)
	{
		if(type == 0)
		{
			i =GetValueFromData(fn,"FOTA",'#',0,';',ss);
			if(!i)
				return;
			if((strlen(ss) > 40) || (strlen(ss) < 3))
				return;
			
		memset((void*)&DownloadReq,0x00,sizeof(download_req_info_s));
		strcpy(DownloadReq.IP,VTSData.ServerData.IP3);
		strcpy(DownloadReq.User,"CCMSV1");
		strcpy(DownloadReq.Pass,"CCMS@123");
		strcpy(DownloadReq.Sender,SMSSender);
		strcpy(DownloadReq.FilePath,ss);
		strcpy(DownloadReq.InternalFilePath,SERVER_FOTA_FILEPATH);
		DownloadReq.Port = 21;
		DownloadReq.IsServer = OTASource;
		DownloadReq.AttemptCount=3;
		DownloadReq.RequestType=FTP_REQ_TYPE_FOTA;
		DownloadReq.IsValid = FOTA_REQ_VALID_CODE;
		UpdateFTPConfigInFlash(&DownloadReq);

		sprintf(CMD_Buff,"%s",ss);
		SendResponce(SMSSender,"FOTA",OTASource,1);
		FTPStart(&DownloadReq);
		nwy_sleep(3000);		}
		return;
	}
	fn = strstr(msg,"OSL#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			sprintf(CMD_Buff,"%2.0f",VTSData.VehicleData.OverSpeed);
			SendResponce(SMSSender,"GET:OSL",OTASource,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"OSL",'#',0,';',ss);
			if(!i)
				return;
			int val = atoi(ss);
			if(val < 1 || val > MAX_UINT16)
				return;
			VTSData.VehicleData.OverSpeed = val;
			sprintf(CMD_Buff,"%2.0f",VTSData.VehicleData.OverSpeed);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:OSL",OTASource,1);
			return;
		
		}
		if(type == 3)
		{
			VTSData.VehicleData.OverSpeed = DEFAULT_OVERSPEED;
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:OSL",OTASource,1);
			return;
		}
	}
	fn = strstr(msg,"PGF#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{
			memset(CMD_Buff,0,250);
			for(int j = 0; j < 10; j++)
			{
				if(VTSData.GeoLatLng[0].Latitude[j] == 0 && VTSData.GeoLatLng[0].Longitude[j] == 0)
					break;

				sprintf(ss,"%03.6f,%03.6f,",VTSData.GeoLatLng[0].Latitude[j],VTSData.GeoLatLng[0].Longitude[j]);
				strcat(CMD_Buff,ss);
			}
			CMD_Buff[strlen(CMD_Buff)-1] = '\0';
			SendResponce(SMSSender,"GET:PGF",OTASource,0);
		}
		if(type == 2)
		{
			if(DecodeGeofenceSR(fn))
			{
				memset(CMD_Buff,0,250);
				for(int j = 0; j < 10; j++)
				{
					if(VTSData.GeoLatLng[0].Latitude[j] == 0 && VTSData.GeoLatLng[0].Longitude[j] == 0)
						break;

					sprintf(ss,"%03.6f,%03.6f,",VTSData.GeoLatLng[0].Latitude[j],VTSData.GeoLatLng[0].Longitude[j]);
					strcat(CMD_Buff,ss);
				}
				CMD_Buff[strlen(CMD_Buff)-1] = '\0';
				SendResponce(SMSSender,"SET:PGF",OTASource,0);
			}
		}
		if(type == 3)
		{
			ClearGeofence();
			UpdateConfigInFlash();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:PGF",OTASource,1);
		}
	}
	fn = strstr(msg,"EPSTOP#");
	if(fn)
	{
		if(type==0)
		{
			ResetSOS();
			sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"EPSTOP",OTASource,1);
		}
		
		return;
	}


}
#endif


uint8_t DecodeSMS(char* msg,uint8_t OTASource)
{
	char* fn;
	char* ls;
	char ss[45];
	#ifndef PROTO_CDAC
	char rnd[10];
	#endif
	int i;
	float f;
	if(msg==NULL)
	{
		nwy_dbg_log("DecodeSMS: Invalid NULL message, SOURCE:%d",OTASource);
		return 0;
	}
	else
		nwy_dbg_log("Decoding: %s || SOURCE:%d",msg,OTASource);
	
	// Check for sensor data from RS232 (MCU) before other command parsing
	// This handles RFID and FUEL sensor data formats
	if(OTASource == OTA_SRC_SERIAL)
	{
		if(ParseSensorData(msg, strlen(msg)))
		{
			// Data was recognized as sensor data, no further processing needed
			return 1;
		}
	}
	
	memset(CMD_Buff,0x00,250);
	#ifdef PROTO_MAHARASHTRA1
	ls = strstr(msg,"+S*R:");
	if(ls)
	{
		ls+=5;
		IsSRCMD=1;
		nwy_dbg_log("SR Command Parsing...");
		SRDecode(ls, OTASource);
		return 1;
	}
	IsSRCMD=0;
	#endif
	#ifndef PROTO_CDAC
	ls = strstr(msg,"ACTV");
	if(ls)
	{
		// if(OTASource==1)
		// 	return 1;
		if(strlen(ls)<11)
			return 0;
		fn=strchr(ls,',');
		if(fn)
		{
			fn++;
			
			ls=strchr(fn,',');
			if(ls)
			{
				*ls=0;
				ls++;
				strncpy(rnd,fn,8);

				fn = strchr(ls,'\r');
				if(!fn)
				{
					fn = strchr(ls,'\0');
				}
				*fn = 0;
				strncpy(ss,ls,14);
				nwy_dbg_log("Sending ACTV reply with Rc : %s to %s",rnd,ss);
				MakeACTMessage(0,  rnd);
				SendSMS(ss,SimData);
				TCPSocket_SendString(&ServerSocket[0],SimData);
				TCPSocket_SendStringNoAck(&ServerSocket[2],SimData);
				#ifdef EXTENDED_IPS
				TCPSocket_SendString(&ServerSocket[3],SimData);
				#endif
				
				ACTMsg=1;
				return 1;
			}
		}
	}
	ls = strstr(msg,"HCHK");
	if(ls)
	{
		// if(OTASource==1)
		// 	return 1;
		if(strlen(ls)<11)
			return 0;
		fn=strchr(ls,',');
		if(fn)
		{
			fn++;
			
			ls=strchr(fn,',');
			if(ls)
			{
				*ls=0;
				ls++;
				strncpy(rnd,fn,8);

				fn = strchr(ls,'\r');
				if(!fn)
				{
					fn = strchr(ls,'\0');
				}
				*fn = 0;
				strncpy(ss,ls,14);
				nwy_dbg_log("\r\nSending HCHKR reply with Rc : %s to %s",rnd,ss);
				MakeACTMessage(1,  rnd);
				SendSMS(ss,SimData);
				TCPSocket_SendString(&ServerSocket[0],SimData);
				TCPSocket_SendStringNoAck(&ServerSocket[2],SimData);
				#ifdef EXTENDED_IPS
				TCPSocket_SendString(&ServerSocket[3],SimData);
				#endif
				ACTMsg=2;
				return 1;
			}
		}
	}
	#endif
	fn=strstr(msg,"GET");
	if(fn)
	{
		memset(SimData,0x00,MSGSIZE);
		fn=fn+3;

		if(strstr(fn,"SIMMAKE"))
		{
			sprintf(SimData,"Current Selected STK Protocol : ");
			strcat(SimData,SIM_MAKE_STR);
			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}

		if(strstr(fn,"OUTSTAT"))
		{
			sprintf(SimData,"Output 1 - %d\nOutput 2 - %d",PeriPheralVal.OP1,PeriPheralVal.OP2);
			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}
		if(strstr(fn,"INPSTAT"))
		{
			sprintf(SimData,"Input 1 - %d\nInput 2 - %d",PeriPheralVal.IP1,PeriPheralVal.IP2);
			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}
		if(strstr(fn,"OPERATOR"))
		{
			memset(ss,0x00,sizeof(ss));
			if(GSM.GSMState > SIM_DETECTED)
			{
				strcpy(ss,NetWork.Network);
			}
			else
			{
				#ifdef 	SIM_PROFILE_AIRTEL	
				if(VTSState.CurrentProfile == SIM_PROFILE_AIRTEL)
					sprintf(ss,"assumed to be AIRTEL");
				#endif
				#ifdef 	SIM_PROFILE_JIO
				else if(VTSState.CurrentProfile == SIM_PROFILE_JIO)
					sprintf(ss,"assumed to be JIO");
				#endif
				#ifdef 	SIM_PROFILE_VI
				else if(VTSState.CurrentProfile == SIM_PROFILE_VI)
					sprintf(ss,"assumed to be VODAFONE");
				#endif
				#ifdef 	SIM_PROFILE_BSNL
				else if(VTSState.CurrentProfile == SIM_PROFILE_BSNL)
					sprintf(ss,"assumed to be BSNL");
				#endif
				else
					sprintf(ss,"currently UNKNOWN");
			}
			sprintf(SimData,"Network Operator is %s",ss);
			SendResponce(SMSSender,SimData,OTASource,0);
			
			return 1;
		}
		if(strstr(fn,"PRF"))
		{
			if(VTSData.ServerData.Url1[0]==1)
			{
				sprintf(SimData,"SPN Subbed as %s",NetWork.Network);
			}
			else
			{
				sprintf(SimData,"SPN Actual as %s",NetWork.Network);
			}
			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}

		if(strstr(fn,"PROFILE"))
		{
			sprintf(SimData,"Current Profile : %d",VTSState.CurrentProfile);
			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}

		if(strstr(fn,"SOSTIMEOUT"))
		{
			sprintf(SimData,"SOS Timout: %d",VTSData.IntervalData.SOSTimeOut);
			SendResponce(SMSSender,SimData,OTASource,0);\
			return 1;
		}

		if(strstr(fn,"VDETAIL"))
		{
			memset(SimData,0x00,MSGSIZE);
			strcpy(SimData,"FirVer = ");
			strcat(SimData,FirmVer);
			strcat(SimData,"\r\nBuild Date - 21/00/2022\r\n Binary Size - 60120\r\n");
			strcat(SimData,"CheckSum 0x51AC");
			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}
		if(strstr(fn,"VSTATUS"))
		{
			memset(SimData,0x00,MSGSIZE);
			strcpy(SimData,"GPRS ");
			if(GSM.GSMState!=GPRS_ACTIVE)
				strcat(SimData,"NOT ");
			strcat(SimData,"ACTIVE\n");
			sprintf(ss,"%d",GSM.SignalStrength);
			strcat(SimData,"SIG : ");
			strcat(SimData,ss);
			sprintf(ss,"\nSOS:%d, IGN:%d",SOS.IsSOS, PeriPheralVal.IGN);
			strcat(SimData,ss);
			sprintf(ss,"%02.2f",PeriPheralVal.MainsVolt);
			strcat(SimData,"\nMV: ");
			strcat(SimData,ss);
			sprintf(ss,"%01.2f",PeriPheralVal.BattVolt);
			strcat(SimData,"\nBV: ");
			strcat(SimData,ss);
			strcat(SimData,"\nSV: ");
			#if  defined(HTTP_SIMULATE) && defined(PROTO_CDAC)
			if(HTTPState == HTTP_STATE_SET)
				strcat(SimData,"G.TR OK, ");
			else
				strcat(SimData,"G.TR NC, ");
			#else
			if(ServerSocket[0].SocketState == SOCKET_CONNECTED)
				strcat(SimData,"G.TR OK, ");
			else
				strcat(SimData,"G.TR NC, ");
			
			#endif

			#ifndef PROTO_CDAC
			if(ServerSocket[1].SocketState == SOCKET_CONNECTED)
				strcat(SimData,"G.EM OK, ");
			else
				strcat(SimData,"G.EM NC, ");
			#endif

			if(ServerSocket[2].SocketState == SOCKET_CONNECTED)
				strcat(SimData,"P.TR OK");
			else
				strcat(SimData,"P.TR NC");
			
			#ifdef EXTENDED_IPS
			if(ServerSocket[3].SocketState == SOCKET_CONNECTED)
				strcat(SimData,"E.IP OK");
			else
				strcat(SimData,"E.IP NC");
			#endif

			if(GPS.GPSFix)
				strcat(SimData,"\nGPS OK");
			else
			{
				if(IsGPSFault)
					strcat(SimData,"\nGPS Flt");
				else
					strcat(SimData,"\nGPS NF");
				
			}
			#ifndef PROTO_CDAC
			sprintf(ss,"\nHP: %d",StoredHistoryDataCount);
			#else
			sprintf(ss,"\nHP: %d",FTable.TotalFiles);
			#endif
			strcat(SimData,ss);


			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}
		if(strstr(fn,"SERVERDETAIL"))
		{
			memset(SimData,0x00,MSGSIZE);
			strcpy(SimData,"IP1 -  ");
			strcat(SimData,VTSData.ServerData.IP1);
			strcat(SimData,"\nPort1 - ");
			strcat(SimData,VTSData.ServerData.Port1);
			strcat(SimData,"\nIP2 - ");
			strcat(SimData,VTSData.ServerData.IP2);
			strcat(SimData,"\nPort2 - ");
			strcat(SimData,VTSData.ServerData.Port2);
			strcat(SimData,"\nIP3 - ");
			strcat(SimData,VTSData.ServerData.IP3);
			strcat(SimData,"\nPort3 - ");
			strcat(SimData,VTSData.ServerData.Port3);
			#ifdef EXTENDED_IPS
			strcat(SimData,"\nEx4 IP:");
			strcat(SimData,VTSData.ServerData.Url2);
			#endif
			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}
		if(strstr(fn,"LOCATION"))
		{
			memset(SimData,0x00,MSGSIZE);
			strcpy(SimData,"Latitude -  ");
			strcat(SimData,sLatitude);
			strcat(SimData,"\nLongitude - ");
			strcat(SimData,sLongitude);
			strcat(SimData,"\nAltitude - ");
			strcat(SimData,sAltitude);
			strcat(SimData,"\nSpeed - ");
			strcat(SimData,sSpeed);
//			strcat(SimData,"\r\nTime : ");
//			sprintf(ss,"%0.2d%0.2d%0.2d",g_sRTC.u32Hour ,g_sRTC.u32Minute,g_sRTC.u32Second);
//			strcat(SimData,ss);
//			strcat(SimData,"\r\nDate : ");
//			sprintf(ss,"%0.2d%0.2d20%0.2d",g_sRTC.u32Day,g_sRTC.u32Month,g_sRTC.u32Year);
//			strcat(SimData,ss);
			SendResponce(SMSSender,SimData,OTASource,0);
	//		SendSMS(SMSSender,SimData);
			return 1;
		}
		if(strstr(fn,"PANIC"))
		{
			memset(SimData,0x00,MSGSIZE);
			strcpy(SimData,"SOS -  ");
			if(SOS.IsSOS)
				strcat(SimData,"ON");
			else
				strcat(SimData,"OFF");
			
			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}
		if(strstr(fn,"VINFO"))
		{
			memset(SimData,0x00,MSGSIZE);
			strcpy(SimData,"VID -  ");
			strcat(SimData,VTSData.VendorID);
			strcat(SimData,"\nIMEI - ");
			strcat(SimData,NetWork.IMEI);
			strcat(SimData,"\nCCID - ");
			strcat(SimData,NetWork.SIMNo);
			strcat(SimData,"\nVehicleNo - ");
			strcat(SimData,VTSData.VehicleData.VehicleRegNo);
			strcat(SimData,"\nNetwork - ");
			strcat(SimData,NetWork.Network);
			strcat(SimData,"\nAPNMODE:");
			if(VTSData.AutoAPN)
				strcat(SimData,"AUTO");
			else
				strcat(SimData,"MANUAL");
			strcat(SimData,"\nCurrent APN - ");
			strcat(SimData,NetWork.APN);
			strcat(SimData,"\nBatt Thershold - ");
			sprintf(ss,"%2.1f",VTSData.BattThrs);
			strcat(SimData,ss);
			strcat(SimData,"\nSOS1 - ");
			strcat(SimData,VTSData.PhoneNumber.Mob0);
			strcat(SimData,"\nSOS2 -  ");
			strcat(SimData,VTSData.PhoneNumber.Mob1);
			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}
		if(strstr(fn,"VINTERVAL"))
		{	
			#ifndef PROTO_CDAC
			memset(SimData,0x00,MSGSIZE);
			strcpy(SimData,"IMEI -  ");
			strcat(SimData,NetWork.IMEI);
			strcat(SimData,"\nINTERVAL\nNORMAL - ");
			sprintf(ss,"%d",VTSData.IntervalData.DataInterval);
			strcat(SimData,ss);
			strcat(SimData,"\nIGN ON - ");
			sprintf(ss,"%d",VTSData.IntervalData.IgnitionInterval);
			strcat(SimData,ss);
			strcat(SimData,"\nSOS - ");
			sprintf(ss,"%d",VTSData.IntervalData.SOSInterval);
			strcat(SimData,ss);
			strcat(SimData,"\nStandBy - ");
			sprintf(ss,"%d",VTSData.IntervalData.StandbyInterval);
			strcat(SimData,ss);
			strcat(SimData,"\nHealth - ");
			sprintf(ss,"%d", VTSData.IntervalData.HealthInterval);
			strcat(SimData,ss);
			#else
			memset(SimData,0x00,MSGSIZE);
			strcpy(SimData,"IMEI -  ");
			strcat(SimData,NetWork.IMEI);
			strcat(SimData,"\nINTERVAL\nMotion - ");
			sprintf(ss,"%d",VTSData.IntervalData.MotionInterval);
			strcat(SimData,ss);
			strcat(SimData,"\nHalt - ");
			sprintf(ss,"%d",VTSData.IntervalData.HaltInterval);
			strcat(SimData,ss);
			strcat(SimData,"\nSOS - ");
			sprintf(ss,"%d",VTSData.IntervalData.EnergencyInterval);
			strcat(SimData,ss);
			strcat(SimData,"\nStandBy - ");
			sprintf(ss,"%d",VTSData.IntervalData.SleepInterval);
			strcat(SimData,ss);
			strcat(SimData,"\nHealth - ");
			sprintf(ss,"%d", VTSData.IntervalData.HealthInterval);
			strcat(SimData,ss);
			strcat(SimData,"\nFull - ");
			sprintf(ss,"%d", VTSData.IntervalData.FullDataPacketInterval);
			strcat(SimData,ss);
			#endif
			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}
		if(strstr(fn,"VBEHAVE"))
		{
			memset(SimData,0x00,MSGSIZE);
			strcpy(SimData,"IMEI -  ");
			strcat(SimData,NetWork.IMEI);
			strcat(SimData,"\nHarsh Acc - ");
			sprintf(ss,"%d",VTSData.VehicleData.HarshAcc);
			strcat(SimData,ss);
			strcat(SimData,"\nHarsh Break - ");
			sprintf(ss,"%d",VTSData.VehicleData.HarshBreak);
			strcat(SimData,ss);
			strcat(SimData,"\nRash Turn - ");
			sprintf(ss,"%d",VTSData.VehicleData.RashTurn);
			strcat(SimData,ss);
			strcat(SimData,"\nOver Speed - ");
			sprintf(ss,"%06.1f",VTSData.VehicleData.OverSpeed);
			strcat(SimData,ss);
			SendResponce(SMSSender,SimData,OTASource,0);
			
			return 1;
		}
		if(strstr(fn,"SENS"))
		{
			// Return new sensor interval config: GET SENSINT
			memset(SimData,0x00,MSGSIZE);
			sprintf(SimData,"SensEN - %d\nDayIGN - %d\nNightIGN - %d\nDayOFF - %d\nNightOFF - %d\nTimeout - %d",
				SensorsConfig.isEnabled,
				SensorsConfig.intervalConfig.dayIGNITIONInterval,
				SensorsConfig.intervalConfig.nightIGNITIONInterval,
				SensorsConfig.intervalConfig.dayOFFInterval,
				SensorsConfig.intervalConfig.nightOFFInterval,
				SensorsConfig.sensorTimeout);
			SendResponce(SMSSender,SimData,OTASource,0);
			return 1;
		}
		#ifndef PROTO_CDAC
		ls = strstr(fn,"GEO");
		if(ls)
		{
			if(OTASource==OTA_SRC_SMS)
				return 1;

			i = ls[3] - '0';
			if(i < 0 || i > 9)
				return 1;
			nwy_dbg_log("sending geo data for loc %d",i);
			SendGeoData(i,OTASource);
			return 1;
		}
		#endif
		if(strstr(fn,"MCU"))
		{
			if(GetMCUVersionReq())
			{
				memset(SimData,0x00,MSGSIZE);
				sprintf(SimData,"Current MCU Version : %s",MCUVersion);
				SendResponce(SMSSender,SimData,OTASource,0);
			}
			else
				SendResponce(SMSSender,"Unable to Get MCU Version",OTASource,0);
			return 1;
		}
		if(strstr(fn,"FUEL"))
		{
			SendFuelData(SMSSender,OTASource);
			return 1;
		}
	}
	fn = strstr(msg,"CLR");
	if(fn)
	{
		fn=fn+3;
		ls = strstr(fn,"HISTORY");
		if(ls)
		{
			#ifndef PROTO_CDAC
			DeleteAllPackets();
			#else
			ClearFileTable();
			#endif

			SendResponce(SMSSender,"History Cleared",OTASource,1);
		}
	}
	fn = strstr(msg,"SET");
	if(fn)
	{
		fn=fn+3;

		ls = strstr(fn,"IMI");
		if(ls)
		{
			
			i=GetValueFromData(ls,"IMI",' ',0,'\0',ss);
			if(i)
			{
				if(ss[0] == '0')
				{
					VTSState.CustomImei.IsEnable=0;
					SendResponce(SMSSender,"Config MI reset",OTASource,1);
				}
				if(strlen(ss) != 15)
				{
					SendResponce(SMSSender,"Invalid IMEI !",OTASource,0);
					return 1;
				}
				nwy_dbg_log("Setting IMEI : %s",ss);
				strncpy(VTSState.CustomImei.Imei,ss,sizeof(VTSState.CustomImei.Imei));
				VTSState.CustomImei.IsEnable=1;
				UpdateStateInFlash();
				SendResponce(SMSSender,"Config MI set",OTASource,1);
				strcpy(NetWork.IMEI,VTSState.CustomImei.Imei);
				InitSockets();
			}
			else
			{
				SendResponce(SMSSender,"Invalid Config IMI!",OTASource,0);
			}
		}
		ls = strstr(fn,"FGPS");
		if(ls)
		{
			nwy_dbg_log("FGPS cmd");
			double flat=0, flng=0, pdop=0, hdop=0; // SET FGPS 28234345,76123123,180,120,8,200,25,180     // SET FGPS lat,long,hdop,pdop,noofsat,altitude,Speed,Heading
			uint8_t nofsat=0;
			int altitude=0, speed=0, heading=0;
			
			i=GetValueFromData(ls,"FGPS",' ',0,',',ss);
			if(i)
			{
				flat = (double)atol(ss)/1000000;
				nwy_dbg_log("FLat : %f",flat);
			}
			i=GetValueFromData(ls,"FGPS",',',1,',',ss);
			if(i)
			{
				flng = (double)atol(ss)/1000000;
				nwy_dbg_log("Flng : %f",flng);
			}
			i=GetValueFromData(ls,"FGPS",',',2,',',ss);
			if(i)
			{
				pdop = (double)atol(ss)/100;
				nwy_dbg_log("pdop : %f",pdop);
			}
			i=GetValueFromData(ls,"FGPS",',',3,',',ss);
			if(i)
			{
				hdop = (double)atol(ss)/100;
				nwy_dbg_log("hdop : %f",hdop);
			}
			i=GetValueFromData(ls,"FGPS",',',4,',',ss);
			if(i)
			{
				nofsat = atoi(ss);
				nwy_dbg_log("sats : %i",nofsat);
			}
			i=GetValueFromData(ls,"FGPS",',',5,',',ss);
			if(i)
			{
				altitude = atoi(ss);
				nwy_dbg_log("altitude : %d",altitude);
			}
			i=GetValueFromData(ls,"FGPS",',',6,',',ss);
			if(i)
			{
				speed = atoi(ss);
				nwy_dbg_log("speed : %d",speed);
			}
			i=GetValueFromData(ls,"FGPS",',',7,'\0',ss);
			if(i)
			{
				heading = atoi(ss);
				nwy_dbg_log("heading : %d",heading);
			}
			
			if(flat != 0 && flng != 0 && pdop != 0 && hdop != 0 && nofsat != 0)
			{
				fGPSAlt = altitude;
				fGPSLat = flat;
				fGPSLong = flng;
				fGPShdop = hdop;
				fGPSpdop = pdop;
				fGPSSats = nofsat;
				fGPSSpeed = speed;
				fGPSHeading = heading;
				fGPSForce=0;
				nwy_dbg_log("FGPS Set complete with Alt: %d, Speed: %d, Heading: %d", fGPSAlt, fGPSSpeed, fGPSHeading);
				SendResponce(SMSSender,"System Config Complete",OTASource,0);
			}
		}
		ls = strstr(fn,"CGPS");
		if(ls)
		{
			nwy_dbg_log("CGPS cmd");
			double flat=0, flng=0, pdop=0, hdop=0; // SET FGPS 28234345,76123123,180,120,8,200,25,180     // SET FGPS lat,long,hdop,pdop,noofsat,altitude,Speed,Heading
			uint8_t nofsat=0;
			int altitude=0, speed=0, heading=0;
			
			i=GetValueFromData(ls,"CGPS",' ',0,',',ss);
			if(i)
			{
				flat = (double)atol(ss)/1000000;
				nwy_dbg_log("FLat : %f",flat);
			}
			i=GetValueFromData(ls,"CGPS",',',1,',',ss);
			if(i)
			{
				flng = (double)atol(ss)/1000000;
				nwy_dbg_log("Flng : %f",flng);
			}
			i=GetValueFromData(ls,"CGPS",',',2,',',ss);
			if(i)
			{
				pdop = (double)atol(ss)/100;
				nwy_dbg_log("pdop : %f",pdop);
			}
			i=GetValueFromData(ls,"CGPS",',',3,',',ss);
			if(i)
			{
				hdop = (double)atol(ss)/100;
				nwy_dbg_log("hdop : %f",hdop);
			}
			i=GetValueFromData(ls,"CGPS",',',4,',',ss);
			if(i)
			{
				nofsat = atoi(ss);
				nwy_dbg_log("sats : %i",nofsat);
			}
			i=GetValueFromData(ls,"CGPS",',',5,',',ss);
			if(i)
			{
				altitude = atoi(ss);
				nwy_dbg_log("altitude : %d",altitude);
			}
			i=GetValueFromData(ls,"CGPS",',',6,',',ss);
			if(i)
			{
				speed = atoi(ss);
				nwy_dbg_log("speed : %d",speed);
			}
			i=GetValueFromData(ls,"CGPS",',',7,'\0',ss);
			if(i)
			{
				heading = atoi(ss);
				nwy_dbg_log("heading : %d",heading);
			}
			
			if(flat != 0 && flng != 0 && pdop != 0 && hdop != 0 && nofsat != 0)
			{
				fGPSAlt = altitude;
				fGPSLat = flat;
				fGPSLong = flng;
				fGPShdop = hdop;
				fGPSpdop = pdop;
				fGPSSats = nofsat;
				fGPSSpeed = speed;
				fGPSHeading = heading;
				fGPSForce=1;
				nwy_dbg_log("FGPS Set complete with Alt: %d, Speed: %d, Heading: %d", fGPSAlt, fGPSSpeed, fGPSHeading);
				SendResponce(SMSSender,"System Config Complete",OTASource,0);
			}
		}

		ls = strstr(fn,"SIMMAKE");
		if(ls)
		{
			ls+= 8;
			if(strstr(ls,"TAIS"))
				VTSData.SIMMake = TAISYS;
			else if(strstr(ls,"SENS"))
				VTSData.SIMMake = SENSORISE;
			else if(strstr(ls,"GND"))
				VTSData.SIMMake = GnD;
			else
			{
				SendResponce(SMSSender,"Invalid Sim make !",OTASource,0);
				return 1;
			}
			UpdateConfigInFlash();
			SendResponce(SMSSender,"Sim Make Changed",OTASource,1);
			nwy_sleep(2000);
			nwy_power_off(2);
			nwy_sleep(3000);
			return 1;
		}

		ls=strstr(fn,"OUTSTAT ");
		if(ls)
		{   //OUTSTAT 0,0
			if(ls[8]=='0')
			{OP1_Set(0);}
			else if(ls[8]=='1')
			{OP1_Set(1);}

			if(ls[10]=='0')
			{OP2_Set(0);}
			else if(ls[10]=='1')
			{OP2_Set(1);}	

			SendResponce(SMSSender,"Output Updated",OTASource,1);
		}

		ls=strstr(fn,"DFTP");
		if(ls)
		{
			LoadDefaultFOTAParams(SMSSender,OTASource);
			return 1;
		}

		ls=strstr(fn,"FOTA");
		if(ls)
		{
			FOTAPacket(ls,SMSSender,OTASource);
			return 1;
		}

		ls=strstr(fn,"MOTA");
		if(ls)
		{
			MOTAPacket(ls,SMSSender,OTASource);
			return 1;
		}
		ls=strstr(fn,"PRF ");
		if(ls)
		{
			if(ls[4] == '0')
			{
				VTSData.ServerData.Url1[0] = 0;
				UpdateConfigInFlash();
				SendResponce(SMSSender,"Prf Reset Requested",OTASource,1);
				nwy_sleep(4000);
				nwy_power_off(2);
				nwy_sleep(4000);
			}
			else
			{
				i=GetValueFromData(ls,"PRF",' ',0,'\0',ss);
				if(i)
				{
					strcpy(&VTSData.ServerData.Url1[1],ss);
					VTSData.ServerData.Url1[0]=1;
					UpdateConfigInFlash();
					SendResponce(SMSSender,"Prf Change Requested",OTASource,1);
					nwy_sleep(4000);
					nwy_power_off(2);
					nwy_sleep(4000);
				}
			}
		}
		ls=strstr(fn,"OPERATOR");
		if(ls)
		{
			i=GetValueFromData(ls,"OPERATOR",' ',0,'\0',ss);
			if(i)
			{
				// Convert to lowercase once for efficient comparison
				char lower_ss[32];
				int j;
				for(j = 0; ss[j] && j < sizeof(lower_ss) - 1; j++)
				{
					lower_ss[j] = (ss[j] >= 'A' && ss[j] <= 'Z') ? ss[j] + 32 : ss[j];
				}
				lower_ss[j] = '\0';
				
				uint8_t req = 0;
				const char *operator_name = NULL;
				
				// Check operator type with single string scan
				if(strstr(lower_ss, "airtel"))
				{
					#ifdef SIM_PROFILE_AIRTEL
					req = SIM_PROFILE_AIRTEL;
					#else
					operator_name = "Airtel";
					#endif
				}
				else if(strstr(lower_ss, "voda") || strstr(lower_ss, "idea") || strstr(lower_ss, "vi"))
				{
					#ifdef SIM_PROFILE_VODAFONE
					req = SIM_PROFILE_VODAFONE;
					#else
					operator_name = "Vodafone";
					#endif
				}
				else if(strstr(lower_ss, "jio"))
				{
					#ifdef SIM_PROFILE_JIO
					req = SIM_PROFILE_JIO;
					#else
					operator_name = "Jio";
					#endif
				}
				else if(strstr(lower_ss, "bsnl"))
				{
					#ifdef SIM_PROFILE_BSNL
					req = SIM_PROFILE_BSNL;
					#else
					operator_name = "BSNL";
					#endif
				}
				else
				{
					nwy_dbg_log("Invalid Operator Profile Req");
					SendResponce(SMSSender,"Invalid Operator Profile Req",OTASource,0);
					return 1;
				}
				
				// Handle unsupported profile case
				if(operator_name != NULL)
				{

					sprintf(SimData, "%s operator profile not supported in this SIM", operator_name);
					nwy_dbg_log("%s", SimData);
					SendResponce(SMSSender, SimData, OTASource, 0);
					return 1;
				}
				
				// Profile change request
				if(req > 0)
				{
					sprintf(SimData,"Operator Profile Change to %s aka %d Requested", lower_ss, req);
					SendResponce(SMSSender,SimData,OTASource,1);
					nwy_dbg_log("%s", SimData);
					nwy_sleep(1000);
					prfReq = req;
				}
			}
		}

		ls=strstr(fn,"PROFILE ");
		if(ls)
		{
			switch(ls[8])
			{
				case '1' : prfReq = 1;
					break;
				case '2' : prfReq = 2;
					break;
				case '3' : prfReq = 3;
					break;
				default:
					SendResponce(SMSSender,"INVALID Profile Change Req",OTASource,0);
					return 1;
			
			}
			SendResponce(SMSSender,"Profile Change Requested",OTASource,1);
			return 1;
		}
		ls = strstr(fn,"VID");
		if(ls)
		{
			i=GetValueFromData(ls,"VID",' ',0,'\0',ss);
			if(i)
			{
				if(strlen(ss)>25 || strlen(ss) < 3)
				{
					SendResponce(SMSSender,"Vendor ID invalid Length",OTASource,0);
					return 1;
				}
				strcpy(VTSData.VendorID,ss);
				UpdateConfigInFlash();
				sprintf(SimData,"Vendor ID Changed to %s",VTSData.VendorID);
				SendResponce(SMSSender,SimData,OTASource,0);
				return 1;
			}
		}

		ls=strstr(fn,"SERVER");
		if(ls)
		{
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER1",' ',0,',',ss);// SETSERVER1 ip,port[,SERVER2 ip,port]
			if(i)
			{
				strcpy(VTSData.ServerData.IP1,ss);
			}
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER1",',',1,',',ss);
			if(i)
			{
				strcpy(VTSData.ServerData.Port1,ss);
			}
			else
			{
				memset(ss,0,sizeof(ss));
				i=GetValueFromData(ls,"SERVER1",',',1,'\0',ss);
				if(i)
				{
					strcpy(VTSData.ServerData.Port1,ss);
				}
			}
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER2",' ',0,',',ss);
			if(i)
			{
				strcpy(VTSData.ServerData.IP2,ss);
			}
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER2",',',1,',',ss);
			if(i)
			{
				strcpy(VTSData.ServerData.Port2,ss);
			}
			else
			{
				memset(ss,0,sizeof(ss));
				i=GetValueFromData(ls,"SERVER2",',',1,'\0',ss);
				if(i)
				{
					strcpy(VTSData.ServerData.Port2,ss);
				}
			}

			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER3",' ',0,',',ss);
			if(i)
			{
				strcpy(VTSData.ServerData.IP3,ss);
			}
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER3",',',1,',',ss);
			if(i)
			{
				strcpy(VTSData.ServerData.Port3,ss);
			}
			else
			{
				memset(ss,0,sizeof(ss));
				i=GetValueFromData(ls,"SERVER3",',',1,'\0',ss);
				if(i)
				{
					strcpy(VTSData.ServerData.Port3,ss);
				}
			}
			#ifdef EXTENDED_IPS
			// Check if SERVER4 is provided, otherwise leave Url2 unchanged
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER4",' ',0,',',ss);
			if(i)  // SERVER4 found
			{
				char ip[50], port[50];
				strcpy(ip,ss);
				
				memset(ss,0,sizeof(ss));
				i=GetValueFromData(ls,"SERVER4",',',1,'\0',ss);
				if(i)
				{
					strcpy(port,ss);
					sprintf(VTSData.ServerData.Url2,"%s,%s",ip,port);
					nwy_dbg_log("SERVER4 updated: %s", VTSData.ServerData.Url2);
				}
			}
			sprintf(SimData,"Update IP1: %s, %s\nIP2: %s,%s\nIP3: %s,%s\nIP4: %s",VTSData.ServerData.IP1,VTSData.ServerData.Port1,VTSData.ServerData.IP2,VTSData.ServerData.Port2,VTSData.ServerData.IP3,VTSData.ServerData.Port3,VTSData.ServerData.Url2);
			#else
			sprintf(SimData,"Update IP1: %s, %s\nIP2: %s,%s\nIP3: %s,%s",VTSData.ServerData.IP1,VTSData.ServerData.Port1,VTSData.ServerData.IP2,VTSData.ServerData.Port2,VTSData.ServerData.IP3,VTSData.ServerData.Port3);
			#endif
			SendResponce(SMSSender,SimData,OTASource,1);
			UpdateConfigInFlash();
			InitSockets();

			return 1;
		}
		ls = strstr(fn,"APN");
		if(ls)
		{
			if(strstr(ls,"AUTO"))
			{
				VTSData.AutoAPN=1;
				UpdateConfigInFlash();
				SendResponce(SMSSender,"APN Set to AUTO, Restarting...",OTASource,1);
				nwy_sleep(5000);
				nwy_power_off(2);
				return 1;
			}
			i=GetValueFromData(ls,"APN",' ',0,'\0',ss);
			if(i)
			{
				if(strlen(ss) > 1 && strlen(ss) < 20)
				{
					VTSData.AutoAPN=0;
					strcpy(VTSData.mAPN,ss);
					UpdateConfigInFlash();
					sprintf(SimData,"APN Changed to %s, Restarting...",VTSData.mAPN);
					SendResponce(SMSSender,SimData,OTASource,1);
					nwy_sleep(5000);
					nwy_power_off(2);
				}
				else
					SendResponce(SMSSender,"INVALID APN",OTASource,0);
			}

		}
		ls=strstr(fn,"INTERVAL");
		if(ls)
		{
			#ifndef PROTO_CDAC
			i=GetValueFromData(ls,"INTERVAL",' ',0,',',ss);
			if(i)
			{
				i=atoi(ss);
			//	i= (int)ss;
				if(i > 4)
					VTSData.IntervalData.DataInterval = i;
			}
			i=GetValueFromData(ls,"INTERVAL",',',1,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 1)
					VTSData.IntervalData.IgnitionInterval = i;
			}
			i=GetValueFromData(ls,"INTERVAL",',',2,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 1)
					VTSData.IntervalData.SOSInterval = i;
			}
			i=GetValueFromData(ls,"INTERVAL",',',3,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 10)
					VTSData.IntervalData.StandbyInterval = i;
			}
			i=GetValueFromData(ls,"INTERVAL",',',4,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 10)
					VTSData.IntervalData.HealthInterval = i;
			}
			

			UpdateConfigInFlash();
			sprintf(SimData,"INTERVAL CHANGE %d,%d,%d,%d,%d",VTSData.IntervalData.DataInterval,VTSData.IntervalData.IgnitionInterval,VTSData.IntervalData.SOSInterval,
															VTSData.IntervalData.StandbyInterval,VTSData.IntervalData.HealthInterval);

			#else
			i=GetValueFromData(ls,"INTERVAL",' ',0,',',ss);
			if(i)
			{
				i=atoi(ss);
			//	i= (int)ss;
				if(i > 4)
					VTSData.IntervalData.MotionInterval = i;
			}
			i=GetValueFromData(ls,"INTERVAL",',',1,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 1)
					VTSData.IntervalData.HaltInterval = i;
			}
			i=GetValueFromData(ls,"INTERVAL",',',2,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 1)
					VTSData.IntervalData.EnergencyInterval = i;
			}
			i=GetValueFromData(ls,"INTERVAL",',',3,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 10)
					VTSData.IntervalData.SleepInterval = i;
			}
			i=GetValueFromData(ls,"INTERVAL",',',4,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 10)
					VTSData.IntervalData.HealthInterval = i;
			}
			i=GetValueFromData(ls,"INTERVAL",',',5,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 10)
					VTSData.IntervalData.FullDataPacketInterval = i;
			}

			UpdateConfigInFlash();
			sprintf(SimData,"INTERVAL CHANGE %d,%d,%d,%d,%d,%d",VTSData.IntervalData.MotionInterval,VTSData.IntervalData.HaltInterval,VTSData.IntervalData.EnergencyInterval,
															VTSData.IntervalData.SleepInterval,VTSData.IntervalData.HealthInterval,VTSData.IntervalData.FullDataPacketInterval);
			#endif
			SendResponce(SMSSender,SimData,OTASource,1);
			return 1;
		}
		ls=strstr(fn,"BEHAVE");
		if(ls)
		{
			i=GetValueFromData(ls,"BEHAVE",' ',0,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 4)
					VTSData.VehicleData.HarshAcc = i;
			}
			i=GetValueFromData(ls,"BEHAVE",',',1,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 4)
					VTSData.VehicleData.HarshBreak = i;
			}
			i=GetValueFromData(ls,"BEHAVE",',',2,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 4)
					VTSData.VehicleData.RashTurn = i;
			}
			i=GetValueFromData(ls,"BEHAVE",',',3,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 15)
					VTSData.VehicleData.OverSpeed = i;
			}
			UpdateConfigInFlash();
			SendGyroSettingPacket(); 

			sprintf(SimData,"BHV Updated %d,%d,%d,%3.0f",VTSData.VehicleData.HarshAcc,VTSData.VehicleData.HarshBreak,VTSData.VehicleData.RashTurn,VTSData.VehicleData.OverSpeed);
			SendResponce(SMSSender,SimData,OTASource,1);
			return 1;
		}

		ls=strstr(fn,"VEHREG");
		if(ls)
		{
			i=GetValueFromData(ls,"VEHREG",' ',0,'\0',ss);
			if(i)
			{
				strcpy(VTSData.VehicleData.VehicleRegNo,ss);
				
				
				UpdateConfigInFlash();   
				sprintf(SimData,"Vehicle Number Updated : %s",VTSData.VehicleData.VehicleRegNo);
				SendResponce(SMSSender,SimData,OTASource,1);
				return 1;
			}
		}
		ls=strstr(fn,"SOSSET");
		if(ls)
		{
			i=GetValueFromData(ls,"SOSSET",' ',0,',',ss);
			if(i)
			{
				strcpy(VTSData.PhoneNumber.Mob0,ss);
			}
			i=GetValueFromData(ls,"SOSSET",',',1,'\0',ss);
			if(i)
			{
				strcpy(VTSData.PhoneNumber.Mob1,ss);
				UpdateConfigInFlash();
				sprintf(SimData,"SOS Number Updated : %s, %s",VTSData.PhoneNumber.Mob0,VTSData.PhoneNumber.Mob1);
				SendResponce(SMSSender,SimData,OTASource,1);
				return 1;
			}
		}
		ls=strstr(fn,"FOTAUPDATE");
		if(ls)
		{
			
		}
		ls=strstr(fn,"VRESET");
		if(ls)
		{
			DeleteAllSMS();
			SendResponce(SMSSender,"Device Restarting...",OTASource,1);
			nwy_sleep(5000);
			nwy_power_off(2);//Restart
			return 1;
		}
		ls=strstr(fn,"GRESET");
		if(ls)
		{
			SendResponce(SMSSender,"GPS Resetting...",OTASource,1);
			SendGPSResetReq();
			return 1;
		}
		
		ls=strstr(fn,"DEFAULT");
		if(ls)
		{
			DeleteAllSMS();
			SendResponce(SMSSender,"Device Reverted to Default, Restarting...",OTASource,1);
			LoadDefault();
			nwy_sleep(5000);
			nwy_power_off(2);//Restart
				return 1;
		}
		ls=strstr(fn,"SOSCLR");
		if(ls)
		{
			ResetSOS();
			SendResponce(SMSSender,"SOS Clear",OTASource,1);
			return 1;
		}
		ls=strstr(fn,"SOSTIMEOUT");
		if(ls)
		{
			i=GetValueFromData(ls,"SOSTIMEOUT",' ',0,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if((i > 10) && ( i < 0xffff))
				{
					VTSData.IntervalData.SOSTimeOut =i;
					strcpy(CMD_Buff,"SOS Timeout Changed to ");
					strcat(CMD_Buff,ss);
					
					UpdateConfigInFlash();
					SendResponce(SMSSender,CMD_Buff,OTASource,1);
					return 1;
				}
			}
		}
		ls= strstr(fn,"TESTRIG");
		if(ls)
		{
			i=GetValueFromData(ls,"TESTRIG",' ',0,'\0',ss);
			if(i)
			{
				nwy_dbg_log("ss:%s",ss);
				i = atoi(ss);
				if((i>1) && (i<21))
				{
					nwy_dbg_log("activatiing alert %d",i);
					AddAlert(i);
					return 1;
				}
				if(i == 0)
				{
					SOS.IsSOS=1;
					#ifndef PROTO_CDAC
					if(ServerSocket[1].SocketState != SOCKET_CONNECTED)
						SendSOSSMS(1);
					#endif

					SLED_ON;
					SOS.SOSTimeLasped=0;
					VAlert[SOS_ON_ALERT].Enable=1;
	//				if(!LL_GPIO_IsOutputPinSet(SOS_LED_PORT,SOS_LED_PIN))
	//					SLED_ON;
					nwy_dbg_log("********************\nSOS Alert Manual ON\n***********************\n");
					AddAlert(SOS_ON_ALERT);
					#ifndef PROTO_CDAC
					VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.SOSInterval;
					#endif
				}
				else if(i == 1)
				{
					SOS.SOSTimeLasped=0;
					SOS.IsSOS=0;
					SLED_OFF;
	//				GPO1_OFF;
					nwy_dbg_log("********************\nSOS Alert Manual OFF********************\n");
					AddAlert(SOS_OFF_ALERT);
					RemoveAlert(SOS_ON_ALERT);
					#ifndef PROTO_CDAC
					VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
					#endif
				}
			}
		}
		// i want command like ZIGMODE 1 or ZIGMODE 0 to enable or disable zig testing mode which is used for manufacturing testing
		ls = strstr(fn,"ZIGMODE ");
		if(ls)
		{
			i=GetValueFromData(ls,"ZIGMODE",' ',0,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if(i == 0)
				{
					ZigTestMode = 0;
					SendResponce(SMSSender,"Zig Test Mode Disabled",OTASource,1);
				}
				else if(i == 1)
				{
					ZigTestMode = 1;
					SendResponce(SMSSender,"Zig Test Mode Enabled",OTASource,1);
				}
				else
				{
					SendResponce(SMSSender,"Invalid Param",OTASource,0);
				}
			}
			return 1;
		}
		ls = strstr(fn,"BTS ");
		if(ls)
		{
			fn = strchr(fn,'\0');
			if(fn)
			{
				if((fn - ls) > 6)
				{
					if(OTASource)
						return 0;
					SendSMS(SMSSender,"Invalid Param.");
					return 0;
				}
				else
				{
					strcpy(ss,&ls[4]);
					f=(double)atoi(ss)/10;
					if((f > 3.0F) && ( f < 4.2F))
					{
						VTSData.BattThrs =f;
					
						strcpy(CMD_Buff,"Battery Threshold Changed to ");
						sprintf(ss,"%01.1f",VTSData.BattThrs);
						strcat(CMD_Buff,ss);
						UpdateConfigInFlash();
						SendResponce(SMSSender,CMD_Buff,OTASource,1);
						return 1;
					}

					SendResponce(SMSSender,"Invalid Param",OTASource,0);
					return 0;						
					
				}
			}
		}
		ls = strstr(fn,"GF:");
		if(ls)
		{
			DecodeGeofence(ls);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"GeoFence Data Updated",OTASource,1);
			return 1;
		}
		// SET SENSINT dayIGN,nightIGN,dayOFF,nightOFF - Set sensor intervals
		// Example: SET SENSINT 10,20,30,60
		ls = strstr(fn,"SENSINT");
		if(ls)
		{
			uint8_t changed = 0;
			nwy_dbg_log("Processing SENSINT command");
			i=GetValueFromData(ls,"SENSINT",' ',0,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i >= 5 && i <= 3600)  // 5 seconds to 1 hour
				{
					SensorsConfig.intervalConfig.dayIGNITIONInterval = i;
					changed = 1;
				}
			}
			i=GetValueFromData(ls,"SENSINT",',',1,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i >= 5 && i <= 3600)
				{
					SensorsConfig.intervalConfig.nightIGNITIONInterval = i;
					changed = 1;
				}
			}
			i=GetValueFromData(ls,"SENSINT",',',2,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i >= 5 && i <= 3600)
				{
					SensorsConfig.intervalConfig.dayOFFInterval = i;
					changed = 1;
				}
			}
			i=GetValueFromData(ls,"SENSINT",',',3,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if(i >= 5 && i <= 3600)
				{
					SensorsConfig.intervalConfig.nightOFFInterval = i;
					changed = 1;
				}
			}
			
			if(changed)
			{
				SaveSensorConfigToFlash();
				sprintf(SimData,"SensInt: DayIGN=%d, NightIGN=%d, DayOFF=%d, NightOFF=%d",
					SensorsConfig.intervalConfig.dayIGNITIONInterval,
					SensorsConfig.intervalConfig.nightIGNITIONInterval,
					SensorsConfig.intervalConfig.dayOFFInterval,
					SensorsConfig.intervalConfig.nightOFFInterval);
				SendResponce(SMSSender,SimData,OTASource,1);
				nwy_dbg_log("%s", SimData);
			}
			else
			{
				SendResponce(SMSSender,"Invalid Param (5-3600)",OTASource,0);
				nwy_dbg_log("Invalid Param for SENSINT command");
			}
			return 1;
		}
		// SET SENSEN 0/1 - Enable/Disable sensors
		ls = strstr(fn,"SENSEN");
		if(ls)
		{
			i=GetValueFromData(ls,"SENSEN",' ',0,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if(i == 0 || i == 1)
				{
					SensorsConfig.isEnabled = i;
					SaveSensorConfigToFlash();
					sprintf(SimData,"Sensors %s", i ? "Enabled" : "Disabled");
					SendResponce(SMSSender,SimData,OTASource,1);
				}
				else
				{
					SendResponce(SMSSender,"Invalid Param (0/1)",OTASource,0);
				}
			}
			return 1;
		}
		// SET SENSTOUT timeout - Set sensor timeout in seconds
		ls = strstr(fn,"SENSTOUT");
		if(ls)
		{
			i=GetValueFromData(ls,"SENSTOUT",' ',0,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if(i >= 30 && i <= 3600)  // 30 seconds to 1 hour
				{
					SensorsConfig.sensorTimeout = i;
					SaveSensorConfigToFlash();
					sprintf(SimData,"Sensor Timeout: %d sec", SensorsConfig.sensorTimeout);
					SendResponce(SMSSender,SimData,OTASource,1);
				}
				else
				{
					SendResponce(SMSSender,"Invalid Param (30-3600)",OTASource,0);
				}
			}
			return 1;
		}	
	}
	fn = strstr(msg,"CLR");
	if(fn)
	{
		ls = strstr(fn,"GF");
		if(ls)
		{
			DecodeGeofence("GF");
			UpdateConfigInFlash();
			SendResponce(SMSSender,"GeoFence Data Cleared",OTASource,1);
			return 1;
		}	
	}

	return 0;
}

