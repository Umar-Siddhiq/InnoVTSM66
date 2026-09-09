#include "SMS.h"
#include "Server.h"
#include "FTP.h"
#include "EPO.h"
#include "Geofence.h"
#include "Utilities.h"
#include "TCP.h"
#include "BLE.h"
#include "ql_fs.h"
#include "Batch.h"
#include "Sensors.h"
#include "SystemRecovery.h"

/* Defined in Server.c — routes SET/GET/CLR OTA commands from RS232/RS485. */
extern void DecodeOTAData(char* buff, uint8_t isserver);
extern void EmergencyPacket(uint8_t IsOff);

// Forward declaration for RS232 response queueing (from Hardware.c)
extern void QueueRS232Response(const char* response);


//http://maps.google.com/maps?z=12&t=m&q=loc:38.9419+-78.3020

#define MAPSLINK "http://maps.google.com/maps?z=12&t=m&q=loc:"



char RcvIndex[30]={0};
char SMSData[500]={0};
char SMSSender[25]={0};
char SimData[MSGSIZE]={0};
uint8_t ACTMsg=0;

#if defined(PROTO_MAHARASHTRA1) || defined(PROTO_OG)
uint8_t IsSRCMD;
#endif

#define CMDBUFFSIZE 250
char CMD_Buff[CMDBUFFSIZE]= {0};

extern volatile uint8_t SendLogin1;  // Match Server.h declarations with volatile
extern volatile uint8_t SendLogin2;
extern volatile uint8_t SendLogin3;
uint16_t IsSMS=0;
extern uint32_t FrameNumber;

#ifdef PROTO_CDAC
extern volatile char VehicleMovingMode;
#endif

// ZigTestMode: Manufacturing test mode flag (0=disabled, 1=enabled)
uint8_t ZigTestMode = 0;

