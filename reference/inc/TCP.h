#ifndef _TCP_H
#define _TCP_H


#include "project.h"
#include "GPRS.h"

#define MAX_TCP_SOCKETS 4

typedef void (*connectionupdatecb)();
typedef enum {SOCKET_IP4,SOCKET_IP6}SocketIPTypedef;



typedef struct 
{
  int af_inet_flag;
  struct sockaddr_in sa_v4;
  struct sockaddr_in6 sa_v6;
}runtimeValTypedef;

/**
 * TCP Socket Structure 
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
    enum {SOCKET_CLOSED,SOCKET_OPEN,SOCKET_CONNECTING,SOCKET_CONNECTED} SocketState;
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
    int noackcount;
    uint64_t connect_start_time;  // Timestamp when connection started
    uint64_t last_close_time;     // Timestamp when socket was last closed
    uint32_t reconnect_delay_ms;  // Current reconnect delay (exponential backoff)
    uint8_t connect_attempt;      // Current connection attempt (for step-up timeout)
    
}TCPSocketTypedef;

extern TCPSocketTypedef ServerSocket[];


typedef enum
{
  NWY_CUSTOM_IP_TYPE_OR_DNS_NONE = -1,
  NWY_CUSTOM_IP_TYPE_OR_DNS_IPV4 = 0,
  NWY_CUSTOM_IP_TYPE_OR_DNS_IPV6 = 1,
  NWY_CUSTOM_IP_TYPE_OR_DNS_DNS = 2
}nwy_ip_type_or_dns_enum;


/**
 * TCP Main Thread (Legacy compatibility)
 * @note Now redirects to async manager - independent task, dont return
 * @param void* param (ignored in async mode)
*/
void TCPThreadEntry(void *param);

/**
 * TCP Async Manager Thread 
 * @note Manages all TCP sockets in single thread using select()
 * @param void* param (ignored)
*/
void TCPAsyncManagerThread(void *param);

/**
 * TCP Send String
 * @note Sends String to the Socket
 * @param TCPSocketTypedef Socket 
 * @param char *data 
 * @return 1 on succes, 0 on fail
*/
uint8_t TCPSocket_SendString(TCPSocketTypedef* socket,char *data);

uint8_t TCPSocket_SendStringNoAck(TCPSocketTypedef* socket, char *data);

uint8_t TCPSocket_OPEN(TCPSocketTypedef* socket);
uint8_t TCPSocket_CONNECT(TCPSocketTypedef* socket);
uint8_t TCPSocket_Disconnect(TCPSocketTypedef *socket);
void TCPSocket_PollConnection(TCPSocketTypedef* socket);
void TCPSocket_CheckState(TCPSocketTypedef* socket);
void nwy_tcp_check_func(TCPSocketTypedef* socket);
#endif

