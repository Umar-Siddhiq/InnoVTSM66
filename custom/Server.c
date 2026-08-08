#include "Server.h"
#include "File.h"
#if defined(PROTO_CDAC)
#include "HttpQueue.h"
#endif


// Helper function prototypes
static void handleProfileRequests(void);
static void handleFTPRequests(void); 
static void handleIncomingMessages(void);
static void handleRFIDData(void);
static void handleLoginRequests(void);
static void handlePackets(void);
static void handleServerResponses(void);
static void handleNormalPackets(void);
static void handleHealthPackets(void);
void InitBuffer(uint8_t alt);
void LoginString(void);
void CheckAlerts(void);
void EmergencyPacket(uint8_t IsOff);
void SendDatatoServer0(void);
void ProcessHistoryPacket(void);
void DecodeGeofence(char* data);
void GetActiveGeoID(char* buff);
void InsertCurrentDateTime_OLD(char* target, uint8_t IsTime);

#if defined(PROTO_CDAC)
static void handleCriticalPackets(void);
static void handleNormalCDACPackets(void);
static void handleHealthCDACPackets(void);
static void handleFullCDACPackets(void);
static void handleCDACProtocol(void);

// Optional helper functions for CDAC protocol
static void handleSOSAlerts(void);
static void handleNonSOSAlerts(void);
static void handleRegularPackets(void);
static void handleRepeatingCriticalPackets(void);
#endif

/* Returns 1 if the URL string starts with http:// or https://, 0 for bare IP:port */
static uint8_t IsHttpUrl(const char* url)
{
    if (!url) return 0;
    return (Ql_strncmp(url, "http://", 7) == 0 || Ql_strncmp(url, "https://", 8) == 0);
}

char Server1RxData[RECV_BUFFER_LEN] = {0};
char Server2RxData[RECV_BUFFER_LEN] = {0};
char Server3RxData[RECV_BUFFER_LEN] = {0};
int lastcrc;

uint16_t IsCritical = 0;
uint16_t IsPackeAlert = 0;

#if defined(ENABLE_UNIFIED_FIRMWARE)
#define _REGULAR_SIZE	109
uint8_t IsSendProcess = 0;
uint32_t FrameNumber = 1;
OTATypeDef OTAValue;
extern VehicleTypeDef VehicleState;
extern volatile char VehicleMovingMode;
uint8_t IsOverSpeed;
uint16_t DeltaDis = 0;
uint16_t StoredHistoryDataCount = 0;
uint8_t IsEMRSend = 0, IsEMRTSend = 0, CNFChange = 0;
uint8_t IsStored = 0;
char SendString[DATA_MAX_BUFF];
char CriticalString[5][CRITICAL_MAX_BUFF];
char ActivationKey[18];
char dataBuffer[DATA_MAX_BUFF];
#else
#ifdef PROTO_CDAC
#define _REGULAR_SIZE	109
volatile uint8_t IsSendProcess = 0;
uint32_t FrameNumber = 1;
OTATypeDef OTAValue;
extern VehicleTypeDef VehicleState;
extern volatile char VehicleMovingMode;
uint8_t IsOverSpeed = 0;
char SendString[DATA_MAX_BUFF];
char CriticalString[5][CRITICAL_MAX_BUFF];
uint8_t CriticalAlertIdx[5]; /* alert index that produced each CriticalString slot */
char ActivationKey[18];
#else
uint16_t DeltaDis = 0;
uint16_t StoredHistoryDataCount = 0;
uint8_t IsEMRSend = 0, IsEMRTSend = 0, CNFChange = 0, IsOverSpeed = 0;
uint32_t FrameNumber = 0;
uint8_t IsStored = 0;
char dataBuffer[DATA_MAX_BUFF];
#endif
#endif

double prevLat=0,prevLong=0;
double fGPSLat=0,fGPSLong=0,fGPSAlt=0,fGPSpdop=0,fGPShdop=0,fGPSSats=0, fGPSSpeed=0, fGPSHeading=0, fGPSForce=0;


uint8_t IsServerRes=0;

extern volatile uint8_t ServerThreadTimeout;

uint8_t MemoryPercent;
uint16_t GetMemeryPercentage(void);
#if defined(PROTO_CDAC)
uint8_t SendDataToServer(char* data, uint8_t KeepAlive, uint16_t currentIntervalSec);
#else
uint8_t SendDataToServer(char* data, uint8_t KeepAlive);
#endif
volatile uint8_t SendLogin1;
volatile uint8_t SendLogin2;
#ifdef EXTENDED_IPS
volatile uint8_t SendLogin3;
#endif

unsigned short CRC16(char* buf, int len)
{
	register int counter;
	register unsigned short crc = 0;
	for( counter = 0; counter < len; counter++)
		crc = (crc<<8) ^ crc16tab[((crc>>8) ^ *(char *)buf++)&0x00FF];
	return crc;
}

unsigned short chksum(unsigned char *data, int len)
{
    unsigned char chk[2]={0,0};
    for(int i =0; i < len; i++)
    {
        chk[0] += data[i];
        chk[1] += chk[0];
        
    }
    chk[0] &= 0xff;
    chk[1] &= 0xff;
    return (chk[0] << 8)| (chk[1]);
}

uint32_t checksum32(const uint8_t *data, size_t length) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum += data[i];
        checksum = (checksum >> 1) | (checksum << 31); // Rotate right
    }
    return checksum;
}

char CRC8(const char *data,int length) 
{
   char crc = 0x00;
   char extract;
   char sum;
   for(int i=0;i<length;i++)
   {
      extract = *data;
      for (char tempI = 8; tempI; tempI--) 
      {
         sum = (crc ^ extract) & 0x01;
         crc >>= 1;
         if (sum)
            crc ^= 0x8C;
         extract >>= 1;
      }
      data++;
   }
   return crc;
}
#if defined(PROTO_CDAC)
void InsertStringValue(const char* value, uint16_t position, uint16_t length, uint8_t wh)
{
	uint16_t n=0,i=0;
	char vl;
	if(!value)
		return;
	uint16_t ln=Ql_strlen(value);
	if(ln > 0)
	{
		if(ln <= length)
			n=length-ln;
		else
			ln=length;

		position = position + n;
		
		// Bounds check before writing
		if (position + ln > DATA_MAX_BUFF)
		{
			LOGData(TAG_SERVER, "InsertStringValue: Buffer overflow prevented, pos=%d, len=%d", position, ln);
			return;
		}
		
		while(i<ln)
		{
			vl=value[i++];
			if((wh) && (vl=='-'))
				vl='0';
			SendString[position++]=vl;
		}
	}
}

#ifdef ENABLE_UNIFIED_FIRMWARE
void InsertIntValueCDAC(uint16_t value, uint16_t position, uint16_t length)
#else
void InsertIntValue(uint16_t value, uint16_t position, uint16_t length)
#endif
{
	uint16_t n=0, i=0;
	uint16_t ln;
	char ss[20];
	const char ls[]="%d";
	Ql_sprintf(ss,ls,value);
	ln=Ql_strlen(ss);
	if(ln > 0)
	{
		if(ln <= length)
			n=length-ln;
		else
			ln=length;

		position = position + n;
		
		// Bounds check before writing
		if (position + ln > DATA_MAX_BUFF)
		{
			LOGData(TAG_SERVER, "InsertIntValue: Buffer overflow prevented");
			return;
		}
		
		while(i<ln)
		{
			SendString[position++]=ss[i++];
		}
	}
}

#ifdef ENABLE_UNIFIED_FIRMWARE
void InsertFloatValueCDAC(double value, uint16_t position, uint16_t length,const char* decimal)
#else
void InsertFloatValue(double value, uint16_t position, uint16_t length,const char* decimal)
#endif
{
	uint16_t n=0, i=0;
	uint16_t ln;
	char ss[20];
	Ql_sprintf(ss,decimal,value);
	ln=Ql_strlen(ss);
	if(ln > 0)
	{
		if(ln <= length)
			n=length-ln;
		else
			ln=length;
		position = position + n;
		
		// Bounds check before writing
		if (position + ln > DATA_MAX_BUFF)
		{
			LOGData(TAG_SERVER, "InsertFloatValue: Buffer overflow prevented");
			return;
		}
		
		while(i<ln)
		{
			SendString[position++]=ss[i++];
		}
	}
}

static void InsertCurrentDateTimeAt(uint16_t position)
{
	char tempData[16];
	Ql_sprintf(tempData,"%02d%02d%02d",CurrentDateTime.Date,CurrentDateTime.Month,CurrentDateTime.Year);
	InsertStringValue(tempData,position,6,0);
	Ql_sprintf(tempData,"%02d%02d%02d",CurrentDateTime.Hour,CurrentDateTime.Min,CurrentDateTime.Sec);
	InsertStringValue(tempData,position+6,6,0);
}
#endif
void InsertHEXStringToBuffer(char *buffer, uint8_t *data, uint16_t len)
{
	char ss[5];
	for(int i=0;i<len;i++)
	{
		Ql_sprintf(ss,"%02X",data[i]);
		Ql_strcat(buffer,ss);
	}
}

void ConnectedCallback(int socketno)
{
    if(socketno == 0 || socketno == 2)
    {
        hw_led_state_set(GSMLED,1,10,10);
    }
	if(socketno == 0)
	{
		SendLogin1 = 1;
	}
	else if(socketno == 2)
	{
		SendLogin2 = 1;
	}
	else if(socketno == 3)
	{
		#ifdef EXTENDED_IPS
		SendLogin3 = 1;
		#endif
	}
	LOGData(TAG_SERVER,"Socket %d Connected Callback!!",socketno);
    if ((socketno == 0 || socketno == 2 || socketno == 3) &&
        VTSState.CurrentProfile != 0)
    {
        MarkActiveProfile(VTSState.CurrentProfile, ACTIVE_PROFILE_REASON_SERVER);
    }
}

void whileConnected(int socketno)
{
    if(socketno == 0 || socketno == 2)
    {
        //ServerHangtimeout = 0;
    }
}

void DisConnectedCallback(int socketno)
{
    
    
}

void InitSockets(void)
{
#ifdef PROTO_CDAC
    UpdateURL(VTSData.ServerData.IP1);
    UpdateSecondaryURL(VTSData.ServerData.IP3);
    #ifdef EXTENDED_IPS
    UpdateTertiaryURL(VTSData.ServerData.IP4);
    #endif
#endif
    if(VTSData.ServerData.IPConfig[0])
        ServerSocket[0].isEnabled=1;
    ServerSocket[0].SocketNo = 0;
    ServerSocket[0].SocketIndex = -1;
    ServerSocket[0].SocketState = SOCKET_CLOSED;
    Ql_strncpy(ServerSocket[0].DNSorIP, VTSData.ServerData.IP1, sizeof(ServerSocket[0].DNSorIP) - 1);
    ServerSocket[0].DNSorIP[sizeof(ServerSocket[0].DNSorIP) - 1] = '\0';
    ServerSocket[0].Port = Ql_atoi(VTSData.ServerData.Port1);
    ServerSocket[0].OnConnect = &ConnectedCallback;
    ServerSocket[0].Connected = &whileConnected;
    ServerSocket[0].OnDisconnect = &DisConnectedCallback;
    ServerSocket[0].rxSizeMAX = RECV_BUFFER_LEN;
    ServerSocket[0].rxBuffer = Server1RxData;
    
    if(VTSData.ServerData.IPConfig[1])
        ServerSocket[1].isEnabled=1;
    ServerSocket[1].SocketNo = 1;
    ServerSocket[1].SocketIndex = -1;
    ServerSocket[1].SocketState = SOCKET_CLOSED;
    Ql_strncpy(ServerSocket[1].DNSorIP, VTSData.ServerData.IP2, sizeof(ServerSocket[1].DNSorIP) - 1);
    ServerSocket[1].DNSorIP[sizeof(ServerSocket[1].DNSorIP) - 1] = '\0';
    ServerSocket[1].Port = Ql_atoi(VTSData.ServerData.Port2);

    /* ServerSocket[2] (Server 3): TCP if bare IP:port, HTTP if http(s):// URL */
    if(VTSData.ServerData.IPConfig[2] && !IsHttpUrl(VTSData.ServerData.IP3))
        ServerSocket[2].isEnabled = 1;
    else
        ServerSocket[2].isEnabled = 0;
    ServerSocket[2].SocketNo = 2;
    ServerSocket[2].SocketIndex = -1;
    ServerSocket[2].SocketState = SOCKET_CLOSED;
    Ql_strncpy(ServerSocket[2].DNSorIP, VTSData.ServerData.IP3, sizeof(ServerSocket[2].DNSorIP) - 1);
    ServerSocket[2].DNSorIP[sizeof(ServerSocket[2].DNSorIP) - 1] = '\0';
    ServerSocket[2].Port = Ql_atoi(VTSData.ServerData.Port3);
    ServerSocket[2].OnConnect = ServerSocket[2].isEnabled ? &ConnectedCallback : NULL;
    ServerSocket[2].OnDisconnect = ServerSocket[2].isEnabled ? &DisConnectedCallback : NULL;
    ServerSocket[2].rxSizeMAX = RECV_BUFFER_LEN;
    ServerSocket[2].rxBuffer = Server2RxData;
    #ifdef EXTENDED_IPS
    /* ServerSocket[3] (Server 4): TCP if bare IP:port, HTTP if http(s):// URL */
    ServerSocket[3].isEnabled = !IsHttpUrl(VTSData.ServerData.IP4) ? 1 : 0;
    ServerSocket[3].SocketNo = 3;
    ServerSocket[3].SocketIndex = -1;
    ServerSocket[3].SocketState = SOCKET_CLOSED;
    Ql_strncpy(ServerSocket[3].DNSorIP, VTSData.ServerData.IP4, sizeof(ServerSocket[3].DNSorIP) - 1);
    ServerSocket[3].DNSorIP[sizeof(ServerSocket[3].DNSorIP) - 1] = '\0';
    ServerSocket[3].Port = Ql_atoi(VTSData.ServerData.Port4);
    ServerSocket[3].OnConnect = NULL;
    ServerSocket[3].OnDisconnect = NULL;
    ServerSocket[3].rxSizeMAX = RECV_BUFFER_LEN;
    ServerSocket[3].rxBuffer = Server3RxData;
    
    #endif  
}


#ifndef PROTO_CDAC
// void SensorString(void)
// {
// 	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
// 	strcpy(dataBuffer,"$SENS,");
// 	Ql_strncat(dataBuffer,NetWork.IMEI,15);
// 	InsertChar(dataBuffer,',');
// 	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
// 	InsertChar(dataBuffer,',');
// 	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
// 	InsertChar(dataBuffer,',');
// 	InsertCurrentDateTime(dataBuffer,0);
// 	InsertChar(dataBuffer,',');
// 	InsertCurrentDateTime(dataBuffer,1);
	
// 	Ql_strcat(dataBuffer,",{");
// 	if(DHT11.Status)
// 	{
// 		InsertIntValue(dataBuffer,DHT11.temp,"%01d,");
// 		InsertIntValue(dataBuffer,DHT11.humidity,"%01d},{");
// 	}
// 	else
// 		Ql_strcat(dataBuffer,"0,0},{");
// 	if(IsFuelData)
// 		Ql_strcat(dataBuffer,FuelData);
// 	else
// 		InsertChar(dataBuffer,'0');
// 	Ql_strcat(dataBuffer,"}*");

// }
#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_MAHARASHTRA1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void LoginStringMH(void)
#else
void LoginString(void)
#endif
{
	char ss[18];
	uint8_t crc;
	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcpy(dataBuffer,"$LGN,");
	Ql_strcat(dataBuffer,VTSData.VendorID);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	//AppendVariableString(dataBuffer,FirmVer,6,3,"V1.6.2");
	Ql_strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,PROTOVER);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	// InsertChar(dataBuffer,'*');
	crc = CRC8(dataBuffer,Ql_strlen(dataBuffer));
	lastcrc= crc;
	//Ql_sprintf(ss,"*%02X",crc);
	Ql_sprintf(ss,"*");
	Ql_strcat(dataBuffer,ss);
	// FrameNumber = 1;
}
#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_NIC1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void LoginStringNIC(void)
#else
void LoginString(void)
#endif
{
	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcpy(dataBuffer,"$LGN,");
	Ql_strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	//AppendVariableString(dataBuffer,FirmVer,6,3,"V1.6.2");
	Ql_strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,PROTOVER);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,sLatitude);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,sLongitude);
	InsertChar(dataBuffer,'*');
	// FrameNumber = 1;
}
#endif

#if defined(PROTO_CDAC)
void LoginPacket(void)
{
	// vltdata=
	uint16_t dLen=8;
	if(Ql_strlen(ActivationKey)<12)
		strcpy(ActivationKey,"1234567890123456");
	Ql_memset(SendString,0,DATA_MAX_BUFF);	
	Ql_memset(SendString,'0',150);
	InsertStringValue("vltdata=LGN",0,11,0);
	InsertStringValue(NetWork.IMEI,dLen + 3,15,1);
	InsertStringValue(ActivationKey,dLen + 18,16,1);
	InsertFloatValueCDAC(GPS.Latitude,dLen + 34,10,"%010.6f");
	SendString[dLen + 44]=GPS.LatDir;
	InsertFloatValueCDAC(GPS.Longitude,dLen + 45,10,"%010.6f");
	SendString[dLen + 55]=GPS.LngDir;
	InsertCurrentDateTimeAt(dLen + 56);
	InsertFloatValueCDAC(GPS.Speed,dLen + 68,6,"%06.2f");
	SendString[dLen + 74]=0;
}

static void InsertMNCCDAC(uint16_t mnc, uint16_t offset)
{
	char buff[4];
	if (mnc < 10)
		Ql_sprintf(buff, "xx%d", mnc);
	else if (mnc < 100)
		Ql_sprintf(buff, "x%02d", mnc);
	else
		Ql_sprintf(buff, "%03d", mnc);
	InsertStringValue(buff, offset, 3, 0);
}

void DataPacket(void)
{
	uint16_t dLen=8;
	//Ql_memset(SendString,'0',100);
	//GetAlertsHeader();
	InsertStringValue(NetWork.IMEI,dLen + 3,15,0);
	SendString[dLen + 21]=GPS.GPSFix + '0';
	InsertCurrentDateTimeAt(dLen + 22);
	InsertFloatValueCDAC(GPS.Latitude,dLen + 34,10,"%010.6f");
	SendString[dLen + 44]=GPS.LatDir;
	InsertFloatValueCDAC(GPS.Longitude,dLen + 45,10,"%010.6f");
	SendString[dLen + 55]=GPS.LngDir;
	InsertIntValueCDAC(GSM.MCC,dLen + 56,3);
	InsertMNCCDAC(GSM.MNC,dLen + 59);
	InsertStringValue(GSM.LAC,dLen + 62,4,1);
	InsertStringValue(GSM.CellID,dLen + 66,9,1);
	InsertFloatValueCDAC(GPS.Speed,dLen + 75,6,"%03.2f");
	InsertFloatValueCDAC(GPS.Heading,dLen + 81,6,"%03.2f");
	InsertIntValueCDAC(GPS.NoOfSatalite,dLen + 87,2);
	InsertIntValueCDAC((uint16_t)GPS.HDOP,dLen + 89,2);
	InsertIntValueCDAC(GSM.SignalStrength,dLen + 91,2);
	SendString[dLen + 93]=PeriPheralVal.IGN + '0';
	SendString[dLen + 94]=PeriPheralVal.IsMain + '0';
	SendString[dLen + 95]=VehicleMovingMode;	
	InsertFloatValueCDAC(GPS.Altitude,dLen + 96,7,"%04.2f");
	char spn[7];
	Ql_sprintf(spn,"%s",NetWork.Network);
	int rem = 6 - Ql_strlen(NetWork.Network);
	for(int i =0; i < rem; i++)
		spn[5 - i] = 'X';

	// SendString[dLen + 103] = 0;
	// Ql_strncat(SendString,NetWork.Network,6);
	
	
	// SendString[dLen+110] = 0;
	InsertStringValue(spn,dLen + 103,6,1);
}
#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_OG)
#ifdef ENABLE_UNIFIED_FIRMWARE
void LoginStringOG(void)
#else
void LoginString(void)
#endif
{
	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcpy(dataBuffer,"$");
	Ql_strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,'$');
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,'$');
	Ql_strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,'$');
	Ql_strcat(dataBuffer,"AIS140");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,'$');
	Ql_strcat(dataBuffer,sLatitude);
	InsertChar(dataBuffer,'N');
	Ql_strcat(dataBuffer,sLongitude);
	InsertChar(dataBuffer,'E');
}
#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_ODISA1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void LoginStringOD(void)
#else
#if !defined(PROTO_MAHARASHTRA1) && !defined(PROTO_NIC1) && !defined(PROTO_CDAC) && !defined(PROTO_OG)
void LoginString(void)
#endif
#endif
#if defined(ENABLE_UNIFIED_FIRMWARE) || (!defined(PROTO_MAHARASHTRA1) && !defined(PROTO_NIC1) && !defined(PROTO_CDAC) && !defined(PROTO_OG))
{
	char ss[18];
	uint32_t crc;
	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcpy(dataBuffer,"$LGN,");
	Ql_strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	//AppendVariableString(dataBuffer,FirmVer,6,3,"V1.6.2");
	Ql_strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,"AIS140");
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,sLatitude);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,sLongitude);
	InsertChar(dataBuffer,',');
	// InsertChar(dataBuffer,'*');
	crc = checksum32(dataBuffer,Ql_strlen(dataBuffer));
	Ql_sprintf(ss,"%08X*",crc);
	lastcrc= crc;
	Ql_strcat(dataBuffer,ss);
	InsertChar(dataBuffer,'\n');
	// FrameNumber = 1;
}
#endif
#endif

#ifdef ENABLE_UNIFIED_FIRMWARE
void LoginString(void)
{
	if (IS_PROTO_NIC()) {
		LoginStringNIC();
	} else if (IS_PROTO_MH()) {
		LoginStringMH();
	} else if (IS_PROTO_ODISHA()) {
		LoginStringOD();
	} else if (IS_PROTO_OG()) {
		LoginStringOG();
	}
}
#endif

// void MakeRFIDPacket(uint8_t *data, uint16_t len)
// {
// 	char ss[18];
// 	uint32_t crc;
// 	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
// 	strcpy(dataBuffer,"$RFID,");
// 	Ql_strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
// 	InsertChar(dataBuffer,',');
// 	Ql_strncat(dataBuffer,NetWork.IMEI,15);
// 	InsertChar(dataBuffer,',');
// 	Ql_strcat(dataBuffer,FirmVer);
// 	InsertChar(dataBuffer,',');
// 	Ql_strcat(dataBuffer,sLatitude);
// 	InsertChar(dataBuffer,',');
// 	Ql_strcat(dataBuffer,sLongitude);
// 	InsertChar(dataBuffer,',');
// 	InsertHEXStringToBuffer(dataBuffer,data,len);
// 	InsertChar(dataBuffer,',');
// 	crc = checksum32(dataBuffer,Ql_strlen(dataBuffer));
// 	lastcrc= crc;
// 	Ql_sprintf(ss,"%08X*",crc);
// 	Ql_strcat(dataBuffer,ss);
// }