int GetValueFromData(char* data, char* cmd,char delim1, int delimPos, char delim2, char* value)
{
	char* fn;
	char* ls;
	int ln;
	if (!data || !cmd || !value)
		return 0;
	ln=Ql_strlen(cmd);
	fn=Ql_strstr(data,cmd);
	if(fn)
	{
		fn=fn+ln;
		fn++;
		if(delimPos==0)
		{
			ls=Ql_strchr(fn,delim2);
			if(ls)
			{
				ln=ls-fn;
				if (ln <= 0 || ln >= 60) return 0;
				Ql_strncpy(value,fn,ln);
				value[ln]=0;
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
			ls=Ql_strchr(fn,delim1);
			if (!ls) return 0;
			fn=ls+1;
		}
		if(ls)
		{
			ls++;
			fn=Ql_strchr(ls,delim2);
			if (!fn) return 0;
			ln=fn-ls;
			if((ln > 0) && (ln < 60))
			{
				Ql_memset(value,0x00,ln+1);
				Ql_strncpy(value,ls,ln);
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

static uint8_t ParseEpoFtpArgs(const char* tokenStart, char* ip, uint16_t* port, char* user, char* pass, char* filename)
{
	if (!tokenStart || !ip || !port || !user || !pass || !filename) {
		return 0;
	}

	const char* p = tokenStart;
	while (*p && *p != 'E') p++;
	if (Ql_strncmp(p, "EPO ", 4) != 0) {
		return 0;
	}
	p += 4;

	const char* fields[5] = {0};
	uint16_t lens[5] = {0};
	int field = 0;

	while (*p && field < 5) {
		while (*p == ' ' || *p == '\t') p++;
		fields[field] = p;
		while (*p && *p != ',' && *p != '\r' && *p != '\n') p++;
		lens[field] = (uint16_t)(p - fields[field]);
		if (*p == ',') p++;
		field++;
	}
	if (field < 5) {
		return 0;
	}

	if (lens[0] == 0 || lens[0] >= 50) return 0;
	Ql_memset(ip, 0, 50);
	Ql_strncpy(ip, fields[0], lens[0]);
	ip[lens[0]] = '\0';

	char tmp[64];
	if (lens[1] == 0 || lens[1] >= sizeof(tmp)) return 0;
	Ql_memset(tmp, 0, sizeof(tmp));
	Ql_strncpy(tmp, fields[1], lens[1]);
	tmp[lens[1]] = '\0';
	int pnum = atoi(tmp);
	if (pnum <= 0 || pnum > 65535) return 0;
	*port = (uint16_t)pnum;

	if (lens[2] == 0 || lens[2] >= 50) return 0;
	Ql_memset(user, 0, 50);
	Ql_strncpy(user, fields[2], lens[2]);
	user[lens[2]] = '\0';

	if (lens[3] == 0 || lens[3] >= 50) return 0;
	Ql_memset(pass, 0, 50);
	Ql_strncpy(pass, fields[3], lens[3]);
	pass[lens[3]] = '\0';

	if (lens[4] == 0 || lens[4] >= 80) return 0;
	Ql_memset(filename, 0, 80);
	Ql_strncpy(filename, fields[4], lens[4]);
	filename[lens[4]] = '\0';
	return 1;
}

extern void FOTAStart(void);
void LoadDefaultFOTAParams(char *Sender, uint8_t IsServer)
{
	memset((void*)&DownloadReq,0x00,sizeof(download_req_info_s));
	Ql_strncpy(DownloadReq.IP, "124.123.18.16", sizeof(DownloadReq.IP) - 1);
	DownloadReq.IP[sizeof(DownloadReq.IP) - 1] = '\0';
	Ql_strncpy(DownloadReq.User, "CCMSV1", sizeof(DownloadReq.User) - 1);
	DownloadReq.User[sizeof(DownloadReq.User) - 1] = '\0';
	Ql_strncpy(DownloadReq.Pass, "CCMS@123", sizeof(DownloadReq.Pass) - 1);
	DownloadReq.Pass[sizeof(DownloadReq.Pass) - 1] = '\0';
	Ql_strncpy(DownloadReq.Sender, Sender, sizeof(DownloadReq.Sender) - 1);
	DownloadReq.Sender[sizeof(DownloadReq.Sender) - 1] = '\0';
	Ql_strncpy(DownloadReq.FilePath, "RAM:app_fota.bin", sizeof(DownloadReq.FilePath) - 1);
	DownloadReq.FilePath[sizeof(DownloadReq.FilePath) - 1] = '\0';
	Ql_strncpy(DownloadReq.InternalFilePath, SERVER_FOTA_FILEPATH, sizeof(DownloadReq.InternalFilePath) - 1);
	DownloadReq.InternalFilePath[sizeof(DownloadReq.InternalFilePath) - 1] = '\0';
	DownloadReq.Port = 21;
	DownloadReq.IsServer = IsServer;
	DownloadReq.AttemptCount=3;
	DownloadReq.RequestType=FTP_REQ_TYPE_FOTA;
	DownloadReq.IsValid = FOTA_REQ_VALID_CODE;
	UpdateFTPConfigInFlash(&DownloadReq);
	SendResponce(Sender,"Attemping FTP for FOTA...",IsServer,1);
	FTPStart(&DownloadReq);
	ThreadSleep(3000);
}

static uint8_t IsValidSOSMobileNumber(const char *number)
{
	return number != NULL && Ql_strlen(number) > 3 &&
		!Ql_strstr(number, "0000000");
}

static void GetSOSISTDateTime(_RTC *ist)
{
	uint8_t daysInMonth;
	ist->Year = CurrentDateTime.Year;
	ist->Month = CurrentDateTime.Month;
	ist->Date = CurrentDateTime.Date;
	ist->Hour = CurrentDateTime.Hour + 5;
	ist->Min = CurrentDateTime.Min + 30;
	ist->Sec = CurrentDateTime.Sec;
	if (ist->Min >= 60) { ist->Min -= 60; ist->Hour++; }
	if (ist->Hour < 24) return;
	ist->Hour -= 24;
	ist->Date++;
	daysInMonth = (ist->Month == 4 || ist->Month == 6 || ist->Month == 9 || ist->Month == 11) ? 30 :
		(ist->Month == 2 ? (((ist->Year % 4) == 0) ? 29 : 28) : 31);
	if (ist->Date <= daysInMonth) return;
	ist->Date = 1;
	ist->Month++;
	if (ist->Month > 12) { ist->Month = 1; ist->Year++; }
}

void SendSOSAlertSMS(uint8_t AlertNum)
{
	const char *alertName;
	_RTC ist;
	char message[160];

#if !SOS_SMS_FEATURE_ENABLED
	LOGData(TAG_OTA, "SOS SMS disabled in firmware");
	return;
#endif
	if (!VTSData.SOSSmsEnabled || GSM.GSMState < SIM_DETECTED)
		return;
	if (AlertNum == 16) {
#if !SOS_WIRECUT_SMS_ENABLED
		return;
#endif
		alertName = "Emergency wirecut";
	} else if (AlertNum == 10) {
		alertName = "Emergency State ON";
	} else if (AlertNum == 11) {
		alertName = "Emergency State OFF";
	} else {
		return;
	}

	GetSOSISTDateTime(&ist);
	Ql_sprintf(message, "ID:%d %s\nIMEI:%s\nLAT:%s%c LON:%s%c\nhttp://maps.google.com/?q=%s,%s\nT:%02d-%02d-20%02d %02d:%02d:%02d IST",
		AlertNum, alertName, NetWork.IMEI, sLatitude, GPS.LatDir, sLongitude, GPS.LngDir,
		sLatitude, sLongitude, ist.Date, ist.Month, ist.Year, ist.Hour, ist.Min, ist.Sec);
	/* Notify all configured emergency numbers (Mob0–Mob4), matching the
	 * reference. Each is validated so unset/placeholder slots are skipped. */
	if (IsValidSOSMobileNumber(VTSData.PhoneNumber.Mob0))
		SendSMS(VTSData.PhoneNumber.Mob0, message);
	if (IsValidSOSMobileNumber(VTSData.PhoneNumber.Mob1))
		SendSMS(VTSData.PhoneNumber.Mob1, message);
	if (IsValidSOSMobileNumber(VTSData.PhoneNumber.Mob2))
		SendSMS(VTSData.PhoneNumber.Mob2, message);
	if (IsValidSOSMobileNumber(VTSData.PhoneNumber.Mob3))
		SendSMS(VTSData.PhoneNumber.Mob3, message);
	if (IsValidSOSMobileNumber(VTSData.PhoneNumber.Mob4))
		SendSMS(VTSData.PhoneNumber.Mob4, message);
}

uint8_t SendSMS(char* ph, char* msg)
{
    char cleanPh[25] = {0};
    uint16_t len = 0;
    uint16_t idx = 0;

    if (ph == NULL || msg == NULL || Ql_strlen(ph) == 0 || Ql_strlen(msg) == 0)
    {
        LOGData(TAG_OTA, "Invalid phone number or message.\n");
        return 0;
    }

    len = Ql_strlen(ph);
    for (uint16_t i = 0; i < len && idx < sizeof(cleanPh) - 1; i++)
    {
        char c = ph[i];
        if (c != '"' && c != '\'' && c != ' ' && c != '\r' && c != '\n')
        {
            cleanPh[idx++] = c;
        }
    }
    cleanPh[idx] = '\0';

    char formattedPh[25] = {0};
    if (Ql_strlen(cleanPh) == 10 && cleanPh[0] >= '0' && cleanPh[0] <= '9')
    {
        Ql_sprintf(formattedPh, "+91%s", cleanPh);
    }
    else
    {
        Ql_strcpy(formattedPh, cleanPh);
    }

    LOGData(TAG_OTA, "SMS Sending to %s: %s\n", formattedPh, msg);
    bool isUCS2 = false;
    return SMS_SendTextMessage(formattedPh, msg, isUCS2) ? 1 : 0;
}

uint8_t ParseFOTAPacket(char *buf, char *IP, uint16_t *port, char *User, char *pass, char *Filepath)
{
    char *p;
    int field = 0;
    int i = 0;
    char temp[128];

    if (!buf || !IP || !port || !User || !pass || !Filepath)
        return 0;

    if ((Ql_StrPrefixMatch(buf, "FOTA ") == 0) && (Ql_StrPrefixMatch(buf,"MOTA ") == 0)){
        return 0;
    }

    p = buf + 5;

    while (*p && field < 5) {
        i = 0;

        while (*p == ' ' || *p == '\t') p++;

        while (*p && *p != ',' && *p != '\n' && *p != '\r' && i < sizeof(temp) - 1) {
            temp[i++] = *p++;
        }
        temp[i] = '\0';

        if (*p == ',') p++;

        switch (field) {
            case 0: 
                Ql_strncpy(IP, temp, 64);
                IP[63] = '\0';
                break;
            case 1: 
                *port = (uint16_t)atoi(temp); 
                break;
            case 2: 
                Ql_strncpy(User, temp, 32);
                User[31] = '\0';
                break;
            case 3: 
                Ql_strncpy(pass, temp, 32);
                pass[31] = '\0';
                break;
            case 4: 
                Ql_strncpy(Filepath, temp, 128);
                Filepath[127] = '\0';
                break;
        }

        field++;
    }

    return (field == 5) ? 1 : 0;
}

//SET MOTA ip,port,id,pass,filepath
void MOTAPacket(char *buf,char *sender, uint8_t IsServer)
{
	memset((void*)&DownloadReq,0x00,sizeof(download_req_info_s));
	if(!ParseFOTAPacket(buf,DownloadReq.IP,&DownloadReq.Port,DownloadReq.User,DownloadReq.Pass,DownloadReq.FilePath))
	{
		SendResponce(sender,"MOTA INVALID COMMAND",IsServer,0);
		return;
	}
	if(!Ql_strstr(DownloadReq.FilePath,".bin"))
	{
		SendResponce(sender,"MOTA file invalid format, only bin file is supported!",IsServer,0);
		return;
	}
	Ql_strcpy(DownloadReq.Sender,sender);
	DownloadReq.IsServer=IsServer;
	DownloadReq.RequestType=FTP_REQ_TYPE_CONFIG;
	DownloadReq.AttemptCount=3;
	DownloadReq.IsValid = FOTA_REQ_VALID_CODE;
	Ql_strcpy(DownloadReq.InternalFilePath,SERVER_MOTA_FILEPATH);
	UpdateFTPConfigInFlash(&DownloadReq);
	SendResponce(sender,"Attemping FTP for MOTA...",IsServer,1);
	FTPStart(&DownloadReq);
	ThreadSleep(3000);
	//Ql_Reset(0);	
}


void FOTAPacket(char *buf,char *sender, uint8_t IsServer)
{
	LOGData(TAG_OTA,"Parsing FOTA Packet");
	memset((void*)&DownloadReq,0x00,sizeof(download_req_info_s));
	if(!ParseFOTAPacket(buf,DownloadReq.IP,&DownloadReq.Port,DownloadReq.User,DownloadReq.Pass,DownloadReq.FilePath))
	{
		SendResponce(sender,"FOTA INVALID COMMAND",IsServer,0);
		LOGData(TAG_OTA,"FOTA INVALID COMMAND");
		return;
	}
	if(!Ql_strstr(DownloadReq.FilePath,".bin"))
	{
		SendResponce(sender,"FOTA file invalid format, only bin file is supported!",IsServer,0);
		LOGData(TAG_OTA,"FOTA file invalid format, only bin file is supported!");
		return;
	}
	Ql_strcpy(DownloadReq.Sender,sender);
	DownloadReq.IsServer=IsServer;
	DownloadReq.RequestType=FTP_REQ_TYPE_FOTA;
	DownloadReq.AttemptCount=3;
	DownloadReq.IsValid = FOTA_REQ_VALID_CODE;
	Ql_strcpy(DownloadReq.InternalFilePath,SERVER_FOTA_FILEPATH);
	UpdateFTPConfigInFlash(&DownloadReq);
	SendResponce(sender,"Attemping FTP for FOTA...",IsServer,1);
	LOGData(TAG_OTA,"FOTA Packet: IP:%s, Port:%d, User:%s, Pass:%s, FilePath:%s", 
		DownloadReq.IP, DownloadReq.Port, DownloadReq.User, DownloadReq.Pass, DownloadReq.FilePath);
	FTPStart(&DownloadReq);
	ThreadSleep(3000);
	//Ql_Reset(0);
	
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
		Ql_strcpy(SimData,"$ACTVR,");
	else
		Ql_strcpy(SimData,"$HCHKR,");
	#else
	if(mode==0)
		Ql_strcpy(SimData,"ACTVR,");
	else
		Ql_strcpy(SimData,"HCHKR,");
	#endif
	Ql_strncat(SimData, code,6);
	InsertChar(SimData,',');
	Ql_strcat(SimData,VTSData.VendorID);
	InsertChar(SimData,',');
	if(FirmVer[0] == 'V')
		Ql_strcat(SimData,FirmVer);
	else
	{
		Ql_strcat(SimData,"V");
		Ql_strcat(SimData,FirmVer);
	}
	InsertChar(SimData,',');
	Ql_strncat(SimData,NetWork.IMEI,15);
	
	char alert_id_str[5];
	Ql_sprintf(alert_id_str, ",%02d,", mode + 1);
	Ql_strcat(SimData, alert_id_str);
	
	//Ql_strcat(SimData,sLatitude);
	StringAdd(SimData,"%.6f",GPS.Latitude);
	Ql_strcat(SimData,",N,");
	//Ql_strcat(SimData,sLongitude);
	StringAdd(SimData,"%.6f",GPS.Longitude);
	Ql_strcat(SimData,",E,");
	InsertChar(SimData,GPS.GPSFix + '0');
	InsertChar(SimData,',');
	Ql_sprintf(ss,"%02d%02d20%02d ",CurrentDateTime.Date,CurrentDateTime.Month,CurrentDateTime.Year);
	Ql_strcat(SimData,ss);
	//InsertChar(SimData,',');
	Ql_sprintf(ss,"%02d%02d%02d",CurrentDateTime.Hour ,CurrentDateTime.Min , CurrentDateTime.Sec);
	Ql_strcat(SimData,ss);
	InsertChar(SimData,',');
	Ql_strcat(SimData,sHeading);
	InsertChar(SimData,',');

	//Ql_strcat(SimData,sSpeed);
	StringAdd(SimData,"%03.1f",GPS.Speed);
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
	Ql_strcat(SimData,GSM.LAC);
	InsertChar(SimData,',');
	InsertChar(SimData,PeriPheralVal.IsMain + '0');
	InsertChar(SimData,',');
	InsertChar(SimData,PeriPheralVal.IGN + '0');
	InsertChar(SimData,',');
	Ql_sprintf(ss,"%04.1f",(PeriPheralVal.MainsVolt));
	Ql_strcat(SimData,ss);
	InsertChar(SimData,',');
	Ql_sprintf(ss,"%06lu",FrameNumber);
	Ql_strcat(SimData,ss);
	#ifdef PROTO_CDAC
	InsertChar(SimData,',');
	InsertChar(SimData,'0');
	InsertChar(SimData,VehicleMovingMode);
	#else
	if (PeriPheralVal.IGN)
		Ql_strcat(SimData,",IN");
	else
		Ql_strcat(SimData,",IF");
	#endif
//	SendSMS(phone,ModemCMD);
	
}


/* GEO commands are accepted by every non-CDAC protocol. */
#ifndef PROTO_CDAC
void SendGeoData(uint8_t index, uint8_t OTASource)
{
	char tmp[48];
	int j;
	Ql_memset(dataBuffer, 0x00, DATA_MAX_BUFF);
	Ql_strcat(dataBuffer, "$GFR,");
	if (VTSData.GeoLatLng[index].InOut != 0) {
		Ql_sprintf(tmp, "Geo[%d]:ID:%d,Mask:%d,", index,
		           VTSData.GeoLatLng[index].ID, VTSData.GeoLatLng[index].InOut);
		Ql_strcat(dataBuffer, tmp);
		for (j = 0; j < 10; j++) {
			if (VTSData.GeoLatLng[index].Latitude[j] != 0) {
				Ql_sprintf(tmp, "LAT[%d]:%09.6f,LNG[%d]:%010.6f,",
				           j, (float)VTSData.GeoLatLng[index].Latitude[j],
				           j, (float)VTSData.GeoLatLng[index].Longitude[j]);
				Ql_strcat(dataBuffer, tmp);
			}
		}
		{
			uint16_t len = Ql_strlen(dataBuffer);
			if (len > 0) dataBuffer[len - 1] = '*';
		}
	} else {
		Ql_strcat(dataBuffer, "NC*");
	}
	if (OTASource == OTA_SRC_SCK_1)
		TCPSocket_SendString(&ServerSocket[0], dataBuffer);
	else if (OTASource == OTA_SRC_SCK_2)
		TCPSocket_SendString(&ServerSocket[2], dataBuffer);
	#ifdef EXTENDED_IPS
	else if (OTASource == OTA_SRC_SCK_3)
		TCPSocket_SendString(&ServerSocket[3], dataBuffer);
	#endif
}
#endif

#if defined(PROTO_MAHARASHTRA1) || defined(PROTO_OG)
void SRDecode(char* msg, uint8_t IsServer)
{
	char *fn;
	char ss[40];
	int i ;
	uint8_t type = 0;;
	if(Ql_strstr(msg,"GET"))
		type = 1;
	else if (Ql_strstr(msg,"SET"))
		type = 2;
	else if (Ql_strstr(msg,"CLR"))
		type = 3;
	
	LOGData(TAG_OTA,"SR parse Type : %d",type);
	fn = Ql_strstr(msg,"GIP#");
	if(fn)
	{
		LOGData(TAG_OTA,"SR GIP cmd");
		if(type == 0)
			return;
		if(type == 1)
		{
			Ql_sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP1,VTSData.ServerData.Port1);
			SendResponce(SMSSender,"GET:GIP",IsServer,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"GIP",'#',0,',',ss);//SET:GIP#13.234.160.106,8224;
			if(!i)
				return;
			if((Ql_strlen(ss) > 49) || (Ql_strlen(ss) < 5))
				return;
			char ip[50]={0};
			Ql_strncpy(ip, ss, sizeof(ip) - 1);
			ip[sizeof(ip) - 1] = '\0';


			i = GetValueFromData(fn,"GIP",',',1,';',ss);
			if(!i)
				return;

			Ql_strncpy(VTSData.ServerData.IP1, ip, sizeof(VTSData.ServerData.IP1) - 1);
			VTSData.ServerData.IP1[sizeof(VTSData.ServerData.IP1) - 1] = '\0';
			Ql_strncpy(VTSData.ServerData.Port1, ss, sizeof(VTSData.ServerData.Port1) - 1);
			VTSData.ServerData.Port1[sizeof(VTSData.ServerData.Port1) - 1] = '\0';
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP1,VTSData.ServerData.Port1);
			SendResponce(SMSSender,"SET:GIP",IsServer,1);
			ReinitSingleSocket(0);
			return;
		}
		if(type == 3)
		{
			Ql_strcpy(VTSData.ServerData.IP1,DEFAULT_IP1);
			Ql_strcpy(VTSData.ServerData.Port1,DEFAULT_PORT1);
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:GIP",IsServer,1);
			ReinitSingleSocket(0);
			return;
		}
	}
	fn = Ql_strstr(msg,"APN#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{
			if(VTSData.AutoAPN)
				Ql_sprintf(CMD_Buff,"AUTO:%s",NetWork.APN);
			else
				Ql_sprintf(CMD_Buff,"%s",NetWork.APN);

			SendResponce(SMSSender,"GET:APN",IsServer,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"APN",'#',0,';',ss);
			if(!i)
				return;
			if((Ql_strlen(ss) > 19) || (Ql_strlen(ss) < 5))
				return;
			if(Ql_strstr(ss,"AUTO"))
			{
				VTSData.AutoAPN=1;
				Ql_sprintf(CMD_Buff,"AUTO");
			}
			else 
			{
				VTSData.AutoAPN = 0;
				Ql_strcpy(VTSData.mAPN,ss);
				Ql_sprintf(CMD_Buff,"%s",VTSData.mAPN);
			}
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:APN",IsServer,1);
			ThreadSleep(2000);
			Ql_Reset(0);
			ThreadSleep(3000);
		}
		if(type == 3)
		{
			VTSData.AutoAPN=1;
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:APN",IsServer,1);
			ThreadSleep(2000);
			Ql_Reset(0);
			ThreadSleep(3000);
		}
	}
	fn = Ql_strstr(msg,"SOS#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{
			
			Ql_sprintf(CMD_Buff,"%s",VTSData.PhoneNumber.Mob0);
			SendResponce(SMSSender,"GET:SOS",IsServer,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"SOS",'#',0,';',ss);
			if(!i)
				return;
			if((Ql_strlen(ss) > 13) || (Ql_strlen(ss) < 5))
				return;
			
			char formatted[25];
			if (Ql_strlen(ss) == 10 && ss[0] >= '0' && ss[0] <= '9')
				Ql_sprintf(formatted, "+91%s", ss);
			else
				Ql_strcpy(formatted, ss);
			Ql_strcpy(VTSData.PhoneNumber.Mob0,formatted);
			Ql_sprintf(CMD_Buff,"%s",VTSData.PhoneNumber.Mob0);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:SOS",IsServer,1);
			return;
		
		}
		if(type == 3)
		{
			Ql_strcpy(VTSData.PhoneNumber.Mob0,DEFAULT_MOB1);
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:SOS",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"EIP#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{
			Ql_sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP2,VTSData.ServerData.Port2);
			SendResponce(SMSSender,"GET:EIP",IsServer,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"EIP",'#',0,',',ss);
			if(!i)
				return;
			if((Ql_strlen(ss) > 49) || (Ql_strlen(ss) < 5))
				return;
			char ip[50]={0};
			Ql_strncpy(ip, ss, sizeof(ip) - 1);
			ip[sizeof(ip) - 1] = '\0';
			
			i = GetValueFromData(fn,"EIP",',',1,';',ss);
			if(!i)
				return;
			
			Ql_strncpy(VTSData.ServerData.IP2, ip, sizeof(VTSData.ServerData.IP2) - 1);
			VTSData.ServerData.IP2[sizeof(VTSData.ServerData.IP2) - 1] = '\0';
			Ql_strncpy(VTSData.ServerData.Port2, ss, sizeof(VTSData.ServerData.Port2) - 1);
			VTSData.ServerData.Port2[sizeof(VTSData.ServerData.Port2) - 1] = '\0';
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP2,VTSData.ServerData.Port2);
			SendResponce(SMSSender,"SET:EIP",IsServer,1);
			ReinitSingleSocket(1);
			return;
		}
		if(type == 3)
		{
			Ql_strcpy(VTSData.ServerData.IP2,DEFAULT_IP2);
			Ql_strcpy(VTSData.ServerData.Port2,DEFAULT_PORT2);
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:EIP",IsServer,1);
			ReinitSingleSocket(1);
			return;
		}
	}
	fn = Ql_strstr(msg,"PIP#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{
			Ql_sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP3,VTSData.ServerData.Port3);
			SendResponce(SMSSender,"GET:PIP",IsServer,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"PIP",'#',0,',',ss);
			if(!i)
				return;
			if((Ql_strlen(ss) > 49) || (Ql_strlen(ss) < 5))
				return;
			char ip[50]={0};
			Ql_strncpy(ip, ss, sizeof(ip) - 1);
			ip[sizeof(ip) - 1] = '\0';
			
			i = GetValueFromData(fn,"PIP",',',1,';',ss);
			if(!i)
				return;
			
			Ql_strncpy(VTSData.ServerData.IP3, ip, sizeof(VTSData.ServerData.IP3) - 1);
			VTSData.ServerData.IP3[sizeof(VTSData.ServerData.IP3) - 1] = '\0';
			Ql_strncpy(VTSData.ServerData.Port3, ss, sizeof(VTSData.ServerData.Port3) - 1);
			VTSData.ServerData.Port3[sizeof(VTSData.ServerData.Port3) - 1] = '\0';
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"%s,%s",VTSData.ServerData.IP3,VTSData.ServerData.Port3);
			SendResponce(SMSSender,"SET:PIP",IsServer,1);
			ReinitSingleSocket(2);
			return;
		}
		if(type == 3)
		{
			Ql_strcpy(VTSData.ServerData.IP3,DEFAULT_IP3);
			Ql_strcpy(VTSData.ServerData.Port3,DEFAULT_PORT3);
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:PIP",IsServer,1);
			ReinitSingleSocket(2);
			return;
		}
	}
	fn = Ql_strstr(msg,"VRN#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%s",VTSData.VehicleData.VehicleRegNo);
			SendResponce(SMSSender,"GET:VRN",IsServer,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"VRN",'#',0,';',ss);
			if(!i)
				return;
			if((Ql_strlen(ss) > 19) || (Ql_strlen(ss) < 3))
				return;
			
			Ql_strcpy(VTSData.VehicleData.VehicleRegNo,ss);
			Ql_sprintf(CMD_Buff,"%s",VTSData.VehicleData.VehicleRegNo);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:VRN",IsServer,1);
			return;
		
		}
		if(type == 3)
		{
			Ql_strcpy(VTSData.VehicleData.VehicleRegNo,DEFAULT_VEHREG);
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:VRN",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"LOGS#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%d",VTSData.IntervalData.IgnitionInterval);
			SendResponce(SMSSender,"GET:LOGS",IsServer,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"LOGS",'#',0,';',ss);
			if(!i)
				return;
			int val = atoi(ss);
			if(val < 1 || val > 65535)
				return;
			VTSData.IntervalData.IgnitionInterval = val;
			Ql_sprintf(CMD_Buff,"%d",VTSData.IntervalData.IgnitionInterval);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:LOGS",IsServer,1);
			return;
		
		}
		if(type == 3)
		{
			VTSData.IntervalData.IgnitionInterval = DEFAULT_INV_IGN;
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:LOGS",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"LOG2#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%d",VTSData.IntervalData.DataInterval);
			SendResponce(SMSSender,"GET:LOG2",IsServer,0);
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
			Ql_sprintf(CMD_Buff,"%d",VTSData.IntervalData.DataInterval);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:LOG2",IsServer,1);
			return;
		
		}
		if(type == 3)
		{
			VTSData.IntervalData.DataInterval = DEFAULT_INV_DATA;
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:LOG2",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"HPTI#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%d",VTSData.IntervalData.HealthInterval);
			SendResponce(SMSSender,"GET:HPTI",IsServer,0);
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
			Ql_sprintf(CMD_Buff,"%d",VTSData.IntervalData.HealthInterval);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:HPTI",IsServer,1);
			return;
		
		}
		if(type == 3)
		{
			VTSData.IntervalData.HealthInterval = DEFAULT_INV_HEALTH;
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:HPTI",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"EPTI#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%d",VTSData.IntervalData.SOSInterval);
			SendResponce(SMSSender,"GET:EPTI",IsServer,0);
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
			Ql_sprintf(CMD_Buff,"%d",VTSData.IntervalData.SOSInterval);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:EPTI",IsServer,1);
			return;
		
		}
		if(type == 3)
		{
			VTSData.IntervalData.SOSInterval = DEFAULT_INV_SOS;
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:EPTI",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"EMTD#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%d",VTSData.IntervalData.SOSTimeOut);
			SendResponce(SMSSender,"GET:EMTD",IsServer,0);
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
			SetSOSTimeOutSeconds((uint16_t)val);
			Ql_sprintf(CMD_Buff,"%d",VTSData.IntervalData.SOSTimeOut);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:EMTD",IsServer,1);
			return;
		
		}
		if(type == 3)
		{
			SetSOSTimeOutSeconds(DEFAULT_INV_STM);
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:EMTD",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"IMON#");
	if(fn)
	{
		if(type == 0)
		{
			OP1_Set(1);
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"IMON",IsServer,1);
		}
		return;
	}
	fn = Ql_strstr(msg,"IMOFF#");
	if(fn)
	{
		if(type == 0)
		{
			OP1_Set(0);
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"IMOFF",IsServer,1);
		}
		return;
	}
	fn = Ql_strstr(msg,"RST#");
	if(fn)
	{
		if(type == 0)
		{
			if(fn[4] == '1')
				Ql_sprintf(CMD_Buff,"1");
			else
				Ql_sprintf(CMD_Buff,"0");
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"RST",IsServer,1);
			ThreadSleep(1500);
			Ql_Reset(0);
			ThreadSleep(3000);
		}
		return;
	}
	fn = Ql_strstr(msg,"FOTA#");
	if(fn)
	{
		if(type == 0)
		{
			i =GetValueFromData(fn,"FOTA",'#',0,';',ss);
			if(!i)
				return;
			if((Ql_strlen(ss) > 40) || (Ql_strlen(ss) < 3))
				return;
			
			memset((void*)&DownloadReq,0x00,sizeof(download_req_info_s));
			Ql_strcpy(DownloadReq.IP,VTSData.ServerData.IP3);
			Ql_strcpy(DownloadReq.User,"CCMSV1");
			Ql_strcpy(DownloadReq.Pass,"CCMS@123");
			Ql_strcpy(DownloadReq.Sender,SMSSender);
			Ql_strcpy(DownloadReq.FilePath,ss);
			Ql_strcpy(DownloadReq.InternalFilePath,SERVER_FOTA_FILEPATH);
			DownloadReq.Port = 21;
			DownloadReq.IsServer = IsServer;
			DownloadReq.AttemptCount=3;
			DownloadReq.RequestType=FTP_REQ_TYPE_FOTA;
			DownloadReq.IsValid = FOTA_REQ_VALID_CODE;
			UpdateFTPConfigInFlash(&DownloadReq);

			Ql_sprintf(CMD_Buff,"%s",ss);
			SendResponce(SMSSender,"FOTA",IsServer,1);
			FTPStart(&DownloadReq);
			ThreadSleep(3000);

		}
		return;
	}
	fn = Ql_strstr(msg,"OSL#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%2.0f",VTSData.VehicleData.OverSpeed);
			SendResponce(SMSSender,"GET:OSL",IsServer,0);
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
			Ql_sprintf(CMD_Buff,"%2.0f",VTSData.VehicleData.OverSpeed);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:OSL",IsServer,1);
			return;
		
		}
		if(type == 3)
		{
			VTSData.VehicleData.OverSpeed = DEFAULT_OVERSPEED;
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:OSL",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"HBT#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%d",VTSData.VehicleData.HarshBreak);
			SendResponce(SMSSender,"GET:HBT",IsServer,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"HBT",'#',0,';',ss);
			if(!i)
				return;
			int val = atoi(ss);
			if(val < 1 || val > MAX_UINT16)
				return;
			VTSData.VehicleData.HarshBreak = val;
			Ql_sprintf(CMD_Buff,"%d",VTSData.VehicleData.HarshBreak);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:HBT",IsServer,1);
			return;
		}
		if(type == 3)
		{
			VTSData.VehicleData.HarshBreak = DEFAULT_HB;
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:HBT",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"HAT#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%d",VTSData.VehicleData.HarshAcc);
			SendResponce(SMSSender,"GET:HAT",IsServer,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"HAT",'#',0,';',ss);
			if(!i)
				return;
			int val = atoi(ss);
			if(val < 1 || val > MAX_UINT16)
				return;
			VTSData.VehicleData.HarshAcc = val;
			Ql_sprintf(CMD_Buff,"%d",VTSData.VehicleData.HarshAcc);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:HAT",IsServer,1);
			return;
		}
		if(type == 3)
		{
			VTSData.VehicleData.HarshAcc = DEFAULT_HA;
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:HAT",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"RTT#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%d",VTSData.VehicleData.RashTurn);
			SendResponce(SMSSender,"GET:RTT",IsServer,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"RTT",'#',0,';',ss);
			if(!i)
				return;
			int val = atoi(ss);
			if(val < 1 || val > MAX_UINT16)
				return;
			VTSData.VehicleData.RashTurn = val;
			Ql_sprintf(CMD_Buff,"%d",VTSData.VehicleData.RashTurn);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:RTT",IsServer,1);
			return;
		}
		if(type == 3)
		{
			VTSData.VehicleData.RashTurn = DEFAULT_RT;
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:RTT",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"OVT#");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%2.0f",VTSData.VehicleData.OverSpeed);
			SendResponce(SMSSender,"GET:OVT",IsServer,0);
			return;
		}
		if(type == 2)
		{
			i = GetValueFromData(fn,"OVT",'#',0,';',ss);
			if(!i)
				return;
			int val = atoi(ss);
			if(val < 1 || val > MAX_UINT16)
				return;
			VTSData.VehicleData.OverSpeed = val;
			Ql_sprintf(CMD_Buff,"%2.0f",VTSData.VehicleData.OverSpeed);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"SET:OVT",IsServer,1);
			return;
		}
		if(type == 3)
		{
			VTSData.VehicleData.OverSpeed = DEFAULT_OVERSPEED;
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:OVT",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"SOS");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{	
			Ql_sprintf(CMD_Buff,"%d",SOS.IsSOS);
			SendResponce(SMSSender,"GET:SOS",IsServer,0);
			return;
		}
		if(type == 3)
		{
			ResetSOS();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:SOS",IsServer,1);
			return;
		}
	}
	fn = Ql_strstr(msg,"INFO");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{
			memset(CMD_Buff,0,CMDBUFFSIZE);
			// Format: IMEI,ICCID,IMSI,VehicleNumber,SPN,APN,Mob0,Mob1
			Ql_sprintf(CMD_Buff,"%s,",NetWork.IMEI);
			Ql_strcat(CMD_Buff,NetWork.SIMNo);
			Ql_strcat(CMD_Buff,",");
			Ql_strcat(CMD_Buff,NetWork.IMSI);
			Ql_strcat(CMD_Buff,",");
			Ql_strcat(CMD_Buff,VTSData.VehicleData.VehicleRegNo);
			Ql_strcat(CMD_Buff,",");
			Ql_strcat(CMD_Buff,NetWork.Network);
			Ql_strcat(CMD_Buff,",");
			Ql_strcat(CMD_Buff,NetWork.APN);
			Ql_strcat(CMD_Buff,",");
			Ql_strcat(CMD_Buff,VTSData.PhoneNumber.Mob0);
			Ql_strcat(CMD_Buff,",");
			Ql_strcat(CMD_Buff,VTSData.PhoneNumber.Mob1);
			SendResponce(SMSSender,"GET:INFO",IsServer,0);
			return;
		}
	}
	fn = Ql_strstr(msg,"LOCATION");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 1)
		{
			memset(CMD_Buff,0,CMDBUFFSIZE);
			// Format: Latitude,Longitude
			Ql_sprintf(CMD_Buff,"%.6f,%.6f",GPS.Latitude,GPS.Longitude);
			SendResponce(SMSSender,"GET:LOCATION",IsServer,0);
			return;
		}
	}
	fn = Ql_strstr(msg,"RESET");
	if(fn)
	{
		if(type == 0)
			return;
		if(type == 2)
		{
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"SET:RESET",IsServer,1);
			Ql_Reset(0);
			return;
		}
	}
	fn = Ql_strstr(msg,"PGF#");
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

				Ql_sprintf(ss,"%03.6f,%03.6f,",VTSData.GeoLatLng[0].Latitude[j],VTSData.GeoLatLng[0].Longitude[j]);
				Ql_strcat(CMD_Buff,ss);
			}
			CMD_Buff[Ql_strlen(CMD_Buff)-1] = '\0';
			SendResponce(SMSSender,"GET:PGF",IsServer,0);
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

					Ql_sprintf(ss,"%03.6f,%03.6f,",VTSData.GeoLatLng[0].Latitude[j],VTSData.GeoLatLng[0].Longitude[j]);
					Ql_strcat(CMD_Buff,ss);
				}
				CMD_Buff[Ql_strlen(CMD_Buff)-1] = '\0';
				SendResponce(SMSSender,"SET:PGF",IsServer,0);
			}
		}
		if(type == 3)
		{
			ClearGeofence();
			UpdateConfigInFlash();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"CLR:PGF",IsServer,1);
		}
	}
	fn = Ql_strstr(msg,"EPSTOP#");
	if(fn)
	{
		if(type==0)
		{
			ResetSOS();
			Ql_sprintf(CMD_Buff,"1");
			SendResponce(SMSSender,"EPSTOP",IsServer,1);
		}
		
		return;
	}


}
#endif

