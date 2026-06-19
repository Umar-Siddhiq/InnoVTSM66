#include "MCU.h"
#include "stdlib.h"
#include "Server.h"

char RSSend[RSSendSIZE];
char RSBuffer[RSBufferSIZE];
uint8_t IsRsCmd, ovalert;
uint8_t IsCanMsg;
CANPKT CanMsg;
double PHeading;

#define OTABufferSIZE 256
char OTABuffer[OTABufferSIZE];
uint8_t IsOTACmd;

#define MCOMM_RX_MAX 500
char MCOMMRxBuffer[MCOMM_RX_MAX];
uint16_t MCOMMRxLen;

char MCOMMHPacket[256];
uint8_t IsMCOMMHData;
uint16_t HPacketCount;
uint8_t IsHCountReply;

uint8_t MCUOKRes, MCUErrRes;

char FuelData[150];
uint8_t IsFuelData, IsGPSFault;



double PrevLat, PrevLong;

//// &#GIP,ign\r\n
//// &#GOP,op1,op2\r\n
//// &#SOS,state\r\n
//// &#RCM,message\r\n
//// &#CCM,id,type,dlc,d1,d2,d3,d4,d5,d6,d7,d8\r\n
//// &#AUP,mains,ad1,ad2\r\n
//// &#GPS,state,lat,long,Alt,Speed,PDOP,HDOP,sHeading\r\n 
//// &#DBG,state\r\n
////&#ALV\r\n

#define EARTH_RADIUS_METERS 6371000.0

// Define your own PI constant
#define PI 3.14159265358979323846

// Function to convert degrees to radians
double toRadians(double degrees) {
    return degrees * PI / 180.0;
}

// Function to compute the square root using the Newton-Raphson method
double sqrtNewtonRaphson(double number) {
    // Guard against invalid inputs that could cause infinite loop
    if (number <= 0) return 0;
    if (number == 1) return 1;
    
    double x = number;
    double y = 1.0;
    double epsilon = 0.000001; // Error tolerance
    int iterations = 0;
    const int MAX_ITERATIONS = 50;  // Prevent infinite loop

    while (x - y > epsilon && iterations < MAX_ITERATIONS) {
        x = (x + y) / 2;
        y = number / x;
        iterations++;
    }
    return x;
}

// Function to compute the cosine of an angle in radians using the Taylor series expansion
double cosTaylor(double x) {
    double term = 1.0;
    double sum = 1.0;
    double x2 = x * x;
    int n = 1;
    double factorial = 1.0;
    
    // Limit iterations to prevent infinite loop
    // 20 iterations is more than enough for double precision convergence
    const int MAX_ITERATIONS = 20;

    // Loop to compute the terms of the Taylor series expansion
    while (n <= MAX_ITERATIONS) {
        factorial *= (2 * n - 1) * (2 * n);
        term *= -x2 / factorial;
        // Check for convergence using a small epsilon instead of exact zero
        if (term > -1e-15 && term < 1e-15) break;
        sum += term;
        n++;
    }

    return sum;
}

// Function to calculate the distance between two lat/lon points
double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
    // Convert latitude and longitude from degrees to radians
    double lat1Rad = toRadians(lat1);
    double lon1Rad = toRadians(lon1);
    double lat2Rad = toRadians(lat2);
    double lon2Rad = toRadians(lon2);
    
    // Compute deltas
    double dLat = lat2Rad - lat1Rad;
    double dLon = lon2Rad - lon1Rad;
    
    // Compute distance using Euclidean method
    double a = dLat * EARTH_RADIUS_METERS;
    double b = dLon * EARTH_RADIUS_METERS * cosTaylor((lat1Rad + lat2Rad) / 2);
    double distance = sqrtNewtonRaphson(a * a + b * b);
    
    return distance;
}

uint8_t IsMCU;
extern volatile uint16_t MCUHangTimeOut;
uint8_t MCUUart;


GPS_Typedef GPS;
char sLatitude[12];
char sLongitude[12];
char sAltitude[9];
char sSpeed[8];
char sPDOP[6];
char sHDOP[6];
char sHeading[7];


#define MCU_UART_ECHO_BUFF_LEN  512
void ParseMCUVersion(char* str);

void UartStringOut(char *fmt, ...)
{
    static char echo_str[MCU_UART_ECHO_BUFF_LEN];
    static int flag = 0;
    static nwy_osi_mutex_t mcu_mutex;
    va_list a;
    int i, size;
        
    if (0 == flag) {
        nwy_create_mutex(&mcu_mutex);
        flag = 1;
    }
    nwy_lock_mutex(mcu_mutex, NWY_OSA_SUSPEND);
    va_start(a, fmt);
    vsnprintf(echo_str, MCU_UART_ECHO_BUFF_LEN, fmt, a);
    va_end(a);
   // nwy_dbg_log("MCU Tx: %s",echo_str);
    size = strlen((char *)echo_str);
    i = 0;
    while (1)
    {
        int tx_size;

        tx_size = nwy_uart_send_data(MCUUart, (char *)echo_str + i, size - i);
        if (tx_size <= 0)
            break;
        i += tx_size;
        if ((i < size))
            nwy_sleep(10);
        else
            break;
    }
    nwy_unlock_mutex(mcu_mutex);
}

void MCURcv(const char *data, uint32 length)
{
    if(length > MCOMM_RX_MAX)
    {
        nwy_dbg_log("MCU Rcv length = %d > max supported %d", length,MCOMM_RX_MAX);
        return;
    }
    if(MCOMMRxLen)
    {
        nwy_dbg_log("MCU Rcv length = %d while previous not parsed err", length);
        return;
    }
    memcpy(MCOMMRxBuffer,data,length);
    MCOMMRxLen = length;
    //nwy_dbg_log("MCU Rcv length = %d", length);
}

