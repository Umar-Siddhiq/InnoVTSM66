#ifndef _FTP_H
#define _FTP_H

#include "VTS.h"
#include "GPRS.h"
#include "ql_fota.h"





#define FTP_DEFAULT_CHANNEL         1
#define FTP_CALLBACK_TIMEOUT        5000
#define FTP_DATA_CALLBACK_TIMEOUT   3000
#define FTP_LOGIN_TIMEOUT           180  
#define FTP_CONNECT_ATTEMPTS        3
#define FTP_FILE_SIZE_ATTEMPTS      2
#define FTP_FILESIZE_TIMEOUT        100
#define FTP_MAX_FILE_BUFFER_SIZE    1024

#define FOTA_MAX_BUFF_SIZE          512

#define FTP_FILEGET_TYPE_BINARY    80
#define FTP_FILEGET_TYPE_ASCII      1




#define SERVER_FOTA_FILEPATH        "app.bin"
#define SERVER_MOTA_FILEPATH        "mcu.bin"


typedef enum {FTP_STATE_CLOSED,FTP_STATE_ERROR,FTP_STATE_INIT,FTP_STATE_CONNECTED}FTPStateTypedef;
typedef enum {FTP_TRANSFER_CLOSED,FTP_TRANSFER_ERROR,FTP_TRANSFER_INIT,FTP_TRANSFER_GOTSIZE,FTP_TRANSFER_DATAREQ,FTP_TRANSFER_DATACALLBACK,FTP_TRANSFER_COMPLETED}FTPDownloadTypedef;

typedef enum 
{
    FTP_EVENT_NONE=0x80,
    FTP_EVENT_LOGIN_FAIL,
    FTP_EVENT_LOGIN_SUCESS,
    FTP_EVENT_FILE_SIZE_ERR,
    FTP_EVENT_FILE_SIZE_SUCCESS,
    FTP_EVENT_FILE_DOWNLOAD_ERR,
    FTP_EVENT_FILE_DOWNLOAD_PROGRESS,
    FTP_EVENT_FILE_DOWNLOAD_COMPLETE,
    FTP_EVENT_LOGOUT
}FTPEventTypedef;

typedef struct nwy_file_ftp_info_s
{
  int is_vaild;
  char filename[256];
  int pos;
  //int length;
  int file_size;
}nwy_file_ftp_info_s;


typedef struct _ota_package {
  unsigned int offset;
  unsigned int len;
  unsigned char *data;
} ota_package_t;


#define FOTA_REQ_VALID_CODE 0xA5A3


typedef enum {FTP_REQ_TYPE_FOTA,FTP_REQ_TYPE_CONFIG}FtpReqTypedef;
typedef struct 
{
  uint16_t IsValid;
  uint8_t AttemptCount;
  char IP[50];
  uint16_t Port;
  char User[50];
  char Pass[50];
  char FilePath[50];
  char InternalFilePath[50];
  char Sender[20];
  uint8_t IsServer;
  FtpReqTypedef RequestType;
  uint8_t Status;
}download_req_info_s;



extern download_req_info_s  DownloadReq;
extern nwy_file_ftp_info_s CurrentFile;

extern FTPStateTypedef FTPState;
extern FTPDownloadTypedef FTPDownloadState;

void UpdateFTPConfigInFlash(download_req_info_s* FTPHandle);
void LoadFTPConfig(download_req_info_s* FTPHandle);

void PrintSystemSizes(void);
uint8_t FTP_Login(char* IP, uint16_t Port, uint8_t IsActiveMode, char* User, char* Pass);
uint8_t FTPGetFileSize(char* FileName, int *size);
uint8_t FLS_DeleteExistingFile(char* Filename);
uint8_t FTPDownloadFile(char* FTPFilePath, char* InternalFilePath);
uint8_t FOTAUpdate(char *FileName);
uint8_t FTP_Logout(void);
uint8_t FTPStart(download_req_info_s* ftpHandle);

// One-shot helper (no config persistence). Intended for SMS-triggered features like EPO.
// Downloads `remotePath` from FTP to `localPath` using selected `storage` ("RAM" or "UFS").
uint8_t FTP_DownloadOnce(const char* ip, uint16_t port, const char* user, const char* pass,
                         const char* remotePath, const char* localPath, const char* storage);

uint8_t FTP_CleanupDiskSpace(uint32_t requiredSize);

typedef struct
{
    uint32_t freeSpaceBefore;
    uint32_t freeSpaceAfter;
    uint16_t historyFilesDeleted;
    uint16_t batchFilesDeleted;
    uint16_t transientFilesDeleted;
    uint16_t failures;
} DiskCleanupResult;

// Clears recoverable UFS data only. Device configuration and state are retained.
uint8_t FTP_ClearRecoverableDiskData(DiskCleanupResult *result);

#endif
