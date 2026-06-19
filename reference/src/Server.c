#include "MCU.h"
#include "Server.h"
#include "Utilities.h"
//#include "Hardware.h"
#include "SOS.h"
#include "SMS.h"
#include "Alert.h"
#include "PktSave.h"
#include "Sensors.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "DHT11.h"

#ifdef PROTO_CDAC
#include "CDAC_Protocol.h"
#endif

#ifdef HTTP_QUEUE
#include "HttpQueue.h"
#endif

uint16_t IsCritical;
uint16_t IsPackeAlert;
uint16_t DeltaDis;
#ifdef PROTO_CDAC
/* Use CDAC_REGULAR_PACKET_SIZE from CDAC_Protocol.h instead of magic number */
#define _REGULAR_SIZE	CDAC_REGULAR_PACKET_SIZE
uint16_t IsCritical;
uint16_t IsPackeAlert;
uint8_t IsSendProcess;
uint32_t FrameNumber=1;
OTATypeDef OTAValue;
extern VehicleTypeDef VehicleState;
extern char VehicleMovingMode;
uint8_t IsOverSpeed;

#else
uint16_t StoredHistoryDataCount;
uint8_t IsEMRSend, IsEMRTSend, CNFChange, IsOverSpeed;
uint32_t FrameNumber;
uint8_t IsStored = 0;
#endif

double prevLat,prevLong;
double fGPSLat,fGPSLong,fGPSAlt,fGPSpdop,fGPShdop,fGPSSats, fGPSSpeed, fGPSHeading, fGPSForce;
char Server1RxData[SERVER_RX_SIZE_MAX+1] = {0};
char Server2RxData[SERVER_RX_SIZE_MAX+1] = {0};

uint8_t IsServerRes;

extern volatile uint16_t ServerThreadTimeout;

#ifdef PROTO_CDAC
char SendString[DATA_MAX_BUFF];
char CriticalString[5][CRITICAL_MAX_BUFF];
char ActivationKey[18];
#else
char dataBuffer[DATA_MAX_BUFF];
#endif



uint8_t MemoryPercent;
uint16_t GetMemeryPercentage(void);
#ifdef PROTO_CDAC
uint8_t SendDataToServer(char* data, uint8_t KeepAlive, uint16_t currentIntervalSec);
#else
uint8_t SendDataToServer(char* data, uint8_t KeepAlive);
#endif
uint8_t SendLogin1;
uint8_t SendLogin2;
#ifdef EXTENDED_IPS
uint8_t SendLogin3;
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

// Simple 1-byte checksum - sum of all bytes
uint8_t SimpleChecksum(const char *data, int length) 
{
    uint8_t checksum = 0;
    for(int i = 0; i < length; i++) {
        checksum += (uint8_t)data[i];
    }
    return checksum;
}
#ifdef PROTO_CDAC
void InsertStringValue(const char* value, uint16_t position, uint16_t length, uint8_t wh)
{
	uint16_t n=0,i=0;
	char vl;
	uint16_t ln=strlen(value);
	if(!value) // Or ln == 0
		return;
	if(ln > 0)
	{
		if(ln <= length)
			n=length-ln;
		else
			ln=length;

		position = position + n;
		while(i<ln)
		{
			vl=value[i++];
			if((wh) && (vl=='-'))
				vl='0';
			SendString[position++]=vl;
		}
	}
}

void InsertIntValue(uint16_t value, uint16_t position, uint16_t length)
{
	uint16_t n=0, i=0;
	uint16_t ln;
	char ss[20];
	const char ls[]="%d";
	sprintf(ss,ls,value);
	ln=strlen(ss);
	if(ln > 0)
	{
		if(ln <= length)
			n=length-ln;
		else
			ln=length;

		position = position + n;
		while(i<ln)
		{
			SendString[position++]=ss[i++];
		}
	}
}

void InsertFloatValue(double value, uint16_t position, uint16_t length,const char* decimal)
{
	uint16_t n=0, i=0;
	uint16_t ln;
	char ss[20];
	sprintf(ss,decimal,value);
	ln=strlen(ss);
	if(ln > 0)
	{
		if(ln <= length)
			n=length-ln;
		else
			ln=length;
		position = position + n;
		while(i<ln)
		{
			SendString[position++]=ss[i++];
		}
	}
}
void InsertCurrentDateTime(uint16_t position)
{
	char tempData[16];
	sprintf(tempData,"%02d%02d%02d",CurrentDateTime.Date,CurrentDateTime.Month,CurrentDateTime.Year);
	InsertStringValue(tempData,position,6,0);
	sprintf(tempData,"%02d%02d%02d",CurrentDateTime.Hour,CurrentDateTime.Min,CurrentDateTime.Sec);
	InsertStringValue(tempData,position+6,6,0);
}
#endif
void InsertHEXStringToBuffer(char *buffer, uint8_t *data, uint16_t len)
{
	char ss[5];
	for(int i=0;i<len;i++)
	{
		sprintf(ss,"%02X",data[i]);
		strcat(buffer,ss);
	}
}

#ifndef PROTO_CDAC
void SensorString(void)
{
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcpy(dataBuffer,"$SENS,");
	strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
	
	strcat(dataBuffer,",{");
	if(DHT11.Status)
	{
		InsertIntValue(dataBuffer,DHT11.temp,"%01d,");
		InsertIntValue(dataBuffer,DHT11.humidity,"%01d},{");
	}
	else
		strcat(dataBuffer,"0,0},{");
	if(IsFuelData)
		strcat(dataBuffer,FuelData);
	else
		InsertChar(dataBuffer,'0');
	strcat(dataBuffer,"}*");

}
#endif

#ifdef PROTO_MAHARASHTRA1

void LoginString(void)
{
	char ss[18];
	uint8_t checksum;
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcpy(dataBuffer,"$LGN,");
	strcat(dataBuffer,VTSData.VendorID);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	//AppendVariableString(dataBuffer,FirmVer,6,3,"V1.6.2");
	strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,PROTOVER);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,'*');
	checksum = SimpleChecksum(dataBuffer, strlen(dataBuffer));
	sprintf(ss, "%02X", checksum);
	strcat(dataBuffer, ss);
	// FrameNumber = 1;
}

#elif defined(PROTO_NIC1) 

void LoginString(void)
{
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcpy(dataBuffer,"$LGN,");
	strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	//AppendVariableString(dataBuffer,FirmVer,6,3,"V1.6.2");
	strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,PROTOVER);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,sLatitude);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,sLongitude);
	InsertChar(dataBuffer,'*');
	// FrameNumber = 1;
}

#elif defined(PROTO_CDAC)

/**
 * @brief Build Login Packet (LGN) for CDAC protocol
 * Format: vltdata=LGN<IMEI><ActivationKey><Lat><LatDir><Lon><LonDir><DateTime><Speed>
 */
void LoginPacket(void)
{
	uint16_t dLen = CDAC_BASE_OFFSET;
	
	if(strlen(ActivationKey) < 12)
		strcpy(ActivationKey, "1234567890123456");
	
	memset(SendString, 0, DATA_MAX_BUFF);	
	memset(SendString, '0', CDAC_LOGIN_PACKET_SIZE);
	
	/* Header */
	InsertStringValue(CDAC_HTTP_PREFIX CDAC_HDR_LOGIN, 0, CDAC_HTTP_PREFIX_LEN + CDAC_HEADER_SIZE, 0);
	
	/* Login data fields */
	InsertStringValue(NetWork.IMEI, dLen + LGN_IMEI_POS, CDAC_IMEI_SIZE, 1);
	InsertStringValue(ActivationKey, dLen + LGN_ACTIVATION_KEY_POS, CDAC_ACTIVATION_KEY_SIZE, 1);
	InsertFloatValue(GPS.Latitude, dLen + LGN_LATITUDE_POS, CDAC_LAT_SIZE, "%010.6f");
	SendString[dLen + LGN_LAT_DIR_POS] = GPS.LatDir;
	InsertFloatValue(GPS.Longitude, dLen + LGN_LONGITUDE_POS, CDAC_LON_SIZE, "%010.6f");
	SendString[dLen + LGN_LON_DIR_POS] = GPS.LngDir;
	InsertCurrentDateTime(dLen + LGN_DATETIME_POS);
	InsertFloatValue(GPS.Speed, dLen + LGN_SPEED_POS, CDAC_SPEED_SIZE, "%06.2f");
	SendString[dLen + LGN_PACKET_SIZE] = 0;
}

/**
 * @brief Build common data fields for NRM/EPB/CRT/ALT packets
 * Fills GPS, cell info, and vehicle status fields
 */
void DataPacket(void)
{
	uint16_t dLen = CDAC_BASE_OFFSET;
	
	/* IMEI */
	InsertStringValue(NetWork.IMEI, dLen + DATA_IMEI_POS, CDAC_IMEI_SIZE, 0);
	
	/* GPS Fix */
	SendString[dLen + DATA_GPS_FIX_POS] = GPS.GPSFix + '0';
	
	/* Date/Time */
	InsertCurrentDateTime(dLen + DATA_DATETIME_POS);
	
	/* Position */
	InsertFloatValue(GPS.Latitude, dLen + DATA_LATITUDE_POS, CDAC_LAT_SIZE, "%010.6f");
	SendString[dLen + DATA_LAT_DIR_POS] = GPS.LatDir;
	InsertFloatValue(GPS.Longitude, dLen + DATA_LONGITUDE_POS, CDAC_LON_SIZE, "%010.6f");
	SendString[dLen + DATA_LON_DIR_POS] = GPS.LngDir;
	
	/* Cell Info */
	InsertIntValue(GSM.MCC, dLen + DATA_MCC_POS, CDAC_MCC_SIZE);
	InsertIntValue(GSM.MNC, dLen + DATA_MNC_POS, CDAC_MNC_SIZE);
	InsertStringValue(GSM.LAC, dLen + DATA_LAC_POS, CDAC_LAC_SIZE, 1);
	InsertStringValue(GSM.CellID, dLen + DATA_CELLID_POS, CDAC_CELLID_SIZE, 1);
	
	/* Speed and Heading */
	InsertFloatValue(GPS.Speed, dLen + DATA_SPEED_POS, CDAC_SPEED_SIZE, "%03.2f");
	InsertFloatValue(GPS.Heading, dLen + DATA_HEADING_POS, CDAC_HEADING_SIZE, "%03.2f");
	
	/* GPS Quality */
	InsertIntValue(GPS.NoOfSatalite, dLen + DATA_SATS_POS, CDAC_SATS_SIZE);
	InsertIntValue((uint16_t)GPS.HDOP, dLen + DATA_HDOP_POS, CDAC_HDOP_SIZE);
	InsertIntValue(GSM.SignalStrength, dLen + DATA_SIGNAL_POS, CDAC_SIGNAL_SIZE);
	
	/* Vehicle Status */
	SendString[dLen + DATA_IGN_POS] = PeriPheralVal.IGN + '0';
	SendString[dLen + DATA_MAIN_POWER_POS] = PeriPheralVal.IsMain + '0';
	SendString[dLen + DATA_VEHICLE_MODE_POS] = VehicleMovingMode;
	
	/* Altitude */
	InsertFloatValue(GPS.Altitude, dLen + DATA_ALTITUDE_POS, CDAC_ALTITUDE_SIZE, "%04.2f");
	
	/* Network Operator Name (padded with 'X' if shorter than 6 chars) */
	char spn[7];
	sprintf(spn, "%s", NetWork.Network);
	int rem = CDAC_NETWORK_NAME_SIZE - strlen(NetWork.Network);
	for(int i = 0; i < rem; i++)
		spn[CDAC_NETWORK_NAME_SIZE - 1 - i] = 'X';
	InsertStringValue(spn, dLen + DATA_NETWORK_NAME_POS, CDAC_NETWORK_NAME_SIZE, 1);
}

#elif defined(PROTO_OG)

void LoginString(void)  // Original
{
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcpy(dataBuffer,"$");
	strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,'$');
	strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,'$');
	strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,'$');
	strcat(dataBuffer,"AIS140");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,'$');
	strcat(dataBuffer,sLatitude);
	InsertChar(dataBuffer,'N');
	strcat(dataBuffer,sLongitude);
	InsertChar(dataBuffer,'E');

}

#else


void LoginString(void)  // ODISA
{
	char ss[18];
	uint32_t crc;
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcpy(dataBuffer,"$LGN,");
	strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	//AppendVariableString(dataBuffer,FirmVer,6,3,"V1.6.2");
	strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,"AIS140");
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,sLatitude);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,sLongitude);
	InsertChar(dataBuffer,',');
	// InsertChar(dataBuffer,'*');
	crc = checksum32(dataBuffer,strlen(dataBuffer));
	sprintf(ss,"%08X*",crc);
	strcat(dataBuffer,ss);
	InsertChar(dataBuffer,'\n');
	// FrameNumber = 1;
}
#endif

// void MakeRFIDPacket(uint8_t *data, uint16_t len)
// {
// 	char ss[18];
// 	uint32_t crc;
// 	memset(dataBuffer,0x00,DATA_MAX_BUFF);
// 	strcpy(dataBuffer,"$RFID,");
// 	strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
// 	InsertChar(dataBuffer,',');
// 	strncat(dataBuffer,NetWork.IMEI,15);
// 	InsertChar(dataBuffer,',');
// 	strcat(dataBuffer,FirmVer);
// 	InsertChar(dataBuffer,',');
// 	strcat(dataBuffer,sLatitude);
// 	InsertChar(dataBuffer,',');
// 	strcat(dataBuffer,sLongitude);
// 	InsertChar(dataBuffer,',');
// 	InsertHEXStringToBuffer(dataBuffer,data,len);
// 	InsertChar(dataBuffer,',');
// 	crc = checksum32(dataBuffer,strlen(dataBuffer));
// 	sprintf(ss,"%08X*",crc);
// 	strcat(dataBuffer,ss);
// }


#ifdef PROTO_MAHARASHTRA1
void InitBuffer(uint8_t alt)
{
	char ss[20];
	uint16_t i;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	memset(dataBuffer,0,DATA_MAX_BUFF);
	sprintf(dataBuffer,"$NMP,%s,",VTSData.VendorID);
	strcat(dataBuffer,FirmVer);


	switch(alt)
	{
		case 1:
			strcat(dataBuffer,",NR,1,L,");// NORMAL PACKET
			break;
		case 3:
			strcat(dataBuffer,",BD,3,L,"); // MAIN OFF
			break;
		case 4:
			strcat(dataBuffer,",BL,4,L,"); // BATTERY LOW
			break;
		case 5:
			strcat(dataBuffer,",BH,5,L,"); // Battery LOW Restore
			break;
		case 6:
			strcat(dataBuffer,",BR,6,L,"); // MAIN ON
			break;
		case 7:
			strcat(dataBuffer,",IN,7,L,"); // IGNITION ON
			break;
		case 8:
			strcat(dataBuffer,",IF,8,L,"); // IGNITION OFF
			break;
		case 9:
			strcat(dataBuffer,",TA,9,L,"); // BOX TAMPER
			break;
		case 10:
			strcat(dataBuffer,",EA,10,L,"); // SOS ON
			break;
		case 11:
			strcat(dataBuffer,",EO,11,L,"); // SOS OFF
			break;
		case 12:
			strcat(dataBuffer,",OT,12,L,"); // OTA 
			break;
		case 13:
			strcat(dataBuffer,",HB,13,L,"); // Harsh Braking
			break;
		case 14:
			strcat(dataBuffer,",HA,14,L,"); // Harsh Acceleration
			break;
		case 15:
			strcat(dataBuffer,",RT,15,L,"); // Rash turn
			break;
		case 16:
			strcat(dataBuffer,",TA,16,L,");  // SOS Tamper
			break;
		case 17:
			strcat(dataBuffer,",GI,18,L,");  // GeoFence In
			break;
		case 18:
			strcat(dataBuffer,",GO,19,L,");  // GeoFence Out
			break;
				
		case 23:
			strcat(dataBuffer,",OS,17,L,"); // Over Speed
			break;
		case 24:
			strcat(dataBuffer,",TL,24,L,"); // Vehicle Tilt
			break;

		case 25:
			strcat(dataBuffer,",RF,25,L,"); // RFID Data
		case 30:
			strcat(dataBuffer,",HP,1,L,"); // HP Data
	}
	strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.GPSFix + '0');
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
	strcat(dataBuffer,NetWork.Network);
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
		InsertChar(dataBuffer,'-');
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
	if(SOS_STATE)
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


	//strcat(dataBuffer,"(0,0),");
	if(alt == 25) // For RFID Data 
	{
		InsertChar(dataBuffer,'(');
		InsertHEXStringToBuffer(dataBuffer,RFIDData,RFIDDataCount);
		strcat(dataBuffer,"),");
	}
	else
		strcat(dataBuffer,"(0,0,0)");
	

	uint8_t checksum = SimpleChecksum(dataBuffer, strlen(dataBuffer));
	sprintf(ss, "*%02X", checksum);
	strcat(dataBuffer, ss);
	FrameNumber++;
	
}

#elif defined(PROTO_NIC1) 

