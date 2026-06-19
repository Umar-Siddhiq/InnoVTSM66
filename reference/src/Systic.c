#include "project.h"
#include "Systic.h"
#include "Systic.h"
#include "Sensors.h"


volatile uint16_t StateLEDCount,ServerHangTimeOut ,ServerThreadTimeout, MCUHangTimeOut, AlivePktCount, ntpcount,ntptime, ProfileChangeCount, SystemStateCount, SystemStateHOLD;
volatile uint8_t oc, HourlyResetCount;
uint8_t updntp, IsNeigh, IsALVSend=1;
static uint8_t SendToggle = 0;  // Toggle: 0=Alive, 1=SystemState, 2=CellTower, 3=Peripheral - Response has priority

// RS232 Response Buffer - stores responses from server commands to avoid overwhelming MCU
// Responses are sent with priority, interrupting normal rotation when pending
char RS232ResponseBuffer[RS232_RESPONSE_BUFFER_SIZE];
uint8_t IsRS232ResponsePending = 0;

#define PRF_TIMEOUT (10*60)
StatLEDtypedef StatLED;
PacketReadyTypedef IsPacketReady;
#ifdef PROTO_CDAC
VehicleTypeDef VehicleState;
volatile uint32_t HaltCounter;
volatile uint32_t SleepCounter;
char VehicleMovingMode;
#endif

double PrevSpeed;
double PrevHeading;
double ReferenceHeading;  // Reference heading for 20-degree change detection
uint8_t ReferenceHeadingValid = 0;  // Flag to track if reference is initialized
uint8_t PrevFix;

unsigned long stime;
//$INF,imei,Firmare,GSMState(0=NoSIM,1=SimOK,2=SimREG,3=GPRSOK,4=SrvOK),Signal,SIMAKE,Currentprofile,Defaultprofile,Supportedprofile,SPN,CCID,IMSI,IP1:Port1,IP2:Port2,IP3:Port3,Url2\r\n
void SystemStateSend(void)
{
    char ss[330], str[64];
    int gsmState = 0;
    
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif

    // Initialize buffers
    memset(ss, 0, sizeof(ss));
    memset(str, 0, sizeof(str));

    // Start with $INF header and IMEI
    snprintf(ss, sizeof(ss), "$INF,%s,", NetWork.IMEI);
    
    // Firmware version
    snprintf(str, sizeof(str), "FV_%s_%s,", PROTO_TAG, FIRMWAREVERSION);
    strncat(ss, str, sizeof(ss) - strlen(ss) - 1);
    
    // GSM State - Convert to numeric code: 0=NoSIM, 1=SimOK, 2=SimREG, 3=GPRSOK, 4=ServerOK
    #ifndef HTTP_SIMULATE
    if(ServerSocket[0].SocketState == SOCKET_CONNECTED)
    {
        gsmState = 4;
    }
    #else
    if(HTTPState == HTTP_STATE_SET)
    {
        gsmState = 4;
    }
    #endif
    else
    {
        switch (GSM.GSMState)
        {
        case SIM_NOT_DETECTED:
            gsmState = 0;
            break;
        case SIM_DETECTED:
            gsmState = 1;
            break;
        case GPRS_INIT:
            gsmState = 2;
            break;
        default:
            gsmState = 3;
            break;
        }
    }
    
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d,", gsmState);
    strncat(ss, str, sizeof(ss) - strlen(ss) - 1);
    
    // Signal strength
    if(GSM.GSMState > SIM_NOT_DETECTED)
    {
        memset(str, 0, sizeof(str));
        snprintf(str, sizeof(str), "%d,", GSM.SignalStrength);
        strncat(ss, str, sizeof(ss) - strlen(ss) - 1);
        
        // SIM Make
        strncat(ss, SIM_MAKE_STR, sizeof(ss) - strlen(ss) - 1);
        strncat(ss, ",", sizeof(ss) - strlen(ss) - 1);
        
        // Current Profile
        memset(str, 0, sizeof(str));
        snprintf(str, sizeof(str), "%d,", VTSState.CurrentProfile);
        strncat(ss, str, sizeof(ss) - strlen(ss) - 1);
        
        // Default Profile
        memset(str, 0, sizeof(str));
        snprintf(str, sizeof(str), "%d,", VTSData.DefProfile);
        strncat(ss, str, sizeof(ss) - strlen(ss) - 1);
        
        // Supported Profile
        memset(str, 0, sizeof(str));
        snprintf(str, sizeof(str), "%d,", STKdata.profiles_supported);
        strncat(ss, str, sizeof(ss) - strlen(ss) - 1);
    }
    else
    {
        strncat(ss, ",,,,,", sizeof(ss) - strlen(ss) - 1); // Empty fields for signal, sim make, profiles
    }
    
    // SPN (Service Provider Name) - Truncate to 20 chars max
    if(GSM.GSMState > SIM_DETECTED)
    {
        char tempNet[21];
        memset(tempNet, 0, sizeof(tempNet));
        strncpy(tempNet, NetWork.Network, 20);
        strncat(ss, tempNet, sizeof(ss) - strlen(ss) - 1);
        strncat(ss, ",", sizeof(ss) - strlen(ss) - 1);
    }
    else
    {
        strncat(ss, ",", sizeof(ss) - strlen(ss) - 1);
    }
    
    // CCID - Truncate to 20 chars max
    if(GSM.GSMState > SIM_NOT_DETECTED)
    {
        char tempSIM[21];
        memset(tempSIM, 0, sizeof(tempSIM));
        strncpy(tempSIM, NetWork.SIMNo, 20);
        strncat(ss, tempSIM, sizeof(ss) - strlen(ss) - 1);
        strncat(ss, ",", sizeof(ss) - strlen(ss) - 1);
    }
    else
    {
        strncat(ss, ",", sizeof(ss) - strlen(ss) - 1);
    }
    

    // IMSI - Truncate to 15 chars max
    if(GSM.GSMState > SIM_NOT_DETECTED)
    {
        char tempIMSI[16];
        memset(tempIMSI, 0, sizeof(tempIMSI));
        strncpy(tempIMSI, NetWork.IMSI, 15);
        strncat(ss, tempIMSI, sizeof(ss) - strlen(ss) - 1);
        strncat(ss, ",", sizeof(ss) - strlen(ss) - 1);
    }
    else
    {
        strncat(ss, ",", sizeof(ss) - strlen(ss) - 1);
    }
    

    
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%s:%s,", VTSData.ServerData.IP1, VTSData.ServerData.Port1);
    strncat(ss, str, sizeof(ss) - strlen(ss) - 1);
    
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%s:%s,", VTSData.ServerData.IP2, VTSData.ServerData.Port2);
    strncat(ss, str, sizeof(ss) - strlen(ss) - 1);
    
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%s:%s,", VTSData.ServerData.IP3, VTSData.ServerData.Port3);
    strncat(ss, str, sizeof(ss) - strlen(ss) - 1);
    
    // URL2 - Truncate to max 30 chars to ensure we fit in 350 bytes
    size_t remaining = sizeof(ss) - strlen(ss) - 3; // Leave room for \r\n\0
    if(remaining > 30)
        remaining = 30;
    strncat(ss, VTSData.ServerData.Url2, remaining);
    strncat(ss, "\n", sizeof(ss) - strlen(ss) - 1);
    
    nwy_dbg_log("%s", ss);
    SendRS232String(ss);
}


