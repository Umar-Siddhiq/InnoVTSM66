#include "nwy_test_cli_adpt.h"
#include "nwy_test_cli_utils.h"
#include "nwy_osi_api.h"
#include "ctype.h"
#include "project.h"
#include "SOS.h"
#include "fdlibm.h"
#include "FTP.h"
#include "Geofence.h"
#include "DHT11.h"
#include "Sensors.h"
#ifdef PROTO_CDAC
#include "HTTP.h"
#endif

uint8_t IsDebug=0;
uint8_t stkCb;
volatile Providertypedef prfReq = NONE;  // FIX #2: Made volatile to prevent race conditions
uint8_t PrfChanged;
volatile TickTypeDef IntervalTick;
VTSTypedef VTSData;
VTSStateTypedef VTSState;
int EchoUart;
char FirmVer[15];
extern uint8_t updntp;
#ifdef PROTO_CDAC
extern VehicleTypeDef VehicleState;
extern char VehicleMovingMode;
#endif



void InitSockets(void);

 nwy_osi_thread_t nwy_systic_thread = 0;
 nwy_osi_thread_t nwy_test_cli_thread = 0;
 nwy_osi_thread_t nwy_mcu_thread = 0;
 nwy_osi_thread_t nwy_gprs_thread = 0;
#ifdef USE_TCP
 nwy_osi_thread_t nwy_tcp_thread1 = 0;
 nwy_osi_thread_t nwy_tcp_thread2 = 0;
 nwy_osi_thread_t nwy_tcp_thread3 = 0;
 nwy_osi_thread_t nwy_tcp_thread4 = 0;
#endif
nwy_osi_thread_t nwy_dht11_thread = 0;
#ifdef HTTP_QUEUE
nwy_osi_thread_t nwy_httpqueue_thread = 0;
#endif

#ifdef  _DATASEND
 nwy_osi_thread_t nwy_server_thread = 0;
#endif
#ifdef EXTENDED_IPS
TCPSocketTypedef ServerSocket[4];
#else
TCPSocketTypedef ServerSocket[3];
#endif


static unsigned char tolow(unsigned char c)
{
    if ((c > 64) && (c<=90))
        c -= 'A'-'a';
    return c;
}



void LowerString(char	*str)
{
	uint16_t len = strlen(str);
	uint16_t i;
	for(i = 0; i < len;i++)
	{
		str[i] = tolow(str[i]);
	}
}


void nwy_test_cli_dbg(const char *func, int line, char *fmt, ...)
{
    static char buf[1024];
    va_list args;
    int len = 0;

    memset(buf, 0, sizeof(buf));

    sprintf(buf, "NWY_CLI %s[%d]:", func, line);
    len = strlen(buf);
    va_start(args, fmt);

    vsnprintf(&buf[len], sizeof(buf) - len - 1, fmt, args);
    va_end(args);

    nwy_open_sdk_log("%s\n", buf);
}

void nwy_dbg_log(char *fmt, ...)
{
    static char buf[1024];
    va_list args;
    int len = 0;

    memset(buf, 0, sizeof(buf));

    sprintf(buf, "DBG: ");
    len = strlen(buf);
    va_start(args, fmt);

    vsnprintf(&buf[len], sizeof(buf) - len - 1, fmt, args);
    va_end(args);

    nwy_open_sdk_log("%s", buf);
    //OSI_LOGI(0,"%s\n",buf);
    //nwy_uart_send_data(EchoUart,&buf[len],strlen(&buf[len]));
}

void print_long_string(const char* long_string) {
    size_t len = strlen(long_string);
    size_t chunk_size = 100; // Max bytes per log
    size_t total_chunks = (len + chunk_size - 1) / chunk_size; // Calculate total chunks

    for (size_t i = 0; i < total_chunks; i++) {
        // Calculate the start and end indices for the current chunk
        size_t start_index = i * chunk_size;
        size_t end_index = start_index + chunk_size;
        
        // Adjust end_index if it exceeds the length of the string
        if (end_index > len) {
            end_index = len;
        }

        // Create the chunk and the header
        char chunk[chunk_size + 50]; // Extra space for header
        snprintf(chunk, sizeof(chunk), "[%zu/%zu] %.*s", start_index + 1, len, (int)(end_index - start_index), long_string + start_index);
        
        // Print the chunk using nwy_dbg_log
        nwy_dbg_log(chunk);
    }
}

