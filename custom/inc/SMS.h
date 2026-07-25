#ifndef							_SMS_H
#define							_SMS_H

#include "VTS.h"
#include "SMSlib.h"
#define MSGSIZE			400
extern char SMSSender[];
extern uint16_t IsSMS;
extern char SMSData[];
extern char SimData[];


#if  defined(PROTO_MAHARASHTRA1) || defined(PROTO_OG)
extern uint8_t IsSRCMD;
extern char CMD_Buff[];
#endif

#define OTA_SRC_SMS 0
#define OTA_SRC_SCK_1   1
#define OTA_SRC_SCK_2   2
#define OTA_SRC_SCK_3   3
#define OTA_SRC_SCK_4   4
#define OTA_SRC_BLE     5
#define OTA_SRC_RS232   6
#define OTA_SRC_RS485   7

void SendSOSSMS(uint8_t isFall);
void SendSOSAlertSMS(uint8_t AlertNum);
void SendSMS(char* ph,char* msg);

uint8_t DecodeSMS(char* msg,uint8_t IsServer);
void MakeACTMessage(uint8_t mode, char* code);
int GetValueFromData(char* data, char* cmd,char delim1, int delimPos, char delim2, char* value);

void MOTAPacket(char *buf,char *sender, uint8_t IsServer);
void FOTAPacket(char *buf,char *sender, uint8_t IsServer);

// RS232/RS485 OTA handling functions
void ProcessRS232OTAData(void);
void ProcessRS485OTAData(void);
void SendRS232Response(char* response);
void SendRS485Response(char* response);

#endif								// _SMS_H