void InitBuffer(uint8_t alt)
{
	char ss[20];
	uint16_t i;
	uint16_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	memset(dataBuffer,0,DATA_MAX_BUFF);
	sprintf(dataBuffer,"$PVT,%s,",VTSData.VendorID);
	strcat(dataBuffer,FirmVer);


	switch(alt)
	{
		case 1:
			strcat(dataBuffer,",NR,01,L,");// NORMAL PACKET
			break;
		case 3:
			strcat(dataBuffer,",BD,03,L,"); // MAIN OFF
			break;
		case 4:
			strcat(dataBuffer,",BL,04,L,"); // BATTERY LOW
			break;
		case 5:
			strcat(dataBuffer,",BH,05,L,"); // Battery LOW Restore
			break;
		case 6:
			strcat(dataBuffer,",BR,06,L,"); // MAIN ON
			break;
		case 7:
			strcat(dataBuffer,",IN,07,L,"); // IGNITION ON
			break;
		case 8:
			strcat(dataBuffer,",IF,08,L,"); // IGNITION OFF
			break;
		case 9:
			strcat(dataBuffer,",TA,09,L,"); // BOX TAMPER
			break;
		case 10:
			strcat(dataBuffer,",EA,10,L,"); // SOS ON
			break;
		case 11:
			strcat(dataBuffer,",EO,11,L,"); // SOS OFF
			break;
		case 12:
			strcat(dataBuffer,",OT,12,L,"); // OTA 
			break;
		case 13:
			strcat(dataBuffer,",HB,13,L,"); // Harsh Braking
			break;
		case 14:
			strcat(dataBuffer,",HA,14,L,"); // Harsh Acceleration
			break;
		case 15:
			strcat(dataBuffer,",RT,15,L,"); // Rash turn
			break;
		case 16:
			strcat(dataBuffer,",TA,16,L,");  // SOS Tamper
			break;
		case 17:
			strcat(dataBuffer,",GI,18,L,");  // GeoFence In
			break;
		case 18:
			strcat(dataBuffer,",GO,19,L,");  // GeoFence Out
			break;
		case 23:
			strcat(dataBuffer,",OS,17,L,"); // Over Speed
			break;
		case 24:
			strcat(dataBuffer,",TL,22,L,"); // Vehicle Tilt
			break;

		case 25:
			strcat(dataBuffer,",RF,25,L"); // RFID Data
	}
	strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.GPSFix + '0');
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
	#ifdef BSNL_PROTO
	strcat(dataBuffer,"CellOne");
	#else
	strcat(dataBuffer,NetWork.Network);
	#endif
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IGN + '0');
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.IsMain + '0');
	InsertChar(dataBuffer,',');
	#ifndef NO_PARAM 
	if(alt==3)
		strcat(dataBuffer,"00.0");
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
	if(SOS_STATE)
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
		strcat(dataBuffer,"),");
	}

	// InsertFloatValue(dataBuffer,PeriPheralVal.AN1,"%2.1f");
	// InsertChar(dataBuffer,',');
	// InsertFloatValue(dataBuffer,PeriPheralVal.AN2,"%2.1f");
	// InsertChar(dataBuffer,',');

	// InsertIntValue(dataBuffer,DeltaDis,"%02d");
	// InsertChar(dataBuffer,',');


	////AppendVariableString(dataBuffer,"(0,0,0)",20,5,"(0,0,0)");

	//strcat(dataBuffer,"(0,0),");
	#if defined(NIC_UTTRA)
	strcat(dataBuffer,"00*");
	#else
	crc = chksum(dataBuffer,strlen(dataBuffer));
	sprintf(ss,"%04X*",crc);
	strcat(dataBuffer,ss);
	#endif
	FrameNumber++;



	
	
}
#elif defined(PROTO_CDAC)



// uint8_t GetAlertsHeader(void)
// {
// 	uint8_t rt;
// 	uint16_t n=0;
// 	uint16_t dLen=8;
// 	uint8_t j=StoredAlert.AlertPosition;
// 	memset(SendString,0,DATA_MAX_BUFF);	
// 	memset(SendString,'0',118);
// 	InsertStringValue("vltdata=",0,8,0);
// 	if(!StoredAlert.TotalAlert)
// 	{
// 		InsertStringValue("NRM",dLen + 0,3,0);
// 		InsertStringValue("01L",dLen + 18,3,0);
// 		SendString[dLen + _REGULAR_SIZE]=0;
// 		return 0xFF;
// 	}
// 	nwy_dbg_log("Stored total alt %d > 0,  altsval : %d, pos: %d",StoredAlert.TotalAlert,StoredAlert.Alerts[j],j);
// 	if(StoredAlert.Alerts[j] != 0xFF)
// 	{
// 		nwy_dbg_log("found stored alert %d",StoredAlert.Alerts[j]);
// 		InsertStringValue(VAlert[StoredAlert.Alerts[j]].Header,dLen + 0,3,1);
// 		InsertStringValue(VAlert[StoredAlert.Alerts[j]].ID,dLen + 18,2,1);
// 		SendString[dLen + 20]='L';
// 		if(VAlert[StoredAlert.Alerts[j]].WithACK)
// 		{
// 			nwy_dbg_log("alert ack size %d",strlen(VAlert[StoredAlert.Alerts[j]].ACK));
// 			InsertStringValue(VAlert[StoredAlert.Alerts[j]].ACK,dLen + _REGULAR_SIZE,strlen(VAlert[StoredAlert.Alerts[j]].ACK),1);
// 			n=strlen(VAlert[StoredAlert.Alerts[j]].ACK);
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

/**
 * @brief Build Normal Packet (NRM) for CDAC protocol
 * Format: vltdata=NRM<IMEI><01L><GPSFix><DateTime>...
 * @return 1 if packet built successfully, 0 if overspeed detected
 */
uint8_t MakeNormalPacket(void)
{
	uint16_t dLen = CDAC_BASE_OFFSET;
	
	memset(SendString, 0, DATA_MAX_BUFF);	
	memset(SendString, '0', BTH_LIVE_PACKET_SIZE + CDAC_BASE_OFFSET);
	
	/* Header and prefix */
	InsertStringValue(CDAC_HTTP_PREFIX, 0, CDAC_HTTP_PREFIX_LEN, 0);
	InsertStringValue(CDAC_HDR_NORMAL, dLen + DATA_HEADER_POS, CDAC_HEADER_SIZE, 0);
	
	/* Packet status: 01 = live, L = live */
	InsertStringValue("01L", dLen + DATA_ALERT_ID_POS, 3, 0);
	
	/* Null terminate at regular packet size */
	SendString[dLen + _REGULAR_SIZE] = 0;
	
	/* Fill in data fields */
	DataPacket();
	
	/* Don't send if overspeed */
	if(GPS.Speed > VTSData.VehicleData.OverSpeed)
		return 0;
	return 1;
}


#elif defined(PROTO_OG)


void InitBuffer(uint8_t alt)
{
	char ss[20];
	uint16_t i;
	uint8_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	memset(dataBuffer,0,DATA_MAX_BUFF);
	sprintf(dataBuffer,"$,NMP,%s,",VTSData.VendorID);
	strcat(dataBuffer,FirmVer);


	switch(alt)
	{
		case 1:
			strcat(dataBuffer,",NR,1,L,");// NORMAL PACKET
			break;
		case 3:
			strcat(dataBuffer,",BD,3,L,"); // MAIN OFF
			break;
		case 4:
			strcat(dataBuffer,",BL,4,L,"); // BATTERY LOW
			break;
		case 5:
			strcat(dataBuffer,",BH,5,L,"); // Battery LOW Restore
			break;
		case 6:
			strcat(dataBuffer,",BR,6,L,"); // MAIN ON
			break;
		case 7:
			strcat(dataBuffer,",IN,7,L,"); // IGNITION ON
			break;
		case 8:
			strcat(dataBuffer,",IF,8,L,"); // IGNITION OFF
			break;
		case 9:
			strcat(dataBuffer,",DT,9,L,"); // BOX TAMPER
			break;
		case 10:
			strcat(dataBuffer,",EA,10,L,"); // SOS ON
			break;
		case 11:
			strcat(dataBuffer,",EO,11,L,"); // SOS OFF
			break;
		case 12:
			strcat(dataBuffer,",OT,12,L,"); // OTA 
			break;
		case 13:
			strcat(dataBuffer,",HB,13,L,"); // Harsh Braking
			break;
		case 14:
			strcat(dataBuffer,",HA,14,L,"); // Harsh Acceleration
			break;
		case 15:
			strcat(dataBuffer,",RT,15,L,"); // Rash turn
			break;
		case 16:
			strcat(dataBuffer,",TA,16,L,");  // SOS Tamper
			break;
		case 17:
			strcat(dataBuffer,",GI,18,L,");  // GeoFence In
			break;
		case 18:
			strcat(dataBuffer,",GO,19,L,");  // GeoFence Out
			break;
				
		case 23:
			strcat(dataBuffer,",OS,17,L,"); // Over Speed
			break;
		case 24:
			strcat(dataBuffer,",TL,24,L,"); // Vehicle Tilt
			break;

		case 25:
			strcat(dataBuffer,",RF,25,L,"); // RFID Data
		case 30:
			strcat(dataBuffer,",HP,01,L,"); // HP Data
	}
	strncat(dataBuffer,NetWork.IMEI,15);
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
	strcat(dataBuffer,NetWork.Network);
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
	if(SOS.IsSOS)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,FrameNumber,"%06d");
	InsertChar(dataBuffer,',');
	

	crc = CRC8(dataBuffer,strlen(dataBuffer));
	sprintf(ss,"%02X",crc);
	strcat(dataBuffer,ss);
	sprintf(ss,",*");
	strcat(dataBuffer,ss);
	FrameNumber++;
	
}

#else
void InitBuffer(uint8_t alt) // ODISA
{
	char ss[20];
	uint16_t i;
	uint32_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	memset(dataBuffer,0,DATA_MAX_BUFF);
	sprintf(dataBuffer,"$PVT,%s,",VTSData.VendorID);
	strcat(dataBuffer,FirmVer);


	switch(alt)
	{
		case 1:
			strcat(dataBuffer,",NR,01,L,");// NORMAL PACKET
			break;
		case 3:
			strcat(dataBuffer,",BD,03,L,"); // MAIN OFF
			break;
		case 4:
			strcat(dataBuffer,",BL,04,L,"); // BATTERY LOW
			break;
		case 5:
			strcat(dataBuffer,",BC,05,L,"); // Battery LOW Restore
			break;
		case 6:
			strcat(dataBuffer,",BR,06,L,"); // MAIN ON
			break;
		case 7:
			strcat(dataBuffer,",IN,07,L,"); // IGNITION ON
			break;
		case 8:
			strcat(dataBuffer,",IF,08,L,"); // IGNITION OFF
			break;
		case 9:
			strcat(dataBuffer,",TA,09,L,"); // BOX TAMPER
			break;
		case 10:
			strcat(dataBuffer,",EA,10,L,"); // SOS ON
			break;
		case 11:
			strcat(dataBuffer,",EO,11,L,"); // SOS OFF
			break;
		case 12:
			strcat(dataBuffer,",CFG,12,L,"); // OTA 
			break;
		case 13:
			strcat(dataBuffer,",HB,13,L,"); // Harsh Braking
			break;
		case 14:
			strcat(dataBuffer,",HA,14,L,"); // Harsh Acceleration
			break;
		case 15:
			strcat(dataBuffer,",RT,15,L,"); // Rash turn
			break;
		case 16:
			strcat(dataBuffer,",TA,16,L,");  // SOS Tamper
			break;
		case 17:
			strcat(dataBuffer,",GI,17,L,");  // GeoFence In
			break;
		case 18:
			strcat(dataBuffer,",GO,18,L,");  // GeoFence Out
			break;
				
		case 23:
			strcat(dataBuffer,",OS,20,L,"); // Over Speed
			break;
		case 24:
			strcat(dataBuffer,",TL,24,L,"); // Vehicle Tilt
			break;

		case 25:
			strcat(dataBuffer,",RF,25,L,"); // RFID Data
		case 30:
			strcat(dataBuffer,",HP,01,L,"); // HP Data
	}
	strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.GPSFix + '0');
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
	strcat(dataBuffer,NetWork.Network);
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
	if(SOS_STATE)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	strcat(dataBuffer,"00,");


	InsertIntValue(dataBuffer,FrameNumber,"%06d");
	InsertChar(dataBuffer,',');

	// InsertFloatValue(dataBuffer,PeriPheralVal.AN1,"%2.1f");
	// InsertChar(dataBuffer,',');
	// InsertFloatValue(dataBuffer,PeriPheralVal.AN2,"%2.1f");
	// InsertChar(dataBuffer,',');

	// InsertIntValue(dataBuffer,DeltaDis,"%02d");
	// InsertChar(dataBuffer,',');


	////AppendVariableString(dataBuffer,"(0,0,0)",20,5,"(0,0,0)");

	//strcat(dataBuffer,"(0,0),");
	if(alt == 25) // For RFID Data 
	{
		InsertChar(dataBuffer,'{');
		InsertHEXStringToBuffer(dataBuffer,RFIDData,RFIDDataCount);
		strcat(dataBuffer,"},");
	}
	

	//#ifdef ODISA_LD
	crc = CRC16(dataBuffer,strlen(dataBuffer));
	sprintf(ss,"%04X*",crc);
	strcat(dataBuffer,ss);
	//#else
	// crc = checksum32(dataBuffer,strlen(dataBuffer));
	// sprintf(ss,"%08X*",crc);
	// strcat(dataBuffer,ss);
	#ifndef ODISA_LD
    InsertChar(dataBuffer,'\n');
    #endif
	//#endif
	FrameNumber++;
	
}
#endif


#ifdef PROTO_MAHARASHTRA1


void EmergencyPacket(uint8_t IsOff)
{
	char ss[20];
	uint8_t crc;

	if(prevLat==0)
		DeltaDis=0;
	else
		DeltaDis = calculateDistance(prevLat,prevLong,GPS.Latitude,GPS.Longitude);

	prevLat = GPS.Latitude;
	prevLong = GPS.Longitude;

	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	if(IsOff)
		strcat(dataBuffer,"$EPB,EMR,");
	else
		strcat(dataBuffer,"$EPB,SEM,");

	strncat(dataBuffer,NetWork.IMEI,15);

	if(IsStored)
		AppendFixString(dataBuffer,",SP,",4,",SP,");
	else
		AppendFixString(dataBuffer,",NM,",4,",NM,");

	InsertCurrentDateTime(dataBuffer,0);
	InsertCurrentDateTime(dataBuffer,1);

	if(GPS.GPSFix)
		strcat(dataBuffer,",A,");
	else
		strcat(dataBuffer,",V,");

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
	
	strcat(dataBuffer,"G,");
	InsertIntValue(dataBuffer,DeltaDis,"%02d");
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.PhoneNumber.Mob1,12,9,"NO_NUMBER");
	crc = CRC8(dataBuffer,strlen(dataBuffer));
	sprintf(ss,"*%02X",crc);
	//sprintf(ss,"*");
	strcat(dataBuffer,ss);
	
}

#elif defined(PROTO_NIC1)

void EmergencyPacket(uint8_t IsOff)
{
	char ss[20];
	uint16_t crc;
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	if(IsOff)
		strcat(dataBuffer,"$EPB,EMR,");
	else
		strcat(dataBuffer,"$EPB,SEM,");

	strncat(dataBuffer,NetWork.IMEI,15);

	if(IsStored)
		AppendFixString(dataBuffer,",SP,",4,",SP,");
	else
		AppendFixString(dataBuffer,",NM,",4,",NM,");

	InsertCurrentDateTime(dataBuffer,0);
	InsertCurrentDateTime(dataBuffer,1);

	if(GPS.GPSFix)
		strcat(dataBuffer,",A,");
	else
		strcat(dataBuffer,",V,");

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
	strcat(dataBuffer,",G,");
	strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.PhoneNumber.Mob1,12,9,"NO_NUMBER");
	InsertChar(dataBuffer,'*');
	crc = chksum(dataBuffer,strlen(dataBuffer));
	sprintf(ss,"%04X",crc);
	strcat(dataBuffer,ss);
	
}

#elif defined(PROTO_CDAC)

uint16_t MakeCriticalString(uint8_t IsURE)
{
	nwy_dbg_log("crir chk");
	uint16_t n, cn=0;
	uint16_t dLen=8;
	IsCritical=0;
	memset(SendString,0,DATA_MAX_BUFF);	
	memset(SendString,'0',120);
	memset(&CriticalString[0][0],0x00,CRITICAL_MAX_BUFF);
	memset(&CriticalString[1][0],0x00,CRITICAL_MAX_BUFF);
	memset(&CriticalString[2][0],0x00,CRITICAL_MAX_BUFF);
	memset(&CriticalString[3][0],0x00,CRITICAL_MAX_BUFF);
	memset(&CriticalString[4][0],0x00,CRITICAL_MAX_BUFF);
	InsertStringValue("vltdata=",0,8,0);
	for(int i=0;i<ALERT_COUNT;i++)
	{
		if(VTAlertHeaderType[i] > 1)
			continue;
		if(VAlert[i].Enable)
		{
			if(!IsURE)
			{
				if((VAlert[i].ContMode != ALT_CONT_NORMAL) || SOS.IsSOS || SOS.IsSOSTamper || VAlert[i].AlertSent==0)
					continue;
			}
			else
			{
				if(VAlert[i].AlertSent)	
				{
					if(VAlert[i].ContMode == ALT_CONT_NONE)
						continue;

					// For continuous alerts, check the specific condition for each alert type
					// SOS/Tamper: continue while SOS.IsSOS or SOS.IsSOSTamper
					// Tilt: continue while PeriPheralVal.IsTilt
					// Overspeed: continue while IsOverSpeed (geofence handled at packet build time)
					if(VAlert[i].ContMode == ALT_CONT_NORMAL) {
						// Check if the specific continuous condition is still active
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
			
			// For OVER_SPEED_ALERT: Use AlertID 20 if inside geofence, else 17
			#ifdef PROTO_CDAC
			if(i == OVER_SPEED_ALERT && CheckIfInside())
			{
				InsertStringValue("20", dLen + 18, 2, 1);  // Geofence Overspeed AlertID
				// Add geofence ID as ACK data
				char gfAck[17];
				memset(gfAck, 0, 17);
				sprintf(gfAck, "%05d", GetInsideGeofenceID());
				InsertStringValue(gfAck, dLen + _REGULAR_SIZE, strlen(gfAck), 1);
				SendString[dLen + _REGULAR_SIZE + strlen(gfAck)] = 0;
			}
			else
			#endif
			{
				InsertStringValue(VAlert[i].ID,dLen + 18,2,1);
				if(VAlert[i].WithACK)
				{
					InsertStringValue(VAlert[i].ACK,dLen + _REGULAR_SIZE,strlen(VAlert[i].ACK),1);
					n=strlen(VAlert[i].ACK);
					SendString[dLen+_REGULAR_SIZE+n]=0;
				}
				else
					SendString[dLen+_REGULAR_SIZE]=0;
			}
			SendString[dLen + 20]='L';
			DataPacket();
			strcpy(&CriticalString[cn][0],SendString);
			cn++;
			RemoveNonRepeatAlert(i);
			VAlert[i].AlertSent=1;
			if(cn >= CRITICAL_MAX_BUFF)
				break;
		}
	}
	return cn;
}


uint16_t MakeAlertString(void)
{
	nwy_dbg_log("alt chk (SOS=%d)", SOS.IsSOS);
	uint16_t n, cn=0;
	uint16_t dLen=8;
	IsCritical=0;
	memset(SendString,0,DATA_MAX_BUFF);	
	memset(SendString,'0',120);
	memset(&CriticalString[0][0],0x00,CRITICAL_MAX_BUFF);
	memset(&CriticalString[1][0],0x00,CRITICAL_MAX_BUFF);
	memset(&CriticalString[2][0],0x00,CRITICAL_MAX_BUFF);
	memset(&CriticalString[3][0],0x00,CRITICAL_MAX_BUFF);
	memset(&CriticalString[4][0],0x00,CRITICAL_MAX_BUFF);
	InsertStringValue("vltdata=",0,8,0);
	
	// Log MAINS_RES status for debugging
	if(VAlert[MAINS_RES_ALERT].Enable) {
		nwy_dbg_log("MAINS_RES_ALERT enabled, AlertSent=%d", VAlert[MAINS_RES_ALERT].AlertSent);
	}
	
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
				InsertStringValue(VAlert[i].ACK,dLen + _REGULAR_SIZE,strlen(VAlert[i].ACK),1);
				n=strlen(VAlert[i].ACK);
				SendString[dLen+_REGULAR_SIZE+n]=0;
			}
			else
			{
				// For ACK packets (OTA param change), always end with * per CDAC spec 6.6
				if(strcmp(VAlert[i].Header, "ACK") == 0)
				{
					SendString[dLen+_REGULAR_SIZE]='*';
					SendString[dLen+_REGULAR_SIZE+1]=0;
				}
				else
					SendString[dLen+_REGULAR_SIZE]=0;
			}
			DataPacket();
			strcpy(&CriticalString[cn][0],SendString);
			cn++;
			RemoveNonRepeatAlert(i);
			VAlert[i].AlertSent = 1;  // Mark as sent to prevent duplicate sending
			if(cn >= CRITICAL_MAX_BUFF)
				break;
		}
	}
	
	return cn;
}
#elif defined(PROTO_OG)

