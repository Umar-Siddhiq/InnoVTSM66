#include "project.h"
#include "sys/time.h"
#ifdef USE_TCP

// MAX_TCP_SOCKETS now defined in TCP.h

// int af_inet_flag = AF_INET;
// static struct sockaddr_in sa_v4;
// static struct sockaddr_in6 sa_v6;

#ifdef _ONLINE_DBG_
uint8_t TCP_Debug(TCPSocketTypedef *socket, const char *fmt, ...)
{
    va_list args;
    char buffer[512];
    char output[576]; // Extra space for header
    int ret = 0;

    if (socket == NULL || socket->SocketIndex < 0 || fmt == NULL)
    {
        nwy_dbg_log("TCP_Debug: Invalid parameters");
        return NWY_GEN_E_UNKNOWN;
    }

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    snprintf(output, sizeof(output), "%s:DBG:%s",NetWork.IMEI,buffer);

    ret = TCPSocket_SendString(socket, output);
    if (ret == 0)
    {
        nwy_dbg_log("TCP_Debug: Failed to send data");
        return NWY_GEN_E_UNKNOWN;
    }

    return NWY_SUCESS;
}
#endif


static nwy_ip_type_or_dns_enum nwy_judge_ip_or_dns(char *str)
{
    int len = 0;
    int strLen = 0;
    nwy_ip_type_or_dns_enum retValue = NWY_CUSTOM_IP_TYPE_OR_DNS_DNS;
    if (str == NULL)
    {
        return NWY_CUSTOM_IP_TYPE_OR_DNS_NONE;
    }
    else
    {
        if (strlen(str) <= 0)
        {
            return NWY_CUSTOM_IP_TYPE_OR_DNS_NONE;
        }
    }
    strLen = strlen(str);
    for (len = 0; len < strLen; len++)
    {
        if (((*(str + len) >= '0') && (*(str + len) <= '9')) || (*(str + len) == '.'))
        {
            continue;
        }
        else
        {
            break;
        }
    }
    if (len == strLen)
    {
        retValue = NWY_CUSTOM_IP_TYPE_OR_DNS_IPV4;
        return retValue;
    }
    len = 0;
    for (len = 0; len < strLen; len++)
    {
        if (((*(str + len) >= '0') && (*(str + len) <= '9')) ||
            ((*(str + len) >= 'a') && (*(str + len) <= 'f')) ||
            ((*(str + len) >= 'A') && (*(str + len) <= 'F')) ||
            (*(str + len) == ':'))
        {
            continue;
        }
        else
        {
            break;
        }
    }
    if (len == strLen)
    {
        retValue = NWY_CUSTOM_IP_TYPE_OR_DNS_IPV6;
        return retValue;
    }
    return retValue;
}
static int nwy_hostname_check(char *hostname)
{
    int a, b, c, d;
    char temp[32] = {0};
    if (strlen(hostname) > 15)
        return NWY_GEN_E_UNKNOWN;
    if ((sscanf(hostname, "%d.%d.%d.%d", &a, &b, &c, &d)) != 4)
        return NWY_GEN_E_UNKNOWN;
    if (!((a <= 255 && a >= 0) && (b <= 255 && b >= 0) && (c <= 255 && c >= 0)))
        return NWY_GEN_E_UNKNOWN;
    sprintf(temp, "%d.%d.%d.%d", a, b, c, d);

    strcpy(hostname, temp);
    return NWY_SUCESS;
}
static int nwy_get_ip_str(char *url_or_ip, char *ip_str, int *isipv6)
{
    char *str = NULL;
    nwy_ip_type_or_dns_enum ip_dns_type = NWY_CUSTOM_IP_TYPE_OR_DNS_NONE;

    ip_dns_type = nwy_judge_ip_or_dns(url_or_ip);
    if (ip_dns_type == NWY_CUSTOM_IP_TYPE_OR_DNS_DNS)
    {
        char *fn = strchr(url_or_ip,'/');
        if(fn)
            *fn=0;
        str = nwy_sdk_gethostbyname1(url_or_ip, isipv6);
        if (str == NULL || 0 == strlen(str))
        {
            nwy_dbg_log("input ip or url %s invalid", url_or_ip);
            return NWY_GEN_E_UNKNOWN;
        }
        memcpy(ip_str, str, strlen(str));

        NWY_CLI_LOG("%s get ip:%s", url_or_ip, ip_str);
    }
    else
    {
        memcpy(ip_str, url_or_ip, strlen(url_or_ip));
    }
    if (strchr(ip_str, ':') != NULL)
    {
        *isipv6 = 1;
    }
    else
    {
        *isipv6 = 0;
    }
    return NWY_SUCESS;
}

// static int nwy_cli_socket_destory(int *socketid)
// {
//     int ret = 0;
//     static int flag = 0;
//     static nwy_osi_mutex_t mutex;

//     if (flag == 0) {
//         nwy_create_mutex(&mutex);
//         flag = 1;
//     }
//     nwy_lock_mutex(mutex, NWY_OSA_SUSPEND);

//     if (*socketid <= 0) {
//         NWY_CLI_LOG("nwy_cli_socket_destory:socket has closed");
//         nwy_unlock_mutex(mutex);
//         return 0;
//     }

//     ret = nwy_sdk_socket_close(*socketid);
//     if (ret != NWY_SUCESS)
//     {
//         nwy_dbg_log("Socket close fail");

