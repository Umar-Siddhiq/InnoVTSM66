
#include "GPRS.h"
#include "Hardware.h"
#include "Systic.h"
#include "GPS.h"
#include "File.h"
#include "SMS.h"
#include "ril_custom.h"
/* ------------------------------------------------------------------
 * FIX #9 | Added SystemRecovery.h include
 * FILE   : custom/GPRS.c
 * DATE   : 2026-05-13
 * ------------------------------------------------------------------
 * ISSUE FIXED:
 *   HandleGSMHang(), IsGSMUnresponsive() and ResetGSMModule() were
 *   defined in this file but GPRSThreadEntry() never called them.
 *   To wire in a full device reset as the final recovery step after
 *   GSM hang detection, SystemRecovery_RequestReset() is needed.
 *   Without this include the symbol would be undefined.
 *
 * OLD CODE: (this include did not exist)
 * ------------------------------------------------------------------ */
#include "SystemRecovery.h"
#include "custom_feature_def.h"

extern uint8_t IsMotaProcessing;
extern uint8_t IsFotaProcessing;

GSM_Typedef GSM = {0};
NET_Typedef NetWork = {{0}};
uint8_t updntp = 0;
_RTC CurrentDateTime = {0};
uint8_t IsQNITZSet=0;
Providertypedef prfReq=0;
 uint8_t PrfChanged;
#define STK_ENB

STKDatatypedef STKdata = {0};

#if SYSTEM_WDTTEST_COMMAND_ENABLE
/* TEST ONLY: runtime recovery fault injection.
 * Compiled out in production because SYSTEM_RECOVERY_ENABLE/WATCHDOG are
 * disabled in this build and these commands intentionally create faults. */
static uint8_t s_testForceGsmHang = 0;
static uint8_t s_testBadProfileSwitch = 0;

void GPRS_ArmGsmHangTest(void)
{
    s_testForceGsmHang = 1;
    GSM.GSMState = GPRS_INIT;
    GSM.SignalStrength = 99;
    LOGData(TAG_GPRS, "TEST GSMHANG armed: forcing CSQ=99 and GPRS_INIT failures");
}

void GPRS_TriggerBadProfileSwitchTest(void)
{
    uint8_t badProfile = STKdata.profiles_supported + 1;

    if (badProfile <= STKdata.profiles_supported)
    {
        badProfile = 4;
    }
    if (badProfile < 4)
    {
        badProfile = 4;
    }

    s_testBadProfileSwitch = 1;
    if (GSM.GSMState < SIM_DETECTED)
    {
        GSM.GSMState = SIM_DETECTED;
    }
    prfReq = (Providertypedef)badProfile;
    LOGData(TAG_PROFILE,
            "TEST BADPRF armed: forcing invalid profile request %d (supported=%d)",
            badProfile,
            STKdata.profiles_supported);
}

void GPRS_ClearRecoveryTests(void)
{
    if (s_testBadProfileSwitch)
    {
        prfReq = NONE;
    }
    s_testForceGsmHang = 0;
    s_testBadProfileSwitch = 0;
    LOGData(TAG_GPRS, "TEST recovery fault injection cleared");
}

void GPRS_GetRecoveryTestStatus(char *out, u32 outLen)
{
    if (out == NULL || outLen == 0)
    {
        return;
    }

    Ql_memset(out, 0, outLen);
    Ql_sprintf(out,
               "GPRSTEST recovery=%d gsmhang=%d badprf=%d prfReq=%d gsm=%d csq=%d",
               (int)SYSTEM_RECOVERY_ENABLE,
               s_testForceGsmHang,
               s_testBadProfileSwitch,
               (uint8_t)prfReq,
               GSM.GSMState,
               GSM.SignalStrength);
}
#endif

void LoadsensoriseSTKData(STKDatatypedef *stk)
{
    if (!stk) return;  // Add null check
    
    stk->setup_cmd_count = STK_SENS_SETUP_COUNT;
    Ql_strcpy(stk->Setup[0].data, STK_SENS_MENU);
    stk->Setup[0].type = 0;
    Ql_strcpy(stk->Setup[1].data, STK_SENS_ITEM);
    stk->Setup[1].type = 1;
    Ql_strcpy(stk->Setup[2].data, STK_SENS_NETWORK);
    stk->Setup[2].type = 0;
    stk->profiles_supported = 2;
    Ql_strcpy(stk->Profile[0].data, STK_SENS_PRIMARY);
    Ql_strcpy(stk->Profile[1].data, STK_SENS_SECONDARY);
    stk->is_valid = 1;
}

void LoadTaisysSTKData(STKDatatypedef *stk)
{
    if (!stk) return;  // Add null check
    
    stk->setup_cmd_count = STK_TAISYS_SETUP_COUNT;
    Ql_strcpy(stk->Setup[0].data, STK_TAISYS_MENU);
    stk->Setup[0].type = 0;
    Ql_strcpy(stk->Setup[1].data, STK_TAISYS_ITEM);
    stk->Setup[1].type = 1;
    
    #ifdef SIMMAKE_TACHNOJACKS
    stk->profiles_supported = 3;
    #elif defined SIMMAKE_IDEMIA_3P
    stk->profiles_supported = 3;
    #else
    stk->profiles_supported = 2;
    #endif
    
    Ql_strcpy(stk->Profile[0].data, STK_TAISYS_PRIMARY);
    Ql_strcpy(stk->Profile[1].data, STK_TAISYS_SECONDARY);
    #if defined(SIMMAKE_TACHNOJACKS) || defined(SIMMAKE_IDEMIA_3P)
    Ql_strcpy(stk->Profile[2].data, STK_TAISYS_THIRD);
    #endif
    stk->is_valid = 1;
}

void LoadTaisysGNDData(STKDatatypedef *stk)
{
    if (!stk) return;  // Add null check
    
    stk->setup_cmd_count = STK_GND_SETUP_COUNT;
    Ql_strcpy(stk->Setup[0].data, STK_GND_MENU);
    stk->Setup[0].type = 0;
    Ql_strcpy(stk->Setup[1].data, STK_GND_ITEM);
    stk->Setup[1].type = 1;
    Ql_strcpy(stk->Setup[2].data, STK_GND_NETWORK);
    stk->Setup[2].type = 0;
    stk->profiles_supported = 3;
    Ql_strcpy(stk->Profile[0].data, STK_GND_PRIMARY);
    Ql_strcpy(stk->Profile[1].data, STK_GND_SECONDARY);
    Ql_strcpy(stk->Profile[2].data, STK_GND_THIRD);
    stk->is_valid = 1;
}

void LoadColorplastSTKData(STKDatatypedef *stk)
{
    if (!stk) return;  // Add null check
    
    stk->setup_cmd_count = STK_COLORPLAST_SETUP_COUNT;
    Ql_strcpy(stk->Setup[0].data, STK_COLORPLAST_MENU);
    stk->Setup[0].type = 0;  // Terminal Response
    Ql_strcpy(stk->Setup[1].data, STK_COLORPLAST_ITEM);
    stk->Setup[1].type = 1;  // Envelope Command
    stk->profiles_supported = STK_COLORPLAST_PROFILE_COUNT;  // 3 profiles: AIRTEL, BSNL, VIL
    Ql_strcpy(stk->Profile[0].data, STK_COLORPLAST_PRIMARY);    // AIRTEL
    Ql_strcpy(stk->Profile[1].data, STK_COLORPLAST_SECONDARY);  // BSNL
    Ql_strcpy(stk->Profile[2].data, STK_COLORPLAST_THIRD);      // VIL
    stk->is_valid = 1;
}


#ifdef STK_ENB
bool EnableSTK(void)
{
    bool stk_enabled = false;
    s32 ret;

    if(VTSData.SIMMake == SENSORISE)
        LoadsensoriseSTKData(&STKdata);
    else if(VTSData.SIMMake == TAISYS)
        LoadTaisysSTKData(&STKdata);
    else if(VTSData.SIMMake == GnD)
        LoadTaisysGNDData(&STKdata);
    #ifdef SIMMAKE_COLORPLAST
    else if(VTSData.SIMMake == COLORPLAST)
        LoadColorplastSTKData(&STKdata);
    #endif
    else
    {
        LOGData(TAG_GPRS, "Invalid SIM Make");
        return false;
    }

    // Check STK status
    ret = RIL_QSTKGet(&stk_enabled);
    if (ret != RIL_AT_SUCCESS)
    {
        LOGData(TAG_GPRS, "Failed to get STK status");
        return false;
    }

    if (stk_enabled)
    {
        LOGData(TAG_GPRS, "STK already enabled");
        return true;
    }

    // Enable STK
    ret = RIL_QSTKSet(true);
    if (ret != RIL_AT_SUCCESS)
    {
        LOGData(TAG_GPRS, "Failed to enable STK");
        return false;
    }

    LOGData(TAG_GPRS, "STK enabled successfully");
    return true;
}

/* ------------------------------------------------------------------
 * FIX #2a | MAX_CONSECUTIVE_FAILS - Profile Blacklist Threshold
 * FILE   : custom/GPRS.c
 * DATE   : 2026-05-13
 * ------------------------------------------------------------------
 * ISSUE FIXED:
 *   ProfileFailCount[] was incremented but never had a clear threshold
 *   documented. GetNextValidProfile() uses this to skip bad profiles.
 *   Value of 3 means a profile must fail 3 consecutive STK switch
 *   attempts OR 3 consecutive GPRS activation cycles before it is
 *   skipped. Kept at 3 (original intent).
 *
 * OLD CODE: (same value, no define existed in some older builds)
 * ------------------------------------------------------------------ */
#define MAX_CONSECUTIVE_FAILS   3

/* ------------------------------------------------------------------
 * FIX #9b | MAX_GPRS_INIT_FAILS - GPRS Hang / Profile Switch Trigger
 * FILE   : custom/GPRS.c
 * DATE   : 2026-05-13
 * ------------------------------------------------------------------
 * ISSUE FIXED:
 *   If a profile's GPRS never activates (network/APN rejection or GSM
 *   module hang), the device would wait the full 12-minute PRF_TIMEOUT
 *   timer before switching profiles. This made "stuck at Profile 1"
 *   conditions last 12+ minutes per cycle.
 *
 * SOLUTION:
 *   After 5 consecutive ActivateGPRS() failures (~5 minutes), check
 *   whether the GSM module is hung or if the network is simply
 *   rejecting this profile's APN. Act immediately instead of waiting.
 *
 * OLD CODE: (this define did not exist - no GPRS failure counter existed)
 * ------------------------------------------------------------------ */
#define MAX_GPRS_INIT_FAILS     5

/* =======================================================================
 * DIAGNOSTIC LOGGING SYSTEM — GPRS Module Helpers
 * Added : 2026-05-13
 * =======================================================================
 *
 * Two helpers are added here to provide rich diagnostic logging across
 * the entire GPRS/Profile state machine without modifying every individual
 * function signature:
 *
 *   LogGSMStateChange()  — logs every GSM state transition with a reason
 *                          string and pushes it to the Diag ring buffer.
 *                          Call it BEFORE assigning GSM.GSMState.
 *
 *   LogProfileDiag()     — logs a full profile status snapshot including
 *                          current/next profile, all fail counts, CSQ, and
 *                          network registration state. Call at key profile
 *                          events (before switch, after switch, after boot).
 *
 * HOW TO READ STATE LOGS:
 *   Search logs for "STATE:" prefix. Lines look like:
 *     STATE: CHANGE SIM_DETECTED(1)->GPRS_INIT(2) reason=[registered]
 *   If STATE: CHANGE GPRS_INIT(2)->SIM_DETECTED(1) appears repeatedly
 *   it means the device registers on the network but GPRS keeps dropping.
 *
 * HOW TO READ PROFILE LOGS:
 *   Search logs for "PROFILE:" prefix. Lines look like:
 *     PROFILE: [before-switch] cur=1(VI) req=2(BSNL) fails=[0,3,0,0,0] CSQ=15 creg=1
 *   fail count of 3 for profile 2 (index 2) means BSNL is blacklisted.
 * ======================================================================= */

/* GSM state name lookup — matches GSM_Typedef.GSMState values in GPRS.h */
static const char* GetGSMStateName(uint8_t state)
{
    switch(state)
    {
        case 0:  return "SIM_NOT_DETECTED";
        case 1:  return "SIM_DETECTED";
        case 2:  return "GPRS_INIT";
        case 3:  return "GPRS_ACTIVE";
        default: return "UNKNOWN";
    }
}



