

// TCP.c
#include "TCP.h"

TCPSocketTypedef ServerSocket[TCP_MAX_SOCKETS] = {{0}};
static uint8_t tcp_callbacks_registered = 0;

uint8_t TCP_IsAnySocketConnected(void)
{
    int i;

    for (i = 0; i < TCP_MAX_SOCKETS; ++i)
    {
        if (ServerSocket[i].SocketState == SOCKET_CONNECTED)
            return TRUE;
    }

    return FALSE;
}

// Helper function to get current time in seconds
static uint32_t GetCurrentTime(void) {
    return  Ql_GetMsSincePwrOn() / 1000;
}

TCPSocketTypedef* GetSocketByIndex(int index) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (ServerSocket[i].SocketIndex < 0)
            continue;
        if (ServerSocket[i].Port == 0)
            continue;
        if (ServerSocket[i].SocketIndex == index) {
            return &ServerSocket[i];
        }
    }
    return NULL;
}

TCPSocketTypedef* GetSocketByNo(int socketNo) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (ServerSocket[i].SocketNo == socketNo) {
            return &ServerSocket[i];
        }
    }
    return NULL;
}

void CallBack(connectionupdatecb cb, int socketno) {
    if (cb != NULL) {
        cb(socketno);
    }
}

void TCP_CloseALLSockets(void) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (ServerSocket[i].SocketState >= SOCKET_OPEN && 
            ServerSocket[i].SocketIndex >= 0) {
            Ql_SOC_Close(ServerSocket[i].SocketIndex);
            ServerSocket[i].SocketState = SOCKET_IDLE;
            ServerSocket[i].SocketIndex = -1;
        }
        ServerSocket[i].isEnabled=0; //BEWARE THIS WILL DISABLE ALL SOCKETS
    }
}

// ==================== CALLBACK FUNCTIONS ====================

void callback_socket_connect(s32 socketId, s32 errCode, void* customParam) {
    TCPSocketTypedef *socket = GetSocketByIndex(socketId);
    if (socket == NULL) {
        LOGData(TAG_TCP, "<--Callback: socket not found for socketId %d-->", socketId);
        return;
    }
    
    if (errCode == SOC_SUCCESS) {
        LOGData(TAG_TCP, "<--Callback: socket %d connected successfully-->", socket->SocketNo);
        socket->SocketState = SOCKET_CONNECTED;
        
        // Note: Failure count reset is handled at GPRS activation, not TCP connection
        // TCP success depends on server availability, but GPRS active proves operator granted data
        
        // Initialize ACK tracking for new connection
        socket->rval.last_ack_number = 0;
        socket->rval.noackcount = 0;
        socket->rval.connectedSince = GetCurrentTime();
        socket->rval.lastRxTime = GetCurrentTime();
        socket->rval.sendsSinceRx = 0;

        CallBack(socket->OnConnect, socket->SocketNo);
    } else {
        LOGData(TAG_TCP, "<--Callback: socket %d connect failed, errCode=%d-->", 
                socket->SocketNo, errCode);
        socket->SocketState = SOCKET_ERROR;
    }
}

void callback_socket_close(s32 socketId, s32 errCode, void* customParam) {
    TCPSocketTypedef *socket = GetSocketByIndex(socketId);
    if (socket == NULL) {
        LOGData(TAG_TCP, "<--Callback: socket not found for socketId %d-->", socketId);
        return;
    }
    
    LOGData(TAG_TCP, "<--Callback: socket %d closed, errCode=%d-->", 
            socket->SocketNo, errCode);
    
    // Reset ACK tracking when socket closes
    socket->rval.last_ack_number = 0;
    socket->rval.noackcount = 0;
    
    socket->SocketState = SOCKET_IDLE;
    socket->SocketIndex = -1;
    CallBack(socket->OnDisconnect, socket->SocketNo);
}

void callback_socket_accept(s32 listenSocketId, s32 errCode, void* customParam) {
    // Not used for client sockets
}

