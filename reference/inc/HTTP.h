#ifndef _HTTP_H
#define _HTTP_H

#include "project.h"
#include "VTS.h"
#include "Hardware.h"
#include "GPRS.h"


#define HTTP_DEFAULT_CHANNEL     1

#define HTTP_CALLBACK_TIMEOUT 5000
#define HTTP_RECEIVE_MAX_SIZE 800

#define HTTP_INSECURE       0
#define HTTP_SECURE         1

typedef enum 
{
    HTTP_EVENT_NONE,
    HTTP_EVENT_ERROR=3,
    HTTP_EVENT_CLOSED,
    HTTP_EVENT_SETUP,
    HTTP_EVENT_SEND,
    HTTP_EVENT_RECEIVE
}HttpEventTypedef;

typedef enum
{
    HTTP_STATE_NOTSET,
    HTTP_STATE_SET
}HTTPStattypedef;

extern HTTPStattypedef HTTPState;
extern uint8_t IsHTTPRes,HTTPConnectFlag;



uint8_t HTTP_Connect(TCPSocketTypedef *socket);
uint8_t HTTP_Setup(char* IP, uint16_t port);
uint8_t HTTPS_Setup(char* IP, int port, nwy_app_ssl_conf_t* ssl);
uint8_t HTTP_Post(uint8_t KeepAlive, uint8_t type, char *data, int datalen, uint8_t IsHTTPS);
uint8_t HTTP_WaitResponce(void);
uint8_t HTTP_Close(uint8_t IsSecure);
void HTTPThreadEntry(void *param);


#endif

