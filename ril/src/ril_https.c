/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. This software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2015
*
*****************************************************************************/
#include "custom_feature_def.h"
#include "ql_type.h"
#include "ql_stdlib.h"
#include "ql_trace.h"
#include "ql_error.h"
#include "ql_common.h"
#include "ql_system.h"
#include "ql_memory.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_https.h"

#ifdef __OCPU_RIL_SUPPORT__

static Enum_SSL_Action m_sslAction = SSL_ACTION_IDLE;

// AT response handler for SSL commands
static s32 ATRsp_SSL_Handler(char* line, u32 len, void* param)
{
    char* pHead = NULL;
    
    // Check for CONNECT (entering data mode for certificate upload)
    pHead = Ql_RIL_FindLine(line, len, "CONNECT");
    if (pHead)
    {
        if (SSL_ACTION_WRITE_CERT == m_sslAction)
        {
            // In data mode - certificate data will be sent by caller
            return RIL_ATRSP_CONTINUE;
        }
        return RIL_ATRSP_CONTINUE;
    }
    
    // Check for OK response
    pHead = Ql_RIL_FindLine(line, len, "OK");
    if (pHead)
    {
        if (param != NULL)
        {
            *((s32*)param) = RIL_AT_SUCCESS;
        }
        return RIL_ATRSP_SUCCESS;
    }
    
    // Check for ERROR response
    pHead = Ql_RIL_FindLine(line, len, "ERROR");
    if (pHead)
    {
        if (param != NULL)
        {
            *((s32*)param) = RIL_AT_FAILED;
        }
        return RIL_ATRSP_FAILED;
    }
    
    // Check for CME ERROR
    pHead = Ql_RIL_FindString(line, len, "+CME ERROR:");
    if (pHead)
    {
        if (param != NULL)
        {
            Ql_sscanf(line, "%*[^: ]: %d[^\r\n]", (s32*)param);
        }
        return RIL_ATRSP_FAILED;
    }
    
    return RIL_ATRSP_CONTINUE;
}

// Enable/Disable HTTPS function
s32 RIL_HTTPS_SetEnable(u8 enable)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[40];
    
    m_sslAction = SSL_ACTION_SET_ENABLE;
    Ql_sprintf(strAT, "AT+QSSLCFG=\"https\",%d\r\n", enable);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

// Set SSL context index for HTTPS
s32 RIL_HTTPS_SetContextIndex(u8 ctxIndex)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[40];
    
    m_sslAction = SSL_ACTION_SET_CTX_INDEX;
    Ql_sprintf(strAT, "AT+QSSLCFG=\"httpsctxi\",%d\r\n", ctxIndex);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

// Configure SSL version
s32 RIL_HTTPS_SetSSLVersion(u8 ctxIndex, u8 version)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[50];
    
    m_sslAction = SSL_ACTION_SET_VERSION;
    Ql_sprintf(strAT, "AT+QSSLCFG=\"sslversion\",%d,%d\r\n", ctxIndex, version);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

// Configure cipher suite
s32 RIL_HTTPS_SetCipherSuite(u8 ctxIndex, const char *cipherSuite)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[70];
    
    if (!cipherSuite)
        return RIL_AT_INVALID_PARAM;
    
    m_sslAction = SSL_ACTION_SET_CIPHER;
    Ql_sprintf(strAT, "AT+QSSLCFG=\"ciphersuite\",%d,%s\r\n", ctxIndex, cipherSuite);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

// Configure security level (authentication mode)
s32 RIL_HTTPS_SetSecurityLevel(u8 ctxIndex, u8 level)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[50];
    
    m_sslAction = SSL_ACTION_SET_SECLEVEL;
    Ql_sprintf(strAT, "AT+QSSLCFG=\"seclevel\",%d,%d\r\n", ctxIndex, level);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

// Configure ignore RTC time
s32 RIL_HTTPS_SetIgnoreRtcTime(u8 ignore)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[50];
    
    m_sslAction = SSL_ACTION_SET_IGNORE_RTC;
    Ql_sprintf(strAT, "AT+QSSLCFG=\"ignorertctime\",%d\r\n", ignore);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

// Set CA certificate path
s32 RIL_HTTPS_SetCACert(u8 ctxIndex, const char *certPath)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[100];
    
    if (!certPath)
        return RIL_AT_INVALID_PARAM;
    
    m_sslAction = SSL_ACTION_IDLE;
    Ql_sprintf(strAT, "AT+QSSLCFG=\"cacert\",%d,\"%s\"\r\n", ctxIndex, certPath);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

// Set client certificate path
s32 RIL_HTTPS_SetClientCert(u8 ctxIndex, const char *certPath)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[100];
    
    if (!certPath)
        return RIL_AT_INVALID_PARAM;
    
    m_sslAction = SSL_ACTION_IDLE;
    Ql_sprintf(strAT, "AT+QSSLCFG=\"clientcert\",%d,\"%s\"\r\n", ctxIndex, certPath);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

// Set client key path
s32 RIL_HTTPS_SetClientKey(u8 ctxIndex, const char *keyPath)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[100];
    
    if (!keyPath)
        return RIL_AT_INVALID_PARAM;
    
    m_sslAction = SSL_ACTION_IDLE;
    Ql_sprintf(strAT, "AT+QSSLCFG=\"clientkey\",%d,\"%s\"\r\n", ctxIndex, keyPath);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

// Write certificate/key to RAM or NVRAM
s32 RIL_HTTPS_WriteCert(const char *filename, u32 fileSize, u32 timeout)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[100];
    
    if (!filename || fileSize == 0)
        return RIL_AT_INVALID_PARAM;
    
    m_sslAction = SSL_ACTION_WRITE_CERT;
    Ql_sprintf(strAT, "AT+QSECWRITE=\"%s\",%d,%d\r\n", filename, fileSize, timeout);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

// Read certificate checksum
s32 RIL_HTTPS_ReadCertChecksum(const char *filename)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[80];
    
    if (!filename)
        return RIL_AT_INVALID_PARAM;
    
    m_sslAction = SSL_ACTION_READ_CERT;
    Ql_sprintf(strAT, "AT+QSECREAD=\"%s\"\r\n", filename);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

// Delete certificate/key
s32 RIL_HTTPS_DeleteCert(const char *filename)
{
    s32 retRes;
    s32 errCode = RIL_AT_FAILED;
    char strAT[80];
    
    if (!filename)
        return RIL_AT_INVALID_PARAM;
    
    m_sslAction = SSL_ACTION_DELETE_CERT;
    Ql_sprintf(strAT, "AT+QSECDEL=\"%s\"\r\n", filename);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATRsp_SSL_Handler, &errCode, 0);
    
    if (retRes != RIL_AT_SUCCESS)
    {
        if (RIL_AT_FAILED == errCode)
            return retRes;
        else
            return errCode;
    }
    return retRes;
}

#endif /* __OCPU_RIL_SUPPORT__ */