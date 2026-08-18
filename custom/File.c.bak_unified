#include "File.h"
#include "Diag.h"
#include "Geofence.h"

VTSTypedef VTSData = {{0}};
VTSStateTypedef VTSState = {0};

#define ACTIVE_PROFILE_MAGIC  0xAC710001UL
#define ACTIVE_PROFILE_MIN    1
#define ACTIVE_PROFILE_MAX    4

typedef struct
{
    u32 Magic;
    u8  Profile;
    u8  Reason;
    /* u8  Reserved[2]; */
    u16 ProfileTimeoutSec;
    u32 UptimeSec;
} ActiveProfileTypedef;

static ActiveProfileTypedef ActiveProfileState = {0};

/* =======================================================================
 * DIAGNOSTIC LOGGING SYSTEM — Global Instance
 * Added : 2026-05-13
 *
 * DiagCounters is the single RAM instance of the diagnostic structure.
 * It is loaded from Diag.bin at boot by LoadDiag() and written back
 * to flash by SaveDiagToFlash() / PushDiagEvent().
 *
 * This is declared extern in VTS.h so any module can call PushDiagEvent()
 * or read DiagCounters fields directly for logging.
 * ======================================================================= */
DiagCountersTypedef DiagCounters = {0};

static bool s_flashRecoveryInProgress = FALSE;

static uint8_t IsFatalFsError(s32 ret)
{
    return (ret == QL_RET_ERR_FS_FATAL_ERR1 || ret == QL_RET_ERR_FS_FATAL_ERR2) ? 1 : 0;
}

static uint8_t IsFlashWritePowerUnsafe(const char *filename)
{
#if SYSTEM_FLASH_WRITE_POWER_GUARD_ENABLE
    double battThreshold = (VTSData.BattThrs > 3.0F) ? VTSData.BattThrs : LOW_BAT_THRS_VOLT;

    if (PeriPheralVal.BattVolt > 0.1F &&
        PeriPheralVal.BattVolt < battThreshold &&
        PeriPheralVal.MainsVolt < 6.0F)
    {
        LOGData(TAG_FILE,
                "Flash write skipped: low power file=%s batt_mv=%lu thrs_mv=%lu mains_mv=%lu",
                filename ? filename : "?",
                (u32)(PeriPheralVal.BattVolt * 1000.0),
                (u32)(battThreshold * 1000.0),
                (u32)(PeriPheralVal.MainsVolt * 1000.0));
        return 1;
    }
#else
    (void)filename;
#endif
    return 0;
}

static void LogModemFlashCorruption(const char *filename,
                                    const char *operation,
                                    s32 ret,
                                    s32 expected,
                                    s32 actual)
{
    LOGData(TAG_FILE,
            "MODEM FLASH CORRUPTION: file=%s op=%s ret=%d expected=%d actual=%d",
            filename ? filename : "?",
            operation ? operation : "?",
            ret,
            expected,
            actual);

    if (!s_flashRecoveryInProgress)
    {
        s_flashRecoveryInProgress = TRUE;
        PushDiagEvent(DIAG_EVT_FLASH_ERR,
                      (uint8_t)(ret & 0xFF),
                      (uint8_t)(actual & 0xFF),
                      filename ? filename : "flash");
        s_flashRecoveryInProgress = FALSE;
    }
}

static uint8_t FullEraseUfsAfterFlashCorruption(const char *reason, s32 ret)
{
#if SYSTEM_FLASH_AUTO_FORMAT_ENABLE
    s32 fmtRet;

    if (s_flashRecoveryInProgress)
    {
        return 0;
    }

    s_flashRecoveryInProgress = TRUE;
    LOGData(TAG_FILE,
            "MODEM FLASH CORRUPTION: starting full UFS erase/format, reason=%s ret=%d",
            reason ? reason : "?",
            ret);

    /* OLD CODE: only Ql_FS_Format(Ql_FS_UFS) was called after create-open
     * failure, with no modem-flash-corruption log or diagnostic event. */
    fmtRet = Ql_FS_Format(Ql_FS_UFS);
    if (fmtRet != QL_RET_OK)
    {
        LOGData(TAG_FILE,
                "MODEM FLASH CORRUPTION: full UFS erase/format failed, ret=%d",
                fmtRet);
        s_flashRecoveryInProgress = FALSE;
        return 0;
    }

    LOGData(TAG_FILE, "MODEM FLASH CORRUPTION: full UFS erase/format completed");
    s_flashRecoveryInProgress = FALSE;
    return 1;
#else
    LOGData(TAG_FILE,
            "MODEM FLASH CORRUPTION: automatic UFS format blocked, reason=%s ret=%d",
            reason ? reason : "?",
            ret);
    return 0;
#endif
}