void mcu_InitUart(void)
{
     
    int hd = nwy_uart_init(NWY_NAME_UART1, 1);
    if(hd == NWY_ERROR)
    {
        nwy_dbg_log("MCU UART INIT FAILED !!");
        return;
    }
    MCUUart = hd;
    nwy_dbg_log("MCU uart handle: %d",MCUUart);
    if(!nwy_uart_set_baud(MCUUart, 115200))
    {
        nwy_dbg_log("MCU UART BD set FAILED !!");
        return;
    }
    // if(!nwy_uart_get_baud(MCUUart,&baud))
    // {
    //     nwy_dbg_log("MCU UART BD get failed!");
    //     return;
    // }
    // if(baud != 115200)
    // {
    //     nwy_dbg_log("MCU UART BD get incorrect, exp 115200, got %d!!",baud);
    //     return;
    // }

    if(!nwy_uart_reg_recv_cb(MCUUart, MCURcv))
    {
        nwy_dbg_log("MCU UART cb set  FAILED !!");
        return;
    }

    nwy_dbg_log("MCU Uart Init Success");

}


uint8_t WaitMCUReply(uint8_t *flag,uint16_t timeout)
{
    uint16_t tm = timeout / 5;
    while(*flag == 0)
    {
        if(--tm == 0)
        {
            nwy_dbg_log("MCU flag wait Timeout");
            return 0;
        }
        if(MCUErrRes)
            return 0;

        nwy_sleep(5);
    }
    return 1;
}

void ClearMcuSendBuffer(void)
{
    memset(RSSend,0x00,RSSendSIZE);
}

void SendOKPacket(void)
{
    UartStringOut("%s#OK%s",MCUHEADER,MCUFOOTER);
}

/**
 * Sends MCU Alive Status
 * 
*/
void SendAlivePacket(void)
{
    UartStringOut("%s%s%s",MCUHEADER,ALIVE,MCUFOOTER);
}

void SendGyroSettingPacket(void)
{
    char ss[40];
    sprintf(ss,"%s%s,%d,%d,%d,%d%s",MCUHEADER,BEHAVE,VTSData.VehicleData.HarshAcc,VTSData.VehicleData.HarshBreak,VTSData.VehicleData.RashTurn,
                                                        VTSData.VehicleData.TiltAngle,MCUFOOTER);
    UartStringOut(ss);
}
/**
 * Sets or Resets SOS LED
 @note  &#SOS,state\r\n
*/
void SOSLedSet(uint8_t state)
{
    UartStringOut("%s%s,%d%s",MCUHEADER,SOSLED,state,MCUFOOTER);
}

void SendSleepReq(void)
{
    UartStringOut("&#SLPMD\r\n");
}

/**
 * Changes Debug Status 
*/
void ChangeDBGState(char *str)
{
    char *fn;
    fn = strchr(str,',');
    if(!fn)
        return;
    if(fn[1] == '0')
        IsDebug = 0;
    else if(fn[1] == '1')
        IsDebug = 1;
    
    //SendOKPacket();

}

/**
 * Sends General Input Request Packet
 * @note &#GIP\r\n
*/
void SendGIPReq(void)
{
    UartStringOut("%s%s%s",MCUHEADER,INPUTUPDATE,MCUFOOTER);
}

/**
 * Decodes General Input Packet 
 * @note &#GIP,ign\r\n
*/
void DecodeGIPString(char *str)
{

    char *fn;
    fn = strchr(str,',');
    if(!fn)
        return;
    fn++;
    if(fn[0] == '0')
        PeriPheralVal.IGN = 0;
    else if(fn[0] == '1')
        PeriPheralVal.IGN = 1; 
    //SendOKPacket();
    nwy_dbg_log("MCU GIP Update : %d",PeriPheralVal.IGN);
}

/**
 * Sends Cell Battery Config Packet
 * @note &#CEL,stat\r\n
*/
void SendCellCmd(uint8_t stat)
{
    UartStringOut("%s%s,%d%s",MCUHEADER,CELLUPDATE,stat,MCUFOOTER);
}

/**
 * Sends RS232 String Packet
 * @note &#RCM,message\r\n
*/
void SendRS232String(char *str)
{
    #ifdef ENABLE_RS232_PRINT
    UartStringOut("%s%s,%s%s",MCUHEADER,RS485COM,str,MCUFOOTER);
    #endif
}

/**
 * Decodes RS232 Data Packet
 * @note &#RCM,message\r\n
*/
void DecodeRCMString(char *str)
{

    char *fn;
    fn = strchr(str,',');
    if(!fn)
        return;
    fn++;
    memset(RSBuffer,0x00,RSBufferSIZE);
    strcpy(RSBuffer,fn);
    IsRsCmd=1;
    nwy_dbg_log("MCU RS RCV : %s",RSBuffer);
    //SendOKPacket();
}

/**
 * Decodes CAN Data Packet
 * @note &#CCM,id,type,dlc,d1,d2,d3,d4,d5,d6,d7,d8\r\n
*/
void DecodeCCMString(char *str)
{
    uint32_t id;
    uint8_t type, dlc, i;
    uint8_t dt[8] = {0};
    char *fn, *ln;
    fn = strchr(str,',');
    if(!fn)
        return;
    fn++;
    ln = strchr(fn,',');
    if(!ln)
        return;
    ln[0]= 0;
    ln++;

    id = atoi(fn);

    fn = strchr(ln,',');
    if(!fn)
        return;
    fn[0]=0;
    fn++;

    type = atoi(ln);

    ln = strchr(fn,',');
    if(!ln)
        return;
    ln[0]= 0;
    ln++;

    dlc = atoi(fn);

    for(i = 0;i<dlc;i+=2)
    {
        fn = strchr(ln,',');
        if(!fn)
            return;
        fn[0]=0;
        fn++;

        dt[i] = atoi(ln);

        if((i+1) >= dlc)
            break;

        ln = strchr(fn,',');
        if(!ln)
            return;
        ln[0]= 0;
        ln++;

        dt[i+1] = atoi(fn);

    }

    CanMsg.Address = id;
    CanMsg.rtr = type;
    CanMsg.dlc = dlc;
    IsCanMsg=1;
    memcpy(CanMsg.data,dt,8);
    //SendOKPacket();

}

