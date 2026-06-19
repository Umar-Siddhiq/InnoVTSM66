#include "GPRS.h"
//#include "Hardware.h"
#include "VTS.h"
#include "nwy_test_cli_func_def.h"
GSM_Typedef GSM;
NET_Typedef NetWork;
_RTC CurrentDateTime;
static uint8_t DataCallConnectedFlag = 0;  // Flag to track if StatChange callback confirmed connection
//#define _2G_PRIORITY
#define STK_ENB
STKDatatypedef STKdata;
/**
 * Whether Time is aquired by network or not
*/
uint8_t IsTimeSet;

uint8_t DayTable[] =
{
    31,28,31,30,31,30,31,31,30,31,30,31
};

char sIMEI[30];

static void ExtractDigitString(const char* in, char* out, unsigned int out_sz)
{
    unsigned int j = 0;
    if(out_sz == 0)
        return;
    if(in == NULL)
    {
        out[0] = '\0';
        return;
    }
    for(unsigned int k = 0; in[k] != '\0' && (j + 1) < out_sz; k++)
    {
        if(in[k] >= '0' && in[k] <= '9')
            out[j++] = in[k];
    }
    out[j] = '\0';
}


void LoadsensoriseSTKData(STKDatatypedef *stk)
{
    stk->setup_cmd_count = STK_SENS_SETUP_COUNT;
    strcpy(stk->Setup[0].data,STK_SENS_MENU);
    stk->Setup[0].type=0;
    strcpy(stk->Setup[1].data,STK_SENS_ITEM);
    stk->Setup[1].type=1;
    strcpy(stk->Setup[2].data,STK_SENS_NETWORK);
    stk->Setup[2].type=0;
    stk->profiles_supported=2;
    strcpy(stk->Profile[0].data,STK_SENS_PRIMARY);
    strcpy(stk->Profile[1].data,STK_SENS_SECONDARY);
    stk->is_valid=1;
} 

void LoadTaisysSTKData(STKDatatypedef *stk)
{
    stk->setup_cmd_count = STK_TAISYS_SETUP_COUNT;
    strcpy(stk->Setup[0].data,STK_TAISYS_MENU);
    stk->Setup[0].type=0;
    strcpy(stk->Setup[1].data,STK_TAISYS_ITEM);
    stk->Setup[1].type=1;
    #ifdef SIMMAKE_TACHNOJACKS
    stk->profiles_supported=3;
    #elif defined SIMMAKE_IDEMIA_3P
    stk->profiles_supported=3;
    #else
    stk->profiles_supported=2;
    #endif
    strcpy(stk->Profile[0].data,STK_TAISYS_PRIMARY);
    strcpy(stk->Profile[1].data,STK_TAISYS_SECONDARY);
    strcpy(stk->Profile[2].data,STK_TAISYS_THIRD);
    stk->is_valid=1;
} 

void LoadTaisysGNDData(STKDatatypedef *stk)
{
    stk->setup_cmd_count = STK_GND_SETUP_COUNT;
    strcpy(stk->Setup[0].data,STK_GND_MENU);
    stk->Setup[0].type=0;
    strcpy(stk->Setup[1].data,STK_GND_ITEM);
    stk->Setup[1].type=1;
    strcpy(stk->Setup[2].data,STK_GND_NETWORK);
    stk->Setup[2].type=0;
    stk->profiles_supported=3;
    strcpy(stk->Profile[0].data,STK_GND_PRIMARY);
    strcpy(stk->Profile[1].data,STK_GND_SECONDARY);
    strcpy(stk->Profile[2].data,STK_GND_THIRD);
    stk->is_valid=1;
} 

uint8_t SendAtCmd(char* str, char* resp, char* grep)
{
    nwy_at_info_t at_cmd;
    int ret;
    memset(&at_cmd, 0, sizeof(nwy_at_info_t));
    memcpy(at_cmd.at_command, str, strlen(str));
    nwy_dbg_log("AT Sending : %s",at_cmd.at_command);
    at_cmd.length = strlen(at_cmd.at_command);
    ret = nwy_sdk_at_cmd_send(&at_cmd,resp,100,NWY_AT_TIMEOUT_DEFAULT);
    if(ret == NWY_AT_GET_RESP_TIMEOUT)
    {   
        nwy_dbg_log("AT Reply TIMEOUT!");
        return 0;
    }
    else if(ret != NWY_SUCCESS)
    {
        nwy_dbg_log("AT CMD ERROR!");
        return 0;
    }
    if(strstr(resp,grep))
    {
        nwy_dbg_log("AT Good Reply : %s",resp);
        return 1;
    }
    nwy_dbg_log("AT unrec Reply : %s",resp);
    return 0;

}

uint8_t GetSimState(void)
{
    char resp[100] = {0};
    if(!SendAtCmd("AT+CPIN?\r\n",resp,":READY"))
        return 0;
    // if(nwy_sim_get_card_status(NWY_SIM_ID_SLOT_1)!=NWY_SIM_STATUS_READY)
    // {
    //     nwy_sim_status state=0;
    //     nwy_sleep(200);
    //     state = nwy_sim_get_card_status(NWY_SIM_ID_SLOT_1);
    //     if(state!=NWY_SIM_STATUS_READY)
    //     {
    //         nwy_dbg_log("Sim State err: %i",state);
    //         return 0;
    //     }
    // }

    return 1;
}



#ifdef STK_ENB
uint8_t EnableSTK(void)
{
    char resp[100] = {0}, *fn;
    //EnableVat();
    if(VTSData.SIMMake == SENSORISE)
        LoadsensoriseSTKData(&STKdata);
    else if(VTSData.SIMMake == TAISYS)
        LoadTaisysSTKData(&STKdata);
    else if(VTSData.SIMMake == GnD)
        LoadTaisysGNDData(&STKdata);
    else
    {
        nwy_dbg_log("Invalid SIM Make");
        return 0;
    }

    if(!SendAtCmd("AT+STKEN?\r\n",resp,"EN:"))
    {
        nwy_dbg_log("STK Reply Error");
        return 0;
    }
    fn = strstr(resp,"EN:");
    if(!fn)
    {
        nwy_dbg_log("STK Reply Error");
        return 0;
    }
    if(fn[4] == '1')
    {
        nwy_dbg_log("STK Enabled ");
        //nwy_power_off(2);
        return 1;
    }
    return 0;
}

/**
 * Get next profile to try based on failure history
 * Skips profiles that have failed too many times consecutively
 * @param currentProfile Current active profile number
 * @return Next profile number to try (1-3), or 0 if all profiles have failed
 */
