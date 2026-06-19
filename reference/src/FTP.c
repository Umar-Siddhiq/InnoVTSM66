#include "FTP.h"
#include "nwy_test_cli_utils.h"
#include "nwy_test_cli_adpt.h"
#include "stdlib.h"
#include "stdarg.h"
#include "string.h"
#include "stdio.h"
#include "project.h"

#define FTP_EVENT_BASED
download_req_info_s  DownloadReq;
FTPStateTypedef FTPState;
FTPDownloadTypedef FTPDownloadState;

uint8_t FTPFileBuffer[FOTA_MAX_BUFF_SIZE];
uint32_t FTPFileSize;

#ifdef FTP_EVENT_BASED
nwy_osi_thread_t CurrentThread=NULL;
#else
uint16_t FTPTimeout;
#define FTP_LOGIN_TIMEOUT       12
#define FTP_DOWNLOAD_TIMEOUT    60
#endif


nwy_file_ftp_info_s CurrentFile;
uint32_t DataGetTimeout;

int FTP_LoadRecievedDatatoFile(nwy_file_ftp_info_s *pFileFtp, unsigned char* data,unsigned int len);

void PrintSystemSizes(void)
{
    unsigned long usersize;
    FLS_DeleteExistingFile(SERVER_FOTA_FILEPATH);
    FLS_DeleteExistingFile(SERVER_MOTA_FILEPATH);
    usersize = nwy_sdk_vfs_ls();
    nwy_dbg_log("File Rem Size : %lu",usersize);

}