/**
 * Sends Analog Update Req
 * @note  &#AUP\r\n
*/
void SendAUPReq(char* str)
{
    UartStringOut("%s%s%s",MCUHEADER,ADCUPDATE,MCUFOOTER);
}

/**
 * Decode Analog Update String
 * @note Format: #AUP,mains,ad1,ad2,memstate\r\n
 */
void DecodeAUPString(char *str)
{
    char *token;
    int fieldIndex = 0;

    token = strtok(str, ",");
    while (token != NULL) {
        fieldIndex++;

        switch (fieldIndex) {
            case 2: // mains
                PeriPheralVal.MainsVolt = (double)atoi(token) / 1000.0;
                break;
            case 3: // ad1
                PeriPheralVal.AN1 = (double)atoi(token) / 1000.0;
                break;
            case 4: // ad2
                PeriPheralVal.AN2 = (double)atoi(token) / 1000.0;
                break;
            case 5: // memstate
                PeriPheralVal.IsMEMs = atoi(token);
                break;
            case 6: // flashstate
                PeriPheralVal.IsFlash = atoi(token);
                break;
        }

        token = strtok(NULL, ",");
    }

    //SendOKPacket();
    nwy_dbg_log("MCU AUP Update %f, %f, %f",
        PeriPheralVal.MainsVolt,
        PeriPheralVal.AN1,
        PeriPheralVal.AN2);
}
/**
 * Sends  GPS Reset Request To MCU
 * @note Sends : &#GPS\r\n
*/
void SendGPSResetReq(void)
{
    UartStringOut("%s%s%s",MCUHEADER,GPSRESET,MCUFOOTER);
}

/**
 * Send  GPS Data Request To MCU
 * @note Sends : &#GPS\r\n
*/
void GetGPSState(void)
{
    char ss[75];
    memset(ss,0x00,75);
    sprintf(ss,"%s%s%s",MCUHEADER,GPSUPDATE,MCUFOOTER);
    UartStringOut(ss);
}

void ApplyFGPS(void)
{
    GPS.GPSFix = 1;

    // Apply random fluctuation to latitude and longitude
    double latFluctuation = ((rand() % 25) + 1) / EARTH_RADIUS_METERS * (180.0 / PI);
    double longFluctuation = ((rand() % 25) + 1) / (EARTH_RADIUS_METERS * cosTaylor(toRadians(fGPSLat))) * (180.0 / PI);

    GPS.Latitude = fGPSLat + ((rand() % 2 == 0) ? latFluctuation : -latFluctuation);
    GPS.Longitude = fGPSLong + ((rand() % 2 == 0) ? longFluctuation : -longFluctuation);

    // Apply random fluctuation to altitude, HDOP, PDOP, and number of satellites
    GPS.Altitude = fGPSAlt + ((rand() % 2 == 0) ? (rand() % 5 + 1) : -(rand() % 5 + 1));
    GPS.HDOP = fGPShdop + ((rand() % 2 == 0) ? ((rand() % 10 + 1) / 10.0) : -((rand() % 10 + 1) / 10.0));
    GPS.PDOP = fGPSpdop + ((rand() % 2 == 0) ? ((rand() % 10 + 1) / 10.0) : -((rand() % 10 + 1) / 10.0));
    GPS.NoOfSatalite = fGPSSats + ((rand() % 2 == 0) ? (rand() % 2) : -(rand() % 2));

    GPS.LatDir = 'N';
    GPS.LngDir = 'E';
    GPS.Heading = 0;

    GPS.Speed = fGPSSpeed + ((rand() % 2 == 0) ? (rand() % 5 + 1) : -(rand() % 5 + 1));
    if(GPS.Speed < 0)
        GPS.Speed = 0;

    sprintf(sLatitude, "%03.7f", GPS.Latitude);
    sprintf(sLongitude, "%03.7f", GPS.Longitude);
    #ifdef BSNL_PROTO
    sprintf(sAltitude, "%05.2f", GPS.Altitude);
    #else
    sprintf(sAltitude, "%3.2f", GPS.Altitude);
    #endif
    sprintf(sPDOP, "%2.2f", GPS.PDOP);
    sprintf(sHDOP, "%2.2f", GPS.HDOP);
    #ifdef BSNL_PROTO
    sprintf(sSpeed,"%04.1f", GPS.Speed);
    #else
    sprintf(sSpeed,"%02.1f", GPS.Speed);
    #endif
    sprintf(sHeading,"%03.2f", GPS.Heading);
}

uint8_t ovsCount;

/**
 * Decodes GPS String from MCU with backwards compatibility
 * @note String Format : &#GPS,state,lat,long,ltDir,lgDir,Alt,Speed,PDOP,HDOP,Heading,NoOfSats[,future_fields...]\r\n
 * @note New fields can be added at the end without breaking backwards compatibility
 * 
 * Field breakdown:
 * 0: Command (#GPS)
 * 1: state (0=not fixed, 1=fixed, 2=fault)
 * 2: latitude (multiplied by 1000000)
 * 3: longitude (multiplied by 1000000)
 * 4: latitude direction (N/S)
 * 5: longitude direction (E/W)
 * 6: altitude (multiplied by 10)
 * 7: speed (multiplied by 10)
 * 8: PDOP (multiplied by 10)
 * 9: HDOP (multiplied by 10)
 * 10: heading (multiplied by 10)
 * 11: number of satellites
 * [12+: reserved for future expansion]
 */
