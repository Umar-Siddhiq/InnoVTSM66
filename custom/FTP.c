#include "FTP.h"
#define FTPEN

// Define MIN macro
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#ifdef FTPEN
#include "ril_ftp.h"
#include "VTS.h"
#include "ril.h"
#include "ril_ftp.h"
#include "ql_fs.h"
#include "Systic.h"
#include "File.h"
#include "MOTA.h"
#include "ql_common.h"
#include "PktSave.h"
#include "Batch.h"

extern ST_ExtWatchdogCfg* Ql_WTD_GetWDIPinCfg(void);



//#define FTP_EVENT_BASED
download_req_info_s  DownloadReq= {0};
FTPStateTypedef FTPState= FTP_STATE_CLOSED;
FTPDownloadTypedef FTPDownloadState= FTP_TRANSFER_CLOSED;
uint8_t IsFotaProcessing = 0;


uint8_t FTPFileBuffer[FOTA_MAX_BUFF_SIZE]={0};
u32 FTPFileSize=0;
uint32_t DownloadedFileSize=0;

#ifdef FTP_EVENT_BASED
nwy_osi_thread_t CurrentThread=NULL;
#else
uint16_t FTPTimeout=0;

#endif


nwy_file_ftp_info_s CurrentFile={0};
uint32_t DataGetTimeout=0;

static volatile s32 g_ftpLastErrCode = 0;

static void FTP_BuildLocalDownloadedPath(const char* storage, const char* remoteName, char* outPath, u16 outPathLen)
{
    if (!outPath || outPathLen == 0) {
        return;
    }
    outPath[0] = 0;

    if (!remoteName || remoteName[0] == 0) {
        return;
    }

    // RIL_FTP sample convention:
    // - storage == "UFS"  -> local file is "<remoteName>"
    // - storage == "RAM"  -> local file is "RAM:<remoteName>"
    // - other storages     -> local file is "<storage>:<remoteName>"
    if (!storage || Ql_strncmp(storage, "UFS", 3) == 0) {
        Ql_strncpy(outPath, remoteName, outPathLen - 1);
        outPath[outPathLen - 1] = 0;
        return;
    }

    Ql_memset(outPath, 0, outPathLen);
    Ql_sprintf(outPath, "%s:%s", storage, remoteName);
    outPath[outPathLen - 1] = 0;
}

static void FTP_SplitRemotePath(const char* remotePath, char* outDir, u16 outDirLen, char* outName, u16 outNameLen)
{
    if (!remotePath || !outDir || !outName || outDirLen == 0 || outNameLen == 0)
    {
        return;
    }

    outDir[0] = 0;
    outName[0] = 0;

    // Remote path may be:
    //  - "EPO.DAT" -> dir="/", name="EPO.DAT"
    //  - "/epo/EPO.DAT" -> dir="/epo", name="EPO.DAT"
    //  - "epo/EPO.DAT" -> dir="epo", name="EPO.DAT"
    const char* lastSlash = strrchr(remotePath, '/');
    if (!lastSlash)
    {
        Ql_strncpy(outDir, "/", outDirLen - 1);
        outDir[outDirLen - 1] = 0;
        Ql_strncpy(outName, remotePath, outNameLen - 1);
        outName[outNameLen - 1] = 0;
        return;
    }

    // Copy dir part
    u32 dirLen = (u32)(lastSlash - remotePath);
    if (dirLen == 0)
    {
        Ql_strncpy(outDir, "/", outDirLen - 1);
        outDir[outDirLen - 1] = 0;
    }
    else
    {
        u32 copyLen = dirLen;
        if (copyLen > (u32)(outDirLen - 1)) copyLen = (u32)(outDirLen - 1);
        Ql_memcpy(outDir, remotePath, copyLen);
        outDir[copyLen] = 0;
    }

    // Copy filename part
    const char* name = lastSlash + 1;
    Ql_strncpy(outName, name, outNameLen - 1);
    outName[outNameLen - 1] = 0;
}

void UpdateFTPConfigInFlash(download_req_info_s* FTPHandle)
{
    SaveToFlash(FOTA_CONFIG_PATH, (void*)FTPHandle, sizeof(download_req_info_s));
}