// $CEL,imei,MCC,MNC,LAC,CELLID,Signal,NeighborCell1,MCC1,MNC1,LAC1,CELLID1,Signal1,NeighborCell2,MCC2,MNC2,LAC2,CELLID2,Signal2,NeighborCell3,MCC3,MNC3,LAC3,CELLID3,Signal3\r\n
void SendCellTowerdata(void)
{
    char cel[300], str[80];
    int i;
    
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif
    
    // Check if GSM is initialized
    if(GSM.GSMState <= SIM_NOT_DETECTED)
    {
        return;
    }
    
    // Initialize buffer
    memset(cel, 0, sizeof(cel));
    memset(str, 0, sizeof(str));
    
    // Start with $CEL header and IMEI
    snprintf(cel, sizeof(cel), "$CEL,%s,", NetWork.IMEI);
    
    // Add serving cell information: MCC, MNC, LAC, CELLID, Signal (CSQ)
    // SignalStrength is already in CSQ format (0-31)
    snprintf(str, sizeof(str), "%d,%d,%s,%s,%d,", GSM.MCC, GSM.MNC, GSM.LAC, GSM.CellID, GSM.SignalStrength);
    strncat(cel, str, sizeof(cel) - strlen(cel) - 1);
    // Add neighbor cells (up to 4)
    for(i = 0; i < 4; i++)
    {
        memset(str, 0, sizeof(str));
        if(GSM.NeigbourCell[i].mcc > 0 && GSM.NeigbourCell[i].mnc >= 0)
        {
            // CellDB is already in CSQ format (0-31) due to DBM_IN_CSQ define in GPRS.c
            // Add neighbor cell data: NeighborCell#, MCC#, MNC#, LAC#, CELLID#, Signal# (CSQ)
            snprintf(str, sizeof(str), "N%d,%d,%d,%s,%s,%s,", 
                    i+1, 
                    GSM.NeigbourCell[i].mcc, 
                    GSM.NeigbourCell[i].mnc, 
                    GSM.NeigbourCell[i].LAC, 
                    GSM.NeigbourCell[i].CellID, 
                    GSM.NeigbourCell[i].CellDB);  // Already CSQ
        }
        else
        {
            // Add empty neighbor cell data
            snprintf(str, sizeof(str), "N%d,,,,,,", i+1);
        }
        strncat(cel, str, sizeof(cel) - strlen(cel) - 1);
    }
    
    // Remove the trailing comma and add line ending
    int len = strlen(cel);
    if(len > 0 && cel[len-1] == ',')
    {
        cel[len-1] = '\0';
    }
    strncat(cel, "\n", sizeof(cel) - strlen(cel) - 1);
    
    SendRS232String(cel);
    nwy_dbg_log("%s", cel);
}