static void FTP_CallbackFunction(nwy_ftp_result_t *param)
{
    int *size;
    #ifdef FTP_EVENT_BASED
    nwy_event_msg_t event={0};
    #endif
    //nwy_dbg_log("***********\n FTP CALLBACK!!!\n***********\n");
    if(NULL == param)
    {
        nwy_dbg_log("event is NULL\r\n");
    }
    
  
    if(NWY_FTP_EVENT_LOGIN == param->event)
    {
        nwy_dbg_log("Ftp login success");
        FTPState=FTP_STATE_CONNECTED;
        #ifdef FTP_EVENT_BASED
        event.id = FTP_EVENT_LOGIN_SUCESS;
        nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
        #endif
    }
    else if(NWY_FTP_EVENT_PASS_ERROR == param->event)
    {
        nwy_dbg_log("Ftp passwd error");
        FTPState=FTP_STATE_ERROR;
        #ifdef FTP_EVENT_BASED
        event.id = FTP_EVENT_LOGIN_FAIL;
        nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
        #endif
    }
    else if(NWY_FTP_EVENT_FILE_NOT_FOUND == param->event)
    {
        nwy_dbg_log("Ftp file not found");
        FTPDownloadState= FTP_TRANSFER_ERROR;
        #ifdef FTP_EVENT_BASED
        event.id = FTP_EVENT_FILE_SIZE_ERR;
        nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
        #endif

    }
    else if(NWY_FTP_EVENT_FILE_SIZE_ERROR == param->event)
    {
        nwy_dbg_log("Ftp file size error");
        FTPDownloadState = FTP_TRANSFER_ERROR;
        #ifdef FTP_EVENT_BASED
        event.id = FTP_EVENT_FILE_SIZE_ERR;
        nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
        #endif
    }
    else if(NWY_FTP_EVENT_SIZE == param->event)
    {
        size = (int*)param->data;
        nwy_dbg_log("Ftp size is %d", *size);
        FTPFileSize=*size;
        FTPDownloadState = FTP_TRANSFER_GOTSIZE;
        #ifdef FTP_EVENT_BASED
        event.id = FTP_EVENT_FILE_SIZE_SUCCESS;
        event.param1 = *size;
        nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
        #endif

    }
    else if(NWY_FTP_EVENT_LOGOUT == param->event)
    {
        nwy_dbg_log("Ftp logout");
        if(FTPState==FTP_STATE_CONNECTED)
            FTPState = FTP_STATE_CLOSED;
        else
            FTPState = FTP_STATE_ERROR;
        #ifdef FTP_EVENT_BASED
        event.id = FTP_EVENT_LOGOUT;
        nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
        #endif
    }
    else if(NWY_FTP_EVENT_CLOSED == param->event)
    {
        nwy_dbg_log("Ftp connection closed");
        if(FTPState==FTP_STATE_CONNECTED)
            FTPState = FTP_STATE_CLOSED;
        else
            FTPState = FTP_STATE_ERROR;
        #ifdef FTP_EVENT_BASED
        event.id = FTP_EVENT_LOGOUT;
        nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
        #endif
    }
    else if(NWY_FTP_EVENT_SIZE_ZERO == param->event)
    {
        nwy_dbg_log("Ftp size is zero");
        FTPDownloadState = FTP_TRANSFER_ERROR;
        #ifdef FTP_EVENT_BASED
        event.id = FTP_EVENT_FILE_SIZE_ERR;
        nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
        #endif
    }
    else if(NWY_FTP_EVENT_FILE_DELE_SUCCESS == param->event)
    {
        nwy_dbg_log("Ftp file del success");
    }
    else if(NWY_FTP_EVENT_DATA_PUT_FINISHED == param->event)
    {
        nwy_dbg_log("Ftp put file success");
    }
    else if(NWY_FTP_EVENT_DNS_ERR == param->event || NWY_FTP_EVENT_OPEN_FAIL == param->event)
    {
        nwy_dbg_log("Ftp login fail");
        FTPState = FTP_STATE_ERROR;
        #ifdef FTP_EVENT_BASED
        event.id = FTP_EVENT_LOGIN_FAIL;
        nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
        #endif
    }
    else if(NWY_FTP_EVENT_DATA_GET == param->event)
    {
        //nwy_dbg_log("Ftp recv %d data",param->data_len);
        // Data Recived Here
        if(CurrentFile.is_vaild)
        {
            FTP_LoadRecievedDatatoFile(&CurrentFile,(uint8_t*)param->data,param->data_len);
            FTPDownloadState=FTP_TRANSFER_DATACALLBACK;
            #ifdef FTP_EVENT_BASED
            event.id = FTP_EVENT_FILE_DOWNLOAD_PROGRESS;
            nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
            #endif
        }
        
        
    }
    else if(NWY_FTP_EVENT_DATA_CLOSED == param->event)
    {
        // File Downloaded here
        nwy_dbg_log("Ftp download finish");
        if(CurrentFile.is_vaild)
        {
            CurrentFile.is_vaild=0;
            FTPDownloadState=FTP_TRANSFER_COMPLETED;
            #ifdef FTP_EVENT_BASED
            event.id = FTP_EVENT_FILE_DOWNLOAD_COMPLETE;
            nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
            #endif
        }
        
    }
    else if(NWY_FTP_EVENT_OPEN_FAIL == param->event)
    {
        nwy_dbg_log("file data open faile.");
        FTPDownloadState = FTP_TRANSFER_ERROR;
        #ifdef FTP_EVENT_BASED
        event.id = FTP_EVENT_FILE_DOWNLOAD_ERR;
        nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
        #endif
    }
    else if(NWY_FTP_EVENT_DATA_SETUP_ERROR == param->event)
    {
        nwy_dbg_log("Ftp data setup error");
        FTPDownloadState = FTP_TRANSFER_ERROR;
        #ifdef FTP_EVENT_BASED
        event.id = FTP_EVENT_FILE_DOWNLOAD_ERR;
        nwy_send_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND);
        #endif
    }
    else
    {
        //nwy_dbg_log("data_size is %d", param->data_len);
        //nwy_dbg_log("param->data is %s", param->data);
    }
    return;
}

int FTP_LoadRecievedDatatoFile(nwy_file_ftp_info_s *pFileFtp, unsigned char* data,unsigned int len)
{
    int fs =nwy_sdk_fopen(pFileFtp->filename, NWY_CREAT | NWY_RDWR);
    if(fs < 0)
    {
        nwy_dbg_log("FTP Data Callback File Open Error");
        return -1;
    }
    nwy_sdk_fseek(fs, pFileFtp->pos, 0);
    nwy_sdk_fwrite(fs, data, len);
    nwy_sdk_fclose(fs);
    pFileFtp->pos += len;
    nwy_dbg_log("Downloaded %d / %d", pFileFtp->pos,pFileFtp->file_size);
    return 0;
}

