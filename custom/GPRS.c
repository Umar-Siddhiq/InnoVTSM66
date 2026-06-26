
#include "GPRS.h"
#include "Hardware.h"
#include "Systic.h"
#include "GPS.h"
#include "File.h"
#include "SMS.h"
#include "ril_custom.h"

GSM_Typedef GSM = {0};
NET_Typedef NetWork = {{0}};
uint8_t updntp = 0;
_RTC CurrentDateTime = {0};
uint8_t IsQNITZSet=0;
Providertypedef prfReq=0;
 uint8_t PrfChanged;
extern uint8_t IsFotaProcessing;
extern uint8_t IsMotaProcessing;
#define STK_ENB

STKDatatypedef STKdata = {0};

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

#define MAX_CONSECUTIVE_FAILS 3

/******************************************************************************
* Function: GetNextValidProfile
* 
* Description: Selects next profile for switching.
*              
*              If ENABLE_PROFILE_BLACKLIST is defined:
*                - Intelligently selects next profile based on failure history
*                - Skips profiles with consecutive failures >= MAX_CONSECUTIVE_FAILS
*                - Resets all counters if all profiles are exhausted
*              
*              If ENABLE_PROFILE_BLACKLIST is NOT defined:
*                - Simple round-robin: 1->2->3->1->2->3...
*                - All profiles tried equally regardless of previous failures
* 
* Parameters:
*   currentProfile - Current profile number (1-based)
* 
* Returns: Next profile number (1-based)
******************************************************************************/
uint8_t GetNextValidProfile(uint8_t currentProfile)
{
    uint8_t nextProfile;
    
    // DEBUG: Print CurrentProfile value being used
    LOGData(TAG_GPRS, "[DEBUG] GetNextValidProfile called with CurrentProfile = %d, VTSState.CurrentProfile = %d", currentProfile, VTSState.CurrentProfile);
    
    // Validate STK data
    if (!STKdata.is_valid || STKdata.profiles_supported == 0)
    {
        LOGData(TAG_GPRS, "STK data not valid, defaulting to profile 1");
        return 1;
    }
    
    #ifdef ENABLE_PROFILE_BLACKLIST
    // ===== BLACKLIST MODE: Intelligent selection based on failure history =====
    uint8_t attemptedProfiles = 0;
    uint8_t i;
    
    // Try to find a profile with failures < MAX_CONSECUTIVE_FAILS
    nextProfile = (currentProfile % STKdata.profiles_supported) + 1;  // Wrap around
    
    while (attemptedProfiles < STKdata.profiles_supported)
    {
        if (VTSState.ProfileFailCount[nextProfile] < MAX_CONSECUTIVE_FAILS)
        {
            LOGData(TAG_GPRS, "[BLACKLIST ON] Selected profile %d (fail count: %d)", 
                    nextProfile, VTSState.ProfileFailCount[nextProfile]);
            return nextProfile;
        }
        
        attemptedProfiles++;
        nextProfile = (nextProfile % STKdata.profiles_supported) + 1;  // Try next
    }
    
    // All profiles have high failure counts - reset all and start from profile 1
    LOGData(TAG_GPRS, "[BLACKLIST ON] All profiles exhausted, resetting failure counts");
    for (i = 1; i <= STKdata.profiles_supported; i++)
    {
        VTSState.ProfileFailCount[i] = 0;
    }
    UpdateStateInFlash();
    
    return 1;  // Start fresh from profile 1
    
    #else
    // ===== ROUND-ROBIN MODE: Simple rotation through all profiles =====
    nextProfile = (currentProfile % STKdata.profiles_supported) + 1;
    LOGData(TAG_GPRS, "[BLACKLIST OFF] Round-robin to profile %d", nextProfile);
    return nextProfile;
    
    #endif
}