uint8_t GetNextValidProfile(uint8_t currentProfile)
{
    uint8_t nextProfile = currentProfile;
    uint8_t attempts = 0;
    const uint8_t MAX_CONSECUTIVE_FAILS = 3; // Skip profile after 3 consecutive failures
    
    // Try to find a valid profile, max attempts = profiles_supported
    while(attempts < STKdata.profiles_supported)
    {
        // Calculate next profile with wrap-around
        nextProfile++;
        if(nextProfile > STKdata.profiles_supported)
            nextProfile = 1;
            
        // Check if this profile has failed too many times
        if(VTSState.ProfileFailCount[nextProfile] < MAX_CONSECUTIVE_FAILS)
        {
            nwy_dbg_log("Selected profile %d (fail count: %d)", nextProfile, VTSState.ProfileFailCount[nextProfile]);
            return nextProfile;
        }
        else
        {
            nwy_dbg_log("Skipping profile %d (failed %d times consecutively)", 
                        nextProfile, VTSState.ProfileFailCount[nextProfile]);
        }
        
        attempts++;
    }
    
    // All profiles have failed too many times, reset counters and try profile 1
    nwy_dbg_log("All profiles have high fail counts, resetting counters...");
    for(int i = 1; i <= STKdata.profiles_supported; i++)
        VTSState.ProfileFailCount[i] = 0;
    
    // Save the reset counters immediately - important for persistence
    UpdateStateInFlash();
    nwy_dbg_log("All profile fail counts reset and saved to flash");
    
    return 1; // Start fresh from profile 1
}

uint8_t SwitchProfile(uint8_t num)
{
    char resp[256]= {0};  // Increased buffer size for safety (FIX #1)
    char newimsi[64]={0}; // Digit-only IMSI after switch
    char previmsi[64]={0}; // Digit-only IMSI before switch
    char cmd[256];        // Increased cmd buffer as well
    //uint8_t tries=5;
    int i;

    if(!STKdata.is_valid)
    {
        nwy_dbg_log("STK Data not valid!!!");
        return 0;
    }
    if(num > STKdata.profiles_supported)
    {
        nwy_dbg_log("Profile Num %d not supported, max %d",num,STKdata.profiles_supported);
        return 0;
    }
    // Read current IMSI first (baseline for comparison)
    {
        char imsi_resp[256] = {0};
        if(!SendAtCmd("AT+CIMI\r\n", imsi_resp, "OK"))
        {
            nwy_dbg_log("Profile %d switch failed (pre-IMSI read error)", num);
            if(num >= 1 && num <= 3)
            {
                VTSState.ProfileFailCount[num]++;
                nwy_dbg_log("Profile %d switch failed, fail count: %d", num, VTSState.ProfileFailCount[num]);
            }
            return 0;
        }
            ExtractDigitString(imsi_resp, previmsi, sizeof(previmsi));
        nwy_dbg_log("Pre-switch IMSI: %s", previmsi);
    }

   
    
    //DecodeATCommand("&#ATC,AT+CIMI");
    nwy_sleep(100);
    stkCb=0;
    // while(1)
    // {
    if(VTSData.SIMMake != GnD){

        sprintf(cmd,"AT+CRSM=214,28539,0,0,12,\"FFFFFFFFFFFFFFFFFFFFFFFF\"\r\n");
        SendAtCmd(cmd,resp,"RSM:144");
        nwy_sleep(1500);

        sprintf(cmd,"AT+STKTR=\"8103010300820281828301008402011E\"\r\n");
        SendAtCmd(cmd,resp,"OK");
        nwy_sleep(1500);
    }   
    else
    {
        // sprintf(cmd,"AT+CSIM=14,\"00A4000C023F00\"\r\n");
        // SendAtCmd(cmd,resp,"SIM");
        // nwy_sleep(1500);


        // sprintf(cmd,"AT+CSIM=18,\"00A4080C0444445542\"\r\n");
        // SendAtCmd(cmd,resp,"CSIM");
        // nwy_sleep(1500);


        // sprintf(cmd,"AT+CSIM=90,\"00DC020428534D534332FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFFFFFFFFFF0791194924909979FFFFFFFFFFFFFF\"\r\n");
        // SendAtCmd(cmd,resp,"CSIM");
        // nwy_sleep(1500);


        sprintf(cmd,"AT+STKTR=\"8103010300820281828301008402011E\"\r\n");
        SendAtCmd(cmd,resp,"OK");
        nwy_sleep(2500);

        sprintf(cmd,"AT+STKTR=\"8103010300820281828301008402011E\"\r\n");
        SendAtCmd(cmd,resp,"OK");
        nwy_sleep(2500);
    }

       


    for(i=0;i<STKdata.setup_cmd_count;i++)
    {
        if(STKdata.Setup[i].type==1)
        {
            sprintf(cmd,"AT+STKENV=\"%s\"\r\n",STKdata.Setup[i].data);
            SendAtCmd(cmd,resp," ");
            nwy_sleep(2000);
        }
        else
        {
            sprintf(cmd,"AT+STKTR=\"%s\"\r\n",STKdata.Setup[i].data);
            SendAtCmd(cmd,resp," ");
            nwy_sleep(2000);
        }
    }
    stkCb=0;
    if(STKdata.Profile[num-1].type==1)
    {
        sprintf(cmd,"AT+STKENV=\"%s\"\r\n",STKdata.Profile[num-1].data);
        SendAtCmd(cmd,resp," ");
        nwy_sleep(2000);
    }
    else
    {
        sprintf(cmd,"AT+STKTR=\"%s\"\r\n",STKdata.Profile[num-1].data);
        SendAtCmd(cmd,resp," ");
        nwy_sleep(2000);
    }
   
    nwy_sleep(2000);
    
    // Clear response buffer before critical IMSI read (FIX #4)
    memset(resp, 0, sizeof(resp));
    
    if(!SendAtCmd("AT+CIMI\r\n",resp,"OK"))
    {
        // Failed to get IMSI - increment fail count for this profile
        if(num >= 1 && num <= 3)
        {
            VTSState.ProfileFailCount[num]++;
            nwy_dbg_log("Profile %d switch failed (IMSI read error), fail count: %d", 
                        num, VTSState.ProfileFailCount[num]);
        }
        return 0;
    }

    // Extract digit-only IMSI from response
    ExtractDigitString(resp, newimsi, sizeof(newimsi));
    nwy_dbg_log("Post-switch IMSI: %s", newimsi);

    if(previmsi[0] != '\0' && newimsi[0] != '\0' && (strcmp(previmsi, newimsi) == 0))
    {
        if(VTSState.CurrentProfile == 0)
        {
            // First time setup, this is success
            if(num >= 1 && num <= 3)
                VTSState.ProfileFailCount[num] = 0; // Reset fail count on success
            return 1;
        }
        // IMSI didn't change - profile switch failed (likely non-existent profile)
        nwy_dbg_log("Unable to change profile (imsi same)");
        nwy_dbg_log("Prev IMSI: %s", previmsi);
        nwy_dbg_log("new IMSI reading : %s", newimsi);
        
        // Increment failure count for this profile
        if(num >= 1 && num <= 3)
        {
            VTSState.ProfileFailCount[num]++;
            nwy_dbg_log("Profile %d switch failed, fail count: %d", num, VTSState.ProfileFailCount[num]);
        }
        return 0;
    }

    // Update global IMSI on successful change
    if(newimsi[0] != '\0')
    {
        strncpy(NetWork.IMSI, newimsi, sizeof(NetWork.IMSI) - 1);
        NetWork.IMSI[sizeof(NetWork.IMSI) - 1] = '\0';
    }


    nwy_dbg_log("Profile Successfully Changed to %d", num);
    
    // Success! Reset fail count for this profile and update last successful profile
    if(num >= 1 && num <= 3)
    {
        VTSState.ProfileFailCount[num] = 0; // Reset this profile's fail count
        nwy_dbg_log("Profile %d switch successful, resetting fail count", num);
    }
    
    return 1;
}
#endif
uint8_t NOSimCount;
uint8_t STKOK;
void ProcessSim(void)
{

    int size=0;
    #ifdef STK_ENB
    if(!STKOK)
        STKOK = EnableSTK();
    #endif
    if(!GetSimState())
    {
        GSM.GSMState = SIM_NOT_DETECTED;
        if(++NOSimCount > 30)
        {
            // ZigTestMode: Prevent automatic restart during manufacturing test
            if(ZigTestMode)
            {
                nwy_dbg_log("ZigTestMode Active - Automatic restart prevented (SIM Detection Timeout)");
                NOSimCount = 30; // Keep at threshold to prevent overflow
            }
            else
            {
                nwy_dbg_log("SIM Detection Timeout, Restarting...");
                nwy_power_off(2);
                nwy_sleep(5000);
            }
            return;
        }
        return;
    }

        
    size = sizeof(NetWork.SIMNo);
    if(nwy_sim_get_iccid(NWY_SIM_ID_SLOT_1,NetWork.SIMNo,size)!=NWY_RES_OK)
        return;

    size = sizeof(NetWork.IMSI);
    if(nwy_sim_get_imsi(NWY_SIM_ID_SLOT_1,NetWork.IMSI,size)!=NWY_RES_OK)
        return;

    
    nwy_dbg_log("Sim Detected!");
    nwy_dbg_log("IMEI - %s\r\n",NetWork.IMEI);        
    nwy_dbg_log("ICCID - %s\r\n",NetWork.SIMNo);
    nwy_dbg_log("IMSI - %s\r\n",NetWork.IMSI);
    NOSimCount=0;
    #ifdef ENABLE_RS232_FAST
    sprintf(RSSend,"SIM DETECTED IMSI - %s\n",NetWork.IMSI);
    //SendRS232String(RSSend);
    #endif

    GSM.GSMState = SIM_DETECTED;
    SendGyroSettingPacket();
    SetStatLED(10,2);
}

