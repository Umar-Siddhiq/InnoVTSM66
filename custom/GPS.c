#include "GPS.h"
#include "ql_stdlib.h"
#include "Hardware.h"
#include "Alert.h"
#include "SMS.h"
#include "Geofence.h"
#include "EPO.h"
#include "File.h"

// Define a simple LCG random number generator to avoid unsupported Ql_rand() log flood
static uint32_t s_rand_seed = 12345;
static s32 my_rand(void)
{
    s_rand_seed = s_rand_seed * 1103515245 + 12345;
    return (s32)((s_rand_seed / 65536) % 32768);
}
#define Ql_rand my_rand
u8 *gps_uart_buffer = NULL;
uint8_t gps_data_available = 0;
GPS_Typedef GPS = {0};
static uint8_t ovsCount = 0;  // Over-speed counter
GLLTypedef GLLData = {{0}};
GGATypedef GGAData = {{0}};
RMCTypedef RMCData = {{0}};
GSVTypedef GSVData = {0};
GSATypedef GSAData = {0};
VTGTypedef VTGData = {{0}};
ZDATypedef ZDAData = {{0}};
GSTTypedef GSTData = {{0}};
extern uint8_t IsFotaProcessing;
extern uint8_t IsMotaProcessing;

// Global command status
GPS_CmdStatus gps_cmd = {
    .type = GPS_CMD_NONE,
    .cmd_prefix = NULL,
    .response_pending = false,
    .response_received = false,
    .response_ok = false,
    .response = {0}
};

uint16_t GPSTimeout = 0;
// NOTE: gpstmpdata moved to dynamic allocation to prevent global memory corruption
static u8 *gpstmpdata = NULL;  // Will be allocated dynamically
char sLatitude[20] = {0};
char sLongitude[20] = {0};
char sAltitude[20] = {0};
char sSpeed[20] = {0};
char sPDOP[20] = {0};
char sHDOP[20] = {0};
char sHeading[20]  = {0};
_RTC GPSDateTime = {0};
LastFixedGPS_Typedef LastFixedGPS = {0};

// Store last valid NMEA data for RS232 forwarding - moved to dynamic allocation
static char *last_nmea_data = NULL;
static uint8_t nmea_data_ready = 0;

// -----------------------------------------------------------------------------
// AGNSS reference aiding (PAIR590/PAIR600)
// Runs after GNSS boot, but never during baudrate switching / other UART cmds.
// -----------------------------------------------------------------------------

void gps_switch_baudrate(void);

static uint8_t g_agnss_aid_enabled = 1;
static uint8_t g_agnss_time_sent = 0;
static uint8_t g_agnss_pos_sent = 0;
static uint8_t g_agnss_attempts = 0;
static u32 g_agnss_next_try_ms = 0;

static volatile uint8_t g_gps_reinit_request = 0;

void GPS_RequestReinit(void)
{
    g_gps_reinit_request = 1;
}

static void gps_agnss_schedule(u32 delayMs)
{
    g_agnss_time_sent = 0;
    g_agnss_pos_sent = 0;
    g_agnss_attempts = 0;
    g_agnss_next_try_ms = Ql_GetMsSincePwrOn() + delayMs;
}

static void gps_agnss_poll(void)
{
    if (!g_agnss_aid_enabled) {
        return;
    }
    if (g_agnss_time_sent && g_agnss_pos_sent) {
        return;
    }
    if (g_agnss_attempts >= 12) {
        // Don't keep spamming forever (esp. if no valid time/pos available).
        return;
    }
    if (gps_cmd.response_pending) {
        // Another UART command is in progress (e.g. baud switch); don't interfere.
        return;
    }
    u32 now = Ql_GetMsSincePwrOn();
    if (now < g_agnss_next_try_ms) {
        return;
    }

    bool didSomething = false;

    if (!g_agnss_time_sent) {
        if (EPO_SendReferenceTimeNow()) {
            g_agnss_time_sent = 1;
            LOGData(TAG_GPS, "AGNSS: Ref UTC sent (PAIR590)");
        }
        didSomething = true;
    }

    if (!g_agnss_pos_sent) {
        if (EPO_SendReferencePositionNow()) {
            g_agnss_pos_sent = 1;
            LOGData(TAG_GPS, "AGNSS: Ref Pos sent (PAIR600)");
        }
        didSomething = true;
    }

    g_agnss_attempts++;
    // Retry later; if time wasn't valid yet, it may become valid once CTZU syncs.
    g_agnss_next_try_ms = Ql_GetMsSincePwrOn() + (didSomething ? 5000 : 8000);
}

static void gps_handle_reinit_request(void)
{
    if (!g_gps_reinit_request) {
        return;
    }
    if (gps_cmd.response_pending) {
        // Don't interrupt an in-flight PAIR exchange (baud switch, etc.).
        return;
    }

    g_gps_reinit_request = 0;
    LOGData(TAG_GPS, "GPS: reinit requested (reset + baud resync)\r\n");

    // Stop UART while we reboot GNSS.
    Ql_UART_Close(GPS_UART_PORT);
    ThreadSleep(50);

    // Hard reset GNSS so it comes up at default baud (usually 9600).
    GPS_RESET_ON;
    ThreadSleep(200);
    GPS_RESET_OFF;
    ThreadSleep(800);

    // Re-open at default baud and re-apply our normal baudrate switch procedure.
    int ret = Ql_UART_Open(GPS_UART_PORT, 9600, FC_NONE);
    if (ret < QL_RET_OK) {
        LOGData(TAG_GPS, "GPS: UART open @9600 failed, ret=%d\r\n", ret);
        return;
    }

    gps_switch_baudrate();
    gps_agnss_schedule(3000);
}


// Helper function to calculate NMEA checksum
static uint8_t calculate_nmea_checksum(const char* sentence) {
    uint8_t checksum = 0;
    // Skip the $ character
    sentence++;
    // Calculate checksum until * character
    while (*sentence && *sentence != '*') {
        checksum ^= *sentence++;
    }
    return checksum;
}

// -----------------------------------------------------------------------------
// Binary protocol ACK support (for Flash EPO, etc.)
// -----------------------------------------------------------------------------

#define GPS_BINARY_ACK_PREAMBLE1 0x04
#define GPS_BINARY_ACK_PREAMBLE2 0x24
#define GPS_BINARY_ACK_END1      0xAA
#define GPS_BINARY_ACK_END2      0x44

#define GPS_BINARY_RX_BUF_SIZE   64

static volatile uint16_t g_gps_bin_last_ack_for = 0;
static volatile uint32_t g_gps_bin_last_ack_tick = 0;

static uint8_t g_gps_bin_rx_buf[GPS_BINARY_RX_BUF_SIZE];
static uint32_t g_gps_bin_rx_len = 0;