/* -----------------------------------------------------------------------
 * LogGSMStateChange
 *
 * Logs a GSM state machine transition with a human-readable reason.
 * Call this function BEFORE changing GSM.GSMState so the log shows
 * the transition correctly (old → new with reason).
 *
 * Parameters:
 *   oldState — current GSM.GSMState value (before change)
 *   newState — new GSM.GSMState value (about to be assigned)
 *   reason   — short string explaining why the transition is happening
 *              e.g. "registered", "gprs-timeout", "no-sim", "reg-denied"
 *
 * HOW TO READ:
 *   STATE: CHANGE SIM_DETECTED(1)->GPRS_INIT(2) reason=[registered]
 *   ↑ device moved from waiting for registration to attempting GPRS attach
 *
 *   STATE: CHANGE GPRS_ACTIVE(3)->GPRS_INIT(2) reason=[connection-dropped]
 *   ↑ device lost data connection and is re-attempting GPRS activation
 * ----------------------------------------------------------------------- */
#if 0
/* Disabled 2026-05-25:
 * This helper is not called anywhere in the current state machine. Keeping it
 * compiled only adds dead code and an unused-function warning; retain the body
 * here as documentation for a future state-transition cleanup. */
static void LogGSMStateChange(uint8_t oldState, uint8_t newState, const char *reason)
{
    if (oldState == newState)
        return;   /* no transition — do not log */

    LOGData(TAG_STATE, "CHANGE %s(%d)->%s(%d) reason=[%s]",
            GetGSMStateName(oldState), oldState,
            GetGSMStateName(newState), newState,
            reason ? reason : "?");

    PushDiagEvent(DIAG_EVT_STATE_CHG, oldState, newState, reason ? reason : "?");
}
#endif

/* -----------------------------------------------------------------------
 * LogProfileDiag
 *
 * Logs a full snapshot of the profile state at a given event point.
 * Includes: current profile, requested profile, all fail counts,
 * CSQ signal strength, and CREG/CGREG registration state.
 *
 * Parameters:
 *   event — label for log context e.g. "before-switch", "after-switch",
 *            "boot", "gprs-fail", "reg-denied", "auto-timeout"
 *
 * HOW TO READ:
 *   PROFILE: [before-switch] cur=1(VI) req=2(BSNL) fails=[0,0,3,0,0] CSQ=15 creg=1 cgatt=0
 *   ↑ cur = current profile (VI), req = requested next (BSNL)
 *   ↑ fails[2]=3 means BSNL has 3 consecutive failures = blacklisted!
 *   ↑ CSQ=15 = good signal, creg=1 = registered home, cgatt=0 = no GPRS attach
 * ----------------------------------------------------------------------- */
static void LogProfileDiag(const char *event)
{
    s32  creg = -1, cgatt = -1;
    u32  csq = 0, ber = 0;

    RIL_NW_GetGSMState(&creg);
    RIL_NW_GetGPRSState(&cgatt);
    RIL_NW_GetSignalQuality(&csq, &ber);

    LOGData(TAG_PROFILE,
            "[%s] cur=%d(%s) req=%d(%s) fails=[%d,%d,%d,%d,%d] CSQ=%d creg=%d cgatt=%d",
            event,
            VTSState.CurrentProfile, GetProfileName(VTSState.CurrentProfile),
            (uint8_t)prfReq,         GetProfileName((uint8_t)prfReq),
            VTSState.ProfileFailCount[0],
            VTSState.ProfileFailCount[1],
            VTSState.ProfileFailCount[2],
            VTSState.ProfileFailCount[3],
            VTSState.ProfileFailCount[4],
            (uint8_t)csq,
            (int)creg,
            (int)cgatt);
}

static uint8_t GetBootRestoreActiveProfile(void)
{
    uint8_t lastActiveProfile = GetLastActiveProfile();

    if (lastActiveProfile == NONE)
    {
        return NONE;
    }

    if (!STKdata.is_valid || lastActiveProfile > STKdata.profiles_supported)
    {
        LOGData(TAG_PROFILE,
                "Last active profile %d ignored at boot: STK valid=%d supported=%d",
                lastActiveProfile,
                STKdata.is_valid,
                STKdata.profiles_supported);
        return NONE;
    }

    if (VTSState.ProfileFailCount[lastActiveProfile] >= MAX_CONSECUTIVE_FAILS)
    {
        LOGData(TAG_PROFILE,
                "Last active profile %d ignored at boot: blacklisted fail count=%d/%d",
                lastActiveProfile,
                VTSState.ProfileFailCount[lastActiveProfile],
                MAX_CONSECUTIVE_FAILS);
        return NONE;
    }

    return lastActiveProfile;
}

/******************************************************************************
* Function: GetNextValidProfile
* 
* Description: Intelligently selects next profile based on failure history.
*              Skips profiles with consecutive failures >= MAX_CONSECUTIVE_FAILS.
*              Resets all counters if all profiles are exhausted.
* 
* Parameters:
*   currentProfile - Current profile number (1-based)
* 
* Returns: Next valid profile number (1-based), or 1 after full reset
******************************************************************************/
uint8_t GetNextValidProfile(uint8_t currentProfile)
{
    uint8_t nextProfile;
    uint8_t attemptedProfiles = 0;
    uint8_t i;
    
    // Validate STK data
    if (!STKdata.is_valid || STKdata.profiles_supported == 0)
    {
        LOGData(TAG_GPRS, "STK data not valid, defaulting to profile 1");
        return 1;
    }
    
    // Try to find a profile with failures < MAX_CONSECUTIVE_FAILS
    nextProfile = (currentProfile % STKdata.profiles_supported) + 1;  // Wrap around
    
    while (attemptedProfiles < STKdata.profiles_supported)
    {
        if (VTSState.ProfileFailCount[nextProfile] < MAX_CONSECUTIVE_FAILS)
        {
            LOGData(TAG_GPRS, "Selected profile %d (fail count: %d)", 
                    nextProfile, VTSState.ProfileFailCount[nextProfile]);
            return nextProfile;
        }
        
        attemptedProfiles++;
        nextProfile = (nextProfile % STKdata.profiles_supported) + 1;  // Try next
    }
    
    /* ------------------------------------------------------------------
     * FIX #2 | GetNextValidProfile() - Exhaustion Restart Logic
     * DATE   : 2026-05-13
     * ------------------------------------------------------------------
     * ISSUE FIXED:
     *   When ALL profiles had fail counts >= MAX_CONSECUTIVE_FAILS the
     *   function reset all counts and always returned profile 1. This
     *   caused devices to permanently loop 1→1→1 or 1→3→1→3 because
     *   after every full reset the cycle always restarted from profile 1,
     *   never giving profile 2 a fair chance.
     *
     * ROOT CAUSE:
     *   "return 1" hardcoded the restart point. If the device was on
     *   profile 3 when exhaustion hit, it would jump back to 1 and
     *   skip 2 again in the next round.
     *
     * SOLUTION:
     *   After resetting all fail counts, continue round-robin from the
     *   profile AFTER the current one, not always from profile 1. This
     *   ensures every profile is tried in order after a full reset.
     *
     * REVERT:
     *   Replace the return statement below with: return 1;
     * ------------------------------------------------------------------
     * OLD CODE: return 1;
     * ------------------------------------------------------------------ */
    LOGData(TAG_GPRS, "All profiles exhausted, resetting failure counts");
    /* BENCH TEST: Disable exhaustion reset to force permanent blacklist lockup */
    for (i = 1; i <= STKdata.profiles_supported; i++)
    {
        VTSState.ProfileFailCount[i] = 0;
    }
    UpdateStateInFlash();
    

    /* FIX #2 - continue round-robin from next after current, not from 1 */
    return (currentProfile % STKdata.profiles_supported) + 1;
}

