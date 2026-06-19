#include "project.h"
#include "sys/time.h"
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
//         nwy_dbg_log("UDP Socket close fail");

//         nwy_unlock_mutex(mutex);
//         return ret;
//     }
//     *socketid = 0;
//     nwy_dbg_log("UDP Socket close sucess");

//     nwy_unlock_mutex(mutex);
//     return ret;
// }

static int nwy_cli_socket_destory(int *socketid)
{
    int ret = 0;
    ret = nwy_sdk_socket_close(*socketid);
    if (ret != NWY_SUCESS)
    {
        nwy_dbg_log("UDP Socket Index %d close fail",*socketid);
        return ret;
    }
    nwy_dbg_log("UDP Socket Index %d close sucess",*socketid);
    *socketid = 0;
    return ret;
}



nwy_osi_mutex_t udpSelectMutex = NULL;
void nwy_udp_check_func(UDPSocketTypedef* socket)
{
    // if (NULL == udpSelectMutex)
    //     nwy_create_mutex(&udpSelectMutex);
    // if (NULL == udpSelectMutex)
    //     return;
    // nwy_lock_mutex(udpSelectMutex, NWY_OSA_SUSPEND);
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
        //nwy_cli_socket_destory(&s_nwy_cli_udp_fd);
        
        socket->SocketState = UDP_SOCKET_CLOSED;

        //nwy_sleep(1000);
        goto RET;
    }
    result = nwy_sdk_socket_select(socket->SocketIndex + 1, &rd_fd, NULL, &ex_fd, &tv);
    if (result < 0)
    {
        nwy_dbg_log("udp select error:");
        //nwy_cli_socket_destory(&s_nwy_cli_udp_fd);
        socket->SocketState = UDP_SOCKET_CLOSED;
        //nwy_sleep(1000);
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
                    nwy_dbg_log("UDP Socket Index %i read[%d]:",socket->SocketIndex, recv_len);

                    if(recv_len > socket->rxSizeMAX)
                        nwy_dbg_log("UDP Socket rcv data len %d > max %d, ignoring!",recv_len,socket->rxSizeMAX);
                    else
                    {
                        memcpy(socket->rxBuffer,recv_buff,recv_len);
                        socket->isRXData=1;
                    }
                }
                else
                    nwy_dbg_log("UDP Socket no %d Index %i rcv not supported, ignoring...",socket->SocketNo,socket->SocketIndex);
            }
            else if (recv_len == 0)
            {
                
                //nwy_cli_socket_destory(&s_nwy_cli_udp_fd);
                nwy_dbg_log("udp srvdc cb");
                socket->SocketState = UDP_SOCKET_CLOSED;
                nwy_dbg_log("UDP Socket index %d Disconnected by Server",socket->SocketIndex);
                if(socket->OnDisconnect != NULL)
                    CallBack(socket->OnDisconnect);
                goto RET;
            }
            else
            {

                //nwy_cli_socket_destory(&s_nwy_cli_udp_fd);
                nwy_dbg_log("udp cc cb");
                socket->SocketState = UDP_SOCKET_CLOSED;
                nwy_dbg_log("UDP Socket index %d Connection Closed",socket->SocketIndex);
                if(socket->OnDisconnect != NULL)
                    CallBack(socket->OnDisconnect);
                goto RET;
            }
        }
        if (FD_ISSET(socket->SocketIndex, &ex_fd))
        {
            //nwy_cli_socket_destory(&s_nwy_cli_udp_fd);
            nwy_dbg_log("udp ex fd");
            socket->SocketState = UDP_SOCKET_CLOSED;
            nwy_dbg_log("UDP Socket index %d Disconnected ex fd",socket->SocketIndex);
            if(socket->OnDisconnect != NULL)
                CallBack(socket->OnDisconnect);
            goto RET;
        }
    }
    // else
    //     nwy_dbg_log("UDP Socket Index %d select timeout!",socket->SocketIndex);

    RET:
    //nwy_unlock_mutex(udpSelectMutex);
    return;
}



uint8_t UDPSocket_OPEN(UDPSocketTypedef* socket)
{
    char ip_buf[256] = {0};
    char ip_cpy[256];
    int ret=0;
    ip_addr_t addr = {0};
    strcpy(ip_cpy,socket->DNSorIP);
    if(socket->SocketIndex >=0)
        nwy_cli_socket_destory(&socket->SocketIndex);
    ret = nwy_get_ip_str(ip_cpy, ip_buf, (int*)&socket->SOCKET_IPType);
    if (ret != NWY_SUCESS) {
        return 0;
    }
    //w
    //nwy_ip6addr_ntoa()
    if (socket->SOCKET_IPType) {
        if (nwy_ipv6_addr_aton( ip_buf,&addr.u_addr.ip6) == 0) {
            nwy_dbg_log("2.UDP Socket %d input ip or url is invalid",socket->SocketNo);
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
            nwy_dbg_log("3.UDP Socket %d input ip or url is invalid", socket->SocketNo);
            return 0;
        }

        if (nwy_ipv4_addr_aton(ip_buf,(ip_addr_t *) &addr.u_addr.ip4) == 0)
        {
            nwy_dbg_log("4.UDP Socket %d ip error",socket->SocketNo);
            return 0;
        }
        inet_addr_from_ip4addr(&socket->rVals.sa_v4.sin_addr, ip_2_ip4(&addr));
        socket->rVals.sa_v4.sin_len = sizeof(struct sockaddr_in);
        socket->rVals.sa_v4.sin_family = AF_INET;
        socket->rVals.sa_v4.sin_port = htons(socket->Port);
        socket->rVals.af_inet_flag = AF_INET;
    }
    if(socket->SocketIndex == 0)
        ret= nwy_sdk_socket_open(socket->rVals.af_inet_flag, SOCK_DGRAM, IPPROTO_UDP);
    if(ret < 0){
        nwy_dbg_log("5.UDP Socket %d Open Fail!",socket->SocketNo);
        return 0;
    }
    nwy_dbg_log("5.Socket Index : %d Open Success!",ret);
    socket->SocketIndex = ret;
    socket->SocketState = UDP_SOCKET_OPEN;
    return 1;
}


void CallBack(ptr cb)
{
    cb();
}

uint8_t UDPSocket_SendData(UDPSocketTypedef* socket,uint8_t *data, int size)
{
    int send_len;
    if(socket->SocketState != SOCKET_OPEN)
    {
        nwy_dbg_log("UDP Socket %d Not Open for Sending Data...",socket->SocketNo);
        return 0;
    }
    send_len = nwy_sdk_socket_sendto(socket->SocketIndex, data, size, 0,(const struct sockaddr *)&socket->rVals.sa_v4,sizeof(socket->rVals.sa_v4));
    if(send_len > 0)
    {
        nwy_dbg_log("UDP Socket %d, Index : %d, Sent Data : ",socket->SocketNo,socket->SocketIndex);
        //nwy_dbg_log("%s",data);
        return 1;
    }

    nwy_dbg_log("UDP Socket %d Data Send FAIL!",socket->SocketNo);
    socket->SocketState = UDP_SOCKET_CLOSED;
    return 0;
    
}
 
uint8_t UDPSocket_Disconnect(UDPSocketTypedef *socket)
{
    nwy_cli_socket_destory(&socket->SocketIndex);
    socket->SocketState=UDP_SOCKET_CLOSED;
    return 1;
}
