#ifndef _UDP_H
#define _UDP_H


#include "project.h"
#include "GPRS.h"


/**
 * UDP Socket Structure 
 * @param SocketIndex - Internal Socket Handle
 * @param SocketNo  - Socket Number (no internal function)
 * @param SocketState - Current Socket State - CLOSED, OPEN or CONNECTED
 * @param Port - Current Port Number
 * @param DNSorIP - Current IP or Host Name
 * @param SOCKET_IPtype - Current Socket IP Type - IP4 or IP6
*/
typedef struct 
{
    uint8_t isEnabled;
    int SocketIndex;
    uint8_t SocketNo;
    uint8_t InProcess;
    enum {UDP_SOCKET_CLOSED,UDP_SOCKET_OPEN,UDP_SOCKET_CONNECTED} SocketState;
    int Port;
    char DNSorIP[100];
    SocketIPTypedef SOCKET_IPType;
    char *rxBuffer;
    uint16_t rxSizeMAX;
    uint8_t isRXData;
    runtimeValTypedef rVals;
    connectionupdatecb OnConnect;
    connectionupdatecb Connected;
    connectionupdatecb OnDisconnect;
    
}UDPSocketTypedef;



/**
 * UDP Send String
 * @note Sends String to the Socket
 * @param UDPSocketTypedef Socket 
 * @param char *data 
 * @return 1 on succes, 0 on fail
*/
uint8_t UDPSocket_SendData(UDPSocketTypedef* socket,uint8_t *data, int size);
uint8_t UDPSocket_OPEN(UDPSocketTypedef* socket);
uint8_t UDPSocket_Disconnect(UDPSocketTypedef *socket);
void nwy_udp_check_func(UDPSocketTypedef* socket);


#endif