void DecodeGPSString(char *str)
{
    char *token;
    char buffer[256];
    int field_index = 0;
    uint8_t gps_state = 0;
    
    // Create a copy of the string for parsing (strtok modifies the string)
    strncpy(buffer, str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    // Parse using strtok (thread safety: caller responsible for synchronization)
    token = strtok(buffer, ",\r\n");
    
    while (token != NULL && field_index < 20) // 20 field limit for future expansion
    {
        switch (field_index)
        {
            case 0: // Command identifier (#GPS) - skip
                break;
                
            case 1: // GPS State (0=not fixed, 1=fixed, 2=fault)
                gps_state = atoi(token);
                
                // Handle non-fix states
                if (gps_state == 0 || gps_state == 2)
                {
                    nwy_dbg_log("MCU GPS Not Fixed (state=%d)", gps_state);
                    
                    // Try fallback GPS if configured
                    if (fGPSLat != 0 && fGPSLong != 0)
                    {
                        ApplyFGPS();
                    }
                    else
                    {
                        GPS.GPSFix = 0;
                    }
                    
                    IsGPSFault = (gps_state == 2) ? 1 : 0;
                    return; // Exit early for non-fix states
                }
                
                // Check if forced GPS mode is enabled
                if (fGPSLat != 0 && fGPSLong != 0 && fGPSForce)
                {
                    IsGPSFault = 0;
                    ApplyFGPS();
                    return;
                }
                
                IsGPSFault = 0;
                break;
                
            case 2: // Latitude (scaled by 1000000)
                GPS.Latitude = (double)atol(token) / 1000000.0;
                sprintf(sLatitude, "%03.7f", GPS.Latitude);
                break;
                
            case 3: // Longitude (scaled by 1000000)
                GPS.Longitude = (double)atol(token) / 1000000.0;
                sprintf(sLongitude, "%03.7f", GPS.Longitude);
                break;
                
            case 4: // Latitude Direction (N/S)
                GPS.LatDir = token[0];
                break;
                
            case 5: // Longitude Direction (E/W)
                GPS.LngDir = token[0];
                break;
                
            case 6: // Altitude (scaled by 10)
                GPS.Altitude = (double)atol(token) / 10.0;
                #ifdef BSNL_PROTO
                sprintf(sAltitude, "%05.2f", GPS.Altitude);
                #else
                sprintf(sAltitude, "%3.2f", GPS.Altitude);
                #endif
                break;
                
            case 7: // Speed (scaled by 10)
                GPS.Speed = (double)atol(token) / 10.0;
                #ifdef BSNL_PROTO
                sprintf(sSpeed, "%04.1f", GPS.Speed);
                #else
                sprintf(sSpeed, "%02.1f", GPS.Speed);
                #endif
                break;
                
            case 8: // PDOP (scaled by 10)
                GPS.PDOP = (double)atol(token) / 10.0;
                sprintf(sPDOP, "%2.2f", GPS.PDOP);
                break;
                
            case 9: // HDOP (scaled by 10)
                GPS.HDOP = (double)atol(token) / 10.0;
                sprintf(sHDOP, "%2.2f", GPS.HDOP);
                break;
                
            case 10: // Heading (scaled by 10)
                GPS.Heading = (double)atol(token) / 10.0;
                sprintf(sHeading, "%03.2f", GPS.Heading);
                // Heading change detection is done in Systic.c CheckHeadingChange()
                break;
                
            case 11: // Number of Satellites
                GPS.NoOfSatalite = atoi(token);
                break;
                

            case 12: // GPSTime  HHMMSS
                GPS.DateTime.Hour = (uint8_t)atoi(token) / 10000;
                GPS.DateTime.Min = (uint8_t)(atoi(token) % 10000) / 100;
                GPS.DateTime.Sec = (uint8_t)(atoi(token) % 100);
                break;
            case 13: // GPSDate DDMMYY
                GPS.DateTime.Date = (uint8_t)atoi(token) / 10000;
                GPS.DateTime.Month = (uint8_t)(atoi(token) % 10000) / 100;
                GPS.DateTime.Year = (uint8_t)(atoi(token) % 100);  // Store as 2-digit year (e.g., 25 for 2025)
                break;
            
            default:
                // Unknown field - log but don't fail
                if (field_index > 13)
                {
                    nwy_dbg_log("GPS: Ignoring unknown field %d: %s", field_index, token);
                }
                break;
        }
        
        token = strtok(NULL, ",\r\n");
        field_index++;
    }
    
    // Validate minimum required fields received (at least through field 11)
    if (field_index < 12)
    {
        nwy_dbg_log("GPS: Incomplete data - only %d fields received (minimum 12 required)", field_index);
        return;
    }
    
    // Mark GPS as fixed and send acknowledgement
    GPS.GPSFix = 1;
    //SendOKPacket();
    
    // --- Over-speed Alert Processing ---
    // Always trigger OVER_SPEED_ALERT - Server handles AlertID 17 vs 20/21 based on geofence state
    if (GPS.Speed > VTSData.VehicleData.OverSpeed)
    {
        #ifdef PROTO_CDAC
        if (!IsOverSpeed)
        {
        #endif
            if (++ovsCount > 5)
            {
                VAlert[OVER_SPEED_ALERT].Enable = 1;  
                AddAlert(OVER_SPEED_ALERT);
                #ifdef PROTO_CDAC
                // SMS: 17 for regular overspeed, 20 for geofence overspeed
                PeriPheralVal.PendingSMSAlert = CheckIfInside() ? 20 : 17;
                IsOverSpeed = 1;
                #endif
            }
        #ifdef PROTO_CDAC
        }
        #endif
    }
    else
    {
        #ifdef PROTO_CDAC
        if (IsOverSpeed)
        {
        #endif
            ovsCount = 0;
            VAlert[OVER_SPEED_ALERT].Enable = 0;      
            RemoveAlert(OVER_SPEED_ALERT);     
            #ifdef PROTO_CDAC
            IsOverSpeed = 0;
        }
        #endif  
    }
    
    // --- Odometer Calculation ---
    if (PrevLat == 0)
    {
        PrevLat = GPS.Latitude;
        PrevLong = GPS.Longitude;
    }
    else
    {
        double distance = calculateDistance(GPS.Latitude, GPS.Longitude, PrevLat, PrevLong);
        if (distance > 2 && GPS.Speed > 5)
        {
            VTSState.OdoCount += distance;
            UpdateStateInFlash();
        }
        PrevLat = GPS.Latitude;
        PrevLong = GPS.Longitude;
    }
} 


// &#PRF,prof\r\n
void DecodeProfileString(char* str)
{
    char* fn;
    fn = strchr(str,',');
    if(!fn)
        return;
    fn++;
    switch(fn[0])
    {
        case '1': prfReq = 1;
                break;
        case '2': prfReq = 2;
                break;
        case '3': prfReq = 3;
                break;
        default:
                nwy_dbg_log("INVALID PROF");
                return;

    }
    nwy_dbg_log("Sim Profile Requested");
    //nwy_power_off(2);
}

//&#ATC,cmd\r\n
void DecodeATCommand(char* str)
{
    char *fn, ss[100]={0}, resp[100] = {0};
    fn = strchr(str,',');
    if(!fn)
        return;
    fn++;
    strcpy(ss,fn);
    if(strlen(ss) < 1)
        return;
    strcat(ss,"\r\n");
    SendAtCmd(ss,resp," ");
    nwy_dbg_log("***********\n Send AT : ");
    nwy_dbg_log("%s",ss);
    nwy_dbg_log("\nResp: %s",resp);
    nwy_dbg_log("\n***********");
}

void DecodeSMSString(char *str)
{
    char *fn, *ln;
    char num[20];


    if(GSM.GSMState < GPRS_INIT)
        return;
    nwy_dbg_log("\n***********SMS Send Req*********\n%s",str);
    //memset(num,0x00,20);

    fn = strchr(str,',');
    if(!fn)
    {
        nwy_dbg_log("\n***********R1*********");
        return;
    }
    fn++;
    ln = strchr(fn,',');
    if(!ln)
    {
        nwy_dbg_log("\n***********R2*********");
        return;
    }
    *ln=0;
    ln++;
    strcpy(num,fn);
    
    
    SendSMS(num,ln);
    
    return;
    
}

void DecodeGHCPacket(char *str)
{
    char *fn, *ln;
    uint16_t pckt;
    fn = strchr(str,',');
    if(!fn)
        return;
    fn++;
    ln = strchr(fn,'\r');
    if(!ln)
        return;
    *ln = 0;
    if(strlen(fn)>6)
        return;
    pckt = atoi(fn);
    if(pckt > 8000)
        return;

    HPacketCount = pckt;
    IsHCountReply=1;
    nwy_dbg_log("MCU History Count : %d",HPacketCount);
    return;
}

void DecodeGDHPacket(char *str)
{
    char *fn, *ln;
    fn = strchr(str,',');
    if(!fn)
        return;
    fn++;
    ln = strchr(fn,'\r');
    if(!ln)
        return;
    *ln = 0;
    if(strlen(fn)>256)
        return;
    strcpy(MCOMMHPacket,fn);
    IsMCOMMHData=1;
    nwy_dbg_log("MCU History Data: %s",MCOMMHPacket);
    return;
}





void ParseMCUString(char* str)
{
    //UartStringOut("\r\nRecieved : %s",str);
    char *fn, *next_packet;
    uint8_t parsed = 0; // Flag to track if anything was parsed
    
    // Keep parsing until no more recognizable packets found
    do {
        parsed = 0;
        
        fn = strstr(str,INPUTUPDATE);
        if(fn)
        {
            DecodeGIPString(fn);
            MCUHangTimeOut=MCU_HANG_TIME;
            IsMCU=1;
            // Move past this packet - find the \r\n and skip it
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2; // Skip \r\n
                parsed = 1;
                continue;
            } else {
                return; // No complete packet, exit
            }
        }

        fn = strstr(str,ADCUPDATE);
        if(fn)
        {
            DecodeAUPString(fn);
            MCUHangTimeOut=MCU_HANG_TIME;
            IsMCU=1;
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }

        fn = strstr(str,RS485COM);
        if(fn)
        {
            DecodeRCMString(fn);
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }

        fn = strstr(str,CANCOM);
        if(fn)
        {
            DecodeCCMString(fn);
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }

        fn = strstr(str,GPSUPDATE);
        if(fn)
        {
            DecodeGPSString(fn);
            MCUHangTimeOut=MCU_HANG_TIME;
            IsMCU=1;
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }

        fn = strstr(str,GETHISTORY);
        if(fn)
        {
            DecodeGDHPacket(fn);
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        } 

        fn = strstr(str,GETHCOUNT);
        if(fn)
        {
            DecodeGHCPacket(fn);
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }   

        fn = strstr(str,DBGSTATE);
        if(fn)
        {
            ChangeDBGState(fn);
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }

        fn = strstr(str,PROFCHANGE);
        if(fn)
        {
            DecodeProfileString(fn);
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }
        
        fn = strstr(str,RESET);
        if(fn)
        {
            nwy_dbg_log("Restarting...");
            nwy_power_off(2);
            // No continue needed - device resets
            return;
        }
        
        fn = strstr(str,ATCMD);
        if(fn)
        {
            DecodeATCommand(fn);
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }
        
        fn = strstr(str,OTACMD);
        if(fn)
        {
            strcpy(SMSSender,VTSData.PhoneNumber.Mob0);
            #ifndef PROTO_CDAC
            memset(OTABuffer, 0x00, OTABufferSIZE);
            strncpy(OTABuffer, (char*)&fn[5], OTABufferSIZE-1);
            IsOTACmd = 1;
            nwy_dbg_log("MCU OTA CMD : %s", OTABuffer);
            #endif
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }
        
        fn = strstr(str,SMSCMD);
        if(fn)
        {
            DecodeSMSString(fn);
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }
        
        fn = strstr(str,HANDLING);
        if(fn)
        {
            if(fn[5]=='1')
            {
                #ifdef MCU_HARSH_ALERTS
                VAlert[HARSH_ACC_ALERT].Enable=1;
                AddAlert(HARSH_ACC_ALERT);
                nwy_dbg_log("********* HARSH ACCEL ALERT********\n");
                #endif
            }
            else if(fn[5]=='2')
            {
                #ifdef MCU_HARSH_ALERTS
                VAlert[HARSH_BRK_ALERT].Enable=1;
                AddAlert(HARSH_BRK_ALERT);
                nwy_dbg_log("********* HARSH BRAKE ALERT********\n");
                #endif
            }
            else if(fn[5]=='3')
            {
                #ifdef MCU_HARSH_ALERTS
                VAlert[RASH_TURN_ALERT].Enable=1;
                AddAlert(RASH_TURN_ALERT);
                nwy_dbg_log("********* RASH TURN ALERT********\n");
                #endif
            }
            #ifdef PROTO_CDAC
            // else if(fn[5]=='4')
            // {
            //     VAlert[TILT_ALERT].Enable=1;
            //     AddAlert(TILT_ALERT);
            //     nwy_dbg_log("********* VEHICLE TILT ALERT (IMMEDIATE)********\n");
            //     #ifdef PROTO_CDAC
            //     SMSAlert(22);
            //     #endif
            // }
            #endif
            else if(fn[5]=='5')
            {
                if(!PeriPheralVal.IsTilt)
                {
                    #ifdef PROTO_CDAC
                    if(!VAlert[TILT_ALERT].Enable)
                    {
                        VAlert[TILT_ALERT].Enable=1;
                        AddAlert(TILT_ALERT);
                        nwy_dbg_log("********* VEHICLE TILT ALERT (RE-EN)********\n");
                        #ifdef PROTO_CDAC
                        // CDAC spec 4.iii req 6 & 7: Server first, then SMS
                        // If no GPRS, send SMS immediately (req 7)
                        if(GSM.GSMState < GPRS_ACTIVE) {
                            SMSAlert(22);
                        } else {
                            PeriPheralVal.PendingSMSAlert = 22;
                        }
                        #endif
                    }
                    #endif
                    PeriPheralVal.IsTilt=1;
                }
            }
            else if(fn[5]=='6')
            {
                #ifdef PROTO_CDAC
                if(VAlert[TILT_ALERT].Enable)
                {
                    RemoveAlert(TILT_ALERT);
                    nwy_dbg_log("********* VEHICLE TILT ALERT REMOVED********\n");
                }
                #endif
                PeriPheralVal.IsTilt=0;
            }
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }

        // Check for simple OK/ERR responses (often come bundled with other packets)
        fn = strstr(str,MCUOK);
        if(fn)
        {
            MCUOKRes=1;
            nwy_dbg_log("MCU ok Res");
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }

        fn = strstr(str,MCUERR);
        if(fn)
        {
            MCUErrRes=1;
            nwy_dbg_log("MCU ERROR Res");
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }
        
        fn = strstr(str,MCUVER);
        if(fn)
        {
            ParseMCUVersion(fn);
            next_packet = strstr(fn, "\r\n");
            if(next_packet) {
                str = next_packet + 2;
                parsed = 1;
                continue;
            } else {
                return;
            }
        }
        
      
        
    } while(parsed); // Continue parsing while we're finding packets

}

