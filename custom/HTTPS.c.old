//C:\Users\Admin\Desktop\InnoVTSM66\custom\HTTPS.c
#include "HTTPS.h"

#ifdef PROTO_CDAC

#include "Server.h"
#include "ril.h"
#include "ril_http.h"

// Import RIL HTTPS functions (from RIL layer)
// These are declared in ril.h which includes ril_https.h
extern s32 RIL_HTTPS_SetEnable(u8 enable);
extern s32 RIL_HTTPS_SetContextIndex(u8 ctxIndex);
extern s32 RIL_HTTPS_SetSSLVersion(u8 ctxIndex, u8 version);
extern s32 RIL_HTTPS_SetCipherSuite(u8 ctxIndex, const char *cipherSuite);
extern s32 RIL_HTTPS_SetSecurityLevel(u8 ctxIndex, u8 level);
extern s32 RIL_HTTPS_SetIgnoreRtcTime(u8 ignore);
extern s32 RIL_HTTPS_SetCACert(u8 ctxIndex, const char *certPath);
extern s32 RIL_HTTPS_SetClientCert(u8 ctxIndex, const char *certPath);
extern s32 RIL_HTTPS_SetClientKey(u8 ctxIndex, const char *keyPath);

// State variables
HTTPSStattypedef HTTPSState = HTTPS_STATE_NOTSET;
uint8_t IsHTTPSRes = 0;
uint8_t HTTPSConnectFlag = 0;

// Static configuration
static char HTTPSPath[128] = "/";
static char HTTPSHost[128] = {0};
static char HTTPSUrl[220] = {0};
static uint16_t HTTPSResponseLength = 0;
static uint8_t HTTPSCtxIndex = 0;
static uint8_t HTTPSInitialized = 0;

// ============================================================================
// INTERNAL HELPER FUNCTIONS
// ============================================================================

/**
 * Check if port is standard HTTPS port (443)
 */
static uint8_t HTTPS_IsSecurePort(uint16_t port)
{
    return (port == 443) ? 1 : 0;
}

/**
 * Check if URL uses HTTPS scheme
 */
static uint8_t HTTPS_UsesSecureScheme(const char *input)
{
    if(input == NULL) {
        return 0;
    }
    return (Ql_strncmp(input, "https://", 8) == 0) ? 1 : 0;
}

/**
 * Extract hostname and path from HTTPS URL
 * Supports formats: https://host/path, host/path, https://host:port/path
 */
static void HTTPS_ExtractHostAndPath(const char *input, char *host, uint16_t hostSize, 
                                      char *path, uint16_t pathSize)
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

    // Skip scheme if present
    if(Ql_strncmp(start, "https://", 8) == 0) {
        start += 8;
    } else if(Ql_strncmp(start, "http://", 7) == 0) {
        start += 7;
    }

    // Find path separator
    slash = Ql_strchr(start, '/');
    if(slash == NULL) {
        // No path, just host
        Ql_strncpy(host, start, hostSize - 1);
        host[hostSize - 1] = '\0';
        Ql_strncpy(path, "/", pathSize - 1);
        path[pathSize - 1] = '\0';
        return;
    }

    // Extract host part
    hostLen = (uint16_t)(slash - start);
    if(hostLen >= hostSize) {
        hostLen = hostSize - 1;
    }
    Ql_memcpy(host, start, hostLen);
    host[hostLen] = '\0';

    // Extract path part
    Ql_strncpy(path, slash, pathSize - 1);
    path[pathSize - 1] = '\0';
}

/**
 * Build complete HTTPS URL from components
 */
static void HTTPS_BuildUrl(uint16_t port)
{
    const char *scheme = "https";

    if(HTTPSPath[0] == '\0') {
        Ql_strncpy(HTTPSPath, "/", sizeof(HTTPSPath) - 1);
        HTTPSPath[sizeof(HTTPSPath) - 1] = '\0';
    }

    if(port == 443) {
        Ql_sprintf(HTTPSUrl, "%s://%s%s", scheme, HTTPSHost, HTTPSPath);
    } else {
        Ql_sprintf(HTTPSUrl, "%s://%s:%u%s", scheme, HTTPSHost, port, HTTPSPath);
    }
}