#ifdef PROTO_OG
/* AMD3 sect8+9 - inbound OTA command parser
   Frame: $<IMEI>,<PWD>,<MODE>,<CMD_ID>[:<VALUE>],...*<XX>\r\n */
uint8_t AIS140SocketReinitPending = 0;

/* A server-IP/port command must not close the TCP session that delivered it
 * before the required OA,12 result can be returned.  Non-TCP origins do not
 * need that protection and can reconnect immediately. */
static void AIS140ReinitSocketsAfterReply(uint8_t src)
{
    if (src >= OTA_SRC_SCK_1 && src <= OTA_SRC_SCK_4)
        AIS140SocketReinitPending = 1;
    else
        InitSockets();
}

uint8_t AIS140ResetPending = 0;
char AIS140ResetReason[32] = {0};

void ParseStandardAIS140Command(const char* raw, uint8_t src)
{
    char buf[300];
    char cmd_imei[20]  = {0};
    char cmd_pwd[10]   = {0};
    char mode_str[8]   = {0};
    char cmd_id[8]     = {0};
    char value[64]     = {0};
    const char* p;
    const char* star_pos;
    uint8_t mode;
    int cmd_num;
    uint8_t status = 0;
    uint8_t item_count = 0;
    uint8_t overall_status = 1;
    uint8_t reset_type = 0;

    Ql_strncpy(buf, raw, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    p = (buf[0] == '$') ? buf + 1 : buf;

    star_pos = Ql_strchr(p, '*');
    if (!star_pos) { LOGData(TAG_OTA, "AMD3: no * in frame"); return; }

    /* Layer 3 - XOR checksum (soft warning only, command still executes) */
    {
        uint8_t computed = GetXORChecksum(p, (int)(star_pos - p));
        uint8_t received = (uint8_t)strtol(star_pos + 1, NULL, 16);
        if (computed != received)
            LOGData(TAG_OTA, "AMD3 chksum WARN: exp=%02X got=%02X", computed, received);
    }

    /* field 1: IMEI */
    {
        const char* comma = Ql_strchr(p, ',');
        if (!comma) { LOGData(TAG_OTA, "AMD3: parse err IMEI"); return; }
        int l = (int)(comma - p); if (l >= 20) l = 19;
        Ql_strncpy(cmd_imei, p, l); cmd_imei[l] = '\0';
        p = comma + 1;
    }

    /* Layer 1 - IMEI check */
    if (Ql_strcmp(cmd_imei, NetWork.IMEI) != 0)
    {
        LOGData(TAG_OTA, "AMD3 IMEI mismatch: got=%s exp=%s", cmd_imei, NetWork.IMEI);
        return;
    }

    /* field 2: PASSWORD */
    {
        const char* comma = Ql_strchr(p, ',');
        if (!comma) { LOGData(TAG_OTA, "AMD3: parse err PWD"); return; }
        int l = (int)(comma - p); if (l >= 10) l = 9;
        Ql_strncpy(cmd_pwd, p, l); cmd_pwd[l] = '\0';
        p = comma + 1;
    }

    /* Layer 2 - password = last 6 digits of IMEI */
    {
        char expected[7];
        int imei_len = Ql_strlen(NetWork.IMEI);
        Ql_strncpy(expected, NetWork.IMEI + imei_len - 6, 6);
        expected[6] = '\0';
        if (Ql_strcmp(cmd_pwd, expected) != 0)
        {
            LOGData(TAG_OTA, "AMD3 PWD mismatch");
            return;
        }
    }

    /* field 3: MODE */
    {
        const char* comma = Ql_strchr(p, ',');
        if (!comma) { LOGData(TAG_OTA, "AMD3: parse err MODE"); return; }
        int l = (int)(comma - p); if (l >= 8) l = 7;
        Ql_strncpy(mode_str, p, l); mode_str[l] = '\0';
        p = comma + 1;
    }

    if      (Ql_strcmp(mode_str, "GET") == 0) mode = 1;
    else if (Ql_strcmp(mode_str, "SET") == 0) mode = 2;
    else if (Ql_strcmp(mode_str, "CLR") == 0) mode = 3;
    else { LOGData(TAG_OTA, "AMD3: unknown mode %s", mode_str); return; }

    /* map source code to string */
    {
        const char* src_str = "SCK1";
        if      (src == OTA_SRC_SMS)   src_str = "SMS";
        else if (src == OTA_SRC_SCK_1) src_str = "SCK1";
        else if (src == OTA_SRC_SCK_2) src_str = "SCK2";
        else if (src == OTA_SRC_SCK_3) src_str = "SCK3";
        else if (src == OTA_SRC_SCK_4) src_str = "SCK4";
        else if (src == OTA_SRC_BLE)   src_str = "BLE";
        else if (src == OTA_SRC_RS232) src_str = "RS232";
        else if (src == OTA_SRC_RS485) src_str = "RS485";
        Ql_strncpy(LastOTAResponse.Source, src_str, sizeof(LastOTAResponse.Source) - 1);
    }
    Ql_strncpy(LastOTAResponse.Mode, mode_str, sizeof(LastOTAResponse.Mode) - 1);

    LastOTAResponse.CmdId[0] = '\0';
    LastOTAResponse.Value[0] = '\0';

    /* parse one or more CMD_ID[:VALUE] pairs separated by commas */
    while (*p && p < star_pos)
    {
        const char* colon;
        const char* next_comma;
        int id_len;
        char cur_val[64] = {0};

        next_comma = Ql_strchr(p, ',');
        if (!next_comma || next_comma > star_pos)
            next_comma = star_pos;

        colon = Ql_strchr(p, ':');
        if (colon && colon < next_comma)
        {
            id_len = (int)(colon - p); if (id_len >= 8) id_len = 7;
            Ql_strncpy(cmd_id, p, id_len); cmd_id[id_len] = '\0';
            int vl = (int)(next_comma - (colon + 1)); if (vl >= 64) vl = 63;
            Ql_strncpy(value, colon + 1, vl); value[vl] = '\0';
        }
        else
        {
            id_len = (int)(next_comma - p); if (id_len >= 8) id_len = 7;
            Ql_strncpy(cmd_id, p, id_len); cmd_id[id_len] = '\0';
            value[0] = '\0';
        }

        /* strip trailing CR LF from value */
        {
            int vl = Ql_strlen(value);
            while (vl > 0 && (value[vl-1] == '\r' || value[vl-1] == '\n'))
                value[--vl] = '\0';
        }

        cmd_num = (int)strtol(cmd_id, NULL, 10);
        status = 0;

        LOGData(TAG_OTA, "AMD3 cmd=%d mode=%d val=%s src=%d", cmd_num, mode, value, src);

        switch (cmd_num)
        {
        case 1: /* Firmware version - GET only */
            if (mode == 1) {
                Ql_strncpy(cur_val, FirmVer, sizeof(cur_val)-1);
                status = 1;
            }
            break;

        case 2: /* PVT server IP */
            if (mode == 1) {
                Ql_strncpy(cur_val, VTSData.ServerData.IP1, sizeof(cur_val)-1);
                status = 1;
            } else if (mode == 2) {
                int vl = Ql_strlen(value);
                if (vl >= 5 && vl <= 49) {
                    Ql_strncpy(VTSData.ServerData.IP1, value, sizeof(VTSData.ServerData.IP1)-1);
                    UpdateConfigInFlash(); AIS140ReinitSocketsAfterReply(src);
                    Ql_strncpy(cur_val, value, sizeof(cur_val)-1);
                    status = 1;
                }
            } else if (mode == 3) {
                Ql_strncpy(VTSData.ServerData.IP1, DEFAULT_IP1, sizeof(VTSData.ServerData.IP1)-1);
                UpdateConfigInFlash(); AIS140ReinitSocketsAfterReply(src);
                Ql_strncpy(cur_val, VTSData.ServerData.IP1, sizeof(cur_val)-1);
                status = 1;
            }
            break;

        case 3: /* PVT server port */
            if (mode == 1) {
                Ql_strncpy(cur_val, VTSData.ServerData.Port1, sizeof(cur_val)-1);
                status = 1;
            } else if (mode == 2) {
                int vl = Ql_strlen(value);
                if (vl >= 2 && vl <= 9) {
                    Ql_strncpy(VTSData.ServerData.Port1, value, sizeof(VTSData.ServerData.Port1)-1);
                    UpdateConfigInFlash(); AIS140ReinitSocketsAfterReply(src);
                    Ql_strncpy(cur_val, value, sizeof(cur_val)-1);
                    status = 1;
                }
            } else if (mode == 3) {
                Ql_strncpy(VTSData.ServerData.Port1, DEFAULT_PORT1, sizeof(VTSData.ServerData.Port1)-1);
                UpdateConfigInFlash(); AIS140ReinitSocketsAfterReply(src);
                Ql_strncpy(cur_val, VTSData.ServerData.Port1, sizeof(cur_val)-1);
                status = 1;
            }
            break;

        case 4: /* Emergency server IP */
            if (mode == 1) {
                Ql_strncpy(cur_val, VTSData.ServerData.IP2, sizeof(cur_val)-1);
                status = 1;
            } else if (mode == 2) {
                int vl = Ql_strlen(value);
                if (vl >= 5 && vl <= 49) {
                    Ql_strncpy(VTSData.ServerData.IP2, value, sizeof(VTSData.ServerData.IP2)-1);
                    UpdateConfigInFlash(); AIS140ReinitSocketsAfterReply(src);
                    Ql_strncpy(cur_val, value, sizeof(cur_val)-1);
                    status = 1;
                }
            } else if (mode == 3) {
                Ql_strncpy(VTSData.ServerData.IP2, DEFAULT_IP2, sizeof(VTSData.ServerData.IP2)-1);
                UpdateConfigInFlash(); AIS140ReinitSocketsAfterReply(src);
                Ql_strncpy(cur_val, VTSData.ServerData.IP2, sizeof(cur_val)-1);
                status = 1;
            }
            break;

        case 5: /* Emergency server port */
            if (mode == 1) {
                Ql_strncpy(cur_val, VTSData.ServerData.Port2, sizeof(cur_val)-1);
                status = 1;
            } else if (mode == 2) {
                int vl = Ql_strlen(value);
                if (vl >= 2 && vl <= 9) {
                    Ql_strncpy(VTSData.ServerData.Port2, value, sizeof(VTSData.ServerData.Port2)-1);
                    UpdateConfigInFlash(); AIS140ReinitSocketsAfterReply(src);
                    Ql_strncpy(cur_val, value, sizeof(cur_val)-1);
                    status = 1;
                }
            } else if (mode == 3) {
                Ql_strncpy(VTSData.ServerData.Port2, DEFAULT_PORT2, sizeof(VTSData.ServerData.Port2)-1);
                UpdateConfigInFlash(); AIS140ReinitSocketsAfterReply(src);
                Ql_strncpy(cur_val, VTSData.ServerData.Port2, sizeof(cur_val)-1);
                status = 1;
            }
            break;

        case 6: /* Control centre number Mob0 */
            if (mode == 1) {
                Ql_strncpy(cur_val, VTSData.PhoneNumber.Mob0, sizeof(cur_val)-1);
                status = 1;
            } else if (mode == 2) {
                int vl = Ql_strlen(value);
                if (vl >= 6 && vl <= 14) {
                    Ql_strncpy(VTSData.PhoneNumber.Mob0, value, sizeof(VTSData.PhoneNumber.Mob0)-1);
                    UpdateConfigInFlash();
                    Ql_strncpy(cur_val, value, sizeof(cur_val)-1);
                    status = 1;
                }
            } else if (mode == 3) {
                Ql_strncpy(VTSData.PhoneNumber.Mob0, DEFAULT_MOB0, sizeof(VTSData.PhoneNumber.Mob0)-1);
                UpdateConfigInFlash();
                Ql_strncpy(cur_val, VTSData.PhoneNumber.Mob0, sizeof(cur_val)-1);
                status = 1;
            }
            break;

        case 7: /* APN */
            if (mode == 1) {
                if (VTSData.AutoAPN)
                    Ql_sprintf(cur_val, "AUTO:%s", NetWork.APN);
                else
                    Ql_strncpy(cur_val, VTSData.mAPN, sizeof(cur_val)-1);
                status = 1;
            } else if (mode == 2) {
                if (Ql_strstr(value, "AUTO") || Ql_strstr(value, "auto")) {
                    VTSData.AutoAPN = 1;
                    UpdateConfigInFlash();
                    Ql_sprintf(cur_val, "AUTO:%s", NetWork.APN);
                    status = 1;
                    reset_type = 1;
                } else {
                    int vl = Ql_strlen(value);
                    if (vl >= 3 && vl <= 29) {
                        VTSData.AutoAPN = 0;
                        Ql_strncpy(VTSData.mAPN, value, sizeof(VTSData.mAPN)-1);
                        Ql_strncpy(cur_val, value, sizeof(cur_val)-1);
                        UpdateConfigInFlash();
                        status = 1;
                        reset_type = 1;
                    }
                }
            } else if (mode == 3) {
                VTSData.AutoAPN = 1;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "AUTO:%s", NetWork.APN);
                status = 1;
                reset_type = 1;
            }
            break;

        case 8: /* Sleep/Data interval */
            if (mode == 1) {
                Ql_sprintf(cur_val, "%d", VTSData.IntervalData.DataInterval);
                status = 1;
            } else if (mode == 2) {
                int v = (int)Ql_atoi(value);
                if (v >= 5 && v <= 3600) {
                    VTSData.IntervalData.DataInterval = (uint16_t)v;
                    UpdateConfigInFlash();
                    Ql_sprintf(cur_val, "%d", v); status = 1;
                }
            } else if (mode == 3) {
                VTSData.IntervalData.DataInterval = DEFAULT_INV_STB;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", VTSData.IntervalData.DataInterval);
                status = 1;
            }
            break;

        case 9: /* Overspeed limit */
            if (mode == 1) {
                Ql_sprintf(cur_val, "%d", (int)VTSData.VehicleData.OverSpeed);
                status = 1;
            } else if (mode == 2) {
                int v = (int)Ql_atoi(value);
                if (v >= 11 && v <= 199) {
                    VTSData.VehicleData.OverSpeed = (uint16_t)v;
                    UpdateConfigInFlash();
                    Ql_sprintf(cur_val, "%d", v); status = 1;
                }
            } else if (mode == 3) {
                VTSData.VehicleData.OverSpeed = DEFAULT_OVERSPEED;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", (int)VTSData.VehicleData.OverSpeed);
                status = 1;
            }
            break;

        case 10: /* Harsh acceleration threshold */
            if (mode == 1) {
                Ql_sprintf(cur_val, "%d", VTSData.VehicleData.HarshAcc);
                status = 1;
            } else if (mode == 2) {
                int v = (int)Ql_atoi(value);
                VTSData.VehicleData.HarshAcc = (uint16_t)v;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", v); status = 1;
            } else if (mode == 3) {
                VTSData.VehicleData.HarshAcc = DEFAULT_HA;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", VTSData.VehicleData.HarshAcc);
                status = 1;
            }
            break;

        case 11: /* Harsh braking threshold */
            if (mode == 1) {
                Ql_sprintf(cur_val, "%d", VTSData.VehicleData.HarshBreak);
                status = 1;
            } else if (mode == 2) {
                int v = (int)Ql_atoi(value);
                VTSData.VehicleData.HarshBreak = (uint16_t)v;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", v); status = 1;
            } else if (mode == 3) {
                VTSData.VehicleData.HarshBreak = DEFAULT_HB;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", VTSData.VehicleData.HarshBreak);
                status = 1;
            }
            break;

        case 12: /* Harsh cornering threshold */
            if (mode == 1) {
                Ql_sprintf(cur_val, "%d", VTSData.VehicleData.RashTurn);
                status = 1;
            } else if (mode == 2) {
                int v = (int)Ql_atoi(value);
                VTSData.VehicleData.RashTurn = (uint16_t)v;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", v); status = 1;
            } else if (mode == 3) {
                VTSData.VehicleData.RashTurn = DEFAULT_RT;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", VTSData.VehicleData.RashTurn);
                status = 1;
            }
            break;

        case 13: /* Vehicle registration number */
            if (mode == 1) {
                Ql_strncpy(cur_val, VTSData.VehicleData.VehicleRegNo, sizeof(cur_val)-1);
                status = 1;
            } else if (mode == 2) {
                int vl = Ql_strlen(value);
                if (vl >= 3 && vl <= 19) {
                    Ql_strncpy(VTSData.VehicleData.VehicleRegNo, value, sizeof(VTSData.VehicleData.VehicleRegNo)-1);
                    UpdateConfigInFlash();
                    Ql_strncpy(cur_val, value, sizeof(cur_val)-1);
                    status = 1;
                }
            } else if (mode == 3) {
                Ql_strncpy(VTSData.VehicleData.VehicleRegNo, DEFAULT_VEHREG, sizeof(VTSData.VehicleData.VehicleRegNo)-1);
                UpdateConfigInFlash();
                Ql_strncpy(cur_val, VTSData.VehicleData.VehicleRegNo, sizeof(cur_val)-1);
                status = 1;
            }
            break;

        case 14: /* Ignition ON interval */
            if (mode == 1) {
                Ql_sprintf(cur_val, "%d", VTSData.IntervalData.IgnitionInterval);
                status = 1;
            } else if (mode == 2) {
                int v = (int)Ql_atoi(value);
                if (v >= 5 && v <= 3600) {
                    VTSData.IntervalData.IgnitionInterval = (uint16_t)v;
                    UpdateConfigInFlash();
                    Ql_sprintf(cur_val, "%d", v); status = 1;
                }
            } else if (mode == 3) {
                VTSData.IntervalData.IgnitionInterval = DEFAULT_INV_IGN;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", VTSData.IntervalData.IgnitionInterval);
                status = 1;
            }
            break;

        case 15: /* Ignition OFF / data interval */
            if (mode == 1) {
                Ql_sprintf(cur_val, "%d", VTSData.IntervalData.DataInterval);
                status = 1;
            } else if (mode == 2) {
                int v = (int)Ql_atoi(value);
                if (v >= 5 && v <= 3600) {
                    VTSData.IntervalData.DataInterval = (uint16_t)v;
                    UpdateConfigInFlash();
                    Ql_sprintf(cur_val, "%d", v); status = 1;
                }
            } else if (mode == 3) {
                VTSData.IntervalData.DataInterval = DEFAULT_INV_STB;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", VTSData.IntervalData.DataInterval);
                status = 1;
            }
            break;

        case 16: /* Emergency SOS interval */
            if (mode == 1) {
                Ql_sprintf(cur_val, "%d", VTSData.IntervalData.SOSInterval);
                status = 1;
            } else if (mode == 2) {
                int v = (int)Ql_atoi(value);
                if (v >= 1 && v <= 300) {
                    VTSData.IntervalData.SOSInterval = (uint16_t)v;
                    UpdateConfigInFlash();
                    Ql_sprintf(cur_val, "%d", v); status = 1;
                }
            } else if (mode == 3) {
                VTSData.IntervalData.SOSInterval = DEFAULT_INV_SOS;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", VTSData.IntervalData.SOSInterval);
                status = 1;
            }
            break;

        case 17: /* Emergency auto-clear timeout */
            if (mode == 1) {
                Ql_sprintf(cur_val, "%d", VTSData.IntervalData.SOSTimeOut);
                status = 1;
            } else if (mode == 2) {
                int v = (int)Ql_atoi(value);
                if (v >= 0 && v <= 3600) {
                    VTSData.IntervalData.SOSTimeOut = (uint16_t)v;
                    UpdateConfigInFlash();
                    Ql_sprintf(cur_val, "%d", v); status = 1;
                }
            } else if (mode == 3) {
                VTSData.IntervalData.SOSTimeOut = 0;
                UpdateConfigInFlash();
                Ql_sprintf(cur_val, "%d", VTSData.IntervalData.SOSTimeOut);
                status = 1;
            }
            break;

        case 18: /* Device restart - SET only */
            if (mode == 2) {
                Ql_strncpy(cur_val, "1", sizeof(cur_val)-1);
                status = 1;
                reset_type = 2;
            }
            break;

        case 19: /* IMEI - GET only */
            if (mode == 1) {
                Ql_strncpy(cur_val, NetWork.IMEI, sizeof(cur_val)-1);
                status = 1;
            }
            break;

        case 20: /* Clear emergency SOS - CLR only */
            if (mode == 3) {
                EmergencyPacket(0);
                ResetSOS();
                Ql_strncpy(cur_val, "0", sizeof(cur_val)-1);
                status = 1;
            }
            break;

        case 21: /* Emergency SMS centre number Mob1 */
            if (mode == 1) {
                Ql_strncpy(cur_val, VTSData.PhoneNumber.Mob1, sizeof(cur_val)-1);
                status = 1;
            } else if (mode == 2) {
                int vl = Ql_strlen(value);
                if (vl >= 7 && vl <= 13) {
                    Ql_strncpy(VTSData.PhoneNumber.Mob1, value, sizeof(VTSData.PhoneNumber.Mob1)-1);
                    UpdateConfigInFlash();
                    Ql_strncpy(cur_val, value, sizeof(cur_val)-1);
                    status = 1;
                }
            } else if (mode == 3) {
                Ql_strncpy(VTSData.PhoneNumber.Mob1, DEFAULT_MOB1, sizeof(VTSData.PhoneNumber.Mob1)-1);
                UpdateConfigInFlash();
                Ql_strncpy(cur_val, VTSData.PhoneNumber.Mob1, sizeof(cur_val)-1);
                status = 1;
            }
            break;

        case 22: /* Signal strength - GET only */
            if (mode == 1) {
                Ql_sprintf(cur_val, "%d", GSM.SignalStrength);
                status = 1;
            }
            break;

        case 23: /* RFID tag - GET only */
            if (mode == 1) {
                uint8_t found = 0;
                uint8_t ri;
                for (ri = 0; ri < MAX_SENSORS; ri++) {
                    if (SensorData[ri].SensorType == SENSOR_TYPE_RFID && SensorData[ri].IsActive) {
                        Ql_strncpy(cur_val, (const char*)SensorData[ri].SensorData, sizeof(cur_val)-1);
                        found = 1; break;
                    }
                }
                if (!found)
                    Ql_strncpy(cur_val, "NA", sizeof(cur_val)-1);
                status = 1;
            }
            break;

        default:
            LOGData(TAG_OTA, "AMD3: unknown cmd %d", cmd_num);
            break;
        }

        if (status == 0)
            overall_status = 0;

        if (item_count > 0)
        {
            if (Ql_strlen(LastOTAResponse.CmdId) + 1 < sizeof(LastOTAResponse.CmdId))
                Ql_strcat(LastOTAResponse.CmdId, ",");
            if (Ql_strlen(LastOTAResponse.Value) + 1 < sizeof(LastOTAResponse.Value))
                Ql_strcat(LastOTAResponse.Value, ",");
        }

        if (Ql_strlen(LastOTAResponse.CmdId) + Ql_strlen(cmd_id) < sizeof(LastOTAResponse.CmdId))
            Ql_strcat(LastOTAResponse.CmdId, cmd_id);

        const char* val_to_append = (cur_val[0] != '\0') ? cur_val : "NA";
        if (Ql_strlen(LastOTAResponse.Value) + Ql_strlen(val_to_append) < sizeof(LastOTAResponse.Value))
            Ql_strcat(LastOTAResponse.Value, val_to_append);

        item_count++;

        if (next_comma >= star_pos) break;
        p = next_comma + 1;
    }

    if (item_count > 0)
    {
        LastOTAResponse.Status  = overall_status;
        LastOTAResponse.Pending = 1;
        /* trigger OTA-ACK PVT packet */
        AddAlert(CONF_CHANGE_ALERT);
    }

    if (reset_type == 1)
    {
        AIS140ResetPending = 1;
        Ql_strncpy(AIS140ResetReason, "SMS_SET_APN", sizeof(AIS140ResetReason) - 1);
        if (src < OTA_SRC_SCK_1 || src > OTA_SRC_SCK_4) {
            ThreadSleep(500);
            SystemRecovery_RequestReset(AIS140ResetReason);
            AIS140ResetPending = 0;
        }
    }
    else if (reset_type == 2)
    {
        AIS140ResetPending = 2;
        Ql_strncpy(AIS140ResetReason, "SMS_RESTART", sizeof(AIS140ResetReason) - 1);
        if (src < OTA_SRC_SCK_1 || src > OTA_SRC_SCK_4) {
            ThreadSleep(1000);
            SystemRecovery_RequestReset(AIS140ResetReason);
            AIS140ResetPending = 0;
        }
    }
}