//         nwy_unlock_mutex(mutex);
//         return ret;
//     }
//     *socketid = 0;
//     nwy_dbg_log("Socket close sucess");

//     nwy_unlock_mutex(mutex);
//     return ret;
// }

static int nwy_cli_socket_destory(int *socketid)
{
    int ret = 0;
    
    if (socketid == NULL || *socketid <= 0)
    {
        // Socket already closed or invalid
        return NWY_SUCESS;
    }
    
    int socket_to_close = *socketid;  // Save for logging
    
    // Set to 0 FIRST, before any system calls
    *socketid = 0;
    
    ret = nwy_sdk_socket_close(socket_to_close);
    if (ret != NWY_SUCESS)
    {
        nwy_dbg_log("Socket Index %d close fail", socket_to_close);
        return ret;
    }
    
    nwy_dbg_log("Socket Index %d close success", socket_to_close);
    
    // Critical delay - factory example uses 1000ms after every close
    // We use 100ms to allow SDK to fully process the close
    nwy_sleep(100);
    
    return ret;
}



nwy_osi_mutex_t tcpSelectMutex = NULL;
// Non-blocking check version for async manager (timeout in ms)
void nwy_tcp_check_func_nonblock(TCPSocketTypedef* socket, int timeout_ms)
{
    // Validate socket state before proceeding
    if (socket == NULL || socket->SocketIndex <= 0 || socket->SocketState != SOCKET_CONNECTED)
    {
        return;
    }
    
    // Additional validation - check if socket descriptor is in valid range
    if(socket->SocketIndex >= FD_SETSIZE)
    {
        nwy_dbg_log("Socket %d has invalid descriptor %d, closing", socket->SocketNo, socket->SocketIndex);
        socket->SocketIndex = 0;
        socket->SocketState = SOCKET_CLOSED;
        return;
    }
    
    char recv_buff[700];
    int recv_len = 0, result = 0;
    fd_set rd_fd;
    fd_set ex_fd;
    FD_ZERO(&rd_fd);
    FD_ZERO(&ex_fd);
    FD_SET(socket->SocketIndex, &rd_fd);
    FD_SET(socket->SocketIndex, &ex_fd);
    struct timeval tv = {0};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
   
    if (CheckGPRSState() == 0) {
        nwy_dbg_log("Data call disconnect");
        nwy_dbg_log("Socket index %d Closing due to GPRS disconnect...", socket->SocketIndex);
        nwy_cli_socket_destory(&socket->SocketIndex);
        socket->SocketIndex = 0;
        socket->SocketState = SOCKET_CLOSED;
        return;
    }
    result = nwy_sdk_socket_select(socket->SocketIndex + 1, &rd_fd, NULL, &ex_fd, &tv);
    if (result < 0)
    {
        nwy_dbg_log("tcp select error on socket index %d", socket->SocketIndex);
        nwy_dbg_log("Socket index %d Closing due to select error...", socket->SocketIndex);
        nwy_cli_socket_destory(&socket->SocketIndex);
        socket->SocketIndex = 0;
        socket->SocketState = SOCKET_CLOSED;
        return;
    }
    else if (result > 0)
    {
        if (FD_ISSET(socket->SocketIndex, &rd_fd))
        {
            memset(recv_buff, 0, 700);
            recv_len = nwy_sdk_socket_recv(socket->SocketIndex, recv_buff, 700, 0);
            if (recv_len > 0)
            {
                if(socket->rxBuffer!=NULL)
                {
                    //Succesfully Received
                    nwy_dbg_log("socket Index %i read[%d]:",socket->SocketIndex, recv_len);
                    print_long_string(recv_buff);
                    if(recv_len > socket->rxSizeMAX)
                        nwy_dbg_log("socket rcv data len %d > max %d, ignoring!",recv_len,socket->rxSizeMAX);
                    else
                    {
                        strcpy(socket->rxBuffer,recv_buff);
                        socket->isRXData=1;
                    }
                }
                else
                {
                    nwy_dbg_log("socket no %d Index %i rcv not supported, ignored but printing",socket->SocketNo,socket->SocketIndex, recv_len, recv_buff);
                    print_long_string(recv_buff);
                }
            }
            else if (recv_len == 0)
            {
                nwy_dbg_log("socket no %d Index %i disconnected",socket->SocketNo,socket->SocketIndex);
                nwy_dbg_log("Socket index %d Closing due to graceful disconnect...", socket->SocketIndex);
                nwy_cli_socket_destory(&socket->SocketIndex);
                socket->SocketIndex = 0;
                socket->SocketState = SOCKET_CLOSED;
                return;
            }
            else
            {
                nwy_dbg_log("socket no %d Index %i recv error %d", socket->SocketNo, socket->SocketIndex, recv_len);
                nwy_dbg_log("Socket index %d Closing due to recv error...", socket->SocketIndex);
                nwy_cli_socket_destory(&socket->SocketIndex);
                socket->SocketIndex = 0;
                socket->SocketState = SOCKET_CLOSED;
                return;
            }
        }
        if (FD_ISSET(socket->SocketIndex, &ex_fd))
        {
            nwy_dbg_log("socket no %d Index %i exception", socket->SocketNo, socket->SocketIndex);
            nwy_dbg_log("Socket index %d Closing due to exception...", socket->SocketIndex);
            nwy_cli_socket_destory(&socket->SocketIndex);
            socket->SocketIndex = 0;
            socket->SocketState = SOCKET_CLOSED;
            return;
        }
    }
}