void LoadFTPConfig(download_req_info_s* FTPHandle)
{
    extern uint8_t IsFTPReq;
    if (LoadFromFlash(FOTA_CONFIG_PATH, (void*)FTPHandle, sizeof(download_req_info_s), NULL))
    {
        if (FTPHandle->IsValid == FOTA_REQ_VALID_CODE)
        {
            LOGData(TAG_FTP, "FOTA REQUEST FOUND, Attempting Upon Data Connection...");
            IsFTPReq = 1;
        }
        else
        {
            LOGData(TAG_FTP, "No Fota Req!");
        }
    }
}




void PrintSystemSizes(void)
{
    unsigned long usersize;
    Ql_FS_Delete(SERVER_FOTA_FILEPATH);
    usersize = Ql_FS_GetFreeSpace(Ql_FS_UFS);
    LOGData(TAG_FTP,"File Rem Size : %lu",usersize);

}

static void FTP_Callback_OnUpDown(s32 result, s32 size)
{
    if (result)
    {
        LOGData(TAG_FTP,"<-- Succeed in uploading/downloading image bin via FTP, file size:%d -->\r\n", size);
        FTPDownloadState = FTP_TRANSFER_COMPLETED;
        DownloadedFileSize = size;
        g_ftpLastErrCode = 0;
    }else{
        // When result==0, 'size' carries the negative error code from +QFTPGET/+QFTPPUT.
        LOGData(TAG_FTP,"<-- Failed to upload/download file to FTP server, err=%d -->\r\n", size);
        FTPDownloadState = FTP_TRANSFER_ERROR;
        g_ftpLastErrCode = size;
    }
   
}

uint8_t FTP_Login(char* IP, uint16_t Port, uint8_t IsActiveMode, char* User, char* Pass)
{
    u32 ret;
    u8  attempts = 0;
    
    FTPDownloadState=FTP_TRANSFER_CLOSED;
    FTPState = FTP_STATE_CLOSED;
   
    
    LOGData(TAG_FTP,"FTP Login attempting %d/%d Login @ %s, %d, %s, %s",attempts+1,FTP_CONNECT_ATTEMPTS,IP,Port,User,Pass);
    FTPState = FTP_STATE_INIT;
    do
    {
        ret = RIL_FTP_QFTPOPEN((u8*)IP, Port, (u8*)User, (u8*)Pass, 1);
        LOGData(TAG_FTP,"<-- FTP open connection, ret=%d -->\r\n", ret);
        if (RIL_AT_SUCCESS == ret)
        {
            attempts = 0;
            LOGData(TAG_FTP,"<-- Open ftp connection -->\r\n");
            break;
        }
        attempts++;
        LOGData(TAG_FTP,"<-- Retry to open FTP 2s later -->\r\n");
        ThreadSleep(2000);
    } while (attempts < FTP_CONNECT_ATTEMPTS);
    if (FTP_CONNECT_ATTEMPTS == attempts)
    {
        LOGData(TAG_FTP,"<-- Fail to open ftp connection for %d times -->\r\n",FTP_CONNECT_ATTEMPTS);

        // Do not hard-reset here. Let the caller decide recovery strategy.
        return 0;
    }
  
    FTPState=FTP_STATE_CONNECTED;

    return 1;
}

static uint8_t FTP_Login_NoReset(const char* IP, uint16_t Port, const char* User, const char* Pass)
{
    return FTP_Login((char*)IP, Port, 0, (char*)User, (char*)Pass);
}

uint8_t FTPGetFileSize(char* FileName, int *size)
{
    s32 ret;
    int attempts = 0;
    FTPDownloadState = FTP_TRANSFER_INIT;
    do
    {
        ret =  RIL_FTP_QFTPSIZE((u8*)FileName,&FTPFileSize);
        LOGData(TAG_FTP,"<-- FTP get size, ret=%d -->\r\n", ret);
        if (RIL_AT_SUCCESS == ret)
        {
            attempts = 0;
            LOGData(TAG_FTP,"<-- Open ftp connection -->\r\n");
            break;
        }
        attempts++;
        LOGData(TAG_FTP,"<-- Retry to FTP get size 2s later -->\r\n");
        ThreadSleep(2000);
    } while (attempts < FTP_FILE_SIZE_ATTEMPTS);
    if (FTP_FILE_SIZE_ATTEMPTS == attempts)
    {
        LOGData(TAG_FTP,"Unable to get file %s, maybe not exist...",FileName);
        return 0;
    }
    
    FTPDownloadState=FTP_TRANSFER_GOTSIZE;
    *size = FTPFileSize;

    return 1;
}

