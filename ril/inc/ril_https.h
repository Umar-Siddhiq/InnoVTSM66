#ifndef __RIL_HTTPS_H__
#define __RIL_HTTPS_H__

#include "ql_type.h"
#ifdef __cplusplus
extern "C" {
#endif

// SSL Context configuration (same pattern as HTTP_ACTION enum)
typedef enum {
    SSL_ACTION_IDLE = 0,
    SSL_ACTION_SET_ENABLE,
    SSL_ACTION_SET_CTX_INDEX,
    SSL_ACTION_SET_VERSION,
    SSL_ACTION_SET_CIPHER,
    SSL_ACTION_SET_SECLEVEL,
    SSL_ACTION_SET_IGNORE_RTC,
    SSL_ACTION_WRITE_CERT,
    SSL_ACTION_DELETE_CERT,
    SSL_ACTION_READ_CERT
} Enum_SSL_Action;

// SSL Versions
#define SSL_VERSION_SSL30    0
#define SSL_VERSION_TLS10    1
#define SSL_VERSION_TLS11    2
#define SSL_VERSION_TLS12    3
#define SSL_VERSION_ALL      4

// Security Levels
#define SSL_SECLEVEL_NONE           0
#define SSL_SECLEVEL_SERVER_AUTH    1
#define SSL_SECLEVEL_MUTUAL_AUTH    2

// Cipher Suites
#define CIPHER_SUITE_ALL             "0XFFFF"
#define CIPHER_AES_256_CBC_SHA       "0X0035"
#define CIPHER_AES_128_CBC_SHA       "0X002F"
#define CIPHER_RC4_128_SHA           "0X0005"
#define CIPHER_RC4_128_MD5           "0X0004"
#define CIPHER_3DES_EDE_CBC_SHA      "0X000A"
#define CIPHER_AES_256_CBC_SHA256    "0X003D"

// Function declarations - using correct types for your codebase
s32 RIL_HTTPS_SetEnable(u8 enable);
s32 RIL_HTTPS_SetContextIndex(u8 ctxIndex);
s32 RIL_HTTPS_SetSSLVersion(u8 ctxIndex, u8 version);
s32 RIL_HTTPS_SetCipherSuite(u8 ctxIndex, const char *cipherSuite);
s32 RIL_HTTPS_SetSecurityLevel(u8 ctxIndex, u8 level);
s32 RIL_HTTPS_SetIgnoreRtcTime(u8 ignore);
s32 RIL_HTTPS_SetCACert(u8 ctxIndex, const char *certPath);
s32 RIL_HTTPS_SetClientCert(u8 ctxIndex, const char *certPath);
s32 RIL_HTTPS_SetClientKey(u8 ctxIndex, const char *keyPath);

// Certificate management
s32 RIL_HTTPS_WriteCert(const char *filename, u32 fileSize, u32 timeout);
s32 RIL_HTTPS_ReadCertChecksum(const char *filename);
s32 RIL_HTTPS_DeleteCert(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* __RIL_HTTPS_H__ */