#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_MAHARASHTRA1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void InitBufferMH(uint8_t alt)
#else
void InitBuffer(uint8_t alt)
#endif
{
	char ss[20];
	uint16_t i;
	uint8_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	Ql_memset(dataBuffer,0,DATA_MAX_BUFF);
	Ql_sprintf(dataBuffer,"$NMP,%s,",VTSData.VendorID);
	Ql_strcat(dataBuffer,FirmVer);


	switch(alt)
	{
		case 1:
			Ql_strcat(dataBuffer,",NR,01,L,");// NORMAL PACKET
			break;
		case 3:
			Ql_strcat(dataBuffer,",BD,03,L,"); // MAIN OFF
			break;
		case 4:
			Ql_strcat(dataBuffer,",BL,04,L,"); // BATTERY LOW
			break;
		case 5:
			Ql_strcat(dataBuffer,",BH,05,L,"); // Battery LOW Restore
			break;
		case 6:
			Ql_strcat(dataBuffer,",BR,06,L,"); // MAIN ON
			break;
		case 7:
			Ql_strcat(dataBuffer,",IN,07,L,"); // IGNITION ON
			break;
		case 8:
			Ql_strcat(dataBuffer,",IF,08,L,"); // IGNITION OFF
			break;
		case 9:
			Ql_strcat(dataBuffer,",DT,09,L,"); // BOX TAMPER
			break;
		case 10:
			Ql_strcat(dataBuffer,",EA,10,L,"); // SOS ON
			break;
		case 11:
			Ql_strcat(dataBuffer,",EO,11,L,"); // SOS OFF
			break;
		case 12:
			Ql_strcat(dataBuffer,",OT,12,L,"); // OTA 
			break;
		case 13:
			Ql_strcat(dataBuffer,",HB,13,L,"); // Harsh Braking
			break;
		case 14:
			Ql_strcat(dataBuffer,",HA,14,L,"); // Harsh Acceleration
			break;
		case 15:
			Ql_strcat(dataBuffer,",RT,15,L,"); // Rash turn
			break;
		case 16:
			Ql_strcat(dataBuffer,",TA,16,L,");  // SOS Tamper
			break;
		case 17:
			Ql_strcat(dataBuffer,",GI,18,L,");  // GeoFence In
			break;
		case 18:
			Ql_strcat(dataBuffer,",GO,19,L,");  // GeoFence Out
			break;
				
		case 23:
			Ql_strcat(dataBuffer,",OS,17,L,"); // Over Speed
			break;
		case 24:
			Ql_strcat(dataBuffer,",TL,24,L,"); // Vehicle Tilt
			break;

		case 25:
			Ql_strcat(dataBuffer,",RF,25,L,"); // RFID Data
		case 30:
			Ql_strcat(dataBuffer,",HP,01,L,"); // HP Data
	}
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,',');
	if(GPS.GPSFix)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	//AppendVariableString(dataBuffer,sSpeed,7,1,"000.0");
	StringAdd(dataBuffer,"%05.1f",GPS.Speed);
	InsertChar(dataBuffer,',');
	//InsertFloatValue(dataBuffer,GPS.Heading,"%06.2f");
	//AppendVariableString(dataBuffer,sHeading,7,1,"000.0");
	StringAdd(dataBuffer,"%06.2f",GPS.Heading);
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GPS.NoOfSatalite,"%02d");
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%03.1f",GPS.Altitude);
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%04.1f",GPS.PDOP);
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%04.1f",GPS.HDOP);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,NetWork.Network);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IsMain + '0');
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.MainsVolt,"%04.1f");
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.BattVolt,"%03.1f");
	InsertChar(dataBuffer,',');
	if(VAlert[SOS_ON_ALERT].Enable || VAlert[SOS_OFF_ALERT].Enable)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');

	if(PeriPheralVal.IsCoverOpen)
		InsertChar(dataBuffer,'O');
	else
		InsertChar(dataBuffer,'C');
	InsertChar(dataBuffer,',');

	


	InsertIntValue(dataBuffer,GSM.SignalStrength,"%2d");
	InsertChar(dataBuffer,',');
	
	InsertIntValue(dataBuffer,GSM.MCC,"%02d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GSM.MNC,"%02d");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.LAC,5,4,"00D6");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.CellID,5,4,"CFBD");
	InsertChar(dataBuffer,',');

	for(i=0;i<4;i++)
	{
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellDB,4,1,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].LAC,6,2,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellID,5,1,"0");
		InsertChar(dataBuffer,',');	
	}
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(INPUT_SOS_VAL)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,FrameNumber,"%06d");
	InsertChar(dataBuffer,',');

	StringAdd(dataBuffer,"%04.1f",PeriPheralVal.AN1);
	InsertChar(dataBuffer,',');

	StringAdd(dataBuffer,"%04.1f",PeriPheralVal.AN2);
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,VTSState.OdoCount/1000,"%01d");
	InsertChar(dataBuffer,',');


	//Ql_strcat(dataBuffer,"(0,0),");
	if(alt == 25) // For RFID Data 
	{
		InsertChar(dataBuffer,'(');
		InsertHEXStringToBuffer(dataBuffer,RFIDData,RFIDDataCount);
		Ql_strcat(dataBuffer,"),");
	}
	else
		Ql_strcat(dataBuffer,"(0,0,0)");
	


	crc = CRC8(dataBuffer,Ql_strlen(dataBuffer));
	//Ql_sprintf(ss,"*%02X",crc);
	lastcrc= crc;
	Ql_sprintf(ss,"*");
	Ql_strcat(dataBuffer,ss);
	FrameNumber++;
	
}

#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_NIC1) 

void PrepareFTKBuffer(FTKConfigtypedef *ftk)
{
	char ss[20];
	uint16_t i;
	uint16_t crc;
	
	char sLat[12];
	char sLng[12];
	char sSPD[8];
	char sAlt[8];
	char sPD[6];
	char sHD[6];
	char sHead[8];
	Ql_memset(sLat, 0, sizeof(sLat));
	Ql_memset(sLng, 0, sizeof(sLng));
	Ql_memset(sSPD, 0, sizeof(sSPD));
	Ql_memset(sAlt, 0, sizeof(sAlt));
	Ql_memset(sPD, 0, sizeof(sPD));
	Ql_memset(sHD, 0, sizeof(sHD));
	Ql_memset(sHead, 0, sizeof(sHead));

	Ql_sprintf(sLat, "%3.6f", ftk->Lat);
	Ql_sprintf(sLng, "%3.6f", ftk->Long);
	Ql_sprintf(sAlt, "%4.2f", ftk->Altitude);
	Ql_sprintf(sSPD, "%3.2f", ftk->Speed);
	Ql_sprintf(sHD, "%3.2f", ftk->HDOP);
	Ql_sprintf(sPD, "%3.2f", ftk->PDOP);
	Ql_sprintf(sHead, "%3.2f", ftk->Heading);

	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	Ql_memset(dataBuffer,0,DATA_MAX_BUFF);
	Ql_sprintf(dataBuffer,"$PVT,%s,",VTSData.VendorID);
	Ql_strcat(dataBuffer,FirmVer);

	Ql_strcat(dataBuffer,",NR,02,L,");
	
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,'1');
	InsertChar(dataBuffer,',');
	InsertSpecificDateTime(dataBuffer,0,&ftk->FTK_LastPacketTime);
	InsertChar(dataBuffer,',');
	InsertSpecificDateTime(dataBuffer,1,&ftk->FTK_LastPacketTime);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLat,10,sLat);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,'N');
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLng,10,sLng);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,'E');
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,sSPD,7,1,"000.0");
	InsertChar(dataBuffer,',');
	//InsertFloatValue(dataBuffer,GPS.Heading,"%06.2f");
	AppendVariableString(dataBuffer,sHead,7,1,"000.0");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,ftk->Noofsats,"%02d");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sAlt,7,1,"000.0");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sPD,5,1,"00.0");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sHD,5,1,"00.0");
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,NetWork.Network);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IsMain + '0');
	InsertChar(dataBuffer,',');
	#ifndef NO_PARAM 
	if(alt==3)
		Ql_strcat(dataBuffer,"00.0");
	else
		InsertFloatValue(dataBuffer,PeriPheralVal.MainsVolt,"%2.1f");

	#else
	InsertFloatValue(dataBuffer,PeriPheralVal.MainsVolt,"%2.1f");
	#endif
	InsertChar(dataBuffer,',');
	#ifndef NO_PARAM
	float tp=0;
	if(alt==4)
	{
		tp = VTSData.BattThrs-0.1;
		InsertFloatValue(dataBuffer,tp,"%1.1f");
	}
	else
	InsertFloatValue(dataBuffer,PeriPheralVal.BattVolt,"%1.1f");
	#else
	InsertFloatValue(dataBuffer,PeriPheralVal.BattVolt,"%1.1f");
	#endif
	
	InsertChar(dataBuffer,',');
	if(VAlert[SOS_ON_ALERT].Enable || VAlert[SOS_OFF_ALERT].Enable)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');

	if(PeriPheralVal.IsCoverOpen)
		InsertChar(dataBuffer,'O');
	else
		InsertChar(dataBuffer,'C');
	InsertChar(dataBuffer,',');

	


	InsertIntValue(dataBuffer,GSM.SignalStrength,"%2d");
	InsertChar(dataBuffer,',');
	
	InsertIntValue(dataBuffer,GSM.MCC,"%02d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GSM.MNC,"%02d");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.LAC,5,4,"00D6");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.CellID,5,4,"CFBD");
	InsertChar(dataBuffer,',');

	for(i=0;i<4;i++)
	{
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellID,5,1,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].LAC,6,2,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellDB,4,1,"0");
		InsertChar(dataBuffer,',');
		
	}
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(INPUT_SOS_VAL)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,41054-ftk->PacketCount,"%06d");
	InsertChar(dataBuffer,',');


	
	#if defined(NIC_UTTRA)
	Ql_strcat(dataBuffer,"00*");
	#else
	crc = chksum((uint8_t*)dataBuffer,Ql_strlen(dataBuffer));
	lastcrc= crc;
	Ql_sprintf(ss,"%04X*",crc);
	Ql_strcat(dataBuffer,ss);
	#endif
}

uint8_t ProcessFTK(FTKConfigtypedef* ftk)
{
    if(ftk->IsEnabled == 0)
    {
        return 0;
    }
    PrepareFTKBuffer(ftk);
	if(TCPSocket_SendString(&ServerSocket[0], dataBuffer)==1)
	{
		ftk->FTK_LastPacketTime = FTK_DeductTime(ftk->FTK_LastPacketTime,ftk->Interval);
		FTK_ApplyVariation(ftk);
		ftk->PacketCount++;
		if(ftk->PacketCount >= 41000)
		{
			ftk->IsEnabled=0;
			LOGData(TAG_SERVER,"*********************************");
			LOGData(TAG_SERVER,"All FTK Packets Sent Successfully");
			LOGData(TAG_SERVER,"*********************************");
		}
	}
	
    return 1;
}

#ifdef ENABLE_UNIFIED_FIRMWARE
void InitBufferNIC(uint8_t alt)
#else
void InitBuffer(uint8_t alt)
#endif
{
	char ss[20];
	uint16_t i;
	uint16_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	Ql_memset(dataBuffer,0,DATA_MAX_BUFF);
	Ql_sprintf(dataBuffer,"$PVT,%s,",VTSData.VendorID);
	Ql_strcat(dataBuffer,FirmVer);


	switch(alt)
	{
		case 1:
			Ql_strcat(dataBuffer,",NR,01,L,");// NORMAL PACKET
			break;
		case 3:
			Ql_strcat(dataBuffer,",BD,03,L,"); // MAIN OFF
			break;
		case 4:
			Ql_strcat(dataBuffer,",BL,04,L,"); // BATTERY LOW
			break;
		case 5:
			Ql_strcat(dataBuffer,",BH,05,L,"); // Battery LOW Restore
			break;
		case 6:
			Ql_strcat(dataBuffer,",BR,06,L,"); // MAIN ON
			break;
		case 7:
			Ql_strcat(dataBuffer,",IN,07,L,"); // IGNITION ON
			break;
		case 8:
			Ql_strcat(dataBuffer,",IF,08,L,"); // IGNITION OFF
			break;
		case 9:
			Ql_strcat(dataBuffer,",TA,09,L,"); // BOX TAMPER
			break;
		case 10:
			Ql_strcat(dataBuffer,",EA,10,L,"); // SOS ON
			break;
		case 11:
			Ql_strcat(dataBuffer,",EO,11,L,"); // SOS OFF
			break;
		case 12:
			Ql_strcat(dataBuffer,",OT,12,L,"); // OTA 
			break;
		case 13:
			Ql_strcat(dataBuffer,",HB,13,L,"); // Harsh Braking
			break;
		case 14:
			Ql_strcat(dataBuffer,",HA,14,L,"); // Harsh Acceleration
			break;
		case 15:
			Ql_strcat(dataBuffer,",RT,15,L,"); // Rash turn
			break;
		case 16:
			Ql_strcat(dataBuffer,",TA,16,L,");  // SOS Tamper
			break;
		case 17:
			Ql_strcat(dataBuffer,",GI,18,L,");  // GeoFence In
			break;
		case 18:
			Ql_strcat(dataBuffer,",GO,19,L,");  // GeoFence Out
			break;
				
		case 23:
			Ql_strcat(dataBuffer,",OS,17,L,"); // Over Speed
			break;
		case 24:
			Ql_strcat(dataBuffer,",TL,22,L,"); // Vehicle Tilt
			break;

		case 25:
			Ql_strcat(dataBuffer,",RF,25,L"); // RFID Data
	}
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	if(GPS.GPSFix)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,sSpeed,7,1,"000.0");
	InsertChar(dataBuffer,',');
	//InsertFloatValue(dataBuffer,GPS.Heading,"%06.2f");
	AppendVariableString(dataBuffer,sHeading,7,1,"000.0");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GPS.NoOfSatalite,"%02d");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sAltitude,7,1,"000.0");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sPDOP,5,1,"00.0");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sHDOP,5,1,"00.0");
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,NetWork.Network);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IsMain + '0');
	InsertChar(dataBuffer,',');
	#ifndef NO_PARAM 
	if(alt==3)
		Ql_strcat(dataBuffer,"00.0");
	else
		InsertFloatValue(dataBuffer,PeriPheralVal.MainsVolt,"%2.1f");

	#else
	InsertFloatValue(dataBuffer,PeriPheralVal.MainsVolt,"%2.1f");
	#endif
	InsertChar(dataBuffer,',');
	#ifndef NO_PARAM
	float tp=0;
	if(alt==4)
	{
		tp = VTSData.BattThrs-0.1;
		InsertFloatValue(dataBuffer,tp,"%1.1f");
	}
	else
	InsertFloatValue(dataBuffer,PeriPheralVal.BattVolt,"%1.1f");
	#else
	InsertFloatValue(dataBuffer,PeriPheralVal.BattVolt,"%1.1f");
	#endif
	
	InsertChar(dataBuffer,',');
	if(VAlert[SOS_ON_ALERT].Enable || VAlert[SOS_OFF_ALERT].Enable)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');

	if(PeriPheralVal.IsCoverOpen)
		InsertChar(dataBuffer,'O');
	else
		InsertChar(dataBuffer,'C');
	InsertChar(dataBuffer,',');

	


	InsertIntValue(dataBuffer,GSM.SignalStrength,"%2d");
	InsertChar(dataBuffer,',');
	
	InsertIntValue(dataBuffer,GSM.MCC,"%02d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GSM.MNC,"%02d");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.LAC,5,4,"00D6");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.CellID,5,4,"CFBD");
	InsertChar(dataBuffer,',');

	for(i=0;i<4;i++)
	{
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellID,5,1,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].LAC,6,2,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellDB,4,1,"0");
		InsertChar(dataBuffer,',');
		
	}
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	if(alt == 8)
		InsertChar(dataBuffer,'0');
	else
		InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(INPUT_SOS_VAL)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,FrameNumber,"%06d");
	InsertChar(dataBuffer,',');

	if(alt == 25) // For RFID Data 
	{
		InsertChar(dataBuffer,'(');
		InsertHEXStringToBuffer(dataBuffer,RFIDData,RFIDDataCount);
		Ql_strcat(dataBuffer,"),");
	}

	// InsertFloatValue(dataBuffer,PeriPheralVal.AN1,"%2.1f");
	// InsertChar(dataBuffer,',');
	// InsertFloatValue(dataBuffer,PeriPheralVal.AN2,"%2.1f");
	// InsertChar(dataBuffer,',');

	// InsertIntValue(dataBuffer,DeltaDis,"%02d");
	// InsertChar(dataBuffer,',');


	////AppendVariableString(dataBuffer,"(0,0,0)",20,5,"(0,0,0)");

	//Ql_strcat(dataBuffer,"(0,0),");
	#if defined(NIC_UTTRA)
	Ql_strcat(dataBuffer,"00*");
	#else
	crc = chksum((uint8_t*)dataBuffer,Ql_strlen(dataBuffer));
	Ql_sprintf(ss,"%04X*",crc);
	lastcrc= crc;
	Ql_strcat(dataBuffer,ss);
	#endif
	FrameNumber++;



	
	
}

#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_OG)


#ifdef ENABLE_UNIFIED_FIRMWARE
void InitBufferOG(uint8_t alt)
#else
void InitBuffer(uint8_t alt)
#endif
{
	char ss[20];
	uint16_t i;
	uint8_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	Ql_memset(dataBuffer,0,DATA_MAX_BUFF);
	Ql_sprintf(dataBuffer,"$,NMP,%s,",VTSData.VendorID);
	Ql_strcat(dataBuffer,FirmVer);


	switch(alt)
	{
		case 1:
			Ql_strcat(dataBuffer,",NR,1,L,");// NORMAL PACKET
			break;
		case 3:
			Ql_strcat(dataBuffer,",BD,3,L,"); // MAIN OFF
			break;
		case 4:
			Ql_strcat(dataBuffer,",BL,4,L,"); // BATTERY LOW
			break;
		case 5:
			Ql_strcat(dataBuffer,",BH,5,L,"); // Battery LOW Restore
			break;
		case 6:
			Ql_strcat(dataBuffer,",BR,6,L,"); // MAIN ON
			break;
		case 7:
			Ql_strcat(dataBuffer,",IN,7,L,"); // IGNITION ON
			break;
		case 8:
			Ql_strcat(dataBuffer,",IF,8,L,"); // IGNITION OFF
			break;
		case 9:
			Ql_strcat(dataBuffer,",TA,9,L,"); // BOX TAMPER
			break;
		case 10:
			Ql_strcat(dataBuffer,",EA,10,L,"); // SOS ON
			break;
		case 11:
			Ql_strcat(dataBuffer,",EA,11,L,"); // SOS OFF
			break;
		case 12:
			Ql_strcat(dataBuffer,",OT,12,L,"); // OTA 
			break;
		case 13:
			Ql_strcat(dataBuffer,",HB,13,L,"); // Harsh Braking
			break;
		case 14:
			Ql_strcat(dataBuffer,",HA,14,L,"); // Harsh Acceleration
			break;
		case 15:
			Ql_strcat(dataBuffer,",RT,15,L,"); // Rash turn
			break;
		case 16:
			Ql_strcat(dataBuffer,",DT,16,L,");  // SOS Tamper
			break;
		case 17:
			Ql_strcat(dataBuffer,",GI,18,L,");  // GeoFence In
			break;
		case 18:
			Ql_strcat(dataBuffer,",GO,19,L,");  // GeoFence Out
			break;
				
		case 23:
			Ql_strcat(dataBuffer,",OS,17,L,"); // Over Speed
			break;
		case 24:
			Ql_strcat(dataBuffer,",TL,24,L,"); // Vehicle Tilt
			break;

		case 25:
			Ql_strcat(dataBuffer,",RF,25,L,"); // RFID Data
		case 30:
			Ql_strcat(dataBuffer,",HP,1,L,"); // HP Data
	}
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	if(GPS.GPSFix)
		InsertChar(dataBuffer,'1');
	else	
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	//AppendVariableString(dataBuffer,sSpeed,7,1,"000.0");
	StringAdd(dataBuffer,"%05.1f",GPS.Speed);
	InsertChar(dataBuffer,',');
	//InsertFloatValue(dataBuffer,GPS.Heading,"%06.2f");
	//AppendVariableString(dataBuffer,sHeading,7,1,"000.0");
	StringAdd(dataBuffer,"%06.2f",GPS.Heading);
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GPS.NoOfSatalite,"%02d");
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%03.1f",GPS.Altitude);
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%04.1f",GPS.PDOP);
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%04.1f",GPS.HDOP);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,NetWork.Network);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IsMain + '0');
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.MainsVolt,"%04.1f");
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.BattVolt,"%03.1f");
	InsertChar(dataBuffer,',');
	if(VAlert[SOS_ON_ALERT].Enable || VAlert[SOS_OFF_ALERT].Enable)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');

	if(PeriPheralVal.IsCoverOpen)
		InsertChar(dataBuffer,'O');
	else
		InsertChar(dataBuffer,'C');
	InsertChar(dataBuffer,',');

	


	InsertIntValue(dataBuffer,GSM.SignalStrength,"%2d");
	InsertChar(dataBuffer,',');
	
	InsertIntValue(dataBuffer,GSM.MCC,"%02d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GSM.MNC,"%02d");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.LAC,5,4,"00D6");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.CellID,5,4,"CFBD");
	InsertChar(dataBuffer,',');

	for(i=0;i<4;i++)
	{
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellID,5,1,"0");
		InsertChar(dataBuffer,',');	
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].LAC,6,2,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellDB,4,1,"0");
		InsertChar(dataBuffer,',');
		
	}
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(INPUT_SOS_VAL)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,FrameNumber,"%06d");
	InsertChar(dataBuffer,',');
	

	crc = CRC8(dataBuffer,Ql_strlen(dataBuffer));
	Ql_sprintf(ss,"%02X",crc);
	Ql_strcat(dataBuffer,ss);
	lastcrc= crc;
	Ql_sprintf(ss,",*");
	Ql_strcat(dataBuffer,ss);
	FrameNumber++;
	
}


#endif

#if defined(PROTO_CDAC)



// uint8_t GetAlertsHeader(void)
// {
// 	uint8_t rt;
// 	uint16_t n=0;
// 	uint16_t dLen=8;
// 	uint8_t j=StoredAlert.AlertPosition;
// 	Ql_memset(SendString,0,DATA_MAX_BUFF);	
// 	Ql_memset(SendString,'0',118);
// 	InsertStringValue("vltdata=",0,8,0);
// 	if(!StoredAlert.TotalAlert)
// 	{
// 		InsertStringValue("NRM",dLen + 0,3,0);
// 		InsertStringValue("01L",dLen + 18,3,0);
// 		SendString[dLen + _REGULAR_SIZE]=0;
// 		return 0xFF;
// 	}
// 	LOGData(TAG_SERVER,"Stored total alt %d > 0,  altsval : %d, pos: %d",StoredAlert.TotalAlert,StoredAlert.Alerts[j],j);
// 	if(StoredAlert.Alerts[j] != 0xFF)
// 	{
// 		LOGData(TAG_SERVER,"found stored alert %d",StoredAlert.Alerts[j]);
// 		InsertStringValue(VAlert[StoredAlert.Alerts[j]].Header,dLen + 0,3,1);
// 		InsertStringValue(VAlert[StoredAlert.Alerts[j]].ID,dLen + 18,2,1);
// 		SendString[dLen + 20]='L';
// 		if(VAlert[StoredAlert.Alerts[j]].WithACK)
// 		{
// 			LOGData(TAG_SERVER,"alert ack size %d",Ql_strlen(VAlert[StoredAlert.Alerts[j]].ACK));
// 			InsertStringValue(VAlert[StoredAlert.Alerts[j]].ACK,dLen + _REGULAR_SIZE,Ql_strlen(VAlert[StoredAlert.Alerts[j]].ACK),1);
// 			n=Ql_strlen(VAlert[StoredAlert.Alerts[j]].ACK);
// 			SendString[dLen+_REGULAR_SIZE+n]=0;
// 		}
// 		else
// 			SendString[dLen+_REGULAR_SIZE]=0;
// 		RemoveNonRepeatAlert(j);
// 		StoredAlert.AlertPosition++;
// 		if(StoredAlert.AlertPosition >= StoredAlert.TotalAlert)
// 			StoredAlert.AlertPosition=0;
// 		rt=StoredAlert.Alerts[j];
// 		return rt;
// 	}
// 	return 0xFF;
// }

void MakeNormalPacket(void)
{
	uint16_t dLen=8;
	Ql_memset(SendString,0,DATA_MAX_BUFF);	
	Ql_memset(SendString,'0',118);
	InsertStringValue("vltdata=",0,8,0);
	InsertStringValue("NRM",dLen + 0,3,0);
	InsertStringValue("01L",dLen + 18,3,0);
	SendString[dLen + _REGULAR_SIZE]=0;
	DataPacket();
	return;
}

#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_ODISA1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void InitBufferOD(uint8_t alt)
#else
#if !defined(PROTO_MAHARASHTRA1) && !defined(PROTO_NIC1) && !defined(PROTO_CDAC) && !defined(PROTO_OG)
void InitBuffer(uint8_t alt)
#endif
#endif
#if defined(ENABLE_UNIFIED_FIRMWARE) || (!defined(PROTO_MAHARASHTRA1) && !defined(PROTO_NIC1) && !defined(PROTO_CDAC) && !defined(PROTO_OG))
{
	char ss[20];
	uint16_t i;
	uint32_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	Ql_memset(dataBuffer,0,DATA_MAX_BUFF);
	Ql_sprintf(dataBuffer,"$PVT,%s,",VTSData.VendorID);
	Ql_strcat(dataBuffer,FirmVer);


	switch(alt)
	{
		case 1:
			Ql_strcat(dataBuffer,",NR,1,L,");// NORMAL PACKET
			break;
		case 3:
			Ql_strcat(dataBuffer,",BD,3,L,"); // MAIN OFF
			break;
		case 4:
			Ql_strcat(dataBuffer,",BL,4,L,"); // BATTERY LOW	
			break;
		case 5:
			Ql_strcat(dataBuffer,",BC,5,L,"); // Battery LOW Restore
			break;
		case 6:
			Ql_strcat(dataBuffer,",BR,6,L,"); // MAIN ON
			break;
		case 7:
			Ql_strcat(dataBuffer,",IN,7,L,"); // IGNITION ON
			break;
		case 8:
			Ql_strcat(dataBuffer,",IF,8,L,"); // IGNITION OFF
			break;
		case 9:
			Ql_strcat(dataBuffer,",TA,9,L,"); // BOX TAMPER
			break;
		case 10:
			Ql_strcat(dataBuffer,",EA,10,L,"); // SOS ON
			break;
		case 11:
			Ql_strcat(dataBuffer,",EO,11,L,"); // SOS OFF
			break;
		case 12:
			Ql_strcat(dataBuffer,",CFG,12,L,"); // OTA 
			break;
		case 13:
			Ql_strcat(dataBuffer,",HB,13,L,"); // Harsh Braking
			break;
		case 14:
			Ql_strcat(dataBuffer,",HA,14,L,"); // Harsh Acceleration
			break;
		case 15:
			Ql_strcat(dataBuffer,",RT,15,L,"); // Rash turn
			break;
		case 16:
			Ql_strcat(dataBuffer,",TA,16,L,");  // SOS Tamper
			break;
		case 17:
			Ql_strcat(dataBuffer,",GI,17,L,");  // GeoFence In
			break;
		case 18:
			Ql_strcat(dataBuffer,",GO,18,L,");  // GeoFence Out
			break;
				
		case 23:
			Ql_strcat(dataBuffer,",OS,20,L,"); // Over Speed
			break;
		case 24:
			Ql_strcat(dataBuffer,",TL,24,L,"); // Vehicle Tilt
			break;

		case 25:
			Ql_strcat(dataBuffer,",RF,25,L,"); // RFID Data
		case 30:
			Ql_strcat(dataBuffer,",HP,1,L,"); // HP Data
	}
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	if(GPS.GPSFix)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,sSpeed,7,1,"000.0");
	InsertChar(dataBuffer,',');
	//InsertFloatValue(dataBuffer,GPS.Heading,"%06.2f");
	AppendVariableString(dataBuffer,sHeading,7,1,"000.0");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GPS.NoOfSatalite,"%02d");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sAltitude,7,1,"000.0");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sPDOP,5,1,"00.0");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sHDOP,5,1,"00.0");
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,NetWork.Network);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IsMain + '0');
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.MainsVolt,"%2.1f");
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.BattVolt,"%1.1f");
	InsertChar(dataBuffer,',');
	if(VAlert[SOS_ON_ALERT].Enable || VAlert[SOS_OFF_ALERT].Enable)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');

	if(PeriPheralVal.IsCoverOpen)
		InsertChar(dataBuffer,'O');
	else
		InsertChar(dataBuffer,'C');
	InsertChar(dataBuffer,',');

	


	InsertIntValue(dataBuffer,GSM.SignalStrength,"%2d");
	InsertChar(dataBuffer,',');
	
	InsertIntValue(dataBuffer,GSM.MCC,"%02d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GSM.MNC,"%02d");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.LAC,5,4,"00D6");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.CellID,5,4,"CFBD");
	InsertChar(dataBuffer,',');

	for(i=0;i<4;i++)
	{
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellID,5,1,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].LAC,6,2,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellDB,4,1,"0");
		InsertChar(dataBuffer,',');
		
	}
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(INPUT_SOS_VAL)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	Ql_strcat(dataBuffer,"00,");


	InsertIntValue(dataBuffer,FrameNumber,"%06d");
	InsertChar(dataBuffer,',');

	// InsertFloatValue(dataBuffer,PeriPheralVal.AN1,"%2.1f");
	// InsertChar(dataBuffer,',');
	// InsertFloatValue(dataBuffer,PeriPheralVal.AN2,"%2.1f");
	// InsertChar(dataBuffer,',');

	// InsertIntValue(dataBuffer,DeltaDis,"%02d");
	// InsertChar(dataBuffer,',');


	////AppendVariableString(dataBuffer,"(0,0,0)",20,5,"(0,0,0)");

	//Ql_strcat(dataBuffer,"(0,0),");
	if(alt == 25) // For RFID Data 
	{
		InsertChar(dataBuffer,'{');
		InsertHEXStringToBuffer(dataBuffer,RFIDData,RFIDDataCount);
		Ql_strcat(dataBuffer,"},");
	}
	

	//#ifdef ODISA_LD
	crc = CRC16(dataBuffer,Ql_strlen(dataBuffer));
	Ql_sprintf(ss,"%04X*",crc);
	lastcrc= crc;
	Ql_strcat(dataBuffer,ss);
	// #else
	// crc = checksum32(dataBuffer,Ql_strlen(dataBuffer));
	// Ql_sprintf(ss,"%08X*",crc);
	// lastcrc= crc;
	// Ql_strcat(dataBuffer,ss);
	//#endif

	#ifndef ODISA_LD
    InsertChar(dataBuffer,'\n');
    #endif


	FrameNumber++;
	
}
#endif
#endif