uint8_t FLS_DeleteExistingFile(char* Filename)
{
    if(Ql_FS_Check(Filename) != QL_RET_OK)
    {
        LOGData(TAG_FTP,"File Remove %s, already doesn't exist!",Filename);
        return 1;
    }
    if(Ql_FS_Delete(Filename)!= QL_RET_OK)
    {
        LOGData(TAG_FTP,"File Remove %s, Couldn't delete file!",Filename);
        return 0;
    }
    return 1;
}

uint8_t FTP_CleanupDiskSpace(uint32_t requiredSize)
{
    uint32_t freeSpace = Ql_FS_GetFreeSpace(Ql_FS_UFS);
    LOGData(TAG_FTP, "UFS Free Space before cleanup: %lu bytes. Required: %lu bytes", freeSpace, requiredSize);
    
    // Clear space if requiredSize is 0 (forced clean) or free space is less than required size + 50KB safety margin
    if (requiredSize == 0 || freeSpace < (requiredSize + 51200))
    {
        LOGData(TAG_FTP, "Initiating selective cleanup...");
        
        // 1. Delete large logs
        Ql_FS_Delete("FOTA.txt");
        Ql_FS_Delete("EVENT.txt");
        
        // 2. Delete old firmware/temp downloads
        Ql_FS_Delete("app.bin");
        Ql_FS_Delete("mcu.bin");
        Ql_FS_Delete("app_fota.bin");
        
        freeSpace = Ql_FS_GetFreeSpace(Ql_FS_UFS);
        
#ifndef HISTORY_DISABLED
        // 3. Purge history packets if space is still insufficient for download
        if (requiredSize > 0 && freeSpace < (requiredSize + 51200))
        {
            #if defined(PROTO_CDAC)
            LOGData(TAG_FTP, "UFS space still low (%lu bytes), clearing CDAC batch storage...", freeSpace);
            ClearFileTable();
            #else
            uint32_t neededSpace = (requiredSize + 51200) - freeSpace;
            // Each history packet is 512 bytes on disk
            uint16_t packetsToDelete = (neededSpace + 511) / 512;
            LOGData(TAG_FTP, "UFS space still low (%lu bytes), purging oldest %u history packets...", freeSpace, packetsToDelete);
            DeleteFirstPacketsBulk(packetsToDelete);
            #endif
            freeSpace = Ql_FS_GetFreeSpace(Ql_FS_UFS);
        }
#endif
        
        LOGData(TAG_FTP, "UFS Free Space after cleanup: %lu bytes", freeSpace);
    }

    if (requiredSize > 0 && freeSpace < (requiredSize + 51200))
    {
        LOGData(TAG_FTP, "UFS still insufficient after cleanup: %lu bytes available, %lu needed",
                freeSpace, requiredSize + 51200);
        return 0;
    }
    return 1;
}

static void FTP_DeleteCleanupFile(const char *filename, DiskCleanupResult *result)
{
    if (Ql_FS_Check((char *)filename) != QL_RET_OK)
        return;

    if (Ql_FS_Delete((char *)filename) == QL_RET_OK)
    {
        result->transientFilesDeleted++;
        LOGData(TAG_FTP, "Deleted cleanup file %s", filename);
    }
    else
    {
        result->failures++;
        LOGData(TAG_FTP, "Unable to delete cleanup file %s", filename);
    }
}

uint8_t FTP_ClearRecoverableDiskData(DiskCleanupResult *result)
{
#ifndef HISTORY_DISABLED
    uint16_t failed = 0;
#endif
    static const char *const transientFiles[] = {
        "app.bin", "mcu.bin", "app_fota.bin", "FOTA.txt", "EVENT.txt"
    };
    uint8_t i;

    if (result == NULL)
        return 0;

    Ql_memset(result, 0, sizeof(*result));
    result->freeSpaceBefore = Ql_FS_GetFreeSpace(Ql_FS_UFS);

    // Never remove files while they may be read or written by an OTA transfer.
    if (IsFotaProcessing || IsMotaProcessing)
    {
        result->failures = 1;
        result->freeSpaceAfter = result->freeSpaceBefore;
        LOGData(TAG_FTP, "CLR DISK rejected while FOTA/MOTA is active");
        return 0;
    }

#ifndef HISTORY_DISABLED
    if (!ClearHistoryStorage(&result->historyFilesDeleted, &failed))
        result->failures += failed;

    failed = 0;
    if (!ClearBatchStorage(&result->batchFilesDeleted, &failed))
        result->failures += failed;
#endif

    for (i = 0; i < (sizeof(transientFiles) / sizeof(transientFiles[0])); i++)
        FTP_DeleteCleanupFile(transientFiles[i], result);

    result->freeSpaceAfter = Ql_FS_GetFreeSpace(Ql_FS_UFS);
    LOGData(TAG_FTP, "CLR DISK complete: free %lu -> %lu, history=%u, batch=%u, files=%u, failures=%u",
            result->freeSpaceBefore, result->freeSpaceAfter, result->historyFilesDeleted,
            result->batchFilesDeleted, result->transientFilesDeleted, result->failures);
    return result->failures == 0;
}