/**
 * Prepare endpoint configuration from input string and port
 */
static void HTTPS_PrepareEndpoint(char *input, uint16_t port)
{
    char host[128] = {0};
    char path[128] = {0};

    HTTPS_ExtractHostAndPath(input, host, sizeof(host), path, sizeof(path));

    if(host[0] == '\0' && input != NULL) {
        Ql_strncpy(host, input, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    }
    if(path[0] == '\0') {
        Ql_strncpy(path, "/", sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }

    Ql_strncpy(HTTPSHost, host, sizeof(HTTPSHost) - 1);
    HTTPSHost[sizeof(HTTPSHost) - 1] = '\0';
    Ql_strncpy(HTTPSPath, path, sizeof(HTTPSPath) - 1);
    HTTPSPath[sizeof(HTTPSPath) - 1] = '\0';
    HTTPS_BuildUrl(port);
    
    LOGData(TAG_SERVER, "[HTTPS_DBG] Endpoint: host=%s, port=%d, path=%s", 
            HTTPSHost, port, HTTPSPath);
    LOGData(TAG_SERVER, "[HTTPS_DBG] URL prepared: %s (len=%d)", 
            HTTPSUrl, Ql_strlen(HTTPSUrl));
}

/**
 * Reset HTTPS response buffer
 */
static void HTTPS_ResetResponseBuffer(void)
{
    HTTPSResponseLength = 0;
    IsHTTPSRes = 0;
    ServerSocket[HTTPS_SOCKET_INDEX].isRXData = 0;

    if(ServerSocket[HTTPS_SOCKET_INDEX].rxBuffer != NULL) {
        Ql_memset(ServerSocket[HTTPS_SOCKET_INDEX].rxBuffer, 0, 
                  ServerSocket[HTTPS_SOCKET_INDEX].rxSizeMAX);
    }
}

/**
 * Set HTTPS ready state and notify socket state change
 */
static void HTTPS_SetReadyState(uint8_t ready)
{
    uint8_t wasConnected = (ServerSocket[HTTPS_SOCKET_INDEX].SocketState == SOCKET_CONNECTED);
    HTTPSState = ready ? HTTPS_STATE_SET : HTTPS_STATE_NOTSET;
    ServerSocket[HTTPS_SOCKET_INDEX].SocketState = ready ? SOCKET_CONNECTED : SOCKET_CLOSED;
    if(ready && !wasConnected && ServerSocket[HTTPS_SOCKET_INDEX].OnConnect) {
        ServerSocket[HTTPS_SOCKET_INDEX].OnConnect(HTTPS_SOCKET_INDEX);
    }
}

/**
 * Set HTTPS URL with retry logic
 * Retries up to 5 times if AT layer is busy
 */
static s32 HTTPS_SetUrlWithRetry(void)
{
    s32 ret = RIL_AT_BUSY;
    uint8_t attempt;

    for(attempt = 0; attempt < 5; attempt++) {
        ret = RIL_HTTP_SetServerURL(HTTPSUrl, (u16)Ql_strlen(HTTPSUrl));
        if(ret != RIL_AT_BUSY) {
            return ret;
        }
        ThreadSleep(100);
    }

    return ret;
}

/**
 * POST data to HTTPS server with retry logic
 * Retries up to 5 times if AT layer is busy
 */
static s32 HTTPS_PostWithRetry(char *data, u16 len)
{
    s32 ret = RIL_AT_BUSY;
    uint8_t attempt;

    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS_PostWithRetry starting, data_len=%d", len);
    
    for(attempt = 0; attempt < 5; attempt++) {
        LOGData(TAG_SERVER, "[HTTPS_DBG] POST attempt %d/5, sending data", attempt+1);
        ret = RIL_HTTP_RequestToPost(data, len);
        LOGData(TAG_SERVER, "[HTTPS_DBG] POST attempt %d result: ret=%d", attempt+1, ret);
        
        if(ret != RIL_AT_BUSY) {
            if(ret == RIL_AT_SUCCESS) {
                LOGData(TAG_SERVER, "[HTTPS_DBG] POST succeeded on attempt %d", attempt+1);
            } else {
                LOGData(TAG_SERVER, "[HTTPS_DBG] POST failed with error: %d", ret);
            }
            return ret;
        }
        ThreadSleep(100);
    }

    LOGData(TAG_SERVER, "[HTTPS_DBG] POST failed after 5 attempts, final ret=%d", ret);
    return ret;
}

/**
 * Read response callback from HTTPS server
 * Appends received data to response buffer
 */
static void HTTPS_ReadCallback(u8 *ptrData, u32 dataLen, void *reserved)
{
    u32 copyLen;

    (void)reserved;

    if(ServerSocket[HTTPS_SOCKET_INDEX].rxBuffer == NULL || ptrData == NULL || dataLen == 0) {
        return;
    }
    
    if(HTTPSResponseLength >= (ServerSocket[HTTPS_SOCKET_INDEX].rxSizeMAX - 1)) {
        return;
    }

    copyLen = dataLen;
    if((HTTPSResponseLength + copyLen) >= (ServerSocket[HTTPS_SOCKET_INDEX].rxSizeMAX - 1)) {
        copyLen = (u32)((ServerSocket[HTTPS_SOCKET_INDEX].rxSizeMAX - 1) - HTTPSResponseLength);
    }

    if(copyLen == 0) {
        return;
    }

    Ql_memcpy(ServerSocket[HTTPS_SOCKET_INDEX].rxBuffer + HTTPSResponseLength, ptrData, copyLen);
    HTTPSResponseLength = (uint16_t)(HTTPSResponseLength + copyLen);
    ServerSocket[HTTPS_SOCKET_INDEX].rxBuffer[HTTPSResponseLength] = '\0';
    ServerSocket[HTTPS_SOCKET_INDEX].isRXData = 1;
    IsHTTPSRes = 1;

    LOGData(TAG_SERVER, "[HTTPS_DBG] Response received: %d bytes, total=%d", 
            dataLen, HTTPSResponseLength);
}

/**
 * Read response from HTTPS server with retry logic
 * Retries up to 5 times if AT layer is busy
 */
static s32 HTTPS_ReadWithRetry(u32 timeoutSec)
{
    s32 ret = RIL_AT_BUSY;
    uint8_t attempt;

    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS_ReadWithRetry starting, timeout=%ld sec", timeoutSec);
    
    for(attempt = 0; attempt < 5; attempt++) {
        ret = RIL_HTTP_ReadResponse(timeoutSec, HTTPS_ReadCallback);
        if(ret != RIL_AT_BUSY) {
            LOGData(TAG_SERVER, "[HTTPS_DBG] Read attempt %d result: ret=%d", attempt+1, ret);
            return ret;
        }
        ThreadSleep(100);
    }

    LOGData(TAG_SERVER, "[HTTPS_DBG] Read failed after 5 attempts, final ret=%d", ret);
    return ret;
}

/**
 * Initialize HTTPS thread
 */
static void https_thread_init(u32 taskId)
{
    OSThread httpsThread = {0};

    httpsThread.taskId = taskId;
    Ql_strcpy(httpsThread.taskName, "HTTPS");
    httpsThread.taskEnable = 1;
    httpsThread.taskState = TASK_STATE_NORMAL;
    httpsThread.taskPriority = 1;
    InitializeThread(&httpsThread);
}

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

/**
 * Initialize HTTPS module with SSL/TLS configuration
 */
uint8_t HTTPS_Initialize(uint8_t ctxIndex, uint8_t sslVersion, uint8_t secLevel, 
                         const char *caCertPath, const char *clientCertPath, 
                         const char *clientKeyPath)
{
    s32 ret;

    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS_Initialize: ctxIndex=%d, sslVersion=%d, secLevel=%d", 
            ctxIndex, sslVersion, secLevel);

    HTTPSCtxIndex = ctxIndex;

    // Enable HTTPS
    ret = RIL_HTTPS_SetEnable(1);
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTPS_ERR] Failed to enable HTTPS: ret=%d", ret);
        return 0;
    }
    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS enabled");

    // Set SSL context index
    ret = RIL_HTTPS_SetContextIndex(ctxIndex);
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTPS_ERR] Failed to set context index: ret=%d", ret);
        return 0;
    }
    LOGData(TAG_SERVER, "[HTTPS_DBG] SSL context index set");

    // Set SSL version (TLS 1.2 or all)
    ret = RIL_HTTPS_SetSSLVersion(ctxIndex, sslVersion);
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTPS_ERR] Failed to set SSL version: ret=%d", ret);
        return 0;
    }
    LOGData(TAG_SERVER, "[HTTPS_DBG] SSL version configured");

    // Set security level (server auth, mutual auth, none)
    ret = RIL_HTTPS_SetSecurityLevel(ctxIndex, secLevel);
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTPS_ERR] Failed to set security level: ret=%d", ret);
        return 0;
    }
    LOGData(TAG_SERVER, "[HTTPS_DBG] Security level configured");

    // Ignore RTC time validation (for devices without accurate time)
    ret = RIL_HTTPS_SetIgnoreRtcTime(1);
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTPS_WARN] Failed to set ignore RTC time: ret=%d", ret);
        // Don't fail on this - continue anyway
    }
    LOGData(TAG_SERVER, "[HTTPS_DBG] RTC time validation configured");

    // Configure CA certificate if provided
    if(caCertPath != NULL && caCertPath[0] != '\0') {
        ret = RIL_HTTPS_SetCACert(ctxIndex, caCertPath);
        if(ret != RIL_AT_SUCCESS) {
            LOGData(TAG_SERVER, "[HTTPS_WARN] Failed to set CA cert: ret=%d", ret);
            // Don't fail - continue with server auth only if no cert
        } else {
            LOGData(TAG_SERVER, "[HTTPS_DBG] CA certificate configured: %s", caCertPath);
        }
    }

    // Configure client certificate if provided (for mutual auth)
    if(clientCertPath != NULL && clientCertPath[0] != '\0') {
        ret = RIL_HTTPS_SetClientCert(ctxIndex, clientCertPath);
        if(ret != RIL_AT_SUCCESS) {
            LOGData(TAG_SERVER, "[HTTPS_WARN] Failed to set client cert: ret=%d", ret);
        } else {
            LOGData(TAG_SERVER, "[HTTPS_DBG] Client certificate configured: %s", clientCertPath);
        }
    }

    // Configure client key if provided (for mutual auth)
    if(clientKeyPath != NULL && clientKeyPath[0] != '\0') {
        ret = RIL_HTTPS_SetClientKey(ctxIndex, clientKeyPath);
        if(ret != RIL_AT_SUCCESS) {
            LOGData(TAG_SERVER, "[HTTPS_WARN] Failed to set client key: ret=%d", ret);
        } else {
            LOGData(TAG_SERVER, "[HTTPS_DBG] Client key configured: %s", clientKeyPath);
        }
    }

    HTTPSInitialized = 1;
    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS initialization complete");
    return 1;
}