#ifdef DBM_IN_CSQ
#define min_rssi -120
#define max_rssi -50
#define max_csq  31
#define min_csq  0

static int rssi_to_csq(int rssi) {
    // Ensure that rssi is within the specified range
    rssi = (rssi < min_rssi) ? min_rssi : (rssi > max_rssi) ? max_rssi : rssi;

    // Linear scaling formula
    int csq = (int)(((float)(rssi - min_rssi) / (max_rssi - min_rssi)) * (max_csq - min_csq) + min_csq + 0.5);

    return csq;
}
#endif

static void CellInfo_cb (void *infor, int num)
{
    int i;
    nwy_scanned_locator_info_t *cellinfo = (nwy_scanned_locator_info_t *)infor;
    nwy_dbg_log("GPRS ncell Callback!!!rat : %d\r\n", cellinfo->curr_rat);
    if (cellinfo->num == 0)
    {
        nwy_dbg_log("GPRS ncell scan FAILED!!!\r\n");
        return;
    }
    if (cellinfo->curr_rat == 2)
    {

        GSM.MCC =       cellinfo->scell_info.gsm_scell_info.mcc;
        GSM.MNC =        cellinfo->scell_info.gsm_scell_info.mnc;
        sprintf(GSM.LAC,"%04X",cellinfo->scell_info.gsm_scell_info.Lac);
        sprintf(GSM.CellID,"%04X",cellinfo->scell_info.gsm_scell_info.Cellid);
        nwy_dbg_log("nCell Count : %d",cellinfo->ncell.gsm_ncell.gsm_ncell_info.ncell_num);
        for(i = 0;i < cellinfo->ncell.gsm_ncell.gsm_ncell_info.ncell_num; i++)
        {
            if(i<4)
            {
                IsNeigh=1;
                GSM.NeigbourCell[i].mcc = cellinfo->ncell.gsm_ncell.gsm_ncell_info.gsmsNcell[i].mcc;
                GSM.NeigbourCell[i].mnc = cellinfo->ncell.gsm_ncell.gsm_ncell_info.gsmsNcell[i].mnc;
                sprintf(GSM.NeigbourCell[i].CellID,"%04X",cellinfo->ncell.gsm_ncell.gsm_ncell_info.gsmsNcell[i].Cellid);
                sprintf(GSM.NeigbourCell[i].LAC,"%04X",cellinfo->ncell.gsm_ncell.gsm_ncell_info.gsmsNcell[i].lac);
                #ifdef DBM_IN_CSQ
                int raw_rssi = cellinfo->ncell.gsm_ncell.gsm_ncell_info.gsmsNcell[i].rssi;
                // If RSSI is positive, negate it (convert from positive dBm format to negative)
                int rssi_dbm = (raw_rssi > 0) ? -raw_rssi : raw_rssi;
                sprintf(GSM.NeigbourCell[i].CellDB,"%i",rssi_to_csq(rssi_dbm));
                #else
                sprintf(GSM.NeigbourCell[i].CellDB,"%i",cellinfo->ncell.gsm_ncell.gsm_ncell_info.gsmsNcell[i].rssi);
                #endif
                nwy_dbg_log("N%d CID - %s,LAC - %s, CDb - %s\r\n",i,GSM.NeigbourCell[i].CellID,GSM.NeigbourCell[i].LAC,GSM.NeigbourCell[i].CellDB);
            }
        }
    }
    if(cellinfo->curr_rat == 3)
    {
        GSM.MCC =       cellinfo->scell_info.umts_scell_info.mcc;
        GSM.MNC =       cellinfo->scell_info.umts_scell_info.mnc;
        sprintf(GSM.LAC,"%04X",cellinfo->scell_info.umts_scell_info.Lac);
        sprintf(GSM.CellID,"%04X",cellinfo->scell_info.umts_scell_info.ci);
        for(i = 0;i < cellinfo->ncell.umts_ncell.umtsInterRatinfo.interRatGsm.ncell_num; i++)
        {
            if(i<4)
            {
                IsNeigh=1;
                GSM.NeigbourCell[i].mcc = cellinfo->ncell.umts_ncell.umtsInterRatinfo.interRatGsm.gsmsNcell[i].mcc;
                GSM.NeigbourCell[i].mnc = cellinfo->ncell.umts_ncell.umtsInterRatinfo.interRatGsm.gsmsNcell[i].mnc;
                sprintf(GSM.NeigbourCell[i].CellID,"%04X",cellinfo->ncell.umts_ncell.umtsInterRatinfo.interRatGsm.gsmsNcell[i].Cellid);
                sprintf(GSM.NeigbourCell[i].LAC,"%04X",cellinfo->ncell.umts_ncell.umtsInterRatinfo.interRatGsm.gsmsNcell[i].lac);
                #ifdef DBM_IN_CSQ
                int raw_rssi = cellinfo->ncell.umts_ncell.umtsInterRatinfo.interRatGsm.gsmsNcell[i].rssi;
                // If RSSI is positive, negate it (convert from positive dBm format to negative)
                int rssi_dbm = (raw_rssi > 0) ? -raw_rssi : raw_rssi;
                sprintf(GSM.NeigbourCell[i].CellDB,"%i",rssi_to_csq(rssi_dbm));
                #else
                int raw_rssi = cellinfo->ncell.umts_ncell.umtsInterRatinfo.interRatGsm.gsmsNcell[i].rssi;
                // if RSSI is positive, negate it (convert from positive dBm format to negative)
                int rssi_dbm = (raw_rssi > 0) ? -raw_rssi : raw_rssi;
                sprintf(GSM.NeigbourCell[i].CellDB,"%i",rssi_dbm);
                #endif
                nwy_dbg_log("N%d CID - %s,LAC - %s, CDb - %s\r\n",GSM.NeigbourCell[i].CellID,GSM.NeigbourCell[i].LAC,GSM.NeigbourCell[i].CellDB);
            }
        }

    }
}