void callback_socket_read(s32 socketId, s32 errCode, void* customParam) {
    uint8_t m_recv_buf[RECV_BUFFER_LEN];
    TCPSocketTypedef *socket = GetSocketByIndex(socketId);
    
    if (socket == NULL) {
        LOGData(TAG_TCP, "<--Callback: socket not found for socketId %d-->", socketId);
        return;
    }
    
    if (errCode) {
        LOGData(TAG_TCP, "<--Callback: socket %d read error=%d-->", 
                socket->SocketNo, errCode);
        socket->SocketState = SOCKET_ERROR;
        return;
    }

    s32 ret;
    Ql_memset(m_recv_buf, 0, RECV_BUFFER_LEN);
    
    do {
        ret = Ql_SOC_Recv(socketId, m_recv_buf, RECV_BUFFER_LEN);
        
        if (ret < 0 && ret != -2) {
            LOGData(TAG_TCP, "<--Socket %d recv failed, ret=%d-->", 
                    socket->SocketNo, ret);
            socket->SocketState = SOCKET_ERROR;
            break;
        } else if (ret == -2) {
            // No more data available
            break;
        } else if (ret > 0) {
            LOGData(TAG_TCP, "<--Socket %d received %d bytes-->", 
                    socket->SocketNo, ret);
            
            if (socket->rxBuffer == NULL) {
                LOGData(TAG_TCP, "Socket %d: RX buffer not configured", socket->SocketNo);
                break;
            }
            
            if (socket->rxSizeMAX < ret) {
                LOGData(TAG_TCP, "Socket %d: RX buffer overflow", socket->SocketNo);
                break;
            }
            
            if (socket->isRXData) {
                LOGData(TAG_TCP, "Socket %d: Previous data not consumed", socket->SocketNo);
                break;
            }
            
            Ql_memcpy(socket->rxBuffer, m_recv_buf, ret);
            socket->isRXData = 1;
            socket->rval.lastRxTime = GetCurrentTime();
            socket->rval.sendsSinceRx = 0;
            
            if (ret < RECV_BUFFER_LEN) {
                break;  // No more data
            }
        }
    } while (1);
}

void callback_socket_write(s32 socketId, s32 errCode, void* customParam) {
    TCPSocketTypedef *socket = GetSocketByIndex(socketId);
    
    if (socket == NULL) {
        LOGData(TAG_TCP, "<--Callback: socket not found for socketId %d-->", socketId);
        return;
    }
    
    if (errCode) {
        LOGData(TAG_TCP, "<--Callback: socket %d write error=%d-->", 
                socket->SocketNo, errCode);
        socket->SocketState = SOCKET_ERROR;
    }
}

ST_SOC_Callback callback_soc_func = {
    callback_socket_connect,
    callback_socket_close,
    callback_socket_accept,
    callback_socket_read,    
    callback_socket_write
};

// ==================== DNS CALLBACK ====================

void dns_cb(u8 contexId, u8 requestId, s32 errCode, u32 ipAddrCnt, u32* ipAddr) {
    u8* ipSegment = (u8*)ipAddr;
    
    LOGData(TAG_TCP, "<--DNS callback: contextId=%d, requestId=%d, error=%d, ipCount=%d-->",
            contexId, requestId, errCode, ipAddrCnt);
    
    if (requestId >= TCP_MAX_SOCKETS) {
        LOGData(TAG_TCP, "<--DNS: Invalid requestId=%d-->", requestId);
        return;
    }
    
    TCPSocketTypedef *socket = GetSocketByNo(requestId);
    if (socket == NULL) {
        LOGData(TAG_TCP, "<--DNS: Socket not found for requestId=%d-->", requestId);
        return;
    }
    
    if (errCode == SOC_SUCCESS && ipAddrCnt > 0) {
        LOGData(TAG_TCP, "<--DNS resolved for socket %d: %d.%d.%d.%d-->",
                socket->SocketNo, ipSegment[0], ipSegment[1], ipSegment[2], ipSegment[3]);
        
        Ql_memcpy(socket->rval.m_ipaddress, ipSegment, 4);
        socket->rval.dnsresolved = 1;
    } else {
        LOGData(TAG_TCP, "<--DNS failed for socket %d, errCode=%d-->", 
                socket->SocketNo, errCode);
        socket->rval.dnsresolved = 0;
        socket->SocketState = SOCKET_ERROR;
    }
}