uint8_t SwitchProfile(uint8_t num)
{
    
    char imsi[30] = {0};
    s32 ret;
    // At start of SwitchProfile()
    char prevIMSI[30] = {0};//added by vyshak - to hold the IMSI value read fresh at the start of this function for comparison, instead of using NetWork.IMSI which may not have been updated yet.
    RIL_SIM_GetIMSI(prevIMSI);  // ← fresh read, not NetWork.IMSI
    //end of addition 
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
            LOGData(TAG_GPRS, "*************************");
            LOGData(TAG_GPRS, "Sending Setup ENV Command %d: %s", i, STKdata.Setup[i].data);
            LOGData(TAG_GPRS, "*************************");
            ret = RIL_QSTKEnvelopeCommand(STKdata.Setup[i].data);
        }
        else
        {
            LOGData(TAG_GPRS, "*************************");
            LOGData(TAG_GPRS, "Sending Setup TR Command %d: %s", i, STKdata.Setup[i].data);
            LOGData(TAG_GPRS, "*************************");
            ret = RIL_QSTKTerminalResponse(STKdata.Setup[i].data);
        }
        
        if (ret != RIL_AT_SUCCESS)
        {
            LOGData(TAG_GPRS, "Setup command %d failed", i);
            return false;
        }
        else
            LOGData(TAG_GPRS, "Setup command Success");
        ThreadSleep(1500);
    }

    // Send profile command
    if(STKdata.Profile[num-1].type == 1)
    {
        LOGData(TAG_GPRS, "*************************");
        LOGData(TAG_GPRS, "Sending Profile ENV Command %d: %s", num, STKdata.Profile[num-1].data);
        LOGData(TAG_GPRS, "*************************");
        ret = RIL_QSTKEnvelopeCommand(STKdata.Profile[num-1].data);
    }
    else
    {
        LOGData(TAG_GPRS, "*************************");
        LOGData(TAG_GPRS, "Sending Profile TR Command %d: %s", num, STKdata.Profile[num-1].data);
        LOGData(TAG_GPRS, "*************************");
        ret = RIL_QSTKTerminalResponse(STKdata.Profile[num-1].data);
    }
    
    if (ret != RIL_AT_SUCCESS)
    {
        LOGData(TAG_GPRS, "Profile command failed");
        return false;
    }
    else
        LOGData(TAG_GPRS, "Profile command sent");
    ThreadSleep(1500);

    #ifdef SIMMAKE_COLORPLAST
    // COLORPLAST specific: Send confirmation command after profile selection
    if(VTSData.SIMMake == COLORPLAST)
    {
        LOGData(TAG_GPRS, "*************************");
        LOGData(TAG_GPRS, "Sending COLORPLAST Confirmation Command: %s", STK_COLORPLAST_CONFIRM);
        LOGData(TAG_GPRS, "*************************");
        ret = RIL_QSTKTerminalResponse(STK_COLORPLAST_CONFIRM);
        
        if (ret != RIL_AT_SUCCESS)
        {
            LOGData(TAG_GPRS, "COLORPLAST confirmation command failed");
            return false;
        }
        else
            LOGData(TAG_GPRS, "COLORPLAST confirmation sent");
        ThreadSleep(1500);

        // COLORPLAST: Send modem reset (AT+CFUN=1,1) to activate new profile
        LOGData(TAG_GPRS, "*************************");
        LOGData(TAG_GPRS, "Sending CFUN reset: AT+CFUN=1,1");
        LOGData(TAG_GPRS, "*************************");
        //SendCFUNAT(1, 1);  // Mode=1 (full functionality), Reset=1 (with reset)
        ThreadSleep(3000);  // Wait for modem to reinitialize
        LOGData(TAG_GPRS, "Waiting for IMSI change...");
    }
    else
    {
        LOGData(TAG_GPRS, "Waiting for IMSI change...");
        ThreadSleep(5000);
    }
    #else
    LOGData(TAG_GPRS, "Waiting for IMSI change...");
    ThreadSleep(5000);
    #endif

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

    if(Ql_strstr(imsi, prevIMSI))
    //if(Ql_strstr(imsi, NetWork.IMSI)) // commended by vyshak - this was comparing new IMSI with old IMSI stored in NetWork struct, which may not have been updated yet. Changed to compare with prevIMSI which is read fresh at the start of this function.
    {
        if(VTSState.CurrentProfile == 0)
        {
            // First time setup - this is success
            if (num >= 1 && num <= 4)
            {
                VTSState.ProfileFailCount[num] = 0; // Reset on first-time success
                //UpdateStateInFlash();    //commended by vyshak 
            }
            return true;
        }
        
        // IMSI didn't change - profile doesn't exist on this SIM
        LOGData(TAG_GPRS, "!!!!!!!!!!!! Unable to change profile (IMSI same - profile likely doesn't exist)");
        LOGData(TAG_GPRS, "Prev IMSI: %s", NetWork.IMSI);
        LOGData(TAG_GPRS, "New IMSI: %s", imsi);
        
        // Increment failure count - this profile doesn't exist on the SIM
        if (num >= 1 && num <= 4)
        {
            VTSState.ProfileFailCount[num]++;
            LOGData(TAG_GPRS, "Profile %d switch failed (non-existent profile), fail count: %d", 
                    num, VTSState.ProfileFailCount[num]);
            UpdateStateInFlash();
        }
        return false;
    }

    LOGData(TAG_GPRS, "Profile Successfully Changed");
    LOGData(TAG_GPRS, "Prev IMSI: %s", NetWork.IMSI);
    LOGData(TAG_GPRS, "New IMSI: %s", imsi);
    
    // Success! Reset failure count for this profile
    if (num >= 1 && num <= 4)
    {
        VTSState.ProfileFailCount[num] = 0;
        LOGData(TAG_GPRS, "Profile %d switch successful, resetting fail count", num);
        UpdateStateInFlash();
    }
    
    // Update NetWork.IMSI with new IMSI
    Ql_strncpy(NetWork.IMSI, imsi, sizeof(NetWork.IMSI) - 1);
    NetWork.IMSI[sizeof(NetWork.IMSI) - 1] = '\0';
    
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
            LOGData(TAG_GPRS,"[DEBUG] GetSimState: VTSState.CurrentProfile = %d", VTSState.CurrentProfile);
           
            GSM.GSMState = SIM_DETECTED;
        }
        ret = RIL_SIM_GetCCID(sim_id);
        if(ret==RIL_AT_SUCCESS)
        {
            #ifdef VIRTUAL_SIMCCID
            Ql_strcpy(sim_id,VCID);
            #endif
            Ql_strcpy(NetWork.SIMNo,sim_id);
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

static void CheckprfReq(void)
{
    uint8_t tries = 3;
    
    // Check if GSM is in valid state
    if(GSM.GSMState < SIM_DETECTED)
    {
        return;
    }

    if(IsMotaProcessing || IsFotaProcessing)
    {
        return;
    }

    // Set default profile if none is set
    if(VTSState.CurrentProfile == NONE)
    {
        prfReq = VTSData.DefProfile;
    }

    

    
    // Process profile switch request if any
    if(prfReq != NONE)
    {
        if(!STKdata.is_valid)
        {
            LOGData(TAG_GPRS, "STK Data not valid, cannot switch profile");
            prfReq = NONE;
            return;
        }
        if(!IsSMSInit)
        {
            LOGData(TAG_GPRS, "SMS not initialized for STK, waiting before profile switch...");
            //return;
        }
        LOGData(TAG_GPRS, "Profile switch requested to %d", prfReq);
        LOGData(TAG_GPRS, "[DEBUG] CheckprfReq: VTSState.CurrentProfile = %d, Switching to prfReq = %d", VTSState.CurrentProfile, prfReq);
        
        while(!SwitchProfile(prfReq))
        {
            tries--;
            if(!tries)
            {
                LOGData(TAG_GPRS, "*********************");
                LOGData(TAG_GPRS, "UNABLE TO SWITCH TO REQUESTED SIM PROFILE");
                LOGData(TAG_GPRS, "*********************");
                //ADDED BY VYSHAK FOR ONE PROFILE FALLBACK - Save the failed profile to flash so that it can be skipped in future attempts
                VTSState.CurrentProfile = prfReq;  // save failed profile to flash
                UpdateStateInFlash();
                //prfReq = NONE;  // reset request to avoid repeated attempts in the same boot cycle
                //END OF ADDITION - SAVING FAILED PROFILE TO FLASH// CAN REMOVE THIS IN FUTURE ONCE PROFILES ARE STABILIZED
                // Reset module using RIL
                Ql_Reset(0);  
                ThreadSleep(2000);
                return;
            }
            LOGData(TAG_GPRS, "Retrying profile switch, tries left: %d", tries);
            ThreadSleep(1000);
        }
        LOGData(TAG_GPRS, "Profile switch to %d successful", prfReq);
        // Update current profile and save state
        VTSState.CurrentProfile = prfReq;
        UpdateStateInFlash();
        prfReq = NONE;
        PrfChanged = 1;

        // Reset module after successful profile switch
        LOGData(TAG_GPRS, "Profile switch successful, initiating reset");
        ThreadSleep(1000);
        Ql_Reset(0);    
        ThreadSleep(2000);
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
    LOGData(TAG_GPRS,"CSQ is %d \r\n",GSM.SignalStrength);

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
            ThreadSleep(100);
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
        
        
        GSM.IsRegDenied = 1;  // Set flag for LED Manager to detect
        // if(++VTSState.RegDeniedCount > 0)
        // {
           
           
        //     VTSState.RegDeniedCount = 0;
            
        //     UpdateStateInFlash();
            SendRS232Response("****REGISTRATION DENIED, Switching Profile ****\n");
            uint8_t newpf = GetNextValidProfile(VTSState.CurrentProfile);
            prfReq = newpf;
            CheckprfReq();  // Process the profile switch request
        // }
        // else
        // {
        //     UpdateStateInFlash();
        // }
        // SendRS232Response("****REGISTRATION DENIED, restarting*****\n");
        // // LED Manager will show registration denied state automatically
        // ThreadSleep(4000);
        // Ql_Reset(0);
        // ThreadSleep(4000);
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

    LOGData(TAG_GPRS, "SetupAutoTimesync: Starting setup for auto time synchronization");

    // --- Check AT+QNITZ status ---
    Ql_memset(responseBuffer, 0, sizeof(responseBuffer));
    ret = SendATCommandSimple("AT+QNITZ?\r\n", responseBuffer, sizeof(responseBuffer), 1000);
    if (ret == RIL_ATRSP_SUCCESS)
    {
        LOGData(TAG_GPRS, "SetupAutoTimesync: AT+QNITZ query successful, response: %s", responseBuffer);
        if (!Ql_strstr(responseBuffer, "+QNITZ: 1"))
        {
            LOGData(TAG_GPRS, "SetupAutoTimesync: AT+QNITZ is not set to 1, updating...");
            ret = SendATCommandSimple("AT+QNITZ=1\r\n", responseBuffer, sizeof(responseBuffer), 1000);
            if (ret != RIL_ATRSP_SUCCESS)
            {
                LOGData(TAG_GPRS, "SetupAutoTimesync: Failed to set AT+QNITZ to 1, error: %d", ret);
                return ret;
            }
            isResetRequired = TRUE;
        }
    }
    else
    {
        LOGData(TAG_GPRS, "SetupAutoTimesync: Failed to query AT+QNITZ, error: %d", ret);
        return ret;
    }

    // --- Check AT+CTZU status ---
    Ql_memset(responseBuffer, 0, sizeof(responseBuffer));
    ret = SendATCommandSimple("AT+CTZU?\r\n", responseBuffer, sizeof(responseBuffer), 1000);
    if (ret == RIL_ATRSP_SUCCESS)
    {
        LOGData(TAG_GPRS, "SetupAutoTimesync: AT+CTZU query successful, response: %s", responseBuffer);
        if (!Ql_strstr(responseBuffer, "+CTZU: 2"))
        {
            LOGData(TAG_GPRS, "SetupAutoTimesync: AT+CTZU is not set to 2, updating...");
            ret = SendATCommandSimple("AT+CTZU=2\r\n", responseBuffer, sizeof(responseBuffer), 1000);
            if (ret != RIL_ATRSP_SUCCESS)
            {
                LOGData(TAG_GPRS, "SetupAutoTimesync: Failed to set AT+CTZU to 2, error: %d", ret);
                return ret;
            }
            isResetRequired = TRUE;
        }
    }
    else
    {
        LOGData(TAG_GPRS, "SetupAutoTimesync: Failed to query AT+CTZU, error: %d", ret);
        return ret;
    }

    // --- Reset module if required ---
    if (isResetRequired)
    {
        LOGData(TAG_GPRS, "SetupAutoTimesync: Reset required, sending reset command...");
        Ql_sprintf(strAT, "AT+CFUN=1,1\r\n");
        ret = SendATCommandSimple(strAT, responseBuffer, sizeof(responseBuffer), 2000);
        if (ret != RIL_ATRSP_SUCCESS)
        {
            LOGData(TAG_GPRS, "SetupAutoTimesync: Failed to reset module, error: %d", ret);
            return ret;
        }
    }

    LOGData(TAG_GPRS, "SetupAutoTimesync: Auto time synchronization setup completed successfully");
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
        /*
        if (VTSData.CustomImei.IsEnable)
        {
            Ql_strcpy(NetWork.IMEI, VTSData.CustomImei.Imei);
            LOGData(TAG_GPRS, "Device Custom IMEI: %s", NetWork.IMEI);
        }
        else
        {
        */
            #ifdef VIRTUAL_IMEI
            Ql_strcpy(NetWork.IMEI, VIMEI);
            LOGData(TAG_GPRS, "Device VIMEI: %s", NetWork.IMEI);
            #else
            Ql_strcpy(NetWork.IMEI, imei);
            LOGData(TAG_GPRS, "Device IMEI: %s", NetWork.IMEI);
            #endif
        /*
        }
        */
    }
    else
    {
        /*
        if (VTSData.CustomImei.IsEnable)
        {
            Ql_strcpy(NetWork.IMEI, VTSData.CustomImei.Imei);
            LOGData(TAG_GPRS, "Device Custom IMEI (Fallback): %s", NetWork.IMEI);
        }
        else
        {
        */
            LOGData(TAG_GPRS, "Failed to get Device IMEI, error: %d", ret);
        /*
        }
        */
    }
}



void GPRSThreadEntry(s32 taskId)
{
    InitGPRSThread(taskId);
    ThreadSleep(500);
    GetDeviceIMEI();
    EnableSTK();
    while(1)
    {
        while(SleepConfig.IsEnabled)
        {
            ThreadSleep(2000);
            // LED Manager will automatically show sleep state
        }
        switch (GSM.GSMState)
        {
            case SIM_NOT_DETECTED:
                LOGData(TAG_GPRS,"STATE -> SIM NOT DETECTED");
                GetSimState();
                break;
            case SIM_DETECTED:
                LOGData(TAG_GPRS,"STATE -> SIM DETECTED");
                ProcessREGISTER();
                break;
            case GPRS_INIT:
                LOGData(TAG_GPRS,"STATE -> GPRS INIT");
                ActivateGPRS();
                break;
            case GPRS_ACTIVE:
                LOGData(TAG_GPRS,"STATE -> GPRS ACTIVE");
                //CheckGPRSState();
                //RecheckRegistoration();
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