void GetSignal(void)
{
    if(GSM.GSMState<SIM_DETECTED)
    {
        GSM.SignalStrength=0;
        return;
    }
    nwy_nw_get_signal_csq(&GSM.SignalStrength);
    if(GSM.SignalStrength>40)
        GSM.SignalStrength=0;
    if(VTSData.ServerData.Url1[0] == 1)
        GSM.SignalStrength/=3;
    
}

uint8_t GetNeighbourCells(void)
{

    nwy_dbg_log("GPRS ncell scan Start!!!\r\n");
    if (NWY_SUCCESS != nwy_nw_get_neighborLocatorInfo(CellInfo_cb))
    {
      nwy_dbg_log("GPRS ncell scan FAILED!!!\r\n");
      return 0;
    }
    nwy_dbg_log("****GPRS ncell scan SUCCESS*****\r\n");
    return 1;

}

uint8_t GetRegisterStat(void)
{
    nwy_nw_regs_info_type_t reg_info;
    if(NWY_RES_OK != nwy_nw_get_register_info(&reg_info))
        return 0;
    if(reg_info.data_regs.regs_state == NWY_NW_SERVICE_NONE)
        return 0;

    return 1;

}

uint8_t RegDenytemp;
void ProcessREGISTER(void)
{
    //nwy_nw_regs_info_type_t reg_info;
    nwy_nw_operator_name_t opname;
    int rfstate,cs;
    if(!GetSimState())
    {
        GSM.GSMState = SIM_NOT_DETECTED;
        return;
    }
    nwy_nw_get_radio_st(&rfstate);
    if(!rfstate)
        nwy_nw_set_radio_st(1);

    nwy_dbg_log("Current Sim Profile : %d",VTSState.CurrentProfile);
    nwy_nw_get_signal_csq(&GSM.SignalStrength);
    nwy_dbg_log("CSQ is %d \r\n",GSM.SignalStrength);

    #ifdef ENABLE_RS232_FAST
    sprintf(RSSend,"CSQ - %d\n",GSM.SignalStrength);
    //SendRS232String(RSSend);
    #endif

    // #ifdef _2G_PRIORITY
    //     nwy_nw_set_network_mode(2);
    // #else
    //     nwy_nw_set_network_mode(4);
    // #endif

    if(nwy_nw_get_cs_st(&cs)!=NWY_SUCCESS)
    {
        nwy_dbg_log("Unable to get cs State!!!");
        goto REGISTER_FAIL;
    }
    nwy_dbg_log("Network CS State: %d",cs);

    if(cs == 3)
    {
        if(RegDenytemp < 10)
        {
            RegDenytemp++;
            nwy_dbg_log("Network Showing Reg Denied, waiting for resolve...");
            nwy_sleep(500);
            goto REGISTER_FAIL;
        }
         // ZigTestMode: Prevent automatic restart during manufacturing test
        if(ZigTestMode)
        {
            nwy_dbg_log("ZigTestMode Active - Automatic restart prevented (Registration Denied)");
            RegDenytemp=0;
            goto REGISTER_FAIL;
        }
        if(prfReq != NONE)
        {
            nwy_dbg_log("Profile switch already requested (%d), skipping duplicate registration denied handling", prfReq);
            RegDenytemp=0;
            goto REGISTER_FAIL;
        }
   

        nwy_dbg_log("REGISTRATION DENIED !!!!, restarting...");
        #ifndef AUTO_PROFILESWITCH_DISABLE
        // if(++VTSState.RegDeniedCount > 3)
        // {
        //     VTSState.RegDeniedCount=0;
            
            // FIX #2: Check if profile request already pending to avoid race condition
        if(prfReq != NONE)
        {
            nwy_dbg_log("Profile switch already requested (%d), skipping duplicate request", prfReq);
        }
        else
        {
            // Use smart profile selection instead of simple increment
            uint8_t newpf = GetNextValidProfile(VTSState.CurrentProfile);
            
            if(newpf == 0)
            {
                nwy_dbg_log("No valid profiles available, restarting with profile 1...");
                newpf = 1;
            }

            prfReq= newpf;
            nwy_dbg_log("Switching from profile %d to profile %d due to registration denied", 
                        VTSState.CurrentProfile, newpf);
            
            // CRITICAL: Save state IMMEDIATELY before triggering reset
            // Main thread will handle prfReq but we may reset before it saves
            UpdateStateInFlash();
            nwy_dbg_log("State saved with RegDeniedCount reset and new profile request");
        }

        nwy_sleep(15000);
        // }
        // else
        // {
        //     // Save incremented RegDeniedCount
        //     UpdateStateInFlash();
        // }
        #endif
        SetStatLED(2,1);
        nwy_sleep(4000);
        nwy_power_off(2);
        nwy_sleep(5000);  // FIX #5: Increased delay for proper shutdown
    }
    if(cs != 1 && cs != 5)
        goto REGISTER_FAIL;

    RegDenytemp=0;
    if(VTSState.RegDeniedCount>0)
    {
        VTSState.RegDeniedCount=0;
        UpdateStateInFlash();
    }
    // if(NWY_RES_OK != nwy_nw_get_register_info(&reg_info))
    //     goto REGISTER_FAIL;
    // if(reg_info.data_regs_valid!=1)
    //     goto REGISTER_FAIL;


    // nwy_dbg_log("Network Data Reg state: %d\r\n"
    //             "Network Data Roam state: %d\r\n"
    //             "Network Data Radio Tech: %d\r\n",
    //             reg_info.data_regs.regs_state,
    //             reg_info.data_regs.roam_state,
    //             reg_info.data_regs.radio_tech);

    

    // if(reg_info.data_regs.regs_state == NWY_NW_SERVICE_NONE)
    //     goto REGISTER_FAIL;

    if (NWY_SUCCESS != nwy_nw_get_operator_name(&opname))
    {
        nwy_dbg_log("Unable to get SPN Name !!!");
        goto REGISTER_FAIL;
    }


    GSM.MCC = atoi(opname.mcc);
    GSM.MNC = atoi(opname.mnc);
    
    
    if(VTSData.ServerData.Url1[0] == 1)
    {
        strcpy(NetWork.Network,&VTSData.ServerData.Url1[1]);
        nwy_dbg_log("Sub SPN: %s\r\n",NetWork.Network);
    }
    else
    {
        strcpy(NetWork.Network,opname.long_eons);
        nwy_dbg_log("Actual SPN: %s\r\n",NetWork.Network);
    }
   

    #ifdef ENABLE_RS232_FAST
    //SendRS232String("SIM REGISTERED\n");
    sprintf(RSSend,"SPN - %s\n",NetWork.Network);
    //SendRS232String(RSSend);
    #endif

    IsPacketReady.IsGSMParam=1;

    //nwy_sim_get_lacid(&lac,&cid);
    // sprintf(GSM.CellID,"%i",cid);
    // sprintf(GSM.LAC,"%i",lac);
    // nwy_dbg_log("LAC: %s, CELL_ID: %s \r\n", GSM.LAC, GSM.CellID);

    goto REGISTER_SUCCESS;
    REGISTER_SUCCESS:
        GSM.GSMState=GPRS_INIT;
        if(VTSState.RegDeniedCount>0){
            VTSState.RegDeniedCount=0;
            UpdateStateInFlash();
        }
        SetStatLED(10,5);
        return;

     REGISTER_FAIL:
        GSM.GSMState=SIM_DETECTED;
        SetStatLED(10,2);
        return;
}