void nwy_tcp_check_func(TCPSocketTypedef* socket)
{
    // Validate socket state before proceeding
    if (socket == NULL || socket->SocketIndex <= 0 || socket->SocketState != SOCKET_CONNECTED)
    {
        nwy_dbg_log("tcp_check_func: Invalid socket state (Index=%d, State=%d)", 
                    socket ? socket->SocketIndex : -1, 
                    socket ? socket->SocketState : -1);
        return;
    }
    
    char recv_buff[700];
    int recv_len = 0, result = 0;
    fd_set rd_fd;
    fd_set ex_fd;
    FD_ZERO(&rd_fd);
    FD_ZERO(&ex_fd);
    FD_SET(socket->SocketIndex, &rd_fd);
    FD_SET(socket->SocketIndex, &ex_fd);
    struct timeval tv = {0};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
   
    if (CheckGPRSState() == 0) {
        nwy_dbg_log("Data call disconnect");
        nwy_dbg_log("Socket index %d Closing due to GPRS disconnect...", socket->SocketIndex);
        nwy_cli_socket_destory(&socket->SocketIndex);
        socket->SocketIndex = 0;
        socket->SocketState = SOCKET_CLOSED;
        goto RET;
    }
    result = nwy_sdk_socket_select(socket->SocketIndex + 1, &rd_fd, NULL, &ex_fd, &tv);
    if (result < 0)
    {
        nwy_dbg_log("tcp select error on socket index %d", socket->SocketIndex);
        nwy_dbg_log("Socket index %d Closing due to select error...", socket->SocketIndex);
        nwy_cli_socket_destory(&socket->SocketIndex);
        socket->SocketIndex = 0;
        socket->SocketState = SOCKET_CLOSED;
        goto RET;
    }
    else if (result > 0)
    {
        if (FD_ISSET(socket->SocketIndex, &rd_fd))
        {
            memset(recv_buff, 0, 700);
            recv_len = nwy_sdk_socket_recv(socket->SocketIndex, recv_buff, 700, 0);
            if (recv_len > 0)
            {
                if(socket->rxBuffer!=NULL)
                {
                    //Succesfully Received
                    nwy_dbg_log("socket Index %i read[%d]:",socket->SocketIndex, recv_len);
                    print_long_string(recv_buff);
                    if(recv_len > socket->rxSizeMAX)
                        nwy_dbg_log("socket rcv data len %d > max %d, ignoring!",recv_len,socket->rxSizeMAX);
                    else
                    {
                        strcpy(socket->rxBuffer,recv_buff);
                        socket->isRXData=1;
                    }
                }
                else
                {
                    nwy_dbg_log("socket no %d Index %i rcv not supported, ignored but printing",socket->SocketNo,socket->SocketIndex, recv_len, recv_buff);
                    print_long_string(recv_buff);
                }
            }
            else if (recv_len == 0)
            {
                // Server closed connection gracefully
                nwy_dbg_log("tcp srvdc cb");
                nwy_dbg_log("Socket index %d Disconnected by Server",socket->SocketIndex);
                nwy_cli_socket_destory(&socket->SocketIndex);
                socket->SocketIndex = 0;
                socket->SocketState = SOCKET_CLOSED;
                if(socket->OnDisconnect != NULL)
                    CallBack(socket->OnDisconnect);
                goto RET;
            }
            else
            {
                // recv_len < 0 - error occurred
                nwy_dbg_log("tcp cc cb - recv error %d", recv_len);
                nwy_dbg_log("Socket index %d Connection Closed",socket->SocketIndex);
                nwy_cli_socket_destory(&socket->SocketIndex);
                socket->SocketIndex = 0;
                socket->SocketState = SOCKET_CLOSED;
                if(socket->OnDisconnect != NULL)
                    CallBack(socket->OnDisconnect);
                goto RET;
            }
        }
        if (FD_ISSET(socket->SocketIndex, &ex_fd))
        {
            // Exception on socket - must close and cleanup
            nwy_dbg_log("tcp ex fd");
            nwy_dbg_log("Socket index %d Disconnected ex fd",socket->SocketIndex);
            nwy_cli_socket_destory(&socket->SocketIndex);
            socket->SocketIndex = 0;
            socket->SocketState = SOCKET_CLOSED;
            if(socket->OnDisconnect != NULL)
                CallBack(socket->OnDisconnect);
            goto RET;
        }
    }
    // else
    //     nwy_dbg_log("TCP Socket Index %d select timeout!",socket->SocketIndex);

    RET:
    //nwy_unlock_mutex(tcpSelectMutex);
    return;
}