// ==================== SOCKET STATE MACHINE ====================

void TCPSocket_Process(TCPSocketTypedef* socket) {
    int ret;
    uint32_t currentTime = GetCurrentTime();
    uint32_t elapsedTime;
    
    // Check if socket is disabled
    if (!socket->isEnabled) {
        if (socket->SocketState != SOCKET_CLOSED) {
            LOGData(TAG_TCP, "Socket %d disabled by user", socket->SocketNo);
            if (socket->SocketIndex >= 0) {
                Ql_SOC_Close(socket->SocketIndex);
                socket->SocketIndex = -1;
            }
            socket->SocketState = SOCKET_CLOSED;
        }
        return;
    }
    
    // Check if IP is set to "NA" (disabled)
    if (socket->DNSorIP[0] == 'N' && socket->DNSorIP[1] == 'A') {
        if (socket->SocketState >= SOCKET_IDLE) {
            if (socket->SocketIndex >= 0) {
                Ql_SOC_Close(socket->SocketIndex);
                socket->SocketIndex = -1;
            }
            //LOGData(TAG_TCP, "Socket %d disabled due to IP=NA", socket->SocketNo);
            socket->SocketState = SOCKET_CLOSED;
        }
        return;
    }
    
    // Check if sleep mode is enabled
    if (SleepConfig.IsEnabled) {
        if (socket->SocketState >= SOCKET_OPEN) {
            if (socket->SocketIndex >= 0) {
                Ql_SOC_Close(socket->SocketIndex);
                socket->SocketIndex = -1;
            }
            socket->SocketState = SOCKET_IDLE;
        }
        return;
    }
    
    // Check GPRS status
    if (GSM.GSMState < GPRS_ACTIVE) {
        if (socket->SocketState >= SOCKET_OPEN) {
            if (socket->SocketIndex >= 0) {
                Ql_SOC_Close(socket->SocketIndex);
                socket->SocketIndex = -1;
            }
            socket->SocketState = SOCKET_IDLE;
        }
        return;
    }
    
    // State machine
    switch (socket->SocketState) {
        case SOCKET_CLOSED:
            // Reset backoff delay for fresh start
            socket->rval.reconnect_delay_ms = 0;
            socket->SocketState = SOCKET_IDLE;
            socket->rval.stateTimestamp = currentTime;
            break;
            
        case SOCKET_IDLE:
            // Check if we need to wait for reconnect backoff delay
            if (socket->rval.reconnect_delay_ms > 0) {
                elapsedTime = currentTime - socket->rval.stateTimestamp; // elapsed time in seconds
                uint32_t reconnect_delay_sec = socket->rval.reconnect_delay_ms / 1000; // convert ms to seconds
                if (elapsedTime < reconnect_delay_sec) {
                    // Still in backoff period, skip this socket for now
                    return;
                }
                // Backoff period expired, proceed with reconnection
            }
            
            // Register callbacks once
            if (!tcp_callbacks_registered) {
                ret = Ql_SOC_Register(callback_soc_func, NULL);
                if (ret == SOC_SUCCESS || ret == SOC_ALREADY) {
                    LOGData(TAG_TCP, "Socket callbacks registered");
                    tcp_callbacks_registered = 1;
                } else {
                    LOGData(TAG_TCP, "Failed to register callbacks, ret=%d", ret);
                    break;
                }
            }
            
            // Create socket - close existing descriptor first if any
            if (socket->SocketIndex >= 0) {
                Ql_SOC_Close(socket->SocketIndex);
                socket->SocketIndex = -1;
            }
            socket->SocketIndex = Ql_SOC_Create(0, SOC_TYPE_TCP);
            if (socket->SocketIndex < 0) {
                LOGData(TAG_TCP, "Socket %d create failed", socket->SocketNo);
                socket->SocketState = SOCKET_ERROR;
                break;
            }
            
            LOGData(TAG_TCP, "Socket %d created, index=%d", 
                    socket->SocketNo, socket->SocketIndex);
            
            // Try to convert IP directly
            Ql_memset(socket->rval.m_ipaddress, 0, sizeof(socket->rval.m_ipaddress));
            ret = Ql_IpHelper_ConvertIpAddr((u8*)socket->DNSorIP, 
                                           (u32*)socket->rval.m_ipaddress);
            
            if (ret == SOC_SUCCESS) {
                LOGData(TAG_TCP, "Socket %d IP: %d.%d.%d.%d", socket->SocketNo,
                        socket->rval.m_ipaddress[0], socket->rval.m_ipaddress[1],
                        socket->rval.m_ipaddress[2], socket->rval.m_ipaddress[3]);
                socket->rval.dnsresolved = 1;
                socket->SocketState = SOCKET_OPEN;
            } else {
                // Need DNS resolution
                socket->rval.dnsresolved = 0;
                ret = Ql_IpHelper_GetIPByHostName(0, socket->SocketNo, 
                                                   (u8*)socket->DNSorIP, dns_cb);
                
                if (ret == SOC_SUCCESS) {
                    LOGData(TAG_TCP, "Socket %d DNS resolved instantly", socket->SocketNo);
                    socket->SocketState = SOCKET_OPEN;
                } else if (ret == SOC_WOULDBLOCK) {
                    LOGData(TAG_TCP, "Socket %d DNS pending for: %s", 
                            socket->SocketNo, socket->DNSorIP);
                    socket->SocketState = SOCKET_DNS_PENDING;
                    socket->rval.stateTimestamp = currentTime;
                } else {
                    LOGData(TAG_TCP, "Socket %d DNS failed, ret=%d", 
                            socket->SocketNo, ret);
                    socket->SocketState = SOCKET_ERROR;
                }
            }
            break;
            
        case SOCKET_DNS_PENDING:
            // Check timeout
            elapsedTime = currentTime - socket->rval.stateTimestamp;
            if (elapsedTime > TCPSOCKET_DNS_TIMEOUT) {
                LOGData(TAG_TCP, "Socket %d DNS timeout", socket->SocketNo);
                socket->SocketState = SOCKET_ERROR;
            } else if (socket->rval.dnsresolved) {
                LOGData(TAG_TCP, "Socket %d DNS resolved", socket->SocketNo);
                socket->SocketState = SOCKET_OPEN;
            }
            break;
            
        case SOCKET_OPEN:
            // Initiate connection
            LOGData(TAG_TCP, "Socket %d connecting to %d.%d.%d.%d:%d",
                    socket->SocketNo,
                    socket->rval.m_ipaddress[0], socket->rval.m_ipaddress[1],
                    socket->rval.m_ipaddress[2], socket->rval.m_ipaddress[3],
                    socket->Port);
            
            ret = Ql_SOC_Connect(socket->SocketIndex, 
                                (u32)socket->rval.m_ipaddress, socket->Port);
            
            if (ret == SOC_SUCCESS) {
                LOGData(TAG_TCP, "Socket %d connected immediately", socket->SocketNo);
                socket->SocketState = SOCKET_CONNECTED;
                socket->rval.connectedSince = currentTime;
                socket->rval.lastRxTime = currentTime;
                socket->rval.sendsSinceRx = 0;
                socket->rval.last_ack_number = 0;
                socket->rval.noackcount = 0;

                CallBack(socket->OnConnect, socket->SocketNo);
            } else if (ret == SOC_WOULDBLOCK) {
                LOGData(TAG_TCP, "Socket %d connection pending", socket->SocketNo);
                socket->SocketState = SOCKET_CONNECTING;
                socket->rval.stateTimestamp = currentTime;
            } else {
                LOGData(TAG_TCP, "Socket %d connect failed, ret=%d", 
                        socket->SocketNo, ret);
                socket->SocketState = SOCKET_ERROR;
            }
            break;
            
        case SOCKET_CONNECTING:
            // Check timeout
            elapsedTime = currentTime - socket->rval.stateTimestamp;
            if (elapsedTime > TCPSOCKET_CONNECT_TIMEOUT) {
                LOGData(TAG_TCP, "Socket %d connect timeout", socket->SocketNo);
                socket->SocketState = SOCKET_ERROR;
            }
            // State will be updated by callback
            break;
            
        case SOCKET_CONNECTED:
            // Reset exponential backoff delay on successful connection
            socket->rval.reconnect_delay_ms = 0;

            // Zombie connection detection: if we've sent many packets
            // but received nothing from the server, the TCP session may be
            // half-open (NAT timeout, server closed its end silently).
            // Force reconnect after 30+ unanswered sends AND 10 minutes
            // with no server response. Grace period: skip check in first
            // 2 minutes after connect (server may not respond immediately).
            elapsedTime = currentTime - socket->rval.connectedSince;
            if (elapsedTime > 120 && socket->rval.sendsSinceRx >= 30) {
                uint32_t silentTime = currentTime - socket->rval.lastRxTime;
                if (silentTime > 600) {
                    LOGData(TAG_TCP, "Socket %d zombie detected: %d sends without server response in %lus, forcing reconnect",
                            socket->SocketNo, socket->rval.sendsSinceRx, (unsigned long)silentTime);
                    socket->SocketState = SOCKET_ERROR;
                    break;
                }
            }

            // Call user's connected callback
            CallBack(socket->Connected, socket->SocketNo);
            break;
            
        case SOCKET_ERROR:
            LOGData(TAG_TCP, "Socket %d in error state, closing", socket->SocketNo);
            if (socket->SocketIndex >= 0) {
                Ql_SOC_Close(socket->SocketIndex);
                socket->SocketIndex = -1;
            }
            
            // Reset ACK tracking on socket error/close
            socket->rval.last_ack_number = 0;
            socket->rval.noackcount = 0;
            
            // Exponential backoff on timeout
            if (socket->rval.reconnect_delay_ms == 0) {
                socket->rval.reconnect_delay_ms = 2000;
            } else {
                socket->rval.reconnect_delay_ms *= 2;
                if (socket->rval.reconnect_delay_ms > 60000) {
                    socket->rval.reconnect_delay_ms = 60000;
                }
            }
            
            LOGData(TAG_TCP, "Socket %d will retry in %d ms", 
                    socket->SocketNo, socket->rval.reconnect_delay_ms);
            
            // Store the timestamp when we can retry (instead of blocking)
            socket->rval.stateTimestamp = currentTime;
            socket->SocketState = SOCKET_IDLE;
            break;
            
        default:
            break;
    }
}