//$PER,imei,GPSModemState,GPSFix,HHMMSS,DDMMYY,IsMEMs,IsFlash,IsSOS,Ignition,out1,out2,in2\r\n
void PeriperalStateSend(void)
{
    char per[200], str[32];
    
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif
    
    // Initialize buffers
    memset(per, 0, sizeof(per));
    memset(str, 0, sizeof(str));
    
    // Start with $PER header and IMEI
    snprintf(per, sizeof(per), "$PER,%s,", NetWork.IMEI);
    
    // GPS Modem State (1 = OK, 0 = Fault)
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d,", !IsGPSFault);
    strncat(per, str, sizeof(per) - strlen(per) - 1);
    
    // GPS Fix (1 = Fixed, 0 = Not Fixed)
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d,", GPS.GPSFix);
    strncat(per, str, sizeof(per) - strlen(per) - 1);
    
    // Time (HHMMSS)
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%02d%02d%02d,", 
             CurrentDateTime.Hour, 
             CurrentDateTime.Min, 
             CurrentDateTime.Sec);
    strncat(per, str, sizeof(per) - strlen(per) - 1);
    
    // Date (DDMMYY)
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%02d%02d%02d,", 
             CurrentDateTime.Date, 
             CurrentDateTime.Month, 
             CurrentDateTime.Year % 100); // Last 2 digits of year
    strncat(per, str, sizeof(per) - strlen(per) - 1);
    
    // MEMs Status (1 = OK, 0 = Not OK)
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d,", PeriPheralVal.IsMEMs);
    strncat(per, str, sizeof(per) - strlen(per) - 1);
    
    // Flash Status (1 = OK, 0 = Not OK)
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d,", PeriPheralVal.IsFlash);
    strncat(per, str, sizeof(per) - strlen(per) - 1);
    
    // SOS Status (1 = Active, 0 = Not Active)
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d,", SOS.IsSOS);
    strncat(per, str, sizeof(per) - strlen(per) - 1);
    
    // Ignition Status (1 = ON, 0 = OFF)
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d,", PeriPheralVal.IGN);
    strncat(per, str, sizeof(per) - strlen(per) - 1);
    
    // Output 1 (out1)
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d,", PeriPheralVal.OP1);
    strncat(per, str, sizeof(per) - strlen(per) - 1);
    
    // Output 2 (out2)
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d,", PeriPheralVal.OP2);
    strncat(per, str, sizeof(per) - strlen(per) - 1);
    
    // Input 2 (in2)
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d,", PeriPheralVal.IP2);
    strncat(per, str, sizeof(per) - strlen(per) - 1);

    // Mains Voltage
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%04.1f,", PeriPheralVal.MainsVolt);
    strncat(per, str, sizeof(per) - strlen(per) - 1);

    // Battery Voltage
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%03.1f", PeriPheralVal.BattVolt);
    strncat(per, str, sizeof(per) - strlen(per) - 1);
    
    // Add newline before sending
    strncat(per, "\n", sizeof(per) - strlen(per) - 1);
    
    SendRS232String(per);
    nwy_dbg_log("%s", per);
}