static uint8_t FTPDownloadFileWithStorage(char* FTPFilePath, char* InternalFilePath, const char* storage)
{
    int ret;
    int FileSize=0;

    char remoteDir[128] = {0};
    char remoteName[128] = {0};
    FTP_SplitRemotePath(FTPFilePath, remoteDir, sizeof(remoteDir), remoteName, sizeof(remoteName));
    if (remoteName[0] == 0)
    {
        LOGData(TAG_FTP, "Invalid remote filename: %s", FTPFilePath);
        return 0;
    }
    
    LOGData(TAG_FTP,"FTP Download Command Entry!");
    ThreadSleep(100);
    PrintSystemSizes();
    if(FTPState != FTP_STATE_CONNECTED)
    {
        LOGData(TAG_FTP,"Cant Download without FTP Logged In!");
        return 0;
    }
    // Set local storage
    if (storage == NULL) {
        storage = "UFS";
    }

    char downloadedPath[300] = {0};
    FTP_BuildLocalDownloadedPath(storage, remoteName, downloadedPath, sizeof(downloadedPath));

    // Ensure we don't keep stale files around. QFTPGET may fail or refuse to overwrite.
    if (downloadedPath[0]) {
        Ql_FS_Delete(downloadedPath);
    }
    if (InternalFilePath && InternalFilePath[0] && (downloadedPath[0] == 0 || Ql_strcmp(downloadedPath, InternalFilePath) != 0)) {
        Ql_FS_Delete(InternalFilePath);
    }

    ret = RIL_FTP_QFTPCFG(4, (u8*)storage);
    LOGData(TAG_FTP,"<-- Set local storage, ret=%d -->\r\n", ret);

    // Set remote path (directory)
    ret = RIL_FTP_QFTPPATH((u8*)remoteDir);
    LOGData(TAG_FTP,"<-- Set remote path, ret=%d -->\r\n", ret);


    if(!FTPGetFileSize(remoteName,&FileSize))
    {
        LOGData(TAG_FTP,"Couldn't get Filesize for %s",remoteName);
        return 0;
    }
    LOGData(TAG_FTP,"Got File Size: %d",FileSize);
    if(!FTP_CleanupDiskSpace(FileSize))
    {
        LOGData(TAG_FTP, "Aborting download: UFS disk full, cannot store %d bytes", FileSize);
        return 0;
    }
    // if(!FLS_DeleteExistingFile(InternalFilePath))
    // {
    //     LOGData(TAG_FTP,"Cant Delete Existing File!");
    //     return 0;
    // }
    CurrentFile.is_vaild=1;
    CurrentFile.file_size=FileSize;
    strcpy(CurrentFile.filename,InternalFilePath);
    CurrentFile.pos=0;
    FTPDownloadState=FTP_TRANSFER_DATAREQ;
    // Download file from ftp-server
    g_ftpLastErrCode = 0;
    ret = RIL_FTP_QFTPGET((u8 *)remoteName, FileSize, FTP_Callback_OnUpDown);
    if (ret < 0)
    {
        LOGData(TAG_FTP,"<-- Failed to download, cause=%d -->\r\n", ret);
        
        ret = RIL_FTP_QFTPCLOSE();
        LOGData(TAG_FTP,"<-- FTP close connection, ret=%d -->\r\n", ret);

    }
    
   
    FTPTimeout=300;
    while(FTPDownloadState!=FTP_TRANSFER_COMPLETED)
    {
        

        if(FTPDownloadState == FTP_TRANSFER_ERROR)
        {
            LOGData(TAG_FTP,"FTP Download Interrupted! err=%d", g_ftpLastErrCode);
            return 0;
        }
        ThreadSleep(1000);
        if(--FTPTimeout <= 0)
        {
            LOGData(TAG_FTP,"FTP Download Timeout!");
            FTPDownloadState = FTP_TRANSFER_ERROR;

            ret = RIL_FTP_QFTPCLOSE();
            LOGData(TAG_FTP,"<-- FTP close connection, ret=%d -->\r\n", ret);

            return 0;
        }
        LOGData(TAG_FTP,"FTP Downloading, Timeout: %d",FTPTimeout);
    }
    LOGData(TAG_FTP,"FTP Downloaded, Checking File Size...");
    ThreadSleep(200);
    if(DownloadedFileSize != FileSize)
    {
        LOGData(TAG_FTP,"FILE Final Size Check fail!, exp : %d, got: %d",FileSize,DownloadedFileSize);
        //SendRS232String("FOTA Download Failed due to Network!");
        Ql_FS_Delete(InternalFilePath);
        return 0;
    }

    FTP_Logout();
    ThreadSleep(500);

    // Verify the file where the modem saved it.
    if (downloadedPath[0] == 0) {
        LOGData(TAG_FTP,"FTP internal error: downloadedPath empty");
        return 0;
    }
    if (Ql_FS_Check(downloadedPath) != QL_RET_OK) {
        LOGData(TAG_FTP,"FTP Download Success but local file not found: %s", downloadedPath);
        return 0;
    }

    // If caller wants a different final name, rename within the same storage.
    if (InternalFilePath && InternalFilePath[0] && Ql_strcmp(downloadedPath, InternalFilePath) != 0)
    {
        LOGData(TAG_FTP,"FTP Download Success, renaming file %s -> %s", downloadedPath, InternalFilePath);
        // Destination might already exist; attempt delete then rename with retries.
        Ql_FS_Delete(InternalFilePath);

        u8 renameAttempts = 0;
        do
        {
            ret = Ql_FS_Rename(downloadedPath, InternalFilePath);
            if (ret == QL_RET_OK) {
                break;
            }
            renameAttempts++;
            ThreadSleep(300);
        } while (renameAttempts < 3);

        if (ret != QL_RET_OK)
        {
            LOGData(TAG_FTP,"FTP Rename Failed, ret=%d (src=%s dst=%s)", ret, downloadedPath, InternalFilePath);
            return 0;
        }

        Ql_strncpy(downloadedPath, InternalFilePath, sizeof(downloadedPath) - 1);
        downloadedPath[sizeof(downloadedPath) - 1] = 0;
    }
    else
    {
        LOGData(TAG_FTP,"FTP Download Success, local file: %s", downloadedPath);
    }

    ThreadSleep(200);
    u32 verifySize = Ql_FS_GetSize(downloadedPath);
    if(verifySize != FileSize)
    {
        LOGData(TAG_FTP,"File verification FAILED! Expected: %d, Got: %d", FileSize, verifySize);
        return 0;
    }
    LOGData(TAG_FTP,"File verification PASSED: %d bytes", verifySize);

    LOGData(TAG_FTP,"User storage Size Left : %lu",Ql_FS_GetFreeSpace(Ql_FS_UFS));
    ThreadSleep(100);
    
    return 1;

}