uint8_t TCPSocket_SendString(TCPSocketTypedef* socket, char* str) {
    int ret = 0;
    int totalSent = 0;
    int len = Ql_strlen(str);
    int attempts = 0;

    if (socket->SocketIndex < 0) {
        LOGData(TAG_TCP, "Socket %d Disabled for sending data", socket->SocketNo);
        return 0;
    }

    if (socket->SocketState != SOCKET_CONNECTED) {
        LOGData(TAG_TCP, "Socket %d not connected for sending data", socket->SocketNo);
        return 0;
    }

    while (totalSent < len && attempts < 3) {
        ret = Ql_SOC_Send(socket->SocketIndex, (u8*)(str + totalSent), len - totalSent);
        
        if (ret > 0) {
            totalSent += ret;
            attempts = 0;  // Reset attempts on successful partial send
        } else if (ret == 0) {
            attempts++;
            ThreadSleep(100);
        } else {
            LOGData(TAG_TCP, "Socket %d send failed, ret=%d", socket->SocketNo, ret);
            socket->SocketState = SOCKET_ERROR;
            return 0;
        }
    }

    if (totalSent == len) {
        LOGData(TAG_TCP, "Socket %d sent %d bytes", socket->SocketNo, totalSent);
        if (socket->rval.sendsSinceRx < 255)
            socket->rval.sendsSinceRx++;
        print_long_string(str);
        
        // Wait for ACK to ensure data is delivered
        if (!TCPSocket_WaitAck(socket, totalSent)) {
            LOGData(TAG_TCP, "Socket %d ACK wait timed out, updating last_ack_number", socket->SocketNo);
            u64 ack_num = 0;
            if (Ql_SOC_GetAckNumber(socket->SocketIndex, &ack_num) == SOC_SUCCESS) {
                socket->rval.last_ack_number = ack_num;
            }
            // If the socket is still connected, treat 100% transmitted bytes as success to prevent queue locks
            if (socket->SocketState == SOCKET_CONNECTED) {
                return 1;
            }
            return 0;
        }
        
        return 1;
    } else {
        LOGData(TAG_TCP, "Socket %d send incomplete: %d/%d bytes", 
                socket->SocketNo, totalSent, len);
        socket->SocketState = SOCKET_ERROR;
        return 0;
    }
}