uint8_t GetHistoryData(char *data)
{
    char ss[10];
    #ifdef HISTORY_DISABLED
    nwy_dbg_log("Get HPacket data attempt but history disabled!");
    return 0;
    #endif
    nwy_dbg_log("Getting History Last Data...");
    sprintf(ss,"%s%s%s",MCUHEADER,GETHISTORY,MCUFOOTER);
    IsMCOMMHData=0;
    UartStringOut(ss);
    if(!WaitMCUReply(&IsMCOMMHData,1000))
        return 0;
    strcpy(data,MCOMMHPacket);
    nwy_dbg_log("Success, History Datalen : %d",strlen(data));
    return 1;

}

uint8_t GetHistoryCount(uint16_t *count)
{
    char ss[10];
    #ifdef HISTORY_DISABLED
    nwy_dbg_log("Get HPacket count attempt but history disabled!");
    return 0;
    #endif
    nwy_dbg_log("Getting History Count...");
    sprintf(ss,"%s%s%s",MCUHEADER,GETHCOUNT,MCUFOOTER);
    IsHCountReply=0;
    UartStringOut(ss);
    if(!WaitMCUReply(&IsHCountReply,1000))
        return 0;
    *count = HPacketCount;
    nwy_dbg_log("Success, History Count : %d",*count);
    return 1;

}