uint8_t SwitchProfile(uint8_t num)
{
    char imsi[30] = {0};
    s32 ret;
    // LED Manager will automatically show init state
    if(!STKdata.is_valid)
    {
        LOGData(TAG_GPRS, "STK Data not valid!");
        return false;
    }
    
    // Validate profile number (1-based indexing)
    if(num == 0 || num > STKdata.profiles_supported)
    {
        LOGData(TAG_GPRS, "Profile Num %d not supported, valid range: 1-%d", num, STKdata.profiles_supported);
        return false;
    }
    #ifdef PRE_SWITCH_COMMANDS
    // For non-GnD SIMs
    if(VTSData.SIMMake != GnD)
    {
        // Send CRSM command
        ret = RIL_SIM_SendCommand(28539, 0, 0, 12, "FFFFFFFFFFFFFFFFFFFFFFFF", NULL);
        if (ret != RIL_AT_SUCCESS)
        {
            LOGData(TAG_GPRS, "CRSM command failed");
            return false;
        }
        ThreadSleep(1500);

        // Send initial STK terminal response
        ret = RIL_QSTKTerminalResponse("8103010300820281828301008402011E");
        if (ret != RIL_AT_SUCCESS)
        {
            LOGData(TAG_GPRS, "Initial STK TR failed");
            return false;
        }
        ThreadSleep(1500);
    }
    else
    {
        // GnD specific CSIM commands
        const char* csim_commands[] = {
            "00A4000C023F00",
            "00A4080C0444445542",
            "00DC020428534D534332FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFFFFFFFFFF0791194924909979FFFFFFFFFFFFFF"
        };
        
        for(int i = 0; i < 3; i++)
        {
            ret = RIL_SIM_SendCSIMCommand(Ql_strlen(csim_commands[i]), csim_commands[i], NULL);
            if (ret != RIL_AT_SUCCESS)
            {
                LOGData(TAG_GPRS, "CSIM command %d failed", i);
                return false;
            }
            ThreadSleep(1500);
        }
    }
    #endif
    // Send setup commands
    for(int i = 0; i < STKdata.setup_cmd_count; i++)
    {
        if(STKdata.Setup[i].type == 1)
        {
            /* LOG CLEANUP 2026-05-16: dropped two asterisk banner lines per command.
             * OLD: LOGData(TAG_GPRS, "*************************"); (above & below) */
            LOGData(TAG_GPRS, "STK Setup ENV [%d]: %s", i, STKdata.Setup[i].data);
            ret = RIL_QSTKEnvelopeCommand(STKdata.Setup[i].data);
        }
        else
        {
            /* OLD: same asterisk banners around the TR log */
            LOGData(TAG_GPRS, "STK Setup TR  [%d]: %s", i, STKdata.Setup[i].data);
            ret = RIL_QSTKTerminalResponse(STKdata.Setup[i].data);
        }
        
        /* ------------------------------------------------------------------
         * FIX #4 | SwitchProfile() - STK Setup Command Failure Tracking
         * DATE   : 2026-05-13
         * ------------------------------------------------------------------
         * ISSUE FIXED:
         *   When an STK setup command failed, the function returned false
         *   without recording the failure in ProfileFailCount[]. The next
         *   call to GetNextValidProfile() had no knowledge that this profile
         *   had just failed its STK setup, so it kept retrying the same
         *   broken profile indefinitely.
         *
         * ROOT CAUSE:
         *   No fail count increment on STK AT command failure. The fail
         *   count was only incremented on IMSI-unchanged check, not earlier.
         *
         * SOLUTION:
         *   Before returning false, increment ProfileFailCount[num] and
         *   persist it to flash so GetNextValidProfile() can skip this
         *   profile once it reaches MAX_CONSECUTIVE_FAILS.
         *
         * REVERT:
         *   Remove the if() block inside, keep only the LOG and return false.
         * ------------------------------------------------------------------
         * OLD CODE:
         *   if (ret != RIL_AT_SUCCESS)
         *   {
         *       LOGData(TAG_GPRS, "Setup command %d failed", i);
         *       return false;
         *   }
         * ------------------------------------------------------------------ */
        if (ret != RIL_AT_SUCCESS)
        {
            LOGData(TAG_GPRS, "Setup command %d failed", i);
            /* FIX #4 - record failure so profile can be blacklisted */
            if (num >= 1 && num <= 4)
            {
                VTSState.ProfileFailCount[num]++;
                LOGData(TAG_GPRS, "Profile %d STK setup failed, fail count: %d", num, VTSState.ProfileFailCount[num]);
                UpdateStateInFlash();
            }
            return false;
        }
        else
            LOGData(TAG_GPRS, "Setup command Success");
        ThreadSleep(1500);
    }

    // Send profile command
    if(STKdata.Profile[num-1].type == 1)
    {
        /* LOG CLEANUP 2026-05-16: dropped asterisk banners.
         * OLD: LOGData(TAG_GPRS, "*************************"); (above & below) */
        LOGData(TAG_GPRS, "STK Profile ENV [%d]: %s", num, STKdata.Profile[num-1].data);
        ret = RIL_QSTKEnvelopeCommand(STKdata.Profile[num-1].data);
    }
    else
    {
        /* OLD: same asterisk banners around the TR log */
        LOGData(TAG_GPRS, "STK Profile TR  [%d]: %s", num, STKdata.Profile[num-1].data);
        ret = RIL_QSTKTerminalResponse(STKdata.Profile[num-1].data);
    }
    
    /* ------------------------------------------------------------------
     * FIX #4b | SwitchProfile() - STK Profile Command Failure Tracking
     * DATE   : 2026-05-13
     * ------------------------------------------------------------------
     * ISSUE FIXED:
     *   Same gap as Fix #4 but for the actual profile switch AT command.
     *   If the RIL_QSTKEnvelopeCommand / RIL_QSTKTerminalResponse call
     *   for the profile itself failed (e.g., SIM not responding), the
     *   failure was silently dropped with no ProfileFailCount update.
     *
     * SOLUTION:
     *   Increment ProfileFailCount[num] and save to flash before returning
     *   false, same pattern as Fix #4.
     *
     * REVERT:
     *   Remove the if() block inside, keep only LOG and return false.
     * ------------------------------------------------------------------
     * OLD CODE:
     *   if (ret != RIL_AT_SUCCESS)
     *   {
     *       LOGData(TAG_GPRS, "Profile command failed");
     *       return false;
     *   }
     * ------------------------------------------------------------------ */
    if (ret != RIL_AT_SUCCESS)
    {
        LOGData(TAG_GPRS, "Profile command failed");
        /* FIX #4b - record failure so profile can be blacklisted */
        if (num >= 1 && num <= 4)
        {
            VTSState.ProfileFailCount[num]++;
            LOGData(TAG_GPRS, "Profile %d STK profile cmd failed, fail count: %d", num, VTSState.ProfileFailCount[num]);
            UpdateStateInFlash();
        }
        return false;
    }
    else
        LOGData(TAG_GPRS, "Profile command sent, waiting for IMSI change");
    ThreadSleep(5000);

    // Verify IMSI change
    ret = RIL_SIM_GetIMSI(imsi);
    if (ret != RIL_AT_SUCCESS)
    {
        LOGData(TAG_GPRS, "Failed to get new IMSI");
        
        // Increment failure count - IMSI read failed
        if (num >= 1 && num <= 4)
        {
            VTSState.ProfileFailCount[num]++;
            LOGData(TAG_GPRS, "Profile %d switch failed (IMSI read error), fail count: %d", 
                    num, VTSState.ProfileFailCount[num]);
            UpdateStateInFlash();
        }
        return false;
    }

    /* ------------------------------------------------------------------
     * FIX #5 | SwitchProfile() - Remove First-Boot IMSI Bypass
     * DATE   : 2026-05-13
     * ------------------------------------------------------------------
     * ISSUE FIXED:
     *   On first boot (VTSState.CurrentProfile == NONE / 0), the old
     *   code had a special bypass: if IMSI was unchanged after the STK
     *   switch command it returned TRUE (success) instead of FALSE.
     *   This meant a non-existent profile (e.g., trying profile 2 on a
     *   2-profile SIM) would be silently marked as "switched OK" on the
     *   very first boot. The device would think profile 2 was active
     *   even though nothing had changed. On the next cycle, profile 2
     *   would show IMSI mismatch and eventually get blacklisted, but
     *   the first boot wasted a full 12-minute cycle.
     *
     * ROOT CAUSE:
     *   This bypass was originally added because on first boot
     *   NetWork.IMSI might not reflect the current profile correctly,
     *   but this assumption is wrong -- GetSimState() always reads the
     *   real IMSI from the modem before SwitchProfile() is called.
     *
     * SOLUTION:
     *   Remove the bypass entirely. IMSI unchanged after STK command is
     *   always a failure regardless of boot count. Increment fail count
     *   and return false so GetNextValidProfile() skips this profile.
     *
     * REVERT:
     *   Add back before this block:
     *     if(VTSState.CurrentProfile == 0) { return true; }
     *   And remove the fail count increment from the if() below.
     * ------------------------------------------------------------------
     * OLD CODE:
     *   if(VTSState.CurrentProfile == 0)   // <-- first-boot bypass
     *   {
     *       return true;  // Always "success" on first boot even if IMSI unchanged
     *   }
     *   if(Ql_strstr(imsi, NetWork.IMSI))
     *   {
     *       LOGData(TAG_GPRS, "!!!!!!!!! Unable to change profile (IMSI same)");
     *       return false;  // <-- no fail count recorded here
     *   }
     * ------------------------------------------------------------------ */
    if(Ql_strstr(imsi, NetWork.IMSI))
    {
        /* FIX #5 - IMSI unchanged is always a failure; no first-boot bypass */
        LOGData(TAG_GPRS, "!!!!!!!!!!!! Unable to change profile (IMSI same - profile likely doesn't exist)");
        LOGData(TAG_GPRS, "Prev IMSI: %s", NetWork.IMSI);
        LOGData(TAG_GPRS, "New IMSI: %s", imsi);

        if (num >= 1 && num <= 4)
        {
            VTSState.ProfileFailCount[num]++;
            LOGData(TAG_GPRS, "Profile %d switch failed (non-existent profile), fail count: %d",
                    num, VTSState.ProfileFailCount[num]);
            UpdateStateInFlash();
        }
        return false;
    }

    /* DIAG LOG: profile switch verification — confirmed by IMSI change */
    LOGData(TAG_GPRS, "Profile Successfully Changed");
    LOGData(TAG_GPRS, "Prev IMSI: %s", NetWork.IMSI);
    LOGData(TAG_GPRS, "New IMSI: %s", imsi);
    LOGData(TAG_PROFILE,
            "SWITCH VERIFIED: %d(%s)->%d(%s) IMSI_before=%s IMSI_after=%s",
            VTSState.CurrentProfile, GetProfileName(VTSState.CurrentProfile),
            num, GetProfileName(num),
            NetWork.IMSI, imsi);
    DiagCounters.ProfileSwitchCount++;
    PushDiagEvent(DIAG_EVT_PRF_SWITCH, VTSState.CurrentProfile, num, "imsi-changed");

    /* ------------------------------------------------------------------
     * FIX #3 | SwitchProfile() - Fail Count Decay on Successful Switch
     * DATE   : 2026-05-13
     * ------------------------------------------------------------------
     * ISSUE FIXED:
     *   PRIMARY ROOT CAUSE of "device stuck cycling only 2 profiles".
     *   Profile 2 (BSNL) failed 3+ times in a prior session and its
     *   ProfileFailCount[2] was saved to State.bin as >= 3. On every
     *   subsequent boot, GetNextValidProfile() skipped Profile 2 because
     *   its count was at the blacklist threshold. Profiles 1 and 3
     *   succeeded each cycle (IMSI changed), so their counts reset to 0
     *   each time. Profile 2 was never tried, so its count never decayed.
     *   It was permanently blacklisted with no recovery path.
     *
     * ROOT CAUSE:
     *   The old success block only reset the CURRENT profile's fail count
     *   to 0. Other profiles' fail counts were never reduced, so once any
     *   profile hit MAX_CONSECUTIVE_FAILS it stayed blacklisted forever
     *   unless the device received an SMS command to clear it (impossible
     *   on inactive/deployed devices).
     *
     * SOLUTION:
     *   After a successful profile switch, decay all OTHER profiles' fail
     *   counts by 1. After MAX_CONSECUTIVE_FAILS (3) successful cycles
     *   using profiles 1 and 3, Profile 2's count decays 3→2→1→0 and
     *   GetNextValidProfile() includes it in the rotation again.
     *   Example recovery sequence:
     *     Cycle 1: Profile 1 success → Profile 2 decays 3→2
     *     Cycle 2: Profile 3 success → Profile 2 decays 2→1
     *     Cycle 3: Profile 1 success → Profile 2 decays 1→0  ← eligible again
     *
     * REVERT:
     *   Remove the for() loop (5 lines). Keep only:
     *     VTSState.ProfileFailCount[num] = 0;
     *     UpdateStateInFlash();
     * ------------------------------------------------------------------
     * OLD CODE:
     *   if (num >= 1 && num <= 4)
     *   {
     *       VTSState.ProfileFailCount[num] = 0;   // only reset current
     *       UpdateStateInFlash();                 // others never decay
     *   }
     * ------------------------------------------------------------------ */
    if (num >= 1 && num <= 4)
    {
        uint8_t j;
        VTSState.ProfileFailCount[num] = 0;
        LOGData(TAG_GPRS, "Profile %d switch successful, resetting fail count", num);
        /* FIX #3 - decay other profiles so blacklisted ones can recover */
        for (j = 1; j <= STKdata.profiles_supported; j++)
        {
            if (j != num && VTSState.ProfileFailCount[j] > 0)
            {
                VTSState.ProfileFailCount[j]--;
                LOGData(TAG_GPRS, "Profile %d fail count decayed to %d", j, VTSState.ProfileFailCount[j]);
            }
        }
        UpdateStateInFlash();
    }
    
    // Update NetWork.IMSI with new IMSI
    Ql_strncpy(NetWork.IMSI, imsi, sizeof(NetWork.IMSI) - 1);
    NetWork.IMSI[sizeof(NetWork.IMSI) - 1] = '\0';
    
    // Update NetWork.SIMNo with new CCID/ICCID
    char new_ccid[64] = {0};
    ret = RIL_SIM_GetCCID(new_ccid);
    if (ret == RIL_AT_SUCCESS)
    {
        #ifdef VIRTUAL_SIMCCID
        Ql_strcpy(new_ccid, VCID);
        #endif
        
        Ql_strncpy(NetWork.SIMNo, new_ccid, sizeof(NetWork.SIMNo) - 1);
        NetWork.SIMNo[sizeof(NetWork.SIMNo) - 1] = '\0';
        LOGData(TAG_GPRS, "SwitchProfile updated SIMNo: %s", NetWork.SIMNo);
    }
    else
    {
        LOGData(TAG_GPRS, "Failed to get new CCID after profile switch");
    }
    
    return true;
}
#endif

uint8_t GetSimState(void)
{
    s32  card_status = 0;
    char siminfo[64] = {0};
	char sim_id[64] = {0};
    Enum_ATSndError ret = RIL_AT_FAILED;

    if(RIL_SIM_GetSimState(&card_status)==RIL_AT_SUCCESS)
    {
        if(card_status == SIM_STAT_NOT_INSERTED || card_status== SIM_STAT_NOT_READY)
        {
            LOGData(TAG_GPRS,"NO SIM\r\n");
            GSM.GSMState = SIM_NOT_DETECTED;
            return 0;
        }

        ret = RIL_SIM_GetIMSI(siminfo);
        if(ret==RIL_AT_SUCCESS)
        {
            #ifdef VIRTUAL_IMSI
            Ql_strcpy(siminfo,VIMSI);
            #endif
            Ql_strcpy(NetWork.IMSI,siminfo);
            LOGData(TAG_GPRS,"IMEI: %s,IMSI: %s\r\n",NetWork.IMEI,NetWork.IMSI);
           
            GSM.GSMState = SIM_DETECTED;
        }
        ret = RIL_SIM_GetCCID(sim_id);
        if(ret==RIL_AT_SUCCESS)
        {
            #ifdef VIRTUAL_SIMCCID
            Ql_strcpy(sim_id,VCID);
            #endif
            
            Ql_strncpy(NetWork.SIMNo, sim_id, sizeof(NetWork.SIMNo) - 1);
            NetWork.SIMNo[sizeof(NetWork.SIMNo) - 1] = '\0';
            LOGData(TAG_GPRS,"SIM No: %s\r\n",NetWork.SIMNo);
            GSM.GSMState = SIM_DETECTED;
            // LED Manager will automatically update based on GSM state
            if(!IsQNITZSet){
                if(SetupAutoTimesync()==RIL_AT_SUCCESS)
                {
                    IsQNITZSet=1;
                    LOGData(TAG_GPRS,"Auto Time Sync Set\r\n");
                }
                else
                {
                    IsQNITZSet=0;
                    LOGData(TAG_GPRS,"Auto Time Sync Failed\r\n");
                }
            }
        }
        return 1;
    }    
    else
    {
        GSM.GSMState = SIM_NOT_DETECTED;
        // LED Manager will automatically update based on GSM state
        return 0;
    }
    
}