uint8_t FTP_Login(char* IP, uint16_t Port, uint8_t IsActiveMode, char* User, char* Pass)
{
    int result;
    #ifdef FTP_EVENT_BASED
    nwy_event_msg_t event={0};
    #endif
    nwy_ftp_login_para_t ftp_param;
    //SendRS232String("Starting FOTA...");
    memset((void*)&ftp_param,0x00,sizeof(ftp_param));
    FTPDownloadState=FTP_TRANSFER_CLOSED;
    FTPState = FTP_STATE_CLOSED;
    ftp_param.channel=FTP_DEFAULT_CHANNEL;
    strcpy((char*)ftp_param.host,IP);
    ftp_param.port = Port;
    ftp_param.mode=IsActiveMode;
    strcpy((char*)ftp_param.username,User);
    strcpy((char*)ftp_param.passwd,Pass);
    ftp_param.timeout = FTP_LOGIN_TIMEOUT;
    #ifdef FTP_EVENT_BASED
    nwy_get_current_thread(&CurrentThread);
    if(CurrentThread==NULL)
    {
        nwy_dbg_log("FTP GET CURRENT THREAD ERR");
        return 0;
    }
    #endif
    nwy_dbg_log("FTP attempting Login @ %s, %d, %s, %s",ftp_param.host,ftp_param.port,ftp_param.username,ftp_param.passwd);
    FTPState = FTP_STATE_INIT;
    result = nwy_ftp_login(&ftp_param, FTP_CallbackFunction);
    if(result != NWY_RES_OK)
    {
        nwy_dbg_log("FTP Login err ret: %d",result);
        return 0;
    }
    #ifdef FTP_EVENT_BASED
    while(1)
    {
        if(!nwy_wait_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND))
        {
            nwy_dbg_log("FTP Login Callback Timeout!");
            return 0;
        }
        if(event.id==FTP_EVENT_LOGIN_SUCESS)
            break;
        if(event.id==FTP_EVENT_LOGIN_FAIL)
            return 0;
    } 
    #else
    FTPTimeout=5;
    while(FTPState!=FTP_STATE_CONNECTED)
    {
        if(FTPState==FTP_STATE_ERROR)
            return 0;

        if(--FTPTimeout==0)
        {
            nwy_dbg_log("FTP Login Timeout!");
            return 0;
        }
        nwy_sleep(1000);
        
    }
    #endif
    return 1;
}

uint8_t FTPGetFileSize(char* FileName, int *size)
{
    int ret;
    #ifdef FTP_EVENT_BASED
    nwy_event_msg_t event={0};
    #endif
    FTPDownloadState = FTP_TRANSFER_INIT;
    ret = nwy_ftp_filesize(FTP_DEFAULT_CHANNEL,FileName,FTP_FILESIZE_TIMEOUT);
    if(ret != NWY_SUCCESS) 
    {   
        nwy_dbg_log("Get File Size Command err ret: %d",ret);
        return 0;
    }
    #ifdef FTP_EVENT_BASED
    while(1)
    {
        if(!nwy_wait_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND))
        {
            nwy_dbg_log("FTP Size Callback Timeout!");
            return 0;
        }        

        if(event.id == FTP_EVENT_FILE_SIZE_SUCCESS)
            break;
        if(event.id == FTP_EVENT_FILE_SIZE_ERR)
            return 0;
        if(event.id == FTP_EVENT_LOGOUT)
            return 0;
    }
    *size = event.param1;
    #else
    FTPTimeout=5;
    while(FTPDownloadState!=FTP_TRANSFER_GOTSIZE)
    {
        if(FTPDownloadState==FTP_TRANSFER_ERROR)
            return 0;

                if(--FTPTimeout==0)
        {
            nwy_dbg_log("FTP Size Timeout!");
            return 0;
        }

        nwy_sleep(1000);
    }
    *size = FTPFileSize;
    #endif
    return 1;
}