#endif /* PROTO_OG */

/* SOS SMS fallback is shared by every non-CDAC protocol. */
uint8_t SendSOSSMS(uint8_t isFall)
{
    char msg[160];
    char ss[12];
    uint8_t cs;
    uint8_t sent = 0;

    (void)isFall;

    if (GSM.GSMState < SIM_DETECTED) return 0;
    if (!IsValidSOSMobileNumber(VTSData.PhoneNumber.Mob0) &&
        !IsValidSOSMobileNumber(VTSData.PhoneNumber.Mob1)) return 0;

    Ql_memset(msg, 0, sizeof(msg));
    Ql_strcat(msg, "$EPB,EMR,");
    Ql_strncat(msg, NetWork.IMEI, 15);
    InsertChar(msg, ',');
    AppendFixString(msg, sLatitude, 10, sLatitude);
    InsertChar(msg, ',');
    InsertChar(msg, GPS.LatDir ? GPS.LatDir : 'N');
    InsertChar(msg, ',');
    AppendFixString(msg, sLongitude, 10, sLongitude);
    InsertChar(msg, ',');
    InsertChar(msg, GPS.LngDir ? GPS.LngDir : 'E');
    InsertChar(msg, ',');
    InsertChar(msg, GPS.GPSFix ? 'A' : 'V');
    InsertChar(msg, ',');
    InsertFloatValue(msg, GPS.Speed, "%05.1f");
    InsertChar(msg, ',');
    AppendVariableString(msg, GSM.CellID, 5, 4, "0000");
    InsertChar(msg, ',');
    AppendVariableString(msg, GSM.LAC, 5, 4, "0000");
    InsertChar(msg, ',');
    InsertCurrentDateTime(msg, 0);
    InsertCurrentDateTime(msg, 1);
    cs = GetXORChecksum(msg + 1, Ql_strlen(msg) - 1);
    Ql_sprintf(ss, "*%02X\r\n", cs);
    Ql_strcat(msg, ss);

    if (IsValidSOSMobileNumber(VTSData.PhoneNumber.Mob0)) {
        if (SendSMS(VTSData.PhoneNumber.Mob0, msg)) {
            sent = 1;
            LOGData(TAG_OTA, "AMD3 SOS SMS -> Mob0: %s", VTSData.PhoneNumber.Mob0);
        }
    }
    if (IsValidSOSMobileNumber(VTSData.PhoneNumber.Mob1)) {
        if (SendSMS(VTSData.PhoneNumber.Mob1, msg)) {
            sent = 1;
            LOGData(TAG_OTA, "AMD3 SOS SMS -> Mob1: %s", VTSData.PhoneNumber.Mob1);
        }
    }

    return sent;
}