uint8_t GetRegisterStat(void)
{
    s32 ret = 0;
    s32 nw_stat = 0;
    ret = RIL_NW_GetGPRSState(&nw_stat);
    if(nw_stat==NW_STAT_REGISTERED || nw_stat==NW_STAT_REGISTERED_ROAMING)
        ret = 1;

    return ret;
}

void GetSignalStrength(void)
{
    s32 ret;
    u32  csq,ber;
    ret= RIL_NW_GetSignalQuality(&csq,&ber);
    if(ret == QL_RET_OK )
    {
        GSM.SignalStrength=((unsigned char)csq);
        //LOGData(TAG_GPRS,"Signal Strength = %d\r\n" , csq);
    }
}

#define PROFILE_SWITCH_READY_TIMEOUT_MS  30000U
#define PROFILE_SWITCH_READY_STEP_MS     1000U

static uint8_t EnsureSmsStkReadyForProfileSwitch(const char *reason)
{
    u32 waitedMs = 0;

    LOGData(TAG_PROFILE,
            "REG_DENIED flow: checking SMS/STK readiness before switch, reason=%s",
            reason ? reason : "?");

    while (waitedMs <= PROFILE_SWITCH_READY_TIMEOUT_MS)
    {
        if (!IsSMSInit)
        {
            LOGData(TAG_PROFILE,
                    "SMS not ready for profile switch, reinit attempt at %lu/%lu ms",
                    waitedMs,
                    PROFILE_SWITCH_READY_TIMEOUT_MS);
            ResetSMSContext();
            if (SMS_Initialize())
            {
                LOGData(TAG_PROFILE, "SMS reinit OK for profile switch");
            }
        }

        if (IsSMSInit)
        {
            if (EnableSTK())
            {
                LOGData(TAG_PROFILE,
                        "SMS/STK ready for profile switch after %lu ms",
                        waitedMs);
                return 1;
            }

            LOGData(TAG_PROFILE,
                    "STK not ready for profile switch, reinit attempt at %lu/%lu ms",
                    waitedMs,
                    PROFILE_SWITCH_READY_TIMEOUT_MS);
        }

        if (waitedMs >= PROFILE_SWITCH_READY_TIMEOUT_MS)
        {
            break;
        }

        ThreadSleep(PROFILE_SWITCH_READY_STEP_MS);
        waitedMs += PROFILE_SWITCH_READY_STEP_MS;
    }

    if (GSM.SignalStrength == 99)
    {
        LOGData(TAG_PROFILE,
                "SMS/STK not ready after %lu ms, but CSQ=99 (no signal). Allowing profile switch to proceed.",
                PROFILE_SWITCH_READY_TIMEOUT_MS);
        return 1;
    }

    LOGData(TAG_PROFILE,
            "SMS/STK not ready after %lu ms; escalating profile switch recovery",
            PROFILE_SWITCH_READY_TIMEOUT_MS);
    PushDiagEvent(DIAG_EVT_PRF_FAIL,
                  VTSState.CurrentProfile,
                  (uint8_t)prfReq,
                  "SMS-STK-not-ready");
    return 0;
}

static void ClearFailedProfileSwitchRequest(uint8_t targetProfile, const char *reason)
{
    uint8_t maxProfile = STKdata.is_valid ? STKdata.profiles_supported : 4;

    if (maxProfile > 4)
    {
        maxProfile = 4;
    }

    if (VTSState.CurrentProfile == NONE &&
        targetProfile >= 1 &&
        targetProfile <= maxProfile)
    {
        VTSState.CurrentProfile = targetProfile;
        LOGData(TAG_PROFILE,
                "First boot profile switch failed; assuming profile %d(%s) so GPRS can initialize",
                targetProfile,
                GetProfileName(targetProfile));
        UpdateStateInFlash();
    }

    LOGData(TAG_PROFILE,
            "Profile switch request cleared: target=%d(%s) reason=%s current=%d(%s)",
            targetProfile,
            GetProfileName(targetProfile),
            reason ? reason : "?",
            VTSState.CurrentProfile,
            GetProfileName(VTSState.CurrentProfile));
    prfReq = NONE;
}

static uint8_t ExecuteProfileSwitchWithRetry(uint8_t targetProfile, const char *reason)
{
    uint8_t attempt;

    for (attempt = 1; attempt <= 2; attempt++)
    {
        LOGData(TAG_PROFILE,
                "REG_DENIED flow: executing profile switch attempt %d/2 target=%d(%s) reason=%s",
                attempt,
                targetProfile,
                GetProfileName(targetProfile),
                reason ? reason : "?");
        LogProfileDiag((attempt == 1) ? "before-profile-switch" : "before-profile-switch-retry");

        if (SwitchProfile(targetProfile))
        {
            LOGData(TAG_PROFILE,
                    "REG_DENIED flow: IMSI changed, profile switch verified on attempt %d/2",
                    attempt);
            return 1;
        }

        LOGData(TAG_PROFILE,
                "REG_DENIED flow: profile switch attempt %d/2 failed; IMSI unchanged or STK command failed",
                attempt);
        PushDiagEvent(DIAG_EVT_PRF_FAIL,
                      targetProfile,
                      (targetProfile <= 4) ? VTSState.ProfileFailCount[targetProfile] : 0,
                      (attempt == 1) ? "switch-retry" : "switch-failed");

        if (attempt == 1)
        {
            LOGData(TAG_PROFILE,
                    "REG_DENIED flow: retrying once after SMS/STK reinit");
            EnsureSmsStkReadyForProfileSwitch("profile-switch-retry");
            ThreadSleep(1000);
        }
    }

#if SYSTEM_RECOVERY_ENABLE
    LOGData(TAG_PROFILE,
            "REG_DENIED flow: switch failed after retry; resetting modem (full device reset bypassed)");
    ClearFailedProfileSwitchRequest(targetProfile, "switch-failed-after-retry");
    ResetGSMModule();
    // SystemRecovery_RequestReset("profile switch failed after retry");
#else
    LOGData(TAG_PROFILE,
            "REG_DENIED flow: switch failed after retry; recovery disabled, no modem reset or full device reset");
    ClearFailedProfileSwitchRequest(targetProfile, "switch-failed-after-retry");
#endif
    return 0;
}

static void CheckprfReq(void)
{
    /* OLD CODE: uint8_t tries = 3; */
    static uint8_t s_bootActiveProfileRestoreChecked = 0;
    uint8_t lastActiveProfile;
    
    // Check if GSM is in valid state
    if(GSM.GSMState < SIM_DETECTED)
    {
        return;
    }

    if(IsMotaProcessing || IsFotaProcessing)
    {
        return;
    }

    /* ------------------------------------------------------------------
     * FIX #6 | CheckprfReq() - First-Boot Respects Persisted Fail Counts
     * DATE   : 2026-05-13
     * ------------------------------------------------------------------
     * ISSUE FIXED:
     *   On reboot after a session where DefProfile (e.g., Profile 1) had
     *   been blacklisted and all fail counts were saved to State.bin, the
     *   device would still try to switch back to DefProfile first because
     *   first-boot always set prfReq = VTSData.DefProfile unconditionally.
     *   This wasted a full 12-min cycle every reboot trying a known-dead
     *   profile, and immediately incremented its fail count again.
     *
     * ROOT CAUSE:
     *   The first-boot prfReq assignment ignored the ProfileFailCount[]
     *   values that had been loaded from flash by LoadState(). These
     *   values are valid historical data from the previous session and
     *   should be respected even on first boot.
     *
     * SOLUTION:
     *   On first boot (CurrentProfile == NONE), check if DefProfile is
     *   already blacklisted (fail count >= MAX_CONSECUTIVE_FAILS). If it
     *   is, call GetNextValidProfile(0) to find a viable starting profile
     *   instead of blindly using DefProfile.
     *
     * REVERT:
     *   Replace the entire if(STKdata.is_valid...) block with:
     *     prfReq = VTSData.DefProfile;
     * ------------------------------------------------------------------
     * OLD CODE:
     *   if(VTSState.CurrentProfile == NONE)
     *   {
     *       prfReq = VTSData.DefProfile;  // <-- always DefProfile, ignores flash fail counts
     *   }
     * ------------------------------------------------------------------ */
    if (!s_bootActiveProfileRestoreChecked && STKdata.is_valid && prfReq == NONE)
    {
        s_bootActiveProfileRestoreChecked = 1;
        lastActiveProfile = GetBootRestoreActiveProfile();
        if (lastActiveProfile != NONE &&
            VTSState.CurrentProfile != lastActiveProfile)
        {
            /* LAST ACTIVE PROFILE RESTORE 2026-05-25:
             * Active means this profile previously reached GPRS IP or server
             * connection and was saved to ActiveProfile.bin. Restore it once
             * after reset/power-cycle before trying default round-robin logic. */
            prfReq = (Providertypedef)lastActiveProfile;
            LOGData(TAG_PROFILE,
                    "Last active profile %d restored at boot (current=%d)",
                    lastActiveProfile,
                    VTSState.CurrentProfile);
        }
        else if (lastActiveProfile != NONE)
        {
            LOGData(TAG_PROFILE,
                    "Last active profile %d already matches current profile",
                    lastActiveProfile);
        }
    }

    if(VTSState.CurrentProfile == NONE && prfReq == NONE)
    {
        /* FIX #6 - check if DefProfile is blacklisted before trusting it */
        if(STKdata.is_valid &&
           VTSState.ProfileFailCount[VTSData.DefProfile] >= MAX_CONSECUTIVE_FAILS)
        {
            prfReq = GetNextValidProfile(0);
            LOGData(TAG_GPRS, "DefProfile %d blacklisted on first boot, starting from profile %d",
                    VTSData.DefProfile, prfReq);
        }
        else
        {
            prfReq = VTSData.DefProfile;
        }
    }

    

    
    // Process profile switch request if any
    if(prfReq != NONE)
    {
        uint8_t targetProfile = (uint8_t)prfReq;

        /* OLD CODE:
         *   if(!STKdata.is_valid) { prfReq = NONE; return; }
         *   if(!IsSMSInit) { return; }
         *   while(!SwitchProfile(prfReq)) { retry 3 times; Ql_Reset(0); }
         *
         * NEW FLOW:
         *   check SMS/STK ready -> reinit up to 30 sec -> switch profile
         *   -> verify IMSI changed inside SwitchProfile() -> retry once
         *   -> reset modem/full device if still failed.
         */
        LOGData(TAG_GPRS, "Profile switch requested to %d", targetProfile);

        if (!EnsureSmsStkReadyForProfileSwitch("profile-switch-request"))
        {
            ClearFailedProfileSwitchRequest(targetProfile, "SMS-STK-not-ready");
#if SYSTEM_RECOVERY_ENABLE
            LOGData(TAG_PROFILE, "SMS/STK not ready for profile switch; resetting modem (full device reset bypassed)");
            ResetGSMModule();
            // SystemRecovery_RequestReset("SMS/STK not ready for profile switch");
#else
            LOGData(TAG_PROFILE,
                    "SMS/STK not ready for profile switch; recovery disabled, no modem reset or full device reset");
#endif
            return;
        }

        if (!ExecuteProfileSwitchWithRetry(targetProfile, "profile-switch-request"))
        {
            return;
        }

        LOGData(TAG_GPRS, "Profile switch to %d successful", prfReq);
        // Update current profile and save state
        VTSState.CurrentProfile = targetProfile;
        UpdateStateInFlash();
        MarkActiveProfile(targetProfile, ACTIVE_PROFILE_REASON_NONE);
        prfReq = NONE;
        PrfChanged = 1;

        // Reset module after successful profile switch
        LOGData(TAG_GPRS, "Profile switch successful, initiating reset");
        ThreadSleep(1000);
        /* OLD CODE:
         *   Ql_Reset(0);
         *   ThreadSleep(2000);
         * Use SystemRecovery_RequestReset() so the reset reason is logged
         * before reboot and the watchdog is stopped cleanly. */
#if SYSTEM_RECOVERY_ENABLE
        SystemRecovery_RequestReset("profile switch successful");
#else
        LOGData(TAG_GPRS, "!!! Ql_Reset(0) IMMINENT - reason: profile switch successful !!!");
        Ql_Reset(0);
        ThreadSleep(2000);
#endif
    }
}