uint8_t FTPDownloadFile(char* FTPFilePath, char* InternalFilePath)
{
#ifdef FTP_FILE_RAM
    return FTPDownloadFileWithStorage(FTPFilePath, InternalFilePath, "RAM");
#else
    return FTPDownloadFileWithStorage(FTPFilePath, InternalFilePath, "UFS");
#endif
}

uint8_t FTP_DownloadOnce(const char* ip, uint16_t port, const char* user, const char* pass,
                         const char* remotePath, const char* localPath, const char* storage)
{
    if (!ip || !user || !pass || !remotePath || !localPath || !storage) {
        return 0;
    }

    if(!FTP_Login_NoReset(ip, port, user, pass)) {
        return 0;
    }
    if(!FTPDownloadFileWithStorage((char*)remotePath, (char*)localPath, storage)) {
        FTP_Logout();
        return 0;
    }
    FTP_Logout();
    return 1;
}

uint8_t FOTAUpdate(char *firmwareFileName)
{
    u32 fileHandle = 0, operationResult = 0;
    u32 firmwareFileSize = 0;
    u32 bytesRead = 0;
    u32 totalBytesWritten = 0;
    u8 *chunkBuffer = NULL;
    const u32 CHUNK_SIZE = 512;  // Match reference implementation (was 256)

    LOGData(TAG_FTP, "FOTA Update Started, Filename: %s\r\n", firmwareFileName);

    ST_FotaConfig fotaCfg = {0};
    fotaCfg.Q_gpio_pin1 = Ql_WTD_GetWDIPinCfg()->pinWtd1;
    fotaCfg.Q_feed_interval1 = 100;
    if (Ql_WTD_GetWDIPinCfg()->pinWtd2 != PINNAME_END)
    {
        fotaCfg.Q_gpio_pin2 = Ql_WTD_GetWDIPinCfg()->pinWtd2;
        fotaCfg.Q_feed_interval2 = 100;
    }
    else
    {
        fotaCfg.Q_gpio_pin2 = -1;
        fotaCfg.Q_feed_interval2 = 0;
    }
    
    if (Ql_FOTA_Init(&fotaCfg) != QL_RET_OK) {
        LOGData(TAG_FTP,"FOTA init failed");
        return 0;  // Return error on init failure
    }
    else {
        LOGData(TAG_FTP,"FOTA init success");
    }

    // Check if file exists
    if (Ql_FS_Check(firmwareFileName) != QL_RET_OK) {
        LOGData(TAG_FTP, "FOTA Error: File %s not found", firmwareFileName);
        return 0;
    }

    // Get firmware file size
    if((firmwareFileSize = Ql_FS_GetSize(firmwareFileName)) <= 0)
    {
        LOGData(TAG_FTP, "FOTA Error: File %s not found or empty", firmwareFileName);
        return 0;
    }

    // Open firmware file
    fileHandle = Ql_FS_Open(firmwareFileName, QL_FS_READ_ONLY);
    
    if(fileHandle < 0)
    {
        LOGData(TAG_FTP, "FOTA Error: Failed to open firmware file, ret=%d", fileHandle);
        return 0;
    }

    // Allocate buffer for chunks
    if((chunkBuffer = Ql_MEM_Alloc(CHUNK_SIZE)) == NULL)
    {
        LOGData(TAG_FTP, "FOTA Error: Memory allocation failed");
        Ql_FS_Close(fileHandle);
        return 0;
    }

    LOGData(TAG_FTP, "Firmware size: %d bytes, using %d-byte chunks", firmwareFileSize, CHUNK_SIZE);
    LOGData(TAG_FTP, "Processing data before update");
    
    // Process firmware in chunks
    while(totalBytesWritten < firmwareFileSize)
    {
        u32 bytesToRead = MIN(CHUNK_SIZE, firmwareFileSize - totalBytesWritten);
        
        // Read chunk from file
        if((operationResult = Ql_FS_Read(fileHandle, chunkBuffer, bytesToRead, &bytesRead)) != QL_RET_OK)
        {
            LOGData(TAG_FTP, "FOTA Error: File read failed at offset %d, error: %d", 
                   totalBytesWritten, operationResult);
            break;
        }

        // Write chunk to FOTA (with retry logic)
        int retryCount = 0;
        const int MAX_RETRIES = 3;
        
        while(retryCount < MAX_RETRIES)
        {
            operationResult = Ql_FOTA_WriteData(bytesRead, (s8*)chunkBuffer);
            if(operationResult == QL_RET_OK) break;
            
            retryCount++;
            LOGData(TAG_FTP, "FOTA Write retry %d/%d failed: %d", 
                   retryCount, MAX_RETRIES, operationResult);
            Ql_Sleep(100);
        }

        if(operationResult != QL_RET_OK)
        {
            LOGData(TAG_FTP, "FOTA Error: Write failed at offset %d, error: %d", 
                   totalBytesWritten, operationResult);
            break;
        }

        totalBytesWritten += bytesRead;
        if((totalBytesWritten % (CHUNK_SIZE * 10)) == 0)  // Log every 10 chunks
        {
            LOGData(TAG_FTP, "Progress: %d/%d bytes (%d%%)", 
                   totalBytesWritten, firmwareFileSize, 
                   (totalBytesWritten * 100) / firmwareFileSize);
        }
    }
    
    // Clean up
    Ql_MEM_Free(chunkBuffer);
    Ql_FS_Close(fileHandle);

    if(totalBytesWritten != firmwareFileSize)
    {
        LOGData(TAG_FTP, "FOTA FAILED: Only wrote %d/%d bytes", totalBytesWritten, firmwareFileSize);
        return 0;
    }

    LOGData(TAG_FTP, "Firmware transfer completed");
    LOGData(TAG_FTP, "Finish processing data");
    
    // Finalize FOTA process
    Ql_Sleep(300);  // Match reference timing (was 2000)
    if((operationResult = Ql_FOTA_Finish()) != QL_RET_OK)
    {
        LOGData(TAG_FTP, "FOTA Error: Finalization failed, error: %d", operationResult);
        return 0;
    }
    
    // Close all TCP connections before upgrade
    TCP_CloseALLSockets();

    // Delete the firmware file after successful processing
    LOGData(TAG_FTP, "Delete firmware file");
    Ql_FS_Delete(firmwareFileName);

    // Trigger update
    LOGData(TAG_FTP, "Start to Update! System will reboot automatically for upgrade");
    if((operationResult = Ql_FOTA_Update()) != QL_RET_OK)  // Changed from < to !=
    {
        LOGData(TAG_FTP, "FOTA Error: Update failed, error: %d", operationResult);
        LOGData(TAG_FTP, "Reboot 1 second later...");
        Ql_Sleep(1000);
        // Don't force reset here - let the system handle it
        return 0;
    }

    LOGData(TAG_FTP, "FOTA SUCCESSFUL! Module will reboot automatically");
    // Don't call Ql_Reset(0) - the module will reboot automatically after Ql_FOTA_Update()
    return 1;
}