void GPSDataSend(void)
{
    char gpd[250], str[32];
    
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif
    
    // Initialize buffers
    memset(gpd, 0, sizeof(gpd));
    memset(str, 0, sizeof(str));
    
    // Start with $GPD header and IMEI
    sprintf(gpd, "$GPD,%s,", NetWork.IMEI);
    
    // Tracked satellites (from GGA - satellites used in navigation solution)
    memset(str, 0, sizeof(str));
    sprintf(str, "%d,", GPS.NoOfSatalite);
    strncat(gpd, str, sizeof(gpd) - strlen(gpd) - 1);
    
    // Visible satellites (from GSV - total satellites in view across all constellations)
    // Note: SatTotal sums all constellation counts (GP+GL+GA+GB+GQ+GI+GN)
    // If modem sends GN (combined) sentences, this may count satellites multiple times
    memset(str, 0, sizeof(str));
    sprintf(str, "%d,",0);
    strncat(gpd, str, sizeof(gpd) - strlen(gpd) - 1);
    
    // HDOP
    memset(str, 0, sizeof(str));
    sprintf(str, "%.2f,", GPS.HDOP);
    strncat(gpd, str, sizeof(gpd) - strlen(gpd) - 1);
    
    // PDOP
    memset(str, 0, sizeof(str));
    sprintf(str, "%.2f,", GPS.PDOP);
    strncat(gpd, str, sizeof(gpd) - strlen(gpd) - 1);
    
    // Latitude
    memset(str, 0, sizeof(str));
    sprintf(str, "%.6f,", GPS.Latitude);
    strncat(gpd, str, sizeof(gpd) - strlen(gpd) - 1);
    
    // Longitude
    memset(str, 0, sizeof(str));
    sprintf(str, "%.6f,", GPS.Longitude);
    strncat(gpd, str, sizeof(gpd) - strlen(gpd) - 1);
    
    // Speed
    memset(str, 0, sizeof(str));
    sprintf(str, "%.2f,", GPS.Speed);
    strncat(gpd, str, sizeof(gpd) - strlen(gpd) - 1);
    
    // Altitude
    memset(str, 0, sizeof(str));
    sprintf(str, "%.2f,", GPS.Altitude);
    strncat(gpd, str, sizeof(gpd) - strlen(gpd) - 1);
    
    // Heading (no trailing comma for last field)
    memset(str, 0, sizeof(str));
    sprintf(str, "%.2f", GPS.Heading);
    strncat(gpd, str, sizeof(gpd) - strlen(gpd) - 1);
    
    // Add newline before sending
    strncat(gpd, "\n", sizeof(gpd) - strlen(gpd) - 1);
    
    SendRS232String(gpd);
    nwy_dbg_log("%s", gpd);
}


/**
 * Queue RS232 Response - Store response to be sent in next rotation
 * @param response The response string to queue
 */
void QueueRS232Response(const char* response)
{
    if(response == NULL || strlen(response) == 0)
        return;
    
    // Clear buffer and copy new response
    memset(RS232ResponseBuffer, 0, sizeof(RS232ResponseBuffer));
    strncpy(RS232ResponseBuffer, response, RS232_RESPONSE_BUFFER_SIZE - 1);
    IsRS232ResponsePending = 1;
    
    nwy_dbg_log("RS232 Response Queued: %s", RS232ResponseBuffer);
}

/**
 * Send Buffered RS232 Response - Send queued response if available
 */
void SendBufferedRS232Response(void)
{
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif
    
    if(IsRS232ResponsePending && strlen(RS232ResponseBuffer) > 0)
    {
        SendRS232String(RS232ResponseBuffer);
        nwy_dbg_log("RS232 Response Sent: %s", RS232ResponseBuffer);
        
        // Clear buffer after sending
        memset(RS232ResponseBuffer, 0, sizeof(RS232ResponseBuffer));
        IsRS232ResponsePending = 0;
    }
}

void CheckGPSAlerts(void)
{
    if(!GPS.GPSFix)
    {
        PrevFix=0;
        return;
    }
    if(!PrevFix){
        PrevFix=1;
        PrevSpeed = GPS.Speed;
        PrevHeading = GPS.Heading;
        return;
    }

    float deltaSpeed = PrevSpeed-GPS.Speed;
    float deltaHeading = GPS.Heading - PrevHeading;

    if (deltaHeading > 180.0f) {
        deltaHeading -= 360.0f;
    } else if (deltaHeading < -180.0f) {
        deltaHeading += 360.0f;
    }

    if(deltaHeading < 0)
        deltaHeading *= -1;

    #ifdef PROTO_CDAC
    if(deltaSpeed < ((VTSData.VehicleData.HarshAcc/100)*-1)){
        VAlert[HARSH_ACC_ALERT].Enable=1;
        AddAlert(HARSH_ACC_ALERT);
        nwy_dbg_log("********* GPS HARSH ACCEL ALERT********\n");
        SendRS232String("\r\n ********* GPS HARSH ACCEL ALERT********\n");
    }
    if(deltaSpeed > (VTSData.VehicleData.HarshAcc/50)){
        VAlert[HARSH_BRK_ALERT].Enable=1;
        AddAlert(HARSH_BRK_ALERT);
        nwy_dbg_log("********* GPS HARSH BRAKE ALERT********\n");
        SendRS232String("\r\n ********* GPS HARSH BRAKE ALERT********\n");
    }

    if((deltaHeading > VTSData.VehicleData.RashTurn) && GPS.Speed > 20){
        VAlert[RASH_TURN_ALERT].Enable=1;
        AddAlert(RASH_TURN_ALERT);
        nwy_dbg_log("********* GPS RASH TURN ALERT********\n");
        SendRS232String("\r\n ********* GPS RASH TURN ALERT********\n");
    }
    #endif

    PrevSpeed = GPS.Speed;
    PrevHeading = GPS.Heading;
}

