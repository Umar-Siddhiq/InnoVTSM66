
#ifndef							_SMS_H
#define							_SMS_H

#include <stdio.h>
#include "project.h"
#define MSGSIZE			400
extern char SMSSender[];
extern uint16_t IsSMS;
extern char SMSData[];
extern char SimData[];


#ifdef PROTO_MAHARASHTRA1
extern uint8_t IsSRCMD;
extern char CMD_Buff[];
#endif

void InitSMS(void);
void SendSOSSMS(uint8_t isFall);
void SendSMS(char* ph,char* msg);
void ReadSMS(char* num);
void DeleteSMS(char* msgNum);
void DeleteAllSMS(void);
void DeleteAllReadSMS(void);
uint8_t DecodeSMS(char* msg,uint8_t OTASource);
void SMS_rcvcb(void *data, size_t size);
void MakeACTMessage(uint8_t mode, char* code);
int GetValueFromData(char* data, char* cmd,char delim1, int delimPos, char delim2, char* value);

void MOTAPacket(char *buf,char *sender, uint8_t OTASource);
void FOTAPacket(char *buf,char *sender, uint8_t OTASource);

#endif								// _SMS_H