int nwy_test_cli_wait_select()
{
    nwy_event_msg_t event;

    while (1)
    {
        memset(&event, 0, sizeof(event));
        nwy_wait_thread_event(nwy_test_cli_thread, &event, NWY_OSA_SUSPEND);
        if (event.id == NWY_EXT_INPUT_RECV_MSG)
        {
            return 1;
        }
    }
}

void nwy_test_cli_select_enter()
{
    nwy_event_msg_t event;

    memset(&event, 0, sizeof(event));
    event.id = NWY_EXT_INPUT_RECV_MSG;
    nwy_send_thread_event(nwy_test_cli_thread, &event, NWY_OSA_SUSPEND);
}

void nwy_test_cli_send_trans_end()
{
    nwy_event_msg_t event;

    memset(&event, 0, sizeof(event));
    event.id = NWY_EXT_DATA_REC_END_MSG;
    nwy_send_thread_event(nwy_test_cli_thread, &event, NWY_OSA_SUSPEND);
}

int nwy_test_cli_wait_trans_end()
{
    nwy_event_msg_t event;

    memset(&event, 0, sizeof(event));
    nwy_wait_thread_event(nwy_test_cli_thread, &event, NWY_OSA_SUSPEND);
    if (event.id == NWY_EXT_DATA_REC_END_MSG)
    {
        return 1;
    }
    return 0;
}

void CheckprfReq(void)
{
    uint8_t tries=3;
    char resp[50];
    if(GSM.GSMState<SIM_DETECTED)
        return;
    if(VTSState.CurrentProfile == NONE)
    {
        prfReq=VTSData.DefProfile;
    }

    if(prfReq != NONE)
    {
        while(!SwitchProfile(prfReq))
        {
            tries--;
            if(!tries)
            {
                nwy_dbg_log("*********************\nUNABLE TO SWTICH TO REQUESTED SIM PROFILE\n*********************");
                SendAtCmd("AT+CFUN=1,1\r\n",resp,"OK");
                nwy_sleep(5000);  // FIX #5: Increased from 2s to 5s - modem needs time to reinitialize after radio reset
                return;
            }
        }
        VTSState.CurrentProfile=prfReq;
        UpdateStateInFlash();
        prfReq=NONE;
        PrfChanged=1;
        SendAtCmd("AT+CFUN=1,1\r\n",resp,"OK");
        nwy_sleep(5000);  // FIX #5: Increased from 2s to 5s - modem needs time to reinitialize after radio reset
    }
    
}
static void nwy_test_cli_main_func(void *param)
{
    EnableVat();
    PeripheralInit();
    mcu_InitUart();
    LoadConfig();
    LoadState();
    InitSensors();
    LoadSensorConfigFromFlash();
    PrevMain = VTSState.IsPrevMain;
    LoadFTPConfig(&DownloadReq);
    InitSMS();
    SOSInit(VTSData.IntervalData.SOSTimeOut);
    #ifndef PROTO_CDAC
	VTSData.IntervalData.CurrentInterval=VTSData.IntervalData.DataInterval;
    #else
    VehicleState.PacketState=NORMAL;
	VehicleState.VehicleMode=HALT;
    VehicleMovingMode='H';
    VTSData.IntervalData.CurrentInterval=VTSData.IntervalData.HaltInterval;
    #endif
    AlertInitStruct();
    updntp=1;
    while (1)
    {

        ProcessPeripheral();
        CheckprfReq();
        UpdateGeoFence();
        nwy_sleep(500);
        //nwy_dbg_log("Working as of %d",nwy_get_ms());
    }
}

void ConnectedCallback(void)
{
    #ifndef PROTO_CDAC
    SendLogin1 = 1;
    #endif
    nwy_dbg_log("Server 1 Connection CALLBACK!!!!!!");
    SetStatLED(10,10);   
}



void ConnectedCallback2(void)
{

    SendLogin2 = 1;
    nwy_dbg_log("Server 2 Connection CALLBACK!!!!!!");
    #ifdef PROTO_CDAC
    SetStatLED(10,10);   
    #endif
}

void DisConnectedCallback(void)
{
    #ifndef HTTP_SIMULATE
    SetStatLED(10,9);
    #endif
    ServerSocket[0].SocketState = SOCKET_CLOSED;
    ServerSocket[1].SocketState = SOCKET_CLOSED;
}

void DisConnectedCallback2(void)
{ 
    ServerSocket[2].SocketState = SOCKET_CLOSED;
}

void ConnectedCallback3(void)
{
    SendLogin3 = 1;
    nwy_dbg_log("Server 3 (Extended IP) Connection CALLBACK!!!!!!");
    SetStatLED(10,10);   
}