/**
 * Setup HTTPS connection to server
 */
uint8_t HTTPS_Setup(char *ip, uint16_t port)
{
    s32 ret;

    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS_Setup: ip=%s, port=%d", ip, port);

    // Check if already connected
    if(ServerSocket[HTTPS_SOCKET_INDEX].SocketState == SOCKET_CONNECTED && 
       HTTPSState == HTTPS_STATE_SET) {
        LOGData(TAG_SERVER, "[HTTPS_DBG] Already connected to %s:%d", ip, port);
        return 1;
    }

    // Prepare endpoint configuration
    HTTPS_PrepareEndpoint(ip, port);
    
    ServerSocket[HTTPS_SOCKET_INDEX].SocketState = SOCKET_CONNECTING;

    // Set URL with retry
    ret = HTTPS_SetUrlWithRetry();
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTPS_ERR] Failed to set URL: %d", ret);
        HTTPS_SetReadyState(0);
        return 0;
    }

    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS URL configured: %s", HTTPSUrl);
    HTTPS_SetReadyState(1);
    return 1;
}

/**
 * Send HTTPS POST request with encrypted data
 */
uint8_t HTTPS_Post(uint8_t keepAlive, uint8_t type, char *data, int datalen)
{
    s32 ret;
    u32 readTimeoutSec = 30;  // Default timeout for HTTPS

    (void)type;  // Reserved for future use

    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS_Post called: datalen=%d, keepAlive=%d, URL=%s", 
            datalen, keepAlive, HTTPSUrl);

    // Validate input
    if(data == NULL || datalen <= 0) {
        LOGData(TAG_SERVER, "[HTTPS_ERR] Invalid POST data!");
        return 0;
    }

    // Ensure HTTPS is initialized
    if(!HTTPSInitialized) {
        LOGData(TAG_SERVER, "[HTTPS_ERR] HTTPS not initialized!");
        return 0;
    }

    // Setup connection if not already done
    if(HTTPSState != HTTPS_STATE_SET) {
        LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPSState not SET, calling HTTPS_Setup");
        if(!HTTPS_Setup(ServerSocket[HTTPS_SOCKET_INDEX].DNSorIP, 
                        (uint16_t)ServerSocket[HTTPS_SOCKET_INDEX].Port)) {
            LOGData(TAG_SERVER, "[HTTPS_ERR] HTTPS_Setup failed!");
            return 0;
        }
    }

    // Reset response buffer
    HTTPS_ResetResponseBuffer();
    LOGData(TAG_SERVER, "[HTTPS_DBG] Response buffer reset");

    // Refresh URL
    ret = HTTPS_SetUrlWithRetry();
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTPS_ERR] Failed to refresh URL: ret=%d", ret);
        HTTPS_SetReadyState(0);
        return 0;
    }
    LOGData(TAG_SERVER, "[HTTPS_DBG] URL refreshed successfully");

    // Send POST request
    ret = HTTPS_PostWithRetry(data, (u16)datalen);
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTPS_ERR] HTTPS POST failed: %d", ret);
        HTTPS_SetReadyState(0);
        return 0;
    }
    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS POST sent successfully");

    // Increase timeout for secure connections
    readTimeoutSec = 60;

    // Read response
    ret = HTTPS_ReadWithRetry(readTimeoutSec);
    if(ret != RIL_AT_SUCCESS) {
        LOGData(TAG_SERVER, "[HTTPS_ERR] HTTPS READ failed: %d", ret);
        if(!keepAlive) {
            HTTPS_Close();
        }
        return 0;
    }

    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS_Post successful!");
    IsHTTPSRes = 1;
    HTTPSState = HTTPS_STATE_SET;
    return 1;
}