void StatChange(int hndl, nwy_data_call_state_t ind_state)
{
  if (hndl > 0 && hndl <= 8)
  {
    nwy_dbg_log("Data call status update, handle_id:%d,state:%d (0=disconnected,1=connected,2=suspended)\r\n",hndl,ind_state);
    if(ind_state == NWY_DATA_CALL_CONNECTED)
    {
        nwy_dbg_log("Data call CONNECTED - Setting GSM.GSMState to GPRS_ACTIVE\r\n");
        DataCallConnectedFlag = 1;  // Mark that callback confirmed connection
        GSM.GSMState = GPRS_ACTIVE;
    }
    else if(ind_state == NWY_DATA_CALL_DISCONNECTED)
    {
        nwy_dbg_log("Data call DISCONNECTED - Setting GSM.GSMState to GPRS_INIT\r\n");
        DataCallConnectedFlag = 0;  // Clear flag on disconnection
        if(GSM.GSMState > GPRS_INIT)
            GSM.GSMState = GPRS_INIT;
    }
  }
}

void selectAPN(void)
{
    memset(NetWork.APN,0x00,sizeof(NetWork.APN));



	LowerString(NetWork.Network);
	if(strstr(NetWork.Network,"airtel"))
    {
		strcpy(NetWork.APN,"AIRTELIOT.com");
        NetWork.Provider = AIRTEL;
    }
	else if(strstr(NetWork.Network,"jio"))
    {
		strcpy(NetWork.APN,"JioNet");
        NetWork.Provider = JIO;
    }
	else if (strstr(NetWork.Network,"bsnl"))
    {
        strcpy(NetWork.Network,"BSNL");
		strcpy(NetWork.APN,"bsnlnet");
        NetWork.Provider = BSNL;
    }
    else if (strstr(NetWork.Network,"cellone") || strstr(NetWork.Network,"tata"))
    {
        strcpy(NetWork.Network,"CELLONE");
        strcpy(NetWork.APN,"bsnlnet");
        NetWork.Provider = BSNL;
    }
	else if (strstr(NetWork.Network,"vodafone") || strstr(NetWork.Network,"vi") || strstr(NetWork.Network,"idea"))
    {
		strcpy(NetWork.APN,"www");  // or try "portalnmms" or "internet" for Vodafone
        NetWork.Provider = VI;
        strcpy(NetWork.Network,"VI");
    }
	else
    {
		strcpy(NetWork.APN,"www");
        NetWork.Provider = VI;
        strcpy(NetWork.Network,"VI");
        nwy_dbg_log("Unknown provider, defaulting to VI with APN: %s\r\n", NetWork.APN);
    }
  
    if(!VTSData.AutoAPN)
        strcpy(NetWork.APN,VTSData.mAPN);

    nwy_dbg_log("APN Selected : %s",NetWork.APN);
    #ifdef ENABLE_RS232_FAST
    sprintf(RSSend,"APN Selected - %s\n",NetWork.APN);
    //SendRS232String(RSSend);
    #endif

}