void DisConnectedCallback3(void)
{ 
    ServerSocket[3].SocketState = SOCKET_CLOSED;
}

void whileConnected(void)
{
    ServerHangTimeOut=0;
}

static void nwy_usb_con_echo(char *string)
{
    if(!IsDebug)
        return;
    nwy_uart_send_data(1, string, strlen(string));
    nwy_open_sdk_log("usb con out[%d]:%s", strlen(string), string);
}

void nwy_usb_con_recv(const char *data, uint32 length)
{
    nwy_usb_con_echo("\r\nusb con recv:");
    nwy_usb_con_echo((char *)data);
    nwy_open_sdk_log("usb con in[%d]:%s", length, data);
}

void UpdateStateInFlash(void)
{
    int fd=-1,ret;
    fd = nwy_sdk_fopen(STATE_FILE_PATH,NWY_WB_PLUS_MODE);
    if(fd<0)
    {
        nwy_dbg_log("State FIle Create ERROR\n");
        return;
    }

    ret = nwy_sdk_fwrite(fd,(void*)&VTSState,sizeof(VTSStateTypedef));
    nwy_dbg_log("State file write size :%d\n",ret);
    nwy_sdk_fclose(fd);
}

void UpdateConfigInFlash(void)
{
    int fd=-1,ret;
    fd = nwy_sdk_fopen(CONFIG_FILE_PATH,NWY_WB_PLUS_MODE);
    if(fd<0)
    {
        nwy_dbg_log("Config FIle Create ERROR\n");
        return;
    }

    ret = nwy_sdk_fwrite(fd,(void*)&VTSData,sizeof(VTSTypedef));
    nwy_dbg_log("Config file write size :%d\n",ret);
    nwy_sdk_fclose(fd);
}