void EmergencyPacket(uint8_t IsOff)
{
	char ss[20];
	uint8_t crc;

	// if(prevLat==0)
	// 	DeltaDis=0;
	// else
	// 	DeltaDis = calculateDistance(prevLat,prevLong,GPS.Latitude,GPS.Longitude);

	prevLat = GPS.Latitude;
	prevLong = GPS.Longitude;

	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	if(IsOff)
		strcat(dataBuffer,"$EPB,EMR,");
	else
		strcat(dataBuffer,"$EPB,SEM,");

	strncat(dataBuffer,NetWork.IMEI,15);

	if(IsStored)
		AppendFixString(dataBuffer,",SP,",4,",SP,");
	else
		AppendFixString(dataBuffer,",NM,",4,",NM,");

	InsertCurrentDateTime(dataBuffer,0);
	InsertCurrentDateTime(dataBuffer,1);

	if(GPS.GPSFix)
		strcat(dataBuffer,",A,");
	else
		strcat(dataBuffer,",V,");

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
	
	strcat(dataBuffer,"G,");
	InsertIntValue(dataBuffer,DeltaDis,"%02d");
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.PhoneNumber.Mob1,14,9,"NO_NUMBER");
	crc = CRC8(dataBuffer,strlen(dataBuffer));
	sprintf(ss,",*,%02X",crc);
	//sprintf(ss,"*");
	strcat(dataBuffer,ss);
	
}


#else

void EmergencyPacket(uint8_t IsOff) // ODISA
{
	char ss[20];
	uint32_t crc;
	float dis;
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	if(IsOff)
		strcat(dataBuffer,"$EPB,EMR,");
	else
		strcat(dataBuffer,"$EPB,SEM,");

	strncat(dataBuffer,NetWork.IMEI,15);

	if(IsStored)
		AppendFixString(dataBuffer,",SP,",4,",SP,");
	else
		AppendFixString(dataBuffer,",NM,",4,",NM,");

	InsertCurrentDateTime(dataBuffer,0);
	InsertCurrentDateTime(dataBuffer,1);

	if(GPS.GPSFix)
		strcat(dataBuffer,",A,");
	else
		strcat(dataBuffer,",V,");

	AppendFixString(dataBuffer,sLatitude,10,sLatitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LatDir);
	InsertChar(dataBuffer,',');
	AppendFixString(dataBuffer,sLongitude,10,sLongitude);
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.LngDir);
	InsertChar(dataBuffer,',');
	sprintf(ss,"%06.3f",GPS.Altitude);
	strcat(dataBuffer,ss);
	InsertChar(dataBuffer,',');
	sprintf(ss,"%02f",GPS.Speed);
	strcat(dataBuffer,ss);
	InsertChar(dataBuffer,',');
	dis = DeltaDis;
	sprintf(ss,"%05.1f",dis);
	strcat(dataBuffer,ss);
	strcat(dataBuffer,",G,");
	strcat(dataBuffer,VTSData.VehicleData.VehicleRegNo);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.PhoneNumber.Mob1,12,9,"0");
	//InsertChar(dataBuffer,'0');
	InsertChar(dataBuffer,',');
	#ifdef ODISA_LD
	crc = CRC16(dataBuffer,strlen(dataBuffer));
	sprintf(ss,"%04X*",crc);
	strcat(dataBuffer,ss);
	#else
	crc = CRC16(dataBuffer,strlen(dataBuffer));
	sprintf(ss,"%03X*",crc);
	strcat(dataBuffer,ss);
	#ifndef ODISA_LD
    InsertChar(dataBuffer,'\n');
    #endif
	#endif
}

#endif

#ifdef PROTO_MAHARASHTRA1


void HealthPacket(void)
{
	GetMemeryPercentage();
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcat(dataBuffer,"$HLP,");
	strcat(dataBuffer,VTSData.VendorID);
	InsertChar(dataBuffer,',');
	//INV,");
	strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	strncat(dataBuffer,NetWork.IMEI,15);
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
		strcat(dataBuffer,"1*");
	else
		strcat(dataBuffer,"0*");
	
}
#elif defined(PROTO_CDAC)

/**
 * @brief Build Health Packet (HLM) for CDAC protocol
 * Format: vltdata=HLM<VendorID><FirmVer><IMEI><MotionInt><HaltInt><BattPerc>...
 */
void HealthPacket(void)
{
	uint16_t dLen = CDAC_BASE_OFFSET;
	
	memset(SendString, 0, DATA_MAX_BUFF);	
	memset(SendString, '0', CDAC_HEALTH_PACKET_SIZE);
	
	/* Header and prefix */
	InsertStringValue(CDAC_HTTP_PREFIX, 0, CDAC_HTTP_PREFIX_LEN, 0);
	InsertStringValue(CDAC_HDR_HEALTH, dLen + HLM_HEADER_POS, CDAC_HEADER_SIZE, 0);
	
	/* Device identification */
	InsertStringValue(VTSData.VendorID, dLen + HLM_VENDOR_ID_POS, CDAC_VENDOR_ID_SIZE, 1);
	InsertStringValue(FirmVer, dLen + HLM_FIRMWARE_VER_POS, CDAC_FIRMWARE_VER_SIZE, 1);
	InsertStringValue(NetWork.IMEI, dLen + HLM_IMEI_POS, CDAC_IMEI_SIZE, 1);
	
	/* Intervals */
	InsertIntValue(VTSData.IntervalData.MotionInterval, dLen + HLM_MOTION_INTERVAL_POS, 3);
	InsertIntValue(VTSData.IntervalData.HaltInterval, dLen + HLM_HALT_INTERVAL_POS, 3);
	
	/* Battery */
	InsertIntValue((uint16_t)PeriPheralVal.BattPerc, dLen + HLM_BATT_PERCENT_POS, 3);
	InsertIntValue(batteryVoltageToPercentage(VTSData.BattThrs), dLen + HLM_LOW_BATT_THRS_POS, 2);
	
	/* Memory percentage (fixed value) */
	InsertStringValue("060", dLen + HLM_MEMORY_PERCENT_POS, 3, 0);
	
	/* Digital I/O status */
	SendString[dLen + HLM_INPUT1_POS] = PeriPheralVal.IP1 + '0';
	SendString[dLen + HLM_INPUT2_POS] = PeriPheralVal.IP2 + '0';
	SendString[dLen + HLM_OUTPUT1_POS] = PeriPheralVal.OP1 + '0';
	SendString[dLen + HLM_OUTPUT2_POS] = PeriPheralVal.OP2 + '0';
	
	/* Reserved field */
	InsertStringValue("01", dLen + HLM_RESERVED_POS, 2, 0);
	
	/* Date/Time */
	InsertCurrentDateTime(dLen + HLM_DATETIME_POS);
	
	SendString[dLen + HLM_PACKET_SIZE] = '\0';
}
#elif defined(PROTO_OG)

void HealthPacket(void)
{
	GetMemeryPercentage();
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcat(dataBuffer,"$,HP,");
	strcat(dataBuffer,VTSData.VendorID);
	InsertChar(dataBuffer,',');
	//INV,");
	strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	strncat(dataBuffer,NetWork.IMEI,15);
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

	InsertFloatValue(dataBuffer,PeriPheralVal.AN1,"%04.1f,");
	InsertFloatValue(dataBuffer,PeriPheralVal.AN2,"%04.1f*");
	
	// crc = checksum32(dataBuffer,strlen(dataBuffer));
	// sprintf(ss,"%08X*",crc);
	// lastcrc= crc;
	// strcat(dataBuffer,ss);
	
}


#else

void HealthPacket(void)
{
	GetMemeryPercentage();
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	strcat(dataBuffer,"$HEL,");
	strcat(dataBuffer,VTSData.VendorID);
	InsertChar(dataBuffer,',');
	//INV,");
	strcat(dataBuffer,FirmVer);
	InsertChar(dataBuffer,',');
	strncat(dataBuffer,NetWork.IMEI,15);
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
		strcat(dataBuffer,"1*");
	else
		strcat(dataBuffer,"0*");
	
	#ifndef ODISA_LD
    InsertChar(dataBuffer,'\n');
    #endif
	// crc = checksum32(dataBuffer,strlen(dataBuffer));
	// sprintf(ss,"%08X*",crc);
	// strcat(dataBuffer,ss);
	
}
#endif

#ifdef PROTO_MAHARASHTRA1


void MakeParamChangeString(char* Sender, char* param, uint8_t IsServer)
{

	char ss[20];
	uint16_t i;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	memset(dataBuffer,0,DATA_MAX_BUFF);
	sprintf(dataBuffer,"$NMP,%s,",VTSData.VendorID);
	strcat(dataBuffer,FirmVer);

	strcat(dataBuffer,",OT,12,L,"); // OTA 
	
	strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.GPSFix + '0');
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
	strcat(dataBuffer,NetWork.Network);
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
		InsertChar(dataBuffer,'-');
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
	if(SOS_STATE)
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
		strcat(dataBuffer,Sender);
		InsertChar(dataBuffer,',');
		strcat(dataBuffer,param);
		InsertChar(dataBuffer,',');
		strcat(dataBuffer,CMD_Buff);
		InsertChar(dataBuffer,')');
	}
	else
	{
		InsertChar(dataBuffer,'(');
		strcat(dataBuffer,Sender);
		InsertChar(dataBuffer,',');
		strcat(dataBuffer,param);
		InsertChar(dataBuffer,',');
		strcat(dataBuffer,"1)");
	}

	

	
	uint8_t checksum = SimpleChecksum(dataBuffer, strlen(dataBuffer));
	sprintf(ss,"*%02X",checksum);
	//sprintf(ss,"*");
	strcat(dataBuffer,ss);
	FrameNumber++;

}
#elif defined(PROTO_CDAC)
#define SMS_ALERT_ENABLED
void SMSAlert(uint8_t AlertNum)
{
	//for now we disable it 
	#ifndef SMS_ALERT_ENABLED
	return;
	#else
	// SMS uses GSM signaling channel, independent of GPRS data channel
	// No need to wait for IsSendProcess - can send SMS while TCP in progress
	if(GSM.GSMState < GPRS_INIT)
		return;
	
	// First SMS: Alert code format to Mob0
	memset(SimData,0x00,MSGSIZE);
	InsertIntValue_OLD(SimData,AlertNum,"%02d");
	InsertChar(SimData,',');
	strncat(SimData,NetWork.IMEI,15);
	InsertChar(SimData,',');
	strcat(SimData,VTSData.VehicleData.VehicleRegNo);
	InsertChar(SimData,',');
	InsertCurrentDateTime_OLD(SimData,0);
	InsertChar(SimData,',');
	InsertCurrentDateTime_OLD(SimData,1);
	InsertChar(SimData,',');
	strcat(SimData,sLatitude);
	InsertChar(SimData,',');
	InsertChar(SimData,GPS.LatDir);
	InsertChar(SimData,',');
	strcat(SimData,sLongitude);
	InsertChar(SimData,',');
	InsertChar(SimData,GPS.LngDir);
	SendSMS(VTSData.PhoneNumber.Mob0,SimData);

	// Second SMS: Human readable format to Mob1
	memset(SimData,0x00,MSGSIZE);
	switch(AlertNum)
	{
		case 3 : strcpy(SimData,"Main Battery Removed");break;
		case 10: strcpy(SimData,"Emergency State ON");break;
		case 11: strcpy(SimData,"Emergency State OFF");break;
		case 16: strcpy(SimData,"Emergency button wire diconnect");break;
		case 17: strcpy(SimData,"OverSpeed");break;
		case 22: strcpy(SimData,"Vehicle Tilt");break;
		case 20: strcpy(SimData,"Overspeed in Geofence");break;
	}
	InsertChar(SimData,' ');
	strcat(SimData,VTSData.VehicleData.VehicleRegNo);
	InsertChar(SimData,' ');
	StringAdd(SimData,"%02d-%02d-20%02d",CurrentDateTime.Date,CurrentDateTime.Month,CurrentDateTime.Year);
	InsertChar(SimData,' ');
	StringAdd(SimData,"%02d:%02d:%02d",CurrentDateTime.Hour,CurrentDateTime.Min,CurrentDateTime.Sec);
	InsertChar(SimData,' ');
	strcat(SimData,sLatitude);
	InsertChar(SimData,' ');
	strcat(SimData,sLongitude);
	if(!strstr(VTSData.PhoneNumber.Mob1,"0000000") && strlen(VTSData.PhoneNumber.Mob1) > 3)
		SendSMS(VTSData.PhoneNumber.Mob1,SimData);
	// if(!strstr(VTSData.PhoneNumber.Mob2,"0000000") && strlen(VTSData.PhoneNumber.Mob2) > 3)
	// 	SendSMS(VTSData.PhoneNumber.Mob2,SimData);
	// if(!strstr(VTSData.PhoneNumber.Mob3,"0000000") && strlen(VTSData.PhoneNumber.Mob3) > 3)
	// 	SendSMS(VTSData.PhoneNumber.Mob3,SimData);
	// if(!strstr(VTSData.PhoneNumber.Mob4,"0000000") && strlen(VTSData.PhoneNumber.Mob4) > 3)
	// 	SendSMS(VTSData.PhoneNumber.Mob4,SimData);
	#endif
}

/**
 * @brief Build Full Packet (FUL) with Alert ID 25 for CDAC protocol
 * Contains all device info including NMR, voltages, I/O status, CRC
 */
void FullPacket(void)
{
	uint16_t dLen = CDAC_BASE_OFFSET;
	uint16_t i = 0;
	uint16_t pos;
	char tempData[10];
	unsigned short crc;
	
	memset(SendString, '0', DATA_MAX_BUFF);
	
	/* Header and prefix */
	InsertStringValue(CDAC_HTTP_PREFIX, 0, CDAC_HTTP_PREFIX_LEN, 0);
	InsertStringValue(CDAC_HDR_FULL, dLen + FUL_HEADER_POS, CDAC_HEADER_SIZE, 0);
	
	/* IMEI and Alert ID 25 */
	InsertStringValue(NetWork.IMEI, dLen + FUL_IMEI_POS, CDAC_IMEI_SIZE, 1);
	InsertStringValue("25L", dLen + FUL_ALERT_ID_POS, 3, 0);
	
	/* GPS Fix and DateTime */
	SendString[dLen + FUL_GPS_FIX_POS] = GPS.GPSFix + '0';
	InsertCurrentDateTime(dLen + FUL_DATETIME_POS);
	
	/* Position */
	InsertFloatValue(GPS.Latitude, dLen + FUL_LATITUDE_POS, CDAC_LAT_SIZE, "%010.6f");
	SendString[dLen + FUL_LAT_DIR_POS] = GPS.LatDir;
	InsertFloatValue(GPS.Longitude, dLen + FUL_LONGITUDE_POS, CDAC_LON_SIZE, "%010.6f");
	SendString[dLen + FUL_LON_DIR_POS] = GPS.LngDir;
	
	/* Cell Info */
	InsertIntValue(GSM.MCC, dLen + FUL_MCC_POS, CDAC_MCC_SIZE);
	InsertIntValue(GSM.MNC, dLen + FUL_MNC_POS, CDAC_MNC_SIZE);
	InsertStringValue(GSM.LAC, dLen + FUL_LAC_POS, CDAC_LAC_SIZE, 1);
	InsertStringValue(GSM.CellID, dLen + FUL_CELLID_POS, CDAC_CELLID_SIZE, 1);
	
	/* Speed and Heading */
	InsertFloatValue(GPS.Speed, dLen + FUL_SPEED_POS, CDAC_SPEED_SIZE, "%06.2f");
	InsertFloatValue(GPS.Heading, dLen + FUL_HEADING_POS, CDAC_HEADING_SIZE, "%06.2f");
	
	/* GPS Quality */
	InsertIntValue(GPS.NoOfSatalite, dLen + FUL_SATS_POS, CDAC_SATS_SIZE);
	InsertIntValue((uint16_t)GPS.HDOP, dLen + FUL_HDOP_POS, CDAC_HDOP_SIZE);
	InsertIntValue(GSM.SignalStrength, dLen + FUL_SIGNAL_POS, CDAC_SIGNAL_SIZE);
	
	/* Vehicle Status */
	SendString[dLen + FUL_IGN_POS] = PeriPheralVal.IGN + '0';
	SendString[dLen + FUL_MAIN_POWER_POS] = PeriPheralVal.IsMain + '0';
	SendString[dLen + FUL_VEHICLE_MODE_POS] = VehicleMovingMode;
	
	/* Device Info */
	InsertStringValue(VTSData.VendorID, dLen + FUL_VENDOR_ID_POS, CDAC_VENDOR_ID_SIZE, 1);
	InsertStringValue(FirmVer, dLen + FUL_FIRMWARE_VER_POS, CDAC_FIRMWARE_VER_SIZE, 1);
	InsertStringValue(VTSData.VehicleData.VehicleRegNo, dLen + FUL_VEHICLE_REG_POS, CDAC_VEHICLE_REG_SIZE, 1);
	
	/* Altitude and PDOP */
	InsertFloatValue(GPS.Altitude, dLen + FUL_ALTITUDE_POS, CDAC_ALTITUDE_SIZE, "%07.2f");
	InsertIntValue((uint16_t)GPS.PDOP, dLen + FUL_PDOP_POS, 2);
	InsertStringValue(NetWork.Network, dLen + FUL_NETWORK_NAME_POS, CDAC_NETWORK_NAME_SIZE, 1);
	
	/* NMR - Neighbour Cell Info (4 cells x 15 bytes each) */
	pos = dLen + FUL_NMR_POS;
	while(i < 4)
	{
		InsertStringValue(GSM.NeigbourCell[i].CellDB, pos, 2, 1);
		pos += 2;
		InsertStringValue(GSM.NeigbourCell[i].LAC, pos, CDAC_LAC_SIZE, 1);
		pos += CDAC_LAC_SIZE;
		InsertStringValue(GSM.NeigbourCell[i++].CellID, pos, CDAC_CELLID_SIZE, 1);
		pos += CDAC_CELLID_SIZE;
	}
	
	/* Voltages */
	InsertFloatValue(PeriPheralVal.MainsVolt, dLen + FUL_MAIN_VOLTAGE_POS, 5, "%05.1f");
	InsertFloatValue(PeriPheralVal.BattVolt, dLen + FUL_BATT_VOLTAGE_POS, 5, "%05.1f");
	
	/* Tamper and Digital I/O */
	SendString[dLen + FUL_TAMPER_POS] = PeriPheralVal.IsCoverOpen ? 'O' : 'C';
	SendString[dLen + FUL_DIG_INPUT1_POS] = PeriPheralVal.IP1 + '0';
	SendString[dLen + FUL_DIG_INPUT2_POS] = PeriPheralVal.IP2 + '0';
	SendString[dLen + FUL_DIG_OUTPUT1_POS] = PeriPheralVal.OP1 + '0';
	SendString[dLen + FUL_DIG_OUTPUT2_POS] = PeriPheralVal.OP2 + '0';
	
	/* Frame Number and CRC */
	InsertIntValue(FrameNumber, dLen + FUL_FRAME_NUM_POS, CDAC_FRAME_NUM_SIZE);
	crc = CRC16(&SendString[dLen], FUL_CRC_POS);
	sprintf(tempData, "%08X", crc);
	InsertStringValue(tempData, dLen + FUL_CRC_POS, CDAC_CRC_SIZE, 1);
	SendString[dLen + FUL_PACKET_SIZE] = '\0';
	
	FrameNumber++;
}