/**
 * @brief Check for 20-degree heading change and trigger packet
 * Called every second from timer. Sends packet when heading changes
 * by 20 degrees or more from the reference heading.
 * In a roundabout/U-turn, this will generate multiple packets at 20-degree intervals.
 */
void CheckHeadingChange(void)
{
    // Only check when moving and GPS is valid
    if(!GPS.GPSFix || GPS.Speed < 5.0)
    {
        // Reset reference when stopped or no GPS
        ReferenceHeadingValid = 0;
        return;
    }
    
    // Initialize reference heading when starting to move
    if(!ReferenceHeadingValid)
    {
        ReferenceHeading = GPS.Heading;
        ReferenceHeadingValid = 1;
        nwy_dbg_log("Heading reference set: %.1f", ReferenceHeading);
        return;
    }
    
    // Calculate heading change from reference with 360-degree wrap-around
    double headingDiff = GPS.Heading - ReferenceHeading;
    
    // Normalize to -180 to +180 range
    if(headingDiff > 180.0)
        headingDiff -= 360.0;
    else if(headingDiff < -180.0)
        headingDiff += 360.0;
    
    // Get absolute difference
    double absDiff = (headingDiff < 0) ? -headingDiff : headingDiff;
    
    // Check for 20-degree change from reference
    if(absDiff >= 20.0)
    {
        nwy_dbg_log("20-deg heading change: ref=%.1f cur=%.1f diff=%.1f", 
                    ReferenceHeading, GPS.Heading, headingDiff);
        
        // Trigger packet immediately
        IsPacketReady.IsNormalPacket = 1;
        IntervalTick.NormalTick = 0;
        
        // Update reference heading to current for next 20-degree check
        ReferenceHeading = GPS.Heading;
    }
}