uint8_t TCPSocket_OPEN(TCPSocketTypedef* socket)
{
    char ip_buf[256] = {0};
    char ip_cpy[256] = {0};
    int ret = 0;
    ip_addr_t addr;
    memset(&addr, 0, sizeof(addr));
    
    if(socket == NULL)
    {
        nwy_dbg_log("TCPSocket_OPEN: Invalid socket");
        return 0;
    }
    
    // Check if GPRS is active before attempting any connection
    if(GSM.GSMState < GPRS_ACTIVE)
    {
        nwy_dbg_log("TCPSocket_OPEN: GPRS not active (state=%d), cannot open socket %d", 
                   GSM.GSMState, socket->SocketNo);
        return 0;
    }
    
    // Safely copy DNS/IP with bounds checking
    if(strlen(socket->DNSorIP) >= sizeof(ip_cpy))
    {
        nwy_dbg_log("TCPSocket_OPEN: DNS/IP too long (%d bytes)", strlen(socket->DNSorIP));
        return 0;
    }
    strncpy(ip_cpy, socket->DNSorIP, sizeof(ip_cpy) - 1);
    ip_cpy[sizeof(ip_cpy) - 1] = '\0';  // Ensure null termination
    
    // Extract hostname only (strip path if URL contains '/path')
    // e.g., "surakshamitr.org/CDC" -> "surakshamitr.org"
    char *path_separator = strchr(ip_cpy, '/');
    if(path_separator != NULL)
    {
        *path_separator = '\0';  // Terminate at '/' to get hostname only
        nwy_dbg_log("TCPSocket_OPEN: Extracted hostname: %s (from %s)", ip_cpy, socket->DNSorIP);
    }
    
    // Ensure any previous socket is properly closed
    if(socket->SocketIndex > 0)
    {
        nwy_dbg_log("Cleaning up previous socket Index %d before opening new one", socket->SocketIndex);
        nwy_cli_socket_destory(&socket->SocketIndex);
        socket->SocketIndex = 0;
        nwy_sleep(200);  // Give system time to fully close the socket
    }
    
    ret = nwy_get_ip_str(ip_cpy, ip_buf, (int*)&socket->SOCKET_IPType);
    if (ret != NWY_SUCESS) {
        nwy_dbg_log("TCPSocket_OPEN: nwy_get_ip_str failed for %s", ip_cpy);
        return 0;
    }
    
    if (socket->SOCKET_IPType) {
        if (nwy_ipv6_addr_aton( ip_buf,&addr.u_addr.ip6) == 0) {
            nwy_dbg_log("2.TCP Socket %d input ip or url is invalid",socket->SocketNo);
            return 0;
        } else {

            inet6_addr_from_ip6addr(&socket->rVals.sa_v6.sin6_addr, ip_2_ip6(&addr));
            socket->rVals.sa_v6.sin6_len = sizeof(struct sockaddr_in);
            socket->rVals.sa_v6.sin6_family = AF_INET6;
            socket->rVals.sa_v6.sin6_port = htons(socket->Port);
            socket->rVals.af_inet_flag = AF_INET6;
        }
    } else {
        ret = nwy_hostname_check(ip_buf);
        if (ret != NWY_SUCESS) {
            nwy_dbg_log("3.TCP Socket %d input ip or url is invalid", socket->SocketNo);
            return 0;
        }

        if (nwy_ipv4_addr_aton(ip_buf,(ip_addr_t *) &addr.u_addr.ip4) == 0)
        {
            nwy_dbg_log("4.TCP Socket %d ip error",socket->SocketNo);
            return 0;
        }
        inet_addr_from_ip4addr(&socket->rVals.sa_v4.sin_addr, ip_2_ip4(&addr));
        socket->rVals.sa_v4.sin_len = sizeof(struct sockaddr_in);
        socket->rVals.sa_v4.sin_family = AF_INET;
        socket->rVals.sa_v4.sin_port = htons(socket->Port);
        socket->rVals.af_inet_flag = AF_INET;
    }
    
    // Create new socket (SocketIndex should be 0 at this point)
    ret = nwy_sdk_socket_open(socket->rVals.af_inet_flag, SOCK_STREAM, IPPROTO_TCP);
    if(ret < 0){
        int errno_val = nwy_sdk_socket_errno();
        nwy_dbg_log("5.TCP Socket %d Open Fail! errno=%d (EMFILE=24, ENFILE=23, ENOBUFS=105, ENOMEM=12)",
                   socket->SocketNo, errno_val);
        socket->SocketIndex = 0;
        socket->SocketState = SOCKET_CLOSED;
        return 0;
    }
    nwy_dbg_log("5.Socket Index : %d Open Success!",ret);
    socket->SocketIndex = ret;
    socket->SocketState = SOCKET_OPEN;
    return 1;
}

