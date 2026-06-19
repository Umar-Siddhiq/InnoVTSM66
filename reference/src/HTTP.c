#include "HTTP.h"

HTTPStattypedef HTTPState;
int HTTPFlagState;
uint8_t IsHTTPRes, HTTPConnectFlag;
char path[128];
static uint8_t HTTPCurrentSecure;

#define HTTP_CONNECT_WAIT   12000

static uint8_t HTTP_IsSecurePort(uint16_t port)
{
    return (port == 443) ? 1 : 0;
}

static void HTTP_ExtractHostAndPath(const char* input, char* host, unsigned int host_sz, char* out_path, unsigned int path_sz)
{
    const char* start = input;
    const char* slash;
    unsigned int host_len;

    if(host_sz > 0)
        host[0] = '\0';
    if(path_sz > 0)
        out_path[0] = '\0';

    if(input == NULL)
        return;

    if(strncmp(start, "http://", 7) == 0)
        start += 7;
    else if(strncmp(start, "https://", 8) == 0)
        start += 8;

    slash = strchr(start, '/');
    if(slash == NULL)
    {
        if(host_sz > 0)
        {
            strncpy(host, start, host_sz - 1);
            host[host_sz - 1] = '\0';
        }
        if(path_sz > 0)
        {
            strncpy(out_path, "/", path_sz - 1);
            out_path[path_sz - 1] = '\0';
        }
        return;
    }

    host_len = (unsigned int)(slash - start);
    if(host_sz > 0)
    {
        if(host_len >= host_sz)
            host_len = host_sz - 1;
        memcpy(host, start, host_len);
        host[host_len] = '\0';
    }
    if(path_sz > 0)
    {
        strncpy(out_path, slash, path_sz - 1);
        out_path[path_sz - 1] = '\0';
    }
}

static void HTTP_BuildSetupUrl(const char* input, uint16_t port, char* out, unsigned int out_sz)
{
    char host[128];
    char local_path[128];

    if(out_sz == 0)
        return;
    out[0] = '\0';

    HTTP_ExtractHostAndPath(input, host, sizeof(host), local_path, sizeof(local_path));
    if(host[0] == '\0')
        return;

    snprintf(out, out_sz, "%s://%s%s", HTTP_IsSecurePort(port) ? "https" : "http", host, local_path);
}

void HTTPResetFlag(void)
{
    HTTPFlagState=0xA5A5;
}

uint8_t HTTPWaitFlag(int GoodFlag, uint16_t Timeout, int BadFlag)
{
    uint16_t tm = Timeout/5;
    while(HTTPFlagState!=GoodFlag)
    {
        if(--tm == 0)
        {
            nwy_dbg_log("HTTP Flag timeout for %d",GoodFlag);
            return 0;
        }
        if(HTTPFlagState == BadFlag)
            return 0;
        nwy_sleep(5);
    }
    return 1;
}