void UpdateTick(void)
{   
    #ifdef PROTO_CDAC
    // Variables for CDAC critical alert interval handling
    uint8_t inContinuousCritical = SOS.IsSOS || SOS.IsSOSTamper || PeriPheralVal.IsTilt || IsOverSpeed;
    uint8_t isEmergencyState = SOS.IsSOS || SOS.IsSOSTamper;  // Only SOS uses URE
    uint16_t critInterval = isEmergencyState ? VTSData.IntervalData.EnergencyInterval : VTSData.IntervalData.CurrentInterval;
    #endif
    
    #ifndef PROTO_CDAC
    // Sensor tick handling using SensorsConfig intervals
    // Interval is determined by IGNITION state and day/night time
    if(SensorsConfig.isEnabled)
    {
        uint16_t sensorInterval = GetCurrentSensorInterval();
        if(sensorInterval > 0)
        {
            nwy_dbg_log("Sensor Interval: %d, Current Tick: %d", sensorInterval, IntervalTick.SensTick);
            
            // Query CLS sensor 3 seconds before packet is due (gives time for response)
            // Only query if interval is greater than 5 seconds
            if(sensorInterval >= 5 && IntervalTick.SensTick == (sensorInterval - 3))
            {
                QueryCLSSensor();
            }
            
            if(++IntervalTick.SensTick >= sensorInterval)
            {
                IsPacketReady.IsSensPacket = 1;
                IntervalTick.SensTick = 0;
            }
        }
    }

	if(IntervalTick.NormalTick >=VTSData.IntervalData.CurrentInterval-1)
	{
		IsPacketReady.IsNormalPacket=1;
		IntervalTick.NormalTick=0;
	}
	else
		IntervalTick.NormalTick++;


    if(IntervalTick.HistoryTick >= 3)
    {
        IsPacketReady.IsHistoryPacket=1;
        IntervalTick.HistoryTick=0;
    }
    else    
        IntervalTick.HistoryTick++;
//	if(IntervalTick.FullTick >= VTSData.IntervalData.FullDataPacketInterval)
//	{
//		IsPacketReady.IsFullPacket=1;
//		IntervalTick.FullTick=0;
//	}
//	else
//		IntervalTick.FullTick++;
	if(IntervalTick.HealthTick >= VTSData.IntervalData.HealthInterval)
	{
		IsPacketReady.IsHealthPacket=1;
		IntervalTick.HealthTick=0;
	}
	else
		IntervalTick.HealthTick++;
//	if(IntervalTick.CriticalTick >= VTSData.IntervalData.EnergencyInterval-2)
//	{
//		IsPacketReady.IsCriticalPacket=1;
//		IntervalTick.CriticalTick=0;
//	}
//	else
//		IntervalTick.CriticalTick++;
    #else
    // Pre-connect for continuous critical alerts (SOS, Tamper, Tilt, Overspeed)
    // URE interval only when SOS is active, otherwise use normal interval
    if(inContinuousCritical && (IntervalTick.CriticalTick >= critInterval-CONNTECTION_PRETIME))
        HTTPConnectFlag=1;
    else
    {
        if(IntervalTick.NormalTick >=VTSData.IntervalData.CurrentInterval-CONNTECTION_PRETIME)
            HTTPConnectFlag=1;
        if(IntervalTick.FullTick >= VTSData.IntervalData.FullDataPacketInterval-CONNTECTION_PRETIME)
            HTTPConnectFlag=1;
        if(IntervalTick.HealthTick >= VTSData.IntervalData.HealthInterval-CONNTECTION_PRETIME)
            HTTPConnectFlag=1;
    }
    if(IntervalTick.NormalTick >=VTSData.IntervalData.CurrentInterval-1)
	{
		IsPacketReady.IsNormalPacket=1;
		IntervalTick.NormalTick=0;
	}
	else
		IntervalTick.NormalTick++;
	if(IntervalTick.FullTick >= VTSData.IntervalData.FullDataPacketInterval-1)
	{
		IsPacketReady.IsFullPacket=1;
		IntervalTick.FullTick=0;
	}
	else
		IntervalTick.FullTick++;

	if(IntervalTick.HealthTick >= VTSData.IntervalData.HealthInterval-1)
	{
		IsPacketReady.IsHealthPacket=1;
		IntervalTick.HealthTick=0;
	}
	else
		IntervalTick.HealthTick++;

	// Only set IsCriticalPacket when there's an active continuous critical alert
	// and the appropriate interval has elapsed
	if(inContinuousCritical && IntervalTick.CriticalTick >= critInterval-1)
	{
		IsPacketReady.IsCriticalPacket=1;
		IntervalTick.CriticalTick=0;
	}
	else if(inContinuousCritical)
	{
		IntervalTick.CriticalTick++;
	}
	else
	{
		// No continuous critical alert - reset tick
		IntervalTick.CriticalTick = 0;
	}
    #endif
    if(IsNeigh)
    {
        if(IntervalTick.ParamTick >= PARAM_UPDATE)
        {
            IsPacketReady.IsGSMParam=1;
            IntervalTick.ParamTick=0;
        }
        else
            IntervalTick.ParamTick++;
    }
    else
    {
        if(IntervalTick.ParamTick >= 20)
        {
            IsPacketReady.IsGSMParam=1;
            IntervalTick.ParamTick=0;
        }
        else
            IntervalTick.ParamTick++;   
    }
    #ifdef PRF_AUTOSWITCH 
    #ifndef PROTO_CDAC
    if(ServerSocket[0].SocketState != SOCKET_CONNECTED && !IsSleepMode && ServerSocket[1].SocketState!=SOCKET_CONNECTED 
        && ServerSocket[2].SocketState!=SOCKET_CONNECTED && FTPState != FTP_STATE_CONNECTED && GSM.GSMState>=SIM_DETECTED)
    #else
    if(HTTPState!=HTTP_STATE_SET && !IsSleepMode && ServerSocket[1].SocketState!=SOCKET_CONNECTED 
        && ServerSocket[2].SocketState!=SOCKET_CONNECTED && FTPState != FTP_STATE_CONNECTED && GSM.GSMState>=SIM_DETECTED)
    #endif
    {
        #ifndef AUTO_PROFILESWITCH_DISABLE
        if(ProfileChangeCount++ >= (PRF_TIMEOUT))
        {
            ProfileChangeCount=0;
            
            // ZigTestMode: Prevent automatic profile switching during manufacturing test
            if(ZigTestMode)
            {
                nwy_dbg_log("ZigTestMode Active - Automatic profile switch prevented (Connection Timeout)");
            }
            // FIX #2: Check if profile request already pending to avoid race condition
            else if(prfReq != NONE)
            {
                nwy_dbg_log("Profile switch already requested (%d), skipping duplicate timeout request", prfReq);
            }
            else
            {
                // Use smart profile selection instead of simple toggle
                int8_t newpf = GetNextValidProfile(VTSState.CurrentProfile);
                
                if(newpf == 0)
                {
                    nwy_dbg_log("No valid profiles available, restarting with profile 1...");
                    newpf = 1;
                }

                prfReq= newpf;

                nwy_dbg_log("GSM Connection Timeout, Switching from profile %d to profile %d...", 
                            VTSState.CurrentProfile, newpf);
            }
        }
        else
            nwy_dbg_log("Profile connection timeout in  %d/%d",ProfileChangeCount,PRF_TIMEOUT);
        #else
        if(ProfileChangeCount++ >= (PRF_TIMEOUT*2))
        {
            ProfileChangeCount=0;
            if(!ZigTestMode)
            {
                nwy_dbg_log("GSM Connection Timeout, Restarting...");
                nwy_power_off(2);
                nwy_sleep(3000);
            }
            else
            {
                nwy_dbg_log("ZigTestMode Active - Automatic restart prevented (Connection Timeout)");
            }
        }
        else
            nwy_dbg_log("Server connection timeout in  %d/%d",ProfileChangeCount,PRF_TIMEOUT*2);

        #endif

        
    }
    else
        ProfileChangeCount=0;
    #endif

    #define PRINTF
    #ifdef PRINTF

    #ifdef PROTO_CDAC
    nwy_dbg_log("NTick: %d/%d  CTick: %d/%d  HTick: %d/%d  FTick: %d/%d  Mode: %c  Crit: %d",
                           IntervalTick.NormalTick, VTSData.IntervalData.CurrentInterval,
                           IntervalTick.CriticalTick, critInterval, 
                           IntervalTick.HealthTick,VTSData.IntervalData.HealthInterval,
                           IntervalTick.FullTick,VTSData.IntervalData.FullDataPacketInterval,
                           VehicleMovingMode, inContinuousCritical);
    #else
     nwy_dbg_log("Normal Tick: %d / %d\nHealth Tick: %d / %d\n",
                           IntervalTick.NormalTick, VTSData.IntervalData.CurrentInterval,
                           IntervalTick.HealthTick, VTSData.IntervalData.HealthInterval);
    #endif
    #endif
}
#ifdef PROTO_CDAC