uint8_t FTP_Logout(void)
{
    LOGData(TAG_FTP,"FTP Logout Attempt...");
    
    // Check if already disconnected
    if(FTPState == FTP_STATE_CLOSED)
    {
        LOGData(TAG_FTP,"FTP already disconnected, skipping logout");
        return 1;
    }
    
    if(RIL_FTP_QFTPCLOSE() != RIL_AT_SUCCESS)
    {
        LOGData(TAG_FTP,"FTP Logout Failed!");
        FTPState = FTP_STATE_CLOSED;  // Mark as closed anyway
        return 0;
    }
    
    LOGData(TAG_FTP,"FTP Logout Success!");
    FTPState = FTP_STATE_CLOSED;
    ThreadSleep(2000);
    return 1;
}


void DismissFTPReq(download_req_info_s* hdl)
{
    hdl->IsValid=0;
    hdl->Status=2;
    UpdateFTPConfigInFlash(hdl);
}

void PreFTPRoutine(download_req_info_s* hdl)
{
    FTPState = FTP_STATE_INIT;
    // nwy_suspend_thread(nwy_tcp_thread1);
    // nwy_suspend_thread(nwy_tcp_thread2);
    // nwy_suspend_thread(nwy_mcu_thread);
    ThreadSleep(2000);
    hdl->AttemptCount--;
    if(hdl->AttemptCount == 0)
        DismissFTPReq(hdl);
    UpdateFTPConfigInFlash(hdl);
    //nwy_suspend_thread(tcp_recv_thread);
    //LOGData(TAG_FTP,"Closing Socket 2...");
    //TCPDisconnect(&ServerSocket[1]);
    //ThreadSleep(3000);
   
}

