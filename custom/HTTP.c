//C:\Users\Admin\Desktop\InnoVTSM66\custom\HTTP.c
#include "HTTP.h"

#ifdef PROTO_CDAC

#include "Server.h"
#ifdef HTTP_QUEUE
#include "HttpQueue.h"
#endif
#include "ril.h"
#include "ril_http.h"

HTTPStattypedef HTTPState = HTTP_STATE_NOTSET;
uint8_t IsHTTPRes = 0;
uint8_t HTTPConnectFlag = 0;

static char HTTPPath[128] = "/";
static char HTTPHost[128] = {0};
static char HTTPUrl[220] = {0};
static uint8_t HTTPCurrentSecure = 0;
static uint16_t HTTPResponseLength = 0;

static uint8_t HTTP_IsSecurePort(uint16_t port)
{
    return (port == 443) ? 1 : 0;
}

static uint8_t HTTP_UsesSecureScheme(const char *input)
{
    if(input == NULL) {
        return 0;
    }

    return (Ql_strncmp(input, "https://", 8) == 0) ? 1 : 0;
}

static void HTTP_ExtractHostAndPath(const char *input, char *host, uint16_t hostSize, char *path, uint16_t pathSize)
{
    const char *start = input;
    const char *slash;
    uint16_t hostLen;

    if(hostSize > 0) {
        host[0] = '\0';
    }
    if(pathSize > 0) {
        path[0] = '\0';
    }
    if(input == NULL) {
        return;
    }

    if(Ql_strncmp(start, "http://", 7) == 0) {
        start += 7;
    } else if(Ql_strncmp(start, "https://", 8) == 0) {
        start += 8;
    }

    slash = Ql_strchr(start, '/');
    if(slash == NULL) {
        Ql_strncpy(host, start, hostSize - 1);
        host[hostSize - 1] = '\0';
        Ql_strncpy(path, "/", pathSize - 1);
        path[pathSize - 1] = '\0';
        return;
    }

    hostLen = (uint16_t)(slash - start);
    if(hostLen >= hostSize) {
        hostLen = hostSize - 1;
    }
    Ql_memcpy(host, start, hostLen);
    host[hostLen] = '\0';
    Ql_strncpy(path, slash, pathSize - 1);
    path[pathSize - 1] = '\0';
}

static void HTTP_BuildUrl(uint16_t port)
{
    const char *scheme = HTTPCurrentSecure ? "https" : "http";

    if(HTTPPath[0] == '\0') {
        Ql_strncpy(HTTPPath, "/", sizeof(HTTPPath) - 1);
        HTTPPath[sizeof(HTTPPath) - 1] = '\0';
    }

    if((!HTTPCurrentSecure && port == 80) || (HTTPCurrentSecure && port == 443)) {
        Ql_sprintf(HTTPUrl, "%s://%s%s", scheme, HTTPHost, HTTPPath);
    } else {
        Ql_sprintf(HTTPUrl, "%s://%s:%u%s", scheme, HTTPHost, port, HTTPPath);
    }
}

