// TCP.h
#ifndef _TCP_H
#define _TCP_H

#include "VTS.h"
#include "Hardware.h"
#include "Utilities.h"
#include "ql_socket.h"
#include "Systic.h"
#include "File.h"

#define RECV_BUFFER_LEN  1024
#define TCP_MAX_SOCKETS 4
#define TCPSOCKET_CONNECT_TIMEOUT 10  // seconds
#define TCPSOCKET_DNS_TIMEOUT 10      // seconds

typedef void (*connectionupdatecb)(int socketno);

typedef enum {
    SOCKET_CLOSED,
    SOCKET_IDLE,
    SOCKET_DNS_PENDING,
    SOCKET_PENDING_OPEN,
    SOCKET_OPEN,
    SOCKET_CONNECTING,
    SOCKET_CONNECTED,
    SOCKET_CLOSING,
    SOCKET_ERROR
} SocketState_e;

typedef struct {
    u8 m_ipaddress[5];
    uint8_t dnsresolved;
    uint32_t stateTimestamp;  // Timestamp for timeouts
    uint32_t reconnect_delay_ms;  // Exponential backoff delay in milliseconds
    u64 last_ack_number;  // Track last ACK number for send verification
    uint8_t noackcount;   // Counter for consecutive no-ACK events
} runtimeValTypedef;

/**
 * TCP Socket Structure 
 */
typedef struct {
    uint8_t isEnabled;
    int SocketIndex;
    uint8_t SocketNo;
    SocketState_e SocketState;
    SocketState_e PrevState;
    int Port;
    char DNSorIP[100];
    char *rxBuffer;
    uint16_t rxSizeMAX;
    uint8_t isRXData;
    runtimeValTypedef rval;
    connectionupdatecb OnConnect;
    connectionupdatecb Connected;
    connectionupdatecb OnDisconnect;
} TCPSocketTypedef;

extern TCPSocketTypedef ServerSocket[];

void CallBack(connectionupdatecb cb, int socketno);
void TCPThreadEntry(s32 taskId);
void tcp_thread_init(u32 taskId);
uint8_t TCPSocket_SendString(TCPSocketTypedef* socket, char* str);
uint8_t TCPSocket_WaitAck(TCPSocketTypedef* socket, int sent_len);
void TCP_CloseALLSockets(void);
void TCPSocket_Process(TCPSocketTypedef* socket);

#endif /* _TCP_H */