static void QueueVResetResponse(char* sender, uint8_t IsServer)
{
#if FEATURE_GPS_LAST_FIX_FALLBACK
	GPS_SaveLastFixedToFlash();
#endif
#ifdef PROTO_OG
	if (IsServer == OTA_SRC_SMS)
	{
		SendSMS(sender, "Device Restarting...");
	}
	else if (IsServer == OTA_SRC_RS232)
	{
		SendRS232Response("Device Restarting...\r\n");
	}
	else if (IsServer == OTA_SRC_RS485)
	{
		SendRS485Response("Device Restarting...\r\n");
	}
	else if (IsServer == OTA_SRC_BLE)
	{
		BLE_SendReply((uint8_t*)"Device Restarting...\r\n", 22);
	}

	Ql_memset(&LastOTAResponse, 0, sizeof(LastOTAResponse));
	LastOTAResponse.Channel = IsServer;
	switch (IsServer)
	{
	case OTA_SRC_SMS:
		Ql_strncpy(LastOTAResponse.Source, (sender && sender[0]) ? sender : "SMS", sizeof(LastOTAResponse.Source) - 1);
		break;
	case OTA_SRC_SCK_1:
		Ql_sprintf(LastOTAResponse.Source, "%s:%s", VTSData.ServerData.IP1, VTSData.ServerData.Port1);
		break;
	case OTA_SRC_SCK_2:
		Ql_sprintf(LastOTAResponse.Source, "%s:%s", VTSData.ServerData.IP3, VTSData.ServerData.Port3);
		break;
	case OTA_SRC_SCK_3:
		Ql_sprintf(LastOTAResponse.Source, "%s:%s", VTSData.ServerData.IP4, VTSData.ServerData.Port4);
		break;
	case OTA_SRC_SCK_4:
		Ql_sprintf(LastOTAResponse.Source, "%s:%s", VTSData.ServerData.IP4, VTSData.ServerData.Port4);
		break;
	case OTA_SRC_BLE:
		Ql_strncpy(LastOTAResponse.Source, "BLE", sizeof(LastOTAResponse.Source) - 1);
		break;
	case OTA_SRC_RS232:
		Ql_strncpy(LastOTAResponse.Source, "RS232", sizeof(LastOTAResponse.Source) - 1);
		break;
	case OTA_SRC_RS485:
		Ql_strncpy(LastOTAResponse.Source, "RS485", sizeof(LastOTAResponse.Source) - 1);
		break;
	default:
		Ql_sprintf(LastOTAResponse.Source, "%s:%s", VTSData.ServerData.IP1, VTSData.ServerData.Port1);
		break;
	}
	LastOTAResponse.Source[sizeof(LastOTAResponse.Source) - 1] = '\0';
	Ql_strncpy(LastOTAResponse.Mode, "SET", sizeof(LastOTAResponse.Mode) - 1);
	Ql_strncpy(LastOTAResponse.CmdId, "VRESET", sizeof(LastOTAResponse.CmdId) - 1);
	Ql_strncpy(LastOTAResponse.Value, "System Restarting", sizeof(LastOTAResponse.Value) - 1);
	LastOTAResponse.Status = 1;
	LastOTAResponse.Pending = 1;
	AddAlert(CONF_CHANGE_ALERT);

	AIS140ResetPending = 2;
	Ql_strncpy(AIS140ResetReason, "VRESET", sizeof(AIS140ResetReason) - 1);
#else
	SendResponce(sender, "Device Restarting...", IsServer, 1);
	ThreadSleep(6000);
	Ql_Reset(0);
#endif
}


