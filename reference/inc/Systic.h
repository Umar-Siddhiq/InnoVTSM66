#ifndef _SYSTIC_H
#define _SYSTIC_H
#include "project.h"
#include "VTS.h"
extern volatile uint16_t ServerHangTimeOut,FuelCount;

#define	PARAM_UPDATE						180
#define SERVER_HANG_TIME				15
#define MCU_HANG_TIME                   10
#define ALIVE_PKT_TIME                  3
typedef struct 
{
    uint16_t TTime;
    uint16_t ONTime;

} StatLEDtypedef;

extern StatLEDtypedef StatLED;
extern uint8_t IsALVSend;

// RS232 Response Buffer
#define RS232_RESPONSE_BUFFER_SIZE 300
extern char RS232ResponseBuffer[RS232_RESPONSE_BUFFER_SIZE];
extern uint8_t IsRS232ResponsePending;

void SetStatLED(uint16_t tt, uint16_t on);
void SysticThreadEntry(void *param);
void SendBufferedRS232Response(void);
void QueueRS232Response(const char* response);
#ifdef PROTO_CDAC
void UpdateInterval(void);
#endif

#endif