#ifdef ENABLE_UNIFIED_FIRMWARE
void InitBuffer(uint8_t alt)
{
	if (IS_PROTO_NIC()) {
		InitBufferNIC(alt);
	} else if (IS_PROTO_MH()) {
		InitBufferMH(alt);
	} else if (IS_PROTO_ODISHA()) {
		InitBufferOD(alt);
	} else if (IS_PROTO_OG()) {
		InitBufferOG(alt);
	}
}
#endif


#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_MAHARASHTRA1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void EmergencyPacketMH(uint8_t IsOff)
#else
void EmergencyPacket(uint8_t IsOff)
#endif
{
	char ss[20];
	uint8_t crc;

	if(prevLat==0)
		DeltaDis=0;
	else
		DeltaDis = calculateDistance(prevLat,prevLong,GPS.Latitude,GPS.Longitude);

	prevLat = GPS.Latitude;
	prevLong = GPS.Longitude;

	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	if(IsOff)
		Ql_strcat(dataBuffer,"$EPB,EMR,");
	else
		Ql_strcat(dataBuffer,"$EPB,SEM,");

	Ql_strncat(dataBuffer,NetWork.IMEI,15);

	if(IsStored)
		AppendFixString(dataBuffer,",SP,",4,",SP,");
	else
		AppendFixString(dataBuffer,",NM,",4,",NM,");

	InsertCurrentDateTime(dataBuffer,0);
	InsertCurrentDateTime(dataBuffer,1);

	if(GPS.GPSFix)
		Ql_strcat(dataBuffer,",A,");
	else
		Ql_strcat(dataBuffer,",V,");

	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	StringAdd(dataBuffer,"%03.1f",GPS.Altitude);
	InsertChar(dataBuffer,',');
	StringAdd(dataBuffer,"%05.1f",GPS.Speed);
	InsertChar(dataBuffer,',');
	
	Ql_strcat(dataBuffer,"G,");
	InsertIntValue(dataBuffer,DeltaDis,"%02d");
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.PhoneNumber.Mob1,14,9,"NO_NUMBER");
	crc = CRC8(dataBuffer,Ql_strlen(dataBuffer));
	lastcrc= crc;
	//Ql_sprintf(ss,"*%02X",crc);
	Ql_sprintf(ss,"*");
	Ql_strcat(dataBuffer,ss);
	
}

#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_NIC1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void EmergencyPacketNIC(uint8_t IsOff)
#else
void EmergencyPacket(uint8_t IsOff)
#endif
{
	char ss[20];
	uint16_t crc;
	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	if(IsOff)
		Ql_strcat(dataBuffer,"$EPB,EMR,");
	else
		Ql_strcat(dataBuffer,"$EPB,SEM,");

	Ql_strncat(dataBuffer,NetWork.IMEI,15);

	if(IsStored)
		AppendFixString(dataBuffer,",SP,",4,",SP,");
	else
		AppendFixString(dataBuffer,",NM,",4,",NM,");

	InsertCurrentDateTime(dataBuffer,0);
	InsertCurrentDateTime(dataBuffer,1);

	if(GPS.GPSFix)
		Ql_strcat(dataBuffer,",A,");
	else
		Ql_strcat(dataBuffer,",V,");

	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,sAltitude,7,1,"000.0");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,sSpeed,7,1,"000.0");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,DeltaDis,"%02d");
	Ql_strcat(dataBuffer,",G,");
	Ql_strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.PhoneNumber.Mob1,15,9,"NO_NUMBER");
	InsertChar(dataBuffer,'*');
	crc = chksum((uint8_t*)dataBuffer,Ql_strlen(dataBuffer));
	Ql_sprintf(ss,"%04X",crc);
	lastcrc= crc;
	Ql_strcat(dataBuffer,ss);
	
}

#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_OG)
#ifdef ENABLE_UNIFIED_FIRMWARE
void EmergencyPacketOG(uint8_t IsOff)
#else
void EmergencyPacket(uint8_t IsOff)
#endif
{
	char ss[20];
	uint8_t crc;

	// if(prevLat==0)
	// 	DeltaDis=0;
	// else
	// 	DeltaDis = calculateDistance(prevLat,prevLong,GPS.Latitude,GPS.Longitude);

	prevLat = GPS.Latitude;
	prevLong = GPS.Longitude;

	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	if(IsOff)
		Ql_strcat(dataBuffer,"$,EPB,EMR,");
	else
		Ql_strcat(dataBuffer,"$,EPB,SEM,");

	Ql_strncat(dataBuffer,NetWork.IMEI,15);

	if(IsStored)
		AppendFixString(dataBuffer,",SP,",4,",SP,");
	else
		AppendFixString(dataBuffer,",NM,",4,",NM,");

	InsertCurrentDateTime(dataBuffer,0);
	InsertCurrentDateTime(dataBuffer,1);

	if(GPS.GPSFix)
		Ql_strcat(dataBuffer,",A,");
	else
		Ql_strcat(dataBuffer,",V,");

	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	StringAdd(dataBuffer,"%05.1f",GPS.Altitude);
	InsertChar(dataBuffer,',');
	StringAdd(dataBuffer,"%05.1f",GPS.Speed);
	InsertChar(dataBuffer,',');
	float dd = (float)DeltaDis;
	InsertFloatValue(dataBuffer,dd,"%05.1f");
	//InsertIntValue(dataBuffer,DeltaDis,"%02d");
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,"G,");
	Ql_strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.PhoneNumber.Mob1,14,9,"NO_NUMBER");
	crc = CRC8(dataBuffer,Ql_strlen(dataBuffer));
	lastcrc= crc;
	Ql_sprintf(ss,",*,%02X",crc);
	//Ql_sprintf(ss,"*");
	Ql_strcat(dataBuffer,ss);
	
}


#endif

#if defined(PROTO_CDAC)

uint16_t MakeCriticalString(uint8_t IsURE)
{
	LOGData(TAG_SERVER,"crir chk");
	uint16_t n, cn=0;
	uint16_t dLen=8;
	IsCritical=0;
	Ql_memset(SendString,0,DATA_MAX_BUFF);	
	Ql_memset(SendString,'0',120);
	Ql_memset(&CriticalString[0][0],0x00,CRITICAL_MAX_BUFF);
	Ql_memset(&CriticalString[1][0],0x00,CRITICAL_MAX_BUFF);
	Ql_memset(&CriticalString[2][0],0x00,CRITICAL_MAX_BUFF);
	Ql_memset(&CriticalString[3][0],0x00,CRITICAL_MAX_BUFF);
	Ql_memset(&CriticalString[4][0],0x00,CRITICAL_MAX_BUFF);
	InsertStringValue("vltdata=",0,8,0);
	for(int i=0;i<ALERT_COUNT;i++)
	{
		if(VTAlertHeaderType[i] > 1)
			continue;
		if(VAlert[i].Enable)
		{
			if(!IsURE)
			{
				if((VAlert[i].ContMode != ALT_CONT_NORMAL) || SOS.IsSOS || VAlert[i].AlertSent==0)
					continue;
			}
			else
			{
				if(VAlert[i].AlertSent)	
				{
					if(VAlert[i].ContMode == ALT_CONT_NONE)
						continue;

					if(VAlert[i].ContMode == ALT_CONT_NORMAL)
					{
						uint8_t continueCondition = 0;
						if(i == TILT_ALERT && PeriPheralVal.IsTilt)
							continueCondition = 1;
						else if(i == OVER_SPEED_ALERT && IsOverSpeed)
							continueCondition = 1;
						else if(SOS.IsSOS || SOS.IsSOSTamper)
							continueCondition = 1;

						if(!continueCondition)
							continue;
					}
				}
			}

			InsertStringValue(VAlert[i].Header,dLen + 0,3,1);
			InsertStringValue(VAlert[i].ID,dLen + 18,2,1);
			SendString[dLen + 20]='L';
			if(VAlert[i].WithACK)
			{
				InsertStringValue(VAlert[i].ACK,dLen + _REGULAR_SIZE,Ql_strlen(VAlert[i].ACK),1);
				n=Ql_strlen(VAlert[i].ACK);
				SendString[dLen+_REGULAR_SIZE+n]=0;
			}
			else
				SendString[dLen+_REGULAR_SIZE]=0;
			DataPacket();
			Ql_strncpy(&CriticalString[cn][0], SendString, CRITICAL_MAX_BUFF - 1);
			CriticalString[cn][CRITICAL_MAX_BUFF - 1] = '\0';
			CriticalAlertIdx[cn] = (uint8_t)i;
			cn++;
			VAlert[i].AlertSent=1;
			if(cn >= 5)
				break;
		}
	}
	return cn;
}


uint16_t MakeAlertString(void)
{
	LOGData(TAG_SERVER,"alt chk");
	uint16_t n, cn=0;
	uint16_t dLen=8;
	IsCritical=0;
	Ql_memset(SendString,0,DATA_MAX_BUFF);	
	Ql_memset(SendString,'0',120);
	Ql_memset(&CriticalString[0][0],0x00,CRITICAL_MAX_BUFF);
	Ql_memset(&CriticalString[1][0],0x00,CRITICAL_MAX_BUFF);
	Ql_memset(&CriticalString[2][0],0x00,CRITICAL_MAX_BUFF);
	Ql_memset(&CriticalString[3][0],0x00,CRITICAL_MAX_BUFF);
	Ql_memset(&CriticalString[4][0],0x00,CRITICAL_MAX_BUFF);
	InsertStringValue("vltdata=",0,8,0);
	for(int i=0;i<ALERT_COUNT;i++)
	{
		if(VAlert[i].Enable)
		{
			if(VTAlertHeaderType[i] < 2)
				continue;

			if(VAlert[i].AlertSent)
				continue;

			InsertStringValue(VAlert[i].Header,dLen + 0,3,1);
			InsertStringValue(VAlert[i].ID,dLen + 18,2,1);
			SendString[dLen + 20]='L';
			if(VAlert[i].WithACK)
			{
				InsertStringValue(VAlert[i].ACK,dLen + _REGULAR_SIZE,Ql_strlen(VAlert[i].ACK),1);
				n=Ql_strlen(VAlert[i].ACK);
				SendString[dLen+_REGULAR_SIZE+n]=0;
			}
			else
				SendString[dLen+_REGULAR_SIZE]=0;
			DataPacket();
			Ql_strncpy(&CriticalString[cn][0], SendString, CRITICAL_MAX_BUFF - 1);
			CriticalString[cn][CRITICAL_MAX_BUFF - 1] = '\0';
			CriticalAlertIdx[cn] = (uint8_t)i;
			cn++;
			VAlert[i].AlertSent = 1;
			if(cn >= 5)  // Max 5 critical strings (array size check)
				break;
		}
	}
	
	return cn;
}


#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_ODISA1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void EmergencyPacketOD(uint8_t IsOff)
#else
#if !defined(PROTO_MAHARASHTRA1) && !defined(PROTO_NIC1) && !defined(PROTO_CDAC) && !defined(PROTO_OG)
void EmergencyPacket(uint8_t IsOff)
#endif
#endif
#if defined(ENABLE_UNIFIED_FIRMWARE) || (!defined(PROTO_MAHARASHTRA1) && !defined(PROTO_NIC1) && !defined(PROTO_CDAC) && !defined(PROTO_OG))
{
	char ss[20];
	uint32_t crc;
	float dis;
	uint16_t dd;
	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	if(IsOff)
		Ql_strcat(dataBuffer,"$EPB,EMR,");
	else
		Ql_strcat(dataBuffer,"$EPB,SEM,");

	Ql_strncat(dataBuffer,NetWork.IMEI,15);

	if(IsStored)
		AppendFixString(dataBuffer,",SP,",4,",SP,");
	else
		AppendFixString(dataBuffer,",NM,",4,",NM,");

	InsertCurrentDateTime(dataBuffer,0);
	InsertCurrentDateTime(dataBuffer,1);

	if(GPS.GPSFix)
		Ql_strcat(dataBuffer,",A,");
	else
		Ql_strcat(dataBuffer,",V,");

	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,sAltitude,7,1,"000.0");
	InsertChar(dataBuffer,',');
	dd = GPS.Speed;
	InsertIntValue(dataBuffer,dd,"%02d");
	InsertChar(dataBuffer,',');
	dis = DeltaDis;
	StringAdd(dataBuffer,"%05.1f",dis);
	Ql_strcat(dataBuffer,",G,");
	Ql_strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.PhoneNumber.Mob1,12,9,"NO_NUMBER");
	InsertChar(dataBuffer,',');
	#ifdef ODISA_LD
	crc = CRC16(dataBuffer,Ql_strlen(dataBuffer));
	Ql_sprintf(ss,"%04X*",crc);
	lastcrc= crc;
	Ql_strcat(dataBuffer,ss);
	#else
	crc = CRC16(dataBuffer,Ql_strlen(dataBuffer));
	Ql_sprintf(ss,"%04X*",crc);
	lastcrc= crc;
	Ql_strcat(dataBuffer,ss);
	#endif

	#ifndef ODISA_LD
    InsertChar(dataBuffer,'\n');
    #endif
}
#endif
#endif

#ifdef ENABLE_UNIFIED_FIRMWARE
void EmergencyPacket(uint8_t IsOff)
{
	if (IS_PROTO_NIC()) {
		EmergencyPacketNIC(IsOff);
	} else if (IS_PROTO_MH()) {
		EmergencyPacketMH(IsOff);
	} else if (IS_PROTO_ODISHA()) {
		EmergencyPacketOD(IsOff);
	} else if (IS_PROTO_OG()) {
		EmergencyPacketOG(IsOff);
	}
}
#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_MAHARASHTRA1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void HealthPacketMH(void)
#else
void HealthPacket(void)
#endif
{
	GetMemeryPercentage();
	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	Ql_strcat(dataBuffer,"$HLP,");
	Ql_strcat(dataBuffer,VTSData.VendorID);
	InsertChar(dataBuffer,',');
	//INV,");
	Ql_strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,PeriPheralVal.BattPerc,"%03d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,LOW_BAT_THRS_PER,"%02d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,MemoryPercent,"%03d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,VTSData.IntervalData.IgnitionInterval,"%d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,VTSData.IntervalData.DataInterval,"%d");
	//AppendFixString(dataBuffer,",010000,00*",11,",010000,00*");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(PrevTamp)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');

	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');

	InsertChar(dataBuffer,',');

	if(PeriPheralVal.AN1 > 3.0f)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');

	if(PeriPheralVal.AN2 > 3.0f)
		Ql_strcat(dataBuffer,"1*");
	else
		Ql_strcat(dataBuffer,"0*");
	
}
#endif

#if defined(PROTO_CDAC)
#ifdef ENABLE_UNIFIED_FIRMWARE
void HealthPacketCDAC(void)
#else
void HealthPacket(void)
#endif
{
	uint16_t dLen=8;
	Ql_memset(SendString,0,DATA_MAX_BUFF);	
	Ql_memset(SendString,'0',150);
	InsertStringValue("vltdata=",0,8,0);
	InsertStringValue("HLM",dLen + 0,3,0);
	InsertStringValue(VTSData.VendorID,dLen + 3,6,1);
	InsertStringValue(FirmVer,dLen + 9,6,1);
	InsertStringValue(NetWork.IMEI,dLen + 15,15,1);
	InsertIntValueCDAC(VTSData.IntervalData.MotionInterval,dLen + 30,3);
	InsertIntValueCDAC(VTSData.IntervalData.HaltInterval,dLen + 33,3);
	InsertIntValueCDAC((uint16_t)PeriPheralVal.BattPerc,dLen + 36,3);
	InsertIntValueCDAC(batteryVoltageToPercentage(VTSData.BattThrs),dLen + 39,2);
	InsertStringValue("060",dLen + 41,3,0);
	SendString[dLen + 44]=PeriPheralVal.IP1 + '0';
	SendString[dLen + 45]=PeriPheralVal.IP2 + '0';
	SendString[dLen + 46]=PeriPheralVal.OP1 + '0';
	SendString[dLen + 47]=PeriPheralVal.OP2 + '0';
	InsertStringValue("01",dLen + 48,2,0);
	InsertCurrentDateTimeAt(dLen + 50);
	SendString[dLen + 62]='\0';
	
}

#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_OG)
#ifdef ENABLE_UNIFIED_FIRMWARE
void HealthPacketOG(void)
#else
void HealthPacket(void)
#endif
{
	GetMemeryPercentage();
	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	Ql_strcat(dataBuffer,"$,HLM,");
	Ql_strcat(dataBuffer,VTSData.VendorID);
	InsertChar(dataBuffer,',');
	//INV,");
	Ql_strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,PeriPheralVal.BattPerc,"%03d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,LOW_BAT_THRS_PER,"%03d");
	InsertChar(dataBuffer,',');
	float memPer = (float)MemoryPercent;
	InsertFloatValue(dataBuffer,memPer,"%04.1f");
	//InsertIntValue(dataBuffer,MemoryPercent,"%03d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,VTSData.IntervalData.IgnitionInterval,"%d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,VTSData.IntervalData.DataInterval,"%d");
	//AppendFixString(dataBuffer,",010000,00*",11,",010000,00*");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(PrevTamp)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');

	//InsertFloatValue(dataBuffer,PeriPheralVal.AN1,"%04.1f,");
	//InsertFloatValue(dataBuffer,PeriPheralVal.AN2,"%04.1f,*");
	strcat(dataBuffer,"0.00 0.00,*");
	// crc = checksum32(dataBuffer,Ql_strlen(dataBuffer));
	// Ql_sprintf(ss,"%08X*",crc);
	// lastcrc= crc;
	// Ql_strcat(dataBuffer,ss);
	
}
#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_ODISA1) || (!defined(PROTO_MAHARASHTRA1) && !defined(PROTO_CDAC) && !defined(PROTO_OG))
#ifdef ENABLE_UNIFIED_FIRMWARE
void HealthPacketOD(void)
#else
#if !defined(PROTO_MAHARASHTRA1) && !defined(PROTO_CDAC) && !defined(PROTO_OG)
void HealthPacket(void)
#endif
#endif
#if defined(ENABLE_UNIFIED_FIRMWARE) || (!defined(PROTO_MAHARASHTRA1) && !defined(PROTO_CDAC) && !defined(PROTO_OG))
{
	GetMemeryPercentage();
	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	Ql_strcat(dataBuffer,"$HEL,");
	Ql_strcat(dataBuffer,VTSData.VendorID);
	InsertChar(dataBuffer,',');
	//INV,");
	Ql_strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,PeriPheralVal.BattPerc,"%03d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,LOW_BAT_THRS_PER,"%03d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,MemoryPercent,"%03d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,VTSData.IntervalData.IgnitionInterval,"%d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,VTSData.IntervalData.DataInterval,"%d");
	//AppendFixString(dataBuffer,",010000,00*",11,",010000,00*");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(PrevTamp)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');

	if(PeriPheralVal.AN1 > 3.0f)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');

	if(PeriPheralVal.AN2 > 3.0f)
		Ql_strcat(dataBuffer,"1*");
	else
		Ql_strcat(dataBuffer,"0*");
	
	// crc = checksum32(dataBuffer,Ql_strlen(dataBuffer));
	// Ql_sprintf(ss,"%08X*",crc);
	// lastcrc= crc;
	// Ql_strcat(dataBuffer,ss);

	#ifndef ODISA_LD
    InsertChar(dataBuffer,'\n');
    #endif
	
}
#endif
#endif

#ifdef ENABLE_UNIFIED_FIRMWARE
void HealthPacket(void)
{
	if (IS_PROTO_MH()) {
		HealthPacketMH();
	} else if (IS_PROTO_OG()) {
		HealthPacketOG();
	} else {
		HealthPacketOD();
	}
}
#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_MAHARASHTRA1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void MakeParamChangeStringMH(char* Sender, char* param, uint8_t IsServer)
#else
void MakeParamChangeString(char* Sender, char* param, uint8_t IsServer)
#endif
{

	char ss[20];
	uint16_t i;
	uint8_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	Ql_memset(dataBuffer,0,DATA_MAX_BUFF);
	Ql_sprintf(dataBuffer,"$NMP,%s,",VTSData.VendorID);
	Ql_strcat(dataBuffer,FirmVer);

	Ql_strcat(dataBuffer,",OT,12,L,"); // OTA 
	
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,',');
	if(GPS.GPSFix)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	//AppendVariableString(dataBuffer,sSpeed,7,1,"000.0");
	StringAdd(dataBuffer,"%05.1f",GPS.Speed);
	InsertChar(dataBuffer,',');
	//InsertFloatValue(dataBuffer,GPS.Heading,"%06.2f");
	//AppendVariableString(dataBuffer,sHeading,7,1,"000.0");
	StringAdd(dataBuffer,"%06.2f",GPS.Heading);
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GPS.NoOfSatalite,"%02d");
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%03.1f",GPS.Altitude);
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%04.1f",GPS.PDOP);
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%04.1f",GPS.HDOP);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,NetWork.Network);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IsMain + '0');
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.MainsVolt,"%04.1f");
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.BattVolt,"%03.1f");
	InsertChar(dataBuffer,',');
	if(VAlert[SOS_ON_ALERT].Enable || VAlert[SOS_OFF_ALERT].Enable)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');

	if(PeriPheralVal.IsCoverOpen)
		InsertChar(dataBuffer,'O');
	else
		InsertChar(dataBuffer,'C');
	InsertChar(dataBuffer,',');

	


	InsertIntValue(dataBuffer,GSM.SignalStrength,"%2d");
	InsertChar(dataBuffer,',');
	
	InsertIntValue(dataBuffer,GSM.MCC,"%02d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GSM.MNC,"%02d");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.LAC,5,4,"00D6");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.CellID,5,4,"CFBD");
	InsertChar(dataBuffer,',');

	for(i=0;i<4;i++)
	{
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellDB,4,1,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].LAC,6,2,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellID,5,1,"0");
		InsertChar(dataBuffer,',');	
	}
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(INPUT_SOS_VAL)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,FrameNumber,"%06d");
	InsertChar(dataBuffer,',');

	StringAdd(dataBuffer,"%04.1f",PeriPheralVal.AN1);
	InsertChar(dataBuffer,',');

	StringAdd(dataBuffer,"%04.1f",PeriPheralVal.AN2);
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,VTSState.OdoCount/1000,"%01d");
	InsertChar(dataBuffer,',');

	if(IsSRCMD)
	{
		InsertChar(dataBuffer,'(');
		Ql_strcat(dataBuffer,Sender);
		InsertChar(dataBuffer,',');
		Ql_strcat(dataBuffer,param);
		InsertChar(dataBuffer,',');
		Ql_strcat(dataBuffer,CMD_Buff);
		InsertChar(dataBuffer,')');
	}
	else
	{
		InsertChar(dataBuffer,'(');
		Ql_strcat(dataBuffer,Sender);
		InsertChar(dataBuffer,',');
		Ql_strcat(dataBuffer,param);
		InsertChar(dataBuffer,',');
		Ql_strcat(dataBuffer,"1)");
	}

	

	
	// crc = CRC8(dataBuffer,Ql_strlen(dataBuffer));
	// Ql_sprintf(ss,"*%02X",crc);
	lastcrc= crc;
	Ql_sprintf(ss,"*");
	Ql_strcat(dataBuffer,ss);
	FrameNumber++;

}