void UpdateFTPConfigInFlash(download_req_info_s* FTPHandle)
{
    int fd=-1,ret;
    fd = nwy_sdk_fopen(FOTA_CONFIG_PATH,NWY_WB_PLUS_MODE);
    if(fd<0)
    {
        nwy_dbg_log("Fota Config FIle Create ERROR\n");
        return;
    }

    ret = nwy_sdk_fwrite(fd,(void*)FTPHandle,sizeof(download_req_info_s));
    nwy_dbg_log("Fota Config file write size :%d\n",ret);
    nwy_sdk_fclose(fd);
}
void LoadDefault(void)
{
    VTSData.DefID = DEFVAL;
	strcpy(VTSData.VendorID,DEFAULT_VENDOR);
	VTSData.BattThrs=LOW_BAT_THRS_VOLT;
    #ifndef PROTO_CDAC
	VTSData.IntervalData.DataInterval=DEFAULT_INV_DATA;
	VTSData.IntervalData.HealthInterval=DEFAULT_INV_HEALTH;
	VTSData.IntervalData.IgnitionInterval=DEFAULT_INV_IGN;
	VTSData.IntervalData.SOSInterval=DEFAULT_INV_SOS;
	VTSData.IntervalData.SOSTimeOut=DEFAULT_INV_STM;
	VTSData.IntervalData.StandbyInterval=DEFAULT_INV_STB;
    #else
	VTSData.IntervalData.HealthInterval=DEFAULT_INV_HEALTH;
	VTSData.IntervalData.MotionInterval=DEFAULT_INV_MOTION;
	VTSData.IntervalData.HaltInterval=DEFAULT_INV_HALT;
	VTSData.IntervalData.EnergencyInterval=DEFAULT_INV_CRIT;
	VTSData.IntervalData.SOSTimeOut=DEFAULT_INV_STM*60;
	VTSData.IntervalData.SleepInterval=DEFAULT_INV_SLEEP;
    VTSData.IntervalData.FullDataPacketInterval=DEFAULT_INV_FULL;
	VTSData.IntervalData.HaltTime=DEFAULT_HALT_TIME;
	VTSData.IntervalData.SleepTime=DEFAULT_SLEEP_TIME;
    #endif
	
	strcpy(VTSData.ServerData.IP1,DEFAULT_IP1); // 13.234.160.106 // 103.143.84.2
	strcpy(VTSData.ServerData.Port1,DEFAULT_PORT1);  // 18110
    VTSData.ServerData.IPConfig[0]=1;
    #ifndef PROTO_CDAC
	strcpy(VTSData.ServerData.IP2,DEFAULT_IP2); // 13.234.160.106 //103.143.84.2
	strcpy(VTSData.ServerData.Port2,DEFAULT_PORT2);   // 18110
    VTSData.ServerData.IPConfig[1]=1;
    #endif
    strcpy(VTSData.ServerData.IP3,DEFAULT_IP3);
	strcpy(VTSData.ServerData.Port3,DEFAULT_PORT3);
    VTSData.ServerData.IPConfig[2]=1;   
    #ifdef EXTENDED_IPS
    strcpy(VTSData.ServerData.Url2,DEFAULT_IP4);
    #endif
    VTSData.SensorSetting.Uart2Mode = UART2_MODE_RFID;
    VTSData.SensorSetting.IP2Mode = IP2_MODE_DHT11;
    VTSData.SensorSetting.IGNInterval = 20;
    VTSData.SensorSetting.OFFInterval = 300;

	VTSData.VehicleData.HarshAcc=DEFAULT_HA;
	VTSData.VehicleData.HarshBreak=DEFAULT_HB;
	VTSData.VehicleData.OverSpeed=DEFAULT_OVERSPEED;
	VTSData.VehicleData.DefaultSpeed=DEFAULT_SPEED;
	VTSData.VehicleData.RashTurn=DEFAULT_RT;
	VTSData.VehicleData.TiltAngle=DEFAULT_TL;


	strcpy(VTSData.VehicleData.VehicleRegNo,DEFAULT_VEHREG);

	strcpy(VTSData.PhoneNumber.Mob0,DEFAULT_MOB0);
	strcpy(VTSData.PhoneNumber.Mob1,DEFAULT_MOB1); 

    #ifdef SIMMAKE_TACHNOJACKS
    VTSData.SIMMake = TAISYS;
    VTSData.DefProfile=SIM_PROFILE_AIRTEL;
    #warning SIM MAKE TECHNOJACKS SELECTED
    #elif defined SIMMAKE_APM
    VTSData.SIMMake = TAISYS;
    VTSData.DefProfile=SIM_PROFILE_AIRTEL;
    #warning SIM MAKE APM SELECTED
    #elif defined SIMMAKE_IDEMIA_3P
    VTSData.SIMMake = TAISYS;
    VTSData.DefProfile=SIM_PROFILE_AIRTEL;
    #warning SIM MAKE IDEMIA_3P SELECTED
    #elif defined SIMMAKE_SENS
    VTSData.SIMMake = SENSORISE;
    VTSData.DefProfile = SIM_PROFILE_AIRTEL;
    #warning SIM MAKE SENSORISE SELECTED
    #elif defined SIMMAKE_GND
    VTSData.SIMMake = GnD;
    VTSData.DefProfile = SIM_PROFILE_AIRTEL;
    #warning SIM MAKE GND SELECTED
    #else
    #error NO SIM MAKE DEFINED
    #endif


 
    VTSData.AutoAPN=1;
    sprintf(VTSData.mAPN,"AIRTELIOT.COM");
    ClearGeofence();
    UpdateConfigInFlash();
    InitSockets();

}

void LoadDefaultState(void)
{
    VTSState.OdoCount = 0;
    VTSState.DefVal = DEFSTATE;
    VTSState.CurrentProfile = NONE;
    VTSState.IsPrevMain=1;
    VTSState.LastAttemptedProfile = 0;
    // Initialize profile fail counters
    for(int i = 0; i < 4; i++)
        VTSState.ProfileFailCount[i] = 0;
    UpdateStateInFlash();
}

uint8_t LoadServerStringToSocket(TCPSocketTypedef *socket, char *serverstring)
{
    if(serverstring[0] == 'N' && serverstring[1] == 'A')
    {
        strcpy(socket->DNSorIP,"NA");
        return 0;
    }
    char *fn, url[100] = {0};
    strncpy(url, serverstring, sizeof(url) - 1);
    url[sizeof(url) - 1] = '\0'; 
    fn = strchr(url,',');
    if(!fn || *(fn + 1) == '\0') 
    {
        strcpy(socket->DNSorIP, "NA");
        return 0;
    }
    fn[0]=0;
    fn++;
    if(*fn == '\0') {
        strcpy(socket->DNSorIP, "NA");
        return 0;
    }
    strcpy(socket->DNSorIP,url);
    socket->Port = atoi(fn);
    return 1;
}