uint8_t RegDenytemp;
void ProcessREGISTER(void)
{
    //nwy_nw_regs_info_type_t reg_info;
    char opname[30] = {0};
    s32 cs;
    if(!GetSimState())
    {
        GSM.GSMState = SIM_NOT_DETECTED;
        return;
    }
    // nwy_nw_get_radio_st(&rfstate);
    // if(!rfstate)
    //     nwy_nw_set_radio_st(1);

    //LOGData(TAG_GPRS,"Current Sim Profile : %d",VTSState.CurrentProfile);
    GetSignalStrength();
    /* LOG CLEANUP 2026-05-16: CSQ fires every few seconds on every
     * GetGSMStatus() call. State-change logs and reg-denied paths
     * already print CSQ when it actually matters. The author at
     * line 802 had already commented out an earlier similar log.
     * OLD CODE:
     *   LOGData(TAG_GPRS,"CSQ is %d \r\n",GSM.SignalStrength);
     */

    #ifdef ENABLE_RS232_FAST
    Ql_sprintf(RSSend,"CSQ - %d\n",GSM.SignalStrength);
    //SendRS232String(RSSend);
    #endif

    // #ifdef _2G_PRIORITY
    //     nwy_nw_set_network_mode(2);
    // #else
    //     nwy_nw_set_network_mode(4);
    // #endif

    if(RIL_NW_GetGPRSState(&cs)==-1)
    {
        LOGData(TAG_GPRS,"Unable to get cs State!!!");
        goto REGISTER_FAIL;
    }
    LOGData(TAG_GPRS,"Network CS State: %d",cs);

    if(cs == 3)
    {
        RIL_NW_GetOperator(opname);
        LOGData(TAG_GPRS,"Current Selected Network: %s",opname);
        if(RegDenytemp < 25) 
        {
            RegDenytemp++;
            LOGData(TAG_GPRS,"Network Showing Reg Denied, waiting for resolve...");
            ThreadSleep(1000);
            goto REGISTER_FAIL;
        }
         // Get next valid profile intelligently
        if(ZigTestMode || IsMotaProcessing || IsFotaProcessing)
        {
            LOGData(TAG_GPRS, "ZigTestMode or FOTA/MOTA active, Skipping reset & profile switch on registration denied");
            goto REGISTER_FAIL;
        }
        #ifdef AUTO_PROFILESWITCH_DISABLE

        LOGData(TAG_GPRS, "Auto Profile Switch Disabled - Skipping profile switch on registration denied");
        goto REGISTER_FAIL;
        #else
        LOGData(TAG_GPRS, "*********************");
        
        
        GSM.IsRegDenied = 1;  /* Set flag for LED Manager to detect */

        /* ------------------------------------------------------------------
         * FIX #8 | ProcessREGISTER() - Clear RegDenytemp Before Profile Switch
         * DATE   : 2026-05-13
         * ------------------------------------------------------------------
         * ISSUE FIXED:
         *   After a registration denied event triggered a profile switch,
         *   RegDenytemp was left at its threshold value (25). On the very
         *   next registration check after the switch, the new profile could
         *   encounter a brief denied state (normal during network attach).
         *   Because RegDenytemp was already at 25, the device would
         *   IMMEDIATELY trigger another profile switch without waiting the
         *   25-second debounce window. This caused rapid cascading switches
         *   and premature blacklisting of profiles that just needed a few
         *   seconds to register.
         *
         * ROOT CAUSE:
         *   RegDenytemp was only cleared at the bottom of ProcessREGISTER()
         *   on a SUCCESSFUL registration (cs==1 or cs==5). It was never
         *   cleared when a profile switch was triggered, so the next profile
         *   started with the debounce counter already maxed out.
         *
         * SOLUTION:
         *   Zero RegDenytemp immediately before setting prfReq so the new
         *   profile gets a fresh 25-second debounce window after switching.
         *
         * REVERT:
         *   Remove the "RegDenytemp = 0;" line before the newpf assignment.
         * ------------------------------------------------------------------
         * OLD CODE (no RegDenytemp clear before switch):
         *   SendRS232Response("****REGISTRATION DENIED, Switching Profile ****\n");
         *   uint8_t newpf = GetNextValidProfile(VTSState.CurrentProfile);
         *   prfReq = newpf;
         *   CheckprfReq();
         * ------------------------------------------------------------------ */
        SendRS232Response("****REGISTRATION DENIED, Switching Profile ****\n");
        LOGData(TAG_PROFILE,
                "REG_DENIED detected: profile=%d(%s) CSQ=%d; selecting next profile",
                VTSState.CurrentProfile,
                GetProfileName(VTSState.CurrentProfile),
                GSM.SignalStrength);
        /* FIX #8 - clear debounce counter so new profile gets full window */
        RegDenytemp = 0;
        /* DIAG LOG: registration denied — log full profile state + reason */
        DiagCounters.RegDeniedTotal++;
        LOGData(TAG_PROFILE,
                "FAIL reason=REG_DENIED profile=%d(%s) CSQ=%d creg=3 fail_count=%d total_denied=%lu",
                VTSState.CurrentProfile,
                GetProfileName(VTSState.CurrentProfile),
                GSM.SignalStrength,
                VTSState.ProfileFailCount[VTSState.CurrentProfile],
                DiagCounters.RegDeniedTotal);
        PushDiagEvent(DIAG_EVT_REG_DENIED,
                      VTSState.CurrentProfile,
                      (uint8_t)GSM.SignalStrength,
                      "CS=3");
        LogProfileDiag("before-reg-denied-switch");
        uint8_t newpf = GetNextValidProfile(VTSState.CurrentProfile);
        LOGData(TAG_PROFILE,
                "REG_DENIED flow: selected next profile %d(%s); checking SMS/STK readiness",
                newpf,
                GetProfileName(newpf));
        LOGData(TAG_PROFILE,
                "SWITCH requested: %d(%s)->%d(%s) reason=reg-denied",
                VTSState.CurrentProfile, GetProfileName(VTSState.CurrentProfile),
                newpf, GetProfileName(newpf));
        prfReq = newpf;
        CheckprfReq();
        #endif
    }
    if(cs != 1 && cs != 5)
        goto REGISTER_FAIL;
    else 
    {
        if(RIL_NW_GetOperator(opname)==RIL_AT_SUCCESS)
        {
            Ql_strcpy(NetWork.Network, opname);
            LOGData(TAG_GPRS,"Network Name: %s",NetWork.Network);       
        }
        else
        {
            LOGData(TAG_GPRS,"Unable to get Network Name !!!");
        }

        if(VTSData.IsCustomSPN)
        {
            Ql_strcpy(NetWork.Network,VTSData.mSPN);
            LOGData(TAG_GPRS,"SPN(s) Name: %s",NetWork.Network);
        }
        
        
    }

    RegDenytemp=0;
    // if(VTSState.RegDeniedCount>0)
    // {
    //     VTSState.RegDeniedCount=0;
    //     //UpdateStateInFlash();
    // }
    // if(NWY_RES_OK != nwy_nw_get_register_info(&reg_info))
    //     goto REGISTER_FAIL;
    // if(reg_info.data_regs_valid!=1)
    //     goto REGISTER_FAIL;


    // LOGData(TAG_GPRS,"Network Data Reg state: %d\r\n"
    //             "Network Data Roam state: %d\r\n"
    //             "Network Data Radio Tech: %d\r\n",
    //             reg_info.data_regs.regs_state,
    //             reg_info.data_regs.roam_state,
    //             reg_info.data_regs.radio_tech);

    

    // if(reg_info.data_regs.regs_state == NWY_NW_SERVICE_NONE)
    //     goto REGISTER_FAIL;

    if (RIL_AT_SUCCESS != RIL_NW_GetOperator(opname))
    {
        LOGData(TAG_GPRS,"Unable to get SPN Name !!!");
        goto REGISTER_FAIL;
    }

    #ifdef ENABLE_RS232_FAST
    //SendRS232String("SIM REGISTERED\n");
    Ql_sprintf(RSSend,"SPN - %s\n",NetWork.Network);
    //SendRS232String(RSSend);
    #endif
    IsPacketReady.IsGSMNeighbour=1;

    //nwy_sim_get_lacid(&lac,&cid);
    // Ql_sprintf(GSM.CellID,"%i",cid);
    // Ql_sprintf(GSM.LAC,"%i",lac);
    // LOGData(TAG_GPRS,"LAC: %s, CELL_ID: %s \r\n", GSM.LAC, GSM.CellID);

    goto REGISTER_SUCCESS;
    REGISTER_SUCCESS:
        // Note: Don't reset failure count here - registration doesn't guarantee GPRS/data connectivity
        // Failure count is only reset when TCP connection succeeds (actual proof of working profile)
        GSM.GSMState=GPRS_INIT;
        // if(VTSState.RegDeniedCount>0){
        //     VTSState.RegDeniedCount=0;
        //     //UpdateStateInFlash();
        // }
        //SetStatLED(10,5);
        EnableQENG();
        // LED Manager will automatically show registered state
        return;

     REGISTER_FAIL:
        GSM.GSMState=SIM_DETECTED;
        //SetStatLED(10,2);
        // LED Manager will automatically show searching state
        return;
}


void selectAPN(void)
{
    Ql_memset(NetWork.APN,0x00,sizeof(NetWork.APN));

    LowerString(NetWork.Network);
	if(Ql_strstr(NetWork.Network,"airtel"))
    {
		Ql_strncpy(NetWork.APN,"airtelgprs.com", sizeof(NetWork.APN) - 1);
        NetWork.Provider = AIRTEL;
    }
	else if(Ql_strstr(NetWork.Network,"jio"))
    {
		Ql_strncpy(NetWork.APN,"JioNet", sizeof(NetWork.APN) - 1);
        NetWork.Provider = JIO;
    }
	else if (Ql_strstr(NetWork.Network,"bsnl") || Ql_strstr(NetWork.Network,"cellone"))
    {
        Ql_strncpy(NetWork.Network,"BSNL", sizeof(NetWork.Network) - 1);
		Ql_strncpy(NetWork.APN,"bsnlnet", sizeof(NetWork.APN) - 1);
        NetWork.Provider = BSNL;
    }
	else
    {
		Ql_strncpy(NetWork.APN,"www", sizeof(NetWork.APN) - 1);
        NetWork.Provider = VI;
        Ql_strncpy(NetWork.Network,"VI", sizeof(NetWork.Network) - 1);
    }
  
    if(!VTSData.AutoAPN)
        Ql_strncpy(NetWork.APN, VTSData.mAPN, sizeof(NetWork.APN) - 1);

    NetWork.APN[sizeof(NetWork.APN) - 1] = '\0';  // Ensure null termination
    LOGData(TAG_GPRS,"APN Selected : %s",NetWork.APN);
    #ifdef ENABLE_RS232_FAST
    Ql_sprintf(RSSend,"APN Selected - %s\n",NetWork.APN);
    //SendRS232String(RSSend);
    #endif

}

static void Callback_GPRS_Activated(u8 contextId, s32 errCode, void* customParam)
{
    if (errCode == SOC_SUCCESS)
    {
        LOGData(TAG_GPRS, "<--CallBack: activated GPRS successfully.-->\r\n");
        GSM.GSMState = GPRS_ACTIVE;
    }
    else
    {
        LOGData(TAG_GPRS, "<--CallBack: fail to activate GPRS, cause=%d)-->\r\n", errCode);
        GSM.GSMState = GPRS_INIT;
    }
    // Redundant assignment removed - already set in the if-else above
}

static void Callback_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam)
{
    if (errCode == SOC_SUCCESS)
    {
        LOGData(TAG_GPRS, "<--CallBack: deactivated GPRS successfully.-->\r\n");
    }
    else
    {
        LOGData(TAG_GPRS, "<--CallBack: fail to deactivate GPRS, cause=%d)-->\r\n", errCode);
    }
    if (GSM.GSMState == GPRS_ACTIVE)
    {
        GSM.GSMState = GPRS_INIT;
        LOGData(TAG_GPRS, "<-- GPRS drops down -->\r\n");
    }
}