void nwy_http_result_cb(nwy_ftp_result_t *param)
{
    if(NULL == param)
    {
        nwy_dbg_log("event is NULL");
    }
    if(NWY_HTTP_DNS_ERR == param->event)
    {
        nwy_dbg_log("HTTP dns err");
        HTTPState = HTTP_STATE_NOTSET;
        ServerSocket[0].SocketState = SOCKET_CLOSED;
        HTTPFlagState = HTTP_EVENT_ERROR;
    }
    else if(NWY_HTTP_OPEN_FAIL == param->event)
    {
        nwy_dbg_log("HTTP open fail");
        HTTPState = HTTP_STATE_NOTSET;
        ServerSocket[0].SocketState = SOCKET_CLOSED;
        HTTPFlagState = HTTP_EVENT_ERROR;
    }
    else if(NWY_HTTP_OPENED == param->event )
    {
        nwy_dbg_log("HTTP setup success");
        HTTPState = HTTP_STATE_SET;
        ServerSocket[0].SocketState = SOCKET_CONNECTED;
        HTTPFlagState = HTTP_EVENT_SETUP;
    }
    else if(NWY_HTTPS_SSL_CONNECTED == param->event)
    {
        nwy_dbg_log("HTTPS setup success");
        HTTPState = HTTP_STATE_SET;
        ServerSocket[0].SocketState = SOCKET_CONNECTED;
        HTTPFlagState = HTTP_EVENT_SETUP;
        return;
    }
    else if(NWY_HTTP_CLOSED_PASV == param->event || NWY_HTTP_CLOSED == param->event)
    {
        nwy_dbg_log("HTTP closed");
        HTTPState = HTTP_STATE_NOTSET;
        ServerSocket[0].SocketState = SOCKET_CLOSED;
        HTTPFlagState = HTTP_EVENT_CLOSED;
        //ServerSocket[0].SocketState = SOCKET_CLOSED;
    }
    else if(NWY_HTTP_DATA_RECVED == param->event)
    {
        nwy_dbg_log("HTTP recv data len %d.\r\n",param->data_len);
        IsHTTPRes=1;
        if(!ServerSocket[0].isRXData && param->data_len < ServerSocket[0].rxSizeMAX)
        {
            strcat(ServerSocket[0].rxBuffer,param->data);
            ServerSocket[0].isRXData=1;
        }
        HTTPFlagState = HTTP_EVENT_RECEIVE;
        //Data Recieved Here
    }
    else if(NWY_HTTP_DATA_SEND_ERR == param->event)
    {
        nwy_dbg_log("data send error");
        HTTPFlagState = HTTP_EVENT_ERROR;
    }
    else if(NWY_HTTP_DATA_SEND_FINISHED == param->event)
    {
        nwy_dbg_log("data send finished");
        HTTPFlagState = HTTP_EVENT_SEND;
    }
    else if(NWY_HTTPS_SSL_INIT_ERROR == param->event)
    {
        nwy_dbg_log("HTTPS SSL init fail");
        HTTPState = HTTP_STATE_NOTSET;
        ServerSocket[0].SocketState = SOCKET_CLOSED;
        HTTPFlagState = HTTP_EVENT_ERROR;
    }
    else if(NWY_HTTPS_SSL_HANDSHAKE_ERROR == param->event)
    {
        nwy_dbg_log("HTTPS SSL handshare fail");
        HTTPState = HTTP_STATE_NOTSET;
        ServerSocket[0].SocketState = SOCKET_CLOSED;
        HTTPFlagState = HTTP_EVENT_ERROR;
    }
    else if(NWY_HTTPS_SSL_AUTH_FAIL == param->event)
    {
        nwy_dbg_log("HTTPS SSL Authentication fail");
        HTTPState = HTTP_STATE_NOTSET;
        ServerSocket[0].SocketState = SOCKET_CLOSED;
        HTTPFlagState = HTTP_EVENT_ERROR;
    }
    else
    {
        nwy_dbg_log("unkown error");
    }

    return;
}

uint8_t HTTP_Setup(char* IP, uint16_t port)
{
    char host[128];
    char setup_url[192];
    nwy_app_ssl_conf_t ssl_cfg;

    HTTP_ExtractHostAndPath(IP, host, sizeof(host), path, sizeof(path));
    HTTPCurrentSecure = HTTP_IsSecurePort(port);

#ifdef HTTP_SIMULATE
    if(!HTTPCurrentSecure)
    {
        if(path[0] == '\0')
            strcpy(path, "/");

        if(ServerSocket[0].SocketState == SOCKET_CONNECTED)
        {
            nwy_dbg_log("HTTP_Setup: Reusing existing connection (Keep-Alive)");
            return 1;
        }

        if(!TCPSocket_OPEN(&ServerSocket[0]))
            goto RT;
        if(!TCPSocket_CONNECT(&ServerSocket[0]))
            goto RT;

        HTTPState = HTTP_STATE_SET;
        return 1;
    }
#endif

    if(path[0] == '\0')
        strcpy(path, "/");

    HTTP_BuildSetupUrl(IP, port, setup_url, sizeof(setup_url));
    if(setup_url[0] == '\0')
    {
        nwy_dbg_log("HTTP Setup URL build error");
        return 0;
    }

    if(HTTPCurrentSecure)
    {
        memset(&ssl_cfg, 0, sizeof(ssl_cfg));
        ssl_cfg.ssl_version = NWY_VERSION_TLS_V1_2_E;
        ssl_cfg.authmode = NWY_SSL_AUTH_NONE_E;
        strncpy((char*)ssl_cfg.hostname, host, sizeof(ssl_cfg.hostname) - 1);
        ssl_cfg.hostname[sizeof(ssl_cfg.hostname) - 1] = '\0';
        return HTTPS_Setup(setup_url, port, &ssl_cfg);
    }

    nwy_dbg_log("HTTP Setup @%s, %u", setup_url, port);
    HTTPResetFlag();
    if(nwy_http_setup(HTTP_DEFAULT_CHANNEL, setup_url, port, (httpresultcb)nwy_http_result_cb) != NWY_SUCCESS)
    {
        nwy_dbg_log("HTTP Setup Error");
        return 0;
    }
    if(!HTTPWaitFlag(HTTP_EVENT_SETUP, HTTP_CONNECT_WAIT, HTTP_EVENT_ERROR))
    {
        HTTP_Close(0);
        nwy_dbg_log("HTTP Setup Callback Timeout!");
        return 2;
    }
    HTTPState = HTTP_STATE_SET;
    return 1;

#ifdef HTTP_SIMULATE
RT:
    SetStatLED(10,9);
    HTTPState = HTTP_STATE_NOTSET;
    return 0;
#endif
}