uint8_t SaveToFlash(char *filename, void *data, u32 size)
{
    int fd;

    if (IsFlashWritePowerUnsafe(filename))
    {
        return 0;
    }

    // Check if file already exists with identical content to prevent unnecessary SPI flash wear
    fd = Ql_FS_Open(filename, QL_FS_READ_ONLY);
    if(fd >= 0)
    {
        u32 fileSize = Ql_FS_GetSize(filename);
        if(fileSize == size && size <= 2048)
        {
            uint8_t tempBuf[512];
            uint8_t* checkPtr = (size <= 512) ? tempBuf : (uint8_t*)Ql_MEM_Alloc(size);
            if(checkPtr)
            {
                u32 readBytes = 0;
                s32 rRes = Ql_FS_Read(fd, checkPtr, size, &readBytes);
                Ql_FS_Close(fd);
                if(rRes == QL_RET_OK && readBytes == size)
                {
                    if(Ql_memcmp(checkPtr, data, size) == 0)
                    {
                        LOGData(TAG_FILE, "Flash write skipped (unchanged): %s (%lu bytes)", filename, size);
                        if(size > 512 && checkPtr != tempBuf) Ql_MEM_Free(checkPtr);
                        return 1;
                    }
                }
                if(size > 512 && checkPtr != tempBuf) Ql_MEM_Free(checkPtr);
            }
            else
            {
                Ql_FS_Close(fd);
            }
        }
        else
        {
            Ql_FS_Close(fd);
        }
    }

    fd = Ql_FS_Open(filename, QL_FS_CREATE_ALWAYS);
    if (fd < 0 || IsFatalFsError(fd))
    {
        LOGData(TAG_FILE, "File %s create failed, ret : %d", filename, fd);
        LogModemFlashCorruption(filename, "create-open", fd, (s32)size, 0);
        if (!FullEraseUfsAfterFlashCorruption("create-open", fd))
        {
            return 0;
        }
        LOGData(TAG_FILE, "UFS formatted by enabled recovery policy, retrying file creation...");
        fd = Ql_FS_Open(filename, QL_FS_CREATE_ALWAYS);
        if(fd < 0 || IsFatalFsError(fd))
        {
            LOGData(TAG_FILE, "File %s create failed again, ret : %d", filename, fd);
            LogModemFlashCorruption(filename, "create-open-retry", fd, (s32)size, 0);
            return 0;
        }
    }

    u32 bytesWritten = 0;
    s32 ret = Ql_FS_Write(fd, data, size, &bytesWritten);
    Ql_FS_Close(fd);

    if (ret != QL_RET_OK || bytesWritten != size)
    {
        LOGData(TAG_FILE, "Write error/mismatch for %s: ret=%d, written=%d/%d\n", filename, ret, bytesWritten, size);
        LogModemFlashCorruption(filename, "write", ret, (s32)size, (s32)bytesWritten);
        if (IsFatalFsError(ret))
        {
            FullEraseUfsAfterFlashCorruption("write", ret);
        }
        return 0;
    }

    LOGData(TAG_FILE, "File saved: %s (%d bytes)\n", filename, bytesWritten);
    return 1;
}