int RcsHandle=0;
static bool isGPRSRegistered = false;  // Track registration state

void ActivateGPRS(void)
{
    ST_PDPContxt_Callback callback_gprs_func = {
        Callback_GPRS_Activated,
        Callback_GPRS_Deactived
    };
    int ret=0;
    if(!GetRegisterStat())
    {
        GSM.GSMState = SIM_DETECTED;
        return;
    }    

    if(prfReq != NONE)
    {
        LOGData(TAG_GPRS, "Profile switch pending, skipping GPRS activation attempt");
        return;
    }
    
    // Check if GPRS context was left in a bad state from previous failed attempt
    if(isGPRSRegistered)
    {
        LOGData(TAG_GPRS, "GPRS already registered from previous attempt, deactivating first...\r\n");
        
        // Try to deactivate if it was in progress
        ret = Ql_GPRS_Deactivate(RcsHandle);
        if (ret == GPRS_PDP_SUCCESS)
        {
            LOGData(TAG_GPRS, "GPRS deactivated successfully\r\n");
        }
        else
        {
            LOGData(TAG_GPRS, "GPRS deactivation returned: %d (may not have been active)\r\n", ret);
        }
        
        ThreadSleep(1000);  // Wait for deactivation to complete
    }
    else
    {
        // First time - need to register callbacks
        RcsHandle=0;
        ret = RIL_NW_SetGPRSContext(RcsHandle);
        if (ret != RIL_AT_SUCCESS)
        {
            LOGData(TAG_GPRS, "Failed to set GPRS context via RIL, ret: %d\r\n", ret);
            return;
        }
        else
        {
            LOGData(TAG_GPRS, "GPRS context set via RIL successful\r\n");
        }

        ret = Ql_GPRS_Register(RcsHandle, &callback_gprs_func, NULL);
        if(ret!=GPRS_PDP_SUCCESS)
        {
            LOGData(TAG_GPRS,"Registering GPRS Failed, ret: %d\r\n",ret);
            return;
        }
        else
        {
            LOGData(TAG_GPRS,"Registering GPRS Handle Success!\r\n");
            isGPRSRegistered = true;  // Mark as registered (once only)
        }
    }

    selectAPN();
    static ST_GprsConfig m_GprsConfig = {
        "CMNET",    // APN name
        "",         // User name for APN
        "",         // Password for APN
        0,
        NULL,
        NULL,
    };
    Ql_strcpy((char*)m_GprsConfig.apnName,NetWork.APN);
    ret = Ql_GPRS_Config(RcsHandle, &m_GprsConfig);
    if(ret!=GPRS_PDP_SUCCESS)
    {
        LOGData(TAG_GPRS,"Configuring GPRS Failed, ret: %d\r\n",ret);
        // Config failed - just return, handle stays registered for retry
        return;
    }
    else
        LOGData(TAG_GPRS,"Configuring GPRS Success!\r\n");
  
   
    LOGData(TAG_GPRS,"Dail up Initiating...\r\n");
    ret = Ql_GPRS_ActivateEx(RcsHandle, FALSE);
    if (ret != GPRS_PDP_SUCCESS && ret != GPRS_PDP_WOULDBLOCK)
    {
        LOGData(TAG_GPRS, "Activating GPRS Failed, ret: %d\r\n", ret);
        // Activation failed - just return, handle stays registered for retry
        return;
    }
    else if (ret == GPRS_PDP_ALREADY)
    {
        LOGData(TAG_GPRS, "GPRS is already activated!\r\n");
        GSM.GSMState = GPRS_ACTIVE;
        // Don't return here - continue to get IP address
    }
    else
    {
        LOGData(TAG_GPRS, "Activating GPRS initiated successfully, waiting for activation...\r\n");
    }

    int timeout = 60; // Timeout in seconds
    while (GSM.GSMState != GPRS_ACTIVE && timeout > 0)
    {
        if(prfReq != NONE)
        {
            LOGData(TAG_GPRS, "Profile switch requested during GPRS activation, aborting activation wait");

            ret = Ql_GPRS_Deactivate(RcsHandle);
            if (ret == GPRS_PDP_SUCCESS)
            {
                LOGData(TAG_GPRS, "GPRS deactivated successfully before profile switch\r\n");
            }
            else
            {
                LOGData(TAG_GPRS, "GPRS deactivate before profile switch returned: %d\r\n", ret);
            }

            ThreadSleep(500);
            GSM.GSMState = GPRS_INIT;
            return;
        }

        ThreadSleep(1000); 
        timeout--;
        LOGData(TAG_GPRS, "Waiting for GPRS activation... Remaining timeout: %d seconds\r\n", timeout);
    }

    if (GSM.GSMState != GPRS_ACTIVE)
    {
        LOGData(TAG_GPRS, "GPRS activation timeout! Cleaning up...\r\n");
        
        // Cleanup: Deactivate the GPRS context (no unregister - handle stays registered)
        ret = Ql_GPRS_Deactivate(RcsHandle);
        if (ret == GPRS_PDP_SUCCESS)
        {
            LOGData(TAG_GPRS, "GPRS deactivated successfully after timeout\r\n");
        }
        else
        {
            LOGData(TAG_GPRS, "Failed to deactivate GPRS after timeout, ret: %d\r\n", ret);
        }
        
        ThreadSleep(500); // Give it a moment to complete deactivation
        
        GSM.GSMState = GPRS_INIT;  // Reset state to allow retry
        // Note: isGPRSRegistered stays true, handle is still registered for next attempt
        return;
    }

    // GPRS is now active, get IP address
    LOGData(TAG_GPRS, "GPRS activated successfully!\r\n");

    u32 ipaddr = 0;
    ret = Ql_GPRS_GetLocalIPAddress(RcsHandle, &ipaddr);
    if(ret!=GPRS_PDP_SUCCESS)
    {
        LOGData(TAG_GPRS,"Getting IP Address Failed, ret: %d\r\n",ret);
        
        // IP address retrieval failed - deactivate and retry
        Ql_GPRS_Deactivate(RcsHandle);
        ThreadSleep(500);
        GSM.GSMState = GPRS_INIT;  // Reset state to retry
        // Note: isGPRSRegistered stays true, handle is still registered for next attempt
        return;
    }
    else
    {
        LOGData(TAG_GPRS,"Getting IP Address Success!\r\n");
        char ipStr[16] = {0};
        Ql_sprintf(ipStr, "%d.%d.%d.%d", 
                   (ipaddr >> 24) & 0xFF, 
                   (ipaddr >> 16) & 0xFF, 
                   (ipaddr >> 8) & 0xFF, 
                   ipaddr & 0xFF);
        LOGData(TAG_GPRS,"IP Address : %s\r\n", ipStr);
    }  
    
    #ifdef ENABLE_RS232_FAST 
    //SendRS232String("GPRS Activated !");
    #endif
    

    //SetStatLED(10,9);
    // LED Manager will automatically show GPRS ready state
    LOGData(TAG_GPRS, "GPRS fully activated with IP address\r\n");
    MarkActiveProfile(VTSState.CurrentProfile, ACTIVE_PROFILE_REASON_GPRS);
    if (VTSState.CurrentProfile >= 1 &&
        VTSState.CurrentProfile <= 4 &&
        VTSState.ProfileFailCount[VTSState.CurrentProfile] > 0)
    {
        LOGData(TAG_PROFILE,
                "Profile %d reached GPRS; clearing fail count %d",
                VTSState.CurrentProfile,
                VTSState.ProfileFailCount[VTSState.CurrentProfile]);
        VTSState.ProfileFailCount[VTSState.CurrentProfile] = 0;
        UpdateStateInFlash();
    }
    // Keep isGPRSRegistered = true since it's actively being used
    //IsPacketReady.IsGSMParam=1;
   // GSM.GSMState=GPRS_ACTIVE;

}

void ntp_cb(char *strURC)
{
    LOGData(TAG_GPRS,"NTP Time: %s",strURC);
}



s32 SetupAutoTimesync(void)
{
    s32 ret = RIL_AT_FAILED;
    char strAT[200];
    char responseBuffer[200];
    bool isResetRequired = FALSE;

    /* LOG CLEANUP 2026-05-16: condensed from 17 verbose LOGData lines
     * to 4 essential ones (start / query results / change action / final).
     * Each AT command result is still logged on failure paths.
     * OLD CODE shape (kept for revert reference):
     *   LOGData(TAG_GPRS, "SetupAutoTimesync: Starting setup ...");
     *   LOGData(TAG_GPRS, "SetupAutoTimesync: AT+QNITZ query successful, response: %s", ...);
     *   LOGData(TAG_GPRS, "SetupAutoTimesync: AT+QNITZ is not set to 1, updating...");
     *   LOGData(TAG_GPRS, "SetupAutoTimesync: Failed to set AT+QNITZ to 1, error: %d", ret);
     *   ... 13 more similar lines for CTZU + reset paths ...
     */
    LOGData(TAG_GPRS, "TimeSync: starting QNITZ+CTZU configuration");

    // --- Check AT+QNITZ status ---
    Ql_memset(responseBuffer, 0, sizeof(responseBuffer));
    ret = SendATCommandSimple("AT+QNITZ?\r\n", responseBuffer, sizeof(responseBuffer), 1000);
    if (ret != RIL_ATRSP_SUCCESS)
    {
        LOGData(TAG_GPRS, "TimeSync: QNITZ query failed (%d)", ret);
        return ret;
    }
    if (!Ql_strstr(responseBuffer, "+QNITZ: 1"))
    {
        ret = SendATCommandSimple("AT+QNITZ=1\r\n", responseBuffer, sizeof(responseBuffer), 1000);
        if (ret != RIL_ATRSP_SUCCESS)
        {
            LOGData(TAG_GPRS, "TimeSync: QNITZ=1 set failed (%d)", ret);
            return ret;
        }
        isResetRequired = TRUE;
    }

    // --- Check AT+CTZU status ---
    Ql_memset(responseBuffer, 0, sizeof(responseBuffer));
    ret = SendATCommandSimple("AT+CTZU?\r\n", responseBuffer, sizeof(responseBuffer), 1000);
    if (ret != RIL_ATRSP_SUCCESS)
    {
        LOGData(TAG_GPRS, "TimeSync: CTZU query failed (%d)", ret);
        return ret;
    }
    if (!Ql_strstr(responseBuffer, "+CTZU: 2"))
    {
        ret = SendATCommandSimple("AT+CTZU=2\r\n", responseBuffer, sizeof(responseBuffer), 1000);
        if (ret != RIL_ATRSP_SUCCESS)
        {
            LOGData(TAG_GPRS, "TimeSync: CTZU=2 set failed (%d)", ret);
            return ret;
        }
        isResetRequired = TRUE;
    }

    // --- Reset module if required ---
    if (isResetRequired)
    {
        /* FIX 2026-05-17: explicit log immediately before AT+CFUN=1,1.
         * Used by field analysis to correlate modem-reboot events with
         * the originating subsystem (here: time-sync configuration). */
        LOGData(TAG_GPRS, "!!! AT+CFUN=1,1 IMMINENT — reason: TimeSync (QNITZ/CTZU config change commit) !!!");
        Ql_sprintf(strAT, "AT+CFUN=1,1\r\n");
        ret = SendATCommandSimple(strAT, responseBuffer, sizeof(responseBuffer), 2000);
        if (ret != RIL_ATRSP_SUCCESS)
        {
            LOGData(TAG_GPRS, "TimeSync: CFUN=1,1 failed (%d)", ret);
            return ret;
        }
    }

    LOGData(TAG_GPRS, "TimeSync: setup OK (reset_applied=%d)", isResetRequired);
    return RIL_AT_SUCCESS;
}



