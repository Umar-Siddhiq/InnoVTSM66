#include "HTTP.h"

#if defined(PROTO_CDAC)

#include "Server.h"
#include "GPRS.h"
#include "File.h"
#ifdef HTTP_QUEUE
#include "HttpQueue.h"
#endif
#include "ril.h"
#include "ril_http.h"

extern s32 SendATCommandSimple(char *atCmd, char *responseBuf, u32 maxLen, u32 timeout);

volatile HTTPStattypedef HTTPState = HTTP_STATE_NOTSET;
volatile uint8_t IsHTTPRes = 0;
volatile uint8_t HTTPConnectFlag = 0;

static char HTTPPath[128] = "/";
static char HTTPHost[128] = {0};
static char HTTPUrl[220] = {0};
static uint8_t HTTPCurrentSecure = 0;
static uint16_t HTTPResponseLength = 0;

static uint8_t HTTP_IsGprsReady(void)
{
    return (GSM.GSMState == GPRS_ACTIVE) ? 1 : 0;
}

#if 0
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
#endif

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

    // CDAC Force-Plain Transport: regional standard requires plain HTTP transport
    HTTPCurrentSecure = 0;
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
    HTTPState = ready ? HTTP_STATE_SET : HTTP_STATE_NOTSET;
    ServerSocket[0].SocketState = ready ? SOCKET_CONNECTED : SOCKET_CLOSED;
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
    s32 ret;
    uint8_t attempt;

    for(attempt = 0; attempt < 5; attempt++) {
        ret = RIL_HTTP_RequestToPost(data, len);
        if(ret != RIL_AT_BUSY) {
            /* Any result other than BUSY (success, error, timeout) is
             * final — do not retry. AT+QHTTPPOST already blocks for the
             * full modem-side timeout (set in ril_http.c), so retrying
             * a non-BUSY failure would multiply that delay by the retry
             * count (e.g. 3 × 30 s = 90 s) before VLT even starts. */
            return ret;
        }
        ThreadSleep(200);
    }

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
static uint8_t HTTP_SendSSLCommand(char *cmd, char *label, uint8_t required)
{
    char responseBuffer[64];
    s32 ret;

    Ql_memset(responseBuffer, 0, sizeof(responseBuffer));
    ret = SendATCommandSimple(cmd, responseBuffer, sizeof(responseBuffer), 2000);
    if(ret != RIL_ATRSP_SUCCESS) {
        LOGData(TAG_SERVER, "QHTTP: SSL %s failed: %d, resp: %s", label, ret, responseBuffer);
        return required ? 0 : 1;
    }

    LOGData(TAG_SERVER, "QHTTP: SSL %s response: %s", label, responseBuffer);
    return 1;
}

/* SSL context index used for the HTTP(S) stack. The M66/MC60 default
 * sslctxid for HTTP is 1, and the GSM HTTPS/SSL Application Note configures
 * the SSL context and binds httpsctxi using context 1. Keep every QSSLCFG
 * below on the SAME context id so httpsctxi binds the context we configured. */
#define HTTP_SSL_CTX_ID  1