void InitSockets(void)
{
    // First disable all sockets to signal TCP thread to stop using them
    // This provides thread safety when called from SMS handler
    for(int i = 0; i < MAX_TCP_SOCKETS; i++)
    {
        ServerSocket[i].isEnabled = 0;
    }
    
    // Give TCP async manager time to see disabled state and stop processing
    nwy_sleep(100);
    
    // Now safe to close any existing sockets
    for(int i = 0; i < MAX_TCP_SOCKETS; i++)
    {
        if(ServerSocket[i].SocketIndex > 0)
        {
            nwy_dbg_log("InitSockets: Closing existing socket %d (Index %d)", 
                       ServerSocket[i].SocketNo, ServerSocket[i].SocketIndex);
            TCPSocket_Disconnect(&ServerSocket[i]);
        }
    }
    
    // Initialize Socket 1
    if(VTSData.ServerData.IPConfig[0])
        ServerSocket[0].isEnabled=1;
    else
        ServerSocket[0].isEnabled=0;
    ServerSocket[0].SOCKET_IPType = SOCKET_IP4;
    ServerSocket[0].SocketNo = 1;
    ServerSocket[0].SocketIndex = 0;
    ServerSocket[0].SocketState = SOCKET_CLOSED;
    ServerSocket[0].reconnect_delay_ms = 0;  // Reset exponential backoff
    ServerSocket[0].last_close_time = 0;
    ServerSocket[0].connect_start_time = 0;
    ServerSocket[0].noackcount = 0;
    strcpy(ServerSocket[0].DNSorIP,VTSData.ServerData.IP1);
    ServerSocket[0].Port = atoi(VTSData.ServerData.Port1);
    ServerSocket[0].OnConnect = &ConnectedCallback;
    ServerSocket[0].Connected = &whileConnected;
    ServerSocket[0].OnDisconnect = &DisConnectedCallback;
    ServerSocket[0].rxSizeMAX = SERVER_RX_SIZE_MAX;
    ServerSocket[0].rxBuffer = Server1RxData;
    
    // Initialize Socket 2
    if(VTSData.ServerData.IPConfig[1])
        ServerSocket[1].isEnabled=1;
    else
        ServerSocket[1].isEnabled=0;
    ServerSocket[1].SOCKET_IPType = SOCKET_IP4;
    ServerSocket[1].SocketNo = 2;
    ServerSocket[1].SocketIndex = 0;
    ServerSocket[1].SocketState = SOCKET_CLOSED;
    ServerSocket[1].reconnect_delay_ms = 0;  // Reset exponential backoff
    ServerSocket[1].last_close_time = 0;
    ServerSocket[1].connect_start_time = 0;
    ServerSocket[1].noackcount = 0;
    strcpy(ServerSocket[1].DNSorIP,VTSData.ServerData.IP2);
    ServerSocket[1].Port = atoi(VTSData.ServerData.Port2);

    // Initialize Socket 3
    if(VTSData.ServerData.IPConfig[2])
        ServerSocket[2].isEnabled=1;
    else
        ServerSocket[2].isEnabled=0;
    ServerSocket[2].SOCKET_IPType = SOCKET_IP4;
    ServerSocket[2].SocketNo = 3;
    ServerSocket[2].SocketIndex = 0;
    ServerSocket[2].SocketState = SOCKET_CLOSED;
    ServerSocket[2].reconnect_delay_ms = 0;  // Reset exponential backoff
    ServerSocket[2].last_close_time = 0;
    ServerSocket[2].connect_start_time = 0;
    ServerSocket[2].noackcount = 0;
    strcpy(ServerSocket[2].DNSorIP,VTSData.ServerData.IP3);
    ServerSocket[2].Port = atoi(VTSData.ServerData.Port3);
    ServerSocket[2].OnConnect = &ConnectedCallback2;
    ServerSocket[2].OnDisconnect= &DisConnectedCallback2;
    ServerSocket[2].rxSizeMAX = SERVER_RX_SIZE_MAX;
    ServerSocket[2].rxBuffer = Server2RxData;
    
    #ifdef EXTENDED_IPS
    // Initialize Socket 4
    ServerSocket[3].isEnabled=1;
    ServerSocket[3].SOCKET_IPType = SOCKET_IP4;
    ServerSocket[3].SocketNo = 4;
    ServerSocket[3].SocketIndex = 0;
    ServerSocket[3].SocketState = SOCKET_CLOSED;
    ServerSocket[3].reconnect_delay_ms = 0;  // Reset exponential backoff
    ServerSocket[3].last_close_time = 0;
    ServerSocket[3].connect_start_time = 0;
    ServerSocket[3].noackcount = 0;
    ServerSocket[3].OnConnect = &ConnectedCallback3;
    ServerSocket[3].OnDisconnect = &DisConnectedCallback3;
    ServerSocket[3].rxSizeMAX = SERVER_RX_SIZE_MAX;
    ServerSocket[3].rxBuffer = Server2RxData;
    LoadServerStringToSocket(&ServerSocket[3],VTSData.ServerData.Url2);
    nwy_dbg_log("Socket 4 initialized - DNSorIP: %s, Port: %d", 
                ServerSocket[3].DNSorIP, ServerSocket[3].Port);
    #endif  
    
    nwy_dbg_log("InitSockets: All sockets reinitialized with reset timing");
}