/**
 * Check if HTTPS response is available
 */
uint8_t HTTPS_WaitResponse(void)
{
    return IsHTTPSRes ? 1 : 0;
}

/**
 * Close HTTPS connection
 */
uint8_t HTTPS_Close(void)
{
    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS_Close called");

    HTTPSConnectFlag = 0;
    HTTPS_ResetResponseBuffer();
    HTTPS_SetReadyState(0);
    ServerSocket[HTTPS_SOCKET_INDEX].SocketIndex = -1;
    return 1;
}

/**
 * HTTPS thread main entry point
 * Handles background HTTPS operations and connection management
 */
void HTTPSThreadEntry(s32 taskId)
{
    https_thread_init(taskId);
    LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS thread started");

    ThreadSleep(3000);

    while(1) {
        // Process socket if enabled
        if(ServerSocket[HTTPS_SOCKET_INDEX].isEnabled) {
            TCPSocket_Process(&ServerSocket[HTTPS_SOCKET_INDEX]);
        }

        // Handle connection establishment request
        if(HTTPSConnectFlag && HTTPSState != HTTPS_STATE_SET && !IsSendProcess) {
            LOGData(TAG_SERVER, "[HTTPS_DBG] HTTPS thread arming transport for %s:%d", 
                    ServerSocket[HTTPS_SOCKET_INDEX].DNSorIP, 
                    ServerSocket[HTTPS_SOCKET_INDEX].Port);
            HTTPS_Setup(ServerSocket[HTTPS_SOCKET_INDEX].DNSorIP, 
                        (uint16_t)ServerSocket[HTTPS_SOCKET_INDEX].Port);
        }

        // Update socket state based on HTTPS state
        if(HTTPSState == HTTPS_STATE_SET) {
            ServerSocket[HTTPS_SOCKET_INDEX].SocketState = SOCKET_CONNECTED;
        } else if(!HTTPSConnectFlag) {
            ServerSocket[HTTPS_SOCKET_INDEX].SocketState = SOCKET_CLOSED;
        }

        ThreadSleep(50);
    }
}

#endif