void UpdateTime(void)
{
    static int st = 0;
    uint8_t tmsource = 0;
    ST_Time julian_time = {0};
    static uint8_t count;
    uint8_t useGPSTime = 0;

    GetSignalStrength();

    Ql_GetLocalTime(&julian_time);

    // Check if network time is valid (year > 2022)
    if (julian_time.year > 2022)
    {
        // Use network time (priority)
        CurrentDateTime.Date = julian_time.day;
        CurrentDateTime.Month = julian_time.month;
        CurrentDateTime.Year = julian_time.year % 100;
        CurrentDateTime.Hour = julian_time.hour;
        CurrentDateTime.Min = julian_time.minute;
        CurrentDateTime.Sec = julian_time.second;
        
        #ifdef PROTO_CDAC
        // Network time is in UTC (due to AT+CTZU=2), adjust to IST (+5:30) for CDAC protocol
        AdjustGPSTimeToIST(&CurrentDateTime);
        #endif
        
        GSM.IsTimeSet = 1;
        tmsource = 1;
    }
    else if (GPSDateTime.Year > 22)
    {
        // Network time invalid, but GPS has valid time - use GPS time as fallback
        LOGData(TAG_GPRS, "Network time invalid, using GPS time as fallback");
        
        CurrentDateTime.Date = GPSDateTime.Date;
        CurrentDateTime.Month = GPSDateTime.Month;
        CurrentDateTime.Year = GPSDateTime.Year;
        CurrentDateTime.Hour = GPSDateTime.Hour;
        CurrentDateTime.Min = GPSDateTime.Min;
        CurrentDateTime.Sec = GPSDateTime.Sec;
        
        #ifdef PROTO_CDAC
        // GPS time is in UTC, adjust to IST (+5:30) for CDAC protocol
        AdjustGPSTimeToIST(&CurrentDateTime);
        LOGData(TAG_GPRS, "GPS time adjusted to IST (+5:30)");
        #endif
        
        GSM.IsTimeSet = 1;
        tmsource = 2;
        useGPSTime = 1;
    }
    else
    {
        // Neither network nor GPS time is valid
        GSM.IsTimeSet = 0;
        LOGData(TAG_GPRS, "No valid time source available (Network year: %d, GPS Year: %d)", 
                julian_time.year, GPSDateTime.Year);
        
        // Try NTP as last resort if GPRS is active
        if (GSM.GSMState >= GPRS_ACTIVE)
        {
            if (!st)
            {
                RIL_NTP_START((u8*)"time.nist.gov", 123, ntp_cb);
                st = 1;
                LOGData(TAG_GPRS, "NTP Time Request Sent\r\n");
            }
        }
    }
    
    if (++count > 4)
    {
        LOGData(TAG_GPRS, "20%02d-%02d-%02d %02d:%02d:%02d, ts:%d csq: %d%s", 
                CurrentDateTime.Year, CurrentDateTime.Month, CurrentDateTime.Date,
                CurrentDateTime.Hour, CurrentDateTime.Min, CurrentDateTime.Sec, 
                tmsource, GSM.SignalStrength,
                useGPSTime ? " [GPS]" : "");
        count = 0;
    }
}

void GetDeviceIMEI(void)
{
    s32 ret = RIL_AT_FAILED;
    char imei[20] = {0};
    ret = RIL_GetIMEI(imei);
    if (ret == RIL_AT_SUCCESS)
    {
        #ifdef VIRTUAL_IMEI
        Ql_strcpy(NetWork.IMEI, VIMEI);
        LOGData(TAG_GPRS, "Device VIMEI: %s", NetWork.IMEI);
        #else
        Ql_strcpy(NetWork.IMEI, imei);
        LOGData(TAG_GPRS, "Device IMEI: %s", NetWork.IMEI);
        #endif
    }
    else
    {
        LOGData(TAG_GPRS, "Failed to get Device IMEI, error: %d", ret);
    }
}

/******************************************************************************
* Function: CheckGSMSignalQuality
* 
* Description: Checks GSM signal quality to detect if module is unresponsive.
*              Returns signal quality in CSQ format (0-31, 99=unknown).
* 
* Returns: Signal quality value (0-31), 99 if unknown/error
******************************************************************************/
uint8_t CheckGSMSignalQuality(void)
{
    s32 ret;
    u32 csq, ber;

#if SYSTEM_WDTTEST_COMMAND_ENABLE
    if (s_testForceGsmHang)
    {
        GSM.SignalStrength = 99;
        LOGData(TAG_GPRS, "TEST GSMHANG: forcing signal quality CSQ=99");
        return 99;
    }
#endif
    
    ret = RIL_NW_GetSignalQuality(&csq, &ber);
    if (ret == QL_RET_OK)
    {
        LOGData(TAG_GPRS, "GSM Signal Quality: CSQ=%d, BER=%d", csq, ber);
        return (uint8_t)csq;
    }
    else
    {
        LOGData(TAG_GPRS, "Failed to check GSM signal quality, error: %d", ret);
        return 99;  // Return unknown value
    }
}

/******************************************************************************
* Function: IsGSMUnresponsive
* 
* Description: Detects if GSM module is unresponsive based on signal quality.
*              Returns true if signal is 0 or module cannot be queried.
* 
* Returns: 1 if unresponsive, 0 if responsive
******************************************************************************/
uint8_t IsGSMUnresponsive(void)
{
    s32 ret;
    u32 csq, ber;

#if SYSTEM_WDTTEST_COMMAND_ENABLE
    if (s_testForceGsmHang)
    {
        LOGData(TAG_GPRS, "TEST GSMHANG: forcing GSM unresponsive state");
        return 1;
    }
#endif

    ret = RIL_NW_GetSignalQuality(&csq, &ber);
    if (ret == QL_RET_OK)
    {
        return 0; // AT command succeeded, module is responsive (even if no signal)
    }

    LOGData(TAG_GPRS, "GSM module is unresponsive! RIL_NW_GetSignalQuality failed, error: %d", ret);
    return 1;
}

/******************************************************************************
* Function: ResetGSMModule
* 
* Description: Resets GSM module using AT+CFUN=1,1 command.
*              This performs a soft reset of the GSM module.
*              Does NOT reset the device itself.
* 
* Returns: None
******************************************************************************/
void ResetGSMModule(void)
{
    s32 ret;
    char responseBuffer[256] = {0};
    
    /* FIX 2026-05-17: collapsed 4 banner lines into 1 explicit imminent log.
     * OLD CODE:
     *   LOGData(TAG_GPRS, "========================================");
     *   LOGData(TAG_GPRS, "Attempting GSM module reset...");
     *   LOGData(TAG_GPRS, "Sending AT+CFUN=1,1 command");
     *   LOGData(TAG_GPRS, "========================================");
     */
    LOGData(TAG_GPRS, "!!! AT+CFUN=1,1 IMMINENT — reason: GSM module hang recovery (ResetGSMModule) !!!");
    /* DIAG counter is incremented by the caller (GPRSThreadEntry GPRS-fail
     * threshold block) which already pushes DIAG_EVT_GSM_RESET. */

    // Send AT+CFUN=1,1 to reset GSM module
    ret = SendATCommandSimple("AT+CFUN=1,1\r\n", responseBuffer, sizeof(responseBuffer), 5000);
    
    if (ret == RIL_ATRSP_SUCCESS)
    {
        LOGData(TAG_GPRS, "GSM module reset command accepted");
        LOGData(TAG_GPRS, "Response: %s", responseBuffer);
        ThreadSleep(3000);  // Wait for module to stabilize
        LOGData(TAG_GPRS, "GSM module reset completed successfully");
    }
    else
    {
        LOGData(TAG_GPRS, "GSM module reset command failed, error code: %d", ret);
        LOGData(TAG_GPRS, "Response: %s", responseBuffer);
    }
}