void LoadState(void)
{
    int fd=-1;
    int ret;
    if(nwy_sdk_fexist(STATE_FILE_PATH))
    {
        ret = nwy_sdk_fsize(STATE_FILE_PATH);
        if(ret != sizeof(VTSStateTypedef))
        {
            nwy_dbg_log("State file Invalid size");
            LoadDefaultState();
            return;
        }
        fd = nwy_sdk_fopen(STATE_FILE_PATH,NWY_RDONLY);
        if(fd<0)
        {
            nwy_dbg_log("State file Cant Open! Loading Default...");
            LoadDefaultState();
            return;
        }
        
        ret = nwy_sdk_fread(fd,(void*)&VTSState,sizeof(VTSStateTypedef));
        nwy_dbg_log("State file Read size :%d\n",ret);
        if(VTSState.DefVal != DEFSTATE)
        {
            nwy_dbg_log("State file Def Mismatch! Loading Default...");
            LoadDefaultState();
            nwy_sdk_fclose(fd);
            return;
        }
        
        // Initialize profile fail counters if they're all zero (backward compatibility)
        uint8_t all_zero = 1;
        for(int i = 0; i < 4; i++)
        {
            if(VTSState.ProfileFailCount[i] != 0)
            {
                all_zero = 0;
                break;
            }
        }
        
        nwy_dbg_log("State loaded - Current Profile: %d, Fail counts: [%d, %d, %d]", 
                    VTSState.CurrentProfile,
                    VTSState.ProfileFailCount[1], 
                    VTSState.ProfileFailCount[2], 
                    VTSState.ProfileFailCount[3]);
        
        nwy_sdk_fclose(fd);
        return;

    }
    nwy_dbg_log("No State file ! Loading Default...");
    LoadDefaultState();
}

void LoadConfig(void)
{
    int fd=-1;
    int ret;
    if(nwy_sdk_fexist(CONFIG_FILE_PATH))
    {
        ret = nwy_sdk_fsize(CONFIG_FILE_PATH);
        if(ret != sizeof(VTSTypedef))
        {
            nwy_dbg_log("CONFIG file Invalid size");
            LoadDefault();
            return;
        }
        fd = nwy_sdk_fopen(CONFIG_FILE_PATH,NWY_RDONLY);
        if(fd<0)
        {
            nwy_dbg_log("CONFIG file Cant Open! Loading Default...");
            LoadDefault();
            return;
        }
        
        ret = nwy_sdk_fread(fd,(void*)&VTSData,sizeof(VTSTypedef));
        nwy_dbg_log("CONFIG file Read size :%d\n",ret);
        if(VTSData.DefID != DEFVAL)
        {
            nwy_dbg_log("CONFIG file Def Mismatch! Loading Default...");
            LoadDefault();
            nwy_sdk_fclose(fd);
            return;
        }
        nwy_sdk_fclose(fd);
        InitGeoState();  // Initialize GeoState for geofences loaded from flash
        InitSockets();
        return;

    }
    nwy_dbg_log("No CONFIG file ! Loading Default...");
    LoadDefault();
}

void LoadFTPConfig(download_req_info_s* FTPHandle)
{
    int fd=-1;
    int ret;
    if(nwy_sdk_fexist(FOTA_CONFIG_PATH))
    {
        ret = nwy_sdk_fsize(FOTA_CONFIG_PATH);
        if(ret != sizeof(download_req_info_s))
        {
            nwy_dbg_log("FOTA Req file Invalid size");
            return;
        }
        fd = nwy_sdk_fopen(FOTA_CONFIG_PATH,NWY_RDONLY);
        if(fd<0)
        {
            nwy_dbg_log("Unable to open FOTA Req file");
            return;
        }
        ret = nwy_sdk_fread(fd,(void*)FTPHandle,sizeof(download_req_info_s));
        if(FTPHandle->IsValid == FOTA_REQ_VALID_CODE)
        {
            nwy_dbg_log("FOTA REQUEST FOUND, Attempting Upon Data Connection...");
            IsFTPReq=1;
            return;
        }
        nwy_sdk_fclose(fd);
        nwy_dbg_log("No Fota Req!");
        return;

    }
    nwy_dbg_log("No Fota Req File");
}