uint8_t FLS_DeleteExistingFile(char* Filename)
{
    if(!nwy_sdk_fexist(Filename))
    {
        nwy_dbg_log("File Remove %s, already doesn't exist!",Filename);
        return 1;
    }
    if(nwy_sdk_file_unlink(Filename)!=NWY_SUCESS)
    {
        nwy_dbg_log("File Remove %s, Couldn't delete file!",Filename);
        return 0;
    }
    return 1;
}

uint8_t FTPDownloadFile(char* FTPFilePath, char* InternalFilePath)
{
    int ret;
    int FileSize=0;
    #ifdef FTP_EVENT_BASED
    nwy_event_msg_t event={0};
    #endif
    nwy_dbg_log("FTP Download Command Entry!");
    nwy_sleep(100);
    PrintSystemSizes();
    if(FTPState != FTP_STATE_CONNECTED)
    {
        nwy_dbg_log("Cant Download without FTP Logged In!");
        return 0;
    }
    if(!FTPGetFileSize(FTPFilePath,&FileSize))
    {
        nwy_dbg_log("Couldn't get Filesize for %s",FTPFilePath);
        return 0;
    }
    nwy_dbg_log("Got File Size: %d",FileSize);
    // if(!FLS_DeleteExistingFile(InternalFilePath))
    // {
    //     nwy_dbg_log("Cant Delete Existing File!");
    //     return 0;
    // }
    
    CurrentFile.is_vaild=1;
    CurrentFile.file_size=FileSize;
    strcpy(CurrentFile.filename,InternalFilePath);
    CurrentFile.pos=0;
    FTPDownloadState=FTP_TRANSFER_DATAREQ;
    if(nwy_ftp_get(FTP_DEFAULT_CHANNEL,FTPFilePath,FTP_FILEGET_TYPE_BINARY,0,0) != NWY_SUCCESS)
    {
        nwy_dbg_log("FTP Get Failed!");
        return 0;
    }
    #ifdef FTP_EVENT_BASED
    while(1)
    {
        if(!nwy_wait_thread_event(CurrentThread,&event,NWY_OSA_SUSPEND))
        {
            nwy_dbg_log("FTP Download Callback Timeout!");
            return 0;
        }  
        if(event.id == FTP_EVENT_FILE_DOWNLOAD_COMPLETE)
            break;
        if(event.id == FTP_EVENT_LOGOUT)
            return 0;
        if(event.id == FTP_EVENT_FILE_DOWNLOAD_ERR)
            return 0;
        if(event.id != FTP_EVENT_FILE_DOWNLOAD_PROGRESS)
            return 0;
    }
    #else
    FTPTimeout=10;
    while(FTPDownloadState!=FTP_TRANSFER_COMPLETED)
    {
        FTPDownloadState=FTP_TRANSFER_DATAREQ;
        nwy_sleep(5000);
        if(FTPDownloadState==FTP_TRANSFER_COMPLETED)
            break;
        if(FTPDownloadState == FTP_TRANSFER_ERROR)
        {
            nwy_dbg_log("FTP Download Intrupted!");
            return 0;
        }
        if(FTPDownloadState!=FTP_TRANSFER_DATACALLBACK)
        {   
            nwy_dbg_log("FTP Data Wait!");
            FTPTimeout--;
            if(FTPTimeout==0)
            {
                nwy_dbg_log("FTP Data Timeout!");
                return 0;
            }
        }
        else
            FTPTimeout=5;
    }

    #endif
    
    ret = nwy_sdk_fsize(InternalFilePath);
    if(ret != FileSize)
    {
        nwy_dbg_log("FILE Final Size Check fail!, exp : %d, got: %d",FileSize,ret);
        //SendRS232String("FOTA Download Failed due to Network!");
        nwy_sdk_file_unlink(InternalFilePath);
        nwy_power_off(2);
        return 0;
    }

    nwy_dbg_log("FTP Download Success!");
    nwy_dbg_log("User storage Size Left : %lu",nwy_sdk_vfs_ls());
    return 1;

}

uint8_t OTABuff[FOTA_MAX_BUFF_SIZE];