#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_OG)
#ifdef ENABLE_UNIFIED_FIRMWARE
void MakeParamChangeStringOG(char* Sender, char* param, uint8_t IsServer)
#else
void MakeParamChangeString(char* Sender, char* param, uint8_t IsServer)
#endif
{

	char ss[20];
	uint16_t i;
	uint8_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	Ql_memset(dataBuffer,0,DATA_MAX_BUFF);
	Ql_sprintf(dataBuffer,"$,NMP,%s,",VTSData.VendorID);
	Ql_strcat(dataBuffer,FirmVer);

	Ql_strcat(dataBuffer,",OT,12,L,"); // OTA 
	
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	if(GPS.GPSFix)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');

	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	//AppendVariableString(dataBuffer,sSpeed,7,1,"000.0");
	StringAdd(dataBuffer,"%05.1f",GPS.Speed);
	InsertChar(dataBuffer,',');
	//InsertFloatValue(dataBuffer,GPS.Heading,"%06.2f");
	//AppendVariableString(dataBuffer,sHeading,7,1,"000.0");
	StringAdd(dataBuffer,"%06.2f",GPS.Heading);
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GPS.NoOfSatalite,"%02d");
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%03.1f",GPS.Altitude);
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%04.1f",GPS.PDOP);
	InsertChar(dataBuffer,',');
	
	StringAdd(dataBuffer,"%04.1f",GPS.HDOP);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,NetWork.Network);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IsMain + '0');
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.MainsVolt,"%04.1f");
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.BattVolt,"%03.1f");
	InsertChar(dataBuffer,',');
	if(VAlert[SOS_ON_ALERT].Enable || VAlert[SOS_OFF_ALERT].Enable)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');

	if(PeriPheralVal.IsCoverOpen)
		InsertChar(dataBuffer,'O');
	else
		InsertChar(dataBuffer,'C');
	InsertChar(dataBuffer,',');

	


	InsertIntValue(dataBuffer,GSM.SignalStrength,"%2d");
	InsertChar(dataBuffer,',');
	
	InsertIntValue(dataBuffer,GSM.MCC,"%02d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GSM.MNC,"%02d");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.LAC,5,4,"00D6");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.CellID,5,4,"CFBD");
	InsertChar(dataBuffer,',');

	for(i=0;i<4;i++)
	{
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellID,5,1,"0");
		InsertChar(dataBuffer,',');	
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].LAC,6,2,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellDB,4,1,"0");
		InsertChar(dataBuffer,',');
	}
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(INPUT_SOS_VAL)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,FrameNumber,"%06d");
	InsertChar(dataBuffer,',');

	
		


	

	
	crc = CRC8(dataBuffer,Ql_strlen(dataBuffer));
	Ql_sprintf(ss,"%02X",crc);
	Ql_strcat(dataBuffer,ss);
	lastcrc= crc;
	Ql_sprintf(ss,",*\n");
	Ql_strcat(dataBuffer,ss);
	FrameNumber++;

	if(IsServer==OTA_SRC_SMS)
		Ql_strcat(dataBuffer,"SMS,");
	else
		Ql_strcat(dataBuffer,"SERVER,");

	Ql_strcat(dataBuffer,Sender);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,param);
	if(IsSRCMD)
	{
		InsertChar(dataBuffer,'-');
		Ql_strcat(dataBuffer,CMD_Buff);
	}

}
#endif

#if defined(PROTO_CDAC)

void SMSAlert(uint8_t AlertNum)
{
	uint8_t tm=125;
	if(GSM.GSMState < GPRS_INIT)
		return;
	Ql_memset(SimData,0x00,MSGSIZE);
	InsertIntValue_OLD(SimData,AlertNum,"%02d");
	InsertChar(SimData,',');
	Ql_strncat(SimData,NetWork.IMEI,15);
	InsertChar(SimData,',');
	Ql_strcat(SimData,VTSData.VehicleData.VehicleRegNo);
	InsertChar(SimData,',');
	InsertCurrentDateTime_OLD(SimData,0);
	InsertChar(SimData,',');
	InsertCurrentDateTime_OLD(SimData,1);
	InsertChar(SimData,',');
	Ql_strcat(SimData,sLatitude);
	InsertChar(SimData,',');
	InsertChar(SimData,GPS.LatDir);
	InsertChar(SimData,',');
	Ql_strcat(SimData,sLongitude);
	InsertChar(SimData,',');
	InsertChar(SimData,GPS.LngDir);
	while(IsSendProcess)
	{
		ThreadSleep(50);
		if(--tm == 0)
			break;
	}
	SendSMS(VTSData.PhoneNumber.Mob0,SimData);

	Ql_memset(SimData,0x00,MSGSIZE);
	switch(AlertNum)
	{
		case 3 : Ql_strcpy(SimData,"Main Battery Removed");break;
		case 10: Ql_strcpy(SimData,"Emergency State ON");break;
		case 11: Ql_strcpy(SimData,"Emergency State OFF");break;
		case 16: Ql_strcpy(SimData,"Emergency button wire disconnect");break;
		case 17: Ql_strcpy(SimData,"OverSpeed");break;
		case 22: Ql_strcpy(SimData,"Vehicle Tilt");break;
		case 20: Ql_strcpy(SimData,"Overspeed in Geofence");break;
		case 23: Ql_strcpy(SimData,"Impact");break;
	}
	InsertChar(SimData,' ');
	Ql_strcat(SimData,VTSData.VehicleData.VehicleRegNo);
	InsertChar(SimData,' ');
	StringAdd(SimData,"%02d-%02d-20%02d",CurrentDateTime.Date,CurrentDateTime.Month,CurrentDateTime.Year);
	InsertChar(SimData,' ');
	StringAdd(SimData,"%02d:%02d:%02d",CurrentDateTime.Hour,CurrentDateTime.Min,CurrentDateTime.Sec);
	InsertChar(SimData,' ');
	Ql_strcat(SimData,sLatitude);
	InsertChar(SimData,' ');
	Ql_strcat(SimData,sLongitude);
	while(IsSendProcess)
	{
		ThreadSleep(50);
		if(--tm == 0)
			break;
	}
	SendSMS(VTSData.PhoneNumber.Mob0, SimData);
	if(!Ql_strstr(VTSData.PhoneNumber.Mob1,"0000000") && Ql_strlen(VTSData.PhoneNumber.Mob1) > 3)
		SendSMS(VTSData.PhoneNumber.Mob1,SimData);
	if(!Ql_strstr(VTSData.PhoneNumber.Mob2,"0000000") && Ql_strlen(VTSData.PhoneNumber.Mob2) > 3)
		SendSMS(VTSData.PhoneNumber.Mob2,SimData);
	if(!Ql_strstr(VTSData.PhoneNumber.Mob3,"0000000") && Ql_strlen(VTSData.PhoneNumber.Mob3) > 3)
		SendSMS(VTSData.PhoneNumber.Mob3,SimData);
	if(!Ql_strstr(VTSData.PhoneNumber.Mob4,"0000000") && Ql_strlen(VTSData.PhoneNumber.Mob4) > 3)
		SendSMS(VTSData.PhoneNumber.Mob4,SimData);


}

void FullPacket(void)
{
	uint16_t dLen=8;
	uint16_t i=0;
	uint16_t pos;
	char tempData[10];
	unsigned short crc;
	Ql_memset(SendString,'0',DATA_MAX_BUFF);
	InsertStringValue("vltdata=",0,8,0);
	InsertStringValue("FUL",dLen + 0,3,0);
	InsertStringValue(NetWork.IMEI,dLen + 3,15,1);
	InsertStringValue("25L",dLen + 18,3,0);
	SendString[dLen + 21]=GPS.GPSFix + '0';
	InsertCurrentDateTimeAt(dLen + 22);
	InsertFloatValueCDAC(GPS.Latitude,dLen + 34,10,"%010.6f");
	SendString[dLen + 44]=GPS.LatDir;
	InsertFloatValueCDAC(GPS.Longitude,dLen + 45,10,"%010.6f");
	SendString[dLen + 55]=GPS.LngDir;
	InsertIntValueCDAC(GSM.MCC,dLen + 56,3);
	InsertMNCCDAC(GSM.MNC,dLen + 59);
	InsertStringValue(GSM.LAC,dLen + 62,4,1);
	InsertStringValue(GSM.CellID,dLen + 66,9,1);
	InsertFloatValueCDAC(GPS.Speed,dLen + 75,6,"%06.2f");
	InsertFloatValueCDAC(GPS.Heading,dLen + 81,6,"%06.2f");
	InsertIntValueCDAC(GPS.NoOfSatalite,dLen + 87,2);
	InsertIntValueCDAC((uint16_t)GPS.HDOP,dLen + 89,2);
	InsertIntValueCDAC(GSM.SignalStrength,dLen + 91,2);
	SendString[dLen + 93]=PeriPheralVal.IGN + '0';
	SendString[dLen + 94]=PeriPheralVal.IsMain + '0';
	SendString[dLen + 95]=VehicleMovingMode;
	InsertStringValue(VTSData.VendorID,dLen + 96,6,1);
	InsertStringValue(FirmVer,dLen + 102,6,1);
	InsertStringValue(VTSData.VehicleData.VehicleRegNo,dLen + 108,16,1);
	InsertFloatValueCDAC(GPS.Altitude,dLen + 124,7,"%07.2f");
	InsertIntValueCDAC((uint16_t)GPS.PDOP,dLen + 131,2);
	InsertStringValue(NetWork.Network,dLen + 133,6,1);
	pos=dLen + 139;
	while(i<4)
	{
		InsertStringValue(GSM.NeigbourCell[i].CellDB,pos,2,1);
		pos += 2;
		InsertStringValue(GSM.NeigbourCell[i].LAC,pos,4,1);
		pos += 4;
		InsertStringValue(GSM.NeigbourCell[i++].CellID,pos,9,1);
		
		pos += 9;
	}
	InsertFloatValueCDAC(PeriPheralVal.MainsVolt,dLen + 199,5,"%05.1f");
	InsertFloatValueCDAC(PeriPheralVal.BattVolt,dLen + 204,5,"%05.1f");
	if(PeriPheralVal.IsCoverOpen)
		SendString[dLen + 209]='O';
	else
		SendString[dLen + 209]='C';
	SendString[dLen + 210]=PeriPheralVal.IP1 + '0';
	SendString[dLen + 211]=PeriPheralVal.IP2 + '0';
	SendString[dLen + 212]=PeriPheralVal.OP1 + '0';
	SendString[dLen + 213]=PeriPheralVal.OP2 + '0';
	InsertIntValueCDAC(FrameNumber,dLen + 214,6);
	crc = CRC16(&SendString[dLen],220);
	Ql_sprintf(tempData,"%08X",crc);
	lastcrc= crc;
	InsertStringValue(tempData,dLen + 220,8,1);
	SendString[dLen + 228]='\0';
	FrameNumber++;
}

static uint8_t GetBatchPacketPriority(const char *packet)
{
	if(packet == NULL || Ql_strlen(packet) < 11)
	{
		return 4;
	}

	packet += 8;
	if(Ql_strncmp(packet, "EPB", 3) == 0)
		return 0;
	if(Ql_strncmp(packet, "CRT", 3) == 0)
		return 1;
	if(Ql_strncmp(packet, "ALT", 3) == 0)
		return 2;
	if(Ql_strncmp(packet, "ACK", 3) == 0)
		return 3;
	return 4;
}

static int16_t s_batchPacketsSlots[2] = {-1, -1};
static uint8_t s_batchPacketsCount = 0;

void MakeBatchPacket(uint8_t count, uint8_t* nmcount, uint8_t* critcount)
{
	uint16_t dLen=8;
	char histPackets[2][256];

	s_batchPacketsCount = 0;
	s_batchPacketsSlots[0] = -1;
	s_batchPacketsSlots[1] = -1;
	uint8_t histPriority[2] = {0};
	uint8_t histStorageType[2] = {0};
	char tempPacket[256];
	uint8_t histCount = 0;
	uint8_t readAlerts = 1;
	int i;
	int j;

	Ql_memset(SendString,0,DATA_MAX_BUFF);
	Ql_memset(SendString,'0',118+2);
	InsertStringValue("vltdata=",0,8,0);
	InsertStringValue("BTH",dLen + 0,3,0);	
	InsertStringValue(NetWork.IMEI,dLen + 3,15,0);
	InsertIntValueCDAC(count,dLen + 18,3);
	InsertStringValue("01L",dLen + 21,3,0);
	SendString[dLen + 24]=GPS.GPSFix + '0';
	InsertCurrentDateTimeAt(dLen + 25);
	InsertFloatValueCDAC(GPS.Latitude,dLen + 37,10,"%010.6f");
	SendString[dLen + 47]=GPS.LatDir;
	InsertFloatValueCDAC(GPS.Longitude,dLen + 48,10,"%010.6f");
	SendString[dLen + 58]=GPS.LngDir;
	InsertIntValueCDAC(GSM.MCC,dLen + 59,3);
	InsertMNCCDAC(GSM.MNC,dLen + 62);
	InsertStringValue(GSM.LAC,dLen + 65,4,1);
	InsertStringValue(GSM.CellID,dLen + 69,9,1);
	InsertFloatValueCDAC(GPS.Speed,dLen + 78,6,"%03.2f");
	InsertFloatValueCDAC(GPS.Heading,dLen + 84,6,"%03.2f");
	InsertIntValueCDAC(GPS.NoOfSatalite,dLen + 90,2);
	InsertIntValueCDAC((uint16_t)GPS.HDOP,dLen + 92,2);
	InsertIntValueCDAC(GSM.SignalStrength,dLen + 94,2);
	SendString[dLen + 96]=PeriPheralVal.IGN + '0';
	SendString[dLen + 97]=PeriPheralVal.IsMain + '0';
	SendString[dLen + 98]=VehicleMovingMode;	
	InsertFloatValueCDAC(GPS.Altitude,dLen + 99,7,"%04.2f");
	char spn[7];
	Ql_sprintf(spn,"%s",NetWork.Network);
	int rem = 6 - Ql_strlen(NetWork.Network);
	for(int i =0; i < rem; i++)
		spn[5 - i] = 'X';

	// SendString[dLen + 100] = 0;
	// Ql_strncat(SendString,NetWork.Network,6);
	
	
	// SendString[dLen+107] = 0;
	InsertStringValue(spn,dLen + 103,6,1);
	uint8_t nmindex=0, critindex=0;
	*nmcount=0;
	*critcount=0;
	while(histCount < count && histCount < 2)
	{
		int status=0;
		int16_t slotNum = -1;
		Ql_memset(tempPacket,0,sizeof(tempPacket));
		if(readAlerts){
			status = ReadDataBatchExt(tempPacket,ALERT,1,critindex++,0, &slotNum);
			if(!status)
			{
				readAlerts=0;
				continue;
			}
		}
		else
			status = ReadDataBatchExt(tempPacket,NORMAL,1,nmindex++,0, &slotNum);
		
		if(status)
		{
			char* pt = Ql_strstr(tempPacket,NetWork.IMEI);
			int pklen = Ql_strlen(tempPacket);
			if(!pt)
			{
				LOGData(TAG_SERVER,"invalid history packet: IMEI not found, skip=%d", nmindex - 1);
				continue;
			}
			if(pklen < 50 || pklen > 256)
			{
				LOGData(TAG_SERVER,"invalid history packet len %d, skip=%d", pklen, nmindex - 1);
				continue;
			}

			Ql_strncpy(histPackets[histCount], tempPacket, sizeof(histPackets[histCount]) - 1);
			histPackets[histCount][sizeof(histPackets[histCount]) - 1] = '\0';
			histPriority[histCount] = GetBatchPacketPriority(histPackets[histCount]);
			histStorageType[histCount] = readAlerts ? ALERT : NORMAL;

			if(readAlerts)
				critcount[0]++;
			else
				nmcount[0]++;
			
			if(s_batchPacketsCount < 2)
			{
				s_batchPacketsSlots[s_batchPacketsCount++] = slotNum;
			}
			histCount++;
		}
		else if(!readAlerts)
		{
			break;
		}
	}

	for(i = 0; i < histCount; i++)
	{
		for(j = i + 1; j < histCount; j++)
		{
			if(histPriority[j] < histPriority[i])
			{
				char swapPacket[256];
				uint8_t swapPriority;
				uint8_t swapStorage;

				Ql_memcpy(swapPacket, histPackets[i], sizeof(swapPacket));
				Ql_memcpy(histPackets[i], histPackets[j], sizeof(histPackets[i]));
				Ql_memcpy(histPackets[j], swapPacket, sizeof(swapPacket));

				swapPriority = histPriority[i];
				histPriority[i] = histPriority[j];
				histPriority[j] = swapPriority;

				swapStorage = histStorageType[i];
				histStorageType[i] = histStorageType[j];
				histStorageType[j] = swapStorage;
			}
		}
	}

	for(i = 0; i < histCount; i++)
	{
		char* pt = Ql_strstr(histPackets[i], NetWork.IMEI);
		int pklen = Ql_strlen(histPackets[i]);
		int current_len;
		int pt_len;

		if(!pt)
		{
			continue;
		}

		pt += 15;
		if((pt - histPackets[i]) + 3 <= pklen)
		{
			if(pt[0] == '0' && pt[1] == '1')
			{
				pt[1] = '2';
			}
			pt[2] = 'H';
		}

		current_len = Ql_strlen(SendString);
		pt_len = Ql_strlen(pt);
		if(current_len + pt_len < DATA_MAX_BUFF)
		{
			Ql_strcat(SendString, pt);
		}
		else
		{
			LOGData(TAG_SERVER,"SendString buffer full, cannot append history packet");
			break;
		}
	}
	LOGData(TAG_SERVER,"Total Batch hPacket - crit: %d, non-crit: %d ",*critcount,*nmcount);
}

// void UpdateOTA(char* data, uint8_t IsSet)
// {
// 	char* fn = data+4;
// 	Ql_memset(&OTAValue,0,sizeof(OTAValue));
// 	uint16_t ln=Ql_strlen(fn);
// 	uint8_t i=0, n=0, j=0;
// 	if(IsSet)  // SET PU:something,SU:something     or    SET EO      or    SET EP:something
// 	{
// 		while(n < ln)
// 		{
// 			if(*fn==':')
// 			{
// 				if(i == 0)
// 				{
// 					i=1;
// 					j=0;
// 					n++;
// 					fn++;
// 				}
				
// 			}
// 			else if(*fn==',')
// 			{
// 				i=0;
// 				j=0;
// 				n++;
// 				fn++;
// 				OTAValue.TotalOTA++;
// 				if(OTAValue.TotalOTA >= 6)
// 					return;
// 			}
// 			if(i==0)
// 			{
// 				OTAValue.OTData[OTAValue.TotalOTA].KeyVal[j]=*fn;
// 			}
// 			else
// 			{
// 				if(*fn == 0x0D)
// 					*fn=0;
// 				OTAValue.OTData[OTAValue.TotalOTA].Value[j]=*fn;
// 			}
// 			j++;
// 			n++;
// 			fn++;
			
// 		}
// 		if (i > 0)
// 				OTAValue.TotalOTA++;
// 	}
// 	else
// 	{
// 		while(n < ln) // GET PU,EM,SM                  or   GET PU
// 		{
// 			if(*fn==',')
// 			{
// 				i=1;
// 				n=0;
// 			}
// 			else if(*fn == '\r' || *fn == '\n' || *fn == '\0')
// 			{
// 				i=1;
				
// 				n=1;
// 			}
// 			else 
// 				i=0;
// 			if(i==0)
// 			{
// 				OTAValue.OTData[OTAValue.TotalOTA].KeyVal[j++]=*fn;
// 			}
// 			else
// 			{
// 				OTAValue.OTData[OTAValue.TotalOTA].KeyVal[j]=0;
// 				OTAValue.TotalOTA++;
// 				j=0;
// 				if(n==1)
// 					break;
// 			}
// 			fn++;
			
// 		}
// 	}
	
		
// }
static char* OTA_SkipCommand(char *requestData)
{
	char *p = requestData;

	while(*p && *p != ' ' && *p != '\t') {
		p++;
	}
	while(*p == ' ' || *p == '\t') {
		p++;
	}
	return p;
}

static void OTA_TrimTrailingSpaces(char *text)
{
	int len = Ql_strlen(text);

	while(len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t')) {
		text[len - 1] = '\0';
		len--;
	}
}

void UpdateOTA(char* requestData, uint8_t isSetCommand)
{
	char* currentChar;

	Ql_memset(&OTAValue, 0, sizeof(OTAValue));
	if(requestData == NULL) {
		return;
	}

	currentChar = OTA_SkipCommand(requestData);
	while(*currentChar && OTAValue.TotalOTA < MAX_OTA_SIZE)
	{
		uint8_t keyIndex = 0;
		uint8_t valueIndex = 0;
		ParamOTATypedef *item = &OTAValue.OTData[OTAValue.TotalOTA];

		while(*currentChar == ' ' || *currentChar == '\t' || *currentChar == ',') {
			currentChar++;
		}
		if(*currentChar == '\0' || *currentChar == '\r' || *currentChar == '\n') {
			break;
		}

		while(*currentChar &&
			  *currentChar != ':' &&
			  *currentChar != ',' &&
			  *currentChar != '\r' &&
			  *currentChar != '\n' &&
			  *currentChar != ' ' &&
			  *currentChar != '\t')
		{
			if(keyIndex < (sizeof(item->KeyVal) - 1)) {
				item->KeyVal[keyIndex++] = *currentChar;
			}
			currentChar++;
		}
		item->KeyVal[keyIndex] = '\0';

		if(isSetCommand) {
			if(*currentChar == ':') {
				currentChar++;
			} else {
				while(*currentChar == ' ' || *currentChar == '\t') {
					currentChar++;
				}
			}

			while(*currentChar &&
				  *currentChar != ',' &&
				  *currentChar != '\r' &&
				  *currentChar != '\n')
			{
				if(valueIndex < (sizeof(item->Value) - 1)) {
					item->Value[valueIndex++] = *currentChar;
				}
				currentChar++;
			}
			item->Value[valueIndex] = '\0';
			OTA_TrimTrailingSpaces(item->Value);
		}

		OTA_TrimTrailingSpaces(item->KeyVal);
		if(item->KeyVal[0] != '\0') {
			OTAValue.TotalOTA++;
		}

		if(*currentChar == ',') {
			currentChar++;
		}
	}
}
static void ServerCopyText(char *dest, uint16_t destSize, const char *src)
{
	if(destSize == 0) {
		return;
	}

	dest[0] = '\0';
	if(src == NULL) {
		return;
	}

	Ql_strncpy(dest, src, destSize - 1);
	dest[destSize - 1] = '\0';
}

static void ServerAppendText(char *dest, uint16_t destSize, const char *src)
{
	uint16_t used;

	if(destSize == 0 || src == NULL) {
		return;
	}

	used = Ql_strlen(dest);
	if(used >= (destSize - 1)) {
		return;
	}

	Ql_strncpy(dest + used, src, destSize - used - 1);
	dest[destSize - 1] = '\0';
}

static char* ServerEndpointAfterScheme(char *value)
{
	if(Ql_strncmp(value, "https://", 8) == 0) {
		return value + 8;
	}
	if(Ql_strncmp(value, "http://", 7) == 0) {
		return value + 7;
	}
	return value;
}

static uint8_t ServerTextIsDigits(char *start, char *end)
{
	if(start >= end) {
		return 0;
	}

	while(start < end) {
		if(*start < '0' || *start > '9') {
			return 0;
		}
		start++;
	}
	return 1;
}

static char* ServerFindPortSeparator(char *url)
{
	char *host = ServerEndpointAfterScheme(url);
	char *slash = Ql_strchr(host, '/');
	char *colon = NULL;
	char *p = host;

	while(*p && (slash == NULL || p < slash)) {
		if(*p == ':') {
			colon = p;
		}
		p++;
	}

	if(colon == NULL) {
		return NULL;
	}

	if(ServerTextIsDigits(colon + 1, (slash != NULL) ? slash : p)) {
		return colon;
	}
	return NULL;
}

static void ServerNormalizeEndpoint(const char *value, char *urlOut, uint16_t urlOutSize, char *portOut, uint16_t portOutSize)
{
	char url[128];
	char host[128];
	char *endpoint;
	char *slash;
	char *colon;
	char *portEnd;
	uint16_t portLen;

	if(urlOutSize == 0 || portOutSize == 0) {
		return;
	}

	ServerCopyText(url, sizeof(url), value);
	if(Ql_strncmp(url, "//", 2) == 0) {
		char repairedUrl[128];
		ServerCopyText(repairedUrl, sizeof(repairedUrl), "https:");
		ServerAppendText(repairedUrl, sizeof(repairedUrl), url);
		ServerCopyText(url, sizeof(url), repairedUrl);
	}

	endpoint = ServerEndpointAfterScheme(url);
	slash = Ql_strchr(endpoint, '/');
	if(slash != NULL) {
		*slash = '\0';
	}

	ServerCopyText(host, sizeof(host), endpoint);
	colon = ServerFindPortSeparator(host);
	if(colon != NULL) {
		portEnd = host + Ql_strlen(host);
		portLen = (uint16_t)(portEnd - (colon + 1));
		if(portLen >= portOutSize) {
			portLen = portOutSize - 1;
		}
		Ql_memcpy(portOut, colon + 1, portLen);
		portOut[portLen] = '\0';
		*colon = '\0';
	} else if(Ql_strncmp(url, "https://", 8) == 0) {
		/* Only apply scheme default when no port was pre-configured */
		if(portOut[0] == '\0' || Ql_strcmp(portOut, "0") == 0)
			ServerCopyText(portOut, portOutSize, "443");
	} else if(Ql_strncmp(url, "http://", 7) == 0) {
		/* Only apply scheme default when no port was pre-configured */
		if(portOut[0] == '\0' || Ql_strcmp(portOut, "0") == 0)
			ServerCopyText(portOut, portOutSize, "80");
	}

	/* Preserve the full URL (scheme + host + path) so IsHttpUrl() can detect
	 * HTTP servers correctly after storage. For bare IP:port inputs (no scheme)
	 * store just the host as before. */
	if(Ql_strncmp(url, "http://", 7) == 0 || Ql_strncmp(url, "https://", 8) == 0) {
		/* Rebuild: scheme://host:port/path  — use original value, normalised only */
		ServerCopyText(urlOut, urlOutSize, value);
	} else {
		ServerCopyText(urlOut, urlOutSize, host);
	}
}

