//C:\Users\Admin\Desktop\InnoVTSM66\custom\inc\HTTPS.h
#ifndef _HTTPS_H
#define _HTTPS_H

#include "VTS.h"

#ifdef PROTO_CDAC

#define HTTPS_DEFAULT_CHANNEL 1
#define HTTPS_SOCKET_INDEX 1

typedef enum
{
    HTTPS_EVENT_NONE,
    HTTPS_EVENT_ERROR = 3,
    HTTPS_EVENT_CLOSED,
    HTTPS_EVENT_SETUP,
    HTTPS_EVENT_SEND,
    HTTPS_EVENT_RECEIVE
} HttpsEventTypedef;

typedef enum
{
    HTTPS_STATE_NOTSET,
    HTTPS_STATE_SET
} HTTPSStattypedef;

// SSL/TLS Configuration
typedef enum
{
    HTTPS_SSL_VERSION_ALL = 4,    // Support all versions
    HTTPS_SSL_VERSION_TLS12 = 3   // TLS 1.2
} HttpsSSLVersionTypedef;

typedef enum
{
    HTTPS_SECLEVEL_NONE = 0,           // No authentication
    HTTPS_SECLEVEL_SERVER_AUTH = 1,    // Server authentication only
    HTTPS_SECLEVEL_MUTUAL_AUTH = 2     // Mutual authentication
} HttpsSecLevelTypedef;

extern HTTPSStattypedef HTTPSState;
extern uint8_t IsHTTPSRes;
extern uint8_t HTTPSConnectFlag;

/**
 * Initialize HTTPS module
 * Sets up SSL/TLS context, certificates, and security parameters
 * Must be called once before HTTPS_Setup()
 * 
 * @param ctxIndex SSL context index (typically 0 or 1)
 * @param sslVersion SSL/TLS version (HTTPS_SSL_VERSION_TLS12, etc.)
 * @param secLevel Security level (HTTPS_SECLEVEL_SERVER_AUTH, etc.)
 * @param caCertPath Path to CA certificate file
 * @param clientCertPath Path to client certificate (if mutual auth required)
 * @param clientKeyPath Path to client key (if mutual auth required)
 * @return 1 if successful, 0 if failed
 */
uint8_t HTTPS_Initialize(uint8_t ctxIndex, uint8_t sslVersion, uint8_t secLevel, 
                         const char *caCertPath, const char *clientCertPath, 
                         const char *clientKeyPath);

/**
 * Setup HTTPS connection to server
 * Prepares endpoint, configures SSL, and establishes connection
 * 
 * @param ip Server IP address or hostname
 * @param port Server port (typically 443 for HTTPS)
 * @return 1 if connection setup successful, 0 if failed
 */
uint8_t HTTPS_Setup(char *ip, uint16_t port);

/**
 * Send HTTPS POST request with data
 * Sends encrypted POST request to the configured endpoint
 * 
 * @param keepAlive Keep connection alive (1=yes, 0=no)
 * @param type Request type (reserved for future use)
 * @param data POST data payload
 * @param datalen Length of data
 * @return 1 if POST sent and response received, 0 if failed
 */
uint8_t HTTPS_Post(uint8_t keepAlive, uint8_t type, char *data, int datalen);

/**
 * Check if HTTPS response received
 * Non-blocking check for incoming response
 * 
 * @return 1 if response available, 0 if no response yet
 */
uint8_t HTTPS_WaitResponse(void);

/**
 * Close HTTPS connection
 * Cleans up connection and resets state
 * 
 * @return 1 if successful
 */
uint8_t HTTPS_Close(void);

/**
 * HTTPS thread entry point
 * Main thread that handles HTTPS background operations
 * Called by task scheduler
 * 
 * @param taskId Task ID assigned by scheduler
 */
void HTTPSThreadEntry(s32 taskId);

#endif

#endif