void SendResponceFTP(download_req_info_s* hdl,char *msg)
{
    SendResponce(hdl->Sender,msg,hdl->IsServer,0);
    //SendRS232String(msg);
}



uint8_t SendFTPAttemptMsg(download_req_info_s* hdl)
{
    char msg[50]={0};
    if(hdl->AttemptCount==0)
    {
        Ql_sprintf(msg,"FTP Failed! Aborting...");
        SendResponceFTP(hdl,msg);
        return 0;
    }
    else 
    {   
        Ql_sprintf(msg,"FTP Failed. Attempting Again... %d / 3",4-(hdl->AttemptCount));
        SendResponceFTP(hdl,msg);
        return 1;
    }
    
}

void AbortFTPRoutine(download_req_info_s* hdl)
{
    IsFotaProcessing = 0;
    IsMotaProcessing = 0;
    FTPState=FTP_STATE_CLOSED;
    SendFTPAttemptMsg(hdl);
    //nwy_power_off(2);
    //ThreadSleep(5000);
}

// void FotaRoutine(void)
// {
//     PreFotaRoutine();
//     if(!FTP_Login(FotaReq.IP,FotaReq.Port,0,FotaReq.User,FotaReq.Pass))
//     {
//         SendResponceFOTA("FOTA LOGIN ERROR!");
//         ThreadSleep(3000);
//         AbortFOTARoutine();
//         return;
//     }
//     if(!FTPDownloadFile(FotaReq.FilePath,SERVER_FOTA_FILEPATH))
//     {
//         SendResponceFOTA("FOTA Download ERROR!");
//         ThreadSleep(3000);
//         FTP_Logout();
//         AbortFOTARoutine();
//         return;
//     }
//     FTP_Logout();
//     SendResponceFOTA("FOTA Download Success. Installing...");
//     FotaReq.IsValid=0;
//     UpdateFOTAConfigInFlash();
//     ThreadSleep(3000);
//     FTPState=FTP_STATE_CLOSED;
//     if(FOTAUpdate(SERVER_FOTA_FILEPATH))
//         ThreadSleep(5000);
//     else
//     {
//         SendResponceFOTA("FOTA Installation Error");
//         ThreadSleep(3000);
//         AbortFOTARoutine();
//     }