void UpdateURL(char* value)
{
	char url[128];
	char port[sizeof(VTSData.ServerData.Port1)];

	if (!value) return;

	ServerCopyText(port, sizeof(port), VTSData.ServerData.Port1);
	ServerNormalizeEndpoint(value, url, sizeof(url), port, sizeof(port));

	Ql_strncpy(VTSData.ServerData.Port1, port, sizeof(VTSData.ServerData.Port1) - 1);
	VTSData.ServerData.Port1[sizeof(VTSData.ServerData.Port1) - 1] = '\0';
	Ql_strncpy(VTSData.ServerData.IP1, url, sizeof(VTSData.ServerData.IP1) - 1);
	VTSData.ServerData.IP1[sizeof(VTSData.ServerData.IP1) - 1] = '\0';

    ServerSocket[0].SocketState = SOCKET_CLOSED;
    Ql_strncpy(ServerSocket[0].DNSorIP, VTSData.ServerData.IP1, sizeof(ServerSocket[0].DNSorIP) - 1);
    ServerSocket[0].DNSorIP[sizeof(ServerSocket[0].DNSorIP) - 1] = '\0';
    ServerSocket[0].Port = Ql_atoi(VTSData.ServerData.Port1);
    LOGData(TAG_SERVER, "Primary URL updated: %s:%s", VTSData.ServerData.IP1, VTSData.ServerData.Port1);
}

void UpdateSecondaryURL(char* value)
{
	char url[128];
	char port[sizeof(VTSData.ServerData.Port3)];

	if (!value) return;

	ServerCopyText(port, sizeof(port), VTSData.ServerData.Port3);
	ServerNormalizeEndpoint(value, url, sizeof(url), port, sizeof(port));

	Ql_strncpy(VTSData.ServerData.Port3, port, sizeof(VTSData.ServerData.Port3) - 1);
	VTSData.ServerData.Port3[sizeof(VTSData.ServerData.Port3) - 1] = '\0';
	Ql_strncpy(VTSData.ServerData.IP3, url, sizeof(VTSData.ServerData.IP3) - 1);
	VTSData.ServerData.IP3[sizeof(VTSData.ServerData.IP3) - 1] = '\0';
	ServerSocket[2].isEnabled = IsHttpUrl(VTSData.ServerData.IP3) ? 0 : 1;
	ServerSocket[2].SocketState = SOCKET_CLOSED;
    Ql_strncpy(ServerSocket[2].DNSorIP, VTSData.ServerData.IP3, sizeof(ServerSocket[2].DNSorIP) - 1);
    ServerSocket[2].DNSorIP[sizeof(ServerSocket[2].DNSorIP) - 1] = '\0';
    ServerSocket[2].Port = Ql_atoi(VTSData.ServerData.Port3);
    LOGData(TAG_SERVER, "Secondary URL updated: %s:%s (mode=%s)", VTSData.ServerData.IP3, VTSData.ServerData.Port3, ServerSocket[2].isEnabled ? "TCP" : "HTTP");
}

#ifdef EXTENDED_IPS
void UpdateTertiaryURL(char* value)
{
	char url[128];
	char port[sizeof(VTSData.ServerData.Port4)];

	if (!value) return;

	ServerCopyText(port, sizeof(port), VTSData.ServerData.Port4);
	ServerNormalizeEndpoint(value, url, sizeof(url), port, sizeof(port));

	Ql_strncpy(VTSData.ServerData.Port4, port, sizeof(VTSData.ServerData.Port4) - 1);
	VTSData.ServerData.Port4[sizeof(VTSData.ServerData.Port4) - 1] = '\0';
	Ql_strncpy(VTSData.ServerData.IP4, url, sizeof(VTSData.ServerData.IP4) - 1);
	VTSData.ServerData.IP4[sizeof(VTSData.ServerData.IP4) - 1] = '\0';
	ServerSocket[3].isEnabled = IsHttpUrl(VTSData.ServerData.IP4) ? 0 : 1;
	ServerSocket[3].SocketState = SOCKET_CLOSED;
    Ql_strncpy(ServerSocket[3].DNSorIP, VTSData.ServerData.IP4, sizeof(ServerSocket[3].DNSorIP) - 1);
    ServerSocket[3].DNSorIP[sizeof(ServerSocket[3].DNSorIP) - 1] = '\0';
    ServerSocket[3].Port = Ql_atoi(VTSData.ServerData.Port4);
    LOGData(TAG_SERVER, "Tertiary URL updated: %s:%s (mode=%s)", VTSData.ServerData.IP4, VTSData.ServerData.Port4, ServerSocket[3].isEnabled ? "TCP" : "HTTP");
}
#endif

void UpdateVehicleNumber(char* value)
{
	if (!value) return;
	
	Ql_strncpy(VTSData.VehicleData.VehicleRegNo, value, sizeof(VTSData.VehicleData.VehicleRegNo) - 1);
	VTSData.VehicleData.VehicleRegNo[sizeof(VTSData.VehicleData.VehicleRegNo) - 1] = '\0';
}

void UpdateMoblieNo(char* value, uint8_t num)
{
	if (!value) return;
	
	uint16_t j=Ql_strlen(value);
	if((j >3) && (j < 21))
	{
		switch(num)
		{
			case 1:
				Ql_strncpy(VTSData.PhoneNumber.Mob0, value, sizeof(VTSData.PhoneNumber.Mob0) - 1);
				VTSData.PhoneNumber.Mob0[sizeof(VTSData.PhoneNumber.Mob0) - 1] = '\0';
				break;
			case 2:
				Ql_strncpy(VTSData.PhoneNumber.Mob1, value, sizeof(VTSData.PhoneNumber.Mob1) - 1);
				VTSData.PhoneNumber.Mob1[sizeof(VTSData.PhoneNumber.Mob1) - 1] = '\0';
				break;
			case 3:
				Ql_strncpy(VTSData.PhoneNumber.Mob2, value, sizeof(VTSData.PhoneNumber.Mob2) - 1);
				VTSData.PhoneNumber.Mob2[sizeof(VTSData.PhoneNumber.Mob2) - 1] = '\0';
				break;
			case 4:
				Ql_strncpy(VTSData.PhoneNumber.Mob3, value, sizeof(VTSData.PhoneNumber.Mob3) - 1);
				VTSData.PhoneNumber.Mob3[sizeof(VTSData.PhoneNumber.Mob3) - 1] = '\0';
				break;
			case 5:
				Ql_strncpy(VTSData.PhoneNumber.Mob4, value, sizeof(VTSData.PhoneNumber.Mob4) - 1);
				VTSData.PhoneNumber.Mob4[sizeof(VTSData.PhoneNumber.Mob4) - 1] = '\0';
				break;
			default:
				break;
		}
	}
}

void UpdateSleepTime(char* value)
{
	uint16_t j=atoi(value);
	if((j >0) && (j < 1000))
	{
		VTSData.IntervalData.SleepTime=j * 60;
	}
}

void UpdateHaltTime(char* value)
{
	uint16_t j=atoi(value);
	if((j >0) && (j < 1000))
	{
		VTSData.IntervalData.HaltTime=j * 60;
	}
}

void UpdateDefaultSpeedLimit(char* value)
{
	uint16_t j=atoi(value);
	if((j >0) && (j < 1000))
	{
		VTSData.VehicleData.DefaultSpeed=j;
	}
}	

void UpdateSpeedLimit(char* value)
{
	uint16_t j=atoi(value);
	if((j >5) && (j < 1000))
	{
		VTSData.VehicleData.OverSpeed=j;
	}
}	

void UpdateHarshBreak(char* value)
{
	uint16_t j=atoi(value);
	if((j >0) && (j < 20000))
	{
		VTSData.VehicleData.HarshBreak=j;
	}
}	

void UpdateHarshAcck(char* value)
{
	uint16_t j=atoi(value);
	if((j >0) && (j < 20000))
	{
		VTSData.VehicleData.HarshAcc=j;
	}
}	

void UpdateRashTurn(char* value)
{
	uint16_t j=atoi(value);
	if((j >0) && (j < 20000))
	{
		VTSData.VehicleData.RashTurn=j;
	}
}	

void UpdateLowBattThr(char* value)
{
	uint16_t j=atoi(value);
	if((j >0) && (j < 100))
	{
		VTSData.BattThrs=batteryPercentageToVoltage(j);
	}
}

void UpdateTiltAngle(char* value)
{
	uint16_t j=atoi(value);
	if((j >0) && (j < 360))
	{
		VTSData.VehicleData.TiltAngle=j;
	}
}

void UpdateOTAInterval(char* value,uint16_t num)
{
	uint16_t j=atoi(value);
	if((j >0) && (j < 20000))
	{
		switch(num)
		{
			case 1:
				VTSData.IntervalData.HaltInterval=j * 60;			// URT
				break;
			case 2:
				VTSData.IntervalData.SleepInterval=j * 60;				// Sleep
				break;
			case 3:
				VTSData.IntervalData.EnergencyInterval=j;				// Emergency
				break;
			case 4:
				VTSData.IntervalData.FullDataPacketInterval=j * 60;		// FULL 
				break;
			case 5:
				VTSData.IntervalData.HealthInterval=j*60;						// Health
				break;
			case 6:
				VTSData.IntervalData.MotionInterval=j;							// Motion
				break;
			default:
				break;
		}
	}
}

void UpdateVID(char* value)
{
	uint16_t i=Ql_strlen(value);
	if((i>0) && (i<30))
	{
		strcpy(VTSData.VendorID,value);
	}
}



void DecodeOTAData(char* buff,uint8_t isserver)
{
	char* fn;
	char* ls;
	uint16_t i;
	char ss[30]={0};
	char rnd[10];
	OTAValue.TotalOTA=0;
	LOGData(TAG_SERVER,"Paring cmd from source %d",isserver);
	fn = Ql_strstr(buff,"\r\n\r\n");
	if(fn)
	{
		buff = fn+4;
		LOGData(TAG_SERVER,"skipped Http headers to %s",buff);
	}
	if(Ql_strlen(buff)<3)
		return;

	if(Ql_strstr(buff,"XSET")|| Ql_strstr(buff,"XGET"))
	{
		DecodeSMS(buff,isserver);
		return;
	}

	if(Ql_strstr(buff,"ACTV"))
	{
		ls = strchr(buff,',');
		if(!ls)
		{
			strncpy(ss,buff+5,16);
			if(Ql_strlen(ss)==16)
			{
				strcpy(ActivationKey,ss);
				LOGData(TAG_SERVER,"Activation Key %s Recieved, login packet initiated",ActivationKey);
				SendLogin1=1;
			}
			else
				LOGData(TAG_SERVER,"invalid actv key len! %d, key:%s",Ql_strlen(ss),ss);
		}
		else
		{
			fn=strchr(buff,',');
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
					LOGData(TAG_SERVER,"\r\nSending ACTV reply with Rc : %s to %s",rnd,ss);
					MakeACTMessage(0,  rnd);
					SendSMS(ss,SimData);
					return;
				}
			}
		}
	}
	if(Ql_strstr(buff,"HCHK"))
	{
		fn=strchr(buff,',');
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
				LOGData(TAG_SERVER,"\r\nSending HCHK reply with Rc : %s to %s",rnd,ss);
				MakeACTMessage(1,  rnd);
				SendSMS(ss,SimData);
				return;
			}
		}
	}
	Ql_memset(VAlert[CONF_CHANGE_ALERT].ACK,0x00,sizeof(VAlert[CONF_CHANGE_ALERT].ACK));
	if(!isserver)
	{
		Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,"OM");
		InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,':');
		Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,SMSSender);
		InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,',');
	}
	else
	{
		Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,"OU");
		InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,':');
		Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,VTSData.ServerData.IP1);
		InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,',');
	}

	fn=Ql_strstr(buff,"SET");
	if(fn)
	{
		LOGData(TAG_SERVER,"ota set cmd");
		ls=fn;
		if(Ql_strstr(fn,"FOTA"))
		{
			FOTAPacket(buff,SMSSender,1);
			return;
		}
		if(Ql_strstr(fn,"MOTA"))
		{
			MOTAPacket(buff,SMSSender,1);
			return;
		}
		ls=fn;
		fn=Ql_strstr(fn,"GF:");
		if(fn)
		{
			DecodeGeofence(fn);
			UpdateConfigInFlash();
			return;
		}
		fn = Ql_strstr(ls," EO"); // SET EO,MO:238923,UL:something
		if(fn)
		{
			ResetSOS();
			Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,"EO");
			InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,':');
			Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,"OFF");
			if(strchr(fn,','))
			{
				InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,',');
				ls = fn+5;
			}
			else
			{
				if(isserver==0)
					SendSMS(SMSSender,VAlert[CONF_CHANGE_ALERT].ACK);
				AddAlert(CONF_CHANGE_ALERT);
				ls = fn+4;
			}
		}
		UpdateOTA(ls,1);
		
		if(OTAValue.TotalOTA>0)
		{
			VAlert[CONF_CHANGE_ALERT].Enable=1;
			strcpy(VAlert[CONF_CHANGE_ALERT].Header,"ACK");
			
			for(i=0;i<OTAValue.TotalOTA;i++)
			{
				if(Ql_strstr(OTAValue.OTData[i].KeyVal,"PU"))
					UpdateURL(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"SU"))
					UpdateSecondaryURL(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"VN"))
					UpdateVehicleNumber(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M0"))
					UpdateMoblieNo(OTAValue.OTData[i].Value,1);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M1"))
					UpdateMoblieNo(OTAValue.OTData[i].Value,2);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M2"))
					UpdateMoblieNo(OTAValue.OTData[i].Value,3);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M3")) 
					UpdateMoblieNo(OTAValue.OTData[i].Value,4);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"OM"))
					UpdateMoblieNo(OTAValue.OTData[i].Value,5);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"ED"))
					UpdateSOSTimeOut(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"ST"))
					UpdateSleepTime(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"HT"))
					UpdateHaltTime(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"DSL"))
					UpdateDefaultSpeedLimit(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"SL"))
					UpdateSpeedLimit(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"HBT"))
					UpdateHarshBreak(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"HAT"))
					UpdateHarshAcck(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"RTT"))
					UpdateRashTurn(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"LBT"))
					UpdateLowBattThr(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"TA"))
					UpdateTiltAngle(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URT"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,1);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URS"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,2);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URE"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,3);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URF"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,4);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URH"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,5);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"UR"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,6);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"VID"))
					UpdateVID(OTAValue.OTData[i].Value);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"GFR"))
				{
					if(Ql_strstr(OTAValue.OTData[i].Value,"ON") || Ql_strcmp(OTAValue.OTData[i].Value,"1")==0)
						VTSData.DisableGPSFaultReset = 0;
					else
						VTSData.DisableGPSFaultReset = 1;
					UpdateConfigInFlash();
					Ql_strcpy(OTAValue.OTData[i].Value, VTSData.DisableGPSFaultReset ? "OFF" : "ON");
				}
				else
					strcpy(OTAValue.OTData[i].Value,"InvalidKey");


				Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,OTAValue.OTData[i].KeyVal);
				InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,':');
				Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,OTAValue.OTData[i].Value);
				if(i != OTAValue.TotalOTA-1)
					InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,',');
				VAlert[CONF_CHANGE_ALERT].WithACK=1;
			}
			InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,'*');
			if(isserver==0)
				SendSMS(SMSSender,VAlert[CONF_CHANGE_ALERT].ACK);
			else if(isserver==OTA_SRC_RS232)
				SendRS232Response(VAlert[CONF_CHANGE_ALERT].ACK);
			else if(isserver==OTA_SRC_RS485)
				SendRS485Response(VAlert[CONF_CHANGE_ALERT].ACK);
			
			AddAlert(CONF_CHANGE_ALERT);
			UpdateConfigInFlash();
			IsPacketReady.IsNormalPacket=1;
		}
	}
	fn=Ql_strstr(buff,"GET");
	if(fn)
	{
		LOGData(TAG_SERVER,"ota get cmd");

		ls=fn;
		UpdateOTA(ls,0);
		if(OTAValue.TotalOTA>0)
		{
			LOGData(TAG_SERVER,"ota count : %d",OTAValue.TotalOTA);
			VAlert[CONF_CHANGE_ALERT].Enable=1;
			strcpy(VAlert[CONF_CHANGE_ALERT].Header,"ACK");

			for(i=0;i<OTAValue.TotalOTA;i++)
			{
				Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,OTAValue.OTData[i].KeyVal);
				InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,':');
				char cc[100]={0};

				if(Ql_strstr(OTAValue.OTData[i].KeyVal,"PU"))
					Ql_sprintf(cc,"%s:%s",VTSData.ServerData.IP1,VTSData.ServerData.Port1);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"SU"))
					Ql_sprintf(cc,"%s:%s",VTSData.ServerData.IP3,VTSData.ServerData.Port3);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"VN"))
					strcpy(cc,VTSData.VehicleData.VehicleRegNo);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M0"))
					strcpy(cc,VTSData.PhoneNumber.Mob0);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M1"))
					strcpy(cc,VTSData.PhoneNumber.Mob1);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M2"))
					strcpy(cc,VTSData.PhoneNumber.Mob2);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M3"))
					strcpy(cc,VTSData.PhoneNumber.Mob3);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"OM"))
					strcpy(cc,VTSData.PhoneNumber.Mob4);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"ED")){
					Ql_sprintf(cc,"%d",VTSData.IntervalData.SOSTimeOut);
				}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"ST"))
					Ql_sprintf(cc,"%d",VTSData.IntervalData.SleepTime / 60);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"HT"))
					Ql_sprintf(cc,"%d",VTSData.IntervalData.HaltInterval / 60);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"DSL"))
					Ql_sprintf(cc,"%2.0f",VTSData.VehicleData.DefaultSpeed);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"SL"))
					Ql_sprintf(cc,"%2.0f",VTSData.VehicleData.OverSpeed);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"HBT"))
					Ql_sprintf(cc,"%d",VTSData.VehicleData.HarshBreak);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"HAT"))
					Ql_sprintf(cc,"%d",VTSData.VehicleData.HarshAcc);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"RTT"))
					Ql_sprintf(cc,"%d",VTSData.VehicleData.RashTurn);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"LBT"))
					Ql_sprintf(cc,"%i",batteryVoltageToPercentage(VTSData.BattThrs));
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"TA"))
					Ql_sprintf(cc,"%d",VTSData.VehicleData.TiltAngle);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URT"))
					Ql_sprintf(cc,"%d",VTSData.IntervalData.HaltInterval/60);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URS"))
					Ql_sprintf(cc,"%d",VTSData.IntervalData.SleepInterval/60);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URE"))
					Ql_sprintf(cc,"%d",VTSData.IntervalData.EnergencyInterval);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URF"))
					Ql_sprintf(cc,"%d",VTSData.IntervalData.FullDataPacketInterval/60);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URH"))
					Ql_sprintf(cc,"%d",VTSData.IntervalData.HealthInterval/60);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"UR"))
					Ql_sprintf(cc,"%d",VTSData.IntervalData.MotionInterval);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"VID"))
					strcpy(cc,VTSData.VendorID);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"FV"))
					strcpy(cc,FirmVer);
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"GFR"))
					Ql_strcpy(cc, VTSData.DisableGPSFaultReset ? "OFF" : "ON");
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"GF"))
					GetActiveGeoID(cc);
				else
					strcpy(cc,"InvalidKey");

				Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,cc);
				if(i != OTAValue.TotalOTA-1)
					InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,',');

			}
			VAlert[CONF_CHANGE_ALERT].WithACK=1;
			InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,'*');
			if(isserver==0)
				SendSMS(SMSSender,VAlert[CONF_CHANGE_ALERT].ACK);
			else if(isserver==OTA_SRC_RS232)
				SendRS232Response(VAlert[CONF_CHANGE_ALERT].ACK);
			else if(isserver==OTA_SRC_RS485)
				SendRS485Response(VAlert[CONF_CHANGE_ALERT].ACK);
			else
			{
				AddAlert(CONF_CHANGE_ALERT);
				IsPacketReady.IsNormalPacket=1;
			}
		}
	}
	fn=Ql_strstr(buff,"CLR");
	if(fn)
	{
		LOGData(TAG_SERVER,"ota clr cmd");
		ls=fn;
		UpdateOTA(ls,0);
		if(OTAValue.TotalOTA>0)
		{
			LOGData(TAG_SERVER,"ota count : %d",OTAValue.TotalOTA);
			VAlert[CONF_CHANGE_ALERT].Enable=1;
			strcpy(VAlert[CONF_CHANGE_ALERT].Header,"ACK");
			for(i=0;i<OTAValue.TotalOTA;i++)
			{
				Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,OTAValue.OTData[i].KeyVal);
				InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,':');
				char cc[28]={0};

				if(Ql_strstr(OTAValue.OTData[i].KeyVal,"PU")){
					UpdateURL(DEFAULT_IP1);Ql_sprintf(cc,"%s:%s",VTSData.ServerData.IP1,VTSData.ServerData.Port1);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"SU")){
					UpdateSecondaryURL(DEFAULT_IP3);Ql_sprintf(cc,"%s:%s",VTSData.ServerData.IP3,VTSData.ServerData.Port3);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"VN")){
					UpdateVehicleNumber(DEFAULT_VEHREG);strcpy(cc,VTSData.VehicleData.VehicleRegNo);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M0")){
					UpdateMoblieNo(DEFAULT_MOB0,1);strcpy(cc,VTSData.PhoneNumber.Mob0);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M1")){
					UpdateMoblieNo(DEFAULT_MOB1,2);strcpy(cc,VTSData.PhoneNumber.Mob1);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M2")){
					UpdateMoblieNo("0000000000",3);strcpy(cc,VTSData.PhoneNumber.Mob2);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"M3")){
					UpdateMoblieNo("0000000000",4);strcpy(cc,VTSData.PhoneNumber.Mob3);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"OM")){
					UpdateMoblieNo("0000000000",5);strcpy(cc,VTSData.PhoneNumber.Mob4);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"ED")){
					SetSOSTimeOutSeconds(DEFAULT_INV_STM);Ql_sprintf(cc,"%d",VTSData.IntervalData.SOSTimeOut);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"ST")){
					VTSData.IntervalData.SleepTime=DEFAULT_SLEEP_TIME;Ql_sprintf(cc,"%d",VTSData.IntervalData.SleepTime / 60);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"HT")){
					VTSData.IntervalData.HaltInterval=DEFAULT_INV_HALT;Ql_sprintf(cc,"%d",VTSData.IntervalData.HaltInterval / 60);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"DSL")){
					VTSData.VehicleData.DefaultSpeed=DEFAULT_SPEED;Ql_sprintf(cc,"%2.0f",VTSData.VehicleData.DefaultSpeed);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"SL")){
					VTSData.VehicleData.OverSpeed=DEFAULT_OVERSPEED;Ql_sprintf(cc,"%2.0f",VTSData.VehicleData.OverSpeed);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"HBT")){
					VTSData.VehicleData.HarshBreak=DEFAULT_HB;Ql_sprintf(cc,"%d",VTSData.VehicleData.HarshBreak);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"HAT")){
					VTSData.VehicleData.HarshAcc=DEFAULT_HA;Ql_sprintf(cc,"%d",VTSData.VehicleData.HarshAcc);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"RTT")){
					VTSData.VehicleData.RashTurn=DEFAULT_RT;Ql_sprintf(cc,"%d",VTSData.VehicleData.RashTurn);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"LBT")){
					VTSData.BattThrs=LOW_BAT_THRS_VOLT;Ql_sprintf(cc,"%i",batteryVoltageToPercentage(VTSData.BattThrs));}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"TA")){
					VTSData.VehicleData.TiltAngle=DEFAULT_TL;Ql_sprintf(cc,"%d",VTSData.VehicleData.TiltAngle);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URT")){
					VTSData.IntervalData.HaltInterval=DEFAULT_INV_HALT;Ql_sprintf(cc,"%d",VTSData.IntervalData.HaltInterval/60);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URS")){
					VTSData.IntervalData.SleepInterval=DEFAULT_INV_SLEEP;Ql_sprintf(cc,"%d",VTSData.IntervalData.SleepInterval/60);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URE")){
					VTSData.IntervalData.EnergencyInterval=DEFAULT_INV_CRIT;Ql_sprintf(cc,"%d",VTSData.IntervalData.EnergencyInterval);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URF")){
					VTSData.IntervalData.FullDataPacketInterval=DEFAULT_INV_FULL;Ql_sprintf(cc,"%d",VTSData.IntervalData.FullDataPacketInterval/60);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"URH")){
					VTSData.IntervalData.HealthInterval=DEFAULT_INV_HEALTH;Ql_sprintf(cc,"%d",VTSData.IntervalData.HealthInterval/60);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"UR")){
					VTSData.IntervalData.MotionInterval=DEFAULT_INV_MOTION;Ql_sprintf(cc,"%d",VTSData.IntervalData.MotionInterval);}
				else if(Ql_strstr(OTAValue.OTData[i].KeyVal,"VID")){
					strcpy(VTSData.VendorID,DEFAULT_VENDOR);strcpy(cc,VTSData.VendorID);}
				else
					strcpy(cc,"InvalidKey");

				Ql_strcat(VAlert[CONF_CHANGE_ALERT].ACK,cc);
				if(i != OTAValue.TotalOTA-1)
					InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,',');

			}
			VAlert[CONF_CHANGE_ALERT].WithACK=1;
			InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,'*');
			if(isserver==0)
				SendSMS(SMSSender,VAlert[CONF_CHANGE_ALERT].ACK);
			else if(isserver==OTA_SRC_RS232)
				SendRS232Response(VAlert[CONF_CHANGE_ALERT].ACK);
			else if(isserver==OTA_SRC_RS485)
				SendRS485Response(VAlert[CONF_CHANGE_ALERT].ACK);
			else
			{
				AddAlert(CONF_CHANGE_ALERT);
				IsPacketReady.IsNormalPacket=1;
			}
			UpdateConfigInFlash();
		}
		
	}
}