void TCPSocket_PollConnection(TCPSocketTypedef* socket)
{
    if(socket == NULL || socket->SocketState != SOCKET_CONNECTING)
        return;
    
    // Check for connection timeout
    uint32_t now = (uint32_t)nwy_get_ms();  // Use 32-bit for simpler math
    
    // Safety check: if connect_start_time is 0, initialize it now
    if(socket->connect_start_time == 0)
    {
        socket->connect_start_time = now;
        socket->connect_attempt = 0;  // First attempt
        return; // Give it at least one full timeout period
    }
    
    // Handle wraparound safely
    uint32_t start = (uint32_t)socket->connect_start_time;
    uint32_t elapsed = now - start;
    
    // Sanity check - if elapsed is huge, reset (probably uninitialized)
    if(elapsed > 60000)
    {
        socket->connect_start_time = now;
        socket->connect_attempt = 0;
        return;
    }
    
    // Step-up timeout: 3s first attempt, 5s second attempt
    // Connection is the main bottleneck for HTTP - give more time
    #ifdef PROTO_CDAC
    uint32_t current_timeout = 5000;  // Fixed 5s timeout for CDAC
    #else
    uint32_t current_timeout = (socket->connect_attempt == 0) ? 3000 : 5000;
    #endif
    
    if(elapsed >= current_timeout)
    {
        nwy_dbg_log("TCP Socket %d (Index %d) connection timeout after %u ms (attempt %d)", 
                   socket->SocketNo, socket->SocketIndex, elapsed, socket->connect_attempt + 1);
        if(socket->SocketIndex > 0)
        {
            nwy_cli_socket_destory(&socket->SocketIndex);
            socket->SocketIndex = 0;
        }
        socket->SocketState = SOCKET_CLOSED;
        socket->connect_start_time = 0;
#ifdef PROTO_CDAC
        // CDAC: Keep connect_attempt for step-up timeout, but no reconnect delay
        socket->connect_attempt++;  // Track attempt for timeout step-up
        socket->reconnect_delay_ms = 0;
        socket->last_close_time = 0;
#else
        socket->last_close_time = nwy_get_ms();
        socket->connect_attempt++;  // Track attempt for next retry
        
        // Only apply backoff after multiple failed attempts
        if(socket->connect_attempt >= 2)
        {
            if(socket->reconnect_delay_ms == 0)
                socket->reconnect_delay_ms = 2000;
            else {
                socket->reconnect_delay_ms *= 2;
                if(socket->reconnect_delay_ms > 60000)
                    socket->reconnect_delay_ms = 60000;
            }
            nwy_dbg_log("TCP Socket %d next reconnect delay: %dms", 
                       socket->SocketNo, socket->reconnect_delay_ms);
        }
        else
        {
            // First timeout - allow immediate retry (no backoff)
            socket->reconnect_delay_ms = 0;
            socket->last_close_time = 0;
            nwy_dbg_log("TCP Socket %d first timeout, allowing immediate retry", socket->SocketNo);
        }
#endif
        
        return;
    }
    
    // Poll connection status by calling connect() again
    int ret;
    if (socket->rVals.af_inet_flag == AF_INET6) {
        ret = nwy_sdk_socket_connect(socket->SocketIndex, 
                                     (struct sockaddr *)&socket->rVals.sa_v6, 
                                     sizeof(socket->rVals.sa_v6));
    } else {
        ret = nwy_sdk_socket_connect(socket->SocketIndex, 
                                     (struct sockaddr *)&socket->rVals.sa_v4, 
                                     sizeof(socket->rVals.sa_v4));
    }
    
    if(ret == NWY_SUCESS)
    {
        // Connection completed successfully
        nwy_dbg_log("TCP Socket %d connection completed (NWY_SUCESS)", socket->SocketNo);
        socket->SocketState = SOCKET_CONNECTED;
        socket->connect_start_time = 0;
        socket->connect_attempt = 0;  // Reset attempt counter on success
        socket->reconnect_delay_ms = 2000;  // Reset backoff delay on successful connection
        if(socket->OnConnect != NULL)
            CallBack(socket->OnConnect);
    }
    else
    {
        int errno_val = nwy_sdk_socket_errno();
        if(errno_val == EISCONN)
        {
            // Already connected
            nwy_dbg_log("TCP Socket %d connection completed (EISCONN)", socket->SocketNo);
            socket->SocketState = SOCKET_CONNECTED;
            socket->connect_start_time = 0;
            socket->connect_attempt = 0;  // Reset attempt counter on success
            socket->reconnect_delay_ms = 2000;  // Reset backoff delay on successful connection
            if(socket->OnConnect != NULL)
                CallBack(socket->OnConnect);
        }
        else if(errno_val == EINPROGRESS || errno_val == EALREADY)
        {
            // Still in progress - keep waiting
            // Don't log repeatedly to avoid spam
        }
        else
        {
            // Connection failed (ECONNREFUSED=111, ETIMEDOUT=110, ENETUNREACH=101, etc.)
            nwy_dbg_log("TCP Socket %d connection failed during CONNECTING (errno=%d)", 
                       socket->SocketNo, errno_val);
            if(socket->SocketIndex > 0)
            {
                nwy_cli_socket_destory(&socket->SocketIndex);
                socket->SocketIndex = 0;
            }
            socket->SocketState = SOCKET_CLOSED;
            socket->connect_start_time = 0;
            
#ifdef PROTO_CDAC
            // CDAC: No reconnect delay - pre-connect handles timing
            socket->reconnect_delay_ms = 0;
            socket->last_close_time = 0;
#else
            socket->last_close_time = nwy_get_ms();
            
            // Exponential backoff on connection error
            if(socket->reconnect_delay_ms == 0)
                socket->reconnect_delay_ms = 2000;
            else {
                socket->reconnect_delay_ms *= 2;
                if(socket->reconnect_delay_ms > 60000)
                    socket->reconnect_delay_ms = 60000;
            }
            nwy_dbg_log("TCP Socket %d next reconnect delay: %dms", 
                       socket->SocketNo, socket->reconnect_delay_ms);
#endif
        }
    }
}