uint8_t TCPSocket_SendStringNoAck(TCPSocketTypedef* socket, char* str) {
    int ret = 0;
    int totalSent = 0;
    int len = Ql_strlen(str);
    int attempts = 0;

    if (socket == NULL || str == NULL) {
        return 0;
    }

    if (socket->SocketIndex < 0) {
        LOGData(TAG_TCP, "Socket %d Disabled for sending data NoAck", socket->SocketNo);
        return 0;
    }

    if (socket->SocketState != SOCKET_CONNECTED) {
        LOGData(TAG_TCP, "Socket %d not connected for sending data NoAck", socket->SocketNo);
        return 0;
    }

    while (totalSent < len && attempts < 3) {
        ret = Ql_SOC_Send(socket->SocketIndex, (u8*)(str + totalSent), len - totalSent);
        
        if (ret > 0) {
            totalSent += ret;
            attempts = 0;  // Reset attempts on successful partial send
        } else if (ret == 0) {
            attempts++;
            ThreadSleep(100);
        } else {
            LOGData(TAG_TCP, "Socket %d SendNoAck failed, ret=%d", socket->SocketNo, ret);
            socket->SocketState = SOCKET_ERROR;
            return 0;
        }
    }

    if (totalSent == len) {
        LOGData(TAG_TCP, "Socket %d sent %d bytes (NoAck)", socket->SocketNo, totalSent);
        if (socket->rval.sendsSinceRx < 255)
            socket->rval.sendsSinceRx++;
        print_long_string(str);
        
        u64 ack_num = 0;
        if (Ql_SOC_GetAckNumber(socket->SocketIndex, &ack_num) == SOC_SUCCESS) {
            socket->rval.last_ack_number = ack_num;
        }
        
        return 1;
    } else {
        LOGData(TAG_TCP, "Socket %d SendNoAck incomplete: %d/%d bytes", 
                socket->SocketNo, totalSent, len);
        socket->SocketState = SOCKET_ERROR;
        return 0;
    }
}