void UpdateInterval(void)
{
    switch (VehicleState.VehicleMode)
    {
    case MOTION:
        VTSData.IntervalData.CurrentInterval=VTSData.IntervalData.MotionInterval;
        break;
    case HALT:
    VTSData.IntervalData.CurrentInterval=VTSData.IntervalData.HaltInterval;
        break;
    case SLEEP: 
        VTSData.IntervalData.CurrentInterval=VTSData.IntervalData.SleepInterval;
        break;
    default:
        break;
    }
}

void UpdateVehicle(void)
{
	// CDAC VTMS Phase 5: Motion Mode requires Ignition ON AND speed > 3 km/hr
	if((PeriPheralVal.IGN == 1) && (GPS.Speed > 3.0))
	{
		if((VehicleState.VehicleMode==HALT) || (VehicleState.VehicleMode==SLEEP))
		{
//			HaltCounter++;
//			if(HaltCounter >= VTSData.IntervalData.HaltTime)
//			{
				VehicleState.VehicleMode=MOTION;
				UpdateInterval();
                VehicleMovingMode='M';
				HaltCounter=0;
		//	}
		}
	}
	else
	{
		if(VehicleState.VehicleMode==MOTION)
		{
			HaltCounter++;
			if(HaltCounter >= VTSData.IntervalData.HaltTime)
			{
				VehicleState.VehicleMode=HALT;
				VehicleMovingMode='H';
				SleepCounter=0;
				UpdateInterval();
				HaltCounter=0;
			}
		}
		if(VehicleState.VehicleMode==HALT)
		{
			SleepCounter++;
			if(SleepCounter >= VTSData.IntervalData.SleepTime)
			{
				VehicleState.VehicleMode=SLEEP;
				UpdateInterval();
				VehicleMovingMode='S';
				SleepCounter=0;
			}
		}
	}
	
}
#endif

volatile unsigned long msprev100ms ,msprevSec , msprevMin;

void Systic_Event_100ms(void)
{
   
    //100 ms

    msprev100ms = stime;
    if(FTPState == FTP_STATE_CONNECTED)
        SetStatLED(4,2);
    // Defensive: if LED params corrupted/zero, set default pattern
    if(StatLED.TTime == 0 || StatLED.ONTime == 0)
        SetStatLED(20, 2);
    if(StateLEDCount++ < StatLED.ONTime)
    {
        //nwy_dbg_log("LED ON");
        STATLED_ON;
    }
    else
    {
        //nwy_dbg_log("LED OFF");
        STATLED_OFF;
    }
    if(StateLEDCount >= StatLED.TTime)
        StateLEDCount=0;

}

