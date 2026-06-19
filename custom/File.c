#include "File.h"
#include "Geofence.h"

VTSTypedef VTSData = {{0}};
VTSStateTypedef VTSState = {0};





uint8_t SaveToFlash(char *filename, void *data, u32 size)
{
    
    int fd = Ql_FS_Open(filename, QL_FS_CREATE_ALWAYS);
    if (fd < 0)
    {
        LOGData(TAG_FILE, "File %s create failed, ret : %d, Formatting UFS", filename, fd);
        int ret = Ql_FS_Format(Ql_FS_UFS);
        if (ret != QL_RET_OK)
        {
            LOGData(TAG_FILE, "Format UFS failed, ret : %d", ret);
            return 0;
        }
        LOGData(TAG_FILE, "UFS formatted successfully, retrying file creation...");
        fd = Ql_FS_Open(filename, QL_FS_CREATE_ALWAYS);
        if(fd<0)
        {
            LOGData(TAG_FILE, "File %s create failed again, ret : %d", filename, fd);
            return 0;
        }
    }

    u32 bytesWritten = 0;
    s32 ret = Ql_FS_Write(fd, data, size, &bytesWritten);
    Ql_FS_Close(fd);

    if (ret != QL_RET_OK || bytesWritten != size)
    {
        LOGData(TAG_FILE, "Write error/mismatch for %s: ret=%d, written=%d/%d\n", filename, ret, bytesWritten, size);
        return 0;
    }

    LOGData(TAG_FILE, "File saved: %s (%d bytes)\n", filename, bytesWritten);
    return 1;
}

uint8_t LoadFromFlash(char *filename, void *data, u32 size, void (*defaultFunc)(void))
{
    if (Ql_FS_Check(filename)!=QL_RET_OK)
    {
        LOGData(TAG_FILE, "File not found: %s. Loading default.\n", filename);
        if (defaultFunc) defaultFunc();
        return 0;
    }

    s32 fileSize = Ql_FS_GetSize(filename);
    if (fileSize != size)
    {
        LOGData(TAG_FILE, "Invalid file size for %s. Expected: %d, Found: %d\n", filename, size, fileSize);
        if (defaultFunc) defaultFunc();
        return 0;
    }

    int fd = Ql_FS_Open(filename, QL_FS_READ_ONLY);
    if (fd < 0)
    {
        LOGData(TAG_FILE, "Cannot open file: %s. Loading default.\n", filename);
        if (defaultFunc) defaultFunc();
        return 0;
    }

    u32 bytesRead = 0;
    s32 ret = Ql_FS_Read(fd, data, size, &bytesRead);
    Ql_FS_Close(fd);

    if (ret != QL_RET_OK || bytesRead != size)
    {
        LOGData(TAG_FILE, "Read error/mismatch for %s. ret=%d, read=%d/%d\n", filename, ret, bytesRead, size);
        if (defaultFunc) defaultFunc();
        return 0;
    }

    LOGData(TAG_FILE, "File loaded: %s (%d bytes)\n", filename, bytesRead);
    return 1;
}


void UpdateStateInFlash(void)
{
    SaveToFlash(STATE_FILE_PATH, &VTSState, sizeof(VTSStateTypedef));
}

void UpdateConfigInFlash(void)
{
    SaveToFlash(CONFIG_FILE_PATH, &VTSData, sizeof(VTSTypedef));
}

void LoadDefaultState(void)
{
    VTSState.OdoCount = 0;
    VTSState.DefVal = DEFSTATE;
    VTSState.CurrentProfile = NONE;
    VTSState.IsPrevMain = 1;
    UpdateStateInFlash();
}

void LoadDefault(void)
{
    VTSData.DefID = DEFVAL;
	strcpy(VTSData.VendorID,DEFAULT_VENDOR);
	VTSData.BattThrs=LOW_BAT_THRS_VOLT;
    #ifndef PROTO_CDAC
	VTSData.IntervalData.DataInterval=300;
	VTSData.IntervalData.HealthInterval=250;
	VTSData.IntervalData.IgnitionInterval=10;
	VTSData.IntervalData.SOSInterval=5;
	//VTSData.IntervalData.SOSTimeOut=120;
    VTSData.IntervalData.SOSTimeOut=DEFAULT_INV_STM;
	VTSData.IntervalData.StandbyInterval=60;
    #else
	VTSData.IntervalData.HealthInterval=DEFAULT_INV_HEALTH;
	VTSData.IntervalData.MotionInterval=DEFAULT_INV_MOTION;
	VTSData.IntervalData.HaltInterval=DEFAULT_INV_HALT;
	VTSData.IntervalData.EnergencyInterval=DEFAULT_INV_CRIT;
	VTSData.IntervalData.SOSTimeOut=DEFAULT_INV_STM;
	VTSData.IntervalData.SleepInterval=DEFAULT_INV_SLEEP;
    VTSData.IntervalData.FullDataPacketInterval=DEFAULT_INV_FULL;
	VTSData.IntervalData.HaltTime=DEFAULT_HALT_TIME;
	VTSData.IntervalData.SleepTime=DEFAULT_SLEEP_TIME;
    #endif
	LOGData(TAG_FILE,"IP1:%s",DEFAULT_IP1);
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
    strcpy(VTSData.ServerData.IP4,DEFAULT_IP4);
    strcpy(VTSData.ServerData.Port4,DEFAULT_PORT4);   // 18110
    VTSData.ServerData.IPConfig[3]=1;
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
    VTSData.DefProfile=3;
    #warning SIM MAKE TECHNOJACKS SELECTED
    #elif defined SIMMAKE_APM
    VTSData.SIMMake = TAISYS;
    VTSData.DefProfile=1;
    #warning SIM MAKE APM SELECTED
    #elif defined SIMMAKE_IDEMIA_3P
    VTSData.SIMMake = TAISYS;
    VTSData.DefProfile=3;
    #warning SIM MAKE IDEMIA_3P SELECTED
    #elif defined SIMMAKE_SENS
    VTSData.SIMMake = SENSORISE;
    VTSData.DefProfile = 1;
    #warning SIM MAKE SENSORISE SELECTED
    #elif defined SIMMAKE_GND
    VTSData.SIMMake = GnD;
    VTSData.DefProfile = 2;
    #warning SIM MAKE GND SELECTED
    #elif defined SIMMAKE_COLORPLAST
    VTSData.SIMMake = COLORPLAST;
    VTSData.DefProfile = 1;
    #warning SIM MAKE COLORPLAST SELECTED
    #else
    #error NO SIM MAKE DEFINED
    #endif

    VTSData.AutoAPN=1;
    Ql_sprintf(VTSData.mAPN,"AIRTELIOT.COM");
    ClearGeofence();
    VTSData.DisableSOS = 0;
    UpdateConfigInFlash();
    InitSockets();
}

void LoadState(void)
{
    LoadFromFlash(STATE_FILE_PATH, &VTSState, sizeof(VTSStateTypedef), LoadDefaultState);
}

void LoadConfig(void)
{
    if (!LoadFromFlash(CONFIG_FILE_PATH, &VTSData, sizeof(VTSTypedef), LoadDefault))
        return;

    if (VTSData.DefID != DEFVAL)
    {
        LOGData(TAG_FILE, "CONFIG file Def Mismatch! Loading Default...");
        LoadDefault();
        return;
    }

    InitGeoState();
    InitSockets();
}