uint8_t TCPSocket_WaitAck(TCPSocketTypedef* socket, int sent_len) {
    if (socket == NULL || socket->SocketState != SOCKET_CONNECTED) {
        LOGData(TAG_TCP, "Socket invalid or not connected for ACK wait...");
        return 0;
    }

    if (sent_len <= 0) {
        LOGData(TAG_TCP, "Socket %d invalid sent_len: %d", socket->SocketNo, sent_len);
        return 1;  // Nothing to acknowledge
    }

    const int timeout_ms = 15000;   // max wait time (increased to 15s for 2G/GPRS latency)
    const int step_ms    = 100;    // polling interval (100ms)
    const int log_interval_ms = 1000;  // log every 1 second

    int waited = 0;
    int last_log_time = 0;
    u64 ack_num = 0;
    u64 expected_ack = 0;
    s32 ret;

    // Get the expected ACK number (previous ACK + bytes just sent)
    expected_ack = socket->rval.last_ack_number + sent_len;

    LOGData(TAG_TCP, "Socket %d waiting for ACK, sent %d bytes, expecting ACK: %llu (prev: %llu)", 
            socket->SocketNo, sent_len, expected_ack, socket->rval.last_ack_number);

    while (waited < timeout_ms) {
        ret = Ql_SOC_GetAckNumber(socket->SocketIndex, &ack_num);
        
        if (ret != SOC_SUCCESS) {
            LOGData(TAG_TCP, "Socket %d failed to get ACK number, ret=%d", 
                    socket->SocketNo, ret);
            break;
        }

        // Only log every second instead of every 100ms
        if ((waited - last_log_time) >= log_interval_ms) {
            LOGData(TAG_TCP, "Socket %d current ACK: %llu, expected: %llu", 
                    socket->SocketNo, ack_num, expected_ack);
            last_log_time = waited;
        }

        // Check if the cumulative ACK has reached or exceeded expected value
        if (ack_num >= expected_ack) {
            LOGData(TAG_TCP, "Socket %d ACK received, %llu bytes total acknowledged", 
                    socket->SocketNo, ack_num);
            socket->rval.noackcount = 0;
            socket->rval.last_ack_number = ack_num;  // Update for next send
            return 1;   // success
        }

        ThreadSleep(step_ms);  // let the modem breathe
        waited += step_ms;
    }

    LOGData(TAG_TCP, "Socket %d ACK wait timeout! ACK at %llu, expected %llu (difference: %lld)", 
            socket->SocketNo, ack_num, expected_ack, (s64)(expected_ack - ack_num));
    
    if (socket->rval.noackcount++ > 5) {
        LOGData(TAG_TCP, "Socket %d No ACK count exceeded (%d), marking as error...", 
                socket->SocketNo, socket->rval.noackcount);
        socket->SocketState = SOCKET_ERROR;
        socket->rval.noackcount = 0;
    }
    
    return 0;   // timeout
}