/**
 * @brief Build Batch Packet (BTH) for CDAC protocol
 * Format: BTH<IMEI><Count><LivePacket><HistoryPackets...>
 * @param count Number of history packets to include
 * @param nmcount Output: number of normal packets included
 * @param critcount Output: number of critical/alert packets included
 */
void MakeBatchPacket(uint8_t count, uint8_t* nmcount, uint8_t* critcount)
{
	uint16_t dLen = CDAC_BASE_OFFSET;
	
	memset(SendString, 0, DATA_MAX_BUFF);
	memset(SendString, '0', BTH_LIVE_PACKET_SIZE + CDAC_BASE_OFFSET);
	
	/* Header and prefix */
	InsertStringValue(CDAC_HTTP_PREFIX, 0, CDAC_HTTP_PREFIX_LEN, 0);
	InsertStringValue(CDAC_HDR_BATCH, dLen + BTH_HEADER_POS, CDAC_HEADER_SIZE, 0);
	
	/* IMEI and batch count (includes live packet) */
	InsertStringValue(NetWork.IMEI, dLen + BTH_IMEI_POS, CDAC_IMEI_SIZE, 0);
	InsertIntValue(count + 1, dLen + BTH_COUNT_POS, CDAC_BATCH_COUNT_SIZE);
	
	/* Live packet status */
	InsertStringValue("01L", dLen + BTH_STATUS_POS, 3, 0);
	
	/* GPS Fix and DateTime */
	SendString[dLen + BTH_GPS_FIX_POS] = GPS.GPSFix + '0';
	InsertCurrentDateTime(dLen + BTH_DATETIME_POS);
	
	/* Position */
	InsertFloatValue(GPS.Latitude, dLen + BTH_LATITUDE_POS, CDAC_LAT_SIZE, "%010.6f");
	SendString[dLen + BTH_LAT_DIR_POS] = GPS.LatDir;
	InsertFloatValue(GPS.Longitude, dLen + BTH_LONGITUDE_POS, CDAC_LON_SIZE, "%010.6f");
	SendString[dLen + BTH_LON_DIR_POS] = GPS.LngDir;
	
	/* Cell Info */
	InsertIntValue(GSM.MCC, dLen + BTH_MCC_POS, CDAC_MCC_SIZE);
	InsertIntValue(GSM.MNC, dLen + BTH_MNC_POS, CDAC_MNC_SIZE);
	InsertStringValue(GSM.LAC, dLen + BTH_LAC_POS, CDAC_LAC_SIZE, 1);
	InsertStringValue(GSM.CellID, dLen + BTH_CELLID_POS, CDAC_CELLID_SIZE, 1);
	
	/* Speed and Heading */
	InsertFloatValue(GPS.Speed, dLen + BTH_SPEED_POS, CDAC_SPEED_SIZE, "%03.2f");
	InsertFloatValue(GPS.Heading, dLen + BTH_HEADING_POS, CDAC_HEADING_SIZE, "%03.2f");
	
	/* GPS Quality */
	InsertIntValue(GPS.NoOfSatalite, dLen + BTH_SATS_POS, CDAC_SATS_SIZE);
	InsertIntValue((uint16_t)GPS.HDOP, dLen + BTH_HDOP_POS, CDAC_HDOP_SIZE);
	InsertIntValue(GSM.SignalStrength, dLen + BTH_SIGNAL_POS, CDAC_SIGNAL_SIZE);
	
	/* Vehicle Status */
	SendString[dLen + BTH_IGN_POS] = PeriPheralVal.IGN + '0';
	SendString[dLen + BTH_MAIN_POWER_POS] = PeriPheralVal.IsMain + '0';
	SendString[dLen + BTH_VEHICLE_MODE_POS] = VehicleMovingMode;
	
	/* Altitude */
	InsertFloatValue(GPS.Altitude, dLen + BTH_ALTITUDE_POS, CDAC_ALTITUDE_SIZE, "%04.2f");
	
	/* Network Operator Name (padded with 'X') */
	char spn[7];
	sprintf(spn, "%s", NetWork.Network);
	int rem = CDAC_NETWORK_NAME_SIZE - strlen(NetWork.Network);
	for(int j = 0; j < rem; j++)
		spn[CDAC_NETWORK_NAME_SIZE - 1 - j] = 'X';
	InsertStringValue(spn, dLen + BTH_NETWORK_NAME_POS, CDAC_NETWORK_NAME_SIZE, 1);
	
	/* Append history packets from flash storage */
	/* We need to sort by criticality: EPB(0) > CRT(1) > ALT(2) > ACK(3) > NRM */
	
	char* hpacket = malloc(CDAC_MAX_PACKET_SIZE);
	if(!hpacket)
	{
		nwy_dbg_log("hpacket malloc fail!");
		return;
	}
	
	// Temporary storage for history packets before sorting
	// Each entry: packet data pointer (after IMEI), priority (0=highest)
	#define MAX_HIST_PACKETS 2
	char* histPackets[MAX_HIST_PACKETS];
	uint8_t histPriority[MAX_HIST_PACKETS];  // 0=EPB, 1=CRT, 2=ALT, 3=ACK/NRM
	uint8_t histCount = 0;
	
	// Allocate buffers for history packets
	for(int h = 0; h < MAX_HIST_PACKETS; h++) {
		histPackets[h] = malloc(CDAC_MAX_PACKET_SIZE);
		if(!histPackets[h]) {
			nwy_dbg_log("histPackets[%d] malloc fail!", h);
			// Free already allocated
			for(int f = 0; f < h; f++) free(histPackets[f]);
			free(hpacket);
			return;
		}
		histPackets[h][0] = '\0';
		histPriority[h] = 255;
	}
	
	*nmcount = 0;
	*critcount = 0;
	
	// First, collect all history packets from storage (ALERT type first, then NORMAL)
	uint8_t IsCrit = 1, nmindex = 0, critindex = 0;
	int collected = 0;
	
	while(collected < count && histCount < MAX_HIST_PACKETS)
	{
		int status = 0;
		memset(hpacket, 0, CDAC_MAX_PACKET_SIZE);
		
		if(IsCrit) {
			status = ReadDataBatch(hpacket, CDAC_STORAGE_TYPE_ALERT, 1, critindex++, 0);
			nwy_dbg_log("ReadDataBatch ALERT[%d] status=%d", critindex-1, status);
			if(!status) {
				IsCrit = 0;
				continue;
			}
		}
		else {
			status = ReadDataBatch(hpacket, CDAC_STORAGE_TYPE_NORMAL, 1, nmindex++, 0);
			if(!status) {
				collected++;  // No more packets
				continue;
			}
		}
		
		if(status)
		{
			/* Find IMEI position and validate packet */
			char* pt = strstr(hpacket, NetWork.IMEI);
			int pklen = strlen(hpacket);
			
			if(!pt) {
				nwy_dbg_log("invalid history packet:");
				nwy_dbg_log("%s", hpacket);
				continue;
			}
			if(pklen < 50 || pklen > CDAC_MAX_PACKET_SIZE) {
				nwy_dbg_log("invalid history packet len %d", pklen);
				nwy_dbg_log("%s", hpacket);
				continue;
			}
			
			/* Skip past IMEI - now pt points to AlertID position (18) */
			pt += CDAC_IMEI_SIZE;
			
			/* Extract AlertID FIRST before modifying the buffer */
			/* pt[0], pt[1] = AlertID (positions 18-19) */
			/* pt[2] = Status character (position 20): 'L' for Live, 'H' for History */
			char alertIdStr[3] = {pt[0], pt[1], '\0'};
			
			/* Validate AlertID is numeric - if corrupted, set to 02 (History) to clear from storage */
			/* Manual digit check since isdigit() from ctype.h not available on this platform */
			if(!(pt[0] >= '0' && pt[0] <= '9') || !(pt[1] >= '0' && pt[1] <= '9')) {
				nwy_dbg_log("Invalid AlertID '%c%c' - changing to 02", pt[0], pt[1]);
				pt[0] = '0';
				pt[1] = '2';
				alertIdStr[0] = '0';
				alertIdStr[1] = '2';
			}
			
			/* Validate Status character - must be L or H, default to H */
			if(pt[2] != 'L' && pt[2] != 'H') {
				nwy_dbg_log("Invalid Status '%c' - changing to H", pt[2]);
				pt[2] = 'H';
			}
			
			int alertId = atoi(alertIdStr);
			
			/* Convert AlertID 01 (Live) to 02 (History) for stored packets */
			if(alertId == 1) {
				pt[0] = '0';
				pt[1] = '2';
				alertId = 2;
			}
			
			/* Convert status character from Live to History */
			pt[2] = CDAC_STATUS_CHAR_HISTORY;
			
			/* Map AlertID to priority for batch ordering
			 * Priority groups (same priority = maintain LIFO order within group):
			 * 0 = EPB + CRT (Emergency and Critical - highest priority, same level)
			 * 1 = ALT (Alert packets)
			 * 2 = ACK (Acknowledgments)
			 * 3 = NRM/History (Normal packets - lowest priority)
			 */
			uint8_t priority = 2;  // Default: ACK level
			for(int idx = 0; idx < ALERT_COUNT; idx++) {
				if(atoi(VTAlertPKT[idx]) == alertId) {
					// VTAlertHeaderType: 0=EPB, 1=CRT, 2=ALT, 3=ACK
					uint8_t headerType = VTAlertHeaderType[idx];
					if(headerType <= 1) {
						// EPB (0) and CRT (1) get same priority to preserve LIFO order
						priority = 0;
					} else if(headerType == 2) {
						priority = 1;  // ALT
					} else {
						priority = 2;  // ACK
					}
					break;
				}
			}
			
			/* History packets (AlertID 02) get lowest priority */
			if(alertId == 2) priority = 3;
			
			nwy_dbg_log("History packet AlertID=%02d priority=%d", alertId, priority);
			
			/* Store packet and priority */
			strcpy(histPackets[histCount], pt);
			histPriority[histCount] = priority;
			histCount++;
			
			/* Count based on STORAGE TYPE, not priority - this must match delete logic */
			/* IsCrit indicates if packet was read from ALERT(3) or NORMAL(0) storage */
			if(IsCrit)
				(*critcount)++;
			else
				(*nmcount)++;
			
			collected++;
		}
	}
	
	nwy_dbg_log("Collected %d history packets, sorting by priority...", histCount);
	
	/* Sort history packets by priority (simple bubble sort - max 5 elements) */
	for(int a = 0; a < histCount - 1; a++) {
		for(int b = a + 1; b < histCount; b++) {
			if(histPriority[b] < histPriority[a]) {
				// Swap
				char* tmpPkt = histPackets[a];
				histPackets[a] = histPackets[b];
				histPackets[b] = tmpPkt;
				
				uint8_t tmpPri = histPriority[a];
				histPriority[a] = histPriority[b];
				histPriority[b] = tmpPri;
			}
		}
	}
	
	/* Append sorted history packets to batch */
	for(int h = 0; h < histCount; h++) {
		nwy_dbg_log("Appending history packet %d (priority=%d)", h, histPriority[h]);
		strcat(SendString, histPackets[h]);
	}
	
	/* Cleanup */
	for(int h = 0; h < MAX_HIST_PACKETS; h++) {
		free(histPackets[h]);
	}
	free(hpacket);
	
	nwy_dbg_log("Total Batch hPacket - crit: %d, non-crit: %d ", *critcount, *nmcount);
}

// void UpdateOTA(char* data, uint8_t IsSet)
// {
// 	char* fn = data+4;
// 	memset(&OTAValue,0,sizeof(OTAValue));
// 	uint16_t ln=strlen(fn);
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
void UpdateOTA(char* requestData, uint8_t isSetCommand)
{
    char* currentChar = requestData + 4;  // Skip "SET " or "GET "
    memset(&OTAValue, 0, sizeof(OTAValue));

    uint8_t charIndex = 0; // Index for constructing keys or values
    uint8_t isParsingValue = 0; // 0 = parsing key, 1 = parsing value

    while (*currentChar && OTAValue.TotalOTA < MAX_OTA_SIZE)
    {
        if (*currentChar == ':' && !isParsingValue) // First `:` switches to value parsing
        {
            isParsingValue = 1;
            OTAValue.OTData[OTAValue.TotalOTA].KeyVal[charIndex] = '\0';
            charIndex = 0;
            currentChar++;
            continue;
        }
        else if (*currentChar == ',' || *currentChar == '\0' || *currentChar == '\r' || *currentChar == '\n') // End of key-value pair
        {
            if (isParsingValue)
            {
                OTAValue.OTData[OTAValue.TotalOTA].Value[charIndex] = '\0';
            }
            else
            {
                OTAValue.OTData[OTAValue.TotalOTA].KeyVal[charIndex] = '\0';
            }

            OTAValue.TotalOTA++;
            isParsingValue = 0;
            charIndex = 0;
            currentChar++;
            continue;
        }

        // Populate Key or Value based on the state
        if (!isParsingValue)
        {
            OTAValue.OTData[OTAValue.TotalOTA].KeyVal[charIndex++] = *currentChar;
        }
        else
        {
            OTAValue.OTData[OTAValue.TotalOTA].Value[charIndex++] = *currentChar;
        }

        currentChar++;
    }

    // Handle last key or value without a trailing delimiter
    if (charIndex > 0)
    {
        if (isParsingValue)
        {
            OTAValue.OTData[OTAValue.TotalOTA].Value[charIndex] = '\0';
        }
        else
        {
            OTAValue.OTData[OTAValue.TotalOTA].KeyVal[charIndex] = '\0';
        }
        OTAValue.TotalOTA++;
    }
}
void UpdateURL(char* value)
{
	char* fn;
	char url[128];
	strcpy(url,value);
	fn = strchr(url,':');
	if(fn)
	{
		*fn=0;
		fn++;
		strncpy(VTSData.ServerData.Port1,fn,sizeof(VTSData.ServerData.Port1)-1);
	}
	strncpy(VTSData.ServerData.IP1,url,sizeof(VTSData.ServerData.IP1)-1);
    ServerSocket[0].SocketState = SOCKET_CLOSED;
    strcpy(ServerSocket[0].DNSorIP,VTSData.ServerData.IP1);
    ServerSocket[0].Port = atoi(VTSData.ServerData.Port1);
}

void UpdateSecondaryURL(char* value)
{
	char* fn;
	char url[128];
	strcpy(url,value);
	fn = strchr(url,':');
	if(fn)
	{
		*fn=0;
		fn++;
		strncpy(VTSData.ServerData.Port3,fn,sizeof(VTSData.ServerData.Port3)-1);
	}
	strncpy(VTSData.ServerData.IP3,url,sizeof(VTSData.ServerData.IP3)-1);
	ServerSocket[2].SocketState = SOCKET_CLOSED;
    strcpy(ServerSocket[2].DNSorIP,VTSData.ServerData.IP3);
    ServerSocket[2].Port = atoi(VTSData.ServerData.Port3);
}

void UpdateVehicleNumber(char* value)
{
	strncpy(VTSData.VehicleData.VehicleRegNo,value,sizeof(VTSData.VehicleData.VehicleRegNo)-1);
}