static uint8_t HTTP_ConfigureSSL(void)
{
    uint8_t ok = 1;

    if(HTTPCurrentSecure) {
        LOGData(TAG_SERVER, "QHTTP: Configuring SSL context %d for HTTPS...", HTTP_SSL_CTX_ID);

        /* ------------------------------------------------------------------
         * FIX (HTTPS could not connect): the previous code used the wrong
         * AT+QSSLCFG syntax for the "https" and "httpsctxi" parameters and
         * the wrong ordering, so the module answered ERROR to every bind
         * command and the URL was never armed (see CDAC.txt log).
         *
         * On the M66/MC60 GSM HTTPS stack:
         *   AT+QSSLCFG="https",<enable>        <- SINGLE arg (1=on, 0=off)
         *   AT+QSSLCFG="httpsctxi",<ctxid>     <- SINGLE arg (SSL context id)
         * The old code sent "https",0,0 / "https",0,1 (3 fields) and bound
         * httpsctxi BEFORE enabling https — both rejected with ERROR.
         *
         * Correct order: disable -> configure context -> enable -> bind ctx.
         *
         * OLD CODE:
         *   HTTP_SendSSLCommand("AT+QSSLCFG=\"https\",0,0\r\n", ...);
         *   HTTP_SendSSLCommand("AT+QSSLCFG=\"https\",1,0\r\n", ...);
         *   HTTP_SendSSLCommand("AT+QSSLCFG=\"sslversion\",0,4\r\n", ...);
         *   HTTP_SendSSLCommand("AT+QSSLCFG=\"seclevel\",0,0\r\n", ...);
         *   HTTP_SendSSLCommand("AT+QSSLCFG=\"ignorertctime\",1\r\n", ...);
         *   HTTP_SendSSLCommand("AT+QSSLCFG=\"httpsctxi\",0\r\n", ..., 1);
         *   HTTP_SendSSLCommand("AT+QSSLCFG=\"https\",0,1\r\n", ..., 1);
         * ------------------------------------------------------------------ */

        /* Disable HTTPS first so the SSL context can be (re)configured. */
        HTTP_SendSSLCommand("AT+QSSLCFG=\"https\",0\r\n", "https disable", 0);

        /* Configure the SSL context (context id is the 2nd field here). */
        HTTP_SendSSLCommand("AT+QSSLCFG=\"sslversion\",1,4\r\n", "sslversion", 0);   /* 4 = all (SSL3.0..TLS1.2) */
        HTTP_SendSSLCommand("AT+QSSLCFG=\"seclevel\",1,0\r\n", "seclevel", 0);       /* 0 = no certificate auth */
        HTTP_SendSSLCommand("AT+QSSLCFG=\"ciphersuite\",1,\"0xFFFF\"\r\n", "ciphersuite", 0); /* support all */
        HTTP_SendSSLCommand("AT+QSSLCFG=\"ignorertctime\",1\r\n", "ignorertctime", 0);

        /* Enable HTTPS, THEN bind the configured SSL context to the HTTP stack. */
        if(!HTTP_SendSSLCommand("AT+QSSLCFG=\"https\",1\r\n", "https enable", 1)) {
            ok = 0;
        }
        if(!HTTP_SendSSLCommand("AT+QSSLCFG=\"httpsctxi\",1\r\n", "httpsctxi", 1)) {
            ok = 0;
        }

        if(!ok) {
            LOGData(TAG_SERVER, "QHTTP: HTTPS setup failed; URL will not be armed");
        }
        return ok;
    }

    LOGData(TAG_SERVER, "QHTTP: Disabling HTTPS context...");
    return HTTP_SendSSLCommand("AT+QSSLCFG=\"https\",0\r\n", "https disable", 0);
}

uint8_t HTTP_Setup(char *ip, uint16_t port)
{
    s32 ret;

    HTTP_PrepareEndpoint(ip, port);

    if(!HTTP_IsGprsReady()) {
        LOGData(TAG_SERVER, "QHTTP setup deferred: GPRS not active (state=%d) for %s", GSM.GSMState, HTTPUrl);
        HTTP_SetReadyState(0);
        return 0;
    }

    if(!HTTP_ConfigureSSL()) {
        HTTP_SetReadyState(0);
        return 0;
    }

    if(ServerSocket[0].SocketState == SOCKET_CONNECTED && HTTPState == HTTP_STATE_SET) {
        return 1;
    }

    ServerSocket[0].SocketState = SOCKET_CONNECTING;
    ret = HTTP_SetUrlWithRetry();
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "QHTTP setup failed: %d, url=%s, secure=%d, gprs=%d", ret, HTTPUrl, HTTPCurrentSecure, GSM.GSMState);
        HTTP_SetReadyState(0);
        return 0;
    }

    if(HTTPCurrentSecure) {
        LOGData(TAG_SERVER, "QHTTP secure transport armed for %s", HTTPUrl);
    }

    HTTP_SetReadyState(1);
    return 1;
}

uint8_t HTTPS_Setup(char *ip, int port, void *ssl)
{
    (void)ssl;
    return HTTP_Setup(ip, (uint16_t)port);
}