#ifdef USE_DATA
int RcsHandle = 0;
static uint8_t FirstActivation = 1;  // Track first GPRS activation after boot


void ActivateGPRS(void)
{
    nwy_data_profile_info_t profile={0};
    nwy_data_start_call_v02_t param={0};
    nwy_data_addr_t_info info;
    uint8_t tmout=50;
    //static int dlfailcount;
    int ret, len=0;
    int cs_status = 0;
    //char sca[40];

    // Reset the connection flag at start of new activation attempt
    DataCallConnectedFlag = 0;

    // ret = nwy_sms_get_sca(sca);
    // if(ret == 0)
    //     nwy_dbg_log("SCA OBTAINED : %s",sca);
    // else
    //     nwy_dbg_log("SCA ERROR : %d",ret);
    
    if(!GetRegisterStat())
    {
        GSM.GSMState = SIM_DETECTED;
        return;
    }
    
    // Check if we're roaming
    if(nwy_nw_get_cs_st(&cs_status) == NWY_SUCCESS)
    {
        if(cs_status == 5)
        {
            nwy_dbg_log("Device is ROAMING (cs_status=5), enabling roaming support\r\n");
            // Roaming requires extra time for network negotiations
            nwy_sleep(1000);
        }
    }
    
    // On first activation, give system more time to initialize
    if(FirstActivation)
    {
        nwy_dbg_log("First GPRS activation attempt, allowing extra init time\r\n");
        nwy_sleep(2000);  // Wait 2 seconds for modem to be fully ready
        FirstActivation = 0;
    }
    
    // Stop any existing data call first to ensure clean state
    // In roaming, old connections may persist causing -7 error
    if(RcsHandle > 0)
    {
        nwy_dbg_log("Cleaning up existing handle %d before starting new call\r\n", RcsHandle);
        nwy_data_stop_call(RcsHandle);
        nwy_sleep(1000);  // Give time for cleanup
        nwy_data_release_srv_handle(RcsHandle);
        RcsHandle = 0;
        nwy_sleep(500);  // Extra delay after release for system cleanup
    }

    RcsHandle = nwy_data_get_srv_handle(StatChange);
    if(RcsHandle<0)
    {
        nwy_dbg_log("Error getting srv handle, err : %i\r\n",RcsHandle);
        return;
    }
    else
        nwy_dbg_log("srv Handle Obtained, handle : %i\r\n",RcsHandle);

    memset(&profile,0,sizeof(nwy_data_profile_info_t));
    ret = nwy_data_get_profile(1,NWY_DATA_PROFILE_3GPP,&profile);
    if(ret!=NWY_RES_OK)
    {
        nwy_dbg_log("No Available Profile! ret : %i\r\n",ret);
        nwy_dbg_log("Writing Custom Profile! \r\n");
        selectAPN();
        strcpy(profile.apn,NetWork.APN);
        profile.auth_proto = NWY_DATA_AUTH_PROTO_NONE;
        profile.pdp_type = NWY_DATA_PDP_TYPE_IPV4;
        // memcpy(profile.user_name,"card",sizeof(profile.user_name));
        // memcpy(profile.pwd,"card",sizeof(profile.pwd));
        ret = nwy_data_set_profile(1,NWY_DATA_PROFILE_3GPP,&profile);
        if(ret!=NWY_RES_OK)
        {
            nwy_dbg_log("Writing Custom Profile Failed\r\n");
            return;
        }
        else
            nwy_dbg_log("Writing Custom Profile Success!\r\n");
    }
    else
    {
        nwy_dbg_log("Curr APN : %s, pdp_type: %d, auth_proto: %d\r\n",
                   profile.apn, profile.pdp_type, profile.auth_proto);
        selectAPN();
        if(!strstr(profile.apn,NetWork.APN))
        {
            nwy_dbg_log("Changing APN from '%s' to '%s'\r\n", profile.apn, NetWork.APN);

            strcpy(profile.apn,NetWork.APN);
            profile.auth_proto = NWY_DATA_AUTH_PROTO_NONE;
            profile.pdp_type = NWY_DATA_PDP_TYPE_IPV4;

            if(nwy_data_set_profile(1,NWY_DATA_PROFILE_3GPP,&profile)==NWY_RES_OK)
                nwy_dbg_log("APN Change Success! %s (pdp_type: %d, auth_proto: %d)\r\n",
                           NetWork.APN, profile.pdp_type, profile.auth_proto);
            else
                nwy_dbg_log("APN Change FAIL! %s\r\n",NetWork.APN);


        }
        else
        {
            nwy_dbg_log("APN already matches: %s\r\n", NetWork.APN);
        }
    }
    param.profile_idx=1;
    param.is_auto_recon=1;
    param.auto_re_max = 0;
    param.auto_interval_ms=1000;
    ret = nwy_data_start_call(RcsHandle,&param);
    if(ret!=NWY_RES_OK)
    {
        nwy_dbg_log("Dail up Failed, ret: %d, APN: %s, Handle: %d\r\n",ret, NetWork.APN, RcsHandle);
        
        // Error -7 often means previous call not cleaned up or resource busy
        // Try stopping and retrying once
        if(ret == -7)
        {
            nwy_dbg_log("Error -7 (resource busy), stopping call and retrying...\r\n");
            nwy_data_stop_call(RcsHandle);
            nwy_sleep(2000);  // Wait 2 seconds for cleanup
            
            ret = nwy_data_start_call(RcsHandle,&param);
            if(ret == NWY_RES_OK)
            {
                nwy_dbg_log("Retry successful after cleanup\r\n");
            }
            else
            {
                nwy_dbg_log("Retry also failed, ret: %d\r\n", ret);
                #ifdef ENABLE_RS232_FAST
                //SendRS232String("Dail up Failed\n");
                #endif
                return;  // Exit if retry fails
            }
        }
        else
        {
            #ifdef ENABLE_RS232_FAST
            //SendRS232String("Dail up Failed\n");
            #endif
            return;  // Exit on other errors
        }
    }
    else
    {
        nwy_dbg_log("Dail up Initiated for APN: %s, Provider: %d\r\n", NetWork.APN, NetWork.Provider);
        #ifdef ENABLE_RS232_FAST
        //SendRS232String("Dail up Initiated\n");
        #endif
    }

    while(GSM.GSMState!=GPRS_ACTIVE)
    {

        if(!(--tmout))
        {
            nwy_data_stop_call(RcsHandle);
            nwy_dbg_log("GPRS Activation Timeout! APN: %s, Provider: %d, Network: %s\r\n", 
                       NetWork.APN, NetWork.Provider, NetWork.Network);
            #ifdef ENABLE_RS232_FAST
            //SendRS232String("GPRS Activation Timeout\n");
            #endif
            return;
        }
        nwy_dbg_log("GPRS : Waiting for GPRS..., csq: %d, APN: %s\r\n",GSM.SignalStrength, NetWork.APN);
        nwy_sleep(1000);
    }
    
    // Connection confirmed by callback, now wait for IP address to be assigned
    nwy_dbg_log("Data call connected, waiting for IP address assignment...\r\n");
    
    int ip_retry = 20;  // Retry for up to 10 seconds (20 * 500ms)
    while(ip_retry-- > 0)
    {
        memset(&info, 0, sizeof(nwy_data_addr_t_info));
        len = 0;  // Initialize to 0 as per SDK example (output parameter)
        ret = nwy_data_get_ip_addr(RcsHandle, &info, &len);
        if(ret == 0)
        {
            nwy_dbg_log("IP Address obtained: %s\r\n",nwy_ip4addr_ntoa(&info.iface_addr_s.ip_addr));
            break;
        }
        nwy_dbg_log("Waiting for IP address... (retry %d, ret=%d)\r\n", 20-ip_retry, ret);
        nwy_sleep(500);
    }
    
    if(ret != 0)
    {
        nwy_dbg_log("WARNING: Failed to get IP address after connection! ret=%d, APN: %s\r\n", ret, NetWork.APN);
        nwy_dbg_log("This may indicate a network/APN configuration issue\r\n");
        // Don't return - let it continue and see if IP appears later
    }

    #ifdef ENABLE_RS232_FAST 
    //SendRS232String("GPRS Activated !");
    #endif
    

    //SendSMS("7418214555","Can You Hear Me ?");
    SetStatLED(10,9);
    IsPacketReady.IsGSMParam=1;
    
}
#endif