static uint8_t gps_xor_checksum(const uint8_t* data, uint32_t len)
{
    uint8_t cs = 0;
    for (uint32_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

static void gps_binary_rx_push(const uint8_t* data, uint32_t len)
{
    if (!data || len == 0) {
        return;
    }

    for (uint32_t i = 0; i < len; i++) {
        if (g_gps_bin_rx_len >= GPS_BINARY_RX_BUF_SIZE) {
            // shift left by one
            Ql_memmove(g_gps_bin_rx_buf, g_gps_bin_rx_buf + 1, GPS_BINARY_RX_BUF_SIZE - 1);
            g_gps_bin_rx_len = GPS_BINARY_RX_BUF_SIZE - 1;
        }
        g_gps_bin_rx_buf[g_gps_bin_rx_len++] = data[i];
    }
}

static void gps_binary_rx_scan(void)
{
    // Look for complete frames in buffer (we only need ACK frames: MsgID 0x03E8, Len 4)
    // Frame: 0x04 0x24 <msgidLE:2> <lenLE:2> <payload:len> <cs:1> 0xAA 0x44
    uint32_t i = 0;
    while (g_gps_bin_rx_len >= 13 && i + 13 <= g_gps_bin_rx_len) {
        if (g_gps_bin_rx_buf[i] != GPS_BINARY_ACK_PREAMBLE1 || g_gps_bin_rx_buf[i + 1] != GPS_BINARY_ACK_PREAMBLE2) {
            i++;
            continue;
        }

        uint16_t msgId = (uint16_t)g_gps_bin_rx_buf[i + 2] | ((uint16_t)g_gps_bin_rx_buf[i + 3] << 8);
        uint16_t plen  = (uint16_t)g_gps_bin_rx_buf[i + 4] | ((uint16_t)g_gps_bin_rx_buf[i + 5] << 8);

        uint32_t frameLen = 2 + 2 + 2 + (uint32_t)plen + 1 + 2;
        if (i + frameLen > g_gps_bin_rx_len) {
            // Not enough bytes yet
            break;
        }

        if (g_gps_bin_rx_buf[i + frameLen - 2] != GPS_BINARY_ACK_END1 || g_gps_bin_rx_buf[i + frameLen - 1] != GPS_BINARY_ACK_END2) {
            i++;
            continue;
        }

        // checksum is XOR over msgid+len+payload (i+2 .. i+2+2+2+plen-1)
        uint8_t expectedCs = gps_xor_checksum(&g_gps_bin_rx_buf[i + 2], 2 + 2 + (uint32_t)plen);
        uint8_t cs = g_gps_bin_rx_buf[i + 2 + 2 + 2 + plen];
        if (cs != expectedCs) {
            i++;
            continue;
        }

        if (msgId == 0x03E8 && plen == 4) {
            uint16_t ackFor = (uint16_t)g_gps_bin_rx_buf[i + 6] | ((uint16_t)g_gps_bin_rx_buf[i + 7] << 8);
            g_gps_bin_last_ack_for = ackFor;
            g_gps_bin_last_ack_tick = Ql_GetMsSincePwrOn();
#ifdef EPO_VERBOSE_LOG
            LOGData(TAG_GPS, "BIN ACK for msgId=%u", ackFor);
#endif
        }

        // Consume frame bytes from buffer: drop [0 .. i+frameLen)
        uint32_t drop = i + frameLen;
        if (drop >= g_gps_bin_rx_len) {
            g_gps_bin_rx_len = 0;
        } else {
            Ql_memmove(g_gps_bin_rx_buf, g_gps_bin_rx_buf + drop, g_gps_bin_rx_len - drop);
            g_gps_bin_rx_len -= drop;
        }
        i = 0;
    }
}

void GPS_BinaryAckReset(void)
{
    g_gps_bin_last_ack_for = 0;
    g_gps_bin_last_ack_tick = 0;
}

bool GPS_WaitBinaryAck(uint16_t ackForMsgId, uint32_t timeoutMs)
{
    uint32_t start = Ql_GetMsSincePwrOn();
    while (Ql_GetMsSincePwrOn() - start < timeoutMs) {
        if (g_gps_bin_last_ack_for == ackForMsgId && g_gps_bin_last_ack_tick >= start) {
            return true;
        }
        ThreadSleep(20);
    }
    return false;
}

int GPS_UartSendRaw(const uint8_t* data, uint16_t len)
{
    if (!data || len == 0) {
        return 0;
    }
    return Ql_UART_Write(GPS_UART_PORT, (u8*)data, len);
}

void GPS_UartSendString(const char *str) {
    if (str == NULL || Ql_strlen(str) == 0) {
        return;
    }
    int ret = Ql_UART_Write(GPS_UART_PORT, (u8 *)str, Ql_strlen(str));
    if (ret < 0) {
        LOGData(TAG_GPS, "GPS UART send error %d\r\n", ret);
    } else {
        // Limit log output to prevent DBG_BUFFER overflow
        if (Ql_strlen(str) > 100) {
            char sample[101];
            Ql_memset(sample, 0, sizeof(sample));
            Ql_strncpy(sample, str, 100);
            LOGData(TAG_GPS, "GPS UART sent(100): %s...\r\n", sample);
        } else {
            LOGData(TAG_GPS, "GPS UART sent: %s\r\n", str);
        }
    }
    ThreadSleep(100);
}


//--------------------------------------------------------------------------


//#define GPS_UART_DEBUG

s32 gps_uart_read(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen)
{
    s32 rdLen = 0;
    s32 rdTotalLen = 0;
    if (NULL == pBuffer || 0 == bufLen)
    {
        LOGData(TAG_GPS,"<-- Invalid buffer or length! -->\r\n");
        return -1;
    }
    Ql_memset(pBuffer, 0x0, bufLen);
    while (1)
    {
        // Reserve 1 byte for null terminator and check we don't overflow
        if (rdTotalLen >= bufLen - 1) {
            LOGData(TAG_GPS,"Buffer full, stopping read at %d bytes\r\n", rdTotalLen);
            break;
        }
        rdLen = Ql_UART_Read(port, pBuffer + rdTotalLen, bufLen - rdTotalLen - 1);
        if (rdLen <= 0)  // All data is read out, or Serial Port Error!
        {
            break;
        }
        rdTotalLen += rdLen;
        // Continue to read...
    }
    if (rdLen < 0) // Serial Port Error!
    {
        LOGData(TAG_GPS,"Fail to read from port[%d], ret:%d\r\n", port,rdLen);
        return -99;
    }
    // Ensure null termination
    pBuffer[rdTotalLen] = '\0';
    return rdTotalLen;
}

// Helper function to check for command responses
static bool check_cmd_response(const char* data) {
    if (!gps_cmd.response_pending || !gps_cmd.cmd_prefix || !data) {
        return false;
    }

    // Check if this data starts with our expected command prefix
    if (Ql_strstr(data, gps_cmd.cmd_prefix) == NULL) {
        return false;
    }
    LOGData(TAG_GPS, "Detected command response for %s\r\n", gps_cmd.cmd_prefix);
    // Store response with bounds checking
    Ql_memset(gps_cmd.response, 0, sizeof(gps_cmd.response));
    Ql_strncpy(gps_cmd.response, data, sizeof(gps_cmd.response) - 1);
    gps_cmd.response[sizeof(gps_cmd.response) - 1] = '\0';
    gps_cmd.response_received = true;

    if(Ql_strstr(data, "ERROR") != NULL) {
        gps_cmd.response_ok = false;
    }
    else
    {
        gps_cmd.response_ok = true;
    }
    return true;
}

void gps_uart_cb(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{

    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    #ifdef GPS_UART_DEBUG
    LOGData(TAG_GPS,"GPS cb: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    #endif
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
        {
            if (GPS_UART_PORT == port)
            {
                
                s32 totalBytes = gps_uart_read(port, gpstmpdata, GPS_UART_BUFFER_SIZE);
                if (totalBytes <= 0)
                {
                    LOGData(TAG_GPS,"read error ret:%d-->",totalBytes);
                    return;
                }

                // Feed binary frame scanner first (works on raw bytes, not strings)
                gps_binary_rx_push((uint8_t*)gpstmpdata, (uint32_t)totalBytes);
                gps_binary_rx_scan();

                // If this chunk looks like binary traffic, don't push it into NMEA parser buffers
                if ((uint8_t)gpstmpdata[0] == GPS_BINARY_ACK_PREAMBLE1 && (uint8_t)gpstmpdata[1] == GPS_BINARY_ACK_PREAMBLE2) {
                    GPSTimeout = GPS_RCV_TIMEOUT;
                    return;
                }

                // Check if this is a command response
                if (check_cmd_response((char*)gpstmpdata)) {
                    GPSTimeout =GPS_RCV_TIMEOUT;
                }


                if(gps_data_available)
                {
                    LOGData(TAG_GPS,"GPS Rcv While Processing Data\r\n");
                    // Don't log full gpstmpdata - it can overflow DBG_BUFFER (512 bytes)
                    // Only log first 200 chars as sample
                    char sample[201];
                    Ql_memset(sample, 0, sizeof(sample));
                    Ql_strncpy(sample, (char*)gpstmpdata, 200);
                    LOGData(TAG_GPS,"Data sample(200): %s\r\n", sample);
                    return;
                }

                // Safely copy with bounds checking
                Ql_memset(gps_uart_buffer, 0, GPS_UART_BUFFER_SIZE);
                Ql_strncpy((char*)gps_uart_buffer, (char*)gpstmpdata, GPS_UART_BUFFER_SIZE - 1);
                gps_uart_buffer[GPS_UART_BUFFER_SIZE - 1] = '\0';  // Ensure null termination
                #ifdef GPS_UART_DEBUG
                    // Don't log full buffer - truncate to prevent DBG_BUFFER overflow
                    char debug_sample[201];
                    Ql_memset(debug_sample, 0, sizeof(debug_sample));
                    Ql_strncpy(debug_sample, (char*)gps_uart_buffer, 200);
                    LOGData(TAG_GPS,"<-- GPS Data(200): %s -->\r\n", debug_sample);
                #endif

                gps_data_available = 1; // Set flag to indicate data is available
            }
            break;
        }
        
    case EVENT_UART_READY_TO_WRITE: 
        break;
    default:
        break;
    }
}

#if FEATURE_GPS_LAST_FIX_FALLBACK
void GPS_LoadLastFixedFromFlash(void)
{
    Ql_memset(&LastFixedGPS, 0, sizeof(LastFixedGPS_Typedef));
    if (LoadFromFlash(LAST_GPS_FILE_PATH, &LastFixedGPS, sizeof(LastFixedGPS_Typedef), NULL))
    {
        if (LastFixedGPS.Magic == LAST_GPS_MAGIC && LastFixedGPS.Valid == 1 &&
            LastFixedGPS.Latitude != 0.0 && LastFixedGPS.Longitude != 0.0)
        {
            LOGData(TAG_GPS, "Loaded Last Fixed GPS from Flash: %s %c, %s %c, Alt: %s, Spd: %s",
                    LastFixedGPS.sLatitude, LastFixedGPS.LatDir,
                    LastFixedGPS.sLongitude, LastFixedGPS.LngDir,
                    LastFixedGPS.sAltitude, LastFixedGPS.sSpeed);

            GPS.Latitude = LastFixedGPS.Latitude;
            GPS.Longitude = LastFixedGPS.Longitude;
            GPS.Altitude = LastFixedGPS.Altitude;
            GPS.LatDir = LastFixedGPS.LatDir;
            GPS.LngDir = LastFixedGPS.LngDir;
            GPS.Speed = LastFixedGPS.Speed;
            GPS.Heading = LastFixedGPS.Heading;
            GPS.PDOP = LastFixedGPS.PDOP;
            GPS.HDOP = LastFixedGPS.HDOP;
            GPS.NoOfSatalite = LastFixedGPS.NoOfSatalite;
            Ql_strncpy(sLatitude, LastFixedGPS.sLatitude, sizeof(sLatitude) - 1);
            sLatitude[sizeof(sLatitude) - 1] = '\0';
            Ql_strncpy(sLongitude, LastFixedGPS.sLongitude, sizeof(sLongitude) - 1);
            sLongitude[sizeof(sLongitude) - 1] = '\0';
            Ql_strncpy(sAltitude, LastFixedGPS.sAltitude, sizeof(sAltitude) - 1);
            sAltitude[sizeof(sAltitude) - 1] = '\0';
            Ql_strncpy(sSpeed, LastFixedGPS.sSpeed, sizeof(sSpeed) - 1);
            sSpeed[sizeof(sSpeed) - 1] = '\0';
            Ql_strncpy(sHeading, LastFixedGPS.sHeading, sizeof(sHeading) - 1);
            sHeading[sizeof(sHeading) - 1] = '\0';
            Ql_strncpy(sPDOP, LastFixedGPS.sPDOP, sizeof(sPDOP) - 1);
            sPDOP[sizeof(sPDOP) - 1] = '\0';
            Ql_strncpy(sHDOP, LastFixedGPS.sHDOP, sizeof(sHDOP) - 1);
            sHDOP[sizeof(sHDOP) - 1] = '\0';
            return;
        }
    }
    Ql_memset(&LastFixedGPS, 0, sizeof(LastFixedGPS_Typedef));
    LOGData(TAG_GPS, "No valid Last Fixed GPS in Flash");
}

void GPS_SaveLastFixedToFlash(void)
{
    if (LastFixedGPS.Valid && LastFixedGPS.Latitude != 0.0 && LastFixedGPS.Longitude != 0.0)
    {
        LastFixedGPS.Magic = LAST_GPS_MAGIC;
        SaveToFlash(LAST_GPS_FILE_PATH, &LastFixedGPS, sizeof(LastFixedGPS_Typedef));
    }
}
#endif

void gps_parameter_init(void)
{
    Ql_memset(&GPS, 0, sizeof(GPS_Typedef));
    GPS.LatDir = 'N';
    GPS.LngDir = 'E';
    GPS.HDOP = 0.0;
    GPS.PDOP = 0.0;
    Ql_sprintf(sLatitude,"%3.6f", GPS.Latitude);
    Ql_sprintf(sLongitude,"%3.6f", GPS.Longitude);
    Ql_sprintf(sAltitude,"%4.2f", GPS.Altitude);
    Ql_sprintf(sSpeed,"%3.2f", GPS.Speed);
    Ql_sprintf(sHDOP,"%3.2f", GPS.HDOP);
    Ql_sprintf(sPDOP,"%3.2f", GPS.PDOP);
    Ql_sprintf(sHeading,"%3.1f", GPS.Heading);

#if FEATURE_GPS_LAST_FIX_FALLBACK
    GPS_LoadLastFixedFromFlash();
#endif
}

// Send UART configuration command and wait for response
static bool gps_send_uart_cmd(const char* cmd, uint32_t timeout_ms) {
    char command[100];
    uint8_t checksum;
    
    // Prepare command with checksum
    Ql_sprintf(command, "%s", cmd);
    checksum = calculate_nmea_checksum(command);
    Ql_sprintf(command + Ql_strlen(command), "*%02X\r\n", checksum);
    
    // Set command status
    gps_cmd.type = GPS_CMD_UART_CONFIG;
    gps_cmd.response_pending = true;
    gps_cmd.response_received = false;
    gps_cmd.response_ok = false;
    Ql_memset(gps_cmd.response, 0, sizeof(gps_cmd.response));
    
    // Send command
    GPS_UartSendString(command);
    
    // Wait for response with timeout
    uint32_t start_time = Ql_GetMsSincePwrOn();
    while (Ql_GetMsSincePwrOn() - start_time < timeout_ms) {
        if (gps_cmd.response_received) {
            gps_cmd.type = GPS_CMD_NONE;
            gps_cmd.response_pending = false;
            LOGData(TAG_GPS, "Command response: %s\r\n", gps_cmd.response);
            return gps_cmd.response_ok;
        }
        ThreadSleep(20);
    }
    
    // Timeout occurred
    gps_cmd.type = GPS_CMD_NONE;
    gps_cmd.response_pending = false;
    LOGData(TAG_GPS, "Command timeout\r\n");
    return false;
}

void gps_switch_baudrate(void)
{
    if (GPS_UART_BAUDRATE == 9600) {
        LOGData(TAG_GPS, "GPS baudrate already at 9600, no change needed\r\n");
        return;
    }

    // Setup baud command status
    gps_cmd.type = GPS_CMD_UART_CONFIG;
    gps_cmd.cmd_prefix = "$PAIR864";
    gps_cmd.response_pending = true;
    gps_cmd.response_received = false;
    gps_cmd.response_ok = false;
    Ql_memset(gps_cmd.response, 0, sizeof(gps_cmd.response));

    // Send command
    char cmd[50];
    Ql_sprintf(cmd, "$PAIR864,0,0,%d", GPS_UART_BAUDRATE);
    
    // Send command and wait for response
    if (gps_send_uart_cmd(cmd, 1000)) {
        // Setup reboot command status
        gps_cmd.type = GPS_CMD_REBOOT;
        gps_cmd.cmd_prefix = "$PAIR023";
        gps_cmd.response_pending = true;
        gps_cmd.response_received = false;
        gps_cmd.response_ok = false;
        Ql_memset(gps_cmd.response, 0, sizeof(gps_cmd.response));

        Ql_sprintf(cmd, "$PAIR023");

        // Send command and wait for response
        if (!gps_send_uart_cmd(cmd, 1000)) {
            LOGData(TAG_GPS, "Failed to Reboot GPS after command, skipping baud switch\r\n");
            return;
        }
    }
    else
    {
        LOGData(TAG_GPS, "Failed to set GPS baudrate.maybe already set\r\n");
    }
    

    
    // If we got OK response, switch the UART
    ThreadSleep(100);
    Ql_UART_Close(GPS_UART_PORT);
    ThreadSleep(200);
    
    int ret = Ql_UART_Open(GPS_UART_PORT, GPS_UART_BAUDRATE, FC_NONE);
    if (ret < QL_RET_OK) {
        LOGData(TAG_GPS, "Failed to switch UART baudrate to %d, ret=%d\r\n", GPS_UART_BAUDRATE, ret);
        return;
    }
    
    LOGData(TAG_GPS, "Successfully switched GPS baudrate to %d\r\n", GPS_UART_BAUDRATE);
}

int gps_init(void)
{
    int ret;
    gps_parameter_init();
    
    // Allocate GPS temporary data buffer
    if(gpstmpdata == NULL)
    {
        gpstmpdata = (u8*)Ql_MEM_Alloc(GPS_UART_BUFFER_SIZE);
        if(gpstmpdata == NULL)
        {
            LOGData(TAG_GPS,"Failed to allocate memory for gpstmpdata\r\n");
            return -1;
        }
        Ql_memset(gpstmpdata, 0, GPS_UART_BUFFER_SIZE);
        LOGData(TAG_GPS, "gpstmpdata allocated at: %p (size: %d)", gpstmpdata, GPS_UART_BUFFER_SIZE);
    }
    
    // Allocate last_nmea_data buffer
    if(last_nmea_data == NULL)
    {
        last_nmea_data = (char*)Ql_MEM_Alloc(GPS_UART_BUFFER_SIZE);
        if(last_nmea_data == NULL)
        {
            LOGData(TAG_GPS,"Failed to allocate memory for last_nmea_data\r\n");
            return -1;
        }
        Ql_memset(last_nmea_data, 0, GPS_UART_BUFFER_SIZE);
        LOGData(TAG_GPS, "last_nmea_data allocated at: %p (size: %d)", last_nmea_data, GPS_UART_BUFFER_SIZE);
    }
    
    if(gps_uart_buffer == NULL)
    {
        gps_uart_buffer = (u8*)Ql_MEM_Alloc(GPS_UART_BUFFER_SIZE);
        if(gps_uart_buffer == NULL)
        {
            LOGData(TAG_GPS,"Failed to allocate memory for GPS UART buffer\r\n");
            return -1;
        }
    }
    ret = Ql_UART_Register(GPS_UART_PORT, (CallBack_UART_Notify )gps_uart_cb, NULL);
    if (ret < QL_RET_OK)
    {
        LOGData(TAG_GPS,"Fail to register GPS UART port[%d], ret=%d\r\n", GPS_UART_PORT, ret);
        return -1;
    }
    ret = Ql_UART_Open(GPS_UART_PORT, 9600, FC_NONE);  
    if (ret < QL_RET_OK)
    {
        LOGData(TAG_GPS,"Fail to open GPS UART port[%d], ret=%d\r\n", GPS_UART_PORT, ret);
        return -1;
    }
    LOGData(TAG_GPS,"UART port opened successfully\r\n"); 
    gps_switch_baudrate();

    // GNSS has just rebooted as part of baud switching; schedule aiding after boot.
    gps_agnss_schedule(3000);

    GPSTimeout = GPS_RCV_TIMEOUT;
    GPS.State=1;
    return 0;
}

int gps_nmea_extract(const char* fullData, const char* packetType, int index, char* buffer, size_t bufferSize) {
    if (fullData == NULL || packetType == NULL || buffer == NULL || bufferSize == 0 || index < 0) {
        return 0;
    }

    const char* start = fullData;
    const char* end = Ql_strchr(start, '\n');
    int count = 0;

    size_t typeLen = Ql_strlen(packetType);
    if (typeLen < 3) return 0; // Must at least be 3 chars like "GGA"

    // Clear output buffer first
    Ql_memset(buffer, 0, bufferSize);

    // Traverse through the NMEA packets
    while (end != NULL) {
        // Skip '$' - verify we have at least minimal packet length
        size_t line_len = end - start;
        if (line_len >= 6 && *start == '$') {
            const char* sentenceType = start + 1;

            // Match only the LAST 3 chars of packetType (e.g., "GGA")
            if (Ql_strncmp(sentenceType + 2, packetType + (typeLen - 3), 3) == 0) {
                if (count == index) {
                    size_t packetLength = end - start;

                    if (packetLength < bufferSize - 1) {
                        Ql_strncpy(buffer, start, packetLength);
                        buffer[packetLength] = '\0';
                        return 1; // Found
                    }
                    return 0; // Buffer too small
                }
                count++;
            }
        }

        start = end + 1;
        end = Ql_strchr(start, '\n');
    }

    return 0; // Not found
}

// Helper: find the first "$<talker>GSV..." line in the NMEA buffer
// Returns 1 and fills `outPacket` (null-terminated) on success, 0 on failure.
static int find_first_gsv_for_talker(const char *nmea, const char *talker, char *outPacket, size_t outSize)
{
    if (!nmea || !talker || !outPacket || outSize == 0) return 0;

    // Clear output buffer
    Ql_memset(outPacket, 0, outSize);

    const char *start = nmea;
    const char *end = Ql_strchr(start, '\n');

    while (end != NULL) {
        // line starts at 'start', ends at 'end' (not including '\n')
        // check format: $ <t0><t1> G S V
        // Ensure we have at least 6 characters to check
        size_t line_len = end - start;
        if (line_len >= 6 && 
            *start == '$' &&
            start[1] == talker[0] &&
            start[2] == talker[1] &&
            start[3] == 'G' &&
            start[4] == 'S' &&
            start[5] == 'V')
        {
            size_t len = (size_t)(end - start);
            if (len >= outSize - 1) return 0; // buffer too small (need room for null)
            Ql_strncpy(outPacket, start, len);
            outPacket[len] = '\0';
            return 1;
        }

        start = end + 1;
        end = Ql_strchr(start, '\n');
    }

    return 0; // not found
}


void gps_update_sat_counts(const char *nmea, GPS_Typedef *gps)
{
    char packet[300];
    int total = 0;
    bool found_any_gsv = false; // Track if we found ANY GSV sentence

    struct {
        const char *talker;
        int *field;
    } table[] = {
        {"GP", &gps->SatGPS},
        {"GL", &gps->SatGLONASS},
        {"GA", &gps->SatGalileo},
        {"GB", &gps->SatBeiDou},
        {"GQ", &gps->SatQZSS},
        {"GI", &gps->SatNavIC},
        {"GN", &gps->SatMixed}, // combined multi-constellation sentences
    };

    int numTalkers = sizeof(table) / sizeof(table[0]);

    // reset per-constellation counters
    for (int i = 0; i < numTalkers; ++i) {
        *(table[i].field) = 0;
    }
    gps->SatTotal = 0;

    for (int i = 0; i < numTalkers; ++i)
    {
        if (find_first_gsv_for_talker(nmea, table[i].talker, packet, sizeof(packet)))
        {
            GSVTypedef gsvData;
            if (NMEA_Parse_GSV(&gsvData, packet))
            {
                // use the correct field name from your GSVTypedef
                *(table[i].field) = gsvData.total_sats;
                total += gsvData.total_sats;
                found_any_gsv = true; // We found at least one GSV sentence
                //LOGData(TAG_GPS, "%s: %d sats\r\n", table[i].talker, gsvData.total_sats);
            }
        }
    }

    gps->SatTotal = total;
    
    // Return whether we found any GSV data
    if(found_any_gsv) {
        GPSTimeout = GPS_RCV_TIMEOUT; // Reset timeout when GSV data is received
    }
}


void gps_gga_update(GGATypedef *GGA)
{
    int fix;
    fix = GGA->Fix_Quality;
    if(fix > 1)
        fix = 1;
    if(fix != GPS.GPSFix)
    {
#if FEATURE_GPS_LAST_FIX_FALLBACK
        if (GPS.GPSFix == 1 && fix == 0)
        {
            // Transition from Fixed to Unfixed -> commit last fix to flash immediately
            GPS_SaveLastFixedToFlash();
        }
#endif
        GPS.GPSFix=fix;
        LOGData(TAG_GPS,"!! Fix State Changed to %d", GGA->Fix_Quality);
        // LED Manager will automatically update based on GPS fix state
    }
    
    // Debug: Log the satellite count from GGA
    if(GGA->Satellites_Tracked != GPS.NoOfSatalite) {
        LOGData(TAG_GPS,"GGA Satellites_Tracked changed: %d -> %d\r\n", GPS.NoOfSatalite, GGA->Satellites_Tracked);
    }
    
    GPS.NoOfSatalite=GGA->Satellites_Tracked;

    if (GPS.GPSFix)
    {
        GPS.Latitude = nmea_tocoord(&GGA->Latitude);
        GPS.Longitude = nmea_tocoord(&GGA->Longitude);
        GPS.Altitude = nmea_tofloat(&GGA->Altitude);
        GPS.LatDir = (GGA->LatDir == 'S' || GGA->LatDir == 's') ? 'S' : 'N';
        GPS.LngDir = (GGA->LngDir == 'W' || GGA->LngDir == 'w') ? 'W' : 'E';
        Ql_sprintf(sLatitude,"%3.6f",GPS.Latitude);   
        Ql_sprintf(sLongitude,"%3.6f",GPS.Longitude);
        Ql_sprintf(sAltitude,"%4.2f",GPS.Altitude);
        Ql_sprintf(sSpeed,"%3.2f",GPS.Speed);
        Ql_sprintf(sHeading,"%3.1f",GPS.Heading);

#if FEATURE_GPS_LAST_FIX_FALLBACK
        if (GPS.Latitude != 0.0 && GPS.Longitude != 0.0)
        {
            static uint32_t s_lastGpsFlashSaveSec = 0;
            uint32_t curSec = (uint32_t)(Ql_GetMsSincePwrOn() / 1000ULL);

            LastFixedGPS.Valid = 1;
            LastFixedGPS.Latitude = GPS.Latitude;
            LastFixedGPS.Longitude = GPS.Longitude;
            LastFixedGPS.Altitude = GPS.Altitude;
            LastFixedGPS.LatDir = GPS.LatDir;
            LastFixedGPS.LngDir = GPS.LngDir;
            LastFixedGPS.Speed = GPS.Speed;
            LastFixedGPS.Heading = GPS.Heading;
            LastFixedGPS.PDOP = GPS.PDOP;
            LastFixedGPS.HDOP = GPS.HDOP;
            LastFixedGPS.NoOfSatalite = GPS.NoOfSatalite;
            Ql_strncpy(LastFixedGPS.sLatitude, sLatitude, sizeof(LastFixedGPS.sLatitude) - 1);
            LastFixedGPS.sLatitude[sizeof(LastFixedGPS.sLatitude) - 1] = '\0';
            Ql_strncpy(LastFixedGPS.sLongitude, sLongitude, sizeof(LastFixedGPS.sLongitude) - 1);
            LastFixedGPS.sLongitude[sizeof(LastFixedGPS.sLongitude) - 1] = '\0';
            Ql_strncpy(LastFixedGPS.sAltitude, sAltitude, sizeof(LastFixedGPS.sAltitude) - 1);
            LastFixedGPS.sAltitude[sizeof(LastFixedGPS.sAltitude) - 1] = '\0';
            Ql_strncpy(LastFixedGPS.sSpeed, sSpeed, sizeof(LastFixedGPS.sSpeed) - 1);
            LastFixedGPS.sSpeed[sizeof(LastFixedGPS.sSpeed) - 1] = '\0';
            Ql_strncpy(LastFixedGPS.sHeading, sHeading, sizeof(LastFixedGPS.sHeading) - 1);
            LastFixedGPS.sHeading[sizeof(LastFixedGPS.sHeading) - 1] = '\0';
            Ql_strncpy(LastFixedGPS.sPDOP, sPDOP, sizeof(LastFixedGPS.sPDOP) - 1);
            LastFixedGPS.sPDOP[sizeof(LastFixedGPS.sPDOP) - 1] = '\0';
            Ql_strncpy(LastFixedGPS.sHDOP, sHDOP, sizeof(LastFixedGPS.sHDOP) - 1);
            LastFixedGPS.sHDOP[sizeof(LastFixedGPS.sHDOP) - 1] = '\0';

            // Periodically persist fix to flash (every 30 seconds; SaveToFlash skips if unchanged)
            if (s_lastGpsFlashSaveSec == 0 || (curSec - s_lastGpsFlashSaveSec) >= 30)
            {
                s_lastGpsFlashSaveSec = curSec;
                GPS_SaveLastFixedToFlash();
            }
        }
#endif
    }
    else
    {
#if FEATURE_GPS_LAST_FIX_FALLBACK
        if (LastFixedGPS.Valid)
        {
            GPS.Latitude = LastFixedGPS.Latitude;
            GPS.Longitude = LastFixedGPS.Longitude;
            GPS.Altitude = LastFixedGPS.Altitude;
            GPS.LatDir = LastFixedGPS.LatDir;
            GPS.LngDir = LastFixedGPS.LngDir;
            GPS.Speed = LastFixedGPS.Speed;
            GPS.Heading = LastFixedGPS.Heading;
            GPS.PDOP = LastFixedGPS.PDOP;
            GPS.HDOP = LastFixedGPS.HDOP;
            Ql_strcpy(sLatitude, LastFixedGPS.sLatitude);
            Ql_strcpy(sLongitude, LastFixedGPS.sLongitude);
            Ql_strcpy(sAltitude, LastFixedGPS.sAltitude);
            Ql_strcpy(sSpeed, LastFixedGPS.sSpeed);
            Ql_strcpy(sHeading, LastFixedGPS.sHeading);
            Ql_strcpy(sPDOP, LastFixedGPS.sPDOP);
            Ql_strcpy(sHDOP, LastFixedGPS.sHDOP);
        }
        else
        {
            GPS.Latitude = 0.0;
            GPS.Longitude = 0.0;
            GPS.Altitude = 0.0;
            GPS.LatDir = 'N';
            GPS.LngDir = 'E';
            Ql_strcpy(sLatitude, "0.000000");
            Ql_strcpy(sLongitude, "0.000000");
            Ql_strcpy(sAltitude, "0.00");
            Ql_strcpy(sSpeed, "0.00");
            Ql_strcpy(sHeading, "0.0");
        }
#else
        GPS.Latitude = nmea_tocoord(&GGA->Latitude);
        GPS.Longitude = nmea_tocoord(&GGA->Longitude);
        GPS.Altitude = nmea_tofloat(&GGA->Altitude);
        GPS.LatDir='N';
        GPS.LngDir='E';
        Ql_sprintf(sLatitude,"%3.6f",GPS.Latitude);   
        Ql_sprintf(sLongitude,"%3.6f",GPS.Longitude);
        Ql_sprintf(sAltitude,"%4.2f",GPS.Altitude);
        Ql_sprintf(sSpeed,"%3.2f",GPS.Speed);
        Ql_sprintf(sHeading,"%3.1f",GPS.Heading);
#endif
    }
}

void gps_vtg_update(VTGTypedef *VTG)
{
    if (GPS.GPSFix)
    {
        GPS.Speed = nmea_tofloat(&VTG->speed_kph);
        GPS.Heading=nmea_tofloat(&VTG->true_track_degrees);
        Ql_sprintf(sSpeed,"%3.2f",GPS.Speed);
        Ql_sprintf(sHeading,"%3.1f",GPS.Heading);
#if FEATURE_GPS_LAST_FIX_FALLBACK
        if (LastFixedGPS.Valid)
        {
            LastFixedGPS.Speed = GPS.Speed;
            LastFixedGPS.Heading = GPS.Heading;
            Ql_strncpy(LastFixedGPS.sSpeed, sSpeed, sizeof(LastFixedGPS.sSpeed) - 1);
            LastFixedGPS.sSpeed[sizeof(LastFixedGPS.sSpeed) - 1] = '\0';
            Ql_strncpy(LastFixedGPS.sHeading, sHeading, sizeof(LastFixedGPS.sHeading) - 1);
            LastFixedGPS.sHeading[sizeof(LastFixedGPS.sHeading) - 1] = '\0';
        }
#endif
    }
    else
    {
#if FEATURE_GPS_LAST_FIX_FALLBACK
        if (LastFixedGPS.Valid)
        {
            GPS.Speed = LastFixedGPS.Speed;
            GPS.Heading = LastFixedGPS.Heading;
            Ql_strcpy(sSpeed, LastFixedGPS.sSpeed);
            Ql_strcpy(sHeading, LastFixedGPS.sHeading);
        }
#endif
    }
}

void gps_rmc_update(RMCTypedef *RMC)
{
    GPS.Date.Day=RMC->Date.Day;
    GPS.Date.Month=RMC->Date.Month;
    GPS.Date.Year=RMC->Date.Year;
    GPS.Time.Hours=RMC->time.Hours;
    GPS.Time.Minutes=RMC->time.Minutes;
    GPS.Time.Seconds=RMC->time.Seconds;
    GPS.Time.MicroSeconds=RMC->time.MicroSeconds;
    GPSDateTime.Year=RMC->Date.Year;
    GPSDateTime.Month=RMC->Date.Month; 
    GPSDateTime.DaysOfWeek=RMC->Date.Day;
    GPSDateTime.Hour=RMC->time.Hours;
    GPSDateTime.Min=RMC->time.Minutes;
    GPSDateTime.Sec=RMC->time.Seconds;
}

void gps_gsa_update(GSATypedef *GSA)
{
    if (GPS.GPSFix)
    {
        // Only update PDOP/HDOP if valid (non-zero scale means data was present)
        if (GSA->PDOP.Scale != 0) {
            double pdop_val = nmea_tofloat(&GSA->PDOP);
            // Check if value is invalid (99.9 or 99.99 indicates no valid data)
            if (pdop_val >= 99.0) {
                GPS.PDOP = 0.0;
                Ql_sprintf(sPDOP, "0.00");
            } else {
                GPS.PDOP = pdop_val;
                Ql_sprintf(sPDOP,"%3.2f", GPS.PDOP);
            }
        } else {
            GPS.PDOP = 0.0;
            Ql_sprintf(sPDOP, "0.00");
        }
        
        if (GSA->HDOP.Scale != 0) {
            double hdop_val = nmea_tofloat(&GSA->HDOP);
            // Check if value is invalid (99.9 or 99.99 indicates no valid data)
            if (hdop_val >= 99.0) {
                GPS.HDOP = 0.0;
                Ql_sprintf(sHDOP, "0.00");
            } else {
                GPS.HDOP = hdop_val;
                Ql_sprintf(sHDOP,"%3.2f", GPS.HDOP);
            }
        } else {
            GPS.HDOP = 0.0;
            Ql_sprintf(sHDOP, "0.00");
        }

#if FEATURE_GPS_LAST_FIX_FALLBACK
        if (LastFixedGPS.Valid)
        {
            if (GPS.PDOP > 0.0) {
                LastFixedGPS.PDOP = GPS.PDOP;
                Ql_strncpy(LastFixedGPS.sPDOP, sPDOP, sizeof(LastFixedGPS.sPDOP) - 1);
                LastFixedGPS.sPDOP[sizeof(LastFixedGPS.sPDOP) - 1] = '\0';
            }
            if (GPS.HDOP > 0.0) {
                LastFixedGPS.HDOP = GPS.HDOP;
                Ql_strncpy(LastFixedGPS.sHDOP, sHDOP, sizeof(LastFixedGPS.sHDOP) - 1);
                LastFixedGPS.sHDOP[sizeof(LastFixedGPS.sHDOP) - 1] = '\0';
            }
        }
#endif
    }
    else
    {
#if FEATURE_GPS_LAST_FIX_FALLBACK
        if (LastFixedGPS.Valid)
        {
            GPS.PDOP = LastFixedGPS.PDOP;
            GPS.HDOP = LastFixedGPS.HDOP;
            Ql_strcpy(sPDOP, LastFixedGPS.sPDOP);
            Ql_strcpy(sHDOP, LastFixedGPS.sHDOP);
        }
        else
        {
            GPS.PDOP = 0.0;
            GPS.HDOP = 0.0;
            Ql_strcpy(sPDOP, "0.00");
            Ql_strcpy(sHDOP, "0.00");
        }
#else
        GPS.PDOP = 0.0;
        GPS.HDOP = 0.0;
        Ql_sprintf(sPDOP, "0.00");
        Ql_sprintf(sHDOP, "0.00");
#endif
    }
}

// Forward NMEA data to RS232
void SendNMEAToRS232(void)
{
    #ifndef ENABLE_RS232_PRINT
        return;
    #endif
    
    if (!nmea_data_ready || Ql_strlen(last_nmea_data) == 0) {
        LOGData(TAG_GPS, "No NMEA data to send: ready=%d, len=%d\r\n", nmea_data_ready, Ql_strlen(last_nmea_data));
        return;
    }
    
    // Send raw NMEA data directly without any formatting
    // NMEA sentences already have \r\n endings
    SendRS232String(last_nmea_data);
    LOGData(TAG_GPS, "NMEA data forwarded to RS232: %d bytes\r\n", Ql_strlen(last_nmea_data));
}

#define GPS_SENTENCE_MAX_LENGTH 150
void gps_data_process(char* nmea)
{
    char packet[GPS_SENTENCE_MAX_LENGTH]={0};
    bool valid_data_received = false;  // Add this flag
    
    // Validate input
    if (!nmea) {
        LOGData(TAG_GPS, "NULL NMEA data pointer!\r\n");
        return;
    }
    
    // // Store ALL NMEA sentences for RS232 forwarding
    // if (nmea && Ql_strlen(nmea) > 0) {
    //     Ql_strncpy(last_nmea_data, nmea, GPS_UART_BUFFER_SIZE - 1);
    //     last_nmea_data[GPS_UART_BUFFER_SIZE - 1] = '\0';
    //     nmea_data_ready = 1;
    //     LOGData(TAG_GPS, "All NMEA sentences captured for RS232: %d bytes\r\n", Ql_strlen(last_nmea_data));
    // }
    
   // LOGData(TAG_GPS,"***Decoding data...\r\n");
    if(gps_nmea_extract(nmea,"GGA",0,packet,GPS_SENTENCE_MAX_LENGTH))
    {
        NMEA_Parse_GGA(&GGAData,packet);
        gps_gga_update(&GGAData);
        valid_data_received = true;  
    }
    else
        LOGVerbose(TAG_GPS,"NO GGA Packet!");

    if(gps_nmea_extract(nmea,"VTG",0,packet,GPS_SENTENCE_MAX_LENGTH))
    {
        if(NMEA_Parse_VTG(&VTGData,packet))
        {
            gps_vtg_update(&VTGData);
            valid_data_received = true;
        }
    }

    if(gps_nmea_extract(nmea,"RMC",0,packet,GPS_SENTENCE_MAX_LENGTH))
    {
        /* Only accept a VALID ('A') fix. A void ('V') RMC on cold-start carries
         * the 1980 GPS-epoch date, which would leak through as a real time and
         * stamp packets with Date:000180 (1980). */
        if(NMEA_Parse_RMC(&RMCData,packet) && RMCData.Valid)
            gps_rmc_update(&RMCData);
    }

    if(gps_nmea_extract(nmea,"GSA",0,packet,GPS_SENTENCE_MAX_LENGTH))
    {
        if(NMEA_Parse_GSA(&GSAData,packet)) {
            gps_gsa_update(&GSAData);
            valid_data_received = true;
        } 
    }

    // Refresh timeout if we received any valid data
    if(valid_data_received) {
        GPSTimeout = GPS_RCV_TIMEOUT;  // Reset timeout when valid data is processed
    }
    gps_update_sat_counts(nmea,&GPS);


    if(GPS.GPSFix){
        LOGData(TAG_GPS,"FIX Success, Lat: %s %c, Long: %s %c, Speed: %d, Time: %02d:%02d:%02d %02d/%02d/%02d",
                sLatitude, GPS.LatDir, sLongitude, GPS.LngDir, (int)GPS.Speed,
                GPS.Time.Hours, GPS.Time.Minutes, GPS.Time.Seconds, GPS.Date.Day, GPS.Date.Month, GPS.Date.Year);
    }
    else
        LOGVerbose(TAG_GPS,"GPS Not Fixed\r\n");

}

extern uint8_t IsOverSpeed;
// Over-speed alert processing
void gps_overspeed_check(void)
{   
    if (!GPS.GPSFix) {
#ifdef PROTO_CDAC
        // Clear stale overspeed state on fix loss so alerts don't stay latched.
        if (IsOverSpeed) {
            ovsCount = 0;
            VAlert[OVER_SPEED_ALERT].Enable = 0; RemoveAlert(OVER_SPEED_ALERT);
            VAlert[GFIN_OS_ALERT].Enable = 0;    RemoveAlert(GFIN_OS_ALERT);
            VAlert[GFOUT_OS_ALERT].Enable = 0;   RemoveAlert(GFOUT_OS_ALERT);
            IsOverSpeed = 0;
        }
#endif
        return;  // Only check overspeed when GPS has fix
    }

    // --- Over-speed Alert Processing ---
    if (GPS.Speed > VTSData.VehicleData.OverSpeed)
    {
        #ifdef PROTO_CDAC
        if (!IsOverSpeed)
        {
        #endif
            if (++ovsCount > 5)
            {
                #ifdef PROTO_CDAC
                if (CheckIfInside())
                {
                    // Inside a geofence: overspeed-in-fence (CD1 id 20)
                    // Attach the geofence ID as the ACK payload (per tested reference)
                    int geoId = GetInsideGeofenceID();
                    VAlert[GFIN_OS_ALERT].Enable = 1;
                    VAlert[GFIN_OS_ALERT].WithACK = 1;
                    Ql_memset(VAlert[GFIN_OS_ALERT].ACK, 0x00, sizeof(VAlert[GFIN_OS_ALERT].ACK));
                    Ql_sprintf(VAlert[GFIN_OS_ALERT].ACK, "%05d", geoId);
                    AddAlert(GFIN_OS_ALERT);
                    SMSAlert(20);
                    IsOverSpeed = 1;
                }
                else
                {
                    // Outside all fences: overspeed-out-of-fence (CD1 id 21)
                    if ((!VAlert[GFIN_OS_ALERT].Enable) && (!VAlert[GFOUT_OS_ALERT].Enable))
                    {
                        VAlert[GFOUT_OS_ALERT].Enable = 1;
                        AddAlert(GFOUT_OS_ALERT);
                        SMSAlert(21);
                        IsOverSpeed = 1;
                    }
                }
                #else
                    if ((!VAlert[GFIN_OS_ALERT].Enable) && (!VAlert[GFOUT_OS_ALERT].Enable))
                    {
                        VAlert[OVER_SPEED_ALERT].Enable = 1;
                        AddAlert(OVER_SPEED_ALERT);
                    }
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
            VAlert[GFIN_OS_ALERT].Enable = 0;
            RemoveAlert(GFIN_OS_ALERT);
            #ifdef PROTO_CDAC
            VAlert[GFOUT_OS_ALERT].Enable = 0;
            RemoveAlert(GFOUT_OS_ALERT);
            IsOverSpeed = 0;
        }
        #endif
    }
}

#define EARTH_RADIUS_METERS 6371000.0
#define PI 3.14159265358979323846

static double toRadians(double degrees) {
    return degrees * PI / 180.0;
}

static double cosTaylor(double x) {
    double term = 1.0;
    double sum = 1.0;
    double x2 = x * x;
    int n = 1;
    double factorial = 1.0;
    const int MAX_ITERATIONS = 20;

    while (n <= MAX_ITERATIONS) {
        factorial *= (2 * n - 1) * (2 * n);
        term *= -x2 / factorial;
        if (term > -1e-15 && term < 1e-15) break;
        sum += term;
        n++;
    }
    return sum;
}

extern double fGPSLat, fGPSLong, fGPSAlt, fGPSpdop, fGPShdop, fGPSSats, fGPSSpeed, fGPSHeading, fGPSForce;

bool GPS_IsSimulationActive(void)
{
    return (fGPSLat != 0 && fGPSLong != 0);
}

uint8_t GPS_GetState(void)
{
    return GPS.State;
}

void ApplyFGPS(void)
{
    GPS.GPSFix = 1;

    // Apply random fluctuation to latitude and longitude (up to 25 meters)
    double latFluctuation = ((Ql_rand() % 25) + 1) / EARTH_RADIUS_METERS * (180.0 / PI);
    double longFluctuation = ((Ql_rand() % 25) + 1) / (EARTH_RADIUS_METERS * cosTaylor(toRadians(fGPSLat))) * (180.0 / PI);

    GPS.Latitude = fGPSLat + ((Ql_rand() % 2 == 0) ? latFluctuation : -latFluctuation);
    GPS.Longitude = fGPSLong + ((Ql_rand() % 2 == 0) ? longFluctuation : -longFluctuation);

    // Apply random fluctuation to altitude, HDOP, PDOP, and number of satellites
    GPS.Altitude = fGPSAlt + ((Ql_rand() % 2 == 0) ? (Ql_rand() % 5 + 1) : -(Ql_rand() % 5 + 1));
    GPS.HDOP = fGPShdop + ((Ql_rand() % 2 == 0) ? ((Ql_rand() % 10 + 1) / 10.0) : -((Ql_rand() % 10 + 1) / 10.0));
    GPS.PDOP = fGPSpdop + ((Ql_rand() % 2 == 0) ? ((Ql_rand() % 10 + 1) / 10.0) : -((Ql_rand() % 10 + 1) / 10.0));
    GPS.NoOfSatalite = fGPSSats + ((Ql_rand() % 2 == 0) ? (Ql_rand() % 2) : -(Ql_rand() % 2));

    GPS.LatDir = 'N';
    GPS.LngDir = 'E';
    GPS.Heading = 0;

    // Fixed stationary speed fluctuation issue (speed stays strictly as input)
    GPS.Speed = fGPSSpeed;
    if(GPS.Speed < 0)
        GPS.Speed = 0;

    Ql_sprintf(sLatitude, "%03.7f", GPS.Latitude);
    Ql_sprintf(sLongitude, "%03.7f", GPS.Longitude);
    #ifdef BSNL_PROTO
    Ql_sprintf(sAltitude, "%05.2f", GPS.Altitude);
    #else
    Ql_sprintf(sAltitude, "%3.2f", GPS.Altitude);
    #endif
    Ql_sprintf(sPDOP, "%2.2f", GPS.PDOP);
    Ql_sprintf(sHDOP, "%2.2f", GPS.HDOP);
    #ifdef BSNL_PROTO
    Ql_sprintf(sSpeed,"%04.1f", GPS.Speed);
    #else
    Ql_sprintf(sSpeed,"%02.1f", GPS.Speed);
    #endif
    Ql_sprintf(sHeading,"%03.1f", GPS.Heading);
}

void gps_reset_routine(void)
{
    static uint8_t s_fotaBypassLogged = 0;
    if (GPS_IsSimulationActive())
    {
        return;
    }
    if (IsMotaProcessing || IsFotaProcessing)
    {
        if (!s_fotaBypassLogged)
        {
            LOGData(TAG_GPS, "GPS Reset Routine bypassed because FOTA/MOTA is in progress");
            s_fotaBypassLogged = 1;
        }
        return;
    }
    s_fotaBypassLogged = 0;
#if ENABLE_GPS_RESET_RECOVERY
    static uint32_t last_reset_ms = 0;
    uint32_t now = Ql_GetMsSincePwrOn();

    if (now - last_reset_ms < 60000)
    {
        return;
    }

    last_reset_ms = now;

    if (GPS.State == 2)
    {
        LOGData(TAG_GPS, "GPS State is 2 (No NMEA stream). Triggering module reset for recovery.");
        ThreadSleep(1000);
        if (GPSTimeout == 0)
        {
            LOGData(TAG_GPS, "GPS remains in fault state — resetting module.");
            ThreadSleep(1000);
            Ql_Reset(0);
        }
    }
    else if (GPS.State == 0)
    {
        LOGData(TAG_GPS, "GPS State is 0 (Searching for satellite fix...).");
    }
#else
    static uint32_t last_disabled_log_ms = 0;
    uint32_t now = Ql_GetMsSincePwrOn();
    if (now - last_disabled_log_ms >= 60000)
    {
        last_disabled_log_ms = now;
        LOGData(TAG_GPS,"GPS Reset Routine Triggered (disabled by config)\r\n");
    }
#endif
}

void gps_thread_entry(s32 taskId)
{
    static uint8_t s_realGpsFix = 0;
    gps_thread_init(taskId);
    GPS_RESET_INIT;
    GPS_RESET_OFF;
    ThreadSleep(500);
    gps_init();
    // LED Manager will automatically handle GPS LED state
    while(1)
    {
        while(SleepConfig.IsEnabled)
        {
            LOGData(TAG_GPS,"GPS Thread Sleeping...\r\n");
            ThreadSleep(3000);
        }

        // If a re-init was requested (e.g., after Flash EPO injection), do it now.
        gps_handle_reinit_request();

        // Try AGNSS aiding opportunistically (non-blocking and command-safe).
        gps_agnss_poll();

        if (GPSTimeout == 0 && GPS.State == 1)
        {
            GPS.State = 0;
            // LED Manager will automatically turn off GPS LED
        }
        else if (GPS.State == 0 && GPSTimeout > 0)
        {
            GPS.State = 1;
            // LED Manager will automatically show GPS searching
        }


        if (GPS.State == 0)
            gps_reset_routine();
        else if (GPSTimeout > 0)
            GPSTimeout--;

        if (gps_data_available)
        {
            gps_data_available = 0;
            if (!(fGPSLat != 0 && fGPSLong != 0 && fGPSForce)) // CGPS bypasses parsing
            {
                gps_data_process((char*)gps_uart_buffer);
                s_realGpsFix = GPS.GPSFix; // Save real fix status
                if (fGPSLat != 0 && fGPSLong != 0 && !fGPSForce && !s_realGpsFix)
                {
                    ApplyFGPS(); // FGPS fallback applied
                }
            }
            // Check overspeed after processing GPS data
            gps_overspeed_check();
        }

        static uint32_t last_sim_ms = 0;
        uint32_t now_ms = Ql_GetMsSincePwrOn();
        if (now_ms - last_sim_ms >= 1000)
        {
            last_sim_ms = now_ms;
            if (fGPSLat != 0 && fGPSLong != 0)
            {
                if (fGPSForce)
                {
                    ApplyFGPS(); // Continuous forced simulation (CGPS)
                }
                else if (!s_realGpsFix || GPSTimeout == 0)
                {
                    ApplyFGPS(); // Fallback simulation (FGPS)
                }
            }
        }
      
        ThreadSleep(30);
    }

}

void gps_thread_init(u32 taskId)
{
    s32 ret;
    OSThread GPS_Thread = {0};
    GPS_Thread.taskId = taskId;
    Ql_strcpy(GPS_Thread.taskName, "GPS Thread");
    GPS_Thread.taskEnable = 1;
    GPS_Thread.taskState = TASK_STATE_NORMAL;
    GPS_Thread.taskPriority = 1;
    ret = InitializeThread(&GPS_Thread);
    if (ret != 1)
    {
        LOGData(TAG_GPS, "Failed to initialize GPS thread");
        return;
    }
    LOGData(TAG_GPS, "GPS thread initialized successfully");
}