#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_NIC1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void MakeParamChangeStringNIC(char* Sender, char* param, uint8_t IsServer)
#else
void MakeParamChangeString(char* Sender, char* param, uint8_t IsServer)
#endif
{
	// Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	// Ql_sprintf(dataBuffer,"$,PC,12,%s,%d,%s,",NetWork.IMEI,IsServer,Sender);
	// InsertCurrentDateTime(dataBuffer,0);
	// InsertChar(dataBuffer,',');
	// InsertCurrentDateTime(dataBuffer,1);
	// InsertChar(dataBuffer,',');
	// Ql_strcat(dataBuffer,param);
	// Ql_strcat(dataBuffer,",*");
	
	char ss[20];
	uint16_t i;
	uint32_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	Ql_memset(dataBuffer,0,DATA_MAX_BUFF);
	Ql_sprintf(dataBuffer,"$PVT,%s,",VTSData.VendorID);
	Ql_strcat(dataBuffer,FirmVer);
	#ifdef NIC_BIHAR
	Ql_strcat(dataBuffer,",PC,12,L,");
	#else
	Ql_strcat(dataBuffer,",OT,12,L,"); // OTA 
	#endif
	
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	if(GPS.GPSFix)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,sSpeed,7,1,"000.0");
	InsertChar(dataBuffer,',');
	//InsertFloatValue(dataBuffer,GPS.Heading,"%06.2f");
	AppendVariableString(dataBuffer,sHeading,7,1,"000.0");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GPS.NoOfSatalite,"%02d");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sAltitude,7,1,"000.0");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sPDOP,5,1,"00.0");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sHDOP,5,1,"00.0");
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,NetWork.Network);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IsMain + '0');
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.MainsVolt,"%2.1f");
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.BattVolt,"%1.1f");
	InsertChar(dataBuffer,',');
	if(VAlert[SOS_ON_ALERT].Enable || VAlert[SOS_OFF_ALERT].Enable)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');

	if(PeriPheralVal.IsCoverOpen)
		InsertChar(dataBuffer,'O');
	else
		InsertChar(dataBuffer,'C');
	InsertChar(dataBuffer,',');

	


	InsertIntValue(dataBuffer,GSM.SignalStrength,"%2d");
	InsertChar(dataBuffer,',');
	
	InsertIntValue(dataBuffer,GSM.MCC,"%02d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GSM.MNC,"%02d");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.LAC,5,4,"00D6");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.CellID,5,4,"CFBD");
	InsertChar(dataBuffer,',');

	for(i=0;i<4;i++)
	{
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellID,5,1,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].LAC,6,2,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellDB,4,1,"0");
		InsertChar(dataBuffer,',');
		
	}
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(INPUT_SOS_VAL)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,FrameNumber,"%06d");
	InsertChar(dataBuffer,',');

	// InsertFloatValue(dataBuffer,PeriPheralVal.AN1,"%2.1f");
	// InsertChar(dataBuffer,',');
	// InsertFloatValue(dataBuffer,PeriPheralVal.AN2,"%2.1f");
	// InsertChar(dataBuffer,',');

	// InsertIntValue(dataBuffer,DeltaDis,"%02d");
	// InsertChar(dataBuffer,',');
	
	#ifndef NO_PARAM
	InsertChar(dataBuffer,'(');
	Ql_strcat(dataBuffer,Sender);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,param);
	Ql_strcat(dataBuffer,"),");
	#endif
	
	crc = chksum((uint8_t*)dataBuffer,Ql_strlen(dataBuffer));
	Ql_sprintf(ss,"%04X*\n",crc);
	lastcrc= crc;
	Ql_strcat(dataBuffer,ss);

	Ql_strcat(dataBuffer,param);
	FrameNumber++;

}
#endif

#if defined(ENABLE_UNIFIED_FIRMWARE) || defined(PROTO_ODISA1)
#ifdef ENABLE_UNIFIED_FIRMWARE
void MakeParamChangeStringOD(char* Sender, char* param, uint8_t IsServer)
#else
#if !defined(PROTO_MAHARASHTRA1) && !defined(PROTO_NIC1) && !defined(PROTO_CDAC) && !defined(PROTO_OG)
void MakeParamChangeString(char* Sender, char* param, uint8_t IsServer)
#endif
#endif
#if defined(ENABLE_UNIFIED_FIRMWARE) || (!defined(PROTO_MAHARASHTRA1) && !defined(PROTO_NIC1) && !defined(PROTO_CDAC) && !defined(PROTO_OG))
{
	// Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	// Ql_sprintf(dataBuffer,"$,PC,12,%s,%d,%s,",NetWork.IMEI,IsServer,Sender);
	// InsertCurrentDateTime(dataBuffer,0);
	// InsertChar(dataBuffer,',');
	// InsertCurrentDateTime(dataBuffer,1);
	// InsertChar(dataBuffer,',');
	// Ql_strcat(dataBuffer,param);
	// Ql_strcat(dataBuffer,",*");
	
	char ss[20];
	uint16_t i;
	uint32_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	Ql_memset(dataBuffer,0,DATA_MAX_BUFF);
	Ql_sprintf(dataBuffer,"$PVT,%s,",VTSData.VendorID);
	Ql_strcat(dataBuffer,FirmVer);
	
	Ql_strcat(dataBuffer,",CFG,12,L,"); // OTA 

	
	Ql_strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	if(GPS.GPSFix)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,sSpeed,7,1,"000.0");
	InsertChar(dataBuffer,',');
	//InsertFloatValue(dataBuffer,GPS.Heading,"%06.2f");
	AppendVariableString(dataBuffer,sHeading,7,1,"000.0");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GPS.NoOfSatalite,"%02d");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sAltitude,7,1,"000.0");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sPDOP,5,1,"00.0");
	InsertChar(dataBuffer,',');
	
	AppendVariableString(dataBuffer,sHDOP,5,1,"00.0");
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,NetWork.Network);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IsMain + '0');
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.MainsVolt,"%2.1f");
	InsertChar(dataBuffer,',');
	InsertFloatValue(dataBuffer,PeriPheralVal.BattVolt,"%1.1f");
	InsertChar(dataBuffer,',');
	if(VAlert[SOS_ON_ALERT].Enable || VAlert[SOS_OFF_ALERT].Enable)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');

	if(PeriPheralVal.IsCoverOpen)
		InsertChar(dataBuffer,'O');
	else
		InsertChar(dataBuffer,'C');
	InsertChar(dataBuffer,',');

	


	InsertIntValue(dataBuffer,GSM.SignalStrength,"%2d");
	InsertChar(dataBuffer,',');
	
	InsertIntValue(dataBuffer,GSM.MCC,"%02d");
	InsertChar(dataBuffer,',');
	InsertIntValue(dataBuffer,GSM.MNC,"%02d");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.LAC,5,4,"00D6");
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,GSM.CellID,5,4,"CFBD");
	InsertChar(dataBuffer,',');

	for(i=0;i<4;i++)
	{
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellID,5,1,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].LAC,6,2,"0");
		InsertChar(dataBuffer,',');
		AppendVariableString(dataBuffer,GSM.NeigbourCell[i].CellDB,4,1,"0");
		InsertChar(dataBuffer,',');
		
	}
	InsertChar(dataBuffer,PeriPheralVal.IP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IP2 + '0');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	if(INPUT_SOS_VAL)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,FrameNumber,"%06d");
	InsertChar(dataBuffer,',');

	// InsertFloatValue(dataBuffer,PeriPheralVal.AN1,"%2.1f");
	// InsertChar(dataBuffer,',');
	// InsertFloatValue(dataBuffer,PeriPheralVal.AN2,"%2.1f");
	// InsertChar(dataBuffer,',');

	// InsertIntValue(dataBuffer,DeltaDis,"%02d");
	// InsertChar(dataBuffer,',');
	
	crc = CRC16(dataBuffer,Ql_strlen(dataBuffer));
	Ql_sprintf(ss,"%04X*",crc);
	Ql_strcat(dataBuffer,ss);

	// crc = checksum32(dataBuffer,Ql_strlen(dataBuffer));
	// Ql_sprintf(ss,"%08X*",crc);
	// lastcrc= crc;
	// Ql_strcat(dataBuffer,ss);

	#ifndef ODISA_LD
    InsertChar(dataBuffer,'\n');
    #endif
	FrameNumber++;

}

void MakeShortPCString(char* Sender, char* param, uint8_t IsServer)
{
	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	Ql_sprintf(dataBuffer,"$,PC,12,%s,%d,%s,",NetWork.IMEI,IsServer,Sender);
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
	InsertChar(dataBuffer,',');
	Ql_strcat(dataBuffer,param);
	Ql_strcat(dataBuffer,",*");
}
#endif
#endif

#ifdef ENABLE_UNIFIED_FIRMWARE
void MakeParamChangeString(char* Sender, char* param, uint8_t IsServer)
{
	if (IS_PROTO_NIC()) {
		MakeParamChangeStringNIC(Sender, param, IsServer);
	} else if (IS_PROTO_MH()) {
		MakeParamChangeStringMH(Sender, param, IsServer);
	} else if (IS_PROTO_ODISHA()) {
		MakeParamChangeStringOD(Sender, param, IsServer);
	} else if (IS_PROTO_OG()) {
		MakeParamChangeStringOG(Sender, param, IsServer);
	}
}
#endif