void GetImei(void)
{   
    char imei[18] = {0};
    nwy_error_e ret = NWY_GEN_E_UNKNOWN;
    #ifdef VIRTUAL_IMEI
    strcpy(imei,VIMEI);
    nwy_dbg_log("Virtual IMEI Detected : %s",imei);
    #else
    ret = nwy_dm_get_imei(imei, sizeof(imei));
    if (NWY_SUCESS != ret)
    {
        nwy_dbg_log("Get IMEI error \r\n");
        return;
    }
    #endif
    if(VTSState.CustomImei.IsEnable)
    {
        nwy_dbg_log("Custom IMEI Detected : %s",VTSState.CustomImei.Imei);
        strcpy(imei,VTSState.CustomImei.Imei);
    }
    else
        nwy_dbg_log("IMEI Obtained : %s",imei);

    strcpy(NetWork.IMEI,imei);
    strcpy(sIMEI,NetWork.IMEI);
    nwy_dbg_log("IMEI:%s \r\n", NetWork.IMEI);
    //nwy_test_cli_get_imei();
}
int CheckGPRSState(void)
{
    nwy_data_addr_t_info info;
    int ret, len = 0;  // Initialize to 0 as per SDK example
    
    // Validate handle before checking state
    if(RcsHandle <= 0)
    {
        if(GSM.GSMState > GPRS_INIT)
        {
            nwy_dbg_log("CheckGPRSState: Invalid RcsHandle (%d), setting state to GPRS_INIT", RcsHandle);
            GSM.GSMState = GPRS_INIT;
        }
        return 0;
    }
    
    memset(&info, 0, sizeof(nwy_data_addr_t_info));
    ret = nwy_data_get_ip_addr(RcsHandle, &info, &len);
    
    // Only log when there's an issue, not on every successful check
    
    if(ret==0)
    {
        // SendAtCmd("AT+PDPSTATUS\r\n",resp,"TUS");
        // SendAtCmd("AT+CREG?\r\n",resp,"OK");
        // nwy_dbg_log("CheckGPRSState: IP obtained: %s",nwy_ip4addr_ntoa(&info.iface_addr_s.ip_addr));
        
        // Only transition to GPRS_ACTIVE if:
        // 1. We're already in GPRS_ACTIVE (just confirming), OR
        // 2. The StatChange callback has confirmed connection
        if(GSM.GSMState < GPRS_ACTIVE)
        {
            if(DataCallConnectedFlag)
            {
                nwy_dbg_log("CheckGPRSState: Callback confirmed, transitioning from state %d to GPRS_ACTIVE, IP: %s", 
                           GSM.GSMState, nwy_ip4addr_ntoa(&info.iface_addr_s.ip_addr));
                GSM.GSMState = GPRS_ACTIVE;
            }
            else
            {
                nwy_dbg_log("CheckGPRSState: IP available but callback not received yet, staying in state %d", 
                           GSM.GSMState);
                return 0;  // IP exists but callback hasn't confirmed - don't transition yet
            }
        }
        return 1;
    }
    else
    {
        // Only log detailed error if it's not the expected "callback confirmed but IP delayed" case
        if(!DataCallConnectedFlag || GSM.GSMState != GPRS_ACTIVE)
        {
            nwy_dbg_log("CheckGPRSState: nwy_data_get_ip_addr FAILED with ret=%d, DataCallConnectedFlag=%d, GSMState=%d", 
                       ret, DataCallConnectedFlag, GSM.GSMState);
        }
        
        // Don't downgrade if callback has confirmed connection - IP might just be delayed
        if(GSM.GSMState > GPRS_INIT && !DataCallConnectedFlag)
        {
            nwy_dbg_log("CheckGPRSState: No IP address, transitioning from state %d to GPRS_INIT", GSM.GSMState);
            GSM.GSMState = GPRS_INIT;
        }
        else if(GSM.GSMState == GPRS_ACTIVE && DataCallConnectedFlag)
        {
            // Connection confirmed by callback, stay in GPRS_ACTIVE even if IP query temporarily fails
            // This can happen due to timing or transient states
            return 1;  // Return success since we're connected
        }
        SetStatLED(10,9);
        return 0;
    }
}