uint8_t DecodeSMS(char* msg,uint8_t IsServer)
{
	char* fn;
	char* ls;
	char ss[45];
	#ifndef PROTO_CDAC
	char rnd[10];
	#endif
	int i;
	float f;
	memset(CMD_Buff,0x00,250);
	LOGData(TAG_OTA,"OTA Data: %s",msg);
	#if defined(PROTO_MAHARASHTRA1) || defined(PROTO_OG)
	ls = Ql_strstr(msg,"+S*R:");
	if(ls)
	{
		ls+=5;
		IsSRCMD=1;
		LOGData(TAG_OTA,"SR Command Parsing...");
		SRDecode(ls, IsServer);
		return 1;
	}
	IsSRCMD=0;
	#endif
	
	#ifndef PROTO_CDAC
	ls = Ql_strstr(msg,"ACTV");
	if(ls)
	{
		// if(IsServer==OTA_SEC_SCK_1)
		// 	return 1;
		if(Ql_strlen(ls)<11)
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
				
				// Safe string copy with bounds check
				int rnd_len = ls - fn;
				if (rnd_len > 8) rnd_len = 8;
				Ql_strncpy(rnd, fn, rnd_len);
				rnd[8] = '\0';

				fn = strchr(ls,'\r');
				if(!fn)
				{
					fn = strchr(ls,'\0');
				}
				if (fn) *fn = 0;
				
				// Safe string copy with bounds check
				Ql_strncpy(ss, ls, sizeof(ss) - 1);
				ss[sizeof(ss) - 1] = '\0';
				char formatted_ss[25];
				if (Ql_strlen(ss) == 10 && ss[0] >= '0' && ss[0] <= '9')
					Ql_sprintf(formatted_ss, "+91%s", ss);
				else
					Ql_strcpy(formatted_ss, ss);
				LOGData(TAG_OTA,"Sending ACTV reply with Rc : %s to %s",rnd,formatted_ss);
				MakeACTMessage(0,  rnd);
				SendSMS(formatted_ss,SimData);
				TCPSocket_SendString(&ServerSocket[0],SimData);
				TCPSocket_SendString(&ServerSocket[2],SimData);
				#ifdef EXTENDED_IPS
				TCPSocket_SendString(&ServerSocket[3],SimData);
				#endif
				
				ACTMsg=1;
				return 1;
			}
		}
	}
	ls = Ql_strstr(msg,"HCHK");
	if(ls)
	{
		// if(IsServer==OTA_SEC_SCK_1)
		// 	return 1;
		if(Ql_strlen(ls)<11)
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
				
				// Safe string copy with bounds check
				int rnd_len = ls - fn;
				if (rnd_len > 8) rnd_len = 8;
				Ql_strncpy(rnd, fn, rnd_len);
				rnd[8] = '\0';

				fn = strchr(ls,'\r');
				if(!fn)
				{
					fn = strchr(ls,'\0');
				}
				if (fn) *fn = 0;
				
				// Safe string copy with bounds check
				Ql_strncpy(ss, ls, sizeof(ss) - 1);
				ss[sizeof(ss) - 1] = '\0';
				char formatted_ss[25];
				if (Ql_strlen(ss) == 10 && ss[0] >= '0' && ss[0] <= '9')
					Ql_sprintf(formatted_ss, "+91%s", ss);
				else
					Ql_strcpy(formatted_ss, ss);
				LOGData(TAG_OTA,"\r\nSending HCHKR reply with Rc : %s to %s",rnd,formatted_ss);
				MakeACTMessage(1,  rnd);
				SendSMS(formatted_ss,SimData);
				TCPSocket_SendString(&ServerSocket[0],SimData);
				TCPSocket_SendString(&ServerSocket[2],SimData);
				#ifdef EXTENDED_IPS
				TCPSocket_SendString(&ServerSocket[3],SimData);
				#endif
				ACTMsg=2;
				return 1;
			}
		}
	}
	#endif
	ls = Ql_strstr(msg,"TESTRIG");
	if(!ls) ls = Ql_strstr(msg,"TESRIG");
	if(ls)
	{
		i=GetValueFromData(ls,"TESTRIG",' ',0,'\0',ss);
		if(!i) i=GetValueFromData(ls,"TESRIG",' ',0,'\0',ss);
		if(i)
		{
			LOGData(TAG_OTA,"ss:%s",ss);
			i = atoi(ss);
			if((i>0) && (i<ALERT_COUNT))
			{
				LOGData(TAG_OTA,"activating alert %d",i);
				AddAlert(i);
				return 1;
			}
			if(i == 0)
			{
				SOS.IsSOS=1;
				#ifndef PROTO_CDAC
				SendSOSAlertSMS(10);
				#endif
				SLED_ON;
				SOS.SOSTimeLasped=0;
				VAlert[SOS_ON_ALERT].Enable=1;
				LOGData(TAG_OTA,"********************\nSOS Alert Manual ON\n***********************\n");
				AddAlert(SOS_ON_ALERT);
				#ifndef PROTO_CDAC
				VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.SOSInterval;
				#endif
				return 1;
			}
			else if(i == 1)
			{
				SOS.SOSTimeLasped=0;
				SOS.IsSOS=0;
				SLED_OFF;
				LOGData(TAG_OTA,"********************\nSOS Alert Manual OFF********************\n");
				AddAlert(SOS_OFF_ALERT);
				RemoveAlert(SOS_ON_ALERT);
				SendSOSAlertSMS(11);
				#ifndef PROTO_CDAC
				VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
				#endif
				return 1;
			}
		}
	}
	fn=Ql_strstr(msg,"GET");
	if(fn)
	{
		
		memset(SimData,0x00,MSGSIZE);
		fn=fn+3;

		if(Ql_strstr(fn,"SIMMAKE"))
		{
			Ql_sprintf(SimData,"Current Selected STK Protocol : ");
			Ql_strcat(SimData,SIM_MAKE_STR);
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}

		if(Ql_strstr(fn,"OUTSTAT"))
		{
			Ql_sprintf(SimData,"Output 1 - %d\nOutput 2 - %d",PeriPheralVal.OP1,PeriPheralVal.OP2);
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		if(Ql_strstr(fn,"INPSTAT"))
		{
			Ql_sprintf(SimData,"Input 1 - %d\nInput 2 - %d",PeriPheralVal.IP1,PeriPheralVal.IP2);
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		if(Ql_strstr(fn,"OPERATOR"))
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
					Ql_sprintf(ss,"assumed to be AIRTEL");
				#endif
				#ifdef 	SIM_PROFILE_JIO
				else if(VTSState.CurrentProfile == SIM_PROFILE_JIO)
					Ql_sprintf(ss,"assumed to be JIO");
				#endif
				#ifdef 	SIM_PROFILE_VI
				else if(VTSState.CurrentProfile == SIM_PROFILE_VI)
					Ql_sprintf(ss,"assumed to be VODAFONE");
				#endif
				#ifdef 	SIM_PROFILE_BSNL
				else if(VTSState.CurrentProfile == SIM_PROFILE_BSNL)
					Ql_sprintf(ss,"assumed to be BSNL");
				#endif
				else
					Ql_sprintf(ss,"currently UNKNOWN");
			}
			Ql_sprintf(SimData,"Network Operator is %s",ss);
			SendResponce(SMSSender,SimData,IsServer,0);
			
			return 1;
		}
		if(Ql_strstr(fn,"PRF"))
		{
			if(VTSData.IsCustomSPN==1)
			{
				Ql_sprintf(SimData,"SPN, as %s",NetWork.Network);
			}
			else
			{
				Ql_sprintf(SimData,"SPN  as %s",NetWork.Network);
			}
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}

		if(Ql_strstr(fn,"PROFILE"))
		{
			Ql_sprintf(SimData,"Current Profile : %d",VTSState.CurrentProfile);
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}

		if(Ql_strstr(fn,"SOSTIMEOUT"))
		{
			Ql_sprintf(SimData,"SOS Timout: %d",VTSData.IntervalData.SOSTimeOut);
			SendResponce(SMSSender,SimData,IsServer,0);\
			return 1;
		}
		if(Ql_strstr(fn,"SOSDISABLE"))
		{
			Ql_sprintf(SimData,"SOS Disable: %d",VTSData.DisableSOS);
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		/*
		if(Ql_strstr(fn,"IMIDISABLE"))
		{
			Ql_sprintf(SimData,"IMI Disable: %d",VTSData.DisableImiCmd);
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		if(Ql_strstr(fn,"IMI"))
		{
			if(VTSData.DisableImiCmd == 1)
			{
				SendResponce(SMSSender,"IMI Command Disabled",IsServer,0);
				return 1;
			}
			if(VTSData.CustomImei.IsEnable)
			{
				Ql_sprintf(SimData,"Custom IMEI: Enabled, %s", VTSData.CustomImei.Imei);
			}
			else
			{
				Ql_sprintf(SimData,"Custom IMEI: Disabled");
			}
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		*/

		if(Ql_strstr(fn,"VDETAIL"))
		{
			memset(SimData,0x00,MSGSIZE);
			Ql_strcpy(SimData,"FirVer = ");
			Ql_strcat(SimData,PROTO_TAG);
			Ql_strcat(SimData,"_");
			Ql_strcat(SimData,FirmVer);
			Ql_strcat(SimData,"\r\nBuild Date - ");
			Ql_strcat(SimData,__DATE__);  // Adds compile date
			Ql_strcat(SimData," ");
			Ql_strcat(SimData,__TIME__);   // Adds compile time
			Ql_strcat(SimData,"\r\nBinary Size - 60120\r\n");
			Ql_strcat(SimData,"CheckSum 0x51AC");
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		if(Ql_strstr(fn,"VSTATUS"))
		{
			memset(SimData,0x00,MSGSIZE);
			Ql_strcpy(SimData,"GPRS ");
			if(GSM.GSMState!=GPRS_ACTIVE)
				Ql_strcat(SimData,"NOT ");
			Ql_strcat(SimData,"ACTIVE\n");
			Ql_sprintf(ss,"%d",GSM.SignalStrength);
			Ql_strcat(SimData,"SIG : ");
			Ql_strcat(SimData,ss);
			Ql_sprintf(ss,"\nSOS:%d IGN:%d",SOS.IsSOS, PeriPheralVal.IGN);
			Ql_strcat(SimData,ss);
			Ql_sprintf(ss,"%02.2f",PeriPheralVal.MainsVolt);
			Ql_strcat(SimData,"\nMV: ");
			Ql_strcat(SimData,ss);
			Ql_sprintf(ss,"%01.2f",PeriPheralVal.BattVolt);
			Ql_strcat(SimData,"\nBV: ");
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\nSV: ");
			#if  defined(HTTP_SIMULATE) && defined(PROTO_CDAC)
			if(HTTPState == HTTP_STATE_SET)
				Ql_strcat(SimData,"G.TR OK, ");
			else
				Ql_strcat(SimData,"G.TR NC, ");
			#else
			if(VTSData.ServerData.IP1[0] == 'N' && VTSData.ServerData.IP1[1] == 'A')
				Ql_strcat(SimData,"G.TR NA, ");
			else if(ServerSocket[0].SocketState == SOCKET_CONNECTED)
				Ql_strcat(SimData,"G.TR OK, ");
			else
				Ql_strcat(SimData,"G.TR NC, ");
			
			#endif

			#ifndef PROTO_CDAC
			if(VTSData.ServerData.IP2[0] == 'N' && VTSData.ServerData.IP2[1] == 'A')
				Ql_strcat(SimData,"G.EM NA, ");
			else if(ServerSocket[1].SocketState == SOCKET_CONNECTED)
				Ql_strcat(SimData,"G.EM OK, ");
			else
				Ql_strcat(SimData,"G.EM NC, ");
			#endif

			if(VTSData.ServerData.IP3[0] == 'N' && VTSData.ServerData.IP3[1] == 'A')
				Ql_strcat(SimData,"P.TR NA");
			else if(ServerSocket[2].SocketState == SOCKET_CONNECTED)
				Ql_strcat(SimData,"P.TR OK");
			else
				Ql_strcat(SimData,"P.TR NC");
			
			#ifdef EXTENDED_IPS
			if(VTSData.ServerData.IP4[0] == 'N' && VTSData.ServerData.IP4[1] == 'A')
				Ql_strcat(SimData,", E.IP NA");
			else if(ServerSocket[3].SocketState == SOCKET_CONNECTED)
				Ql_strcat(SimData,", E.IP OK");
			else
				Ql_strcat(SimData,", E.IP NC");
			#endif

			if(GPS.State!=1 && !GPS_IsSimulationActive())
			{
				Ql_strcat(SimData,"\nGPS FLT");
			}
			else
			{
				if(GPS.GPSFix)
					Ql_strcat(SimData,"\nGPS OK");
				else
					Ql_strcat(SimData,"\nGPS NF");
				StringAdd(SimData,"\nTS:%d, VS:%d",GPS.NoOfSatalite,GPS.SatTotal);
			}
			
			#ifndef PROTO_CDAC
			#ifdef HISTORY_INTERNAL
			CheckPacketCount();
			Ql_sprintf(ss,"\nHP: %d",PacketConfig.LastPkt);
			#else
			Ql_sprintf(ss,"\nHP: %d",StoredHistoryDataCount);
			#endif
			#else
			Ql_sprintf(ss,"\nHP: %d",FTable.TotalFiles);
			#endif
			Ql_strcat(SimData,ss);


			SendResponce(SMSSender,SimData,IsServer,0);
		}
		if(Ql_strstr(fn,"HISTORY") || Ql_strstr(fn,"HIST"))
		{
			Ql_sprintf(SimData,"History: %s", VTSData.DisableHistory ? "DISABLED" : "ENABLED");
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		if(Ql_strstr(fn,"FTK"))
		{
			#ifndef HISTORY_DISABLED
			memset(SimData,0x00,MSGSIZE);
			if(FTKConfig.IsEnabled)
			{
				Ql_sprintf(SimData,"FTK Enabled\n, Current progress: %d",FTKConfig.PacketCount);
				SendResponce(SMSSender,SimData,IsServer,0);
				return 1;
			}
			else
			{
				Ql_sprintf(SimData,"FTK Disabled");
				SendResponce(SMSSender,SimData,IsServer,0);
				return 1;
			}
			#else
			SendResponce(SMSSender,"FTK Unavailable",IsServer,0);
			return 1;
			#endif
		}
		if(Ql_strstr(fn,"SERVERDETAIL"))
		{
			memset(SimData,0x00,MSGSIZE);
			Ql_strcpy(SimData,"IP1 -  ");
			Ql_strcat(SimData,VTSData.ServerData.IP1);
			Ql_strcat(SimData,"\r\nPort1 - ");
			Ql_strcat(SimData,VTSData.ServerData.Port1);
			Ql_strcat(SimData,"\r\nIP2 - ");
			Ql_strcat(SimData,VTSData.ServerData.IP2);
			Ql_strcat(SimData,"\r\nPort2 - ");
			Ql_strcat(SimData,VTSData.ServerData.Port2);
			Ql_strcat(SimData,"\r\nIP3 - ");
			Ql_strcat(SimData,VTSData.ServerData.IP3);
			Ql_strcat(SimData,"\r\nPort3 - ");
			Ql_strcat(SimData,VTSData.ServerData.Port3);
			#ifdef EXTENDED_IPS
			Ql_strcat(SimData,"\r\nIP4 - ");
			Ql_strcat(SimData,VTSData.ServerData.IP4);
			Ql_strcat(SimData,"\r\nPort4 - ");
			Ql_strcat(SimData,VTSData.ServerData.Port4);
			#endif
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		if(Ql_strstr(fn,"LOCATION"))
		{
			memset(SimData,0x00,MSGSIZE);
			Ql_strcpy(SimData,"Latitude -  ");
			Ql_strcat(SimData,sLatitude);
			Ql_strcat(SimData,"\r\nLongitude - ");
			Ql_strcat(SimData,sLongitude);
			Ql_strcat(SimData,"\r\nAltitude - ");
			Ql_strcat(SimData,sAltitude);
			Ql_strcat(SimData,"\r\nSpeed - ");
			Ql_strcat(SimData,sSpeed);
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		if(Ql_strstr(fn,"RSTEST"))
		{
			memset(SimData,0x00,MSGSIZE);
			if(IsServer==OTA_SRC_RS232)
				Ql_sprintf(SimData,"RS232 Test OK - Source: %d", IsServer);
			else if(IsServer==OTA_SRC_RS485)
				Ql_sprintf(SimData,"RS485 Test OK - Source: %d", IsServer);
			else
				Ql_sprintf(SimData,"Serial Test OK - Source: %d", IsServer);
			
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		if(Ql_strstr(fn,"PANIC"))
		{
			memset(SimData,0x00,MSGSIZE);
			Ql_strcpy(SimData,"SOS -  ");
			if(SOS.IsSOS)
				Ql_strcat(SimData,"ON");
			else
				Ql_strcat(SimData,"OFF");
			
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		if(Ql_strstr(fn,"VINFO"))
		{
			memset(SimData,0x00,MSGSIZE);
			Ql_strcpy(SimData,"VID -  ");
			Ql_strcat(SimData,VTSData.VendorID);
			Ql_strcat(SimData,"\nIMEI - ");
			Ql_strcat(SimData,NetWork.IMEI);
			Ql_strcat(SimData,"\nCCID - ");
			Ql_strcat(SimData,NetWork.SIMNo);
			Ql_strcat(SimData,"\nIMSI - ");
			Ql_strcat(SimData,NetWork.IMSI);
			Ql_strcat(SimData,"\nVN - ");
			Ql_strcat(SimData,VTSData.VehicleData.VehicleRegNo);
			Ql_strcat(SimData,"\nSPN - ");
			Ql_strcat(SimData,NetWork.Network);
			Ql_strcat(SimData,"\nAPNMODE:");
			if(VTSData.AutoAPN)
				Ql_strcat(SimData,"AUTO");
			else
				Ql_strcat(SimData,"MANUAL");
			Ql_strcat(SimData,"\nC.APN - ");
			if(VTSData.AutoAPN)
				Ql_strcat(SimData,NetWork.APN);
			else
				Ql_strcat(SimData,VTSData.mAPN);
			Ql_strcat(SimData,"\nBTH - ");
			Ql_sprintf(ss,"%2.1f",VTSData.BattThrs);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\nN1 - ");
			Ql_strcat(SimData,VTSData.PhoneNumber.Mob0);
			Ql_strcat(SimData,"\nN2 -  ");
			Ql_strcat(SimData,VTSData.PhoneNumber.Mob1);
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		if(Ql_strstr(fn,"VINTERVAL"))
		{	
			#ifndef PROTO_CDAC
			memset(SimData,0x00,MSGSIZE);
			Ql_strcpy(SimData,"IMEI -  ");
			Ql_strcat(SimData,NetWork.IMEI);
			Ql_strcat(SimData,"\r\nINTERVAL\r\nNORMAL - ");
			Ql_sprintf(ss,"%d",VTSData.IntervalData.DataInterval);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\r\nIGN ON - ");
			Ql_sprintf(ss,"%d",VTSData.IntervalData.IgnitionInterval);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\r\nSOS - ");
			Ql_sprintf(ss,"%d",VTSData.IntervalData.SOSInterval);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\r\nStandBy - ");
			Ql_sprintf(ss,"%d",VTSData.IntervalData.StandbyInterval);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\r\nHealth - ");
			Ql_sprintf(ss,"%d", VTSData.IntervalData.HealthInterval);
			Ql_strcat(SimData,ss);
			#else
			memset(SimData,0x00,MSGSIZE);
			Ql_strcpy(SimData,"IMEI -  ");
			Ql_strcat(SimData,NetWork.IMEI);
			Ql_strcat(SimData,"\r\nINTERVAL\r\nMotion - ");
			Ql_sprintf(ss,"%d",VTSData.IntervalData.MotionInterval);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\nHalt - ");
			Ql_sprintf(ss,"%d",VTSData.IntervalData.HaltInterval);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\nSOS - ");
			Ql_sprintf(ss,"%d",VTSData.IntervalData.EnergencyInterval);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\nStandBy - ");
			Ql_sprintf(ss,"%d",VTSData.IntervalData.SleepInterval);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\nHealth - ");
			Ql_sprintf(ss,"%d", VTSData.IntervalData.HealthInterval);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\nFull - ");
			Ql_sprintf(ss,"%d", VTSData.IntervalData.FullDataPacketInterval);
			Ql_strcat(SimData,ss);
			#endif
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		if(Ql_strstr(fn,"VBEHAVE"))
		{
			memset(SimData,0x00,MSGSIZE);
			Ql_strcpy(SimData,"IMEI -  ");
			Ql_strcat(SimData,NetWork.IMEI);
			Ql_strcat(SimData,"\r\nHarsh Acc - ");
			Ql_sprintf(ss,"%d",VTSData.VehicleData.HarshAcc);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\r\nHarsh Break - ");
			Ql_sprintf(ss,"%d",VTSData.VehicleData.HarshBreak);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\r\nRash Turn - ");
			Ql_sprintf(ss,"%d",VTSData.VehicleData.RashTurn);
			Ql_strcat(SimData,ss);
			Ql_strcat(SimData,"\r\nOver Speed - ");
			Ql_sprintf(ss,"%06.1f",VTSData.VehicleData.OverSpeed);
			Ql_strcat(SimData,ss);
			SendResponce(SMSSender,SimData,IsServer,0);
			
			return 1;
		}
		
		ls = Ql_strstr(fn,"MCU");
		if(ls)
		{
			LOGData(TAG_SMS,"Requesting MCU Version");
			if(MCOMM_FetchVER())
			{
				memset(SimData,0x00,MSGSIZE);
				Ql_sprintf(SimData,"Current MCU Version : %s",PeriPheralVal.MCUFirmwareVersion);
				SendResponce(SMSSender,SimData,IsServer,0);
				LOGData(TAG_OTA,"MCU Version received : %s",PeriPheralVal.MCUFirmwareVersion);
			}
			else
			{
				SendResponce(SMSSender,"Unable to Get MCU Version",IsServer,0);
				LOGData(TAG_OTA,"MCU Version request failed");
			}
			return 1;
		}
		if(Ql_strstr(fn,"SENS"))
		{
			memset(SimData,0x00,MSGSIZE);
			Ql_sprintf(SimData,"U2M - %d\nIP2M - %d\nIGINT - %d\nOFINT - %d",VTSData.SensorSetting.Uart2Mode,VTSData.SensorSetting.IP2Mode,VTSData.SensorSetting.IGNInterval,VTSData.SensorSetting.OFFInterval);
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
		#ifndef PROTO_CDAC
		ls = Ql_strstr(fn,"GEO");
		if(ls)
		{
			if(IsServer==OTA_SRC_SMS)
				return 1;

			i = ls[3] - '0';
			if(i < 0 || i > 9)
				return 1;
			LOGData(TAG_OTA,"sending geo data for loc %d",i);
			SendGeoData(i,IsServer);
			return 1;
		}
		#endif

		if(Ql_strstr(fn,"DISK"))
		{
			int freeSpace = Ql_FS_GetFreeSpace(Ql_FS_UFS);
			double free_mb = (double)freeSpace / (1024.0 * 1024.0);
			Ql_sprintf(SimData,"UFS Disk Space: %0.2f MB Free", free_mb);
			SendResponce(SMSSender,SimData,IsServer,0);
			return 1;
		}
	}
	if(Ql_strstr(msg,"VRESET"))
	{
		QueueVResetResponse(SMSSender, IsServer);
		return 1;
	}
	fn = Ql_strstr(msg,"SET");
	if(fn)
	{
		LOGData(TAG_OTA,"SET CMD");
		fn=fn+3;
		ls = Ql_strstr(fn,"EPO ");
		if(ls)
		{
			char ip[50] = {0};
			char user[50] = {0};
			char pass[50] = {0};
			char remote[80] = {0};
			char local[120] = {0};
			uint16_t port = 0;
			char st[80] = {0};

			if(!ParseEpoFtpArgs(ls, ip, &port, user, pass, remote))
			{
				SendResponce(SMSSender,"EPO: bad args. Use SET EPO ip,port,user,pass,filename",IsServer,0);
				return 1;
			}

			LOGData(TAG_OTA,"EPO requested via SMS. FTP %s:%d file:%s", ip, port, remote);

			// The FTP stack saves the local file using the remote basename.
			// For RAM storage this means: "RAM:<basename(remote)>".
			{
				const char* base = remote;
				int j;
				for (j = 0; remote[j] != 0; j++) {
					if (remote[j] == '/') {
						base = &remote[j + 1];
					}
				}
				if (!base || base[0] == 0) {
					SendResponce(SMSSender,"EPO: bad filename",IsServer,0);
					Ql_memset(pass, 0, sizeof(pass));
					return 1;
				}
				Ql_sprintf(local, "RAM:%s", base);
			}

			SendResponce(SMSSender,"EPO: downloading...",IsServer,0);
			if(!FTP_DownloadOnce(ip, port, user, pass, remote, local, "RAM"))
			{
				SendResponce(SMSSender,"EPO: FTP download failed",IsServer,0);
				Ql_memset(pass, 0, sizeof(pass));
				return 1;
			}

			SendResponce(SMSSender,"EPO: injecting...",IsServer,0);
			if(!EPO_FlashInjectFile(local, st, sizeof(st)))
			{
				SendResponce(SMSSender,st[0] ? st : "EPO: inject failed",IsServer,0);
				Ql_FS_Delete(local);
				Ql_memset(pass, 0, sizeof(pass));
				return 1;
			}

			Ql_FS_Delete(local);
			SendResponce(SMSSender,st,IsServer,0);
			Ql_memset(pass, 0, sizeof(pass));
			return 1;
		}
		uint8_t is_cgps = 0;
		ls = Ql_strstr(fn,"FGPS");
		if(!ls)
		{
			ls = Ql_strstr(fn,"CGPS");
			if(ls) is_cgps = 1;
		}
		if(ls)
		{
			char* cmdName = is_cgps ? "CGPS" : "FGPS";
			LOGData(TAG_OTA,"%s cmd", cmdName);
			double flat=0, flng=0, pdop=0, hdop=0; // SET FGPS/CGPS 28234345,76123123,180,120,8,200,25,180
			uint8_t nofsat=0;
			int altitude=0, speed=0, heading=0;
			
			i=GetValueFromData(ls,cmdName,' ',0,',',ss);
			if(i)
			{
				flat = (double)atol(ss)/1000000;
				LOGData(TAG_OTA,"FLat : %s",ss);
			}
			i=GetValueFromData(ls,cmdName,',',1,',',ss);
			if(i)
			{
				flng = (double)atol(ss)/1000000;
				LOGData(TAG_OTA,"Flng : %s",ss);
			}
			i=GetValueFromData(ls,cmdName,',',2,',',ss);
			if(i)
			{
				pdop = (double)atol(ss)/100;
				LOGData(TAG_OTA,"pdop : %s",ss);
			}
			i=GetValueFromData(ls,cmdName,',',3,',',ss);
			if(i)
			{
				hdop = (double)atol(ss)/100;
				LOGData(TAG_OTA,"hdop : %s",ss);
			}
			i=GetValueFromData(ls,cmdName,',',4,',',ss);
			if(i)
			{
				nofsat = atoi(ss);
				LOGData(TAG_OTA,"sats : %i",nofsat);
			}
			i=GetValueFromData(ls,cmdName,',',5,',',ss);
			if(i)
			{
				altitude = atoi(ss);
				LOGData(TAG_OTA,"altitude : %d",altitude);
			}
			i=GetValueFromData(ls,cmdName,',',6,',',ss);
			if(i)
			{
				speed = atoi(ss);
				LOGData(TAG_OTA,"speed : %d",speed);
			}
			i=GetValueFromData(ls,cmdName,',',7,'\0',ss);
			if(i)
			{
				heading = atoi(ss);
				LOGData(TAG_OTA,"heading : %d",heading);
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
				fGPSForce = is_cgps ? 1 : 0;
				LOGData(TAG_OTA,"%s Set complete with Alt: %d, Speed: %d, Heading: %d, Force: %d", cmdName, (int)fGPSAlt, (int)fGPSSpeed, (int)fGPSHeading, (int)fGPSForce);
				SendResponce(SMSSender,"System Config Complete",IsServer,0);
			}
			return 1;
		}

		ls = Ql_strstr(fn,"SIMMAKE");
		if(ls)
		{
			ls+= 8;
			if(Ql_strstr(ls,"TAIS"))
				VTSData.SIMMake = TAISYS;
			else if(Ql_strstr(ls,"SENS"))
				VTSData.SIMMake = SENSORISE;
			else
			{
				SendResponce(SMSSender,"Invalid Sim make !",IsServer,0);
				return 1;
			}
			UpdateConfigInFlash();
			SendResponce(SMSSender,"Sim Make Changed",IsServer,1);
			ThreadSleep(2000);
			Ql_Reset(0);
			ThreadSleep(3000);
			return 1;
		}

		ls=Ql_strstr(fn,"OUTSTAT ");
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

			SendResponce(SMSSender,"Output Updated",IsServer,1);
			return 1;
		}

		ls = Ql_strstr(fn, "IPCON");
		if (ls) {
			i = GetValueFromData(ls, "IPCON", ' ', 0, ',', ss);
			if (i) {
				int index = atoi(ss);
				if (index >= 0 && index < MAX_IP_CONFIG) {
					i = GetValueFromData(ls, "IPCON", ',', 1, '\0', ss);
					if (i) {
						int state = atoi(ss);
						if (state == 0 || state == 1) {
							VTSData.ServerData.IPConfig[index] = state;
							UpdateConfigInFlash();
							ServerSocket[index].isEnabled=state;
							Ql_sprintf(SimData, "IPCON[%d] set to %d", index, state);
							SendResponce(SMSSender, SimData, IsServer, 1);
							return 1;
						}
					}
				}
			}
			SendResponce(SMSSender, "Invalid IPCON command", IsServer, 0);
			return 1;
		}
		/*
		ls = Ql_strstr(fn,"IMIDISABLE");
		if(ls)
		{
			i=GetValueFromData(ls,"IMIDISABLE",' ',0,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if(i == 0 || i == 1)
				{
					VTSData.DisableImiCmd = i;
					UpdateConfigInFlash();
					Ql_sprintf(SimData,"IMI Disable set to %d", VTSData.DisableImiCmd);
					SendResponce(SMSSender,SimData,IsServer,1);
					return 1;
				}
			}
			SendResponce(SMSSender,"Invalid Param",IsServer,0);
			return 1;
		}
		ls = Ql_strstr(fn, "IMI");
		if (ls)
		{
			if(VTSData.DisableImiCmd == 1)
			{
				SendResponce(SMSSender,"IMI Command Disabled",IsServer,0);
				return 1;
			}
			i = GetValueFromData(ls, "IMI", ' ', 0, '\0', ss);
			if (i)
			{
				if (ss[0] == '0' && ss[1] == '\0')
				{
					VTSData.CustomImei.IsEnable = 0;
					Ql_memset(VTSData.CustomImei.Imei, 0, sizeof(VTSData.CustomImei.Imei));
					UpdateConfigInFlash();
					SendResponce(SMSSender, "Config MI reset", IsServer, 1);
					GetDeviceIMEI();
					InitSockets();
					return 1;
				}
				if (Ql_strlen(ss) != 15)
				{
					SendResponce(SMSSender, "Invalid IMEI !", IsServer, 0);
					return 1;
				}
				LOGData(TAG_OTA, "Setting Custom IMEI : %s", ss);
				Ql_strncpy(VTSData.CustomImei.Imei, ss, sizeof(VTSData.CustomImei.Imei));
				VTSData.CustomImei.IsEnable = 1;
				UpdateConfigInFlash();
				SendResponce(SMSSender, "Config MI set", IsServer, 1);
				GetDeviceIMEI();
				InitSockets();
				return 1;
			}
			else
			{
				SendResponce(SMSSender, "Invalid Config IMI!", IsServer, 0);
				return 1;
			}
		}
		*/

		ls=Ql_strstr(fn,"DFTP");
		if(ls)
		{
			LoadDefaultFOTAParams(SMSSender,IsServer);
			return 1;
		}

		ls=Ql_strstr(fn,"FOTA");
		if(ls)
		{
			FOTAPacket(ls,SMSSender,IsServer);
			return 1;
		}

		ls=Ql_strstr(fn,"MOTA");
		if(ls)
		{
			MOTAPacket(ls,SMSSender,IsServer);
			return 1;
		}

		ls=Ql_strstr(fn,"PRF ");
		if(ls)
		{
			if(ls[4] == '0')
			{
				VTSData.IsCustomSPN = 0;
				UpdateConfigInFlash();
				SendResponce(SMSSender,"Prf Reset Requested",IsServer,1);
				ThreadSleep(5000);
				Ql_Reset(0);
				ThreadSleep(4000);
			}
			else
			{
				i=GetValueFromData(ls,"PRF",' ',0,'\0',ss);
				if(i)
				{
					Ql_strcpy(VTSData.mSPN,ss);
					VTSData.IsCustomSPN=1;
					UpdateConfigInFlash();
					SendResponce(SMSSender,"Prf Change Requested",IsServer,1);
					ThreadSleep(5000);
					Ql_Reset(0);
					ThreadSleep(4000);
				}
			}
			return 1;
		}

		ls=Ql_strstr(fn,"OPERATOR");
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
					LOGData(TAG_OTA,"Invalid Operator Profile Req");
					SendResponce(SMSSender,"Invalid Operator Profile Req",IsServer,0);
					return 1;
				}
				
				// Handle unsupported profile case
				if(operator_name != NULL)
				{

					Ql_sprintf(SimData, "%s operator profile not supported in this SIM", operator_name);
					LOGData(TAG_OTA, "%s", SimData);
					SendResponce(SMSSender, SimData, IsServer, 0);
					return 1;
				}
				
				// Profile change request
				if(req > 0)
				{
					Ql_sprintf(SimData,"Operator Profile Change to %s aka %d Requested", lower_ss, req);
					SendResponce(SMSSender,SimData,IsServer,1);
					LOGData(TAG_OTA,"%s", SimData);
					ThreadSleep(1000);
					prfReq = req;
				}
			}
		}

		ls=Ql_strstr(fn,"PROFILE ");
		if(ls)
		{
			LOGData(TAG_OTA,"Profile change CMD");
			uint8_t req=0;
			switch(ls[8])
			{
				case '1':
					req = 1;
					break;
				case '2':
					req = 2;
					break;
				case '3':
					req = 3;
					break;
				default:
					SendResponce(SMSSender,"INVALID Profile Change Req",IsServer,0);
					LOGData(TAG_OTA,"Invalid Profile Change Req");
					return 1;
			}
			
			// Execute profile change if valid
			if(req > 0)
			{
				SendResponce(SMSSender,"Profile Change Requested",IsServer,1);
				LOGData(TAG_OTA,"Profile Change Requested");
				ThreadSleep(1000);
				prfReq = req;
			}
			
			return 1;
		}
		ls = Ql_strstr(fn,"VID");
		if(ls)
		{
			i=GetValueFromData(ls,"VID",' ',0,'\0',ss);
			if(i)
			{
				if(Ql_strlen(ss)>25 || Ql_strlen(ss) < 3)
				{
					SendResponce(SMSSender,"Vendor ID invalid Length",IsServer,0);
					return 1;
				}
				Ql_strcpy(VTSData.VendorID,ss);
				UpdateConfigInFlash();
				Ql_sprintf(SimData,"Vendor ID Changed to %s",VTSData.VendorID);
				SendResponce(SMSSender,SimData,IsServer,0);
				return 1;
			}
		}

		ls=Ql_strstr(fn,"SERVER");
		if(ls)
		{
			uint8_t s1_changed = 0, s2_changed = 0, s3_changed = 0, s4_changed = 0;
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER1",' ',0,',',ss);// SETSERVER1 ip,port[,SERVER2 ip,port]
			if(i)
			{
				Ql_strncpy(VTSData.ServerData.IP1, ss, sizeof(VTSData.ServerData.IP1) - 1);
				VTSData.ServerData.IP1[sizeof(VTSData.ServerData.IP1) - 1] = '\0';
				s1_changed = 1;
			}
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER1",',',1,',',ss);
			if(i)
			{
				Ql_strncpy(VTSData.ServerData.Port1, ss, sizeof(VTSData.ServerData.Port1) - 1);
				VTSData.ServerData.Port1[sizeof(VTSData.ServerData.Port1) - 1] = '\0';
				s1_changed = 1;
			}
			else
			{
				memset(ss,0,sizeof(ss));
				i=GetValueFromData(ls,"SERVER1",',',1,'\0',ss);
				if(i)
				{
					Ql_strncpy(VTSData.ServerData.Port1, ss, sizeof(VTSData.ServerData.Port1) - 1);
					VTSData.ServerData.Port1[sizeof(VTSData.ServerData.Port1) - 1] = '\0';
				s1_changed = 1;
				}
			}
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER2",' ',0,',',ss);
			if(i)
			{
				Ql_strncpy(VTSData.ServerData.IP2, ss, sizeof(VTSData.ServerData.IP2) - 1);
				VTSData.ServerData.IP2[sizeof(VTSData.ServerData.IP2) - 1] = '\0';
				s2_changed = 1;
			}
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER2",',',1,',',ss);
			if(i)
			{
				Ql_strncpy(VTSData.ServerData.Port2, ss, sizeof(VTSData.ServerData.Port2) - 1);
				VTSData.ServerData.Port2[sizeof(VTSData.ServerData.Port2) - 1] = '\0';
				s2_changed = 1;
			}
			else
			{
				memset(ss,0,sizeof(ss));
				i=GetValueFromData(ls,"SERVER2",',',1,'\0',ss);
				if(i)
				{
					Ql_strncpy(VTSData.ServerData.Port2, ss, sizeof(VTSData.ServerData.Port2) - 1);
					VTSData.ServerData.Port2[sizeof(VTSData.ServerData.Port2) - 1] = '\0';
				s2_changed = 1;
				}
			}

			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER3",' ',0,',',ss);
			if(i)
			{
				Ql_strncpy(VTSData.ServerData.IP3, ss, sizeof(VTSData.ServerData.IP3) - 1);
				VTSData.ServerData.IP3[sizeof(VTSData.ServerData.IP3) - 1] = '\0';
				s3_changed = 1;
			}
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER3",',',1,',',ss);
			if(i)
			{
				Ql_strncpy(VTSData.ServerData.Port3, ss, sizeof(VTSData.ServerData.Port3) - 1);
				VTSData.ServerData.Port3[sizeof(VTSData.ServerData.Port3) - 1] = '\0';
				s3_changed = 1;
			}
			else
			{
				memset(ss,0,sizeof(ss));
				i=GetValueFromData(ls,"SERVER3",',',1,'\0',ss);
				if(i)
				{
					Ql_strncpy(VTSData.ServerData.Port3, ss, sizeof(VTSData.ServerData.Port3) - 1);
					VTSData.ServerData.Port3[sizeof(VTSData.ServerData.Port3) - 1] = '\0';
				s3_changed = 1;
				}
			}

			#ifdef EXTENDED_IPS
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER4",' ',0,',',ss);
			if(i)
			{
				Ql_strncpy(VTSData.ServerData.IP4, ss, sizeof(VTSData.ServerData.IP4) - 1);
				VTSData.ServerData.IP4[sizeof(VTSData.ServerData.IP4) - 1] = '\0';
				s4_changed = 1;
			}
			memset(ss,0,sizeof(ss));
			i=GetValueFromData(ls,"SERVER4",',',1,',',ss);
			if(i)
			{
				Ql_strncpy(VTSData.ServerData.Port4, ss, sizeof(VTSData.ServerData.Port4) - 1);
				VTSData.ServerData.Port4[sizeof(VTSData.ServerData.Port4) - 1] = '\0';
				s4_changed = 1;
			}
			else
			{
				memset(ss,0,sizeof(ss));
				i=GetValueFromData(ls,"SERVER4",',',1,'\0',ss);
				if(i)
				{
					Ql_strncpy(VTSData.ServerData.Port4, ss, sizeof(VTSData.ServerData.Port4) - 1);
					VTSData.ServerData.Port4[sizeof(VTSData.ServerData.Port4) - 1] = '\0';
				s4_changed = 1;
				}
			}
			Ql_sprintf(SimData,"Update IP1: %s, %s\nIP2: %s,%s\nIP3: %s,%s\n,IP4: %s,%s",VTSData.ServerData.IP1,VTSData.ServerData.Port1,VTSData.ServerData.IP2,VTSData.ServerData.Port2,VTSData.ServerData.IP3,VTSData.ServerData.Port3,VTSData.ServerData.IP4,VTSData.ServerData.Port4);
			#else
			Ql_sprintf(SimData,"Update IP1: %s, %s\nIP2: %s,%s\nIP3: %s,%s",VTSData.ServerData.IP1,VTSData.ServerData.Port1,VTSData.ServerData.IP2,VTSData.ServerData.Port2,VTSData.ServerData.IP3,VTSData.ServerData.Port3);
			#endif
			SendResponce(SMSSender,SimData,IsServer,1);
			UpdateConfigInFlash();
			if(s1_changed) ReinitSingleSocket(0);
			if(s2_changed) ReinitSingleSocket(1);
			if(s3_changed) ReinitSingleSocket(2);
			#ifdef EXTENDED_IPS
			if(s4_changed) ReinitSingleSocket(3);
			#endif

			return 1;
		}
		ls = Ql_strstr(fn,"APN");
		if(ls)
		{
			if(Ql_strstr(ls,"AUTO"))
			{
				VTSData.AutoAPN=1;
				UpdateConfigInFlash();
				SendResponce(SMSSender,"APN Set to AUTO, Restarting...",IsServer,1);
				ThreadSleep(5000);
				Ql_Reset(0);
				return 1;
			}
			i=GetValueFromData(ls,"APN",' ',0,'\0',ss);
			if(i)
			{
				if(Ql_strlen(ss) > 1 && Ql_strlen(ss) < 20)
				{
					VTSData.AutoAPN=0;
					Ql_strcpy(VTSData.mAPN,ss);
					UpdateConfigInFlash();
					Ql_sprintf(SimData,"APN Changed to %s, Restarting...",VTSData.mAPN);
					SendResponce(SMSSender,SimData,IsServer,1);
					ThreadSleep(5000);
					Ql_Reset(0);
				}
				else
					SendResponce(SMSSender,"INVALID APN",IsServer,0);

				return 1;
			}

		}
		ls=Ql_strstr(fn,"INTERVAL");
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
			Ql_sprintf(SimData,"INTERVAL CHANGE %d,%d,%d,%d,%d",VTSData.IntervalData.DataInterval,VTSData.IntervalData.IgnitionInterval,VTSData.IntervalData.SOSInterval,
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
			Ql_sprintf(SimData,"INTERVAL CHANGE %d,%d,%d,%d,%d,%d",VTSData.IntervalData.MotionInterval,VTSData.IntervalData.HaltInterval,VTSData.IntervalData.EnergencyInterval,
															VTSData.IntervalData.SleepInterval,VTSData.IntervalData.HealthInterval,VTSData.IntervalData.FullDataPacketInterval);
			#endif
			SendResponce(SMSSender,SimData,IsServer,1);
			return 1;
		}
		ls=Ql_strstr(fn,"BEHAVE");
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
			//SendGyroSettingPacket(); 

			Ql_sprintf(SimData,"BHV Updated %d,%d,%d,%3.0f",VTSData.VehicleData.HarshAcc,VTSData.VehicleData.HarshBreak,VTSData.VehicleData.RashTurn,VTSData.VehicleData.OverSpeed);
			SendResponce(SMSSender,SimData,IsServer,1);
			return 1;
		}

		ls=Ql_strstr(fn,"VEHREG");
		if(ls)
		{
			i=GetValueFromData(ls,"VEHREG",' ',0,'\0',ss);
			if(i)
			{
				Ql_strcpy(VTSData.VehicleData.VehicleRegNo,ss);
				
				
				UpdateConfigInFlash();   
				Ql_sprintf(SimData,"Vehicle Number Updated : %s",VTSData.VehicleData.VehicleRegNo);
				SendResponce(SMSSender,SimData,IsServer,1);
				return 1;
			}
		}
		ls=Ql_strstr(fn,"SOSSET");
		if(ls)
		{
			// Check if a comma is present in the command
			char* comma_ptr = Ql_strchr(ls, ',');
			
			if (comma_ptr)
			{
				i=GetValueFromData(ls,"SOSSET",' ',0,',',ss);
				if(i)
				{
					char formatted[25];
					if (Ql_strlen(ss) == 10 && ss[0] >= '0' && ss[0] <= '9')
						Ql_sprintf(formatted, "+91%s", ss);
					else
						Ql_strcpy(formatted, ss);
					Ql_strcpy(VTSData.PhoneNumber.Mob0,formatted);
				}
				i=GetValueFromData(ls,"SOSSET",',',1,'\0',ss);
				if(i)
				{
					char formatted[25];
					if (Ql_strlen(ss) == 10 && ss[0] >= '0' && ss[0] <= '9')
						Ql_sprintf(formatted, "+91%s", ss);
					else
						Ql_strcpy(formatted, ss);
					Ql_strcpy(VTSData.PhoneNumber.Mob1,formatted);
				}
			}
			else
			{
				i=GetValueFromData(ls,"SOSSET",' ',0,'\0',ss);
				if(i)
				{
					char formatted[25];
					if (Ql_strlen(ss) == 10 && ss[0] >= '0' && ss[0] <= '9')
						Ql_sprintf(formatted, "+91%s", ss);
					else
						Ql_strcpy(formatted, ss);
					
					Ql_strcpy(VTSData.PhoneNumber.Mob0,formatted);
					Ql_strcpy(VTSData.PhoneNumber.Mob1,formatted);
				}
			}
			
			UpdateConfigInFlash();
			Ql_sprintf(SimData,"SOS Number Updated : %s, %s",VTSData.PhoneNumber.Mob0,VTSData.PhoneNumber.Mob1);
			SendResponce(SMSSender,SimData,IsServer,1);
			return 1;
		}
		ls=Ql_strstr(fn,"FOTAUPDATE");
		if(ls)
		{
			
		}
		ls=Ql_strstr(fn,"VRESET");
		if(ls)
		{
			QueueVResetResponse(SMSSender, IsServer);
			return 1;
		}
		
		ls=Ql_strstr(fn,"DEFAULT");
		if(ls)
		{
			SendResponce(SMSSender,"Device Reverted to Default, Restarting...",IsServer,1);
			LoadDefault();
			ThreadSleep(5000);
			Ql_Reset(0);//Restart
			return 1;
		}
		ls=Ql_strstr(fn,"SOSCLR");
		if(ls)
		{
			LOGData(TAG_OTA,"SOS Clear Command Received");
			ResetSOS();
			SendResponce(SMSSender,"SOS Clear",IsServer,1);
			return 1;
		}
		ls=Ql_strstr(fn,"SOSTIMEOUT");
		if(ls)
		{
			i=GetValueFromData(ls,"SOSTIMEOUT",' ',0,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if((i > 10) && ( i < UINT16_MAX))
				{
					SetSOSTimeOutSeconds((uint16_t)i);
					Ql_strcpy(CMD_Buff,"SOS Timeout Changed to ");
					Ql_strcat(CMD_Buff,ss);
					
					UpdateConfigInFlash();
					SendResponce(SMSSender,CMD_Buff,IsServer,1);
					return 1;
				}
			}
		}
		ls=Ql_strstr(fn,"SOSDISABLE");
		if(ls)
		{
			i=GetValueFromData(ls,"SOSDISABLE",' ',0,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if(i == 0 || i == 1)
				{
					VTSData.DisableSOS = i;
					UpdateConfigInFlash();
					Ql_sprintf(SimData,"SOS Disable set to %d", VTSData.DisableSOS);
					SendResponce(SMSSender,SimData,IsServer,1);
					return 1;
				}
			}
			SendResponce(SMSSender,"Invalid Param",IsServer,0);
			return 1;
		}
		ls=Ql_strstr(fn,"SOSSMS");
		if(ls)
		{
			if(Ql_strstr(fn,"GET") || !GetValueFromData(ls,"SOSSMS",' ',0,'\0',ss))
			{
#if SOS_SMS_FEATURE_ENABLED
				SendResponce(SMSSender, VTSData.SOSSmsEnabled ? "SOS SMS: ON" : "SOS SMS: OFF", IsServer, 1);
#else
				SendResponce(SMSSender, "SOS SMS: DISABLED IN FIRMWARE", IsServer, 1);
#endif
				return 1;
			}
			i=GetValueFromData(ls,"SOSSMS",' ',0,'\0',ss);
			if(i && (ss[0] == '0' || ss[0] == '1') && ss[1] == '\0')
			{
#if SOS_SMS_FEATURE_ENABLED
				VTSData.SOSSmsEnabled = ss[0] - '0';
				UpdateConfigInFlash();
				SendResponce(SMSSender, VTSData.SOSSmsEnabled ? "SOS SMS Enabled" : "SOS SMS Disabled", IsServer, 1);
#else
				if (ss[0] == '1')
				{
					SendResponce(SMSSender, "SOS SMS Disabled In Firmware", IsServer, 0);
				}
				else
				{
					VTSData.SOSSmsEnabled = 0;
					UpdateConfigInFlash();
					SendResponce(SMSSender, "SOS SMS Disabled", IsServer, 1);
				}
#endif
				return 1;
			}
			SendResponce(SMSSender,"Invalid Param (0/1)",IsServer,0);
			return 1;
		}
		ls = Ql_strstr(fn,"HISTORY");
		if(!ls) ls = Ql_strstr(fn,"HIST");
		if(ls)
		{
			if(Ql_strstr(fn,"GET"))
			{
				Ql_sprintf(SimData,"History: %s", VTSData.DisableHistory ? "DISABLED" : "ENABLED");
				SendResponce(SMSSender,SimData,IsServer,1);
				return 1;
			}
			i = GetValueFromData(ls,"HISTORY",' ',0,'\0',ss);
			if(!i) i = GetValueFromData(ls,"HIST",' ',0,'\0',ss);
			if(i && (ss[0] == '0' || ss[0] == '1') && ss[1] == '\0')
			{
				VTSData.DisableHistory = (ss[0] == '0') ? 1 : 0;
				UpdateConfigInFlash();
				SendResponce(SMSSender, VTSData.DisableHistory ? "History Disabled" : "History Enabled", IsServer, 1);
				return 1;
			}
			if(Ql_strstr(fn,"CLR") || Ql_strstr(fn,"CLEAR"))
			{
				#ifndef HISTORY_DISABLED
				DeleteAllPackets();
				SendResponce(SMSSender,"History Cleared",IsServer,1);
				#else
				SendResponce(SMSSender,"History Clear Not Supported",IsServer,0);
				#endif
				return 1;
			}
			SendResponce(SMSSender,"Invalid Param (0/1 or CLR)",IsServer,0);
			return 1;
		}
	
		ls = Ql_strstr(fn,"FTK");
		if(ls)
		{
			#ifndef HISTORY_DISABLED
			fn = strchr(fn,'\0');
			if(fn)
			{
				if((fn - ls) > 6)
				{
					
					SendResponce(SMSSender,"Invalid Param FTK",IsServer,0);
					LOGData(TAG_OTA,"Invalid Param for FTK");
					return 0;
				}
				else
				{
					Ql_strcpy(ss,&ls[4]);
					f=(double)atoi(ss);
					if((f >= 5) && ( f < 60*60))
					{
						if(FTKConfig.IsEnabled == 1)
						{
							SendResponce(SMSSender,"ftk already enabled. disable first...",IsServer,0);
							return 0;
						}
						EnableFTKLogs(f);
						Ql_sprintf(CMD_Buff,"Config ftk set to %f",f);
						SendResponce(SMSSender,CMD_Buff,IsServer,1);
						return 1;
					}
					else
					{
						LOGData(TAG_OTA,"Invalid Param for FTK.2");
						SendResponce(SMSSender,"Invalid Param for FTK 2",IsServer,0);
					}

					SendResponce(SMSSender,"ftk Invalid Param",IsServer,0);
					return 0;						
					
				}
			}
			#endif
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
					SendResponce(SMSSender,"Zig Test Mode Disabled",IsServer,1);
				}
				else if(i == 1)
				{
					ZigTestMode = 1;
					SendResponce(SMSSender,"Zig Test Mode Enabled",IsServer,1);
				}
				else
				{
					SendResponce(SMSSender,"Invalid Param",IsServer,0);
				}
			}
			return 1;
		}
		ls = Ql_strstr(fn,"BTS ");
		if(ls)
		{
			fn = strchr(fn,'\0');
			if(fn)
			{
				if((fn - ls) > 6)
				{
					if(IsServer> OTA_SRC_SMS)
						return 0;
					SendSMS(SMSSender,"Invalid Param.");
					return 0;
				}
				else
				{
					Ql_strcpy(ss,&ls[4]);
					f=(double)atoi(ss)/10;
					if((f > 3.0F) && ( f < 4.2F))
					{
						VTSData.BattThrs =f;
					
						Ql_strcpy(CMD_Buff,"Battery Threshold Changed to ");
						Ql_sprintf(ss,"%01.1f",VTSData.BattThrs);
						Ql_strcat(CMD_Buff,ss);
						UpdateConfigInFlash();
						SendResponce(SMSSender,CMD_Buff,IsServer,1);
						return 1;
					}

					SendResponce(SMSSender,"Invalid Param",IsServer,0);
					return 0;						
					
				}
			}
		}
		ls = Ql_strstr(fn,"SLP ");
		if(ls)
		{
			fn = strchr(fn,'\0');
			if(fn)
			{
				
				Ql_strcpy(ss,&ls[4]);
				i=atoi(ss);
				
				Ql_strcpy(CMD_Buff,"Activating Sleep Mode for ");
				Ql_sprintf(ss,"%01i",i);
				Ql_strcat(CMD_Buff,ss);
				SendResponce(SMSSender,CMD_Buff,IsServer,1);
				ThreadSleep(1000);
				SleepModeON(i);
				return 1;				
					
				
			}
		}
		ls = Ql_strstr(fn,"GF:");
		if(ls)
		{
			DecodeGeofence(ls);
			UpdateConfigInFlash();
			SendResponce(SMSSender,"GeoFence Data Updated",IsServer,1);
			return 1;
		}	
		ls = Ql_strstr(fn,"SENS");
		if(ls)
		{
			f=0;
			i=GetValueFromData(ls,"SENS",' ',0,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i < UART2_MODE_MAX)
				{
					VTSData.SensorSetting.Uart2Mode = i;
					f=1;
				}
			}
			i=GetValueFromData(ls,"SENS",',',1,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i < IP2_MODE_MAX)
				{
					VTSData.SensorSetting.IP2Mode = i;
					f=1;
				}
			}
			i=GetValueFromData(ls,"SENS",',',2,',',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 1)
				{
					VTSData.SensorSetting.IGNInterval = i;
					f=1;
				}
			}
			i=GetValueFromData(ls,"SENS",',',3,'\0',ss);
			if(i)
			{
				i=atoi(ss);
				if(i > 9)
				{
					VTSData.SensorSetting.OFFInterval = i;
					f=1;
				}
			}

			if(f)
			{
				UpdateConfigInFlash();
				SendResponce(SMSSender,"Sensor Setting Changed",IsServer,1);
			}
			return 1;
		}	
	}
	fn = Ql_strstr(msg,"CLR");
	if(fn)
	{
		fn=fn+3;
		ls = Ql_strstr(fn,"HISTORY");
		if(!ls) ls = Ql_strstr(fn,"HIST");
		if(ls)
		{
			#ifndef HISTORY_DISABLED
			DeleteAllPackets();
			SendResponce(SMSSender,"History Cleared",IsServer,1);
			#else
			SendResponce(SMSSender,"History Clear Not Supported",IsServer,0);
			#endif
			return 1;
		}
		ls = Ql_strstr(fn,"GF");
		if(ls)
		{
			DecodeGeofence("GF");
			UpdateConfigInFlash();
			SendResponce(SMSSender,"GeoFence Data Cleared",IsServer,1);
			return 1;
		}	
		
		ls = Ql_strstr(fn,"SOS");
		if(ls)
		{
			ResetSOS();
			SendResponce(SMSSender,"SOS Data Cleared",IsServer,1);
			return 1;
		}
		
		ls = Ql_strstr(fn,"DISK");
		if(ls)
		{
			DiskCleanupResult cleanup = {0};
			double freed_mb;
			if (FTP_ClearRecoverableDiskData(&cleanup))
			{
				freed_mb = cleanup.freeSpaceAfter >= cleanup.freeSpaceBefore ?
					(double)(cleanup.freeSpaceAfter - cleanup.freeSpaceBefore) / (1024.0 * 1024.0) : 0.0;
				Ql_sprintf(SimData, "Disk Cleared. Free: %0.2f MB (+%0.2f MB), H:%u B:%u F:%u",
					(double)cleanup.freeSpaceAfter / (1024.0 * 1024.0),
					freed_mb,
					cleanup.historyFilesDeleted, cleanup.batchFilesDeleted, cleanup.transientFilesDeleted);
				SendResponce(SMSSender,SimData,IsServer,1);
			}
			else
			{
				Ql_sprintf(SimData, "Disk Clear Failed. Free: %0.2f MB, Errors:%u",
					(double)cleanup.freeSpaceAfter / (1024.0 * 1024.0), cleanup.failures);
				SendResponce(SMSSender,SimData,IsServer,0);
			}
			return 1;
		}
		/*
		ls = Ql_strstr(fn, "IMI");
		if (ls)
		{
			if(VTSData.DisableImiCmd == 1)
			{
				SendResponce(SMSSender,"IMI Command Disabled",IsServer,0);
				return 1;
			}
			VTSData.CustomImei.IsEnable = 0;
			Ql_memset(VTSData.CustomImei.Imei, 0, sizeof(VTSData.CustomImei.Imei));
			UpdateConfigInFlash();
			SendResponce(SMSSender, "Custom IMEI Cleared", IsServer, 1);
			GetDeviceIMEI();
			InitSockets();
			return 1;
		}
		*/
	}

	return 0;
}