uint8_t FOTAUpdate(char *FileName)
{

    int fd, ret;
    ota_package_t ota_pack = {0};
    int ota_size = 0;
    int OTAFileSize;
	int tmp_len = 0;
	int read_len = 0;

    nwy_dbg_log("FOTA Function Entry\r\n");
    //SendRS232String("FOTA Downloaded, Installing...");
    fd = nwy_sdk_fopen(FileName, NWY_RDONLY);
	if(fd < 0)
	{
		nwy_dbg_log("FOTA open appimg fail");
		return 0;
	}
    memset(OTABuff,0x00,FOTA_MAX_BUFF_SIZE);
	ota_pack.data = OTABuff;
	ota_pack.len = 0;
	ota_pack.offset = 0;

	if(ota_pack.data == NULL)
	{
		nwy_dbg_log("FOTA PAcket malloc fail");
		return 0;
	}

	ota_size = nwy_sdk_fsize_fd(fd);
    OTAFileSize = ota_size;
	nwy_dbg_log("Fota_size:%d", ota_size);

	nwy_sdk_fseek(fd, 0, NWY_SEEK_SET);

    memset(FTPFileBuffer,0x00,FOTA_MAX_BUFF_SIZE);

	while(ota_size > 0)
	{
		tmp_len = FOTA_MAX_BUFF_SIZE;
		if(ota_size < tmp_len)
		{
			tmp_len = ota_size;
		}
		read_len = nwy_sdk_fread(fd, FTPFileBuffer, tmp_len);
		if(read_len <= 0)
		{
			nwy_dbg_log("FOTA read file error:%d", read_len);
			nwy_sdk_fclose(fd);
			return 0;
		}
		
		memcpy(ota_pack.data, FTPFileBuffer, read_len);
		ota_pack.len = read_len;

		ret = nwy_fota_dm(&ota_pack);

		if(ret < 0)
		{
			nwy_dbg_log("FOTA write ota error:%d", ret);
			nwy_sdk_fclose(fd);
			return 0;
		}
		ota_pack.offset += read_len;
        nwy_dbg_log("FOTA Installed :%d/%d", ota_pack.offset,OTAFileSize);
		memset(ota_pack.data, 0, read_len);
		ota_size -= read_len;
	}
	nwy_sdk_fclose(fd);
	nwy_dbg_log("FOTA write end");

	nwy_dbg_log("FOTA start checksum");

	ret = nwy_package_checksum();
	if(ret < 0)
	{
		nwy_dbg_log("checksum failed");
		return 0;
	}

	nwy_dbg_log("FOTA start update");
	ret = nwy_fota_ua();
	if(ret < 0)
	{
		nwy_dbg_log("update failed");
		return 0;
	}
    nwy_dbg_log("FOTA SUCCESSFULL !");
     return 1;


}

uint8_t FTP_Logout(void)
{
    nwy_dbg_log("FTP Logout Attempt...");
    if(nwy_ftp_logout(FTP_DEFAULT_CHANNEL,500)!=NWY_SUCESS)
    {
        nwy_dbg_log("FTP Logout Failed!");
    }
    nwy_sleep(2000);
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
    nwy_sleep(2000);
    hdl->AttemptCount--;
    if(hdl->AttemptCount == 0)
        DismissFTPReq(hdl);
    UpdateFTPConfigInFlash(hdl);
    //nwy_suspend_thread(tcp_recv_thread);
    //nwy_dbg_log("Closing Socket 2...");
    //TCPDisconnect(&ServerSocket[1]);
    //nwy_sleep(3000);
   
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
        sprintf(msg,"FTP Failed! Aborting...");
        SendResponceFTP(hdl,msg);
        return 0;
    }
    else 
    {   
        sprintf(msg,"FTP Failed. Attempting Again... %d / 3",4-(hdl->AttemptCount));
        SendResponceFTP(hdl,msg);
        return 1;
    }
    
}

void AbortFTPRoutine(download_req_info_s* hdl)
{
    FTPState=FTP_STATE_CLOSED;
    SendFTPAttemptMsg(hdl);
    //nwy_power_off(2);
    //nwy_sleep(5000);
}