void UpdateMoblieNo(char* value, uint8_t num)
{
	uint16_t j=strlen(value);
	if((j >3) && (j < 21))
	{
		switch(num)
		{
			case 1:
				strncpy(VTSData.PhoneNumber.Mob0,value,sizeof(VTSData.PhoneNumber.Mob0)-1);
				break;
			case 2:
				strncpy(VTSData.PhoneNumber.Mob1,value,sizeof(VTSData.PhoneNumber.Mob1)-1);
				break;
			case 3:
				strncpy(VTSData.PhoneNumber.Mob2,value,sizeof(VTSData.PhoneNumber.Mob2)-1);
				break;
			case 4:
				strncpy(VTSData.PhoneNumber.Mob3,value,sizeof(VTSData.PhoneNumber.Mob3)-1);
				break;
			case 5:
				strncpy(VTSData.PhoneNumber.Mob4,value,sizeof(VTSData.PhoneNumber.Mob4)-1);
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
	uint16_t i=strlen(value);
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
	nwy_dbg_log("Paring cmd from source %d",isserver);
	fn = strstr(buff,"\r\n\r\n");
	if(fn)
	{
		buff = fn+4;
		nwy_dbg_log("skipped Http headers to %s",buff);
	}
	if(strlen(buff)<3)
		return;

	if(strstr(buff,"XSET")|| strstr(buff,"XGET") || strstr(buff,"XCLR"))
	{
		DecodeSMS(buff,isserver);
		return;
	}

	if(strstr(buff,"ACTV"))
	{
		if(isserver != 0)
		{
			nwy_dbg_log("activation not allowed via Server command!");
			return;
		}
		ls = strchr(buff,',');
		if(!ls)
		{
			strncpy(ss,buff+5,16);
			if(strlen(ss)==16)
			{
				strcpy(ActivationKey,ss);
				nwy_dbg_log("Activation Key %s Recieved, login packet initiated",ActivationKey);
				SendLogin1=1;
			}
			else
				nwy_dbg_log("invalid actv key len! %d, key:%s",strlen(ss),ss);
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
					nwy_dbg_log("\r\nSending ACTV reply with Rc : %s to %s",rnd,ss);
					MakeACTMessage(0,  rnd);
					SendSMS(ss,SimData);
					return;
				}
			}
		}
	}
	if(strstr(buff,"HCHK"))
	{
		if(isserver != 0)
			return;
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
				nwy_dbg_log("\r\nSending HCHK reply with Rc : %s to %s",rnd,ss);
				MakeACTMessage(1,  rnd);
				SendSMS(ss,SimData);
				return;
			}
		}
	}
	memset(VAlert[CONF_CHANGE_ALERT].ACK,0x00,100);
	if(!isserver)
	{
		SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, "OM:");
		SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, SMSSender);
		InsertChar(VAlert[CONF_CHANGE_ALERT].ACK, ',');
	}
	else
	{
		// Force null-terminate IP1 to prevent buffer overrun
		VTSData.ServerData.IP1[sizeof(VTSData.ServerData.IP1)-1] = '\0';
		SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, "OU:");
		SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, VTSData.ServerData.IP1);
		InsertChar(VAlert[CONF_CHANGE_ALERT].ACK, ',');
		nwy_dbg_log("OTA ACK prefix: %s", VAlert[CONF_CHANGE_ALERT].ACK);
	}

	fn=strstr(buff,"SET");
	if(fn)
	{
		nwy_dbg_log("ota set cmd");
		ls=fn;
		if(strstr(fn,"FOTA"))
		{
			FOTAPacket(buff,SMSSender,isserver);
			return;
		}
		if(strstr(fn,"MOTA"))
		{
			MOTAPacket(buff,SMSSender,isserver);
			return;
		}
		ls=fn;
		fn=strstr(fn,"GF:");
		if(fn)
		{
			DecodeGeofence(fn);
			UpdateConfigInFlash();
			return;
		}
		fn = strstr(ls," EO"); // SET EO,MO:238923,UL:something
		if(fn)
		{
			//ResetSOS();
			if(SOS.IsSOS)	
			{
				SOS.SOSTimeLasped=0;
				SOS.IsSOS=0;
				SLED_OFF;
				AddAlert(SOS_OFF_ALERT);
				RemoveAlert(SOS_ON_ALERT);
				SMSAlert(11);
				return;
			}
			// strcat(VAlert[CONF_CHANGE_ALERT].ACK,"EO");
			// InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,':');
			// strcat(VAlert[CONF_CHANGE_ALERT].ACK,"OFF");
			// if(strchr(fn,','))
			// {
			// 	InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,',');
			// 	ls = fn+5;
			// }
			// else
			// {
			// 	if(isserver==0)
			// 		SendSMS(SMSSender,VAlert[CONF_CHANGE_ALERT].ACK);
			// 	strcpy(VAlert[CONF_CHANGE_ALERT].Header,"ACK");
			// 	InsertChar(VAlert[CONF_CHANGE_ALERT].ACK,'*');
			// 	VAlert[CONF_CHANGE_ALERT].WithACK=1;
			// 	AddAlert(CONF_CHANGE_ALERT);
			// 	return;
			// }
		}
		UpdateOTA(ls,1);
		
		if(OTAValue.TotalOTA>0)
		{
			VAlert[CONF_CHANGE_ALERT].Enable=1;
			// ACK header for OTA Parameter Acknowledgment per CDAC spec 6.6
			strcpy(VAlert[CONF_CHANGE_ALERT].Header,"ACK");
			
			for(i=0;i<OTAValue.TotalOTA;i++)
			{
				if(strstr(OTAValue.OTData[i].KeyVal,"PU"))
					UpdateURL(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"SU"))
					UpdateSecondaryURL(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"VN"))
					UpdateVehicleNumber(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"M0"))
					UpdateMoblieNo(OTAValue.OTData[i].Value,1);
				else if(strstr(OTAValue.OTData[i].KeyVal,"M1"))
					UpdateMoblieNo(OTAValue.OTData[i].Value,2);
				else if(strstr(OTAValue.OTData[i].KeyVal,"M2"))
					UpdateMoblieNo(OTAValue.OTData[i].Value,3);
				else if(strstr(OTAValue.OTData[i].KeyVal,"M3")) 
					UpdateMoblieNo(OTAValue.OTData[i].Value,4);
				else if(strstr(OTAValue.OTData[i].KeyVal,"OM"))
					UpdateMoblieNo(OTAValue.OTData[i].Value,5);
				else if(strstr(OTAValue.OTData[i].KeyVal,"ED"))
					UpdateSOSTimeOut(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"ST"))
					UpdateSleepTime(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"HT"))
					UpdateHaltTime(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"DSL"))
					UpdateDefaultSpeedLimit(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"SL"))
					UpdateSpeedLimit(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"HBT"))
					UpdateHarshBreak(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"HAT"))
					UpdateHarshAcck(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"RTT"))
					UpdateRashTurn(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"LBT"))
					UpdateLowBattThr(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"TA"))
					UpdateTiltAngle(OTAValue.OTData[i].Value);
				else if(strstr(OTAValue.OTData[i].KeyVal,"URT"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,1);
				else if(strstr(OTAValue.OTData[i].KeyVal,"URS"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,2);
				else if(strstr(OTAValue.OTData[i].KeyVal,"URE"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,3);
				else if(strstr(OTAValue.OTData[i].KeyVal,"URF"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,4);
				else if(strstr(OTAValue.OTData[i].KeyVal,"URH"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,5);
				else if(strstr(OTAValue.OTData[i].KeyVal,"UR"))
					UpdateOTAInterval(OTAValue.OTData[i].Value,6);
				else if(strstr(OTAValue.OTData[i].KeyVal,"VID"))
					UpdateVID(OTAValue.OTData[i].Value);
				else
					strcpy(OTAValue.OTData[i].Value,"InvalidKey");


				SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, OTAValue.OTData[i].KeyVal);
				InsertChar(VAlert[CONF_CHANGE_ALERT].ACK, ':');
				SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, OTAValue.OTData[i].Value);
				if(i != OTAValue.TotalOTA-1)
					InsertChar(VAlert[CONF_CHANGE_ALERT].ACK, ',');
				VAlert[CONF_CHANGE_ALERT].WithACK=1;
			}
			InsertChar(VAlert[CONF_CHANGE_ALERT].ACK, '*');
			nwy_dbg_log("OTA SET ACK complete: %s", VAlert[CONF_CHANGE_ALERT].ACK);
			if(isserver==0)
				SendSMS(SMSSender,VAlert[CONF_CHANGE_ALERT].ACK);
			AddAlert(CONF_CHANGE_ALERT);
			UpdateConfigInFlash();
			IsPacketReady.IsNormalPacket=1;
		}
	}
	fn=strstr(buff,"GET");
	if(fn)
	{
		nwy_dbg_log("ota get cmd");

		ls=fn;
		UpdateOTA(ls,0);
		if(OTAValue.TotalOTA>0)
		{
			nwy_dbg_log("ota count : %d",OTAValue.TotalOTA);
			VAlert[CONF_CHANGE_ALERT].Enable=1;
			// ACK header for OTA Parameter Acknowledgment per CDAC spec 6.6
			strcpy(VAlert[CONF_CHANGE_ALERT].Header,"ACK");

			for(i=0;i<OTAValue.TotalOTA;i++)
			{
				SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, OTAValue.OTData[i].KeyVal);
				InsertChar(VAlert[CONF_CHANGE_ALERT].ACK, ':');
				char cc[100]={0};

				if(strstr(OTAValue.OTData[i].KeyVal,"PU"))
					sprintf(cc,"%s:%s",VTSData.ServerData.IP1,VTSData.ServerData.Port1);
				else if(strstr(OTAValue.OTData[i].KeyVal,"SU"))
					sprintf(cc,"%s:%s",VTSData.ServerData.IP2,VTSData.ServerData.Port2);
				else if(strstr(OTAValue.OTData[i].KeyVal,"VN"))
					strcpy(cc,VTSData.VehicleData.VehicleRegNo);
				else if(strstr(OTAValue.OTData[i].KeyVal,"M0"))
					strcpy(cc,VTSData.PhoneNumber.Mob0);
				else if(strstr(OTAValue.OTData[i].KeyVal,"M1"))
					strcpy(cc,VTSData.PhoneNumber.Mob1);
				else if(strstr(OTAValue.OTData[i].KeyVal,"M2"))
					strcpy(cc,VTSData.PhoneNumber.Mob2);
				else if(strstr(OTAValue.OTData[i].KeyVal,"M3")) 
					strcpy(cc,VTSData.PhoneNumber.Mob3);
				else if(strstr(OTAValue.OTData[i].KeyVal,"ED")){
					sprintf(cc,"%d",VTSData.IntervalData.SOSTimeOut/60);
				}
				else if(strstr(OTAValue.OTData[i].KeyVal,"ST"))
					sprintf(cc,"%d",VTSData.IntervalData.SleepTime/60);
				else if(strstr(OTAValue.OTData[i].KeyVal,"HT"))
					sprintf(cc,"%d",VTSData.IntervalData.HaltInterval/60);
				else if(strstr(OTAValue.OTData[i].KeyVal,"DSL"))
					sprintf(cc,"%2.0f",VTSData.VehicleData.DefaultSpeed);
				else if(strstr(OTAValue.OTData[i].KeyVal,"SL"))
					sprintf(cc,"%2.0f",VTSData.VehicleData.OverSpeed);
				else if(strstr(OTAValue.OTData[i].KeyVal,"HBT"))
					sprintf(cc,"%d",VTSData.VehicleData.HarshBreak);
				else if(strstr(OTAValue.OTData[i].KeyVal,"HAT"))
					sprintf(cc,"%d",VTSData.VehicleData.HarshAcc);
				else if(strstr(OTAValue.OTData[i].KeyVal,"RTT"))
					sprintf(cc,"%d",VTSData.VehicleData.RashTurn);
				else if(strstr(OTAValue.OTData[i].KeyVal,"LBT"))
					sprintf(cc,"%i",batteryVoltageToPercentage(VTSData.BattThrs));
				else if(strstr(OTAValue.OTData[i].KeyVal,"TA"))
					sprintf(cc,"%d",VTSData.VehicleData.TiltAngle);
				else if(strstr(OTAValue.OTData[i].KeyVal,"URT"))
					sprintf(cc,"%d",VTSData.IntervalData.HaltInterval/60);
				else if(strstr(OTAValue.OTData[i].KeyVal,"URS"))
					sprintf(cc,"%d",VTSData.IntervalData.SleepInterval/60);
				else if(strstr(OTAValue.OTData[i].KeyVal,"URE"))
					sprintf(cc,"%d",VTSData.IntervalData.EnergencyInterval);
				else if(strstr(OTAValue.OTData[i].KeyVal,"URF"))
					sprintf(cc,"%d",VTSData.IntervalData.FullDataPacketInterval/60);
				else if(strstr(OTAValue.OTData[i].KeyVal,"URH"))
					sprintf(cc,"%d",VTSData.IntervalData.HealthInterval/60);
				else if(strstr(OTAValue.OTData[i].KeyVal,"UR"))
					sprintf(cc,"%d",VTSData.IntervalData.MotionInterval);
				else if(strstr(OTAValue.OTData[i].KeyVal,"VID"))
					strcpy(cc,VTSData.VendorID);
				else if(strstr(OTAValue.OTData[i].KeyVal,"FV"))
					strcpy(cc,FirmVer);
				else if(strstr(OTAValue.OTData[i].KeyVal,"GF"))
					GetActiveGeoID(cc);
				else
					strcpy(cc,"InvalidKey");

				SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, cc);
				if(i != OTAValue.TotalOTA-1)
					InsertChar(VAlert[CONF_CHANGE_ALERT].ACK, ',');

			}
			VAlert[CONF_CHANGE_ALERT].WithACK=1;
			InsertChar(VAlert[CONF_CHANGE_ALERT].ACK, '*');
			nwy_dbg_log("OTA GET ACK complete: %s", VAlert[CONF_CHANGE_ALERT].ACK);
			if(isserver==0)
				SendSMS(SMSSender,VAlert[CONF_CHANGE_ALERT].ACK);
			else
			{
				AddAlert(CONF_CHANGE_ALERT);
				IsPacketReady.IsNormalPacket=1;
			}
		}
	}
	fn=strstr(buff,"CLR");
	if(fn)
	{
		nwy_dbg_log("ota clr cmd");
		ls=fn;
		UpdateOTA(ls,0);
		if(OTAValue.TotalOTA>0)
		{
			nwy_dbg_log("ota count : %d",OTAValue.TotalOTA);
			VAlert[CONF_CHANGE_ALERT].Enable=1;
			// ACK header for OTA Parameter Acknowledgment per CDAC spec 6.6
			strcpy(VAlert[CONF_CHANGE_ALERT].Header,"ACK");
			for(i=0;i<OTAValue.TotalOTA;i++)
			{
				SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, OTAValue.OTData[i].KeyVal);
				InsertChar(VAlert[CONF_CHANGE_ALERT].ACK, ':');
				char cc[28]={0};

				if(strstr(OTAValue.OTData[i].KeyVal,"PU")){
					UpdateURL(DEFAULT_IP1);sprintf(cc,"%s:%s",VTSData.ServerData.IP1,VTSData.ServerData.Port1);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"SU")){
					UpdateSecondaryURL(DEFAULT_IP3);sprintf(cc,"%s:%s",VTSData.ServerData.IP2,VTSData.ServerData.Port2);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"VN")){
					UpdateVehicleNumber(DEFAULT_VEHREG);strcpy(cc,VTSData.VehicleData.VehicleRegNo);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"M0")){
					UpdateMoblieNo(DEFAULT_MOB0,1);strcpy(cc,VTSData.PhoneNumber.Mob0);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"M1")){
					UpdateMoblieNo(DEFAULT_MOB1,2);strcpy(cc,VTSData.PhoneNumber.Mob1);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"M2")){
					UpdateMoblieNo("0000000000",3);strcpy(cc,VTSData.PhoneNumber.Mob2);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"M3")){
					UpdateMoblieNo("0000000000",4);strcpy(cc,VTSData.PhoneNumber.Mob3);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"OM")){
					UpdateMoblieNo("0000000000",5);strcpy(cc,VTSData.PhoneNumber.Mob4);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"ED")){
					VTSData.IntervalData.SOSTimeOut = DEFAULT_INV_STM*60;sprintf(cc,"%d",VTSData.IntervalData.SOSTimeOut);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"ST")){
					VTSData.IntervalData.SleepTime=DEFAULT_SLEEP_TIME;sprintf(cc,"%d",VTSData.IntervalData.SleepTime);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"HT")){
					VTSData.IntervalData.HaltInterval=DEFAULT_INV_HALT;sprintf(cc,"%d",VTSData.IntervalData.HaltInterval);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"DSL")){
					VTSData.VehicleData.DefaultSpeed=DEFAULT_SPEED;sprintf(cc,"%2.0f",VTSData.VehicleData.DefaultSpeed);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"SL")){
					VTSData.VehicleData.OverSpeed=DEFAULT_OVERSPEED;sprintf(cc,"%2.0f",VTSData.VehicleData.OverSpeed);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"HBT")){
					VTSData.VehicleData.HarshBreak=DEFAULT_HB;sprintf(cc,"%d",VTSData.VehicleData.HarshBreak);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"HAT")){
					VTSData.VehicleData.HarshAcc=DEFAULT_HA;sprintf(cc,"%d",VTSData.VehicleData.HarshAcc);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"RTT")){
					VTSData.VehicleData.RashTurn=DEFAULT_RT;sprintf(cc,"%d",VTSData.VehicleData.RashTurn);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"LBT")){
					VTSData.BattThrs=LOW_BAT_THRS_VOLT;sprintf(cc,"%i",batteryVoltageToPercentage(VTSData.BattThrs));}
				else if(strstr(OTAValue.OTData[i].KeyVal,"TA")){
					VTSData.VehicleData.TiltAngle=DEFAULT_TL;sprintf(cc,"%d",VTSData.VehicleData.TiltAngle);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"URT")){
					VTSData.IntervalData.HaltInterval=DEFAULT_INV_HALT;sprintf(cc,"%d",VTSData.IntervalData.HaltInterval/60);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"URS")){
					VTSData.IntervalData.SleepInterval=DEFAULT_INV_SLEEP;sprintf(cc,"%d",VTSData.IntervalData.SleepInterval/60);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"URE")){
					VTSData.IntervalData.EnergencyInterval=DEFAULT_INV_CRIT;sprintf(cc,"%d",VTSData.IntervalData.EnergencyInterval);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"URF")){
					VTSData.IntervalData.FullDataPacketInterval=DEFAULT_INV_FULL;sprintf(cc,"%d",VTSData.IntervalData.FullDataPacketInterval/60);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"URH")){
					VTSData.IntervalData.HealthInterval=DEFAULT_INV_HEALTH;sprintf(cc,"%d",VTSData.IntervalData.HealthInterval/60);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"UR")){
					VTSData.IntervalData.MotionInterval=DEFAULT_INV_MOTION;sprintf(cc,"%d",VTSData.IntervalData.MotionInterval);}
				else if(strstr(OTAValue.OTData[i].KeyVal,"VID")){
					strcpy(VTSData.VendorID,DEFAULT_VENDOR);strcpy(cc,VTSData.VendorID);}
				else
					strcpy(cc,"InvalidKey");

				SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, cc);
				if(i != OTAValue.TotalOTA-1)
					InsertChar(VAlert[CONF_CHANGE_ALERT].ACK, ',');

			}
			VAlert[CONF_CHANGE_ALERT].WithACK=1;
			InsertChar(VAlert[CONF_CHANGE_ALERT].ACK, '*');
			nwy_dbg_log("OTA CLR ACK complete: %s", VAlert[CONF_CHANGE_ALERT].ACK);
			if(isserver==0)
				SendSMS(SMSSender,VAlert[CONF_CHANGE_ALERT].ACK);
			else
			{
				AddAlert(CONF_CHANGE_ALERT);
				IsPacketReady.IsNormalPacket=1;
			}
			UpdateConfigInFlash();
		}
		
	}
}