uint8_t TCPSocket_CONNECT(TCPSocketTypedef* socket)
{
    int on = 1;
    int opt = 1;
    int ret = 0;
    
    nwy_sdk_socket_setsockopt(socket->SocketIndex, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on));
    nwy_sdk_socket_setsockopt(socket->SocketIndex, IPPROTO_TCP, TCP_NODELAY, (void *)&opt, sizeof(opt));
    if (0 != nwy_sdk_socket_set_nonblock(socket->SocketIndex))
    {
        nwy_dbg_log("6.TCP Socket %d set nonblock err",socket->SocketNo);
        return 0;
    }
    
   
    nwy_dbg_log("Socket %d Connect to %s:%d",socket->SocketNo,socket->DNSorIP,socket->Port);
    
    // Start non-blocking connect
    if (socket->rVals.af_inet_flag == AF_INET6) {
        ret = nwy_sdk_socket_connect(socket->SocketIndex, (struct sockaddr *)&socket->rVals.sa_v6, sizeof(socket->rVals.sa_v6));
    } else {
        ret = nwy_sdk_socket_connect(socket->SocketIndex, (struct sockaddr *)&socket->rVals.sa_v4, sizeof(socket->rVals.sa_v4));
    }

    if(ret == NWY_SUCESS)
    {
        // Connected immediately (rare but possible)
        nwy_dbg_log("8.TCP Socket %d Connect immediate success",socket->SocketNo);
        socket->SocketState = SOCKET_CONNECTED;
        socket->reconnect_delay_ms = 2000;  // Reset backoff delay on successful connection
        if(socket->OnConnect != NULL)
            CallBack(socket->OnConnect);
        return 1;
    }
    else
    {
        int errno_val = nwy_sdk_socket_errno();
        if(errno_val == EISCONN)
        {
            // Already connected
            nwy_dbg_log("8.TCP Socket %d Connect ok (already connected)",socket->SocketNo);
            socket->SocketState = SOCKET_CONNECTED;
            socket->reconnect_delay_ms = 2000;  // Reset backoff delay on successful connection
            if(socket->OnConnect != NULL)
                CallBack(socket->OnConnect);
            return 1;
        }
        else if(errno_val == EINPROGRESS || errno_val == EALREADY)
        {
            // Connection in progress - this is expected for non-blocking
            nwy_dbg_log("TCP Socket %d Connect in progress (errno=%d)",socket->SocketNo, errno_val);
            socket->SocketState = SOCKET_CONNECTING;
            socket->connect_start_time = nwy_get_ms();
            return 1; // Success - connection started
        }
        else
        {
            // Connection failed immediately (including ECONNREFUSED from server rejection)
            nwy_dbg_log("9.TCP Socket %d connect errno = %d",socket->SocketNo, errno_val);
            nwy_dbg_log("Socket Index %d Closing due to connection error...",socket->SocketIndex);
            nwy_cli_socket_destory(&socket->SocketIndex);
            socket->SocketIndex = 0;
            socket->SocketState = SOCKET_CLOSED;
            socket->connect_start_time = 0;
            
#ifdef PROTO_CDAC
            // CDAC: No reconnect delay - pre-connect handles timing
            socket->reconnect_delay_ms = 0;
            socket->last_close_time = 0;
#else
            socket->last_close_time = nwy_get_ms();  // Prevent rapid reconnect
            
            // Exponential backoff on immediate connection failure
            if(socket->reconnect_delay_ms == 0)
                socket->reconnect_delay_ms = 2000;
            else {
                socket->reconnect_delay_ms *= 2;
                if(socket->reconnect_delay_ms > 60000)
                    socket->reconnect_delay_ms = 60000;
            }
            nwy_dbg_log("TCP Socket %d next reconnect delay: %dms", 
                       socket->SocketNo, socket->reconnect_delay_ms);
#endif
            
            return 0;
        }
    }
}

void CallBack(ptr cb)
{
    cb();
}

uint8_t TCPSocket_WaitAck(TCPSocketTypedef* socket)
{
    if (socket == NULL || socket->SocketState != SOCKET_CONNECTED)
    {
        nwy_dbg_log("Socket invalid or not connected for ACK wait...");
        return 0;
    }

    const int timeout_ms = 3000;   // max wait time 
    const int step_ms    = 100;    // polling interval (100ms)

    int waited = 0;

    while (waited < timeout_ms)
    {
        int sent_len = nwy_sdk_socket_get_tcp_sent_size(socket->SocketIndex);
        int ack_len  = nwy_sdk_socket_get_tcp_ack_size(socket->SocketIndex);

        if (sent_len == ack_len)
        {
            nwy_dbg_log("Socket %d ACK received, sent_len=%d, ack_len=%d", socket->SocketNo, sent_len, ack_len);
            socket->noackcount = 0;
            return 1;   // success
        }

        nwy_sleep(step_ms);  // let the modem breathe
        waited += step_ms;
    }

    nwy_dbg_log("Socket %d ACK wait timeout!", socket->SocketNo);
    // ACK timeout means connection is broken - disconnect immediately for faster retry
    nwy_dbg_log("Socket %d disconnecting due to ACK timeout...", socket->SocketNo);
    TCPSocket_Disconnect(socket);
    socket->noackcount = 0;
    return 0;   // timeout
}

uint8_t TCPSocket_SendString(TCPSocketTypedef* socket,char *data)
{
    int send_len;
    
    if(socket == NULL || data == NULL || strlen(data) == 0)
    {
        nwy_dbg_log("Socket Send: Invalid parameters");
        return 0;
    }
    
    if(socket->SocketState != SOCKET_CONNECTED || socket->SocketIndex <= 0)
    {
        nwy_dbg_log("Socket %d Not Connected for Sending Data (State=%d, Index=%d)", 
                    socket->SocketNo, socket->SocketState, socket->SocketIndex);
        return 0;
    }

    send_len = nwy_sdk_socket_send(socket->SocketIndex, data, strlen(data), 0);
    if(send_len > 0)
    {
        nwy_dbg_log("Socket %d, Index : %d, Sent Data[%d] : ",socket->SocketNo,socket->SocketIndex,strlen(data));
        //nwy_dbg_log("%s",data);
        print_long_string(data);
        if (!TCPSocket_WaitAck(socket)) {
            return 0;  // send failed due to ack timeout
        }
        
        return 1;
    }
    
    // Send failed - close socket properly
    nwy_dbg_log("Socket %d Data Send FAIL! (send_len=%d, errno=%d)", 
                socket->SocketNo, send_len, nwy_sdk_socket_errno());
    nwy_dbg_log("Socket Index %d Closing due to send failure...", socket->SocketIndex);
    nwy_cli_socket_destory(&socket->SocketIndex);
    socket->SocketIndex = 0;
    socket->SocketState = SOCKET_CLOSED;
    return 0;
    
}