// }

uint8_t FTPHandleReqType(download_req_info_s* hdl)
{
    if(hdl->RequestType == FTP_REQ_TYPE_FOTA)
    {
        if(FOTAUpdate(hdl->InternalFilePath))
            ThreadSleep(5000);
        else
        {
            SendResponceFTP(hdl,"FOTA Installation Error");
            ThreadSleep(3000);
            AbortFTPRoutine(hdl);
            return 0;
        }
        return 1;
    }
    else if(hdl->RequestType == FTP_REQ_TYPE_CONFIG)
    {
        //IsALVSend = 0;
        IsMotaProcessing = 1;
        if(ProcessMCUOTA(DownloadReq.InternalFilePath))
        {
            
            LOGData(TAG_FTP, "MCU Updated");
            SendResponceFTP(hdl, "MCU OTA Complete... Restarting for Update...");
            ThreadSleep(3000);
            IsMotaProcessing = 0;
            //nwy_power_off(2);  // Power cycle MCU
            ThreadSleep(5000);
            return 1;
        }
         else
        {
            //IsALVSend = 1;
            IsMotaProcessing = 0;
            SendResponceFTP(hdl, "MCU OTA Send err");
            ThreadSleep(3000);
            return 0;
        }
    }
    return 0;
}


uint8_t FTPStart(download_req_info_s* downloadHandle)
{
    if(downloadHandle->IsValid == FOTA_REQ_VALID_CODE)
    {
        if(downloadHandle->RequestType == FTP_REQ_TYPE_FOTA)
        {
            IsFotaProcessing = 1;
        }
        else if(downloadHandle->RequestType == FTP_REQ_TYPE_CONFIG)
        {
            IsMotaProcessing = 1;
        }
        PreFTPRoutine(downloadHandle);
        if(!FTP_Login(downloadHandle->IP,downloadHandle->Port,0,downloadHandle->User,downloadHandle->Pass))
        {
            SendResponceFTP(downloadHandle,"FTP LOGIN ERROR!");
            ThreadSleep(2500);
            AbortFTPRoutine(downloadHandle);
            return 0;
        }
        if(!FTPDownloadFile(downloadHandle->FilePath,downloadHandle->InternalFilePath))
        {
            SendResponceFTP(downloadHandle,"FTP Download ERROR!");
            ThreadSleep(2500);
            FTP_Logout();
            AbortFTPRoutine(downloadHandle);
            return 0;
        }
        FTP_Logout();
        SendResponceFTP(downloadHandle,"FTP Download Success. Processing File...");
        downloadHandle->IsValid=0;
        UpdateFTPConfigInFlash(downloadHandle);
        FTPState=FTP_STATE_CLOSED;
        ThreadSleep(1000);
        
         if(FTPHandleReqType(downloadHandle))
            downloadHandle->Status=1;
        else
            downloadHandle->Status=2;

        IsFotaProcessing = 0;
        IsMotaProcessing = 0;
    }
    return 1;
}










#endif