#elif defined(PROTO_NIC1)

void MakeParamChangeString(char* Sender, char* param, uint8_t IsServer)
{
	// memset(dataBuffer,0x00,DATA_MAX_BUFF);
	// sprintf(dataBuffer,"$,PC,12,%s,%d,%s,",NetWork.IMEI,IsServer,Sender);
	// InsertCurrentDateTime(dataBuffer,0);
	// InsertChar(dataBuffer,',');
	// InsertCurrentDateTime(dataBuffer,1);
	// InsertChar(dataBuffer,',');
	// strcat(dataBuffer,param);
	// strcat(dataBuffer,",*");
	
	char ss[20];
	uint16_t i;
	uint32_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	memset(dataBuffer,0,DATA_MAX_BUFF);
	sprintf(dataBuffer,"$PVT,%s,",VTSData.VendorID);
	strcat(dataBuffer,FirmVer);
	#ifdef NIC_BIHAR
	strcat(dataBuffer,",PC,12,L,");
	#else
	strcat(dataBuffer,",OT,12,L,"); // OTA 
	#endif
	
	strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.GPSFix + '0');
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
	strcat(dataBuffer,NetWork.Network);
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
	if(SOS_STATE)
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
	strcat(dataBuffer,Sender);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,param);
	strcat(dataBuffer,"),");
	#endif
	
	crc = chksum(dataBuffer,strlen(dataBuffer));
	sprintf(ss,"%04X*\n",crc);
	strcat(dataBuffer,ss);

	strcat(dataBuffer,param);
	FrameNumber++;

}

#elif defined(PROTO_OG)

void MakeParamChangeString(char* Sender, char* param, uint8_t IsServer)
{

	char ss[20];
	uint16_t i;
	uint8_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	memset(dataBuffer,0,DATA_MAX_BUFF);
	sprintf(dataBuffer,"$,NMP,%s,",VTSData.VendorID);
	strcat(dataBuffer,FirmVer);

	strcat(dataBuffer,",OT,12,L,"); // OTA 
	
	strncat(dataBuffer,NetWork.IMEI,15);
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
	strcat(dataBuffer,NetWork.Network);
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
	if(SOS.IsSOS)
		InsertChar(dataBuffer,'1');
	else
		InsertChar(dataBuffer,'0');
	

	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,PeriPheralVal.OP1 + '0');
	InsertChar(dataBuffer,PeriPheralVal.OP2 + '0');
	InsertChar(dataBuffer,',');

	InsertIntValue(dataBuffer,FrameNumber,"%06d");
	InsertChar(dataBuffer,',');

	
		


	

	
	crc = CRC8(dataBuffer,strlen(dataBuffer));
	sprintf(ss,"%02X",crc);
	strcat(dataBuffer,ss);
	sprintf(ss,",*\n");
	strcat(dataBuffer,ss);
	FrameNumber++;

	
	strcat(dataBuffer,Sender);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,param);

} 

#else


void MakeParamChangeString(char* Sender, char* param, uint8_t IsServer)
{
	// memset(dataBuffer,0x00,DATA_MAX_BUFF);
	// sprintf(dataBuffer,"$,PC,12,%s,%d,%s,",NetWork.IMEI,IsServer,Sender);
	// InsertCurrentDateTime(dataBuffer,0);
	// InsertChar(dataBuffer,',');
	// InsertCurrentDateTime(dataBuffer,1);
	// InsertChar(dataBuffer,',');
	// strcat(dataBuffer,param);
	// strcat(dataBuffer,",*");
	
	char ss[20];
	uint16_t i;
	uint32_t crc;
	
	//uint16_t dLen=23;
	//GPS.sLngDir='E';
	memset(dataBuffer,0,DATA_MAX_BUFF);
	sprintf(dataBuffer,"$PVT,%s,",VTSData.VendorID);
	strcat(dataBuffer,FirmVer);
	
	strcat(dataBuffer,",CFG,12,L,"); // OTA 

	
	strncat(dataBuffer,NetWork.IMEI,15);
	InsertChar(dataBuffer,',');
	AppendVariableString(dataBuffer,VTSData.VehicleData.VehicleRegNo,12,5,"UNKNOWN");
	InsertChar(dataBuffer,',');
	InsertChar(dataBuffer,GPS.GPSFix + '0');
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
	strcat(dataBuffer,NetWork.Network);
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
	if(SOS_STATE)
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
	crc = CRC16(dataBuffer,strlen(dataBuffer));
	sprintf(ss,"%04X*",crc);
	strcat(dataBuffer,ss);
	
	// crc = checksum32(dataBuffer,strlen(dataBuffer));
	// sprintf(ss,"%08X*",crc);
	// strcat(dataBuffer,ss);
	#ifndef ODISA_LD
    InsertChar(dataBuffer,'\n');
    #endif
	
	FrameNumber++;

}

void MakeShortPCString(char* Sender, char* param, uint8_t IsServer)
{
	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	int src=0;
	if(IsServer!= OTA_SRC_SMS)
		src=1;
	sprintf(dataBuffer,"$,PC,12,%s,%d,%s,",NetWork.IMEI,src,Sender);
	InsertCurrentDateTime(dataBuffer,0);
	InsertChar(dataBuffer,',');
	InsertCurrentDateTime(dataBuffer,1);
	InsertChar(dataBuffer,',');
	strcat(dataBuffer,param);
	strcat(dataBuffer,",*");
	#ifndef ODISA_LD
    InsertChar(dataBuffer,'\n');
    #endif
}

#endif

void SendResponce(char *Sender, char* Resp, uint8_t IsServer, uint8_t IsSET)
{
	#ifndef PROTO_CDAC
	// Handle SMS response
	if(IsServer==OTA_SRC_SMS) {
		SendSMS(Sender,Resp);
	}
	
	// Handle RS232 Serial response - Queue it instead of sending immediately
	if(IsServer==OTA_SRC_SERIAL) {
		char resBuffer[256];
		snprintf(resBuffer, sizeof(resBuffer), "$RES,%s", Resp);
		QueueRS232Response(resBuffer);  // Queue instead of SendRS232String
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
		#ifdef EXTENDED_IPS
		else if(IsServer==OTA_SRC_SCK_3) {
			MakeParamChangeString(VTSData.ServerData.Url2,Resp,IsServer); 
		}
		#endif
		
		// Send to all connected servers
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		#ifdef EXTENDED_IPS
		else if(IsServer==OTA_SRC_SCK_3) {
			MakeShortPCString(VTSData.ServerData.Url2,Resp,IsServer);
		}
		#endif
		
		// Send short PC string to all servers
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		#endif
		return;
	}

	// Handle non-SET commands - send only to source
	if(IsServer==OTA_SRC_SCK_1) {
		MakeParamChangeString(VTSData.ServerData.IP1,Resp,IsServer);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		#if defined(PROTO_ODISA1)
		MakeShortPCString(VTSData.ServerData.IP1,Resp,IsServer);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		#endif
	}
	else if(IsServer==OTA_SRC_SCK_2) {
		MakeParamChangeString(VTSData.ServerData.IP3,Resp,IsServer);
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
		#if defined(PROTO_ODISA1)
		MakeShortPCString(VTSData.ServerData.IP3,Resp,IsServer);
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
		#endif
	}
	#ifdef EXTENDED_IPS
	else if(IsServer==OTA_SRC_SCK_3) {
		MakeParamChangeString(VTSData.ServerData.Url2,Resp,IsServer);
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#if defined(PROTO_ODISA1)
		MakeShortPCString(VTSData.ServerData.Url2,Resp,IsServer);
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
	}
	#endif

	#else
	if(IsServer==0)
		SendSMS(Sender,Resp);
	#endif
}
#ifndef PROTO_CDAC
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
		#ifdef HISTORY_INTERNAL
		if(IsTimeSet)
			SavePacket();
		else
			nwy_dbg_log("History Not Saved as Time Not Set");
		#else
		WriteHistoryData(dataBuffer);
		#endif

	}
	TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
	#ifdef EXTENDED_IPS
	TCPSocket_SendString(&ServerSocket[3],dataBuffer);
	#endif
}
#endif

#ifndef PROTO_CDAC
void SendSensorData(void)
{
	if(DHT11.Status==0 && IsFuelData==0)
		return;
	SensorString();
	
	#ifdef EXTENDED_IPS
	TCPSocket_SendString(&ServerSocket[3],dataBuffer);
	#else
	TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
	#endif
}
#endif

#ifndef PROTO_CDAC
void MakeSMSFallbackPacket(void)
{
	sprintf(dataBuffer,"SOSFB,%s\nLat:%s,%c\nLng:%s,%c,fix:%d, Speed:%s\nCID:%s,LAC:%s\n",NetWork.IMEI,sLatitude,GPS.LatDir,sLongitude,GPS.LngDir,GPS.GPSFix,sSpeed,GSM.CellID,GSM.LAC);
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
			TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendString(&ServerSocket[1],dataBuffer);
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		InitBuffer(11);
		IsEMRSend = 0;
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending SOS OFF Packet\n");
		#endif
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
			#ifndef NO_TAMPER
			TCPSocket_SendString(&ServerSocket[0],dataBuffer);
			TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
			#ifdef EXTENDED_IPS
			TCPSocket_SendString(&ServerSocket[3],dataBuffer);
			#endif
			#ifdef ENABLE_RS232_PRINT
			//SendRS232String("Sending SOS Tamper Packet\n");
			#endif
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
			TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		VAlert[GFIN_ALERT].IsSMS=0;
	}
	if(VAlert[GFOUT_ALERT].IsSMS)
	{
		InitBuffer(18);
		TCPSocket_SendString(&ServerSocket[0],dataBuffer);
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
		#ifdef EXTENDED_IPS
		TCPSocket_SendString(&ServerSocket[3],dataBuffer);
		#endif
		VAlert[GFOUT_ALERT].IsSMS=0;
	}

}
#endif