// void FotaRoutine(void)
// {
//     PreFotaRoutine();
//     if(!FTP_Login(FotaReq.IP,FotaReq.Port,0,FotaReq.User,FotaReq.Pass))
//     {
//         SendResponceFOTA("FOTA LOGIN ERROR!");
//         nwy_sleep(3000);
//         AbortFOTARoutine();
//         return;
//     }
//     if(!FTPDownloadFile(FotaReq.FilePath,SERVER_FOTA_FILEPATH))
//     {
//         SendResponceFOTA("FOTA Download ERROR!");
//         nwy_sleep(3000);
//         FTP_Logout();
//         AbortFOTARoutine();
//         return;
//     }
//     FTP_Logout();
//     SendResponceFOTA("FOTA Download Success. Installing...");
//     FotaReq.IsValid=0;
//     UpdateFOTAConfigInFlash();
//     nwy_sleep(3000);
//     FTPState=FTP_STATE_CLOSED;
//     if(FOTAUpdate(SERVER_FOTA_FILEPATH))
//         nwy_sleep(5000);
//     else
//     {
//         SendResponceFOTA("FOTA Installation Error");
//         nwy_sleep(3000);
//         AbortFOTARoutine();
//     }

// }

uint8_t FTPHandleReqType(download_req_info_s* hdl)
{
    if(hdl->RequestType == FTP_REQ_TYPE_FOTA)
    {
        if(FOTAUpdate(hdl->InternalFilePath))
            nwy_sleep(5000);
        else
        {
            SendResponceFTP(hdl,"FOTA Installation Error");
            nwy_sleep(3000);
            AbortFTPRoutine(hdl);
            return 0;
        }
        return 1;
    }
    else if(hdl->RequestType == FTP_REQ_TYPE_CONFIG)
    {
        IsALVSend=0;
        if(ProcessMCUOTA(DownloadReq.InternalFilePath))
        {
            nwy_dbg_log("MCU Updated");
            SendResponceFTP(hdl,"MCU OTA Complete");
            nwy_sleep(3000);
            nwy_power_off(2);
            nwy_sleep(5000);
            return 1;
        }
        else
        {
            IsALVSend=1;
            SendResponceFTP(hdl,"MCU OTA Send err");
            nwy_sleep(3000);
            return 0;
        }
    }
    return 0;
}

nwy_osi_thread_t FotaThread;



void FOTAThreadEntry(void *param)
{
    download_req_info_s *downloadHandle = (download_req_info_s*)param;
    while(downloadHandle->IsValid == FOTA_REQ_VALID_CODE)
    {
        PreFTPRoutine(downloadHandle);
        if(!FTP_Login(downloadHandle->IP,downloadHandle->Port,0,downloadHandle->User,downloadHandle->Pass))
        {
            SendResponceFTP(downloadHandle,"FTP LOGIN ERROR!");
            nwy_sleep(2500);
            AbortFTPRoutine(downloadHandle);
            continue;
        }
        if(!FTPDownloadFile(downloadHandle->FilePath,downloadHandle->InternalFilePath))
        {
            SendResponceFTP(downloadHandle,"FTP Download ERROR!");
            nwy_sleep(2500);
            FTP_Logout();
            AbortFTPRoutine(downloadHandle);
            continue;
        }
        FTP_Logout();
        SendResponceFTP(downloadHandle,"FTP Download Success. Processing File...");
        downloadHandle->IsValid=0;
        UpdateFTPConfigInFlash(downloadHandle);
        FTPState=FTP_STATE_CLOSED;

         if(FTPHandleReqType(downloadHandle))
            downloadHandle->Status=1;
        else
            downloadHandle->Status=2;

    }
    nwy_exit_thread_self();
}



uint8_t FTPStart(download_req_info_s* ftpHandle)
{
    nwy_dbg_log("Ftp Thread Creating...");
    nwy_sleep(100);
    int ret = nwy_create_thread(&FotaThread, (1024*9), NWY_OSI_PRIORITY_NORMAL, "FtpThread", FOTAThreadEntry, (void*)ftpHandle, 16);
    if (ret != NWY_SUCESS) {
        nwy_dbg_log("Ftp Thread Start Failed !!!\r\n");
    }
    nwy_dbg_log("Ftp Thread Created!");
    while(ftpHandle->Status==0)
        nwy_sleep(500);
    nwy_dbg_log("Ftp Thread Closed, ret: %d",ftpHandle->Status);
    return ftpHandle->Status;
}


