// ==================== THREAD ENTRY ====================

void TCPThreadEntry(s32 taskId) {
    int i;
    
    tcp_thread_init(taskId);
    
    LOGData(TAG_TCP, "TCP thread started, managing %d sockets", TCP_MAX_SOCKETS);
    
    // Wait for GPRS to become active
    ThreadSleep(5000);
    while (GSM.GSMState < GPRS_ACTIVE) {
        ThreadSleep(500);
    }
    LOGData(TAG_TCP, "GPRS active, starting socket processing");
    
    // Main loop - process all sockets
    while (1) {
        while(prfReq!=NONE)
		{
			LOGData(TAG_SERVER,"\r\nTCP Thread Paused For Profile Update");
			ThreadSleep(500);
		}
        for (i = 0; i < TCP_MAX_SOCKETS; i++) {
            TCPSocket_Process(&ServerSocket[i]);
        }
        
        // Small delay to prevent tight looping
        ThreadSleep(100);
    }
}

void tcp_thread_init(u32 taskId) {
    s32 ret;
    OSThread TCP_Thread = {0};
    
    TCP_Thread.taskId = taskId;     
    Ql_strcpy(TCP_Thread.taskName, "TCP Multi");
    TCP_Thread.taskEnable = 1;
    TCP_Thread.taskState = TASK_STATE_NORMAL;
    TCP_Thread.taskPriority = 1;
    
    ret = InitializeThread(&TCP_Thread);
    if (ret != 1) {
        LOGData(TAG_TCP, "Failed to initialize TCP thread");
        return;
    }
    
    LOGData(TAG_TCP, "TCP multi-socket thread initialized");
}