uint8_t HTTPS_Setup(char* IP, int port, nwy_app_ssl_conf_t* ssl)
{
    int ret;
    nwy_dbg_log("HTTPS Setup @%s, %u\n Auth Mode : %d",IP,port,ssl->authmode);
    HTTPResetFlag();
    ret = nwy_https_setup(HTTP_DEFAULT_CHANNEL,IP,port,(httpresultcb)nwy_http_result_cb,ssl);
    if(ret != 0)
    {
        nwy_dbg_log("HTTPS Setup Error ret : %i",ret);
        return 0;
    }

    if(!HTTPWaitFlag(HTTP_EVENT_SETUP,HTTP_CONNECT_WAIT,HTTP_EVENT_ERROR))
    {
        nwy_dbg_log("HTTPS Setup Callback Timeout!");
        if(HTTPFlagState == HTTP_EVENT_ERROR)
            HTTP_Close(1);
        return 2;
    }

    return 1;
}

uint8_t HTTP_Post(uint8_t KeepAlive, uint8_t type, char *data, int datalen, uint8_t IsSecure)
{
#ifdef HTTP_SIMULATE
    if(!IsSecure)
    {
        uint16_t size = datalen + 400;
        char* HTTPBuffer = malloc(size);
        char connectionsetting[80];
        char hostname[128];

        if(HTTPBuffer==NULL){
            nwy_dbg_log("HTTPsim buff malloc fail!");
            return 0;
        }

        HTTP_ExtractHostAndPath(ServerSocket[0].DNSorIP, hostname, sizeof(hostname), path, sizeof(path));
        if(path[0] == '\0')
            strcpy(path, "/");

        if(!KeepAlive)
            strcpy(connectionsetting,"Connection: close");
        else
            strcpy(connectionsetting,"Connection: Keep-Alive\r\nKeep-Alive: timeout=300, max=1000");

        snprintf(HTTPBuffer, size,
                 "POST %s HTTP/1.1\r\n"
                 "%s\r\n"
                 "Host: %s\r\n"
                 "Content-Length: %zu\r\n"
                 "Content-Type: application/x-www-form-urlencoded\r\n"
                 "Cache-Control: no-cache\r\n"
                 "\r\n"
                 "%s",
                 path, connectionsetting, hostname, datalen, data);

        if(!TCPSocket_SendString(&ServerSocket[0],HTTPBuffer)){
            nwy_dbg_log("HTTPsim send error!");
            free(HTTPBuffer);
            return 0;
        }
        free(HTTPBuffer);
        return 1;
    }
#endif

    if(HTTPState != HTTP_STATE_SET)
    {
        nwy_dbg_log("HTTP Cannot Post Without Setup!");
        ServerSocket[0].SocketState = SOCKET_CLOSED;
        return 0;
    } 
    nwy_dbg_log("HTTP Posting..., kp %d, tp %d, sc: %d",KeepAlive,type,IsSecure);
    HTTPResetFlag();
    if(nwy_http_post(HTTP_DEFAULT_CHANNEL,KeepAlive,type,data,datalen,IsSecure)!=NWY_SUCCESS)
    {
        nwy_dbg_log("HTTP Post Error");
        return 0;
    }

    if(!HTTPWaitFlag(HTTP_EVENT_SEND,HTTP_CONNECT_WAIT/2,HTTP_EVENT_ERROR))
    {
        nwy_dbg_log("HTTP Post Callback Timeout!");
        return 0;
    }
    return 1;
    
}

uint8_t HTTP_WaitResponce(void)
{
    if(!HTTPWaitFlag(HTTP_EVENT_RECEIVE,HTTP_CONNECT_WAIT/2,HTTP_EVENT_ERROR))
    {
        nwy_dbg_log("HTTP Responce Callback Timeout!");
        return 0;
    }
    return 1;
}