static void HTTP_PrepareEndpoint(char *input, uint16_t port)
{
    char host[128] = {0};
    char path[128] = {0};

    HTTPCurrentSecure = HTTP_IsSecurePort(port) || HTTP_UsesSecureScheme(input);
    HTTP_ExtractHostAndPath(input, host, sizeof(host), path, sizeof(path));

    if(host[0] == '\0' && input != NULL) {
        Ql_strncpy(host, input, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    }
    if(path[0] == '\0') {
        Ql_strncpy(path, "/", sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }

    Ql_strncpy(HTTPHost, host, sizeof(HTTPHost) - 1);
    HTTPHost[sizeof(HTTPHost) - 1] = '\0';
    Ql_strncpy(HTTPPath, path, sizeof(HTTPPath) - 1);
    HTTPPath[sizeof(HTTPPath) - 1] = '\0';
    HTTP_BuildUrl(port);
    LOGData(TAG_SERVER, "[HTTP_DBG] Endpoint: host=%s, port=%d, path=%s, secure=%d", HTTPHost, port, HTTPPath, HTTPCurrentSecure);
    LOGData(TAG_SERVER, "[HTTP_DBG] URL prepared: %s (len=%d)", HTTPUrl, Ql_strlen(HTTPUrl));
}

static void HTTP_ResetResponseBuffer(void)
{
    HTTPResponseLength = 0;
    IsHTTPRes = 0;
    ServerSocket[0].isRXData = 0;

    if(ServerSocket[0].rxBuffer != NULL) {
        Ql_memset(ServerSocket[0].rxBuffer, 0, ServerSocket[0].rxSizeMAX);
    }
}

static void HTTP_SetReadyState(uint8_t ready)
{
    uint8_t wasConnected = (ServerSocket[0].SocketState == SOCKET_CONNECTED);
    HTTPState = ready ? HTTP_STATE_SET : HTTP_STATE_NOTSET;
    ServerSocket[0].SocketState = ready ? SOCKET_CONNECTED : SOCKET_CLOSED;
    if(ready && !wasConnected && ServerSocket[0].OnConnect)
    {
        ServerSocket[0].OnConnect(0);
    }
}

static s32 HTTP_SetUrlWithRetry(void)
{
    s32 ret = RIL_AT_BUSY;
    uint8_t attempt;

    for(attempt = 0; attempt < 5; attempt++) {
        ret = RIL_HTTP_SetServerURL(HTTPUrl, (u16)Ql_strlen(HTTPUrl));
        if(ret != RIL_AT_BUSY) {
            return ret;
        }
        ThreadSleep(100);
    }

    return ret;
}

static s32 HTTP_PostWithRetry(char *data, u16 len)
{
    s32 ret = RIL_AT_BUSY;
    uint8_t attempt;

    LOGData(TAG_SERVER, "[HTTP_DBG] HTTP_PostWithRetry starting, data_len=%d", len);
    for(attempt = 0; attempt < 5; attempt++) {
        LOGData(TAG_SERVER, "[HTTP_DBG] POST attempt %d/5, sending AT+QHTTPPOST=%d", attempt+1, len);
        ret = RIL_HTTP_RequestToPost(data, len);
        LOGData(TAG_SERVER, "[HTTP_DBG] POST attempt %d result: ret=%d", attempt+1, ret);
        if(ret != RIL_AT_BUSY) {
            if(ret == RIL_AT_SUCCESS) {
                LOGData(TAG_SERVER, "[HTTP_DBG] POST succeeded on attempt %d", attempt+1);
            } else {
                LOGData(TAG_SERVER, "[HTTP_DBG] POST failed with error: %d", ret);
            }
            return ret;
        }
        ThreadSleep(100);
    }

    LOGData(TAG_SERVER, "[HTTP_DBG] POST failed after 5 attempts, final ret=%d", ret);
    return ret;
}

static void HTTP_ReadCallback(u8 *ptrData, u32 dataLen, void *reserved)
{
    u32 copyLen;

    (void)reserved;

    if(ServerSocket[0].rxBuffer == NULL || ptrData == NULL || dataLen == 0) {
        return;
    }
    if(HTTPResponseLength >= (ServerSocket[0].rxSizeMAX - 1)) {
        return;
    }

    copyLen = dataLen;
    if((HTTPResponseLength + copyLen) >= (ServerSocket[0].rxSizeMAX - 1)) {
        copyLen = (u32)((ServerSocket[0].rxSizeMAX - 1) - HTTPResponseLength);
    }

    if(copyLen == 0) {
        return;
    }

    Ql_memcpy(ServerSocket[0].rxBuffer + HTTPResponseLength, ptrData, copyLen);
    HTTPResponseLength = (uint16_t)(HTTPResponseLength + copyLen);
    ServerSocket[0].rxBuffer[HTTPResponseLength] = '\0';
    ServerSocket[0].isRXData = 1;
    IsHTTPRes = 1;
}

static s32 HTTP_ReadWithRetry(u32 timeoutSec)
{
    s32 ret = RIL_AT_BUSY;
    uint8_t attempt;

    for(attempt = 0; attempt < 5; attempt++) {
        ret = RIL_HTTP_ReadResponse(timeoutSec, HTTP_ReadCallback);
        if(ret != RIL_AT_BUSY) {
            return ret;
        }
        ThreadSleep(100);
    }

    return ret;
}

static void http_thread_init(u32 taskId)
{
    OSThread httpThread = {0};

    httpThread.taskId = taskId;
    Ql_strcpy(httpThread.taskName, "HTTP");
    httpThread.taskEnable = 1;
    httpThread.taskState = TASK_STATE_NORMAL;
    httpThread.taskPriority = 1;
    InitializeThread(&httpThread);
}

uint8_t HTTP_Setup(char *ip, uint16_t port)
{
    s32 ret;

    HTTP_PrepareEndpoint(ip, port);
    if(ServerSocket[0].SocketState == SOCKET_CONNECTED && HTTPState == HTTP_STATE_SET) {
        return 1;
    }

    ServerSocket[0].SocketState = SOCKET_CONNECTING;
    ret = HTTP_SetUrlWithRetry();
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "QHTTP setup failed: %d", ret);
        HTTP_SetReadyState(0);
        return 0;
    }

    if(HTTPCurrentSecure) {
        LOGData(TAG_SERVER, "QHTTP secure transport armed for %s", HTTPUrl);
    }

    HTTP_SetReadyState(1);
    return 1;
}