uint8_t HTTP_Post(uint8_t keepAlive, uint8_t type, char *data, int datalen, uint8_t isSecure)
{
    s32 ret;
    /* 10s per attempt × 5 retries = 50s worst-case server thread block.
     * Original 30s × 5 = 150s blocked the server thread for the entire SOS
     * timeout window (100 ticks × 1s = 100s), preventing EPB10 from being sent. */
    u32 readTimeoutSec = 10;

    (void)type;
    (void)isSecure;

    if(data == NULL || datalen <= 0) {
        LOGData(TAG_SERVER, "CDAC HTTP: Data is NULL or empty!");
        return 0;
    }

    if(!HTTP_IsGprsReady()) {
        LOGData(TAG_SERVER, "CDAC HTTP: POST deferred, GPRS not active (state=%d)", GSM.GSMState);
        HTTP_SetReadyState(0);
        return 0;
    }

    if(HTTPState != HTTP_STATE_SET) {
        LOGData(TAG_SERVER, "CDAC HTTP: Setup required, initializing connection to %s:%d", ServerSocket[0].DNSorIP, ServerSocket[0].Port);
        if(!HTTP_Setup(ServerSocket[0].DNSorIP, (uint16_t)ServerSocket[0].Port)) {
            LOGData(TAG_SERVER, "CDAC HTTP: Setup failed!");
            return 0;
        }
    }

    HTTP_ResetResponseBuffer();

    ret = HTTP_SetUrlWithRetry();
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "CDAC HTTP: URL refresh failed: %d", ret);
        HTTP_SetReadyState(0);
        return 0;
    }

    /* Short settle time between AT+QHTTPURL and AT+QHTTPPOST.
     * The M66 QHTTP state machine needs a brief window to arm the TCP
     * context after the URL is accepted before it can accept a POST. */
    ThreadSleep(150);
    LOGData(TAG_SERVER, "CDAC HTTP: Sending POST request of %d bytes...", datalen);
    ret = HTTP_PostWithRetry(data, (u16)datalen);
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "CDAC HTTP: POST request failed: %d", ret);
        HTTP_SetReadyState(0);
        return 0;
    }

    if(HTTPCurrentSecure) {
        readTimeoutSec = 60;
    }

    LOGData(TAG_SERVER, "CDAC HTTP: Waiting for server response (timeout %ds)...", readTimeoutSec);
    ret = HTTP_ReadWithRetry(readTimeoutSec);
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "CDAC HTTP: READ response failed: %d", ret);
        if(!keepAlive) {
            HTTP_Close(HTTPCurrentSecure);
        }
        return 0;
    }

    LOGData(TAG_SERVER, "CDAC HTTP: Response received successfully! Length: %d bytes", HTTPResponseLength);
    if (HTTPResponseLength > 0 && ServerSocket[0].rxBuffer != NULL) {
        // Log a snippet of the response (useful for status/rejection details)
        char resp_snippet[201];
        Ql_memset(resp_snippet, 0, sizeof(resp_snippet));
        Ql_strncpy(resp_snippet, (const char*)ServerSocket[0].rxBuffer, 200);
        LOGData(TAG_SERVER, "CDAC HTTP Response: %s", resp_snippet);

        // Check for common rejection codes in response
        if (Ql_strstr((const char*)ServerSocket[0].rxBuffer, "400") || 
            Ql_strstr((const char*)ServerSocket[0].rxBuffer, "401") ||
            Ql_strstr((const char*)ServerSocket[0].rxBuffer, "403") ||
            Ql_strstr((const char*)ServerSocket[0].rxBuffer, "404") ||
            Ql_strstr((const char*)ServerSocket[0].rxBuffer, "500")) {
            LOGData(TAG_SERVER, "CDAC HTTP: Warning - response contains error/rejection status code!");
        }
    }

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

    /* Drain any pending QHTTP response that the modem may still be holding.
     * Without this, a stale Server 1 session (especially after a failed POST
     * whose response was never read) leaves the modem's QHTTP in a partial
     * state that causes the next AT+QHTTPPOST to return ERROR immediately.
     * A 1-second timeout is enough: if there is no pending data, the modem
     * responds with ERROR instantly and we move on. */
    {
        char dummyBuf[32] = {0};
        SendATCommandSimple("AT+QHTTPREAD=1\r\n", dummyBuf, sizeof(dummyBuf), 2000);
    }
    ThreadSleep(300);

    HTTPConnectFlag = 0;
    HTTP_ResetResponseBuffer();
    HTTP_SetReadyState(0);
    ServerSocket[0].SocketIndex = -1;
    return 1;
}

void HTTPThreadEntry(s32 taskId)
{
    static uint32_t lastGprsWaitLog = 0;

    http_thread_init(taskId);
    LOGData(TAG_SERVER, "HTTP thread started");
#ifdef HTTP_QUEUE
    HttpQueue_Init();
    LOGData(TAG_SERVER, "HTTP queue merged into HTTP thread");
#endif
    ThreadSleep(3000);

    while(1) {
#ifdef PROTO_CDAC
        if(ServerSocket[2].isEnabled) {
            TCPSocket_Process(&ServerSocket[2]);
        }
#endif
        if(HTTPConnectFlag && HTTPState != HTTP_STATE_SET && !IsSendProcess) {
            if(!HTTP_IsGprsReady()) {
                uint32_t now = Ql_GetMsSincePwrOn();

                HTTP_SetReadyState(0);
                if(now - lastGprsWaitLog > 5000) {
                    LOGData(TAG_SERVER, "HTTP thread waiting for GPRS before arming transport (state=%d)", GSM.GSMState);
                    lastGprsWaitLog = now;
                }
            } else {
                LOGData(TAG_SERVER, "HTTP thread arming transport for %s (port=%d)", ServerSocket[0].DNSorIP, ServerSocket[0].Port);
                HTTP_Setup(ServerSocket[0].DNSorIP, (uint16_t)ServerSocket[0].Port);
            }
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

void HTTP_EnableSecure(uint8_t enable)
{
    VTSData.EnableHTTPS = enable;
    UpdateConfigInFlash();
}

uint8_t HTTP_IsSecureEnabled(void)
{
    return VTSData.EnableHTTPS;
}

#endif