uint8_t WriteHistoryData(char* data)
{
    #ifdef HISTORY_DISABLED
    nwy_dbg_log("Saving Packet attempt but history disabled!");
    return 0;
    #endif
    
    nwy_dbg_log("Saving Packet in History...");
    if(strlen(data) > 256)
    {
        nwy_dbg_log("invalid hitory write len %d > 256",strlen(data));
        return 0;
    }
    char sbuff[300] = {0};
    sprintf(sbuff,"%s%s,%s%s",MCUHEADER,ADDHISTORY,data,MCUFOOTER);
    MCUOKRes=0;
    UartStringOut(sbuff);
    if(!WaitMCUReply(&MCUOKRes,1000))
        return 0;
    nwy_dbg_log("Add history Success, len %d",strlen(data));
    return 1;

}

uint8_t DeleteHistoryData(void)
{
    char ss[10] = {0};
    nwy_dbg_log("Deleting Last Packet in History...");
    sprintf(ss,"%s%s%s",MCUHEADER,DELHISTORY,MCUFOOTER);
    MCUOKRes=0;
    UartStringOut(ss);
    if(!WaitMCUReply(&MCUOKRes,1000))
        return 0;

    nwy_dbg_log("Last Packet Delete Success in History...");
    return 1;
}

void InitGPSParam(void)
{
    GPS.GPSFix=0;
	GPS.Latitude=0;
	GPS.LatDir='N';
	GPS.Longitude=0;
	GPS.LngDir='E';
	GPS.Speed=0.00;
	GPS.Altitude=0;
	GPS.HDOP=0;
	GPS.Heading=0;
	GPS.NoOfSatalite=0;
	GPS.PDOP=0;
	sprintf(sLatitude,"%03.7f",GPS.Latitude);
    sprintf(sLongitude,"%03.7f",GPS.Longitude);
    sprintf(sHeading,"%06.2f",GPS.Heading);
    sprintf(sHDOP,"%0.1f",GPS.HDOP);
    sprintf(sPDOP,"%0.1f",GPS.PDOP);
    #ifdef BSNL_PROTO
    sprintf(sAltitude,"%05.2f",GPS.Altitude);
    #else
    sprintf(sAltitude,"%0.1f",GPS.Altitude);
    #endif
    #ifdef BSNL_PROTO
    sprintf(sSpeed,"%04.1f",GPS.Speed);
    #else
    sprintf(sSpeed,"%05.1f",GPS.Speed);
    #endif


}