uint8_t RecheckRegistoration(void)
{
    int cs;  
    if(nwy_nw_get_cs_st(&cs)!=NWY_SUCCESS)
        goto REGISTER_FAIL;


    if(cs!=1 && cs!=5)
        goto REGISTER_FAIL;

    if(GSM.GSMState<GPRS_INIT)
        GSM.GSMState = GPRS_INIT;
    return 1;

    REGISTER_FAIL:
        GSM.GSMState = SIM_DETECTED;
        return 0;
}


int timezone =0;

uint8_t IsLeapYear(int year)
{
    if(year % 4 != 0)
        return 0;

    if(year % 100 == 0)
    {
        if(year % 400 == 0)
            return 1;
        else
            return 0;
    }
    return 1;

}

void AdjustforTimeZone(nwy_time_t* julian_time)
{
    int uf=0;

    if(julian_time->min < 30)
    {
        uf=1;
        julian_time->min = 60 - (30 - julian_time->min);
    }
    else
        julian_time->min -= 30;

    if(julian_time->hour < (2+uf))
    {
        julian_time->hour = 24 - ((2+uf) - julian_time->hour);
        uf=1;
    }
    else
    {
        julian_time->hour-= (2+uf);
        uf=0;
    }
    
    if(julian_time->mon == 3)
    {
        
        if(julian_time->day == 1)
        {
            if(IsLeapYear(julian_time->year))
            {
                julian_time->day = 29 - uf;
            }
            else
                julian_time->day = 28 - uf;
            uf=1;
        }
        else
        {
            julian_time->day -= uf;
            uf=0;
        }
    }
    else
    {
        if(julian_time->day == 1)
        {
            julian_time->day = DayTable[julian_time->mon] - uf;
            uf=1;
        }
        else
        {
            julian_time->day -= uf;
            uf=0;
        }
    }
   julian_time->year -= uf;

}
extern uint8_t get_ntp_time(_RTC *rtc);

/**
 * Adjust GPS time to IST (Indian Standard Time) by adding +5:30
 * @param rtc Pointer to RTC structure containing GPS UTC time
 */
void AdjustGPSTimeToIST(_RTC *rtc)
{
    // Add 5 hours and 30 minutes for IST
    rtc->Min += 30;
    if (rtc->Min >= 60)
    {
        rtc->Min -= 60;
        rtc->Hour += 1;
    }
    
    rtc->Hour += 5;
    if (rtc->Hour >= 24)
    {
        rtc->Hour -= 24;
        rtc->Date += 1;
        
        // Handle month overflow
        uint8_t maxDays = DayTable[rtc->Month - 1]; // Month is 1-based
        if (rtc->Month == 2 && IsLeapYear(rtc->Year + 2000))
            maxDays = 29;
            
        if (rtc->Date > maxDays)
        {
            rtc->Date = 1;
            rtc->Month += 1;
            
            if (rtc->Month > 12)
            {
                rtc->Month = 1;
                rtc->Year += 1;
            }
        }
    }
}

void UpdateTime(void)
{
    nwy_time_t julian_time = {0};
    static uint8_t count;
    uint8_t useGPSTime = 0;

    
    GetSignal();

    // if(IsTimeSet)
    //     get_ntp_time(&CurrentDateTime);

    nwy_get_time(&julian_time, &timezone);
    
    // Check if network time is valid (year > 2022)
    if (julian_time.year > 2022)
    {
        // Use network time (priority)
        CurrentDateTime.Date = julian_time.day;
        CurrentDateTime.Month = julian_time.mon;
        CurrentDateTime.Year = julian_time.year % 100;
        CurrentDateTime.Hour = julian_time.hour;
        CurrentDateTime.Min = julian_time.min;
        CurrentDateTime.Sec = julian_time.sec;
        
        if (CurrentDateTime.Year > 22)
            IsTimeSet = 1;
    }
    else if (GPS.GPSFix && GPS.DateTime.Year > 22)
    {
        // Network time invalid, but GPS has valid fix - use GPS time as fallback
        nwy_dbg_log("Network time invalid, using GPS time as fallback");
        
        CurrentDateTime.Date = GPS.DateTime.Date;
        CurrentDateTime.Month = GPS.DateTime.Month;
        CurrentDateTime.Year = GPS.DateTime.Year;
        CurrentDateTime.Hour = GPS.DateTime.Hour;
        CurrentDateTime.Min = GPS.DateTime.Min;
        CurrentDateTime.Sec = GPS.DateTime.Sec;
        
        #ifdef PROTO_CDAC
        // GPS time is in UTC, adjust to IST (+5:30) for CDAC protocol
        AdjustGPSTimeToIST(&CurrentDateTime);
        nwy_dbg_log("GPS time adjusted to IST (+5:30)");
        #endif
        
        IsTimeSet = 1;
        useGPSTime = 1;
    }
    else
    {
        // Neither network nor GPS time is valid
        nwy_dbg_log("No valid time source available (Network: %d, GPS Fix: %d, GPS Year: %d)",
                    julian_time.year, GPS.GPSFix, GPS.DateTime.Year);
    }

    if (++count > 4)
    {
        nwy_dbg_log("20%02d-%02d-%02d %02d:%02d:%02d, csq: %d%s", 
                    CurrentDateTime.Year, CurrentDateTime.Month, CurrentDateTime.Date,
                    CurrentDateTime.Hour, CurrentDateTime.Min, CurrentDateTime.Sec, 
                    GSM.SignalStrength,
                    useGPSTime ? " [GPS]" : " [NETWORK]");
        count = 0;
    }
}

void GprsThreadEntry(void *param)
{
    nwy_sleep(2000);
    nwy_dbg_log("GPRS Thread Entry!!\r\n");\
    GetImei();
    while(1)
    {
        while(IsSleepMode)
            nwy_sleep(1000);

        switch (GSM.GSMState)
        {
            case SIM_NOT_DETECTED:
                nwy_dbg_log("GPRS: SIM NOT DETECTED\r\n");
                ProcessSim();
                break;
            case SIM_DETECTED:
                nwy_dbg_log("GPRS: SIM DETECTED\r\n");
                ProcessREGISTER();
                break;
            case GPRS_INIT:
                nwy_dbg_log("GPRS: GPRS INIT\r\n");
                #ifdef USE_DATA
                ActivateGPRS();
                #endif
                break;
            case GPRS_ACTIVE:
                //nwy_dbg_log("GPRS: GPRS ACTIVE\r\n");
                CheckGPRSState();
                RecheckRegistoration();
                break;
            default:
                break;
        }
        nwy_sleep(200);
        UpdateTime();
    }
    nwy_exit_thread_self();
}