uint8_t HTTP_Post(uint8_t keepAlive, uint8_t type, char *data, int datalen, uint8_t isSecure)
{
    s32 ret;
    u32 readTimeoutSec = 30;

    (void)type;
    (void)isSecure;

    LOGData(TAG_SERVER, "[HTTP_DBG] HTTP_Post called: datalen=%d, keepAlive=%d, URL=%s", datalen, keepAlive, HTTPUrl);
    if(data == NULL || datalen <= 0) {
        LOGData(TAG_SERVER, "[HTTP_DBG] HTTP_Post invalid data!");
        return 0;
    }
    if(HTTPState != HTTP_STATE_SET) {
        LOGData(TAG_SERVER, "[HTTP_DBG] HTTPState not SET, calling HTTP_Setup");
        if(!HTTP_Setup(ServerSocket[0].DNSorIP, (uint16_t)ServerSocket[0].Port)) {
            LOGData(TAG_SERVER, "[HTTP_DBG] HTTP_Setup failed!");
            return 0;
        }
    }

    HTTP_ResetResponseBuffer();
    LOGData(TAG_SERVER, "[HTTP_DBG] Response buffer reset");

    ret = HTTP_SetUrlWithRetry();
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTP_DBG] QHTTP URL refresh failed: ret=%d", ret);
        HTTP_SetReadyState(0);
        return 0;
    }
    LOGData(TAG_SERVER, "[HTTP_DBG] URL set successfully");

    ret = HTTP_PostWithRetry(data, (u16)datalen);
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTP_DBG] QHTTP POST failed: ret=%d (raw value, may be CME error)", ret);
        LOGData(TAG_SERVER, "QHTTP POST failed: %d", ret);
        HTTP_SetReadyState(0);
        return 0;
    }
    LOGData(TAG_SERVER, "[HTTP_DBG] HTTP POST sent successfully");

    if(HTTPCurrentSecure) {
        readTimeoutSec = 60;
    }

    ret = HTTP_ReadWithRetry(readTimeoutSec);
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTP_DBG] QHTTP READ failed: ret=%d", ret);
        LOGData(TAG_SERVER, "QHTTP READ failed: %d", ret);
        if(!keepAlive) {
            HTTP_Close(HTTPCurrentSecure);
        }
        return 0;
    }

    LOGData(TAG_SERVER, "[HTTP_DBG] HTTP_Post successful!");
    IsHTTPRes = 1;
    HTTPState = HTTP_STATE_SET;
    return 1;
}

uint8_t HTTP_WaitResponce(void)
{
    return IsHTTPRes ? 1 : 0;
}

uint8_t HTTP_Close(uint8_t isSecure)
{
    (void)isSecure;

    HTTPConnectFlag = 0;
    HTTP_ResetResponseBuffer();
    HTTP_SetReadyState(0);
    ServerSocket[0].SocketIndex = -1;
    return 1;
}

void HTTPThreadEntry(s32 taskId)
{
    http_thread_init(taskId);
    LOGData(TAG_SERVER, "HTTP thread started");
#ifdef HTTP_QUEUE
    HttpQueue_Init();
    LOGData(TAG_SERVER, "HTTP queue merged into HTTP thread");
#endif
    ThreadSleep(3000);

    while(1) {
        if(ServerSocket[2].isEnabled) {
            TCPSocket_Process(&ServerSocket[2]);
        }

        if(HTTPConnectFlag && HTTPState != HTTP_STATE_SET && !IsSendProcess) {
            LOGData(TAG_SERVER, "HTTP thread arming transport for %s:%d", ServerSocket[0].DNSorIP, ServerSocket[0].Port);
            HTTP_Setup(ServerSocket[0].DNSorIP, (uint16_t)ServerSocket[0].Port);
        }

        if(HTTPState == HTTP_STATE_SET) {
            ServerSocket[0].SocketState = SOCKET_CONNECTED;
        } else if(!HTTPConnectFlag) {
            ServerSocket[0].SocketState = SOCKET_CLOSED;
        }

#ifdef HTTP_QUEUE
        HttpQueue_Process();
#endif

        ThreadSleep(50);
    }
}

#endif