///---------------------------------MOTA----------------------------------------------//

#define PRINT_UART_DIV 8



uint16_t mcu_chksum(const uint8_t *data, int len)
{
    uint8_t chk[2]={0,0};
    for(int i =0; i < len; i++)
    {
        chk[0] += data[i];
        chk[1] += chk[0];
        
    }
    chk[0] &= 0xff;
    chk[1] &= 0xff;
    return (chk[0] << 8)| (chk[1]);
}

void PrintUartData(const uint8_t *str, uint32_t len, uint8_t IsSend)
{
    char ss[100];
    char st[20];
    uint16_t i,j, count=0;
    uint8_t seg;
    if(IsSend)
        strcpy(ss,"\nMCU SEND: \n");
    else
        strcpy(ss,"\nMCU RCV: \n");

    nwy_dbg_log(ss);

    seg = 1+ (len/PRINT_UART_DIV);
    for(i =0;i<seg;i++)
    {
        memset(ss,0x00,100);
        for(j=0;j<PRINT_UART_DIV;j++)
        {
            if(count < len)
            {
                sprintf(st,"%02X ",str[count++]);
                strcat(ss,st);
            }
        }
        InsertChar(ss,'\n');
        nwy_dbg_log(ss);
    }
    
}

uint8_t SendUart2Data(uint8_t* data, uint16 sz)
{
    int ret;
    PrintUartData(data,sz,1);
    ret = nwy_uart_send_data(MCUUart,data,sz);
    if(ret)
        return 1;
    else
        return 0;
}

MOTAReplyTypedef MOTAReply;
MOTAStateTypedef MOTAState;
uint8_t CanMCURes;
uint8_t IsMCUVerReply;
uint8_t IsMOTAProcess;
char MCUVersion[10];


uint8_t WaitForMCUVerReply(void)
{
    uint16_t timeout = MCU_REPLY_TIMEOUT/10;
    while(IsMCUVerReply==0)
    {
        nwy_sleep(10);
        if(--timeout == 0)
        {
            nwy_dbg_log("MCU Version REPLY TIMEOUT");
            return 0;
        }
    }
    if(CanMCURes==MCU_RES_ERR)
        return 0;
    return 1;
}


uint8_t WaitForMCUMOTAReply(void)
{
    uint16_t timeout = MCU_REPLY_TIMEOUT/10;
    while(MOTAState!=MOTA_REPLY)
    {
        nwy_sleep(10);
        if(--timeout == 0)
        {
            nwy_dbg_log("MCU MOTA ACK Timout!");
            return 0;
        }
    }
    return 1;

}

uint8_t GetMCUVersionReq(void)
{
    // Clear any previous version data
    IsMCUVerReply=0;
    CanMCURes=MCU_RES_INIT;
    memset(MCUVersion, 0x00, sizeof(MCUVersion));
    
    // Wait for MCU thread to finish processing any pending data
    // This prevents version response from being dropped due to buffer occupied
    // MCU sends GIP/AUP/GPS autonomously, so we need to wait for buffer to clear
    uint8_t retry = 0;
    while(MCOMMRxLen != 0 && retry < 10)
    {
        nwy_sleep(5);
        retry++;
    }
    
    if(MCOMMRxLen != 0)
    {
        nwy_dbg_log("MCU Version request: RX buffer still busy after wait");
        // Continue anyway - version response might still work
    }
    
    UartStringOut("%s%s%s",MCUHEADER,MCUVER,MCUFOOTER);
    
    // Small delay to allow MCU thread to wake up and process response
    // MCU thread has 30ms sleep cycle, give it time to catch the response
    nwy_sleep(5);
    
    if(!WaitForMCUVerReply())
        return 0;
    return 1;
}

void ParseMCUVersion(char* str)
{
    char *fn, *ln;
    fn = strchr(str,',');
    if(!fn)
    {
        nwy_dbg_log("MCU Version parse error: no comma found");
        CanMCURes = MCU_RES_ERR;
        IsMCUVerReply = 1;
        return;
    }
    fn++;
    ln = strchr(fn,'\r');
    if(!ln)
    {
        nwy_dbg_log("MCU Version parse error: no \\r found");
        CanMCURes = MCU_RES_ERR;
        IsMCUVerReply = 1;
        return;
    }
    *ln = 0;
    if(strlen(fn) > 9)
    {
        nwy_dbg_log("mcu version reply len %d > 9",strlen(fn));
        CanMCURes = MCU_RES_ERR;
        IsMCUVerReply = 1;
        return;
    }
    strcpy(MCUVersion,fn);
    CanMCURes = MCU_RES_OK;
    IsMCUVerReply=1;
    //SendOKPacket();
    nwy_dbg_log("MCU Version: %s", MCUVersion);
}


uint8_t SendMCUOTAStartPacket(int size)
{
    if(size > (28*1024) || size <= 0)
    {
        nwy_dbg_log("MCU OTA file size err : %d",size);   
        return 0;
    }
    MCUOKRes=0;
    UartStringOut("%s%s,%d%s",MCUHEADER,MOTASTART,size,MCUFOOTER);
    
    
    if(!WaitMCUReply(&MCUOKRes,MCU_REPLY_TIMEOUT))
        return 0;
    return 1;

}

