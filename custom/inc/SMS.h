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

#define OTA_SRC_SMS     0
#define OTA_SRC_SCK_1   1
#define OTA_SRC_SCK_2   2
#define OTA_SRC_SCK_3   3
#define OTA_SRC_SCK_4   4
#define OTA_SRC_BLE     5
#define OTA_SRC_RS232   6
#define OTA_SRC_RS485   7

#ifdef PROTO_OG
typedef struct {
    char    Source[64];  /* Origin address (host:port / phone), or local channel. */
    uint8_t Channel;     /* OTA_SRC_* routing identity, independent of Source. */
    char    Mode[8];     /* "GET", "SET", "CLR" */
    char    CmdId[128];  /* "001" or multiple "001,002,003,019,022" */
    char    Value[256];  /* returned/set value or multiple "val1,val2,val3" */
    uint8_t Status;      /* 1=success, 0=failure */
    uint8_t Pending;     /* 1=waiting for OA/12; restored if socket send fails */
} OTAResponseTypeDef;

extern OTAResponseTypeDef LastOTAResponse;
/* Set by an AMD3 endpoint SET/CLR command received over TCP.  Server.c
 * restarts sockets only after its OA,12 acknowledgement has been queued. */
extern uint8_t AIS140SocketReinitPending;

/* Deferred reset variables for commands 007 & 018 */
extern uint8_t AIS140ResetPending;
extern char AIS140ResetReason[32];

void ParseStandardAIS140Command(const char* raw, uint8_t src);
#endif

uint8_t SendSOSSMS(uint8_t isFall);
#ifndef PROTO_CDAC
void SendGeoData(uint8_t index, uint8_t OTASource);
#endif
void SendSOSAlertSMS(uint8_t AlertNum);
uint8_t SendSMS(char* ph,char* msg);

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