// RS232/RS485 OTA handling functions
void SendRS232Response(char* response)
{
	if (!response || Ql_strlen(response) == 0)
	{
		LOGData(TAG_OTA, "Invalid RS232 response");
		return;
	}
	
	// Format response with protocol: $RES,msg\n
	char formatted_response[300];
	memset(formatted_response, 0, sizeof(formatted_response));
	Ql_sprintf(formatted_response, "$RES,%s\n", response);
	
	LOGData(TAG_OTA, "Queueing RS232 response: %s", formatted_response);
	
	// Queue response instead of sending immediately
	QueueRS232Response(formatted_response);
}

void SendRS485Response(char* response)
{
	if (!response || Ql_strlen(response) == 0)
	{
		LOGData(TAG_OTA, "Invalid RS485 response");
		return;
	}
	
	// Format response with protocol: $RES,msg\n
	char formatted_response[300];
	memset(formatted_response, 0, sizeof(formatted_response));
	Ql_sprintf(formatted_response, "$RES,%s\n", response);
	
	LOGData(TAG_OTA, "Sending RS485 response: %s", formatted_response);
	
	// Send response through RS485
	if (MCOMM_SendSerial(1, (uint8_t*)formatted_response, Ql_strlen(formatted_response)) != 1)
	{
		LOGData(TAG_OTA, "Failed to send RS485 response");
	}
}