// Fire-and-forget send for backup server - no ACK wait needed
uint8_t TCPSocket_SendStringNoAck(TCPSocketTypedef* socket, char *data)
{
    int send_len;
    
    if(socket == NULL || data == NULL || strlen(data) == 0)
    {
        return 0;
    }
    
    if(socket->SocketState != SOCKET_CONNECTED || socket->SocketIndex <= 0)
    {
        nwy_dbg_log("Backup Socket Not Connected (State=%d)", socket->SocketState);
        return 0;
    }

    send_len = nwy_sdk_socket_send(socket->SocketIndex, data, strlen(data), 0);
    if(send_len > 0)
    {
        nwy_dbg_log("Backup sent %d bytes (no ACK wait)", send_len);
        return 1;
    }
    
    // Send failed - close socket properly
    nwy_dbg_log("Backup send failed (errno=%d)", nwy_sdk_socket_errno());
    nwy_cli_socket_destory(&socket->SocketIndex);
    socket->SocketIndex = 0;
    socket->SocketState = SOCKET_CLOSED;
    return 0;
}


void TCPSocket_CheckState(TCPSocketTypedef* socket)
{
    int ret=0;
    
    if(socket == NULL || socket->SocketIndex <= 0)
    {
        nwy_dbg_log("CheckState: Invalid socket");
        return;
    }
    
    ret = nwy_sdk_socket_get_tcp_state(socket->SocketIndex);
    if(ret != NWY_ESTABLISHED)
    {
        nwy_dbg_log("Socket %d Disconnected ret : %d",socket->SocketNo,ret);    
        nwy_dbg_log("Socket Index %d Destroying due to non-established state", socket->SocketIndex);
        nwy_cli_socket_destory(&socket->SocketIndex);
        socket->SocketIndex = 0;
        socket->SocketState = SOCKET_CLOSED;
        if(socket->OnDisconnect != NULL)
            CallBack(socket->OnDisconnect);
    }
}
 
uint8_t TCPSocket_Disconnect(TCPSocketTypedef *socket)
{
    if (socket == NULL)
        return 0;
    
    if (socket->SocketIndex > 0)
    {
        nwy_dbg_log("Disconnecting socket %d, Index %d", socket->SocketNo, socket->SocketIndex);
        nwy_cli_socket_destory(&socket->SocketIndex);
        socket->SocketIndex = 0;
    }
    socket->SocketState = SOCKET_CLOSED;
    return 1;
}