#ifndef PROTO_CDAC
void ChangeToHistoryPacket(char *buf)
{
	char *fn;
	fn = strstr(buf,",NR,01");
	if(!fn)
	{
		// Try alternate format with single digit
		fn = strstr(buf,",NR,1");
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
	
	fn = strstr(buf,",L,");
	if(!fn)
		return;
	fn[1] = 'H';
	nwy_dbg_log("Changed to History Packet, Len :%d",strlen(buf));
	return;
}

void ChangeToHistoryEPB(char *buf)
{
	char *fn;
	fn = strstr(buf,",NM,");
	if(!fn)
		return;

	fn[1] = 'S';
	fn[2] = 'P';
}

uint16_t GetMemeryPercentage(void)
{
	
	#ifdef HISTORY_DISABLED
	return 33;
	#else
	uint16_t count;
	#ifdef HISTORY_INTERNAL
	CheckPacketCount();
	count = PacketConfig.LastPkt;
	#else
	if(!GetHistoryCount(&count))
	{
		nwy_dbg_log("\r\nUnable to get History Count");
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
		#warning History Disabled, will not store or send history packets
		return;
	#endif
	uint16_t count;

	#ifdef HISTORY_INTERNAL
	CheckPacketCount();
	count=  PacketConfig.LastPkt;
	#else
		
	// if(packetConfig.count<=0)
	// 	return;
	
	if(!GetHistoryCount(&count))
	{
		nwy_dbg_log("\r\nUnable to get History Count");
		return;
	}
	#endif
	StoredHistoryDataCount = count;
	MemoryPercent = ((int)count/8000)*100;
	if(count <= 0)
		return;
	

	memset(dataBuffer,0x00,DATA_MAX_BUFF);
	nwy_dbg_log("\r\nFound History Packets : %d, Reading Last...",count);

	#ifdef HISTORY_INTERNAL
	ReadLastPacket();
	#else
	if(!GetHistoryData(dataBuffer))
	{
		nwy_dbg_log("\r\nUnable to get read history data");
		return;
	}
	#endif
	size = strlen(dataBuffer);
	nwy_dbg_log("\r\nHistorty Packet Read Len: %d ",size);
	if(size > 256)
	{
		nwy_dbg_log("\r\nERROR History Packet Size > 256!!!!!");
		#ifdef HISTORY_INTERNAL
		DeleteLastPacket();
		#else
		DeleteHistoryData();
		#endif
		return;
	}
	if (size == 0) {
		nwy_dbg_log("\r\nERROR History Packet size 0!");
		#ifdef HISTORY_INTERNAL
		DeleteLastPacket();
		#else
		DeleteHistoryData();
		#endif
		return;
	}

	if(strstr(dataBuffer,"$EPB"))
	{
		if(ServerSocket[1].SocketState >= SOCKET_CONNECTED)
		{
			nwy_dbg_log("\r\nSending History EMG Packet...");
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
			nwy_dbg_log("\r\nEMG Server not Connected");
		return;
	}
	#ifdef PROTO_MAHARASHTRA1
	if(!strstr(dataBuffer,"$NMP") || strlen(dataBuffer)<180)
	{
		nwy_dbg_log("\r\nInvalid Hitory Packet, deleting...");
		#ifdef HISTORY_INTERNAL
		DeleteLastPacket();
		#else
		DeleteHistoryData();
		#endif
		return;
	}
	#else
	if(!strstr(dataBuffer,"$PVT") || strlen(dataBuffer)<200)
	{
		nwy_dbg_log("\r\nInvalid Hitory Packet, deleting...");
		#ifdef HISTORY_INTERNAL
		DeleteLastPacket();
		#else
		DeleteHistoryData();
		#endif
		return;
	}
	#endif
	ChangeToHistoryPacket(dataBuffer);

	nwy_dbg_log("\r\nSending Packet...");
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
	TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
	#ifdef EXTENDED_IPS
	TCPSocket_SendString(&ServerSocket[3],dataBuffer);
	#endif
}
#else

uint8_t GetBatchData(void)
{
	uint16_t tf,resp;
	uint8_t cc=0,nc=0;
	tf=ReadFileTable();
	char temp[120];
	
	nwy_dbg_log("BATCH CHECK: TotalFiles=%d (ALERT+NORMAL stored)", tf);
	
	if((tf > 0 ) && (tf <= MAX_FILE))
	{
#ifdef HTTP_QUEUE
		// Skip batch if queue is busy or currently sending
		// This prevents conflicts with HttpQueue thread
		if(HttpQueue_Count() > 0 || IsSendProcess) {
			nwy_dbg_log("Batch: skipping, queue busy (count=%d, sending=%d)", 
						HttpQueue_Count(), IsSendProcess);
			return 0;  // Will retry next cycle
		}
		// Lock send process so queue thread doesn't interfere
		IsSendProcess = 1;
#endif

		if(tf > 2)
			tf = 2;
		nwy_dbg_log("*** BATCH SEND: Preparing %d packets (cc=ALERT, nc=NORMAL) ***", tf);
		MakeBatchPacket(tf,&nc,&cc);
		nwy_dbg_log("BATCH SEND: Ready - %d ALERT type, %d NORMAL type", cc, nc);
		print_long_string(SendString);

#ifdef PROTO_CDAC
		resp=SendDataToServer(SendString,IsPacketReady.IsCriticalPacket 
								|| IsPacketReady.IsHealthPacket  || IsPacketReady.IsFullPacket,
								VTSData.IntervalData.CurrentInterval);
#else
		resp=SendDataToServer(SendString,IsPacketReady.IsCriticalPacket 
								|| IsPacketReady.IsHealthPacket  || IsPacketReady.IsFullPacket);
#endif

#ifdef HTTP_QUEUE
		// Release send lock
		IsSendProcess = 0;
#endif

		if(resp)
		{
			int count;
			for(count = 0; count < cc; count++)
			{
				ReadDataBatch(temp,ALERT,1,0,1);
			}


			for(count = 0; count < nc; count++)
			{
				ReadDataBatch(temp,NORMAL,1,0,1);  // NORMAL=0
			}


			return 1;	
		}
		return 0;
	}
	return 0;
}

#ifdef PROTO_CDAC
uint8_t SendDataToServer(char* data, uint8_t KeepAlive, uint16_t currentIntervalSec)
#else
uint8_t SendDataToServer(char* data, uint8_t KeepAlive)
#endif
{
	int ret = 0;
	uint8_t isGood=0;
	uint8_t isSecureHttp = 0;
	uint16_t tmout;
	uint16_t connect_tmout;
	uint16_t response_tmout;
	if(GSM.GSMState != GPRS_ACTIVE)
		return ret;
	isSecureHttp = (ServerSocket[0].Port == 443) ? 1 : 0;
	
#ifdef PROTO_CDAC
	// Dynamic deadline based on current packet interval
	// Leave 500ms buffer before next packet is due
	uint32_t max_send_time_ms;
	if(currentIntervalSec <= 5) {
		max_send_time_ms = 4500;  // 4.5s for critical 5-sec packets
	} else if(currentIntervalSec <= 30) {
		max_send_time_ms = (currentIntervalSec * 1000) - 2000;  // Leave 2s buffer
	} else {
		max_send_time_ms = 30000;  // Cap at 30s for longer intervals
	}
	uint32_t send_deadline = (uint32_t)nwy_get_ms() + max_send_time_ms;
	nwy_dbg_log("HTTP Send: interval=%ds, max_time=%dms", currentIntervalSec, max_send_time_ms);
	
	
	
	connect_tmout = ((currentIntervalSec-1)*700)/15;
	response_tmout = ((currentIntervalSec-1)*300)/15;

	if(connect_tmout > (666))
		connect_tmout = 666;  // ~10s max

	if(response_tmout > (666))
		response_tmout = 666;  // ~10s max
	
#endif
	
	IsSendProcess=1;
	// Check if already connected (Keep-Alive reuse)
	if(ServerSocket[0].SocketState == SOCKET_CONNECTED && KeepAlive)
	{
		nwy_dbg_log("Reusing Keep-Alive connection");
		HTTPConnectFlag=0;
	}
	else
	{
		if(HTTPConnectFlag==1)
			nwy_dbg_log("HTTP Send connect req flag already set!!!!");
		HTTPConnectFlag=1;
	}
	
	for(int lp = 0; lp < 3; lp++)
	{
#ifdef PROTO_CDAC
		// Check deadline before each attempt
		uint32_t now = (uint32_t)nwy_get_ms();
		if(now >= send_deadline)
		{
			nwy_dbg_log("HTTP Send deadline exceeded after %d attempts", lp);
			break;
		}
		// Recalculate remaining time for this attempt
		uint32_t remaining_ms = send_deadline - now;
		nwy_dbg_log("http post attempt %d (remaining: %dms)", lp+1, remaining_ms);
#else
		nwy_dbg_log("http post attempt %d/3", lp+1);
#endif
		// Pre-connect should have connection ready by now
		// If already connected, skip wait. Otherwise short wait for pre-connect to finish.
		if(ServerSocket[0].SocketState == SOCKET_CONNECTED)
			tmout = 1;  // Already connected, no wait
		else
#ifdef PROTO_CDAC
			tmout = connect_tmout;  // Dynamic based on interval
#else
			tmout = 200;  // 3s wait for pre-connect
#endif
		while(ServerSocket[0].SocketState != SOCKET_CONNECTED)
		{
			if(--tmout == 0)
			{
				nwy_dbg_log("HTTP Send Connect TIMEOUT!");
				// Trigger fresh connection attempt for next loop iteration
				HTTPConnectFlag = 1;
				break;
			}
			nwy_sleep(15);
		}
		if(ServerSocket[0].SocketState == SOCKET_CONNECTED)
		{
			isGood=1;
			nwy_sleep(20);
			HTTPConnectFlag=0;
			nwy_dbg_log("Device to Server [%d]: ",strlen(data));
			nwy_dbg_log("%s",data);
			// Shorter timeout for Keep-Alive mode (faster detection of broken connection)
#ifdef PROTO_CDAC
			tmout = response_tmout;  // Dynamic based on interval
#else
			tmout = KeepAlive ? 60 : 150;  // 1.8s for Keep-Alive, 4.5s for normal
#endif
			IsHTTPRes=0;
			ret = HTTP_Post(KeepAlive,0,data,strlen(data),isSecureHttp);
			//HTTP_Close(0);
			#ifdef HTTP_SIMULATE
			if(!isSecureHttp)
			{
			// If HTTP_Post failed (send/ACK error), skip response wait and retry immediately
			if(ret == 0)
			{
				nwy_dbg_log("HTTP_Post failed, closing and retrying...");
				HTTP_Close(0);
				HTTPConnectFlag=1;
				continue;  // retry loop
			}
			while(!ServerSocket[0].isRXData)
			{
#ifdef PROTO_CDAC
				// Check deadline during response wait
				if((uint32_t)nwy_get_ms() >= send_deadline)
				{
					nwy_dbg_log("HTTP Send deadline during response wait");
					tmout = 0;
					break;
				}
#endif
				// Call tcp check FIRST to receive any pending data
				nwy_tcp_check_func(&ServerSocket[0]);
				
				// Now check if data was received (loop condition will exit if so)
				if(ServerSocket[0].isRXData)
					break;
				
				// Only treat as premature disconnect if NO data was received
				if(ServerSocket[0].SocketState != SOCKET_CONNECTED)
				{
					nwy_dbg_log("HTTP Server Disconnected (no response data)");
					tmout =0;
					if(KeepAlive || lp < 2)
						HTTPConnectFlag = 1;
					break;
				}
				
				if(--tmout == 0)
				{
					nwy_dbg_log("No responce from HTTP Server !");
					// Force close stale connection for faster reconnect
					if(KeepAlive)
					{
						nwy_dbg_log("Closing stale Keep-Alive connection");
						HTTP_Close(0);
					}
					break;
				}
				nwy_sleep(30);
			}
			if(!ServerSocket[0].isRXData)
			{
				HTTP_Close(0);
				HTTPConnectFlag=1;
				continue;
			}
			if(tmout!=0)
			{
				nwy_dbg_log("Parsing Server 1 Data...");
				print_long_string((const char*)ServerSocket[0].rxBuffer);
				DecodeOTAData(ServerSocket[0].rxBuffer,1);
				nwy_dbg_log("Parsing done");
				memset(ServerSocket[0].rxBuffer,0,ServerSocket[0].rxSizeMAX);
				ServerSocket[0].isRXData=0;
				break; //all done , dont retry
			}
			}
			else
			{
				if(ret == 0)
				{
					nwy_dbg_log("HTTPS_Post failed, closing and retrying...");
					HTTP_Close(1);
					HTTPConnectFlag = 1;
					continue;
				}
				while(!IsHTTPRes)
				{
#ifdef PROTO_CDAC
					if((uint32_t)nwy_get_ms() >= send_deadline)
					{
						nwy_dbg_log("HTTPS Send deadline during response wait");
						tmout = 0;
						break;
					}
#endif
					if(--tmout == 0)
					{
						nwy_dbg_log("No responce from HTTPS Server !");
						break;
					}
					nwy_sleep(30);
				}
				if(tmout != 0)
				{
					nwy_dbg_log("\r\nParsing Server 1 Data...");
					print_long_string((const char*)ServerSocket[0].rxBuffer);
					DecodeOTAData(ServerSocket[0].rxBuffer,1);
					memset(ServerSocket[0].rxBuffer,0,ServerSocket[0].rxSizeMAX);
					ServerSocket[0].isRXData=0;
					break;
				}
				tmout = 100;
				while(HTTPState == HTTP_STATE_SET)
				{
					if(--tmout == 0)
					{
						nwy_dbg_log("No Auto Close from HTTPS Server !");
						break;
					}
					nwy_sleep(30);
				}
				if(!KeepAlive)
					HTTP_Close(1);
			}
			#else
			while(!IsHTTPRes)
			{
				if(--tmout == 0)
				{
					nwy_dbg_log("No responce from HTTP Server !");
					break;
				}
				nwy_sleep(30);
			}
			if(tmout!=0)
			{
				nwy_dbg_log("\r\nParsing Server 1 Data...");
				print_long_string((const char*)ServerSocket[0].rxBuffer);
				DecodeOTAData(ServerSocket[0].rxBuffer,1);
				memset(ServerSocket[0].rxBuffer,0,ServerSocket[0].rxSizeMAX);
				ServerSocket[0].isRXData=0;
			}
			tmout = 100;
			while(HTTPState== HTTP_STATE_SET)
			{
				if(--tmout == 0)
				{
					nwy_dbg_log("No Auto Close from HTTP Server !");
					break;
				}
				nwy_sleep(30);	
			}
			if(!KeepAlive)
				HTTP_Close(0);
			#endif
		}
		else
		{
			nwy_dbg_log("HTTP not connected to send data !");
		}
	}
	if(!isGood){
		TCPSocket_SendStringNoAck(&ServerSocket[2],data);
		IsSendProcess=0;
		return 0;
	}
	#ifdef HTTP_SIMULATE
	// Only wait for server to close connection when NOT using Keep-Alive
	if(!isSecureHttp && !KeepAlive) {
		tmout = 100;
		while(ServerSocket[0].SocketState == SOCKET_CONNECTED)
		{
			nwy_tcp_check_func(&ServerSocket[0]);
			if(--tmout == 0)
			{
				nwy_dbg_log("No Auto Close from HTTP Server !");
				break;
			}
			nwy_sleep(30);
		}
	}
	#ifdef KEEP_ALIVE
	if(!isSecureHttp && !KeepAlive)
		HTTP_Close(0);
	#else
	if(!isSecureHttp)
		HTTP_CLose();
	if(!isSecureHttp && KeepAlive)
		HTTPConnectFlag=1;
	#endif
	TCPSocket_SendStringNoAck(&ServerSocket[2],data);
	#endif
	IsSendProcess=0;
	return ret;
}
#endif


uint8_t IsFTPReq;

// Helper functions to break down the main logic
static void handleFTPRequests(void) {
	if(GSM.GSMState==GPRS_ACTIVE && FTPState == FTP_STATE_CLOSED && IsFTPReq) {
		FTPStart(&DownloadReq);
		IsFTPReq=0;
	}
}
#ifndef PROTO_CDAC
static void handleIncomingMessages(void) {
	// Handle SMS messages
	if(IsSMS) {
		nwy_dbg_log("\r\nParsing SNS Data...");
		DecodeSMS(SMSData,OTA_SRC_SMS);
		IsSMS=0;
	}
}


static void handleRFIDData(void) {
	#ifndef EXTENDED_IPS
	if(RFIDDataCount && ServerSocket[2].SocketState == SOCKET_CONNECTED) {
		InitBuffer(25);
		TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
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
			nwy_dbg_log("\r\nSending Login Packet to server 1...\r\n");
			#ifdef ENABLE_RS232_PRINT
			//SendRS232String("Sending Login Packet\n");
			#endif
			LoginString();
			TCPSocket_SendString(&ServerSocket[0],dataBuffer);
			SendLogin1 = 0;
		}
	}
	else if (SendLogin2 == 1) {
		if(ServerSocket[2].SocketState == SOCKET_CONNECTED) {
			nwy_dbg_log("\r\nSending Login Packet to server 3...\r\n");
			#ifdef ENABLE_RS232_PRINT
			//SendRS232String("Sending Login Packet\n");
			#endif
			LoginString();
			TCPSocket_SendStringNoAck(&ServerSocket[2],dataBuffer);
			SendLogin2 = 0;
		}
	}
	#ifdef EXTENDED_IPS
	else if (SendLogin3) {
		if(ServerSocket[3].SocketState == SOCKET_CONNECTED) {
			nwy_dbg_log("\r\nSending Login Packet to server 4...\r\n");
			#ifdef ENABLE_RS232_PRINT
			//SendRS232String("Sending Login Packet\n");
			#endif
			LoginString();
			TCPSocket_SendString(&ServerSocket[3],dataBuffer);
			SendLogin3 = 0;
		}
	}
	#endif
}

static void handleNormalPackets(void) {
	if(ServerSocket[0].SocketState == SOCKET_CONNECTED) {
		IsPacketReady.IsNormalPacket = 0;

		// Handle SOS alerts
		if(VAlert[SOS_ON_ALERT].Enable) {
			EmergencyPacket(1); // SOS ON ALERT
			if(ServerSocket[1].SocketState == SOCKET_CONNECTED) {
				TCPSocket_SendString(&ServerSocket[1], dataBuffer);
			}
		}

		// Send normal packet
		#ifdef SOS_FULL_EA
		if(SOS.IsSOS)
			InitBuffer(10);
		else
			InitBuffer(1);
		#else
		InitBuffer(1);  // NORMAL PACKET
		#endif
		#ifdef ENABLE_RS232_PRINT
		//SendRS232String("Sending Normal Packet\n");
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
			#ifdef HISTORY_INTERNAL
			if(IsTimeSet) {
				SavePacket();
			}
			else {
				nwy_dbg_log("History Not Saved as Time Not Set");
			}
			#else
			WriteHistoryData(dataBuffer); 
			#endif
		}
	}
}
#endif

#ifndef PROTO_CDAC
static void handleHealthPackets(void) {
	IsPacketReady.IsHealthPacket = 0;
	HealthPacket();
	
	// Send to all connected servers
	TCPSocket_SendString(&ServerSocket[0], dataBuffer);
	TCPSocket_SendStringNoAck(&ServerSocket[2], dataBuffer);
	#ifdef EXTENDED_IPS
	TCPSocket_SendString(&ServerSocket[3], dataBuffer);
	#endif

	#if defined(PROTO_MAHARASHTRA1)  
	InitBuffer(30);
	TCPSocket_SendString(&ServerSocket[0], dataBuffer);
	TCPSocket_SendStringNoAck(&ServerSocket[2], dataBuffer);
	#ifdef EXTENDED_IPS
	TCPSocket_SendString(&ServerSocket[3], dataBuffer);
	#endif
	
	#elif defined(PROTO_NIC1)
	// Protocol specific handling
	
	#else
	InitBuffer(30);
	TCPSocket_SendString(&ServerSocket[0], dataBuffer);
	TCPSocket_SendStringNoAck(&ServerSocket[2], dataBuffer);
	#ifdef EXTENDED_IPS
	TCPSocket_SendString(&ServerSocket[3], dataBuffer);
	#endif
	#endif
}
#endif

#ifndef PROTO_CDAC
static void handlePackets(void) {
	// Handle alerts if any server is connected
	if(ServerSocket[0].SocketState == SOCKET_CONNECTED || ServerSocket[2].SocketState==SOCKET_CONNECTED) {
		CheckAlerts();
	}

	if(!IsTimeSet)	
		return;

	// Handle history packets
	if(IsPacketReady.IsHistoryPacket) {
		ProcessHistoryPacket();
		IsPacketReady.IsHistoryPacket=0;
	}

	// Handle normal packets
	if(IsPacketReady.IsNormalPacket) {
		handleNormalPackets();
	}

	// Handle health packets 
	else if(IsPacketReady.IsHealthPacket && (ServerSocket[0].SocketState == SOCKET_CONNECTED)) {
		handleHealthPackets();
	}
}

static void handleSensorData(void) {
	// Process all sensor packets (interrupt packets have priority, then regular packets)
	// ProcessSensors() handles both interrupt and regular sensor packets
	// and clears IsPacketReady.IsSensPacket internally for regular packets
	ProcessSensors();
}
#endif

static void handleServerResponses(void) {
	if(ServerSocket[0].isRXData) {
		nwy_dbg_log("\r\nParsing Server 1 Data...");
		DecodeSMS(ServerSocket[0].rxBuffer,OTA_SRC_SCK_1);
		ServerSocket[0].isRXData=0;
	}

	if(ServerSocket[2].isRXData) {
		nwy_dbg_log("\r\nParsing Server 2 Data...");
		DecodeSMS(ServerSocket[2].rxBuffer,OTA_SRC_SCK_2);
		ServerSocket[2].isRXData=0;
	}

	#ifdef EXTENDED_IPS
	if(ServerSocket[3].isRXData) {
		nwy_dbg_log("\r\nParsing Server 3 Data...");
		DecodeSMS(ServerSocket[3].rxBuffer,OTA_SRC_SCK_3);
		ServerSocket[3].isRXData=0;
	}
	#endif
}

#ifdef PROTO_CDAC
/**
 * @brief Send first-time EPB/CRT alerts (SOS ON, Tamper, Main Fail, etc.)
 * These are the initial trigger alerts that start a critical state
 */
static void sendEmergencyAlerts(uint16_t *resp) {
	uint8_t critCount = MakeCriticalString(1);
	IsPacketReady.IsCriticalPacket = 0;
	
	if(critCount) {
		VehicleState.PacketState = CRITICAL;
		nwy_dbg_log("%d critical packet ready", critCount);
		
		// Determine timeout based on whether emergency state (SOS) is active:
		// - SOS/SOS_Tamper: Use URE interval (typically 5s)
		// - Other critical alerts (Tilt, Overspeed): Use current interval (motion/halt/sleep)
		// - One-time alerts (Main Fault, Main Restore, SOS_OFF, etc.): 30s - try harder to send
		uint8_t isEmergencyState = SOS.IsSOS || SOS.IsSOSTamper;
		uint8_t hasContinuousCritical = isEmergencyState || 
			(PeriPheralVal.IsTilt && VAlert[TILT_ALERT].Enable) ||
			(IsOverSpeed && VAlert[OVER_SPEED_ALERT].Enable);
		
		uint16_t criticalTimeout;
		if(isEmergencyState) {
			criticalTimeout = VTSData.IntervalData.EnergencyInterval;
		} else if(hasContinuousCritical) {
			criticalTimeout = VTSData.IntervalData.CurrentInterval;
		} else {
			criticalTimeout = 30;  // One-time alerts
		}
		nwy_dbg_log("Critical timeout: %ds (continuous=%d)", criticalTimeout, hasContinuousCritical);
		
		for(int i = 0; i < critCount; i++) {
#ifdef HTTP_QUEUE
			// Add to queue - queue handles sending, expiry storage, etc.
			// Storage type ALERT ensures priority in batch if it expires
			if(HttpQueue_Add(CriticalString[i], criticalTimeout, HTTP_QUEUE_TYPE_ALERT)) {
				nwy_dbg_log("Critical packet %d queued", i);
				*resp = 1;  // Queued successfully
			} else {
				nwy_dbg_log("Critical packet %d queue failed, storing to flash", i);
				StoreFileToFlash(CriticalString[i], ALERT);
				*resp = 0;
			}
			// With queue, send SMS immediately for critical alerts (can't wait for TCP result)
			if(SOS.IsSOSSMS) {
				nwy_dbg_log("Sending SOS SMS %d (queued mode)", SOS.IsSOSSMS);
				SMSAlert(SOS.IsSOSSMS);
				SOS.IsSOSSMS = 0;
			}
			if(PeriPheralVal.PendingSMSAlert) {
				nwy_dbg_log("Sending peripheral SMS %d (queued mode)", PeriPheralVal.PendingSMSAlert);
				SMSAlert(PeriPheralVal.PendingSMSAlert);
				PeriPheralVal.PendingSMSAlert = 0;
			}
#else
			// Keep-Alive only for multiple packets in same batch or pending other packet types
			// For continuous critical alerts (5s interval), close connection and use pre-connect
			uint8_t useKeepAlive = IsPacketReady.IsNormalPacket || 
				IsPacketReady.IsHealthPacket || 
				IsPacketReady.IsFullPacket || 
				(critCount - i > 1);  // More critical packets pending in this batch
			// Critical packets use dynamic interval based on alert type
			*resp = SendDataToServer(CriticalString[i], useKeepAlive, criticalTimeout);
			if(!*resp) {
				// Store critical packet to flash if sending failed (no GPRS or server unreachable)
				StoreFileToFlash(CriticalString[i], ALERT);
				// For one-time alerts, still send SMS even if TCP failed - user needs notification
				if(!hasContinuousCritical && PeriPheralVal.PendingSMSAlert) {
					nwy_dbg_log("TCP failed - sending one-time alert SMS %d anyway", PeriPheralVal.PendingSMSAlert);
					SMSAlert(PeriPheralVal.PendingSMSAlert);
					PeriPheralVal.PendingSMSAlert = 0;
				}
			} else {
				// CDAC spec 4.iii req 6: Send SMS after successful server packet
				// SMSAlert is now non-blocking (no IsSendProcess waits)
				// SMS uses GSM signaling channel, independent of GPRS data
				if(SOS.IsSOSSMS) {
					nwy_dbg_log("Sending SOS SMS %d after server success", SOS.IsSOSSMS);
					SMSAlert(SOS.IsSOSSMS);
					SOS.IsSOSSMS = 0;
				}
				if(PeriPheralVal.PendingSMSAlert) {
					nwy_dbg_log("Sending peripheral SMS %d after server success", PeriPheralVal.PendingSMSAlert);
					SMSAlert(PeriPheralVal.PendingSMSAlert);
					PeriPheralVal.PendingSMSAlert = 0;
				}
			}
#endif /* HTTP_QUEUE */
		}
	}
	VehicleState.PacketState = NORMAL;
}

/**
 * @brief Send continuous critical alerts
 * For ongoing SOS, Tamper, Tilt, Overspeed states
 * URE interval for SOS/Tamper, CurrentInterval for Tilt/Overspeed
 */
static void sendContinuousCriticalAlerts(uint8_t critCount, uint16_t *resp) {
	VehicleState.PacketState = CRITICAL;
	
	// Determine interval based on alert type:
	// - SOS/Tamper: Use URE (EnergencyInterval)
	// - Tilt/Overspeed: Use CurrentInterval (motion/halt/sleep)
	uint8_t isEmergencyState = SOS.IsSOS || SOS.IsSOSTamper;
	uint16_t critInterval = isEmergencyState ? 
		VTSData.IntervalData.EnergencyInterval : 
		VTSData.IntervalData.CurrentInterval;
	
	nwy_dbg_log("%d Repeating critical packet ready (interval=%ds, emergency=%d)", 
		critCount, critInterval, isEmergencyState);
	
	for(int i = 0; i < critCount; i++) {
#ifdef HTTP_QUEUE
		// Add to queue with appropriate interval
		if(HttpQueue_Add(CriticalString[i], critInterval, HTTP_QUEUE_TYPE_ALERT)) {
			nwy_dbg_log("Repeating critical packet %d queued", i);
			*resp = 1;
		} else {
			nwy_dbg_log("Repeating critical packet %d queue failed, storing", i);
			StoreFileToFlash(CriticalString[i], ALERT);
			*resp = 0;
		}
#else
		// Keep-Alive only for multiple packets in same batch or pending other packet types
		uint8_t useKeepAlive = IsPacketReady.IsCriticalPacket || 
			IsPacketReady.IsHealthPacket || 
			IsPacketReady.IsFullPacket || 
			(critCount - i > 1);  // More critical packets pending in this batch
#ifdef PROTO_CDAC
		// Use appropriate interval based on alert type
		*resp = SendDataToServer(CriticalString[i], useKeepAlive, critInterval);
#else
		*resp = SendDataToServer(CriticalString[i], useKeepAlive);
#endif
		if(!*resp) {
			StoreFileToFlash(CriticalString[i], ALERT);
		}
#endif /* HTTP_QUEUE */
	}
}

/**
 * @brief Store ALT packets to flash while device is in SOS/critical state
 * Can't send now because critical alerts have priority, store for later batch
 */
static void storeAlertsWhileInSOS(uint8_t alertCount) {
	if(alertCount) {
		// Save to Flash - Store as ALERT type so they are sent before normal packets in batch
		nwy_dbg_log("*** SOS BATCH: %d Alert(s) to store ***", alertCount);
		for(int i = 0; i < alertCount; i++) {
			// Alert ID is at position 18-19 in the packet string (CriticalString[i])
			char alertId[3] = {CriticalString[i][18], CriticalString[i][19], '\0'};
			nwy_dbg_log("SOS BATCH: Storing AlertID=%s (len=%d)", alertId, strlen(CriticalString[i]));
			
			// Verify packet is valid before storing
			if(strlen(CriticalString[i]) < 50) {
				nwy_dbg_log("ERROR: Alert packet too short, skipping!");
				continue;
			}
			
			StoreFileToFlash(CriticalString[i], ALERT);
			nwy_dbg_log("SOS BATCH: AlertID=%s stored successfully", alertId);
		}
		nwy_dbg_log("*** SOS BATCH: Stored %d alerts for later batch ***", alertCount);
	} else {
		nwy_dbg_log("SOS BATCH: No alerts to store");
	}
}

/**
 * @brief Send non-critical alerts (ALT/ACK types)
 * Geofence, Harsh events, OTA changes, Battery, Main restore, etc.
 */
static void sendNonCriticalAlerts(uint8_t alertCount, uint16_t *resp) {
	nwy_dbg_log("%d alert packet ready, Sending...", alertCount);
	
	for(int i = 0; i < alertCount; i++) {
#ifdef PROTO_CDAC
		// Determine timeout based on alert type:
		// - Continuous alerts (Tilt, Overspeed): 5s - will retry on next interval
		// - One-time alerts (Main Restore, Tamper, Geofence, etc.): 30s - try harder to send
		uint16_t alertTimeout = 30;  // Default: longer timeout for one-time alerts
		if((PeriPheralVal.IsTilt && VAlert[TILT_ALERT].Enable) || 
		   (IsOverSpeed && VAlert[OVER_SPEED_ALERT].Enable)) {
			alertTimeout = 5;  // Continuous alerts - short timeout, will retry
		}
		
#ifdef HTTP_QUEUE
		// Add to queue - non-SOS alerts stored as NORMAL type for correct batch order
		if(HttpQueue_Add(CriticalString[i], alertTimeout, HTTP_QUEUE_TYPE_NORMAL)) {
			nwy_dbg_log("Non-SOS alert %d queued", i);
			*resp = 1;
		} else {
			nwy_dbg_log("Non-SOS alert %d queue failed, storing as NORMAL", i);
			StoreFileToFlash(CriticalString[i], NORMAL);
			*resp = 0;
		}
#else
		*resp = SendDataToServer(CriticalString[i], 
			IsPacketReady.IsCriticalPacket || 
			IsPacketReady.IsHealthPacket || 
			IsPacketReady.IsFullPacket ||
			(alertCount - i > 1), alertTimeout);  // Keep-Alive if more alerts to send
		
		if(!*resp) {
			// Non-SOS alerts (index >= 13) are non-critical - store as NORMAL for correct batch order
			// Critical alerts in batch should come before non-critical ones
			nwy_dbg_log("Non-SOS alert send failed, storing as NORMAL");
			StoreFileToFlash(CriticalString[i], NORMAL);  // NORMAL=0
		}
#endif /* HTTP_QUEUE */
#else
		*resp = SendDataToServer(CriticalString[i], 
			IsPacketReady.IsCriticalPacket || 
			IsPacketReady.IsHealthPacket || 
			IsPacketReady.IsFullPacket ||
			(alertCount - i > 1));  // Keep-Alive if more alerts to send
#endif
	}
}

/**
 * @brief Send regular NRM packet or batch packet
 * Called when no alerts pending, normal interval packet
 */
static void sendNormalPacket(uint16_t *resp) {
	if(GPS.Speed < VTSData.VehicleData.OverSpeed) {
		// Don't send NRM packets while continuous critical alerts are active
		// Per CDAC spec 4.iii: continuous critical alerts (SOS, SOS Tamper, Tilt, Overspeed)
		// should send CRT at update interval, not NRM
		// Also block NRM if SOS_OFF_ALERT is pending (OTA cleared SOS, but OFF alert not sent yet)
		uint8_t hasContinuousCritical = SOS.IsSOS || SOS.IsSOSTamper || 
			(VAlert[TILT_ALERT].Enable && VAlert[TILT_ALERT].AlertSent) ||
			VAlert[SOS_OFF_ALERT].Enable;  // Block NRM until Emergency OFF is sent
		
		if(!IsOverSpeed && !hasContinuousCritical) {
			if(!GetBatchData()) {
				uint8_t ok = MakeNormalPacket();
				if(ok) {
					nwy_dbg_log("Normal Packet ready");
#ifdef HTTP_QUEUE
					// Add normal packet to queue
					if(HttpQueue_Add(SendString, VTSData.IntervalData.CurrentInterval, HTTP_QUEUE_TYPE_NORMAL)) {
						nwy_dbg_log("Normal packet queued");
						*resp = 1;
					} else {
						nwy_dbg_log("Normal packet queue failed, storing");
						StoreFileToFlash(SendString, NORMAL);
						*resp = 0;
					}
#else
#ifdef PROTO_CDAC
					// Normal packets use current interval (motion/halt/sleep/etc.)
					*resp = SendDataToServer(SendString, 
						IsPacketReady.IsCriticalPacket || 
						IsPacketReady.IsHealthPacket || 
						IsPacketReady.IsFullPacket,
						VTSData.IntervalData.CurrentInterval);
#else
					*resp = SendDataToServer(SendString, 
						IsPacketReady.IsCriticalPacket || 
						IsPacketReady.IsHealthPacket || 
						IsPacketReady.IsFullPacket);
#endif
					if(!*resp) {
						StoreFileToFlash(SendString, NORMAL);  // NORMAL=0
					}
#endif /* HTTP_QUEUE */
				}
			}
		}
	}
}

/**
 * @brief Process CDAC normal interval - handles alerts and normal packets
 * Called when IsNormalPacket flag is set (motion/halt/sleep interval)
 */
static void processCDACNormalInterval(uint16_t *resp, uint8_t criticalAlreadyHandled) {
	uint8_t critCount = 0, alertCount;
	
	IsPacketReady.IsNormalPacket = 0;
	
	// Handle critical packets first - but only if not already handled in this cycle
	// This prevents duplicate packets when both IsCriticalPacket and IsNormalPacket are set
	if(!criticalAlreadyHandled) {
		critCount = MakeCriticalString(0);
		if(critCount) {
			sendContinuousCriticalAlerts(critCount, resp);
		}
	}

	// Handle alerts (Geofence, Harsh events, OTA changes, etc.)
	alertCount = MakeAlertString();
	
	// SOS/Emergency state: ALWAYS store alerts (mandatory per CDAC spec)
	if(SOS.IsSOS) {
		storeAlertsWhileInSOS(alertCount);
	}
#ifdef STORE_ALERTS_DURING_CONTINUOUS_CRITICAL
	// Continuous critical state (Tilt/Overspeed): Store alerts when macro is defined
	else if(PeriPheralVal.IsTilt || IsOverSpeed) {
		if(alertCount) {
			nwy_dbg_log("Continuous critical active - storing %d alerts", alertCount);
			for(int i = 0; i < alertCount; i++) {
				StoreFileToFlash(CriticalString[i], NORMAL);  // Store as NORMAL for batch ordering
			}
		}
		else {
			sendNormalPacket(resp);
		}
	}
#endif
	else if(alertCount) {
		sendNonCriticalAlerts(alertCount, resp);
	}
	else {
		sendNormalPacket(resp);
	}
}

/**
 * @brief Send Health (HLT) packet with device diagnostics
 */
static void sendHealthPacket(uint8_t hasCritical, uint16_t *resp) {
	IsPacketReady.IsHealthPacket = 0;
	if(!hasCritical && !SOS.IsSOS) {
		HealthPacket();
		nwy_dbg_log("health packet ready");
#ifdef HTTP_QUEUE
		// Add health packet to queue
		if(HttpQueue_Add(SendString, VTSData.IntervalData.CurrentInterval, HTTP_QUEUE_TYPE_NORMAL)) {
			nwy_dbg_log("Health packet queued");
			*resp = 1;
		} else {
			nwy_dbg_log("Health packet queue failed");
			*resp = 0;
		}
#else
#ifdef PROTO_CDAC
		// Health packets use current interval
		*resp = SendDataToServer(SendString,
			IsPacketReady.IsFullPacket ||
			IsPacketReady.IsCriticalPacket || 
			IsPacketReady.IsNormalPacket,
			VTSData.IntervalData.CurrentInterval);
#else
		*resp = SendDataToServer(SendString,
			IsPacketReady.IsFullPacket ||
			IsPacketReady.IsCriticalPacket || 
			IsPacketReady.IsNormalPacket);
#endif
#endif /* HTTP_QUEUE */
	}
	else {
		nwy_dbg_log("health packet ignored due to critical state");
	}
}

/**
 * @brief Send Full (FUL) packet with complete vehicle data
 */
static void sendFullPacket(uint8_t hasCritical, uint16_t *resp) {
	IsPacketReady.IsFullPacket = 0;
	if(!hasCritical && !SOS.IsSOS) {
		FullPacket();
		nwy_dbg_log("full packet ready");
#ifdef HTTP_QUEUE
		// Add full packet to queue
		if(HttpQueue_Add(SendString, VTSData.IntervalData.CurrentInterval, HTTP_QUEUE_TYPE_NORMAL)) {
			nwy_dbg_log("Full packet queued");
			*resp = 1;
		} else {
			nwy_dbg_log("Full packet queue failed");
			*resp = 0;
		}
#else
#ifdef PROTO_CDAC
		// Full packets use current interval
		*resp = SendDataToServer(SendString,
			IsPacketReady.IsCriticalPacket || 
			IsPacketReady.IsHealthPacket || 
			IsPacketReady.IsNormalPacket,
			VTSData.IntervalData.CurrentInterval);
#else
		*resp = SendDataToServer(SendString,
			IsPacketReady.IsCriticalPacket || 
			IsPacketReady.IsHealthPacket || 
			IsPacketReady.IsNormalPacket);
#endif
#endif /* HTTP_QUEUE */
	}
	else {
		nwy_dbg_log("full packet ignored due to critical state");
	}
}

static void handleCDACProtocol(void) {
	uint16_t resp;
	uint8_t hasCritical;

	// Handle FTP requests
	handleFTPRequests();

	// Handle incoming SMS
	if(IsSMS) {
		nwy_dbg_log("\r\nParsing SNS Data...");
		nwy_dbg_log("%s", SMSData);
		DecodeOTAData(SMSData, 0);
		IsSMS = 0;
	}

	// Handle server responses - CDAC uses HTTP so ServerSocket[0] is handled differently
	if(ServerSocket[0].isRXData) {
		nwy_dbg_log("\r\nParsing Server 1 Data...");
		print_long_string((const char*)ServerSocket[0].rxBuffer);
		DecodeOTAData(ServerSocket[0].rxBuffer, 1);
		memset(ServerSocket[0].rxBuffer, 0, ServerSocket[0].rxSizeMAX);
		ServerSocket[0].isRXData = 0;
	}
	
	if(ServerSocket[2].isRXData) {
		nwy_dbg_log("\r\nParsing Server 2 Data...");
		DecodeOTAData(ServerSocket[2].rxBuffer, 2);
		ServerSocket[2].isRXData = 0;
	}

	UpdateInterval();

	if((GSM.GSMState >= SIM_DETECTED) && IsTimeSet) {
		// Handle login
		if(SendLogin1) {
			LoginPacket();
			nwy_dbg_log("login packet ready");
#ifdef HTTP_QUEUE
			// Login packet - add to queue with current interval
			if(HttpQueue_Add(SendString, VTSData.IntervalData.CurrentInterval, HTTP_QUEUE_TYPE_NORMAL)) {
				nwy_dbg_log("Login packet queued");
				SendLogin1 = 0;
			}
#else
#ifdef PROTO_CDAC
			// Login packet - use current interval timeout
			resp = SendDataToServer(SendString, 0, VTSData.IntervalData.CurrentInterval);
#else
			resp = SendDataToServer(SendString, 0);
#endif
			if(resp) {
				SendLogin1 = 0;
			}
#endif /* HTTP_QUEUE */
		}
		// Handle other packets
		else {
			uint8_t criticalHandled = 0;  // Track if we already handled critical in this cycle
			
			// Handle critical packets
			if(IsPacketReady.IsCriticalPacket) {
				sendEmergencyAlerts(&resp);
				criticalHandled = 1;  // Mark that critical was handled
			}

			// Handle normal packets
			if(IsPacketReady.IsNormalPacket) {
				processCDACNormalInterval(&resp, criticalHandled);
			}

			// Handle health packets
			hasCritical = 0; // Reset for health/full packet checks
			if(IsPacketReady.IsHealthPacket) {
				sendHealthPacket(hasCritical, &resp);
			}
			// Handle full packets - use 'if' not 'else if' to handle both when intervals align
			if(IsPacketReady.IsFullPacket) {
				sendFullPacket(hasCritical, &resp);
			}
		}
	}
	
	// CDAC spec 4.iii requirement 7: If GPRS unavailable, send SMS immediately
	// SMS is sent after successful server packet in handleCriticalPackets()
	// This section handles the case when GPRS is down (can't send server packet)
	if(GSM.GSMState < GPRS_ACTIVE) {
		if(SOS.IsSOSSMS) {
			nwy_dbg_log("No GPRS - sending SOS SMS %d immediately", SOS.IsSOSSMS);
			SMSAlert(SOS.IsSOSSMS);
			SOS.IsSOSSMS = 0;
		}
		if(PeriPheralVal.PendingSMSAlert) {
			nwy_dbg_log("No GPRS - sending peripheral SMS %d immediately", PeriPheralVal.PendingSMSAlert);
			SMSAlert(PeriPheralVal.PendingSMSAlert);
			PeriPheralVal.PendingSMSAlert = 0;
		}
	}
}
#endif
void ServerThreadEntry(void *param)
{
	nwy_sleep(8000);
	nwy_dbg_log("\r\nServer Thread Entry!!\r\n");
	
	while(1)
	{
		#ifndef PROTO_CDAC
		// Get current interval and reset timeout
		GetCurrentInterval();
		ServerThreadTimeout=0;

		// Handle OTA commands from MCU
		if(IsOTACmd) {
			IsOTACmd = 0;
			nwy_dbg_log("Processing OTA command from MCU");
			DecodeSMS(OTABuffer, OTA_SRC_SERIAL);
		}

		// Handle FTP requests
		handleFTPRequests();

		// Handle incoming messages (SMS)
		handleIncomingMessages();

		// Handle RFID data
		handleRFIDData();

		// Handle login requests or packet processing
		if(SendLogin1 || SendLogin2
		#ifdef EXTENDED_IPS
		|| SendLogin3
		#endif
		) {
			handleLoginRequests();
		}
		else {
			// Handle various packet types
			handlePackets();
		}

		// Handle sensor data
		handleSensorData();

		// Handle server responses
		handleServerResponses();

		#else
		// PROTO_CDAC specific handling
		handleCDACProtocol();
		#endif

		nwy_sleep(100);
	}
	nwy_exit_thread_self();
}