void SendResponce(char *Sender, char* Resp, uint8_t IsServer, uint8_t IsSET)
{
#if defined(ENABLE_UNIFIED_FIRMWARE)
    // Handle SMS response
    if(IsServer==OTA_SRC_SMS) {
        if (IS_PROTO_MH() || IS_PROTO_OG() || IsSRCMD) {
            char combinedResp[300];
            if(CMD_Buff[0] != '\0')
                Ql_sprintf(combinedResp,"%s-%s",Resp,CMD_Buff);
            else
                Ql_sprintf(combinedResp,"%s",Resp);
            SendSMS(Sender,combinedResp);
        } else {
            SendSMS(Sender,Resp);
        }
    }

    // Handle SET commands - send to all servers
    if(IsSET) {
        if(IsSMS) {
            MakeParamChangeString(Sender,Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_SCK_1) {
            MakeParamChangeString(VTSData.ServerData.IP1,Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_SCK_2) {
            MakeParamChangeString(VTSData.ServerData.IP3,Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_SCK_3) {
            MakeParamChangeString(VTSData.ServerData.IP4,Resp,IsServer); 
        }
        else if(IsServer==OTA_SRC_RS232) {
            SendRS232Response(Resp);  // Send response to RS232 source
            MakeParamChangeString("RS232",Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_RS485) {
            SendRS485Response(Resp);  // Send response to RS485 source
            MakeParamChangeString("RS485",Resp,IsServer);
        }
        else
        {
            BLE_SendReply((uint8_t*)Resp,strlen(Resp));
            MakeParamChangeString("BLE",Resp,IsServer);	
        }
        
        // Send to all connected servers
        TCPSocket_SendString(&ServerSocket[0],dataBuffer);
        TCPSocket_SendString(&ServerSocket[2],dataBuffer);
        #ifdef EXTENDED_IPS
        TCPSocket_SendString(&ServerSocket[3],dataBuffer);
        #endif

        if (IS_PROTO_ODISHA()) {
            if(IsSMS) {
                MakeShortPCString(Sender,Resp,IsServer);
            }
            else if(IsServer==OTA_SRC_SCK_1) {
                MakeShortPCString(VTSData.ServerData.IP1,Resp,IsServer);
            }
            else if(IsServer==OTA_SRC_SCK_2) {
                MakeShortPCString(VTSData.ServerData.IP3,Resp,IsServer);
            }
            else if(IsServer==OTA_SRC_SCK_3) {
                MakeShortPCString(VTSData.ServerData.IP4,Resp,IsServer);
            }
            else if(IsServer==OTA_SRC_RS232) {
                MakeShortPCString("RS232",Resp,IsServer);
            }
            else if(IsServer==OTA_SRC_RS485) {
                MakeShortPCString("RS485",Resp,IsServer);
            }
            
            // Send short PC string to all servers
            TCPSocket_SendString(&ServerSocket[0],dataBuffer);
            TCPSocket_SendString(&ServerSocket[2],dataBuffer);
            #ifdef EXTENDED_IPS
            TCPSocket_SendString(&ServerSocket[3],dataBuffer);
            #endif
        }
        return;
    }

    // Handle non-SET commands - send only to source
    if(IsServer==OTA_SRC_BLE) {
        BLE_SendReply((uint8_t*)Resp,strlen(Resp));
    }
    else if(IsServer==OTA_SRC_SCK_1) {
        MakeParamChangeString(VTSData.ServerData.IP1,Resp,IsServer);
        TCPSocket_SendString(&ServerSocket[0],dataBuffer);
        if (IS_PROTO_ODISHA()) {
            MakeShortPCString(VTSData.ServerData.IP1,Resp,IsServer);
            TCPSocket_SendString(&ServerSocket[0],dataBuffer);
        }
    }
    else if(IsServer==OTA_SRC_SCK_2) {
        MakeParamChangeString(VTSData.ServerData.IP3,Resp,IsServer);
        TCPSocket_SendString(&ServerSocket[2],dataBuffer);
        #ifdef EXTENDED_IPS
        TCPSocket_SendString(&ServerSocket[3],dataBuffer);
        #endif
        if (IS_PROTO_ODISHA()) {
            MakeShortPCString(VTSData.ServerData.IP3,Resp,IsServer);
            TCPSocket_SendString(&ServerSocket[2],dataBuffer);
        }
    }
    else if(IsServer==OTA_SRC_SCK_3) {
        MakeParamChangeString(VTSData.ServerData.IP4,Resp,IsServer);
        #ifdef EXTENDED_IPS
        TCPSocket_SendString(&ServerSocket[3],dataBuffer);
        #endif
        if (IS_PROTO_ODISHA()) {
            MakeShortPCString(VTSData.ServerData.IP4,Resp,IsServer);
            TCPSocket_SendString(&ServerSocket[3],dataBuffer);
        }
    }
    else if(IsServer==OTA_SRC_RS232) {
        SendRS232Response(Resp);
    }
    else if(IsServer==OTA_SRC_RS485) {
        SendRS485Response(Resp);
    }
#else
    #ifndef PROTO_CDAC
    // Handle SMS response
    if(IsServer==OTA_SRC_SMS) {
		#if defined(PROTO_MAHARASHTRA1) || defined(PROTO_OG)
		char combinedResp[300];
		if(CMD_Buff[0] != '\0')
			Ql_sprintf(combinedResp,"%s-%s",Resp,CMD_Buff);
		else
			Ql_sprintf(combinedResp,"%s",Resp);
		SendSMS(Sender,combinedResp);
		#else
        SendSMS(Sender,Resp);
		#endif
    }

    // Handle SET commands - send to all servers
    if(IsSET) {
        if(IsSMS) {
            MakeParamChangeString(Sender,Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_SCK_1) {
            MakeParamChangeString(VTSData.ServerData.IP1,Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_SCK_2) {
            MakeParamChangeString(VTSData.ServerData.IP3,Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_SCK_3) {
            MakeParamChangeString(VTSData.ServerData.IP4,Resp,IsServer); 
        }
        else if(IsServer==OTA_SRC_RS232) {
			SendRS232Response(Resp);  // Send response to RS232 source
            MakeParamChangeString("RS232",Resp,IsServer);
           
        }
        else if(IsServer==OTA_SRC_RS485) {
			SendRS485Response(Resp);  // Send response to RS485 source
            MakeParamChangeString("RS485",Resp,IsServer);
        }
		else
		{
			BLE_SendReply((uint8_t*)Resp,strlen(Resp));
			MakeParamChangeString("BLE",Resp,IsServer);	
		}
        
        // Send to all connected servers
        TCPSocket_SendString(&ServerSocket[0],dataBuffer);
        TCPSocket_SendString(&ServerSocket[2],dataBuffer);
        #ifdef EXTENDED_IPS
        TCPSocket_SendString(&ServerSocket[3],dataBuffer);
        #endif

        #if defined(PROTO_ODISA1)
        if(IsSMS) {
            MakeShortPCString(Sender,Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_SCK_1) {
            MakeShortPCString(VTSData.ServerData.IP1,Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_SCK_2) {
            MakeShortPCString(VTSData.ServerData.IP3,Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_SCK_3) {
            MakeShortPCString(VTSData.ServerData.IP4,Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_RS232) {
            MakeShortPCString("RS232",Resp,IsServer);
        }
        else if(IsServer==OTA_SRC_RS485) {
            MakeShortPCString("RS485",Resp,IsServer);
        }
        
        // Send short PC string to all servers
        TCPSocket_SendString(&ServerSocket[0],dataBuffer);
        TCPSocket_SendString(&ServerSocket[2],dataBuffer);
        #ifdef EXTENDED_IPS
        TCPSocket_SendString(&ServerSocket[3],dataBuffer);
        #endif
        #endif
        return;
    }

    // Handle non-SET commands - send only to source
    if(IsServer==OTA_SRC_BLE) {
        BLE_SendReply((uint8_t*)Resp,strlen(Resp));
    }
    else if(IsServer==OTA_SRC_SCK_1) {
        MakeParamChangeString(VTSData.ServerData.IP1,Resp,IsServer);
        TCPSocket_SendString(&ServerSocket[0],dataBuffer);
        #if defined(PROTO_ODISA1)
        MakeShortPCString(VTSData.ServerData.IP1,Resp,IsServer);
        TCPSocket_SendString(&ServerSocket[0],dataBuffer);
        #endif
    }
    else if(IsServer==OTA_SRC_SCK_2) {
        MakeParamChangeString(VTSData.ServerData.IP3,Resp,IsServer);
        TCPSocket_SendString(&ServerSocket[2],dataBuffer);
        #ifdef EXTENDED_IPS
        TCPSocket_SendString(&ServerSocket[3],dataBuffer);
        #endif
        #if defined(PROTO_ODISA1)
        MakeShortPCString(VTSData.ServerData.IP3,Resp,IsServer);
        TCPSocket_SendString(&ServerSocket[2],dataBuffer);
        #endif
    }
    else if(IsServer==OTA_SRC_SCK_3) {
        MakeParamChangeString(VTSData.ServerData.IP4,Resp,IsServer);
        #ifdef EXTENDED_IPS
        TCPSocket_SendString(&ServerSocket[3],dataBuffer);
        #endif
        #if defined(PROTO_ODISA1)
        MakeShortPCString(VTSData.ServerData.IP4,Resp,IsServer);
        TCPSocket_SendString(&ServerSocket[3],dataBuffer);
        #endif
    }
    else if(IsServer==OTA_SRC_RS232) {
        SendRS232Response(Resp);
    }
    else if(IsServer==OTA_SRC_RS485) {
        SendRS485Response(Resp);
    }

    #else
    /* PROTO_CDAC response routing — must mirror every source that calls
     * DecodeSMS() with a non-SMS origin so X-commands get a reply. */
    if(IsServer==OTA_SRC_SMS)
        SendSMS(Sender,Resp);
    else if(IsServer==OTA_SRC_RS232)
        SendRS232Response(Resp);
    else if(IsServer==OTA_SRC_RS485)
        SendRS485Response(Resp);
    #endif
#endif
}
#if defined(ENABLE_UNIFIED_FIRMWARE) || !defined(PROTO_CDAC)
void GetCurrentInterval(void)
{
	if(ServerSocket[0].SocketState != SOCKET_CONNECTED)
		VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.StandbyInterval;

	if(VAlert[SOS_ON_ALERT].Enable)
	{
		if(VTSData.IntervalData.CurrentInterval != VTSData.IntervalData.SOSInterval)
			VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.SOSInterval;
	}
	else if(PeriPheralVal.IGN)
	{
		if(VTSData.IntervalData.CurrentInterval != VTSData.IntervalData.IgnitionInterval)
			VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.IgnitionInterval;
	}
	else if(!PeriPheralVal.IsMain)
	{
		if(VTSData.IntervalData.CurrentInterval != VTSData.IntervalData.StandbyInterval)
			VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.StandbyInterval;
	}
	else
		VTSData.IntervalData.CurrentInterval = VTSData.IntervalData.DataInterval;
}
void SendDatatoServer0(void)
{
	if(!TCPSocket_SendString(&ServerSocket[0],dataBuffer))
	{
		#ifndef HISTORY_DISABLED
		#ifdef HISTORY_INTERNAL
		SavePacket();
		#else
		WriteHistoryData(dataBuffer);
		#endif
		#endif

	}
	TCPSocket_SendString(&ServerSocket[2],dataBuffer);
	#ifdef EXTENDED_IPS
	TCPSocket_SendString(&ServerSocket[3],dataBuffer);
	#endif
}

// void SendSensorData(void)
// {
// 	if(DHT11.Status==0 && IsFuelData==0)
// 		return;
// 	SensorString();
	
// 	#ifdef EXTENDED_IPS
// 	TCPSocket_SendString(&ServerSocket[3],dataBuffer);
// 	#else
// 	TCPSocket_SendString(&ServerSocket[2],dataBuffer);
// 	#endif
// }

void MakeSMSFallbackPacket(void)
{
	Ql_sprintf(dataBuffer,"SOSFB,%s\nLat:%s,%c\nLng:%s,%c,fix:%d, Speed:%s\nCID:%s,LAC:%s\n",NetWork.IMEI,sLatitude,GPS.LatDir,sLongitude,GPS.LngDir,GPS.GPSFix,sSpeed,GSM.CellID,GSM.LAC);
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
}



void CheckAlerts(void)
{
	if(VAlert[SOS_ON_ALERT].Enable)
	{
		if(IsEMRSend==0)
		{
			InitBuffer(10);
			IsEMRSend = 1;
			#ifdef ENABLE_RS232_PRINT
			//SendRS232String("Sending SOS ON Packet\n");
			#endif
			TCPSocket_SendString(&ServerSocket[0],dataBuffer);
			TCPSocket_SendString(&ServerSocket[2],dataBuffer);
			#ifdef EXTENDED_IPS
			TCPSocket_SendString(&ServerSocket[3],dataBuffer);
			#endif
			return;
		}
		
	}
	if(VAlert[SOS_OFF_ALERT].Enable)
	{
		VAlert[SOS_OFF_ALERT].Enable=0; //SOS OFF ALERT
		EmergencyPacket(0);
		/* OLD CODE - COMMENTED OUT AS REQUESTED:
		TCPSocket_SendString(&ServerSocket[1],dataBuffer);
		*/
		// NEW CODE: Dynamic routing based on Server 2 state
		if (VTSData.ServerData.IP2[0] == 'N' && VTSData.ServerData.IP2[1] == 'A') {
			TCPSocket_SendString(&ServerSocket[0],dataBuffer); // Send EPB to Server 1
		} else {
			TCPSocket_SendString(&ServerSocket[1],dataBuffer); // Send EPB to Server 2
		}
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		InitBuffer(11);
		IsEMRSend = 0;
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending SOS OFF Packet\n");
		#endif
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		return;
	}

	if(VAlert[SOS_TMP_ALERT].Enable)
	{
		if(IsEMRTSend==0)  // SOS Temper
		{
			InitBuffer(16);
			IsEMRTSend = 1;
			TCPSocket_SendString(&ServerSocket[0],dataBuffer);
			TCPSocket_SendString(&ServerSocket[2],dataBuffer);
			#ifdef EXTENDED_IPS
			TCPSocket_SendString(&ServerSocket[3],dataBuffer);
			#endif
			#ifdef ENABLE_RS232_PRINT
			//SendRS232String("Sending SOS Tamper Packet\n");
			#endif
			return;
		}
	}
	else if(IsEMRTSend)
		IsEMRTSend=0;

	if(VAlert[MAINS_FAIL_ALERT].Enable) // MAIN OFF 
	{
		InitBuffer(3);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Mains Fail Packet\n");
		#endif
		VAlert[MAINS_FAIL_ALERT].Enable=0;
		return;
	}

	if(VAlert[TILT_ALERT].Enable) // TILT 
	{
		InitBuffer(24);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Tilt Alert Packet\n");
		#endif
		VAlert[TILT_ALERT].Enable=0;	
		return;
	}

	if(VAlert[TAMPER_ALERT].Enable)  // BOX TAMPER
	{
		InitBuffer(9);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Body Tamper Packet\n");
		#endif
		VAlert[TAMPER_ALERT].Enable=0;	
		return;
	}

	if(VAlert[OVER_SPEED_ALERT].Enable) //Over Speed
	{
		if(!IsOverSpeed)
		{
			InitBuffer(23);
			TCPSocket_SendString(&ServerSocket[0],dataBuffer);
			TCPSocket_SendString(&ServerSocket[2],dataBuffer);
			#ifdef EXTENDED_IPS
			TCPSocket_SendString(&ServerSocket[3],dataBuffer);
			#endif
			#ifdef ENABLE_RS232_PRINT
			//SendRS232String("Sending Over Speed Packet\n");
			#endif
			IsOverSpeed=1;
			return;
		}
	}
	else if(IsOverSpeed)
		IsOverSpeed=0;

	if(VAlert[HARSH_BRK_ALERT].Enable) // Harsh Braking
	{
		InitBuffer(13);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Harsh Brake Packet\n");
		#endif
		VAlert[HARSH_BRK_ALERT].Enable=0;	
		return;
	}
	
	if(VAlert[HARSH_ACC_ALERT].Enable) // Harsh Accel
	{
		InitBuffer(14);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Harsh Acceleration Packet\n");
		#endif
		VAlert[HARSH_ACC_ALERT].Enable=0;	
		return;
	}

	if(VAlert[RASH_TURN_ALERT].Enable) // Rash Turn
	{
		InitBuffer(15);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Rash Turn Packet\n");
		#endif
		VAlert[RASH_TURN_ALERT].Enable=0;	
		return;
	}


	if(VAlert[MAINS_RES_ALERT].Enable) // Mains Restore
	{
		InitBuffer(6);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Mains Restore Packet\n");
		#endif
		VAlert[MAINS_RES_ALERT].Enable=0;	
		return;
	}

	if(VAlert[BATT_LOW_ALERT].Enable) // Battery Low 
	{
		InitBuffer(4);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Low Battery Packet\n");
		#endif
		VAlert[BATT_LOW_ALERT].Enable=0;	
		return;
	}

	if(VAlert[IGN_ON_ALERT].Enable) // IGNITION ON
	{
		InitBuffer(7);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Ignition ON Packet\n");
		#endif
		VAlert[IGN_ON_ALERT].Enable=0;	
		return;
	}

	if(VAlert[IGN_OFF_ALERT].Enable) // IGNITION OFF
	{
		InitBuffer(8);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Ignition OFF Packet\n");
		#endif
		VAlert[IGN_OFF_ALERT].Enable=0;	
		return;
	}
	if(VAlert[BATT_LOW_RES_ALERT].Enable) // Battery Low Restore
	{
		InitBuffer(5);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Battery Recharged Packet\n");
		#endif
		VAlert[BATT_LOW_RES_ALERT].Enable=0;	
		return;
	}
	if(VAlert[GFIN_ALERT].IsSMS)
	{
		InitBuffer(17);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		VAlert[GFIN_ALERT].IsSMS=0;
	}
	if(VAlert[GFOUT_ALERT].IsSMS)
	{
		InitBuffer(18);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendString(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		VAlert[GFOUT_ALERT].IsSMS=0;
	}

}

void ChangeToHistoryPacket(char *buf)
{
	char *fn;
	fn = Ql_strstr(buf,",NR,01");
	if(!fn)
	{
		// Try alternate format with single digit
		fn = Ql_strstr(buf,",NR,1");
		if(!fn)
			return;
		// For ",NR,1", position is different (fn[4] instead of fn[5])
		fn[4] = '2';
	}
	else
	{
		// For ",NR,01"
		fn[5] = '2';
	}
	
	fn = Ql_strstr(buf,",L,");
	if(!fn)
		return;
	fn[1] = 'H';
	LOGData(TAG_SERVER,"Changed to History Packet, Len :%d",Ql_strlen(buf));
	return;
}

void ChangeToHistoryEPB(char *buf)
{
	char *fn;
	fn = Ql_strstr(buf,",NM,");
	if(!fn)
		return;

	fn[1] = 'S';
	fn[2] = 'P';
}

uint16_t GetMemeryPercentage(void)
{
	uint16_t count;

	#ifdef HISTORY_DISABLED
	return 33;
	#else

	#ifdef HISTORY_INTERNAL
	CheckPacketCount();
	count = PacketConfig.LastPkt;
	#else
	if(!GetHistoryCount(&count))
	{
		LOGData(TAG_SERVER,"\r\nUnable to get History Count");
		return 0;
	}
	#endif
	StoredHistoryDataCount = count;
	MemoryPercent = ((int)count*100)/4000;
	if(count <= 0)
		return 0;

	return MemoryPercent;
	#endif
}

void ProcessHistoryPacket(void)
{
	int size;
	if(ServerSocket[0].SocketState != SOCKET_CONNECTED)
		return;

	#ifdef HISTORY_DISABLED
	return;

	#else

	// if(ProcessFTK(&FTKConfig))
	// {
	// 	LOGData(TAG_SERVER,"R History Skipped for FTK is enabled");
	// 	return;
	// }
	uint16_t count;

	#ifdef HISTORY_INTERNAL
	CheckPacketCount();
	count=  PacketConfig.LastPkt;
	#else
		
	// if(packetConfig.count<=0)
	// 	return;
	
	if(!GetHistoryCount(&count))
	{
		LOGData(TAG_SERVER,"\r\nUnable to get History Count");
		return;
	}
	#endif
	StoredHistoryDataCount = count;
	MemoryPercent = ((int)count/8000)*100;
	if(count <= 0)
		return;
	

	Ql_memset(dataBuffer,0x00,DATA_MAX_BUFF);
	LOGData(TAG_SERVER,"\r\nFound History Packets : %d, Reading Last...",count);

	#ifdef HISTORY_INTERNAL
	ReadLastPacket();
	#else
	if(!GetHistoryData(dataBuffer))
	{
		LOGData(TAG_SERVER,"\r\nUnable to get read history data");
		return;
	}
	#endif
	size = Ql_strlen(dataBuffer);
	LOGData(TAG_SERVER,"\r\nHistorty Packet Read Len: %d ",size);
	if(size > 256)
	{
		LOGData(TAG_SERVER,"\r\nERROR History Packet Size > 256!!!!!");
		#ifdef HISTORY_INTERNAL
		DeleteLastPacket();
		#else
		DeleteHistoryData();
		#endif
		return;
	}
	if (size == 0) {
		LOGData(TAG_SERVER,"\r\nERROR History Packet size 0!");
		#ifdef HISTORY_INTERNAL
		DeleteLastPacket();
		#else
		DeleteHistoryData();
		#endif
		return;
	}
	#ifndef PROTO_OG
	if(Ql_strstr(dataBuffer,"$EPB"))
	{
		/* OLD CODE - COMMENTED OUT AS REQUESTED:
		if(ServerSocket[1].SocketState >= SOCKET_CONNECTED)
		{
			LOGData(TAG_SERVER,"\r\nSending History EMG Packet...");
			#ifdef ENABLE_RS232_PRINT
			//SendRS232String("Sending History EMG Packet\n");
			#endif
			ChangeToHistoryEPB(dataBuffer);
			if(TCPSocket_SendString(&ServerSocket[1],dataBuffer))
			{
				#ifdef HISTORY_INTERNAL
				DeleteLastPacket();
				#else
				DeleteHistoryData();
				#endif
			}
		}
		else
			LOGData(TAG_SERVER,"\r\nEMG Server not Connected");
		*/
		// NEW CODE: Dynamic routing for history emergency packets based on Server 2 state
		uint8_t isServer2Disabled = (VTSData.ServerData.IP2[0] == 'N' && VTSData.ServerData.IP2[1] == 'A');
		uint8_t socketToCheck = isServer2Disabled ? 0 : 1;
		if(ServerSocket[socketToCheck].SocketState >= SOCKET_CONNECTED)
		{
			LOGData(TAG_SERVER,"\r\nSending History EMG Packet to Server %d...", socketToCheck + 1);
			ChangeToHistoryEPB(dataBuffer);
			if(TCPSocket_SendString(&ServerSocket[socketToCheck],dataBuffer))
			{
				#ifdef HISTORY_INTERNAL
				DeleteLastPacket();
				#else
				DeleteHistoryData();
				#endif
			}
		}
		else
			LOGData(TAG_SERVER,"\r\nHistory EMG Socket %d not Connected", socketToCheck + 1);
		return;
	}
	#else
	if(Ql_strstr(dataBuffer,"$,EPB"))
	{
		/* OLD CODE - COMMENTED OUT AS REQUESTED:
		if(ServerSocket[1].SocketState >= SOCKET_CONNECTED)
		{
			LOGData(TAG_SERVER,"\r\nSending History EMG Packet...");
			#ifdef ENABLE_RS232_PRINT
			//SendRS232String("Sending History EMG Packet\n");
			#endif
			ChangeToHistoryEPB(dataBuffer);
			if(TCPSocket_SendString(&ServerSocket[1],dataBuffer))
			{
				#ifdef HISTORY_INTERNAL
				DeleteLastPacket();
				#else
				DeleteHistoryData();
				#endif
			}
		}
		else
			LOGData(TAG_SERVER,"\r\nEMG Server not Connected");
		*/
		// NEW CODE: Dynamic routing for history emergency packets based on Server 2 state
		uint8_t isServer2Disabled = (VTSData.ServerData.IP2[0] == 'N' && VTSData.ServerData.IP2[1] == 'A');
		uint8_t socketToCheck = isServer2Disabled ? 0 : 1;
		if(ServerSocket[socketToCheck].SocketState >= SOCKET_CONNECTED)
		{
			LOGData(TAG_SERVER,"\r\nSending History EMG Packet to Server %d...", socketToCheck + 1);
			ChangeToHistoryEPB(dataBuffer);
			if(TCPSocket_SendString(&ServerSocket[socketToCheck],dataBuffer))
			{
				#ifdef HISTORY_INTERNAL
				DeleteLastPacket();
				#else
				DeleteHistoryData();
				#endif
			}
		}
		else
			LOGData(TAG_SERVER,"\r\nHistory EMG Socket %d not Connected", socketToCheck + 1);
		return;
	}
	#endif
	#ifdef PROTO_MAHARASHTRA1
	if(!Ql_strstr(dataBuffer,"$NMP") || Ql_strlen(dataBuffer)<150)
	{
		LOGData(TAG_SERVER,"\r\nInvalid Hitory Packet, deleting...");
		#ifdef HISTORY_INTERNAL
		DeleteLastPacket();
		#else
		DeleteHistoryData();
		#endif
		return;
	}
	#elif defined(PROTO_OG)
	if(!Ql_strstr(dataBuffer,"$,NMP") || Ql_strlen(dataBuffer)<150)
	{
		LOGData(TAG_SERVER,"\r\nInvalid Hitory Packet, deleting...");
		#ifdef HISTORY_INTERNAL
		DeleteLastPacket();
		#else
		DeleteHistoryData();
		#endif
		return;
	}
	#else
	if(!Ql_strstr(dataBuffer,"$PVT") || Ql_strlen(dataBuffer)<150)
	{
		LOGData(TAG_SERVER,"\r\nInvalid Hitory Packet, deleting...");
		#ifdef HISTORY_INTERNAL
		DeleteLastPacket();
		#else
		DeleteHistoryData();
		#endif
		return;
	}
	
	#endif
	ChangeToHistoryPacket(dataBuffer);

	LOGData(TAG_SERVER,"\r\nSending Packet...");
	#ifdef ENABLE_RS232_PRINT
	//SendRS232String("Sending History Packet\n");
	#endif
	if(TCPSocket_SendString(&ServerSocket[0],dataBuffer))
	{
		#ifdef HISTORY_INTERNAL
		DeleteLastPacket();
		#else
		DeleteHistoryData();
		#endif
	}
	TCPSocket_SendString(&ServerSocket[2],dataBuffer);
	#ifdef EXTENDED_IPS
	TCPSocket_SendString(&ServerSocket[3],dataBuffer);
	#endif
	#endif
}
#endif

#if defined(PROTO_CDAC)

uint8_t GetBatchData(void)
{
	uint16_t tf,resp;
	uint8_t cc=0,nc=0;

	/* Skip batch entirely while SOS/tamper emergency is pending.
	 * Batch HTTP POST blocks the server thread for up to 50s (10s timeout ×
	 * 5 retries after Fix C).  If SOS fires during that block the EPB10 cannot
	 * be processed until the POST returns, and SOS may time out first.
	 * Deferring batch until after the emergency clears costs at most one
	 * normal-interval delay and guarantees the EPB gets through. */
	if(SOS.IsSOS || SOS.IsSOSTamper || VAlert[SOS_OFF_ALERT].Enable) {
		LOGData(TAG_SERVER, "GetBatchData: skipping batch — emergency state active (SOS=%d Tamp=%d SOSOff=%d)",
			SOS.IsSOS, SOS.IsSOSTamper, VAlert[SOS_OFF_ALERT].Enable);
		return 0;
	}

	tf=ReadFileTable();
	char temp[DATA_MAX_BUFF];
	if((tf > 0 ) && (tf <= MAX_FILE))
	{
		if(tf > 2)
			tf = 2;
		LOGData(TAG_SERVER,"Making Batch Packet for %d packets",tf);
		MakeBatchPacket(tf,&nc,&cc);
		LOGData(TAG_SERVER,"Batch Packet ready");
		print_long_string(SendString);
		resp=SendDataToServer(SendString,IsPacketReady.IsCriticalPacket 
								|| IsPacketReady.IsHealthPacket  || IsPacketReady.IsFullPacket,
								VTSData.IntervalData.CurrentInterval);
		if(resp)
		{
			int count;
			for(count = 0; count < s_batchPacketsCount; count++)
			{
				if (s_batchPacketsSlots[count] >= 0)
				{
					DeleteDataBatchSlot(s_batchPacketsSlots[count]);
				}
			}
			s_batchPacketsCount = 0;
			s_batchPacketsSlots[0] = -1;
			s_batchPacketsSlots[1] = -1;

			return 1;	
		}
		return 0;
	}
	return 0;
}


#if defined(PROTO_CDAC)
static uint8_t server1_offline = 0;
static uint8_t server1_fail_count = 0;
static uint32_t server1_offline_time = 0;
#define SERVER1_COOLDOWN_MS 300000 // 5 minutes
#define SERVER1_MAX_FAILURES 2
#endif

#if defined(PROTO_CDAC)
uint8_t SendDataToServer(char* data, uint8_t KeepAlive, uint16_t currentIntervalSec)
#else
uint8_t SendDataToServer(char* data, uint8_t KeepAlive)
#endif
{
	int ret = 0;
	uint8_t isGood=0;
	uint8_t vlt_good=0;   /* set to 1 if VLT HTTP (Server 3) delivers the packet */
	uint32_t connect_tmout = 3000;
	uint32_t response_tmout = 3000;
	if(GSM.GSMState != GPRS_ACTIVE)
	{
		LOGData(TAG_SERVER, "HTTP send deferred: GPRS not active (state=%d)", GSM.GSMState);
		return ret;
	}
	#ifdef PROTO_CDAC
	if(currentIntervalSec <= 5)
	{
		connect_tmout = 1500;
		response_tmout = 3000;
	}
	else if(currentIntervalSec <= 30)
	{
		connect_tmout = 2000;
		response_tmout = (currentIntervalSec * 1000UL);
		if(response_tmout > 2000)
			response_tmout -= 2000;
		else
			response_tmout = 1000;
	}
	else
	{
		connect_tmout = 3000;
		response_tmout = 30000;
	}
	#endif
	IsSendProcess=1;
	if(HTTPConnectFlag==1)
		LOGData(TAG_SERVER,"HTTP Send connect req flag already set!!!!");
	HTTPConnectFlag=1;

	uint8_t skip_server1 = 0;
	#ifdef PROTO_CDAC
	if(server1_offline)
	{
		if(Ql_GetMsSincePwrOn() - server1_offline_time > SERVER1_COOLDOWN_MS)
		{
			LOGData(TAG_SERVER, "Circuit Breaker: Cooldown elapsed. Probing Server 1 to check connectivity.");
		}
		else
		{
			LOGData(TAG_SERVER, "Circuit Breaker: Server 1 is OFFLINE. Skipping to prevent delay.");
			skip_server1 = 1;
		}
	}
	#endif

	if(!skip_server1)
	{
		/* Single Server 1 attempt only. Retrying 3× would block VLT (Server 3)
		 * for up to 3 × 30 s = 90 s on every send cycle when Server 1 is down.
		 * The caller (HttpQueue_Process / handleCDACProtocol) already has its
		 * own retry loop, so duplicate retries here compound the delay. */
		for(int lp = 0; lp < 1;lp++)
		{
			uint32_t connect_wait_ms = connect_tmout;
			LOGData(TAG_SERVER,"http post attempt %d/3",lp+1);
			if(!HTTP_Setup(ServerSocket[0].DNSorIP, (uint16_t)ServerSocket[0].Port))
			{
				HTTPConnectFlag = 1;
				continue;
			}
			while(ServerSocket[0].SocketState != SOCKET_CONNECTED && connect_wait_ms > 0)
			{
				ThreadSleep(15);
				if(connect_wait_ms > 15)
					connect_wait_ms -= 15;
				else
					connect_wait_ms = 0;
			}
			if(ServerSocket[0].SocketState == SOCKET_CONNECTED)
			{
				uint32_t response_wait_ms = response_tmout;
				isGood=1;
				ThreadSleep(50);
				HTTPConnectFlag=0;
				LOGData(TAG_SERVER,"Device to Server [%d]: ",Ql_strlen(data));
				// Prevent DBG_BUFFER overflow - truncate if data is too long
				if (Ql_strlen(data) > 400) {
					char log_sample[401];
					Ql_memset(log_sample, 0, sizeof(log_sample));
					Ql_strncpy(log_sample, data, 400);
					LOGData(TAG_SERVER,"%s...(truncated)", log_sample);
				} else {
					LOGData(TAG_SERVER,"%s", data);
				}
				IsHTTPRes=0;
				ret = HTTP_Post(KeepAlive,0,data,Ql_strlen(data),0);
				if(!ret)
				{
					HTTPConnectFlag = 1;
					continue;
				}
				#ifdef HTTP_SIMULATE
				while(!ServerSocket[0].isRXData)
				{
					if(ServerSocket[0].SocketState != SOCKET_CONNECTED)
					{
						LOGData(TAG_SERVER,"HTTP Server Premature Disconnection!");
						response_wait_ms =0;
						if(KeepAlive || lp < 2)
							HTTPConnectFlag = 1;
						break;
					}
					if(response_wait_ms <= 30)
					{
						LOGData(TAG_SERVER,"No responce from HTTP Server !");
						response_wait_ms = 0;
						break;
					}
					ThreadSleep(30);
					response_wait_ms -= 30;
				}
				if(!ServerSocket[0].isRXData)
				{
					HTTP_Close(0);
					HTTPConnectFlag=1;
					continue;
				}
				if(response_wait_ms!=0)
				{
					LOGData(TAG_SERVER,"Parsing Server 1 Data...");
					print_long_string((const char*)ServerSocket[0].rxBuffer);
					DecodeOTAData(ServerSocket[0].rxBuffer,1);
					LOGData(TAG_SERVER,"Parsing done");
					Ql_memset(ServerSocket[0].rxBuffer,0,ServerSocket[0].rxSizeMAX);
					ServerSocket[0].isRXData=0;
					break; //all done , dont retry
				}
				#else
				while(!IsHTTPRes)
				{
					if(response_wait_ms <= 30)
					{
						LOGData(TAG_SERVER,"No responce from HTTP Server !");
						response_wait_ms = 0;
						break;
					}
					ThreadSleep(30);
					response_wait_ms -= 30;
					/* Service RS232/RS485 while waiting so commands are not dropped */
					if(RS232_DataAvailable) ProcessRS232OTAData();
					if(RS485_DataAvailable) ProcessRS485OTAData();
				}
				if(response_wait_ms!=0)
				{
					LOGData(TAG_SERVER,"\r\nParsing Server 1 Data...");
					print_long_string((const char*)ServerSocket[0].rxBuffer);
					DecodeOTAData(ServerSocket[0].rxBuffer,1);
					Ql_memset(ServerSocket[0].rxBuffer,0,ServerSocket[0].rxSizeMAX);
					ServerSocket[0].isRXData=0;
					if(!KeepAlive)
						HTTP_Close(0);
					break; /* success — do not retry */
				}
				if(!KeepAlive)
					HTTP_Close(0);
				#endif
			}
			else
			{
				LOGData(TAG_SERVER,"HTTP not connected to send data !");
			}
		}

		#ifdef PROTO_CDAC
		if(isGood && ret)
		{
			if(server1_offline)
			{
				LOGData(TAG_SERVER, "Circuit Breaker: Server 1 is BACK ONLINE.");
			}
			server1_fail_count = 0;
			server1_offline = 0;
		}
		else
		{
			server1_fail_count++;
			if(server1_fail_count >= SERVER1_MAX_FAILURES)
			{
				server1_offline = 1;
				server1_offline_time = Ql_GetMsSincePwrOn();
				LOGData(TAG_SERVER, "Circuit Breaker: Server 1 connection failed consecutively %d times. Marking OFFLINE.", server1_fail_count);
			}
		}
		#endif
	}
	/* Send the same packet to Server 3 via HTTP if it has an http(s):// URL.
	 * This runs regardless of isGood so Server 3 receives data even when
	 * Server 1 (CDAC) is unreachable. */
	if(IsHttpUrl(ServerSocket[2].DNSorIP) && ServerSocket[2].Port > 0)
	{
		LOGData(TAG_SERVER, "VLT HTTP: Sending to %s:%d", ServerSocket[2].DNSorIP, ServerSocket[2].Port);
		/* HTTP_Setup() reuses an existing connected session without switching the URL.
		 * Close any live Server 1 session so HTTP_Setup() opens a fresh connection
		 * to Server 3's URL instead of silently posting to Server 1 again. */
		HTTP_Close(0);
		ServerSocket[0].SocketState = SOCKET_IDLE;
		ThreadSleep(500);
		for(int lp3 = 0; lp3 < 3; lp3++)
		{
			LOGData(TAG_SERVER, "VLT HTTP: attempt %d/3", lp3 + 1);
			if(!HTTP_Setup(ServerSocket[2].DNSorIP, (uint16_t)ServerSocket[2].Port))
			{
				HTTPConnectFlag = 1;
				continue;
			}
			uint32_t vlt_connect_ms = 10000;
			while(ServerSocket[0].SocketState != SOCKET_CONNECTED && vlt_connect_ms > 0)
			{
				ThreadSleep(15);
				vlt_connect_ms = (vlt_connect_ms > 15) ? vlt_connect_ms - 15 : 0;
			}
			if(ServerSocket[0].SocketState == SOCKET_CONNECTED)
			{
				ThreadSleep(50);
				HTTPConnectFlag = 0;
				LOGData(TAG_SERVER, "VLT HTTP: Device to Server [%d]: %s", Ql_strlen(data), data);
				IsHTTPRes = 0;
				int vlt_ret = HTTP_Post(KeepAlive, 0, data, Ql_strlen(data), 0);
				if(!vlt_ret)
				{
					HTTPConnectFlag = 1;
					continue;
				}
				uint32_t vlt_resp_ms = 10000;
				while(!IsHTTPRes)
				{
					if(vlt_resp_ms <= 30)
					{
						LOGData(TAG_SERVER, "VLT HTTP: No response from server!");
						vlt_resp_ms = 0;
						break;
					}
					ThreadSleep(30);
					vlt_resp_ms -= 30;
					/* Service RS232/RS485 while waiting */
					if(RS232_DataAvailable) ProcessRS232OTAData();
					if(RS485_DataAvailable) ProcessRS485OTAData();
				}
				if(vlt_resp_ms != 0)
				{
					vlt_good = 1;
					LOGData(TAG_SERVER, "\r\nParsing Server 3 Data...");
					print_long_string((const char*)ServerSocket[0].rxBuffer);
					DecodeOTAData(ServerSocket[0].rxBuffer, OTA_SRC_SCK_3);
					Ql_memset(ServerSocket[0].rxBuffer, 0, ServerSocket[0].rxSizeMAX);
					ServerSocket[0].isRXData = 0;
					if(!KeepAlive)
						HTTP_Close(0);
					break;
				}
				if(!KeepAlive)
					HTTP_Close(0);
			}
			else
			{
				LOGData(TAG_SERVER, "VLT HTTP: not connected to send data!");
			}
		}
		/* VLT HTTP_Setup() used the single shared M66 QHTTP session, which tears
		 * down any active CDAC keepalive. Force CDAC to reconnect on next packet. */
		HTTPConnectFlag = 1;
		ServerSocket[0].SocketState = SOCKET_IDLE;
	}
	/* Mirror every CDAC packet to Server 3 via TCP when ServerSocket[2] is a raw
	 * TCP endpoint (bare IP:port, no http:// prefix). The VLT HTTP block above
	 * handles the http:// case; this handles the TCP case. */
	else if(ServerSocket[2].isEnabled &&
	        ServerSocket[2].Port > 0 &&
	        ServerSocket[2].SocketState == SOCKET_CONNECTED)
	{
		LOGData(TAG_SERVER, "TCP Mirror: Sending to Server 3 (%s:%d)",
		        ServerSocket[2].DNSorIP, ServerSocket[2].Port);
		TCPSocket_SendString(&ServerSocket[2], data);
	}

	/* Return 1 if EITHER Server 1 (CDAC) OR Server 3 (VLT HTTP) delivered
	 * the packet. Without vlt_good, the HTTP queue would see failure and
	 * re-send a packet that VLT already accepted. */
	if(!isGood && !vlt_good){
		IsSendProcess=0;
		return 0;
	}

	IsSendProcess=0;
	return (isGood && ret) ? 1 : (uint8_t)vlt_good;
}
#endif
uint8_t IsFTPReq;
// Helper functions to break down the main logic
static void handleProfileRequests(void) {
    while(prfReq != NONE) {
        LOGData(TAG_SERVER,"\r\nServer Thread Paused For Profile Update");
        ThreadSleep(500);
    }
}

static void handleFTPRequests(void) {
    if(GSM.GSMState==GPRS_ACTIVE && FTPState == FTP_STATE_CLOSED && IsFTPReq) {
        FTPStart(&DownloadReq);
        IsFTPReq=0;
    }
}

static void handleIncomingMessages(void) {
    // Handle SMS messages
    if(IsSMS) {
        LOGData(TAG_SERVER,"\r\nParsing SNS Data...");
        DecodeSMS(SMSData,OTA_SRC_SMS);
        IsSMS=0;
    }

    // Handle BLE messages
    if(BTRcvBufferLen) {
        LOGData(TAG_SERVER,"\r\nParsing BLE Data...");
        DecodeSMS((char*)BTRcvBuffer,OTA_SRC_BLE);
		BLE_DecodeComplete();
    }

    // Handle RS232 OTA messages
    if(RS232_DataAvailable) {
        LOGData(TAG_SERVER,"\r\nParsing RS232 OTA Data...");
        ProcessRS232OTAData();
    }

    // Handle RS485 OTA messages  
    if(RS485_DataAvailable) {
        LOGData(TAG_SERVER,"\r\nParsing RS485 OTA Data...");
        ProcessRS485OTAData();
    }
}

static void handleRFIDData(void) {
    #ifndef EXTENDED_IPS
    if(RFIDDataCount && ServerSocket[2].SocketState == SOCKET_CONNECTED) {
        InitBuffer(25);
        TCPSocket_SendString(&ServerSocket[2],dataBuffer);
        RFIDDataCount=0;
    }
    #else
    if(RFIDDataCount && ServerSocket[3].SocketState == SOCKET_CONNECTED) {
        InitBuffer(25);
        TCPSocket_SendString(&ServerSocket[3],dataBuffer);
        RFIDDataCount=0;
    }
    #endif
}

static void handleLoginRequests(void) {
    if(SendLogin1 == 1) {
        if(ServerSocket[0].SocketState == SOCKET_CONNECTED) {
            LOGData(TAG_SERVER,"\r\nSending Login Packet to server 1...\r\n");
            LoginString();
            TCPSocket_SendString(&ServerSocket[0],dataBuffer);
            SendLogin1 = 0;
        }
    }
    else if (SendLogin2 == 1) {
        if(ServerSocket[2].SocketState == SOCKET_CONNECTED) {
            LOGData(TAG_SERVER,"\r\nSending Login Packet to server 3...\r\n");
            LoginString();
            TCPSocket_SendString(&ServerSocket[2],dataBuffer);
            SendLogin2 = 0;
        }
    }
    #ifdef EXTENDED_IPS
    else if (SendLogin3 == 1) {
        if(ServerSocket[3].SocketState == SOCKET_CONNECTED) {
            LOGData(TAG_SERVER,"\r\nSending Login Packet to server 4...\r\n");
            LoginString();
            TCPSocket_SendString(&ServerSocket[3],dataBuffer);
            SendLogin3 = 0;
        }
    }
    #endif
}

static void handlePackets(void) {
    // Handle alerts if any server is connected
    if(ServerSocket[0].SocketState == SOCKET_CONNECTED || ServerSocket[2].SocketState==SOCKET_CONNECTED) {
        CheckAlerts();
    }

#ifndef PROTO_CDAC
    // Handle history packets
    if(IsPacketReady.IsHistoryPacket) {
        ProcessHistoryPacket();
        IsPacketReady.IsHistoryPacket=0;
    }
#endif

    // Handle normal packets
    if(IsPacketReady.IsNormalPacket) {
        handleNormalPackets();
    }

    // Handle health packets 
    if(IsPacketReady.IsHealthPacket && (ServerSocket[0].SocketState == SOCKET_CONNECTED)) {
        handleHealthPackets();
    }
}

static void handleServerResponses(void) {
    if(ServerSocket[0].isRXData) {
        LOGData(TAG_SERVER,"\r\nParsing Server 1 Data...");
        DecodeSMS(ServerSocket[0].rxBuffer,OTA_SRC_SCK_1);
        ServerSocket[0].isRXData=0;
    }

    if(ServerSocket[2].isRXData) {
        LOGData(TAG_SERVER,"\r\nParsing Server 2 Data...");
        DecodeSMS(ServerSocket[2].rxBuffer,OTA_SRC_SCK_2);
        ServerSocket[2].isRXData=0;
    }

    #ifdef EXTENDED_IPS
    if(ServerSocket[3].isRXData) {
        LOGData(TAG_SERVER,"\r\nParsing Server 3 Data...");
        DecodeSMS(ServerSocket[3].rxBuffer,OTA_SRC_SCK_3);
        ServerSocket[3].isRXData=0;
    }
    #endif
}

static void handleNormalPackets(void) {

	if(!GSM.IsTimeSet)
	{
		//LOGData(TAG_SERVER,"Time not set, skipping normal packet");
		return;
	}
    if(ServerSocket[0].SocketState == SOCKET_CONNECTED) {
        IsPacketReady.IsNormalPacket = 0;

        // Handle SOS alerts
        if(VAlert[SOS_ON_ALERT].Enable) {
            EmergencyPacket(1); // SOS ON ALERT
            /* OLD CODE - COMMENTED OUT AS REQUESTED:
            if(ServerSocket[1].SocketState == SOCKET_CONNECTED) {
                TCPSocket_SendString(&ServerSocket[1], dataBuffer);
            }
            */
            // NEW CODE: Dynamic routing based on Server 2 state
            if (VTSData.ServerData.IP2[0] == 'N' && VTSData.ServerData.IP2[1] == 'A') {
                // Server 2 is disabled, route EPB to Server 1
                if (ServerSocket[0].SocketState == SOCKET_CONNECTED) {
                    TCPSocket_SendString(&ServerSocket[0], dataBuffer);
                }
            } else {
                // Server 2 is enabled, route EPB to Server 2
                if (ServerSocket[1].SocketState == SOCKET_CONNECTED) {
                    TCPSocket_SendString(&ServerSocket[1], dataBuffer);
                }
            }
        }

        #ifdef SOS_FULL_EA
		if(SOS.IsSOS)
			InitBuffer(10);
		else
			InitBuffer(1);
		#else
		InitBuffer(1);  // NORMAL PACKET
		#endif
        SendDatatoServer0();
    }
    else if(GSM.GSMState >= SIM_DETECTED) {
        IsPacketReady.IsNormalPacket = 0; // Save History Packet
        #ifdef SOS_FULL_EA
		if(SOS.IsSOS)
			InitBuffer(10);
		else
			InitBuffer(1);
		#else
		InitBuffer(1);
		#endif
        SendDatatoServer0();

        // Handle SOS packet saving
        if(VAlert[SOS_ON_ALERT].Enable) {
            EmergencyPacket(1); //SOS Packet Save
			#ifndef HISTORY_DISABLED
            #ifdef HISTORY_INTERNAL
            SavePacket();
            #else
            WriteHistoryData(dataBuffer); 
            #endif
			#endif
        }
    }
}

static void handleHealthPackets(void) {
    IsPacketReady.IsHealthPacket = 0;
    HealthPacket();
    
    // Send to all connected servers
    TCPSocket_SendString(&ServerSocket[0], dataBuffer);
    TCPSocket_SendString(&ServerSocket[2], dataBuffer);
    
    #ifdef EXTENDED_IPS
    TCPSocket_SendString(&ServerSocket[3], dataBuffer);
    #endif

    #if defined(PROTO_MAHARASHTRA1)  
    InitBuffer(30);
    TCPSocket_SendString(&ServerSocket[0], dataBuffer);
    TCPSocket_SendString(&ServerSocket[2], dataBuffer);
    #ifdef EXTENDED_IPS
    TCPSocket_SendString(&ServerSocket[3], dataBuffer);
    #endif
    
    #elif defined(PROTO_NIC1)
    // Protocol specific handling
    
    #elif defined(PROTO_CDAC)
    // CDAC specific handling
    
    #else
    InitBuffer(30);
    TCPSocket_SendString(&ServerSocket[0], dataBuffer);
    TCPSocket_SendString(&ServerSocket[2], dataBuffer);
    #ifdef EXTENDED_IPS
    TCPSocket_SendString(&ServerSocket[3], dataBuffer);
    #endif
    #endif
}

// Main server thread entry point
void ServerThreadEntry(s32 taskId) {
    server_thread_init(taskId);
    ThreadSleep(8000);
    LOGData(TAG_SERVER,"\r\nServer Thread Entry!!\r\n");

    while(1) {
        // Handle profile update requests	
        handleProfileRequests();

#ifdef ENABLE_UNIFIED_FIRMWARE
        GetCurrentInterval();
        ServerThreadTimeout=0;
        handleFTPRequests();
        handleIncomingMessages();
        handleRFIDData();
        handleLoginRequests();
        handlePackets();
        handleServerResponses();
#else
        #ifndef PROTO_CDAC
        // Get current interval and reset timeout
        GetCurrentInterval();
        ServerThreadTimeout=0;

        // Handle FTP requests
        handleFTPRequests();

        // Handle incoming messages (SMS/BLE)
        handleIncomingMessages();

        // Handle RFID data
        handleRFIDData();

        // Handle login requests
        handleLoginRequests();

        // Handle various packet types
        handlePackets();

        // Handle server responses
        handleServerResponses();

        #else
        // PROTO_CDAC specific handling
        handleCDACProtocol();
        #endif
#endif

        ThreadSleep(100);
    }
}

#if defined(PROTO_CDAC)
static void storeAlertsWhileInSOS(void) {
	if(!IsPackeAlert) {
		return;
	}

	for(int i = 0; i < IsPackeAlert; i++) {
		StoreFileToFlash(CriticalString[i], ALERT);
		RemoveNonRepeatAlert(CriticalAlertIdx[i]);
	}
}

static void handleCriticalPackets(void) {
	uint16_t resp = 0;
	uint16_t criticalTimeout;
	uint8_t isEmergencyState;

    IsCritical = MakeCriticalString(1);
    IsPacketReady.IsCriticalPacket = 0;
    
    if(IsCritical) {
        VehicleState.PacketState = CRITICAL;
        LOGData(TAG_SERVER, "%d critical packet ready", IsCritical);

		/* SOS_OFF_ALERT must also bypass the queue — after timeout ResetSOS() clears
		 * IsSOS, so the EPB11 packet would fall into the normal queue path and sit
		 * behind NRM packets until evicted to flash.  Include it in emergency bypass. */
		isEmergencyState = SOS.IsSOS || SOS.IsSOSTamper || VAlert[SOS_OFF_ALERT].Enable;
		if(isEmergencyState) {
			criticalTimeout = VTSData.IntervalData.EnergencyInterval;
		}
		else if((PeriPheralVal.IsTilt && VAlert[TILT_ALERT].Enable) ||
				(IsOverSpeed && VAlert[OVER_SPEED_ALERT].Enable)) {
			criticalTimeout = VTSData.IntervalData.CurrentInterval;
		}
		else {
			criticalTimeout = 30;
		}
        
        for(int i = 0; i < IsCritical; i++) {
			#ifdef HTTP_QUEUE
			if(!isEmergencyState) {
				/* Normal critical: use queue for ordered, rate-limited delivery */
				if(HttpQueue_Add(CriticalString[i], criticalTimeout, HTTP_QUEUE_TYPE_ALERT)) {
					resp = 1;
					RemoveNonRepeatAlert(CriticalAlertIdx[i]);
					if(SOS.IsSOSSMS) {
						SMSAlert(SOS.IsSOSSMS);
						SOS.IsSOSSMS = 0;
					}
					continue;
				}
				else {
					LOGData(TAG_SERVER, "Critical packet queue failed, storing to flash");
					StoreFileToFlash(CriticalString[i], ALERT);
					RemoveNonRepeatAlert(CriticalAlertIdx[i]);
					if(SOS.IsSOSSMS) {
						SMSAlert(SOS.IsSOSSMS);
						SOS.IsSOSSMS = 0;
					}
					continue;
				}
			} else {
				/* Emergency SOS/tamper: bypass HttpQueue entirely.
				 * Queue ordering delays SOS EPB by up to queue_size × per_send_time
				 * (~100s) because the EPB lands at TAIL behind 10 existing items.
				 * Pause the queue thread, then wait for BOTH IsSendProcess AND
				 * httpQueue.sending — pausing stops new sends but an in-progress
				 * HttpQueue_Process() call may already be inside SendDataToServer().
				 * Waiting for both flags prevents a concurrent double-POST.
				 *
				 * If the wait times out (HTTP stuck — e.g. CDAC server taking 10s+
				 * to reject): force-abort via HttpQueue_AbortSend().  Without this,
				 * a single failing HTTP batch POST blocks the server thread for the
				 * entire SOS timeout window and the EPB never gets a send attempt. */
				HttpQueue_Pause();
				{
					uint8_t w = 0;
					while((IsSendProcess || HttpQueue_IsSending()) && w < 30) {
						ThreadSleep(100);
						w++;
					}
					if(IsSendProcess || HttpQueue_IsSending()) {
						LOGData(TAG_SERVER, "Emergency: HTTP still busy after 3s — aborting for SOS EPB");
						HttpQueue_AbortSend();
						ThreadSleep(200);
					}
				}
			}
			#endif

			resp = SendDataToServer(CriticalString[i],
                IsPacketReady.IsNormalPacket ||
                IsPacketReady.IsHealthPacket ||
                IsPacketReady.IsFullPacket ||
				(IsCritical - i > 1),
				criticalTimeout);

			#ifdef HTTP_QUEUE
			if(isEmergencyState) {
				HttpQueue_Resume();
			}
			#endif

            if(!resp) {
				/* Do NOT store EPB10 (SOS_ON) to flash while SOS is still active.
				 * The Systic critical interval will re-set IsCriticalPacket every
				 * EmergencyInterval seconds and retry the EPB automatically.
				 * Storing it now would produce a duplicate: the retry sends it
				 * directly, then after SOS clears the batch sends the flash copy.
				 * Only write to flash if SOS has already timed out (IsSOS=0),
				 * so the EPB can still reach the server via store-and-forward. */
				if(CriticalAlertIdx[i] == SOS_ON_ALERT && SOS.IsSOS) {
					LOGData(TAG_SERVER, "EPB10 not stored to flash — SOS active, critical interval will retry");
				} else {
					StoreFileToFlash(CriticalString[i], ALERT);
				}
            }
			else if(SOS.IsSOSSMS) {
				SMSAlert(SOS.IsSOSSMS);
				SOS.IsSOSSMS = 0;
			}
			/* Deferred removal: alert is now either sent or stored for retry.
			 * Only skip removal if both send and flash fallback failed (resp==0
			 * and StoreFileToFlash had nowhere to write) — not detectable here,
			 * so remove unconditionally to prevent stale re-pack. */
			RemoveNonRepeatAlert(CriticalAlertIdx[i]);
        }
    }
    VehicleState.PacketState = NORMAL;
}

static void handleRepeatingCriticalPackets(void) {
	uint16_t resp = 0;
	uint16_t critInterval;

	if(!IsCritical) {
		return;
	}

	VehicleState.PacketState = CRITICAL;
	critInterval = (SOS.IsSOS || SOS.IsSOSTamper) ?
		VTSData.IntervalData.EnergencyInterval :
		VTSData.IntervalData.CurrentInterval;

	LOGData(TAG_SERVER, "%d Repeating critical packet ready", IsCritical);
	for(int i = 0; i < IsCritical; i++) {
		#ifdef HTTP_QUEUE
		if(HttpQueue_Add(CriticalString[i], critInterval, HTTP_QUEUE_TYPE_ALERT)) {
			resp = 1;
			RemoveNonRepeatAlert(CriticalAlertIdx[i]);
			continue;
		}
		else {
			LOGData(TAG_SERVER, "Repeating critical packet queue failed, storing to flash");
			StoreFileToFlash(CriticalString[i], ALERT);
			RemoveNonRepeatAlert(CriticalAlertIdx[i]);
			continue;
		}
		#endif
		resp = SendDataToServer(CriticalString[i],
			IsPacketReady.IsCriticalPacket ||
			IsPacketReady.IsHealthPacket ||
			IsPacketReady.IsFullPacket ||
			(IsCritical - i > 1),
			critInterval);

		if(!resp) {
			StoreFileToFlash(CriticalString[i], ALERT);
		}
		RemoveNonRepeatAlert(CriticalAlertIdx[i]);
	}

	VehicleState.PacketState = NORMAL;
}

static void handleNormalCDACPackets(void) {
    IsPacketReady.IsNormalPacket = 0;

    // Handle critical packets first
    IsCritical = MakeCriticalString(0);
    if(IsCritical) {
        handleRepeatingCriticalPackets();
    }

    // Handle alerts
    IsPackeAlert = MakeAlertString();
    if(SOS.IsSOS) {
        handleSOSAlerts();
    }
    else if(IsPackeAlert) {
        handleNonSOSAlerts();
    }
    else {
        handleRegularPackets();
    }
}

static void handleSOSAlerts(void) {
	storeAlertsWhileInSOS();
}

static void handleNonSOSAlerts(void) {
	uint16_t resp = 0;
	uint16_t alertTimeout;

	LOGData(TAG_SERVER, "%d alert packet ready", IsPackeAlert);
	for(int i = 0; i < IsPackeAlert; i++) {
		if(i > 0) {
			LOGData(TAG_SERVER, "Logging lower-priority alert to flash: ID=%s", VAlert[CriticalAlertIdx[i]].ID);
			StoreFileToFlash(CriticalString[i], ALERT);
			RemoveNonRepeatAlert(CriticalAlertIdx[i]);
			continue;
		}

		if((PeriPheralVal.IsTilt && VAlert[TILT_ALERT].Enable) ||
		   (IsOverSpeed && VAlert[OVER_SPEED_ALERT].Enable)) {
			alertTimeout = 5;
		}
		else {
			alertTimeout = 30;
		}

		#ifdef HTTP_QUEUE
		if(HttpQueue_Add(CriticalString[i], alertTimeout, HTTP_QUEUE_TYPE_ALERT)) {
			resp = 1;
			RemoveNonRepeatAlert(CriticalAlertIdx[i]);
			continue;
		}
		else {
			LOGData(TAG_SERVER, "Alert packet queue failed, storing to flash");
			StoreFileToFlash(CriticalString[i], ALERT);
			RemoveNonRepeatAlert(CriticalAlertIdx[i]);
			continue;
		}
		#endif

		resp = SendDataToServer(CriticalString[i],
			IsPacketReady.IsCriticalPacket ||
			IsPacketReady.IsHealthPacket ||
			IsPacketReady.IsFullPacket ||
			(IsPackeAlert - i > 1),
			alertTimeout);

		if(!resp) {
			StoreFileToFlash(CriticalString[i], ALERT);
		}
		RemoveNonRepeatAlert(CriticalAlertIdx[i]);
	}
}

static void handleRegularPackets(void) {
	uint16_t resp = 0;
	uint8_t hasContinuousCritical;

	hasContinuousCritical = SOS.IsSOS || SOS.IsSOSTamper ||
		(VAlert[TILT_ALERT].Enable && VAlert[TILT_ALERT].AlertSent) ||
		VAlert[SOS_OFF_ALERT].Enable;

	LOGData(TAG_SERVER, "NRM guard: OVS=%d IsSOS=%d IsTamp=%d TiltEnSnt=%d%d SOSOff=%d",
		IsOverSpeed, SOS.IsSOS, SOS.IsSOSTamper,
		VAlert[TILT_ALERT].Enable, VAlert[TILT_ALERT].AlertSent,
		VAlert[SOS_OFF_ALERT].Enable);

	if(IsOverSpeed || hasContinuousCritical) {
		return;
	}

	if(GetBatchData()) {
		return;
	}

	MakeNormalPacket();
	LOGData(TAG_SERVER, "Normal Packet ready");
	#ifdef HTTP_QUEUE
	/* Deduplication: if the queue already holds a pending normal packet, replace
	 * its data with the current snapshot (fresh timestamp) instead of appending.
	 * Without this, CDAC failures cause the queue to fill with NRM packets that
	 * all share the same frozen GPS/RTC timestamp, producing duplicates on-server. */
	if(HttpQueue_Count() > 0 && HttpQueue_HasNormalPending()) {
		HttpQueue_ReplaceLatestNormal(SendString, VTSData.IntervalData.CurrentInterval);
		return;
	}
	if(HttpQueue_Add(SendString, VTSData.IntervalData.CurrentInterval, HTTP_QUEUE_TYPE_NORMAL)) {
		return;
	}
	else {
		LOGData(TAG_SERVER, "Normal packet queue failed, storing to flash");
		StoreFileToFlash(SendString, NORMAL);
		return;
	}
	#endif
	resp = SendDataToServer(SendString,
		IsPacketReady.IsCriticalPacket ||
		IsPacketReady.IsHealthPacket ||
		IsPacketReady.IsFullPacket,
		VTSData.IntervalData.CurrentInterval);
	if(!resp) {
		StoreFileToFlash(SendString, NORMAL);
	}
}

static void handleHealthCDACPackets(void) {
    IsPacketReady.IsHealthPacket = 0;
    if(!IsCritical && !SOS.IsSOS) {
        HealthPacket();
        LOGData(TAG_SERVER, "health packet ready");
		#ifdef HTTP_QUEUE
		if(HttpQueue_Add(SendString, VTSData.IntervalData.CurrentInterval, HTTP_QUEUE_TYPE_NORMAL)) {
			return;
		}
		#endif
		SendDataToServer(SendString,
            IsPacketReady.IsFullPacket ||
            IsPacketReady.IsCriticalPacket || 
			IsPacketReady.IsNormalPacket,
			VTSData.IntervalData.CurrentInterval);
    }
    else {
        LOGData(TAG_SERVER, "health packet ignored due to critical state");
    }
}

static void handleFullCDACPackets(void) {
    IsPacketReady.IsFullPacket = 0;
    if(!IsCritical && !SOS.IsSOS) {
        FullPacket();
        LOGData(TAG_SERVER, "full packet ready");
		#ifdef HTTP_QUEUE
		if(HttpQueue_Add(SendString, VTSData.IntervalData.CurrentInterval, HTTP_QUEUE_TYPE_NORMAL)) {
			return;
		}
		#endif
		SendDataToServer(SendString,
            IsPacketReady.IsCriticalPacket || 
            IsPacketReady.IsHealthPacket || 
			IsPacketReady.IsNormalPacket,
			VTSData.IntervalData.CurrentInterval);
    }
    else {
        LOGData(TAG_SERVER, "full packet ignored due to critical state");
    }
}

static void handleCDACProtocol(void) {
    uint16_t resp;
	uint8_t criticalHandled = 0;

	handleFTPRequests();

	if(IsSMS) {
		LOGData(TAG_SERVER, "\r\nParsing SNS Data...");
		DecodeOTAData(SMSData, 0);
		IsSMS = 0;
	}

	if(RS232_DataAvailable) {
		LOGData(TAG_SERVER, "\r\nParsing RS232 OTA Data...");
		ProcessRS232OTAData();
	}

	if(RS485_DataAvailable) {
		LOGData(TAG_SERVER, "\r\nParsing RS485 OTA Data...");
		ProcessRS485OTAData();
	}

	if(ServerSocket[0].isRXData) {
		LOGData(TAG_SERVER, "\r\nParsing Server 1 Data...");
		print_long_string((const char*)ServerSocket[0].rxBuffer);
		DecodeOTAData(ServerSocket[0].rxBuffer, 1);
		Ql_memset(ServerSocket[0].rxBuffer, 0, ServerSocket[0].rxSizeMAX);
		ServerSocket[0].isRXData = 0;
	}

	if(ServerSocket[2].isRXData) {
		LOGData(TAG_SERVER, "\r\nParsing Server 2 Data...");
		DecodeOTAData(ServerSocket[2].rxBuffer, 2);
		Ql_memset(ServerSocket[2].rxBuffer, 0, ServerSocket[2].rxSizeMAX);
		ServerSocket[2].isRXData = 0;
	}

    if(GSM.GSMState >= SIM_DETECTED && GSM.IsTimeSet) {
        // Handle login
        if(SendLogin1) {
            LoginPacket();
            LOGData(TAG_SERVER, "login packet ready");
			#ifdef HTTP_QUEUE
			if(HttpQueue_Add(SendString, VTSData.IntervalData.CurrentInterval, HTTP_QUEUE_TYPE_NORMAL)) {
				SendLogin1 = 0;
				return;
			}
			#endif
			resp = SendDataToServer(SendString, 0, VTSData.IntervalData.CurrentInterval);
            if(resp) {
                SendLogin1 = 0;
            }
        }
        // Handle other packets
        else {
            // Handle critical packets
            if(IsPacketReady.IsCriticalPacket) {
                handleCriticalPackets();
				criticalHandled = 1;
				/* Reset IsCritical so the same cycle's HEALTH/FULL packets
				 * are not suppressed by a stale non-zero count. */
				IsCritical = 0;
            }

            // Handle normal packets
            if(IsPacketReady.IsNormalPacket) {
                handleNormalCDACPackets();
            }

            // Handle health packets
            if(IsPacketReady.IsHealthPacket) {
                handleHealthCDACPackets();
            }
			// Handle full packets
			if(IsPacketReady.IsFullPacket) {
                handleFullCDACPackets();
            }
        }
    }

	if(GSM.GSMState < GPRS_ACTIVE && SOS.IsSOSSMS) {
		SMSAlert(SOS.IsSOSSMS);
		SOS.IsSOSSMS = 0;
	}
}

#endif


void server_thread_init(u32 taskId)
{
	s32 ret;
	OSThread Server_Thread = {0};
	Server_Thread.taskId = taskId;
	Ql_strcpy(Server_Thread.taskName, "Server Thread");
	Server_Thread.taskEnable = 1;
	Server_Thread.taskState = TASK_STATE_NORMAL;
	Server_Thread.taskPriority = 1;
	ret = InitializeThread(&Server_Thread);
	if (ret != 1)
	{
		LOGData(TAG_SERVER, "Failed to initialize Server thread");
		return;
	}
	LOGData(TAG_SERVER, "Server thread initialized successfully");
}