void nwy_open_app_entry(void)
{
    int ret = NWY_GEN_E_UNKNOWN;
    nwy_sleep(5000);

    EchoUart = nwy_uart_init(NWY_NAME_UART2, 1);
    nwy_uart_set_baud(EchoUart, 9600);

    nwy_uart_reg_recv_cb(EchoUart, RFIDRcv);

    //  int hd = nwy_uart_init(NWY_NAME_UART1, 1);
    // nwy_uart_set_baud(hd, 115200);

    // nwy_uart_reg_recv_cb(hd, MCURcv);
    #ifdef BSNL_PROTO
    sprintf(FirmVer,"FV_NI1_1.1.9");
    #else
    #ifdef PROTO_CDAC
    sprintf(FirmVer,"V%s",FIRMWAREVERSION);
    #else
    sprintf(FirmVer,"%s",FIRMWAREVERSION);
    #endif
    #endif
    nwy_dbg_log("+++++++++++++++ VTS APP ENRTY ++++++++++++++++");
    nwy_dbg_log("AMP Groups 2G VTS APP Version: V%s\r\n",FirmVer);


#ifdef NWY_OPEN_TEST_SMS
    nwy_init_sms_option();
    nwy_sms_reg_recv_cb((nwy_sms_recv_cb_t)SMS_rcvcb);//nwy_sms_test_recv_cb
#endif

    nwy_nw_register_callback_func((nwy_nw_cb_func)nwy_nw_register_callback_cb);

    ret = nwy_create_thread(&nwy_test_cli_thread, 1024*4, NWY_OSI_PRIORITY_NORMAL, "nwy_test_cli", nwy_test_cli_main_func, NULL, 16);
    if (ret != NWY_SUCESS) {
        nwy_usb_con_echo("\r\nCLI START thread create fail\r\n");
    }
    
 
    ret = nwy_create_thread(&nwy_systic_thread, 1024*3, NWY_OSI_PRIORITY_NORMAL, "nwy_test_cli", SysticThreadEntry, NULL, 16);
    if (ret != NWY_SUCESS) {
        nwy_usb_con_echo("\r\nsystic START thread create fail\r\n");
    }


    ret = nwy_create_thread(&nwy_gprs_thread, 1024*4, NWY_OSI_PRIORITY_NORMAL, "nwy_gprs_thread", GprsThreadEntry, NULL, 16);
    if (ret != NWY_SUCESS) {
        nwy_usb_con_echo("\r\nGPRS START thread create fail\r\n");
    }

    #ifdef USE_TCP
    #ifndef PROTO_CDAC
    ret = nwy_create_thread(&nwy_tcp_thread1, 1024*5, NWY_OSI_PRIORITY_NORMAL, "nwy_tcp_thread1", TCPThreadEntry, (void*)&ServerSocket[0], 16);
    if (ret != NWY_SUCESS) {
        nwy_usb_con_echo("\r\nSocket 1 START thread create fail\r\n");
    }
    // ret = nwy_create_thread(&nwy_tcp_thread2, 1024*4, NWY_OSI_PRIORITY_NORMAL, "nwy_tcp_thread2", TCPThreadEntry, (void*)&ServerSocket[1], 16);
    // if (ret != NWY_SUCESS) {
    //     nwy_usb_con_echo("\r\nSocket 2 START thread create fail\r\n");
    // }
    #else
    ret = nwy_create_thread(&nwy_tcp_thread1, 1024*6, NWY_OSI_PRIORITY_NORMAL, "nwy_http_thread1", HTTPThreadEntry, (void*)&ServerSocket[0], 16);
    if (ret != NWY_SUCESS) {
        nwy_usb_con_echo("\r\nSocket 1 START thread create fail\r\n");
    }
    #endif
    // ret = nwy_create_thread(&nwy_tcp_thread3, 1024*4, NWY_OSI_PRIORITY_NORMAL, "nwy_tcp_thread3", TCPThreadEntry, (void*)&ServerSocket[2], 16);
    // if (ret != NWY_SUCESS) {
    //     nwy_usb_con_echo("\r\nSocket 3 START thread create fail\r\n");
    // }
    // ret = nwy_create_thread(&nwy_tcp_thread4, 1024*4, NWY_OSI_PRIORITY_NORMAL, "nwy_tcp_thread4", TCPThreadEntry, (void*)&ServerSocket[3], 16);
    // if (ret != NWY_SUCESS) {
    //     nwy_usb_con_echo("\r\nSocket 3 START thread create fail\r\n");
    // }
    #endif

    #ifdef HTTP_QUEUE
    extern void HttpQueueThreadEntry(void* param);
    ret = nwy_create_thread(&nwy_httpqueue_thread, 1024*4, NWY_OSI_PRIORITY_NORMAL, "nwy_httpqueue_thread", HttpQueueThreadEntry, NULL, 16);
    if (ret != NWY_SUCESS) {
        nwy_usb_con_echo("\r\nHttpQueue START thread create fail\r\n");
    }
    #endif

    #define USE_MCU
    #ifdef USE_MCU
    ret = nwy_create_thread(&nwy_mcu_thread, 1024*4, NWY_OSI_PRIORITY_NORMAL, "nwy_mcu_thread", MCUThreadEntry, NULL, 16);
    if (ret != NWY_SUCESS) {
        nwy_usb_con_echo("\r\nMCU START thread create fail\r\n");
    }
    #endif

    
    #ifdef  _DATASEND
    ret = nwy_create_thread(&nwy_server_thread, (1024*6), NWY_OSI_PRIORITY_NORMAL, "nwy_Server_thread", ServerThreadEntry, NULL, 16);
    if (ret != NWY_SUCESS) {
        nwy_usb_con_echo("\r\nServer START thread create fail\r\n");
    }
    #endif

    // ret = nwy_create_thread(&nwy_dht11_thread, 1024*2, NWY_OSI_PRIORITY_NORMAL, "nwy_dht11_thread", DHT11ThreadEntry, NULL, 8);
    // if (ret != NWY_SUCESS) {
    //     nwy_usb_con_echo("\r\nMCU START thread create fail\r\n");
    // }

}
void nwy_cli_tmupd_cb(uint8 *data, int len)
{
    nwy_dbg_log("Time Update Callback");
}