uint8_t LoadFromFlash(char *filename, void *data, u32 size, void (*defaultFunc)(void))
{
    s32 checkRet = Ql_FS_Check(filename);
    if (checkRet != QL_RET_OK)
    {
        LOGData(TAG_FILE, "File not found: %s. Loading default.\n", filename);
        if (IsFatalFsError(checkRet))
        {
            LogModemFlashCorruption(filename, "check", checkRet, (s32)size, 0);
            FullEraseUfsAfterFlashCorruption("check", checkRet);
        }
        if (defaultFunc) defaultFunc();
        return 0;
    }

    s32 fileSize = Ql_FS_GetSize(filename);
    if (fileSize != size)
    {
        LOGData(TAG_FILE, "Invalid file size for %s. Expected: %d, Found: %d\n", filename, size, fileSize);
        LogModemFlashCorruption(filename, "size", 0, (s32)size, fileSize);
        if (IsFatalFsError(fileSize))
        {
            FullEraseUfsAfterFlashCorruption("size", fileSize);
        }
        if (defaultFunc) defaultFunc();
        return 0;
    }

    int fd = Ql_FS_Open(filename, QL_FS_READ_ONLY);
    if (fd < 0 || IsFatalFsError(fd))
    {
        LOGData(TAG_FILE, "Cannot open file: %s. Loading default.\n", filename);
        LogModemFlashCorruption(filename, "read-open", fd, (s32)size, 0);
        if (IsFatalFsError(fd))
        {
            FullEraseUfsAfterFlashCorruption("read-open", fd);
        }
        if (defaultFunc) defaultFunc();
        return 0;
    }

    u32 bytesRead = 0;
    s32 ret = Ql_FS_Read(fd, data, size, &bytesRead);
    Ql_FS_Close(fd);

    if (ret != QL_RET_OK || bytesRead != size)
    {
        LOGData(TAG_FILE, "Read error/mismatch for %s. ret=%d, read=%d/%d\n", filename, ret, bytesRead, size);
        LogModemFlashCorruption(filename, "read", ret, (s32)size, (s32)bytesRead);
        if (IsFatalFsError(ret))
        {
            FullEraseUfsAfterFlashCorruption("read", ret);
        }
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

static uint8_t IsValidActiveProfile(uint8_t profile)
{
    return (profile >= ACTIVE_PROFILE_MIN && profile <= ACTIVE_PROFILE_MAX) ? 1 : 0;
}

static uint8_t IsValidActiveProfileReason(uint8_t reason)
{
    /* return (reason == ACTIVE_PROFILE_REASON_GPRS ||
            reason == ACTIVE_PROFILE_REASON_SERVER) ? 1 : 0; */
    return (reason == ACTIVE_PROFILE_REASON_NONE ||
            reason == ACTIVE_PROFILE_REASON_GPRS ||
            reason == ACTIVE_PROFILE_REASON_SERVER) ? 1 : 0;
}

void LoadDefaultActiveProfile(void)
{
    Ql_memset(&ActiveProfileState, 0, sizeof(ActiveProfileState));
    ActiveProfileState.Magic = ACTIVE_PROFILE_MAGIC;
    SaveToFlash(ACTIVE_PROFILE_FILE_PATH,
                &ActiveProfileState,
                sizeof(ActiveProfileTypedef));
}

void LoadActiveProfile(void)
{
    if (!LoadFromFlash(ACTIVE_PROFILE_FILE_PATH,
                       &ActiveProfileState,
                       sizeof(ActiveProfileTypedef),
                       LoadDefaultActiveProfile))
    {
        LOGData(TAG_FILE, "ActiveProfile.bin missing; last active SIM profile unset");
        return;
    }

    if (ActiveProfileState.Magic != ACTIVE_PROFILE_MAGIC ||
        (ActiveProfileState.Profile != 0 &&
         !IsValidActiveProfile(ActiveProfileState.Profile)) ||
        (ActiveProfileState.Profile != 0 &&
         !IsValidActiveProfileReason(ActiveProfileState.Reason)))
    {
        LOGData(TAG_FILE,
                "ActiveProfile.bin invalid: magic=0x%08lX profile=%d reason=%d; resetting",
                ActiveProfileState.Magic,
                ActiveProfileState.Profile,
                ActiveProfileState.Reason);
        LoadDefaultActiveProfile();
        return;
    }

    /* LOGData(TAG_FILE,
            "ActiveProfile.bin loaded: profile=%d reason=%d uptime=%lu",
            ActiveProfileState.Profile,
            ActiveProfileState.Reason,
            ActiveProfileState.UptimeSec); */
    LOGData(TAG_FILE,
            "ActiveProfile.bin loaded: profile=%d reason=%d uptime=%lu timeout=%u",
            ActiveProfileState.Profile,
            ActiveProfileState.Reason,
            ActiveProfileState.UptimeSec,
            ActiveProfileState.ProfileTimeoutSec);

    // Synchronize boot profile to last active profile before comparing
    if (ActiveProfileState.Profile != 0 && IsValidActiveProfile(ActiveProfileState.Profile))
    {
        if (VTSState.CurrentProfile != ActiveProfileState.Profile)
        {
            LOGData(TAG_BOOT, "Sync boot profile to last active profile: %d -> %d",
                    VTSState.CurrentProfile, ActiveProfileState.Profile);
            VTSState.CurrentProfile = ActiveProfileState.Profile;
        }
    }

    // Restore profile connection timeout if the loaded active profile matches the current profile
    if (VTSState.CurrentProfile == ActiveProfileState.Profile)
    {
        uint32_t switch_timeout = PRF_TIMEOUT;
        #ifdef SIM_PROFILE_AIRTEL
        if (VTSState.CurrentProfile == SIM_PROFILE_AIRTEL) switch_timeout = 30 * 60;
        #endif
        #ifdef SIM_PROFILE_VI
        if (VTSState.CurrentProfile == SIM_PROFILE_VI) switch_timeout = 5 * 60;
        #endif
        #ifdef SIM_PROFILE_BSNL
        if (VTSState.CurrentProfile == SIM_PROFILE_BSNL) switch_timeout = 2 * 60;
        #endif

        if (ActiveProfileState.ProfileTimeoutSec < switch_timeout)
        {
            IntervalTick.ProfileChangeCount = ActiveProfileState.ProfileTimeoutSec;
            LOGData(TAG_BOOT, "Restored profile connection timeout: %d s for profile %d",
                    IntervalTick.ProfileChangeCount, VTSState.CurrentProfile);
        }
        else
        {
            IntervalTick.ProfileChangeCount = 0;
            LOGData(TAG_BOOT, "Loaded timeout (%d) >= switch_timeout (%d); reset to 0",
                    ActiveProfileState.ProfileTimeoutSec, switch_timeout);
        }
    }
    else
    {
        IntervalTick.ProfileChangeCount = 0;
        LOGData(TAG_BOOT, "Profile mismatch (current=%d, active=%d); profile timeout cleared",
                VTSState.CurrentProfile, ActiveProfileState.Profile);
    }
}

uint8_t HasLastActiveProfile(void)
{
    return (ActiveProfileState.Magic == ACTIVE_PROFILE_MAGIC &&
            IsValidActiveProfile(ActiveProfileState.Profile) &&
            IsValidActiveProfileReason(ActiveProfileState.Reason)) ? 1 : 0;
}

uint8_t GetLastActiveProfile(void)
{
    /* return HasLastActiveProfile() ? ActiveProfileState.Profile : 0; */
    if (HasLastActiveProfile() && ActiveProfileState.Reason != ACTIVE_PROFILE_REASON_NONE)
    {
        return ActiveProfileState.Profile;
    }
    return 0;
}

uint8_t MarkActiveProfile(uint8_t profile, uint8_t reason)
{
    if (!IsValidActiveProfile(profile) || !IsValidActiveProfileReason(reason))
    {
        LOGData(TAG_FILE,
                "ActiveProfile ignored invalid mark: profile=%d reason=%d",
                profile,
                reason);
        return 0;
    }

    if (HasLastActiveProfile() && ActiveProfileState.Profile == profile)
    {
        if (ActiveProfileState.Reason == ACTIVE_PROFILE_REASON_SERVER ||
            ActiveProfileState.Reason == reason)
        {
            return 1;
        }
    }

    Ql_memset(&ActiveProfileState, 0, sizeof(ActiveProfileState));
    ActiveProfileState.Magic = ACTIVE_PROFILE_MAGIC;
    ActiveProfileState.Profile = profile;
    ActiveProfileState.Reason = reason;
    ActiveProfileState.UptimeSec = (u32)(Ql_GetMsSincePwrOn() / 1000ULL);

    if (!SaveToFlash(ACTIVE_PROFILE_FILE_PATH,
                     &ActiveProfileState,
                     sizeof(ActiveProfileTypedef)))
    {
        LOGData(TAG_FILE,
                "ActiveProfile save failed: profile=%d reason=%d",
                profile,
                reason);
        return 0;
    }

    LOGData(TAG_FILE,
            "ActiveProfile saved: profile=%d reason=%d uptime=%lu",
            ActiveProfileState.Profile,
            ActiveProfileState.Reason,
            ActiveProfileState.UptimeSec);
    return 1;
}

uint16_t GetActiveProfileTimeout(void)
{
    return ActiveProfileState.ProfileTimeoutSec;
}

void SetActiveProfileTimeout(uint16_t timeoutSec)
{
    if (ActiveProfileState.Profile != VTSState.CurrentProfile)
    {
        ActiveProfileState.Profile = VTSState.CurrentProfile;
        ActiveProfileState.Reason = ACTIVE_PROFILE_REASON_NONE;
    }
    ActiveProfileState.ProfileTimeoutSec = timeoutSec;
}

void ResetActiveProfileTimeout(void)
{
    if (ActiveProfileState.ProfileTimeoutSec != 0)
    {
        ActiveProfileState.ProfileTimeoutSec = 0;
        SaveActiveProfileState();
    }
}

uint8_t SaveActiveProfileState(void)
{
    if (!SaveToFlash(ACTIVE_PROFILE_FILE_PATH,
                     &ActiveProfileState,
                     sizeof(ActiveProfileTypedef)))
    {
        LOGData(TAG_FILE,
                "ActiveProfile state save failed: profile=%d timeout=%d",
                ActiveProfileState.Profile,
                ActiveProfileState.ProfileTimeoutSec);
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------
 * FIX #7 | LoadDefaultState - Explicit ProfileFailCount Zero Init
 * FILE   : custom/File.c
 * FUNCTION: LoadDefaultState()
 * DATE   : 2026-05-13
 * ------------------------------------------------------------------
 * ISSUE FIXED:
 *   On a fresh flash or corrupted State.bin, ProfileFailCount[] could
 *   contain garbage values from uninitialized flash memory. If any
 *   profile's count was >= 3 (MAX_CONSECUTIVE_FAILS), that profile
 *   would be permanently blacklisted from first boot with no way to
 *   recover without a full firmware reflash.
 *
 * ROOT CAUSE:
 *   The original LoadDefaultState() did not zero-initialize the
 *   ProfileFailCount[5] array. The VTSStateTypedef struct was defined
 *   with = {0} at file scope, but when LoadDefaultState() is called
 *   mid-session (e.g., after format recovery), the struct already
 *   exists in RAM with potentially dirty values from a prior run.
 *
 * SOLUTION:
 *   Explicitly zero all 5 elements of ProfileFailCount[] before
 *   calling UpdateStateInFlash(), so the saved State.bin is always
 *   clean when defaults are applied.
 *
 * REVERT:
 *   Remove the for() loop (4 lines), keep everything else. The old
 *   code is shown commented below.
 * ------------------------------------------------------------------
 * OLD CODE:
 *   void LoadDefaultState(void)
 *   {
 *       VTSState.OdoCount       = 0;
 *       VTSState.DefVal         = DEFSTATE;
 *       VTSState.CurrentProfile = NONE;
 *       VTSState.RegDeniedCount = 0;
 *       VTSState.IsPrevMain     = 1;
 *       UpdateStateInFlash();   // <-- ProfileFailCount[] NOT zeroed!
 *   }
 * ------------------------------------------------------------------ */
void LoadDefaultState(void)
{
    uint8_t i;
    VTSState.OdoCount       = 0;
    VTSState.DefVal         = DEFSTATE;
    VTSState.CurrentProfile = NONE;
    VTSState.RegDeniedCount = 0;
    VTSState.IsPrevMain     = 1;
    /* FIX #7 - zero every slot so no stale flash garbage remains */
    for (i = 0; i < 5; i++)
        VTSState.ProfileFailCount[i] = 0;
    UpdateStateInFlash();
}

/*void LoadDefaultState(void)
{
    uint8_t i;
    VTSState.OdoCount       = 0;
    VTSState.DefVal         = DEFSTATE;
    VTSState.CurrentProfile = NONE;
    VTSState.RegDeniedCount = 0;
    VTSState.IsPrevMain     = 1;
    
    // Force-blacklist all profiles to simulate uninitialized flash garbage
    for (i = 0; i < 5; i++)
        VTSState.ProfileFailCount[i] = 3; 
        
    UpdateStateInFlash();
}
*/

void LoadDefault(void)
{
    VTSData.DefID = DEFVAL;
	strcpy(VTSData.VendorID,DEFAULT_VENDOR);
	VTSData.BattThrs=LOW_BAT_THRS_VOLT;
#ifdef ENABLE_UNIFIED_FIRMWARE
    VTSData.ActiveProtocol = VTS_PROTO_NIC;
    VTSData.ActiveState = VTS_STATE_ODISHA_DEFAULT;
    VTSData.FeatureFlags = (1u << VTS_FEATURE_BLE) | (1u << VTS_FEATURE_AUTOPRF) | (1u << VTS_FEATURE_GPSREC) | (1u << VTS_FEATURE_SOS);
    GetDefaultIPsAndTags();
#else
    #ifndef PROTO_CDAC
	VTSData.IntervalData.DataInterval=300;
	VTSData.IntervalData.HealthInterval=250;
	VTSData.IntervalData.IgnitionInterval=10;
	VTSData.IntervalData.SOSInterval=5;
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
    VTSData.DefProfile = SIM_PROFILE_AIRTEL;
    #warning SIM MAKE TECHNOJACKS SELECTED
    #elif defined SIMMAKE_APM
    VTSData.SIMMake = TAISYS;
    VTSData.DefProfile = SIM_PROFILE_AIRTEL;
    #warning SIM MAKE APM SELECTED
    #elif defined SIMMAKE_IDEMIA_3P
    VTSData.SIMMake = TAISYS;
    VTSData.DefProfile = SIM_PROFILE_AIRTEL;
    #warning SIM MAKE IDEMIA_3P SELECTED
    #elif defined SIMMAKE_SENS
    VTSData.SIMMake = SENSORISE;
    VTSData.DefProfile = SIM_PROFILE_AIRTEL;
    #warning SIM MAKE SENSORISE SELECTED
    #elif defined SIMMAKE_GND
    VTSData.SIMMake = GnD;
    VTSData.DefProfile = SIM_PROFILE_AIRTEL;
    #warning SIM MAKE GND SELECTED
    #elif defined SIMMAKE_COLORPLAST
    VTSData.SIMMake = COLORPLAST;
    VTSData.DefProfile = SIM_PROFILE_AIRTEL;
    #warning SIM MAKE COLORPLAST SELECTED
    #else
    #error NO SIM MAKE DEFINED
    #endif

    VTSData.AutoAPN=1;
    Ql_sprintf(VTSData.mAPN,"AIRTELIOT.COM");
    VTSData.DisableSOS=0;
    VTSData.DisableHistory=0;
    VTSData.EnableHTTPS=0; // Default: plain HTTP
    VTSData.DisableGPSFaultReset=0;
    ClearGeofence();
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


/* =======================================================================
 * DIAGNOSTIC LOGGING SYSTEM — Implementation
 * Added : 2026-05-13
 * =======================================================================
 *
 * Overview:
 *   Three functions manage the Diag.bin flash file:
 *     LoadDefaultDiag()  — zeros all counters, writes fresh Diag.bin
 *     LoadDiag()         — loads Diag.bin at boot; falls back to default
 *     SaveDiagToFlash()  — writes current DiagCounters RAM image to flash
 *
 *   One function adds events to the ring buffer:
 *     PushDiagEvent()    — appends event, saves to flash immediately
 *
 * Flash layout of Diag.bin:
 *   Offset 0: DiagCountersTypedef (sizeof ~1480 bytes for DIAG_RING_SIZE=50)
 *   Magic field (first 4 bytes) must equal DIAG_MAGIC or file is rejected.
 *
 * Ring buffer behaviour:
 *   RingHead always points to the NEXT slot to write.
 *   RingCount tracks how many valid entries exist (caps at DIAG_RING_SIZE).
 *   When full, oldest entry is silently overwritten (true ring buffer).
 *
 * Typical call flow:
 *   Boot:   LoadDiag() → DiagCounters.BootCount++ → PushDiagEvent(BOOT)
 *   Event:  PushDiagEvent(DIAG_EVT_PRF_SWITCH, from, to, "auto-timeout")
 *   Log dump: DiagCounters fields are printed by LogDiagCounters() at boot
 * ======================================================================= */

/* -----------------------------------------------------------------------
 * LoadDefaultDiag
 *
 * Called when Diag.bin is missing, wrong size, or has an invalid Magic.
 * Zeros all fields, sets Magic, and writes a fresh Diag.bin to flash.
 * Does NOT touch State.bin or UFSConfig.bin.
 * ----------------------------------------------------------------------- */
void LoadDefaultDiag(void)
{
    uint8_t i;
    LOGData(TAG_DIAG, "Loading diagnostic defaults (Diag.bin missing or corrupted)");

    Ql_memset(&DiagCounters, 0, sizeof(DiagCountersTypedef));
    DiagCounters.Magic = DIAG_MAGIC;

    /* Zero the entire ring buffer explicitly */
    for (i = 0; i < DIAG_RING_SIZE; i++)
    {
        Ql_memset(&DiagCounters.Ring[i], 0, sizeof(DiagEventEntry));
    }

    DiagCounters.RingHead  = 0;
    DiagCounters.RingCount = 0;
    SaveDiagToFlash();
}

/* -----------------------------------------------------------------------
 * SaveDiagToFlash
 *
 * Writes the current DiagCounters RAM image to Diag.bin.
 * Called by PushDiagEvent() after every new event, and by LoadDefaultDiag().
 * On flash write failure the function logs the error but does NOT crash —
 * the RAM counters remain valid for the current session.
 * ----------------------------------------------------------------------- */
void SaveDiagToFlash(void)
{
    DiagCounters.Magic = DIAG_MAGIC;   /* always ensure magic is valid before write */
    if (!SaveToFlash(DIAG_FILE_PATH, &DiagCounters, sizeof(DiagCountersTypedef)))
    {
        LOGData(TAG_DIAG, "WARNING: Failed to save Diag.bin — counters NOT persisted this cycle");
        /* Push a flash-error marker without saving (avoid infinite recursion) */
    }
}

/* -----------------------------------------------------------------------
 * LoadDiag
 *
 * Loads Diag.bin from flash into DiagCounters.
 * Validates the Magic field. On any error calls LoadDefaultDiag().
 * Call once at boot, AFTER LoadConfig() and LoadState().
 * ----------------------------------------------------------------------- */
void LoadDiag(void)
{
    uint8_t i;

    if (!LoadFromFlash(DIAG_FILE_PATH, &DiagCounters, sizeof(DiagCountersTypedef), LoadDefaultDiag))
    {
        LOGData(TAG_DIAG, "Diag.bin not found — defaults initialised");
        return;
    }

    if (DiagCounters.Magic != DIAG_MAGIC)
    {
        LOGData(TAG_DIAG, "Diag.bin magic mismatch (got 0x%08lX, expected 0x%08lX) — resetting",
                DiagCounters.Magic, DIAG_MAGIC);
        LOGData(TAG_FILE,
                "MODEM FLASH CORRUPTION: file=%s op=magic ret=0 expected=0x%08lX actual=0x%08lX",
                DIAG_FILE_PATH,
                DIAG_MAGIC,
                DiagCounters.Magic);
        LoadDefaultDiag();
        PushDiagEvent(DIAG_EVT_FLASH_ERR, 0, 0, "Diag.bin");
        return;
    }

    /* Sanity-check ring pointers to guard against partial corruption */
    if (DiagCounters.RingHead >= DIAG_RING_SIZE)
    {
        LOGData(TAG_DIAG, "Diag.bin RingHead out of range (%d) — resetting ring only",
                DiagCounters.RingHead);
        LOGData(TAG_FILE,
                "MODEM FLASH CORRUPTION: file=%s op=ring-head ret=0 expected=%d actual=%d",
                DIAG_FILE_PATH,
                DIAG_RING_SIZE,
                DiagCounters.RingHead);
        DiagCounters.RingHead  = 0;
        DiagCounters.RingCount = 0;
        SaveDiagToFlash();
        PushDiagEvent(DIAG_EVT_FLASH_ERR, 0, 0, "Diag.bin");
    }

    LOGData(TAG_DIAG, "Diag.bin loaded: boots=%lu, wdt_resets=%lu, prf_switches=%lu, gsm_hangs=%lu",
            DiagCounters.BootCount,
            DiagCounters.WatchdogResetCount,
            DiagCounters.ProfileSwitchCount,
            DiagCounters.GsmHangCount);
    LOGData(TAG_DIAG, "              reg_denied=%lu, csq99=%lu, gprs_fails=%lu, gsm_resets=%lu",
            DiagCounters.RegDeniedTotal,
            DiagCounters.Csq99Count,
            DiagCounters.GprsFailCount,
            DiagCounters.GsmResetCount);
    LOGData(TAG_DIAG, "              last_boot_reason=%d, last_profile=%d, ring_entries=%d/%d",
            DiagCounters.LastBootReason,
            DiagCounters.LastProfile,
            DiagCounters.RingCount,
            DIAG_RING_SIZE);

    /* Print the last 10 events from the ring for quick triage at boot */
    LOGData(TAG_DIAG, "--- Last events (newest first) ---");
    {
        uint8_t count = (DiagCounters.RingCount < 10) ? DiagCounters.RingCount : 10;
        for (i = 0; i < count; i++)
        {
            uint8_t idx = (DiagCounters.RingHead + DIAG_RING_SIZE - 1 - i) % DIAG_RING_SIZE;
            DiagEventEntry *e = &DiagCounters.Ring[idx];
            LOGData(TAG_DIAG, "  EVT[%d] uptime=%lu type=%d p1=%d p2=%d detail=[%s]",
                    idx, e->UptimeSec, e->EventType, e->Param1, e->Param2, e->Detail);
        }
    }
    LOGData(TAG_DIAG, "----------------------------------");
}

/* -----------------------------------------------------------------------
 * PushDiagEvent
 *
 * Appends one event to DiagCounters.Ring[] and saves to flash.
 *
 * Parameters:
 *   evtType — DiagEventType code (see VTS.h enum)
 *   p1      — event-specific parameter 1 (meaning depends on evtType)
 *   p2      — event-specific parameter 2
 *   detail  — short human-readable string (max 19 chars + null)
 *
 * Parameter meanings by event type:
 *   DIAG_EVT_BOOT        p1=boot_reason p2=0      detail="PWRKEY"/"WDT"/"ABN"/etc
 *   DIAG_EVT_PRF_SWITCH  p1=from_profile p2=to_profile  detail="reg-denied"/"timeout"/etc
 *   DIAG_EVT_PRF_FAIL    p1=profile      p2=fail_count  detail="IMSI-same"/"STK-fail"/etc
 *   DIAG_EVT_GSM_HANG    p1=csq          p2=0           detail="no-signal"/"AT-timeout"
 *   DIAG_EVT_GSM_RESET   p1=reset_count  p2=0           detail="CFUN=1,1"
 *   DIAG_EVT_WDT_RESET   p1=boot_reason  p2=0           detail="watchdog"
 *   DIAG_EVT_GPRS_FAIL   p1=profile      p2=fail_count  detail="APN-reject"/"timeout"
 *   DIAG_EVT_REG_DENIED  p1=profile      p2=csq         detail="CS=3"
 *   DIAG_EVT_STATE_CHG   p1=old_state    p2=new_state   detail="reason"
 *   DIAG_EVT_FLASH_ERR   p1=0            p2=0           detail="Diag.bin"/"State.bin"
 *   DIAG_EVT_PRF_TIMEOUT p1=profile      p2=0           detail="12-min-timer"
 *   DIAG_EVT_SYS_RESET   p1=0            p2=0           detail="reset reason"
 * ----------------------------------------------------------------------- */
void PushDiagEvent(uint8_t evtType, uint8_t p1, uint8_t p2, const char *detail)
{
    DiagEventEntry *e;

    if (evtType >= DIAG_EVT_MAX)
        return;

    e = &DiagCounters.Ring[DiagCounters.RingHead];

    e->UptimeSec = (uint32_t)(Ql_GetMsSincePwrOn() / 1000ULL);
    e->EventType = evtType;
    e->Param1    = p1;
    e->Param2    = p2;
    e->Reserved  = 0;
    Ql_memset(e->Detail, 0, sizeof(e->Detail));
    if (detail != NULL)
        Ql_strncpy(e->Detail, detail, sizeof(e->Detail) - 1);

    /* Advance ring head (wrap around) */
    DiagCounters.RingHead = (uint8_t)((DiagCounters.RingHead + 1) % DIAG_RING_SIZE);
    if (DiagCounters.RingCount < DIAG_RING_SIZE)
        DiagCounters.RingCount++;

    LOGData(TAG_DIAG, "EVENT type=%d(%s) p1=%d p2=%d detail=[%s] uptime=%lu",
            evtType,
            (evtType == DIAG_EVT_BOOT)        ? "BOOT"        :
            (evtType == DIAG_EVT_PRF_SWITCH)  ? "PRF_SWITCH"  :
            (evtType == DIAG_EVT_PRF_FAIL)    ? "PRF_FAIL"    :
            (evtType == DIAG_EVT_GSM_HANG)    ? "GSM_HANG"    :
            (evtType == DIAG_EVT_GSM_RESET)   ? "GSM_RESET"   :
            (evtType == DIAG_EVT_WDT_RESET)   ? "WDT_RESET"   :
            (evtType == DIAG_EVT_GPRS_FAIL)   ? "GPRS_FAIL"   :
            (evtType == DIAG_EVT_REG_DENIED)  ? "REG_DENIED"  :
            (evtType == DIAG_EVT_STATE_CHG)   ? "STATE_CHG"   :
            (evtType == DIAG_EVT_FLASH_ERR)   ? "FLASH_ERR"   :
            (evtType == DIAG_EVT_PRF_TIMEOUT) ? "PRF_TIMEOUT" :
            (evtType == DIAG_EVT_SYS_RESET)   ? "SYS_RESET"   : "UNKNOWN",
            p1, p2,
            detail ? detail : "",
            e->UptimeSec);

    SaveDiagToFlash();
}

/* =======================================================================
 * Diag.h interface implementations
 * Added : 2026-05-13
 *
 * These functions are declared in Diag.h (no includes, primitive types)
 * and implemented here where the full DiagCounters struct is in scope.
 * They exist so SystemRecovery.c and Systic.c can interact with the
 * diagnostic system without including File.h (which causes circular
 * includes and bool-type redefinition conflicts in those modules).
 * ======================================================================= */

/* -----------------------------------------------------------------------
 * Diag_OnBoot
 *
 * Centralises all boot-time counter updates so SystemRecovery.c never
 * needs to access DiagCounters fields directly.
 *
 * The caller (SystemRecovery_LogBootReason) already checks QL_RESET_WDT
 * and passes the result as isWdtReset so this function needs no ql_power.h.
 * ----------------------------------------------------------------------- */
void Diag_OnBoot(int reason, unsigned char profile, int isWdtReset, const char *reasonName)
{
    DiagCounters.BootCount++;
    DiagCounters.LastBootReason = (uint8_t)reason;
    DiagCounters.LastProfile    = profile;

    if (isWdtReset)
    {
        DiagCounters.WatchdogResetCount++;
        LOGData(TAG_WDT, "!!! WATCHDOG RESET DETECTED — total WDT resets: %lu !!!",
                DiagCounters.WatchdogResetCount);
        PushDiagEvent(DIAG_EVT_WDT_RESET, (uint8_t)reason, profile, "watchdog");
    }
    else
    {
        PushDiagEvent(DIAG_EVT_BOOT, (uint8_t)reason, profile,
                      reasonName ? reasonName : "boot");
    }
}

/* -----------------------------------------------------------------------
 * Diag_GetWdtResetCount / Diag_GetBootCount
 * Safe counter reads for modules that cannot include File.h.
 * ----------------------------------------------------------------------- */
unsigned long Diag_GetWdtResetCount(void)
{
    return (unsigned long)DiagCounters.WatchdogResetCount;
}

unsigned long Diag_GetBootCount(void)
{
    return (unsigned long)DiagCounters.BootCount;
}

#ifdef ENABLE_UNIFIED_FIRMWARE
void GetDefaultIPsAndTags(void)
{
    // Initialize intervals based on active protocol
    VTSData.IntervalData.DataInterval = 300;
    VTSData.IntervalData.HealthInterval = 250;
    VTSData.IntervalData.IgnitionInterval = 10;
    VTSData.IntervalData.SOSInterval = 5;
    VTSData.IntervalData.SOSTimeOut = DEFAULT_INV_STM;
    VTSData.IntervalData.StandbyInterval = 60;

    // Set default IP and Port depending on Protocol and State
    const char *default_ip = "vltspvt.delhi.gov.in";
    const char *default_port = "9031";

    if (IS_PROTO_NIC()) {
        default_ip = "vltspvt.delhi.gov.in";
        default_port = "9031";
    } else if (IS_PROTO_MH()) {
        default_ip = "data.vahanshakti.in";
        default_port = "4030";
    } else if (IS_PROTO_ODISHA()) {
        if (VTSData.ActiveState == VTS_STATE_ODISHA_LADAKH) {
            default_ip = "pvt.vltdladakh.in";
            default_port = "60002";
        } else {
            default_ip = "pvtdevices.odishatransport.gov.in";
            default_port = "8205";
        }
    }

    strcpy(VTSData.ServerData.IP1, default_ip);
    strcpy(VTSData.ServerData.Port1, default_port);
    VTSData.ServerData.IPConfig[0] = 1;

    // Set second server IP/Port to match primary for standard protocols
    strcpy(VTSData.ServerData.IP2, default_ip);
    strcpy(VTSData.ServerData.Port2, default_port);
    VTSData.ServerData.IPConfig[1] = 1;
}
#endif
