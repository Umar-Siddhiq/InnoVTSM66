#ifndef _HTTP_H
#define _HTTP_H

#include "VTS.h"

#if defined(PROTO_CDAC)

#define HTTP_DEFAULT_CHANNEL 1

typedef enum
{
    HTTP_EVENT_NONE,
    HTTP_EVENT_ERROR = 3,
    HTTP_EVENT_CLOSED,
    HTTP_EVENT_SETUP,
    HTTP_EVENT_SEND,
    HTTP_EVENT_RECEIVE
} HttpEventTypedef;

typedef enum
{
    HTTP_STATE_NOTSET,
    HTTP_STATE_SET
} HTTPStattypedef;

extern volatile HTTPStattypedef HTTPState;
extern volatile uint8_t IsHTTPRes;
extern volatile uint8_t HTTPConnectFlag;

uint8_t HTTP_Setup(char *ip, uint16_t port);
uint8_t HTTPS_Setup(char *ip, int port, void *ssl);
uint8_t HTTP_Post(uint8_t keepAlive, uint8_t type, char *data, int datalen, uint8_t isSecure);
uint8_t HTTP_WaitResponce(void);
uint8_t HTTP_Close(uint8_t isSecure);
void HTTPThreadEntry(s32 taskId);
void HTTP_EnableSecure(uint8_t enable);
uint8_t HTTP_IsSecureEnabled(void);

#endif

#endif