void ProcessServerThreadTimeout(void)
{
    if (GSM.GSMState< GPRS_ACTIVE)
        return;

    if(ServerSocket[0].SocketState != SOCKET_CONNECTED && ServerSocket[1].SocketState != SOCKET_CONNECTED && ServerSocket[2].SocketState != SOCKET_CONNECTED)
        return;

    if(++ServerThreadTimeout > 600)
    {
        ServerThreadTimeout=0;
        
        // ZigTestMode: Prevent automatic restart during manufacturing test
        if(ZigTestMode)
        {
            nwy_dbg_log("ZigTestMode Active - Automatic restart prevented (Server Thread Timeout)");
        }
        else
        {
            nwy_dbg_log("Server Thread Timeout, Restarting...");
            nwy_power_off(2);
            nwy_sleep(3000);
        }
    }
    
}

void Systic_Event_1s(void)
{
  
    msprevSec=stime;
    
    UpdateTick();
    #ifdef PROTO_CDAC
    UpdateVehicle();
    #else
    ProcessServerThreadTimeout();
    #endif
    ProcessSOS();
    CheckGPSAlerts();
    CheckHeadingChange();  // Check for 20-degree heading change
    
    // Process sensor timeouts (only for REGULAR sensors)
    // Uses configurable timeout from SensorsConfig.sensorTimeout
    if(SensorsConfig.isEnabled)
    {
        ProcessSensorTimeouts(SensorsConfig.sensorTimeout);
    }

  
    
    #ifndef NEW_SERIAL_INTERFACE
    if(SystemStateHOLD < 3)
    {
        SystemStateHOLD++;
    }
    else
    {
        if(IsALVSend)
        {
            // Priority: Send buffered RS232 response first if available
            if(IsRS232ResponsePending)
            {
                SendBufferedRS232Response();
                // Don't change SendToggle, continue normal rotation next time
            }
            else
            {
                // Normal rotation: Alive -> SystemState -> CellTower -> Peripheral
                if(SendToggle == 0)
                {
                    // Send Alive packet to MCU
                    if(!IsMOTAProcess)
                        SendAlivePacket();
                    SendToggle = 1;
                }
                else if(SendToggle == 1)
                {
                    SystemStateSend();
                    SendToggle = 2;
                }
                else if(SendToggle == 2)
                {
                    SendCellTowerdata();
                    SendToggle = 3;
                }
                else if(SendToggle == 3)
                {
                    PeriperalStateSend();
                    SendToggle = 4;
                }
                else // Send Toggle == 4
                {
                    GPSDataSend();
                    SendToggle = 0;
                }
            }
        }
    }
    #endif
    
    if(MCUHangTimeOut>0)
        MCUHangTimeOut--;
    else
    {
        IsMCU=0;
    }
    if(!IsMCUTime)
    {
        if(stime > 10000)
            IsMCUTime = 1;
    }

    ntpcount++;
    if(IsTimeSet)
        ntptime=(30*60);
    else
        ntptime= 6;
    if(ntpcount>=ntptime)
    {
        updntp=1;
        ntpcount=0;
    }
}

void Systic_Event_1m(void)
{
     // 1 Min
    msprevMin = stime;
    ServerHangTimeOut++;
    if(ServerHangTimeOut > SERVER_HANG_TIME)
    { 
        ServerHangTimeOut=0;
        
        // ZigTestMode: Prevent automatic restart during manufacturing test
        if(ZigTestMode)
        {
            nwy_dbg_log("ZigTestMode Active - Automatic restart prevented (Server Hang Timeout %d)", SERVER_HANG_TIME);
        }
        else
        {
            nwy_dbg_log("Server Timeout %d... Restarting Device",ServerHangTimeOut);
            nwy_power_off(2);//Restart
        }
    }
    // if(++HourlyResetCount > 60)
    // {
    //     nwy_dbg_log("Hourly Reset Time %d!, Restarting Device...",HourlyResetCount);
    //     HourlyResetCount=0;
    //     nwy_power_off(2);//Restart
    // }
}


void SysticThreadEntry(void *param)
{
    SetStatLED(20,2);
    nwy_dbg_log("Systic Thread Enrty");
    while(1)
    {
        stime = nwy_get_ms();
        if((stime-msprev100ms > 96))
            Systic_Event_100ms();
        if((stime-msprevSec) > 996)
            Systic_Event_1s();
        if((stime-msprevMin) > (1000*60)-4)
            Systic_Event_1m();
        nwy_sleep(5);
    }
    nwy_exit_thread_self();
}

void SetStatLED(uint16_t tt, uint16_t on)
{

    StatLED.TTime= tt;
    StatLED.ONTime= on;
}