void TCPAsyncManagerThread(void *param)
{
    extern TCPSocketTypedef ServerSocket[];
    
    nwy_dbg_log("TCP Async Manager Thread Started");
    nwy_sleep(2000);
    
    // Wait for GPRS to be active
    while(GSM.GSMState < GPRS_ACTIVE)
        nwy_sleep(500);
    
    nwy_dbg_log("TCP Manager: GPRS Active, starting socket management");
    
    while(1)
    {
        // Check global conditions first
        if(GSM.GSMState < GPRS_ACTIVE)
        {
            for(int i = 0; i < MAX_TCP_SOCKETS; i++)
            {
                if(ServerSocket[i].SocketState == SOCKET_CONNECTED || 
                   ServerSocket[i].SocketState == SOCKET_CONNECTING ||
                   ServerSocket[i].SocketState == SOCKET_OPEN)  // Also close OPEN sockets when GPRS down
                {
                    nwy_dbg_log("TCP Manager: GPRS down, closing socket %d (state=%d)", 
                               ServerSocket[i].SocketNo, ServerSocket[i].SocketState);
                    if(ServerSocket[i].SocketIndex > 0)
                    {
                        nwy_cli_socket_destory(&ServerSocket[i].SocketIndex);
                        ServerSocket[i].SocketIndex = 0;
                    }
                    ServerSocket[i].SocketState = SOCKET_CLOSED;
                    if(ServerSocket[i].OnDisconnect != NULL)
                        CallBack(ServerSocket[i].OnDisconnect);
                }
            }
            nwy_sleep(1000);
            continue;
        }
        
        // Process each socket's state machine
        for(int i = 0; i < MAX_TCP_SOCKETS; i++)
        {
            // Skip disabled or unused sockets
            if(!ServerSocket[i].isEnabled)
                continue;
            
            // Check waiting conditions
            if(VTSState.CurrentProfile == 0)
                continue;
                
            if(FTPState > FTP_STATE_CLOSED || IsFTPReq)
                continue;
                
            if(ServerSocket[i].DNSorIP[0] == 'N' && ServerSocket[i].DNSorIP[1] == 'A')
                continue;
            
            // Process socket state machine
            switch(ServerSocket[i].SocketState)
            {
                case SOCKET_CLOSED:
                {
#ifndef PROTO_CDAC
                    // Initialize reconnect delay if not set (first time or after reset)
                    if(ServerSocket[i].reconnect_delay_ms == 0)
                        ServerSocket[i].reconnect_delay_ms = 2000;  // Start with 2 seconds
                    
                    // Exponential backoff - wait longer after each failure
                    uint64_t now = nwy_get_ms();
                    if(ServerSocket[i].last_close_time > 0 && 
                       (now - ServerSocket[i].last_close_time) < ServerSocket[i].reconnect_delay_ms)
                    {
                        // Too soon after last close, skip this iteration
                        break;
                    }
#endif
                    
                    nwy_dbg_log("TCP Manager: Socket %d CLOSED, attempting open (delay was %dms)", 
                               ServerSocket[i].SocketNo, ServerSocket[i].reconnect_delay_ms);
                    
                    if(!TCPSocket_OPEN(&ServerSocket[i]))
                    {
                        nwy_dbg_log("TCP Manager: Socket %d open failed, marking closed", ServerSocket[i].SocketNo);
                        ServerSocket[i].SocketState = SOCKET_CLOSED;
#ifndef PROTO_CDAC
                        ServerSocket[i].last_close_time = nwy_get_ms();
                        
                        // Exponential backoff: double the delay, cap at 60 seconds
                        ServerSocket[i].reconnect_delay_ms *= 2;
                        if(ServerSocket[i].reconnect_delay_ms > 60000)
                            ServerSocket[i].reconnect_delay_ms = 60000;  // Cap at 60 seconds
                        
                        nwy_dbg_log("TCP Manager: Socket %d next reconnect delay: %dms", 
                                   ServerSocket[i].SocketNo, ServerSocket[i].reconnect_delay_ms);
#endif
                    }
                    break;
                }
                    
                case SOCKET_OPEN:
                    nwy_dbg_log("TCP Manager: Socket %d OPEN, attempting connect", ServerSocket[i].SocketNo);
                    TCPSocket_CONNECT(&ServerSocket[i]);
                    break;
                    
                case SOCKET_CONNECTING:
                    TCPSocket_PollConnection(&ServerSocket[i]);
                    break;
                    
                case SOCKET_CONNECTED:
                    // Call connected callback if available
                    if(ServerSocket[i].Connected != NULL)
                        CallBack(ServerSocket[i].Connected);
                    break;
                    
                default:
                    break;
            }
        }
        
        // Now check all connected sockets for data using a single multiplexed select()
        fd_set read_fds, except_fds;
        struct timeval tv;
        int max_fd = 0;
        int select_result;
        
        FD_ZERO(&read_fds);
        FD_ZERO(&except_fds);
        
        // Build fd_set for all connected sockets
        for(int i = 0; i < MAX_TCP_SOCKETS; i++)
        {
            if(ServerSocket[i].SocketState == SOCKET_CONNECTED && 
               ServerSocket[i].SocketIndex > 0 && 
               ServerSocket[i].SocketIndex < FD_SETSIZE)  // Validate socket descriptor
            {
                FD_SET(ServerSocket[i].SocketIndex, &read_fds);
                FD_SET(ServerSocket[i].SocketIndex, &except_fds);
                if(ServerSocket[i].SocketIndex > max_fd)
                    max_fd = ServerSocket[i].SocketIndex;
            }
        }
        
        // If we have connected sockets, check for data with short timeout
        if(max_fd > 0)
        {
            tv.tv_sec = 0;
            tv.tv_usec = 100000;  // 100ms timeout for data check
            
            select_result = nwy_sdk_socket_select(max_fd + 1, &read_fds, NULL, &except_fds, &tv);
            
            if(select_result < 0)
            {
                // Select failed - this is critical
                nwy_dbg_log("TCP Manager: select() failed, errno=%d", nwy_sdk_socket_errno());
                nwy_sleep(100);  // Brief delay before retry
            }
            else if(select_result > 0)
            {
                // Process sockets with data available
                for(int i = 0; i < MAX_TCP_SOCKETS; i++)
                {
                    if(ServerSocket[i].SocketState != SOCKET_CONNECTED || ServerSocket[i].SocketIndex <= 0)
                        continue;
                    
                    // Check for exceptions first
                    if(FD_ISSET(ServerSocket[i].SocketIndex, &except_fds))
                    {
                        nwy_dbg_log("Socket %d exception detected", ServerSocket[i].SocketNo);
                        nwy_cli_socket_destory(&ServerSocket[i].SocketIndex);
                        ServerSocket[i].SocketIndex = 0;
                        ServerSocket[i].SocketState = SOCKET_CLOSED;
                        ServerSocket[i].last_close_time = nwy_get_ms();
                        if(ServerSocket[i].OnDisconnect != NULL)
                            CallBack(ServerSocket[i].OnDisconnect);
                        continue;
                    }
                    
                    // Check for data
                    if(FD_ISSET(ServerSocket[i].SocketIndex, &read_fds))
                    {
                        // Use non-blocking check (0ms timeout)
                        nwy_tcp_check_func_nonblock(&ServerSocket[i], 0);
                    }
                }
            }
        }
        
        // Small sleep to prevent busy loop
        nwy_sleep(50);
    }
    
    nwy_exit_thread_self();
}

// Legacy compatibility - redirect to async manager
void TCPThreadEntry(void *param)
{
    // This is now just a wrapper that starts the async manager
    // param is ignored since we manage all sockets in one thread
    TCPAsyncManagerThread(param);
}

#endif