uint8_t HTTP_Close(uint8_t IsSecure)
{
    HTTPResetFlag();
#ifdef HTTP_SIMULATE
    if(!IsSecure)
    {
        if(ServerSocket[0].SocketState== SOCKET_CONNECTED)
			TCPSocket_Disconnect(&ServerSocket[0]);
        HTTPState = HTTP_STATE_NOTSET;
        ServerSocket[0].SocketState = SOCKET_CLOSED;
        HTTPCurrentSecure = 0;
        return 1;
    }
#endif

    nwy_dbg_log("Manually Closing HTTP...");
    if(nwy_http_close(HTTP_DEFAULT_CHANNEL,IsSecure)!=NWY_SUCCESS)
    {
        nwy_dbg_log("HTTP Close Error");
        return 0;
    }
    if(!HTTPWaitFlag(HTTP_EVENT_CLOSED,HTTP_CONNECT_WAIT/2,HTTP_EVENT_ERROR))
    {
        nwy_dbg_log("HTTP Close Callback Timeout!");
        return 0;
    }
    HTTPState = HTTP_STATE_NOTSET;
    ServerSocket[0].SocketState = SOCKET_CLOSED;
    HTTPCurrentSecure = 0;
    return 1;
}

uint8_t HTTP_Connect(TCPSocketTypedef *socket)
{
    if(socket->SocketState == SOCKET_CONNECTED)
        return 1;
    if(HTTP_Setup(socket->DNSorIP,socket->Port) == 1)
        return 1;
    return 0;
    
}

#ifdef PROTO_CDAC
// Manage backup server TCP connection (ServerSocket[2]) in HTTP thread
// This runs independently of main HTTP connection (ServerSocket[0])
static void manageBackupServerConnection(void) {
    // Only manage if backup server is enabled and GPRS is active
    if(!ServerSocket[2].isEnabled || GSM.GSMState < GPRS_ACTIVE)
        return;
    
    switch(ServerSocket[2].SocketState) {
        case SOCKET_CLOSED:
            // Open socket
            if(TCPSocket_OPEN(&ServerSocket[2])) {
                nwy_dbg_log("Backup server socket opened");
            }
            break;
            
        case SOCKET_OPEN:
            // Initiate connection
            if(TCPSocket_CONNECT(&ServerSocket[2])) {
                nwy_dbg_log("Backup server connecting...");
            }
            break;
            
        case SOCKET_CONNECTING:
            // Poll connection status (non-blocking)
            TCPSocket_PollConnection(&ServerSocket[2]);
            break;
            
        case SOCKET_CONNECTED:
            // Check if still connected
            TCPSocket_CheckState(&ServerSocket[2]);
            // Check for incoming data
            nwy_tcp_check_func(&ServerSocket[2]);
            break;
            
        default:
            break;
    }
}
#endif

void HTTPThreadEntry(void *param)
{
    TCPSocketTypedef* socket = param;
    uint8_t isSecure;
    nwy_sleep(3000);
    nwy_dbg_log("HTTP Thread Entry");
    while(1)
    {
        isSecure = HTTP_IsSecurePort(socket->Port);
        nwy_sleep(50);
        while(GSM.GSMState < GPRS_ACTIVE)
            nwy_sleep(1500);

#ifdef PROTO_CDAC
        // Manage backup server TCP connection alongside main HTTP connection
        manageBackupServerConnection();
#endif
        
        while(VTSState.CurrentProfile==0)
            nwy_sleep(3000);

        while(FTPState>FTP_STATE_CLOSED || IsFTPReq)
        {
            nwy_dbg_log("HTTP disabled for FTP");
            nwy_sleep(3000);
        }

#ifdef HTTP_SIMULATE
        if(!isSecure)
        {
            if(socket->SocketState == SOCKET_CONNECTED)
            {
                CallBack(socket->Connected);
                nwy_sleep(50);
                continue;
            }
        }
        else
#endif
        {
            if(HTTPState == HTTP_STATE_SET)
            {
                socket->SocketState = SOCKET_CONNECTED;
                CallBack(socket->Connected);
                continue;
            }
        }
        if(HTTPConnectFlag)
        {
#ifdef HTTP_SIMULATE
            if(!isSecure)
            {
                if(socket->SocketState == SOCKET_CLOSED)
                {
                    HTTP_Setup(socket->DNSorIP,socket->Port);
                }
                else if(socket->SocketState == SOCKET_CONNECTING)
                {
                    TCPSocket_PollConnection(socket);
                }
            }
            else
#endif
            {
                if(HTTP_Setup(socket->DNSorIP,socket->Port) == 1)
                {
                    socket->SocketState = SOCKET_CONNECTED;
                    CallBack(socket->OnConnect);
                }
                else
                {
                    socket->SocketState = SOCKET_CLOSED;
                    CallBack(socket->OnDisconnect);
                }
            }
        }
        
    }
}