/* Trim leading/trailing whitespace and strip a leading '$' framing char. */
static void NormalizeRS232Command(char *out, uint16_t outSize, const char *in)
{
	uint16_t start = 0, end, len;

	if(outSize == 0) return;
	if(in == NULL) { out[0] = '\0'; return; }

	len = (uint16_t)Ql_strlen(in);
	end = len;
	while(start < end && (in[start]=='\r'||in[start]=='\n'||in[start]==' '||in[start]=='\t')) start++;
	while(end > start && (in[end-1]=='\r'||in[end-1]=='\n'||in[end-1]==' '||in[end-1]=='\t')) end--;
	if(start >= end) { out[0] = '\0'; return; }

	len = end - start;
	if(len >= outSize) len = outSize - 1;
	Ql_memcpy(out, in + start, len);
	out[len] = '\0';

	if(out[0] == '$' && out[1] != '\0') {
		uint16_t i;
		for(i = 0; i < len; ++i) out[i] = out[i + 1];
	}
}

void ProcessRS232OTAData(void)
{
	if (!RS232_DataAvailable)
		return;

	LOGData(TAG_OTA, "Processing RS232 data, length: %d", RS232_Buffer.datalen);

	// Null-terminate the received data
	if (RS232_Buffer.datalen < MCOMM_COM_URT_EXG_BUFF_SIZE)
		RS232_Buffer.data[RS232_Buffer.datalen] = '\0';
	else
		RS232_Buffer.data[MCOMM_COM_URT_EXG_BUFF_SIZE - 1] = '\0';

	char normalized[MCOMM_COM_URT_EXG_BUFF_SIZE + 1] = {0};
	NormalizeRS232Command(normalized, sizeof(normalized), (char*)RS232_Buffer.data);
	if (normalized[0] == '\0') {
		RS232_DataAvailable = 0;
		return;
	}
	char* d = normalized;

	// CLS fuel-sensor response (starts with '@') — route to the sensor parser.
	if (d[0] == '@' && ParseCLSResponse(d, Ql_strlen(d))) {
		LOGData(TAG_OTA, "RS232 data handled as CLS Sensor response");
		RS232_DataAvailable = 0;
		return;
	}

	LOGData(TAG_OTA, "RS232 OTA command: %s", d);

	uint8_t result = 0;
	if (d[0] == '$' && Ql_strstr(d, NetWork.IMEI)) {
		DecodeOTAData(d, OTA_SRC_RS232);
		result = 1;
	} else {
		result = DecodeSMS(d, OTA_SRC_RS232);
	}

	// Only reply ERROR when the input actually looked like a command (avoid
	// spurious replies to RS232 line noise).
	if (!result) {
		if (Ql_strstr(d,"SETSENSOR")||Ql_strstr(d,"SETSOS")||Ql_strstr(d,"SETSNS")||
		    Ql_strstr(d,"SET")||Ql_strstr(d,"GET")||Ql_strstr(d,"CLR")||Ql_strstr(d,"APN"))
			SendRS232Response("ERROR: Unknown command");
	}

	RS232_DataAvailable = 0;
}

void ProcessRS485OTAData(void)
{
	if (!RS485_DataAvailable)
		return;
		
	LOGData(TAG_OTA, "Processing RS485 OTA data, length: %d", RS485_Buffer.datalen);
	
	// Null-terminate the received data
	if (RS485_Buffer.datalen < MCOMM_COM_URT_EXG_BUFF_SIZE)
	{
		RS485_Buffer.data[RS485_Buffer.datalen] = '\0';
	}
	else
	{
		RS485_Buffer.data[MCOMM_COM_URT_EXG_BUFF_SIZE - 1] = '\0';
	}
	
	char* d = (char*)RS485_Buffer.data;
	LOGData(TAG_OTA, "RS485 OTA command: %s", d);

	// Route SET/GET/CLR to the OTA decoder; everything else to the SMS decoder.
	uint8_t result = 0;
	if (Ql_strncmp(d, "SET ", 4) == 0 || Ql_strncmp(d, "GET ", 4) == 0 || Ql_strncmp(d, "CLR ", 4) == 0) {
		DecodeOTAData(d, OTA_SRC_RS485);
		result = 1;
	} else {
		result = DecodeSMS(d, OTA_SRC_RS485);
	}

	if (!result)
		SendRS485Response("ERROR: Unknown command");

	RS485_DataAvailable = 0;
}