/******************************************************************************
* Function: HandleGSMHang
* 
* Description: Handles GSM module hang condition.
*              Attempts to reset the module and recover.
*              Logs all reset attempts for debugging.
* 
* Returns: None
******************************************************************************/
void HandleGSMHang(void)
{
    LOGData(TAG_GPRS, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    LOGData(TAG_GPRS, "GSM module hang detected!");
    LOGData(TAG_GPRS, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    
    // Attempt to reset GSM module
    ResetGSMModule();
    
    // Wait and check if recovery was successful
    ThreadSleep(2000);
    
    if (IsGSMUnresponsive())
    {
        LOGData(TAG_GPRS, "GSM module still unresponsive after reset attempt");
        LOGData(TAG_GPRS, "Triggering system-level recovery");
        
        // If GSM still unresponsive, log event and request system recovery
        // Note: The system recovery can be handled by SystemRecovery module
        SendRS232Response("***GSM Module Hang Detected - Recovery in Progress***\n");
    }
    else
    {
        LOGData(TAG_GPRS, "GSM module recovered successfully after reset");
        SendRS232Response("***GSM Module Successfully Recovered***\n");
    }
}

/******************************************************************************
* Function: LogGSMStatus
* 
* Description: Logs current GSM module status including signal, state, and network info.
*              Used for debugging and monitoring GSM health.
* 
* Returns: None
******************************************************************************/
void LogGSMStatus(void)
{
    uint8_t csq;
    char opname[30] = {0};
    
    LOGData(TAG_GPRS, "========== GSM STATUS REPORT ==========");
    
    // Log GSM state
    LOGData(TAG_GPRS, "GSM State: %d (0=Not Detected, 1=Detected, 2=GPRS Init, 3=GPRS Active)",
            GSM.GSMState);
    
    // Log signal quality
    csq = CheckGSMSignalQuality();
    if (csq != 99)
    {
        LOGData(TAG_GPRS, "Signal Quality (CSQ): %d (0=worst, 31=best)", csq);
    }
    else
    {
        LOGData(TAG_GPRS, "Signal Quality: Unknown or not available");
    }
    
    // Log IMSI and IMEI
    LOGData(TAG_GPRS, "IMEI: %s", NetWork.IMEI);
    LOGData(TAG_GPRS, "IMSI: %s", NetWork.IMSI);
    LOGData(TAG_GPRS, "SIM Number: %s", NetWork.SIMNo);
    
    // Log network operator
    if (RIL_NW_GetOperator(opname) == RIL_AT_SUCCESS)
    {
        LOGData(TAG_GPRS, "Operator: %s", opname);
    }
    else
    {
        LOGData(TAG_GPRS, "Operator: Unable to retrieve");
    }
    
    // Log APN
    LOGData(TAG_GPRS, "APN: %s", NetWork.APN);
    LOGData(TAG_GPRS, "Provider: %d", NetWork.Provider);
    
    // Log current profile
    LOGData(TAG_GPRS, "Current Profile: %d", VTSState.CurrentProfile);
    
    // Log registration status
    LOGData(TAG_GPRS, "Registration Denied Flag: %d", GSM.IsRegDenied);
    
    LOGData(TAG_GPRS, "====================================");
}


/* ======================================================================
 * FIX #9 / #10 / #11 | GPRSThreadEntry() - GSM Hang Detection + GPRS
 *                       Failure Counter + Profile Blacklist Integration
 * FILE   : custom/GPRS.c
 * DATE   : 2026-05-13
 * ======================================================================
 *
 * BACKGROUND:
 *   Three functions existed in this file that were NEVER called:
 *     - IsGSMUnresponsive()  - checks CSQ=0/99 to detect module hang
 *     - HandleGSMHang()      - sends AT+CFUN=1,1 soft reset
 *     - ResetGSMModule()     - called by HandleGSMHang internally
 *   These were dead code. Log analysis confirmed they were never
 *   invoked in 78,000+ lines of device logs.
 *
 * -----------------------------------------------------------------------
 * FIX #9 - Wire HandleGSMHang / IsGSMUnresponsive into the GPRS thread
 * -----------------------------------------------------------------------
 * ISSUE FIXED:
 *   When the GSM module hangs (stops responding to AT commands, CSQ=0
 *   or 99), the device would loop in GPRS_INIT indefinitely, timing out
 *   every 60 seconds. After 12 minutes the PRF_AUTOSWITCH timer would
 *   fire, but since the module was unresponsive, the profile switch would
 *   also fail. The device would effectively be dead until the watchdog
 *   (30s hardware watchdog fed by Systic heartbeat) restarted it — but
 *   only if the main Systic thread was also hung, which was not guaranteed.
 *
 * SOLUTION:
 *   Track consecutive ActivateGPRS() failures with gprs_fail_count.
 *   After MAX_GPRS_INIT_FAILS (5) consecutive failures (~5 min), check
 *   IsGSMUnresponsive(). If CSQ=0 or 99:
 *     1. Call HandleGSMHang() → sends AT+CFUN=1,1 soft modem reset
 *     2. Wait 5 seconds for modem to recover
 *     3. Check again — if still unresponsive, call
 *        SystemRecovery_RequestReset() for a full device reset
 *
 * -----------------------------------------------------------------------
 * FIX #10 - Faster profile switch when GPRS consistently fails
 * -----------------------------------------------------------------------
 * ISSUE FIXED:
 *   In the device log, Profile 1 (VI, APN="www") had 10+ consecutive
 *   GPRS activation timeouts of 60 seconds each (~10 minutes) before the
 *   PRF_AUTOSWITCH 12-minute timer triggered a profile switch. The network
 *   was consistently rejecting APN "www" but the device kept retrying with
 *   no early exit. This caused "stuck at Profile 1" for 10-12 minutes per
 *   cycle even though the problem was clearly not going to self-resolve.
 *
 * SOLUTION:
 *   After MAX_GPRS_INIT_FAILS (5) failures AND IsGSMUnresponsive() returns
 *   FALSE (module is alive, signal present), treat this as a network/APN
 *   rejection by this profile. Set prfReq = GetNextValidProfile() to
 *   trigger an immediate profile switch in ~5 minutes instead of waiting
 *   the full 12-minute timer.
 *
 * -----------------------------------------------------------------------
 * FIX #11 - Link GPRS activation failures to ProfileFailCount
 * -----------------------------------------------------------------------
 * ISSUE FIXED:
 *   ProfileFailCount[] was only incremented when the STK switch command
 *   itself failed (AT command error or IMSI unchanged). If a profile's
 *   STK switch succeeded (IMSI changed) but GPRS never activated on that
 *   profile (network/APN rejection), the profile's fail count stayed at 0
 *   and it would keep being selected by GetNextValidProfile() on every
 *   cycle. The scoring system had no awareness of GPRS-level failures.
 *
 * SOLUTION:
 *   After MAX_GPRS_INIT_FAILS consecutive GPRS activation failures on the
 *   current profile, increment VTSState.ProfileFailCount[CurrentProfile]
 *   and save to flash. Once it reaches MAX_CONSECUTIVE_FAILS (3), that
 *   profile is blacklisted from GPRS rotation just as it would be from
 *   STK-level failures.
 *
 * -----------------------------------------------------------------------
 * REVERT ALL THREE (9/10/11):
 *   Replace the entire GPRSThreadEntry() body below with the OLD CODE
 *   block shown in comments. Remove the gprs_fail_count variable.
 *   Also remove the #include "SystemRecovery.h" at the top of this file.
 * -----------------------------------------------------------------------
 * OLD CODE (original GPRSThreadEntry before any fixes):
 *
 *   void GPRSThreadEntry(s32 taskId)
 *   {
 *       InitGPRSThread(taskId);
 *       ThreadSleep(500);
 *       GetDeviceIMEI();
 *       EnableSTK();
 *       while(1)
 *       {
 *           while(SleepConfig.IsEnabled)
 *           {
 *               ThreadSleep(2000);
 *           }
 *           switch (GSM.GSMState)
 *           {
 *               case SIM_NOT_DETECTED:
 *                   LOGData(TAG_GPRS,"STATE -> SIM NOT DETECTED");
 *                   GetSimState();
 *                   break;
 *               case SIM_DETECTED:
 *                   LOGData(TAG_GPRS,"STATE -> SIM DETECTED");
 *                   ProcessREGISTER();
 *                   break;
 *               case GPRS_INIT:
 *                   LOGData(TAG_GPRS,"STATE -> GPRS INIT");
 *                   ActivateGPRS();   // <-- no failure tracking
 *                   break;
 *               case GPRS_ACTIVE:
 *                   //CheckGPRSState();
 *                   //RecheckRegistoration();
 *                   break;            // <-- gprs_fail_count never reset
 *               default:
 *                   break;
 *           }
 *           CheckprfReq();
 *           UpdateTime();
 *           if(GSM.GSMState < GPRS_ACTIVE)
 *               ThreadSleep(1000);
 *           else
 *               ThreadSleep(200);
 *       }
 *   }
 * ====================================================================== */
void GPRSThreadEntry(s32 taskId)
{
    InitGPRSThread(taskId);
    ThreadSleep(500);
    GetDeviceIMEI();
    EnableSTK();

    /* FIX #9/10/11 - consecutive GPRS activation failure counter */
    uint8_t gprs_fail_count = 0;

    while(1)
    {
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_WATCHDOG_ENABLE
        /* Task health check-in is only needed when the recovery watchdog is enabled. */
        SystemRecovery_CheckInTask(WDT_TASK_GPRS);
#endif

        while(SleepConfig.IsEnabled)
        {
            ThreadSleep(2000);
            /* LED Manager automatically shows sleep state */
        }
        switch (GSM.GSMState)
        {
            case SIM_NOT_DETECTED:
                /* LOG CLEANUP 2026-05-16: Removed redundant TAG_GPRS header.
                 * TAG_STATE line below already shows state name AND number.
                 * OLD: LOGData(TAG_GPRS,"STATE -> SIM NOT DETECTED"); */
                LOGData(TAG_STATE, "CURRENT: %s(%d) profile=%d(%s) CSQ=%d",
                        GetGSMStateName(GSM.GSMState), GSM.GSMState,
                        VTSState.CurrentProfile, GetProfileName(VTSState.CurrentProfile),
                        GSM.SignalStrength);
                GetSimState();
                gprs_fail_count = 0;  /* FIX #9 - reset on state change */
                break;

            case SIM_DETECTED:
                /* OLD: LOGData(TAG_GPRS,"STATE -> SIM DETECTED"); */
                LOGData(TAG_STATE, "CURRENT: %s(%d) profile=%d(%s) CSQ=%d prfReq=%d",
                        GetGSMStateName(GSM.GSMState), GSM.GSMState,
                        VTSState.CurrentProfile, GetProfileName(VTSState.CurrentProfile),
                        GSM.SignalStrength, (uint8_t)prfReq);
                ProcessREGISTER();
                gprs_fail_count = 0;  /* FIX #9 - reset on state change */

                break;

            case GPRS_INIT:
                /* OLD: LOGData(TAG_GPRS,"STATE -> GPRS INIT"); */
                LOGData(TAG_STATE, "CURRENT: %s(%d) profile=%d(%s) fails=[%d,%d,%d,%d,%d] CSQ=%d",
                        GetGSMStateName(GSM.GSMState), GSM.GSMState,
                        VTSState.CurrentProfile, GetProfileName(VTSState.CurrentProfile),
                        VTSState.ProfileFailCount[0], VTSState.ProfileFailCount[1],
                        VTSState.ProfileFailCount[2], VTSState.ProfileFailCount[3],
                        VTSState.ProfileFailCount[4], GSM.SignalStrength);
#if SYSTEM_WDTTEST_COMMAND_ENABLE
                if (s_testForceGsmHang)
                {
                    GSM.SignalStrength = 99;
                    LOGData(TAG_STATE,
                            "TEST GSMHANG: skipping ActivateGPRS() to force hang recovery path");
                }
                else
#endif
                {
                    ActivateGPRS();
                }

                /* FIX #9/10/11 - track consecutive GPRS activation failures */
                if (GSM.GSMState != GPRS_ACTIVE)
                {
                    gprs_fail_count++;
                    LOGData(TAG_GPRS, "GPRS activation failed, consecutive failures: %d/%d",
                            gprs_fail_count, MAX_GPRS_INIT_FAILS);

                    if (gprs_fail_count >= MAX_GPRS_INIT_FAILS)
                    {
                        gprs_fail_count = 0;  /* reset so cycle can repeat */

                        /* DIAG LOG: GPRS consistently failing — log full snapshot */
                        DiagCounters.GprsFailCount++;
                        LogProfileDiag("gprs-fail-threshold");
                        PushDiagEvent(DIAG_EVT_GPRS_FAIL,
                                      VTSState.CurrentProfile,
                                      VTSState.ProfileFailCount[VTSState.CurrentProfile],
                                      "APN-reject");

                        if (IsGSMUnresponsive())
                        {
                            /* FIX #9 - GSM module not responding: attempt soft reset */
                            /* DIAG LOG: GSM hang detected */
                            DiagCounters.GsmHangCount++;
                            LOGData(TAG_STATE, "!!! GSM HANG DETECTED CSQ=%d — attempting recovery !!!",
                                    GSM.SignalStrength);
                            PushDiagEvent(DIAG_EVT_GSM_HANG, GSM.SignalStrength, 0, "no-signal");
#if SYSTEM_RECOVERY_ENABLE
                            HandleGSMHang();
                            /* DIAG LOG: GSM reset sent */
                            DiagCounters.GsmResetCount++;
                            PushDiagEvent(DIAG_EVT_GSM_RESET, (uint8_t)DiagCounters.GsmResetCount, 0, "CFUN=1,1");
                            ThreadSleep(5000);  /* wait for modem restart */
                            if (IsGSMUnresponsive())
                            {
                                /* FIX #9 - module still dead: full device reset bypassed */
                                LOGData(TAG_STATE, "!!! GSM still unresponsive after CFUN reset — system reset bypassed !!!");
                                // SystemRecovery_RequestReset("GSM hang: unresponsive after AT+CFUN=1,1 attempt");
                            }
#else
                            LOGData(TAG_STATE,
                                    "!!! GSM hang recovery disabled: no AT+CFUN=1,1 and no full device reset !!!");
#endif
                        }
                        else if (VTSState.CurrentProfile >= 1 && VTSState.CurrentProfile <= 4)
                        {
                            /* FIX #10/11 - module alive, network/APN rejecting this profile.
                             * Score the profile as failing so it can be skipped by
                             * GetNextValidProfile() and trigger an immediate switch instead
                             * of waiting the full 12-minute PRF_AUTOSWITCH timer. */
                            VTSState.ProfileFailCount[VTSState.CurrentProfile]++;
                            LOGData(TAG_GPRS,
                                    "Profile %d GPRS consistently failing (network/APN rejection),"
                                    " fail count: %d/%d",
                                    VTSState.CurrentProfile,
                                    VTSState.ProfileFailCount[VTSState.CurrentProfile],
                                    MAX_CONSECUTIVE_FAILS);
                            /* DIAG LOG: profile fail reason */
                            LOGData(TAG_PROFILE,
                                    "FAIL reason=APN-reject profile=%d(%s) fail_count=%d/%d CSQ=%d",
                                    VTSState.CurrentProfile,
                                    GetProfileName(VTSState.CurrentProfile),
                                    VTSState.ProfileFailCount[VTSState.CurrentProfile],
                                    MAX_CONSECUTIVE_FAILS,
                                    GSM.SignalStrength);
                            UpdateStateInFlash();

                            if (VTSState.ProfileFailCount[VTSState.CurrentProfile] >= MAX_CONSECUTIVE_FAILS)
                            {
                                /* FIX #10 - blacklisted: switch profile now, don't wait 12 min */
                                LOGData(TAG_GPRS,
                                        "Profile %d GPRS blacklisted — switching to next valid profile now",
                                        VTSState.CurrentProfile);
                                LOGData(TAG_PROFILE,
                                        "BLACKLIST profile=%d(%s) — immediate switch triggered",
                                        VTSState.CurrentProfile,
                                        GetProfileName(VTSState.CurrentProfile));
                                LogProfileDiag("before-gprs-blacklist-switch");
                                prfReq = GetNextValidProfile(VTSState.CurrentProfile);
                                LOGData(TAG_PROFILE,
                                        "SWITCH requested: %d(%s)->%d(%s) reason=gprs-blacklist",
                                        VTSState.CurrentProfile, GetProfileName(VTSState.CurrentProfile),
                                        (uint8_t)prfReq, GetProfileName((uint8_t)prfReq));
                            }
                        }
                    }
                }
                break;

            case GPRS_ACTIVE:
                gprs_fail_count = 0;  /* FIX #9 - successful GPRS, clear counter */
                break;

            default:
                break;
        }

        CheckprfReq();

        UpdateTime();
        if(GSM.GSMState < GPRS_ACTIVE)
            ThreadSleep(1000);
        else
            ThreadSleep(200);
    }
}

void InitGPRSThread(u32 taskId)
{
    s32 ret;
    OSThread GPRS_Thread={0};
    GPRS_Thread.taskId = taskId;
    strcpy(GPRS_Thread.taskName, "GPRS Thread");
    GPRS_Thread.taskEnable = 1;
    GPRS_Thread.taskState = TASK_STATE_NORMAL;
    GPRS_Thread.taskPriority = 1;
    ret = InitializeThread(&GPRS_Thread);
    if (ret != 1)
    {
        LOGData(TAG_GPRS, "Failed to initialize GPRS thread");
        return;
    }
    LOGData(TAG_GPRS, "thread initialized successfully");
}