uint8_t CheckMotaReply(const uint8_t* buff, uint32_t LoopdataCount, uint32_t LoopCount, MOTAReplyTypedef* Reply)
{
    uint16_t CheckSum; 

    if(LoopCount!= Reply->pktcount)
    {
        nwy_dbg_log("MCU OTA reply packt num err, exp:%d, got%d !",LoopCount,Reply->pktcount);
        return 0;
    }
    //nwy_dbg_log("Mota confirmation Data count %d",LoopdataCount);;
    CheckSum = mcu_chksum(buff,LoopdataCount);
    if(CheckSum != Reply->checksum)
    {
        nwy_dbg_log("MCU OTA reply packt chksum err, exp:0x%02X, got0x%02X !",CheckSum,Reply->checksum);
        return 0;
    }
    nwy_dbg_log("MCU OTA chksum: %d",Reply->checksum);
    return 1;
}

uint8_t SendMCUOTADataPacket(const uint8_t *data, uint32_t LoopdataCount)
{
    uint8_t buff[260];
    memset(buff,0x00,260);
    buff[0] = MCUMOTAHEADER;
    buff[1] = CANMCU_ID_MOTADATA;
    buff[2] = (LoopdataCount-1)&0xff;
    memcpy((void*)&buff[3],data,LoopdataCount);
    buff[259] = MCUMOTAFOOTER;
    MOTAState=MOTA_WAIT;
    if(!SendUart2Data(buff,260))
        return 0;

    return WaitForMCUMOTAReply();
    
}

uint8_t SendMCUOTACompletePacket(void)
{
    uint8_t buff[20];   
    memset(buff,0x00,20);
    buff[0]= MCUMOTAHEADER;
    buff[1]= CANMCU_ID_MOTADONE;
    buff[2]= MCUMOTAFOOTER;
    CanMCURes=MCU_RES_INIT;
    if(!SendUart2Data(buff,260))
        return 0;
    return WaitMCUReply(&MCUOKRes,MCU_REPLY_TIMEOUT);
 
}

uint8_t ProcessMCUOTA(char* Filename)
{
    int fd,size,ret;
    uint32_t Remainingdatasize,i,LoopDataCount;
    uint8_t buff[BMS_OTA_LOOP_DATASIZE]={0};   
    fd = nwy_sdk_fopen(Filename,NWY_RDONLY);
    if(fd<=0)
    {
        nwy_dbg_log("MCU OTA file not found!");
        return 0;
    }
    size = nwy_sdk_fsize_fd(fd);
    if(!SendMCUOTAStartPacket(size))
    {
        goto ERROR_RET;
    }
    IsMOTAProcess=1;
    i=0;
    Remainingdatasize = size;
    while(Remainingdatasize>0)
    {
        LoopDataCount = Remainingdatasize;
        if(LoopDataCount>BMS_OTA_LOOP_DATASIZE)
            LoopDataCount = BMS_OTA_LOOP_DATASIZE;
        nwy_sdk_fseek(fd,size-Remainingdatasize,0);
        ret = nwy_sdk_fread(fd,buff,LoopDataCount);
        if(ret !=LoopDataCount)
        {
            nwy_dbg_log("MCU OTA file read size err : %d",size);
            goto ERROR_RET;
        }
        if(!SendMCUOTADataPacket((const uint8_t*)buff,LoopDataCount))
        {
            nwy_dbg_log("MCU OTA Loop acknowledgement err!");
            goto ERROR_RET;
        }
        if(!CheckMotaReply((const uint8_t*)buff,LoopDataCount,i,&MOTAReply))
        {
            goto ERROR_RET;
        }
        Remainingdatasize-=LoopDataCount;
        i++;
        nwy_dbg_log("MCU OTA Sent %d/%d",size-Remainingdatasize,size);
    }
    nwy_sdk_fclose(fd);
    if(!SendMCUOTACompletePacket())
    {
        nwy_dbg_log("MCU OTA Complete acknowledgement err!");
        IsMOTAProcess=0;
        return 0;
    }
    nwy_dbg_log("MCU OTA Complete!");
    IsMOTAProcess=0;
    return 1;


    ERROR_RET:
    nwy_sdk_fclose(fd);
    IsMOTAProcess=0;
    return 0;


}

void ParseMotaReply(uint8_t *buf)
{
    buf+=MCU_PACKET_HEADER_AND_TYPE_OVERHEAD;
     if(buf[MCU_MOTAREPLY_FOOTER_INDEX] != MCUMOTAFOOTER)
        return;

    memset((void*)&MOTAReply,0x00,sizeof(MOTAReplyTypedef));
    MOTAReply.pktcount = (buf[0]<<8) | buf[1];
    MOTAReply.checksum = (buf[2]<<8) | buf[3];
    MOTAState = MOTA_REPLY;
}

void ParseMCUSMota(uint8_t* Buff)
{
    if(Buff[MCU_PACKET_HEADER_INDEX] != MCUMOTAHEADER)
    {
        nwy_dbg_log("No MCU header found");
        PrintUartData(Buff,MCOMMRxLen,0);
        return;
    }


    if(Buff[MCU_PACKET_TYPE_INDEX]==CANMCU_ID_MOTAREP)
    {
        if(MOTAState==MOTA_WAIT)
            ParseMotaReply(Buff);

        return;   
    }
    else
    {
        nwy_dbg_log("Invalid Mota Packet :");
        PrintUartData(Buff,MCOMMRxLen,0);
    }
    return;
}





///----------------------------------------------------------------------


void MCUThreadEntry(void *param)
{
    nwy_sleep(500);
    nwy_dbg_log("MCU Thread Entry");
    InitGPSParam();
    //SendGyroSettingPacket();
    while (1)
    {
        if(MCOMMRxLen)
        {
            #ifdef ENABLE_MCU_RX_DEBUG
            nwy_dbg_log("MCU RX Buffer (%d bytes): %s", MCOMMRxLen, MCOMMRxBuffer);
            #endif
            
            if(IsMOTAProcess)
                ParseMCUSMota((uint8_t*)MCOMMRxBuffer);
            else
                ParseMCUString(MCOMMRxBuffer);

            memset(MCOMMRxBuffer,0x00,MCOMM_RX_MAX);
            MCOMMRxLen=0;
        }
        nwy_sleep(30);
        //SendOKPacket();
    }

    nwy_exit_thread_self();
}