#ifdef NWY_OPEN_TEST_VIRT_AT
extern void nwy_cli_pull_out_sim(uint8 *data, int len);
void nwy_cli_stk_cb(uint8 *data, int len)
{
    stkCb=1;
	nwy_dbg_log("nwy_cli_stk test");
	nwy_dbg_log("%s\n", data);
}
void nwy_cli_init_unsol_reg()
{
    //nwy_sdk_at_unsolicited_cb_reg("+EUSIM", nwy_cli_pull_out_sim);
    nwy_sdk_at_unsolicited_cb_reg("+STKPCI",nwy_cli_stk_cb);\
    //nwy_sdk_at_unsolicited_cb_reg("+UPDATETIME",nwy_cli_tmupd_cb);
    /* added by wangchen for N58 sms api to test 20200215 begin */
//    nwy_sdk_at_unsolicited_cb_reg("+CMT", nwy_cli_recv_sms);
    /* added by wangchen for N58 sms api to test 20200215 end */
//    nwy_sdk_at_unsolicited_cb_reg("Connect AcceptSocket=", nwy_cli_client_acpt_cb);
//    nwy_sdk_at_unsolicited_cb_reg("+TCPRECV(S): ", nwy_cli_tcprecvs_cb);
//    nwy_sdk_at_unsolicited_cb_reg("+CLOSECLIENT: ", nwy_cli_acpt_close_cb);
//    nwy_sdk_at_unsolicited_cb_reg("+TCPSETUP: ", nwy_cli_tcpsetup_cb);
//    nwy_sdk_at_unsolicited_cb_reg("+TCPRECV: ", nwy_cli_tcprecv_cb);
//    nwy_sdk_at_unsolicited_cb_reg("+TCPCLOSE: ", nwy_cli_tcp_close_cb);
//    nwy_sdk_at_unsolicited_cb_reg("+UDPRECV: ", nwy_cli_udprecv_cb);
//    nwy_sdk_at_unsolicited_cb_reg("GPRS DISCONNECTION", nwy_cli_gprs_disconnect_cb);
//    nwy_sdk_at_unsolicited_cb_reg("+CMGL: ", nwy_cli_sms_list_resp_cb);
}
#endif

void nwy_exit_thread_self()
{
    return;
}


int nwy_test_cli_check_uart_mode(uint8_t uart_mode)
{
    if(uart_mode == NWY_UART_MODE_AT)
      return 1;

    return 0;
}

void nwy_pdp_set_status(nwy_pdp_status_type status)
{
    return;
}

/*
int nwy_test_cli_check_data_connect()
{
    return 1;
}
*/
