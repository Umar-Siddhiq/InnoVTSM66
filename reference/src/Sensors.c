#include "Sensors.h"
#include "GPRS.h"
#include "Utilities.h"
#include "Hardware.h"
#include "TCP.h"
#include "VTS.h"
#include "MCU.h"
#include "SOS.h"
#include "nwy_file.h"
#include <string.h>
#include <stdio.h>


SensorDataTypedef SensorData[MAX_SENSORS];
SensorsTypedef SensorsConfig;




void InitSensors(void)
{
    // Initialize SensorsConfig with default values
    SensorsConfig.magic = SENSOR_STUCT_MAGIC;
    SensorsConfig.isEnabled = 1; // Enabled by default
    SensorsConfig.intervalConfig.dayIGNITIONInterval = 10; // Default intervals
    SensorsConfig.intervalConfig.nightIGNITIONInterval = 20;
    SensorsConfig.intervalConfig.dayOFFInterval = 30;
    SensorsConfig.intervalConfig.nightOFFInterval = 60;
    SensorsConfig.sensorTimeout = 120; // Default 120 seconds (2 minutes) before sensor considered disconnected

    // Initialize each sensor data
    for(int i = 0; i < MAX_SENSORS; i++)
    {
        SensorData[i].SensorType = SENSOR_TYPE_MAX; // Invalid type by default
        SensorData[i].IsActive = 0;
        SensorData[i].DataType = SENSOR_DATATYPE_MAX; // Invalid data type
        SensorData[i].IntervalType = SENSOR_INTERVALTYPE_MAX; // Invalid interval type
        SensorData[i].IsPending = 0;
        memset(SensorData[i].SensorData, 0, sizeof(SensorData[i].SensorData));
    }
}

void HandleSensorData(SensorTypeEnum sensorIndex, const char* data, uint16_t dataLen, uint8_t dataType, SensorIntervalTypeEnum intervalType, uint16_t timeout)
{
   // look if same sensor type is configured and active
   // If yes, update its data and mark as pending
   // if not, add new sensor data if space available
    for(int i = 0; i < MAX_SENSORS; i++)
    {
        if(SensorData[i].SensorType == sensorIndex && SensorData[i].IsActive)
        {
            // Update existing sensor data
            memcpy(SensorData[i].SensorData, data, dataLen < sizeof(SensorData[i].SensorData) ? dataLen : sizeof(SensorData[i].SensorData) - 1);
            SensorData[i].DataLen = dataLen < sizeof(SensorData[i].SensorData) ? dataLen : sizeof(SensorData[i].SensorData) - 1;
            SensorData[i].IsPending = 1;
            SensorData[i].DataType = dataType;
            SensorData[i].IntervalType = intervalType;
            SensorData[i].Timeout = timeout;
            nwy_dbg_log("HandleSensorData: Updated sensor %d, IntType=%d, IsPending=%d", sensorIndex, intervalType, SensorData[i].IsPending);
            return;
        }
    }

    // Add new sensor data if space available
    for(int i = 0; i < MAX_SENSORS; i++)
    {
        if(SensorData[i].SensorType == SENSOR_TYPE_MAX) // Empty slot
        {
            SensorData[i].SensorType = sensorIndex;
            SensorData[i].IsActive = 1;
            SensorData[i].DataType = SENSOR_DATATYPE_ASCII; // Default data type
            SensorData[i].IntervalType = SENSOR_INTERVALTYPE_REGULAR; // Default interval type
            memcpy(SensorData[i].SensorData, data, dataLen < sizeof(SensorData[i].SensorData) ? dataLen : sizeof(SensorData[i].SensorData) - 1);
            SensorData[i].DataLen = dataLen < sizeof(SensorData[i].SensorData) ? dataLen : sizeof(SensorData[i].SensorData) - 1;
            SensorData[i].IsPending = 1;
            SensorData[i].Timeout = timeout;
            SensorData[i].DataType = dataType;
            SensorData[i].IntervalType = intervalType;
            nwy_dbg_log("HandleSensorData: Added new sensor %d at slot %d, IntType=%d, IsPending=%d", sensorIndex, i, intervalType, SensorData[i].IsPending);
            return;
        }
    }
    nwy_dbg_log("HandleSensorData: No slot available for sensor %d", sensorIndex);
}

void ClearSensorData(SensorTypeEnum sensorIndex)
{
    for(int i = 0; i < MAX_SENSORS; i++)
    {
        if(SensorData[i].SensorType == sensorIndex)
        {
            SensorData[i].SensorType = SENSOR_TYPE_MAX; // Mark as empty
            SensorData[i].IsActive = 0;
            SensorData[i].DataType = SENSOR_DATATYPE_MAX;
            SensorData[i].IntervalType = SENSOR_INTERVALTYPE_MAX;
            SensorData[i].IsPending = 0;
            memset(SensorData[i].SensorData, 0, sizeof(SensorData[i].SensorData));
            SensorData[i].Timeout = 0;
            SensorData[i].DataLen = 0;
            return;
        }
    }
}

// Process sensor timeouts - called every 1 second from Systic
// Only applies to REGULAR interval sensors (INTERRUPT sensors are cleared after sending)
// When timeout is reached, sensor is considered disconnected and cleared
void ProcessSensorTimeouts(uint16_t timeoutThreshold)
{
    for(int i = 0; i < MAX_SENSORS; i++)
    {
        // Only process active REGULAR sensors
        if(SensorData[i].IsActive && 
           SensorData[i].IntervalType == SENSOR_INTERVALTYPE_REGULAR)
        {
            SensorData[i].Timeout++;
            if(SensorData[i].Timeout >= timeoutThreshold)
            {
                // Timeout reached, sensor considered disconnected - clear sensor data
                nwy_dbg_log("Sensor type %d timeout after %d seconds, clearing", 
                           SensorData[i].SensorType, timeoutThreshold);
                ClearSensorData(SensorData[i].SensorType);
            }
        }
    }
}

// Calculate simple checksum for sensor packets (XOR of all bytes between $ and *)
static uint8_t CalculateSensorChecksum(const char* data, int length)
{
    uint8_t checksum = 0;
    for(int i = 0; i < length; i++)
    {
        checksum ^= (uint8_t)data[i];
    }
    return checksum;
}

// Check if any interrupt sensor data is pending (for fast polling)
uint8_t HasPendingInterrupt(void)
{
    for(int i = 0; i < MAX_SENSORS; i++)
    {
        if(SensorData[i].IsActive && 
           SensorData[i].IsPending && 
           SensorData[i].IntervalType == SENSOR_INTERVALTYPE_INTERRUPT)
        {
            return 1;
        }
    }
    return 0;
}

// Check if any regular sensor data is pending
uint8_t HasPendingRegularData(void)
{
    if(!SensorsConfig.isEnabled)
    {
        nwy_dbg_log("HasPendingRegularData: Sensors disabled");
        return 0;
    }
        
    for(int i = 0; i < MAX_SENSORS; i++)
    {
        if(SensorData[i].IsActive && 
           SensorData[i].IsPending && 
           SensorData[i].IntervalType == SENSOR_INTERVALTYPE_REGULAR)
        {
            nwy_dbg_log("HasPendingRegularData: Found pending sensor type %d", SensorData[i].SensorType);
            return 1;
        }
    }
    nwy_dbg_log("HasPendingRegularData: No pending regular sensors");
    return 0;
}

// Make interrupt sensor packet - returns sensor type if interrupt packet available, 0 otherwise
// Format: $SINT,IMEI,Date,Time,Lat,Long,Speed,Altitude,HDOP,IGN,SOS,SensorType,SensorDataType,{SensorData}*CS\r\n
uint8_t MakeInterruptPacket(char* databuf, uint16_t bufferSize)
{
    char tempStr[20];
    uint8_t checksum;
    int startPos;
    
    if(databuf == NULL || bufferSize < 100)
        return 0;
    
    // Find first pending interrupt sensor
    for(int i = 0; i < MAX_SENSORS; i++)
    {
        if(SensorData[i].IsActive && 
           SensorData[i].IsPending && 
           SensorData[i].IntervalType == SENSOR_INTERVALTYPE_INTERRUPT)
        {
            // Build packet: $SINT,IMEI,Date,Time,Lat,Long,Speed,Altitude,HDOP,Heading,IGN,SOS,SensorType,SensorDataType,{SensorData}*CS\r\n
            memset(databuf, 0, bufferSize);
            strcpy(databuf, "$SINT,");
            
            // IMEI
            strncat(databuf, NetWork.IMEI, 15);
            InsertChar(databuf, ',');
            
            // Date (DDMMYY)
            InsertCurrentDateTime(databuf, 0);
            InsertChar(databuf, ',');
            
            // Time (HHMMSS)
            InsertCurrentDateTime(databuf, 1);
            InsertChar(databuf, ',');
            
            // Latitude
            strcat(databuf, sLatitude);
            InsertChar(databuf, ',');
            
            // Longitude
            strcat(databuf, sLongitude);
            InsertChar(databuf, ',');
            
            // Speed
            strcat(databuf, sSpeed);
            InsertChar(databuf, ',');
            
            // Altitude
            strcat(databuf, sAltitude);
            InsertChar(databuf, ',');
            
            // HDOP
            strcat(databuf, sHDOP);
            InsertChar(databuf, ',');
            
            // Heading
            strcat(databuf, sHeading);
            InsertChar(databuf, ',');
            
            // Ignition Status (0 or 1)
            InsertChar(databuf, PeriPheralVal.IGN + '0');
            InsertChar(databuf, ',');
            
            // SOS Status (0 or 1)
            InsertChar(databuf, SOS.IsSOS + '0');
            InsertChar(databuf, ',');
            
            // Sensor Type
            sprintf(tempStr, "%d", SensorData[i].SensorType);
            strcat(databuf, tempStr);
            InsertChar(databuf, ',');
            
            // Sensor Data Type
            sprintf(tempStr, "%d", SensorData[i].DataType);
            strcat(databuf, tempStr);
            InsertChar(databuf, ',');
            
            // Sensor Data in braces
            strcat(databuf, "{");
            if(SensorData[i].DataType == SENSOR_DATATYPE_ASCII)
                strncat(databuf, SensorData[i].SensorData, SensorData[i].DataLen);
            else
            {
                // For HEX data type, convert to hex string
                for(uint8_t j = 0; j < SensorData[i].DataLen; j++)
                {
                    sprintf(tempStr, "%02X", (uint8_t)SensorData[i].SensorData[j]);
                    strcat(databuf, tempStr);
                }
            }
            strcat(databuf, "}");
            
            // Calculate checksum (XOR from after $ to before *)
            startPos = 1; // Skip '$'
            checksum = CalculateSensorChecksum(&databuf[startPos], strlen(databuf) - startPos);
            
            // Append checksum and terminator
            sprintf(tempStr, "*%02X\r\n", checksum);
            strcat(databuf, tempStr);
            
            // Don't clear IsPending here - caller will call ClearSensorData after successful send
            // Return the sensor type so caller can clear it
            return SensorData[i].SensorType;
        }
    }
    
    return 0;
}

// Make regular sensor packet - returns 1 if packet was made, 0 if no pending data
// Format: $SENS,IMEI,Date,Time,Lat,Long,Speed,Altitude,HDOP,IGN,SOS,NoofSensors,Sensor1Type,Sensor1DataType,{Sensor1Data},...*CS\r\n
uint8_t MakeSensorPacket(char* databuf, uint16_t bufferSize)
{
    char tempStr[20];
    uint8_t checksum;
    int startPos;
    uint8_t sensorCount = 0;
    char sensorDataSection[512];
    
    if(databuf == NULL || bufferSize < 100)
        return 0;
    
    // Check if sensors are enabled
    if(!SensorsConfig.isEnabled)
        return 0;
    
    // First, count pending regular sensors and build sensor data section
    memset(sensorDataSection, 0, sizeof(sensorDataSection));
    
    for(int i = 0; i < MAX_SENSORS; i++)
    {
        if(SensorData[i].IsActive && 
           SensorData[i].IsPending && 
           SensorData[i].IntervalType == SENSOR_INTERVALTYPE_REGULAR)
        {
            // Add separator if not first sensor
            if(sensorCount > 0)
            {
                strcat(sensorDataSection, ",");
            }
            
            // Sensor Type
            sprintf(tempStr, "%d,", SensorData[i].SensorType);
            strcat(sensorDataSection, tempStr);
            
            // Sensor Data Type
            sprintf(tempStr, "%d,", SensorData[i].DataType);
            strcat(sensorDataSection, tempStr);
            
            // Sensor Data in braces
            strcat(sensorDataSection, "{");
            if(SensorData[i].DataType == SENSOR_DATATYPE_ASCII)
                strncat(sensorDataSection, SensorData[i].SensorData, SensorData[i].DataLen);
            else
            {
                // For HEX data type, convert to hex string
                for(uint8_t j = 0; j < SensorData[i].DataLen; j++)
                {
                    sprintf(tempStr, "%02X", (uint8_t)SensorData[i].SensorData[j]);
                    strcat(sensorDataSection, tempStr);
                }
            }
            strcat(sensorDataSection, "}");
            
            sensorCount++;
        }
    }
    
    // If no pending sensors, return 0
    if(sensorCount == 0)
        return 0;
    
    // Build the full packet
    memset(databuf, 0, bufferSize);
    strcpy(databuf, "$SENS,");
    
    // IMEI
    strncat(databuf, NetWork.IMEI, 15);
    InsertChar(databuf, ',');
    
    // Date (DDMMYY)
    InsertCurrentDateTime(databuf, 0);
    InsertChar(databuf, ',');
    
    // Time (HHMMSS)
    InsertCurrentDateTime(databuf, 1);
    InsertChar(databuf, ',');
    
    // Latitude
    strcat(databuf, sLatitude);
    InsertChar(databuf, ',');
    
    // Longitude
    strcat(databuf, sLongitude);
    InsertChar(databuf, ',');
    
    // Speed
    strcat(databuf, sSpeed);
    InsertChar(databuf, ',');
    
    // Altitude
    strcat(databuf, sAltitude);
    InsertChar(databuf, ',');
    
    // HDOP
    strcat(databuf, sHDOP);
    InsertChar(databuf, ',');
    
    // Heading
    strcat(databuf, sHeading);
    InsertChar(databuf, ',');
    
    // Ignition Status (0 or 1)
    InsertChar(databuf, PeriPheralVal.IGN + '0');
    InsertChar(databuf, ',');
    
    // SOS Status (0 or 1)
    InsertChar(databuf, SOS.IsSOS + '0');
    InsertChar(databuf, ',');
    
    // Number of sensors
    sprintf(tempStr, "%d,", sensorCount);
    strcat(databuf, tempStr);
    
    // Append all sensor data
    strcat(databuf, sensorDataSection);
    
    // Calculate checksum (XOR from after $ to before *)
    startPos = 1; // Skip '$'
    checksum = CalculateSensorChecksum(&databuf[startPos], strlen(databuf) - startPos);
    
    // Append checksum and terminator
    sprintf(tempStr, "*%02X\r\n", checksum);
    strcat(databuf, tempStr);
    
    // Mark all regular sensors as sent
    for(int i = 0; i < MAX_SENSORS; i++)
    {
        if(SensorData[i].IsActive && 
           SensorData[i].IsPending && 
           SensorData[i].IntervalType == SENSOR_INTERVALTYPE_REGULAR)
        {
            SensorData[i].IsPending = 0;
        }
    }
    
    return 1;
}

// Get current sensor interval based on IGNITION state and time of day
// Returns interval in seconds, based on SensorsConfig.intervalConfig
// Day: 6:00 AM to 6:00 PM (Hour 6-17), Night: 6:00 PM to 6:00 AM (Hour 18-5)
uint16_t GetCurrentSensorInterval(void)
{
    uint8_t isDay;
    uint8_t isIgnition;
    
    // Check if sensors are enabled
    if(!SensorsConfig.isEnabled)
        return 0;
    
    // Determine if it's day or night (Day: 6 AM to 6 PM)
    // CurrentDateTime.Hour is in 24-hour format
    isDay = (CurrentDateTime.Hour >= 6 && CurrentDateTime.Hour < 18) ? 1 : 0;
    
    // Get ignition state
    isIgnition = PeriPheralVal.IGN;
    
    // Return appropriate interval based on conditions
    if(isIgnition)
    {
        // Ignition ON
        if(isDay)
            return (uint16_t)SensorsConfig.intervalConfig.dayIGNITIONInterval;
        else
            return (uint16_t)SensorsConfig.intervalConfig.nightIGNITIONInterval;
    }
    else
    {
        // Ignition OFF
        if(isDay)
            return (uint16_t)SensorsConfig.intervalConfig.dayOFFInterval;
        else
            return (uint16_t)SensorsConfig.intervalConfig.nightOFFInterval;
    }
}

// Buffer for sensor packets
static char sensorPacketBuffer[600];

// Process all sensor packets - call from server thread
// Handles interrupt packets immediately (high priority) and regular packets when IsSensPacket is set
// Returns: number of packets sent
uint8_t ProcessSensors(void)
{
    uint8_t packetsSent = 0;
    uint8_t sensorType;
    uint8_t sendSuccess;
    
    // 1. First priority: Process all pending interrupt packets immediately
    // These are sent as soon as they are available, regardless of interval
    while(HasPendingInterrupt())
    {
        sensorType = MakeInterruptPacket(sensorPacketBuffer, sizeof(sensorPacketBuffer));
        if(sensorType)
        {
            sendSuccess = 0;
            
            // Send interrupt packet to server(s)
            #ifdef EXTENDED_IPS
            if(ServerSocket[3].SocketState == SOCKET_CONNECTED)
            {
                if(TCPSocket_SendString(&ServerSocket[3], sensorPacketBuffer))
                {
                    sendSuccess = 1;
                    packetsSent++;
                }
            }
            #endif
            if(ServerSocket[2].SocketState == SOCKET_CONNECTED)
            {
                if(TCPSocket_SendStringNoAck(&ServerSocket[2], sensorPacketBuffer))
                {
                    sendSuccess = 1;
                    packetsSent++;
                }
            }
            
            // Only clear the interrupt sensor if successfully sent to at least one server
            if(sendSuccess)
            {
                ClearSensorData(sensorType);
            }
            else
            {
                // Send failed - break out to avoid infinite loop
                // Sensor will be retried on next ProcessSensors call
                nwy_dbg_log("Interrupt sensor %d send failed, will retry", sensorType);
                break;
            }
        }
    }
    
    // 2. Second priority: Process regular sensor packets when interval reached
    if(IsPacketReady.IsSensPacket)
    {
        nwy_dbg_log("ProcessSensors: IsSensPacket is set, processing regular sensors");
        IsPacketReady.IsSensPacket = 0;
        
        // Check if we have any pending regular sensor data
        if(HasPendingRegularData())
        {
            if(MakeSensorPacket(sensorPacketBuffer, sizeof(sensorPacketBuffer)))
            {
                nwy_dbg_log("ProcessSensors: Sending sensor packet: %s", sensorPacketBuffer);
                // Send regular sensor packet to server(s)
                #ifdef EXTENDED_IPS
                if(ServerSocket[3].SocketState == SOCKET_CONNECTED)
                {
                    TCPSocket_SendString(&ServerSocket[3], sensorPacketBuffer);
                    packetsSent++;
                }
                #endif
                if(ServerSocket[2].SocketState == SOCKET_CONNECTED)
                {
                    TCPSocket_SendStringNoAck(&ServerSocket[2], sensorPacketBuffer);
                    packetsSent++;
                }
                else
                {
                    nwy_dbg_log("ProcessSensors: ServerSocket[2] not connected, state=%d", ServerSocket[2].SocketState);
                }
            }
            else
            {
                nwy_dbg_log("ProcessSensors: MakeSensorPacket returned 0");
            }
        }
    }
    
    return packetsSent;
}

// Helper function to check if a character is a valid hex digit
static uint8_t IsHexChar(char c)
{
    return ((c >= '0' && c <= '9') || 
            (c >= 'A' && c <= 'F') || 
            (c >= 'a' && c <= 'f'));
}

// Helper function to check if string is all hex characters
static uint8_t IsAllHex(const char* data, uint16_t len)
{
    for(uint16_t i = 0; i < len; i++)
    {
        if(!IsHexChar(data[i]))
            return 0;
    }
    return 1;
}

// Parse incoming RS232 sensor data from MCU (called from DecodeSMS when OTASource == OTA_SRC_SERIAL)
// Returns: 1 if data was recognized as sensor data (RFID or FUEL), 0 otherwise
uint8_t ParseSensorData(const char* data, uint16_t dataLen)
{
    if(data == NULL || dataLen == 0)
        return 0;
    
    // Check if sensors are enabled
    if(!SensorsConfig.isEnabled)
    {
        nwy_dbg_log("ParseSensorData: Sensors disabled");
        return 0;
    }
    
    nwy_dbg_log("ParseSensorData: dataLen=%d, first char=0x%02X, data='%s'", dataLen, data[0], data);
    
    // 1. Check for FUEL data: starts with '*' and ends with '#'
    // Format: "*XD,501J,05,1753,0200,8181,1753,0301#"
    if(data[0] == '*')
    {
        // Find the '#' terminator
        const char* endMarker = NULL;
        for(uint16_t i = 1; i < dataLen; i++)
        {
            if(data[i] == '#')
            {
                endMarker = &data[i];
                break;
            }
        }
        
        if(endMarker != NULL)
        {
            // Valid FUEL data format found
            // Calculate data length (excluding * and #)
            uint16_t fuelDataLen = (uint16_t)(endMarker - data - 1);
            
            if(fuelDataLen > 0 && fuelDataLen < 100)
            {
                // Store FUEL data (skip the '*', don't include '#')
                HandleSensorData(SENSOR_TYPE_FUEL1, 
                                 data + 1,           // Skip '*'
                                 fuelDataLen, 
                                 SENSOR_DATATYPE_ASCII, 
                                 SENSOR_INTERVALTYPE_REGULAR,  // FUEL is regular interval
                                 0);                 // No timeout
                
                nwy_dbg_log("FUEL sensor data received, len=%d", fuelDataLen);
                return 1;
            }
        }
    }
    
    // 2. Check for RFID data: 12 character hex string (no delimiters)
    // Format: "3E0066F560CD"
    // RFID data is exactly 12 hex characters representing 6 bytes
    // Also check for 12 hex chars at the start if there are trailing chars
    if(dataLen >= 12 && IsAllHex(data, 12))
    {
        // Check if remaining chars are just whitespace/newlines
        uint8_t validRfid = 1;
        if(dataLen > 12)
        {
            for(uint16_t i = 12; i < dataLen; i++)
            {
                if(data[i] != '\r' && data[i] != '\n' && data[i] != ' ' && data[i] != '\0')
                {
                    validRfid = 0;
                    break;
                }
            }
        }
        
        if(validRfid)
        {
            // Valid RFID data
            HandleSensorData(SENSOR_TYPE_RFID, 
                             data, 
                             12, 
                             SENSOR_DATATYPE_ASCII,      // It's ASCII representation of hex
                             SENSOR_INTERVALTYPE_INTERRUPT,  // RFID is interrupt type (send immediately)
                             0);                         // No timeout
            
            nwy_dbg_log("RFID sensor data received: %.12s", data);
            return 1;
        }
    }
    
    // 3. Check for CLS fuel sensor response: starts with '@' and ends with '#'
    // Format: "@01E130644332180384091068E0#"
    if(data[0] == '@' && dataLen > 10)
    {
        // Look for '#' terminator
        if(data[dataLen-1] == '#' || (dataLen > 1 && data[dataLen-2] == '#'))
        {
            // Potential CLS response - try to parse it
            if(ParseCLSResponse(data, dataLen))
            {
                nwy_dbg_log("CLS sensor response parsed successfully");
                return 1;
            }
        }
    }
    
    // Not recognized as sensor data
    nwy_dbg_log("ParseSensorData: Not recognized as sensor data");
    return 0;
}

// Save SensorsConfig to flash storage
void SaveSensorConfigToFlash(void)
{
    int fd = -1;
    int ret;
    
    fd = nwy_sdk_fopen(SENSOR_CONFIG_PATH, NWY_WB_PLUS_MODE);
    if(fd < 0)
    {
        nwy_dbg_log("Sensor Config File Create ERROR");
        return;
    }
    
    ret = nwy_sdk_fwrite(fd, (void*)&SensorsConfig, sizeof(SensorsTypedef));
    nwy_dbg_log("Sensor Config file write size: %d", ret);
    nwy_sdk_fclose(fd);
}

// Load SensorsConfig from flash storage (called at startup after InitSensors)
// Returns: 1 if loaded successfully, 0 if file not found or invalid (defaults used)
uint8_t LoadSensorConfigFromFlash(void)
{
    int fd = -1;
    int ret;
    SensorsTypedef tempConfig;
    
    if(!nwy_sdk_fexist(SENSOR_CONFIG_PATH))
    {
        nwy_dbg_log("Sensor Config file not found, using defaults");
        return 0;
    }
    
    ret = nwy_sdk_fsize(SENSOR_CONFIG_PATH);
    if(ret != sizeof(SensorsTypedef))
    {
        nwy_dbg_log("Sensor Config file size mismatch: %d vs %d", ret, sizeof(SensorsTypedef));
        return 0;
    }
    
    fd = nwy_sdk_fopen(SENSOR_CONFIG_PATH, NWY_RDONLY);
    if(fd < 0)
    {
        nwy_dbg_log("Sensor Config file open ERROR");
        return 0;
    }
    
    ret = nwy_sdk_fread(fd, (void*)&tempConfig, sizeof(SensorsTypedef));
    nwy_sdk_fclose(fd);
    
    if(ret != sizeof(SensorsTypedef))
    {
        nwy_dbg_log("Sensor Config file read error: %d", ret);
        return 0;
    }
    
    // Validate magic number
    if(tempConfig.magic != SENSOR_STUCT_MAGIC)
    {
        nwy_dbg_log("Sensor Config magic invalid: 0x%08X", tempConfig.magic);
        return 0;
    }
    
    // Valid config loaded - copy to SensorsConfig
    memcpy(&SensorsConfig, &tempConfig, sizeof(SensorsTypedef));
    nwy_dbg_log("Sensor Config loaded: EN=%d, DayIGN=%d, NightIGN=%d, DayOFF=%d, NightOFF=%d, Timeout=%d",
                SensorsConfig.isEnabled,
                SensorsConfig.intervalConfig.dayIGNITIONInterval,
                SensorsConfig.intervalConfig.nightIGNITIONInterval,
                SensorsConfig.intervalConfig.dayOFFInterval,
                SensorsConfig.intervalConfig.nightOFFInterval,
                SensorsConfig.sensorTimeout);
    
    return 1;
}

// Query CLS fuel level sensor
// Sends command: @01E0006# via RS232 (wrapped with MCU protocol headers)
// Command format:
//   @ - Packet head
//   01 - Sensor ID (fixed)
//   E - Command code (Read fuel level)
//   00 - Command content length (0 bytes)
//   06 - Checksum (sum of ASCII: 01+E+00 = 0x30+0x31+0x45+0x30+0x30 = 0x106, low byte = 0x06)
//   # - Packet end
void QueryCLSSensor(void)
{
    nwy_dbg_log("Querying CLS sensor");
    SendRS232String("@01E0006#");
}

// Calculate checksum for CLS protocol
// Checksum = low byte of sum of all ASCII bytes from ID to end of command content
static uint8_t CalculateCLSChecksum(const char* data, uint16_t start, uint16_t end)
{
    uint32_t sum = 0;
    for(uint16_t i = start; i < end; i++)
    {
        sum += (uint8_t)data[i];
    }
    return (uint8_t)(sum & 0xFF);
}

// Parse CLS sensor response
// Expected format: @01E130644332180384091068E0#
// Structure:
//   @ - Packet head (1 byte)
//   01 - Sensor ID (2 bytes)
//   E - Command code (1 byte)
//   13 - Command content length = 19 bytes (2 bytes, hex in ASCII)
//   06443 - Current fuel level in ten-thousandths (5 bytes) - e.g., 06443/10000 = 0.6443
//   3218 - Fuel level value 0-4095 (4 bytes) - raw ADC value
//   038409 - Fuel  level in litres x 100 (6 bytes) - e.g., 038409 = 384.09 litres
//   1 - Lid status (1 byte) - 1=open, 0=closed
//   068 - Reserved (3 bytes)
//   E0 - Checksum (2 bytes, hex in ASCII)
//   # - Packet end (1 byte)
uint8_t ParseCLSResponse(const char* data, uint16_t dataLen)
{
    char sensorIdStr[3];
    char commandCode;
    char contentLengthStr[3];
    uint16_t contentLength;
    char checksumStr[3];
    uint8_t calculatedChecksum;
    uint8_t receivedChecksum;
    
    // Minimum length: @01E1306443321803840910680E#  (about 26 chars minimum)
    if(dataLen < 20)
    {
        return 0;
    }
    
    // Check packet head and end
    if(data[0] != '@' || data[dataLen-1] != '#')
    {
        return 0;
    }
    
    // Extract sensor ID (should be "01")
    sensorIdStr[0] = data[1];
    sensorIdStr[1] = data[2];
    sensorIdStr[2] = '\0';
    
    // Extract command code (should be 'E')
    commandCode = data[3];
    if(commandCode != 'E')
    {
        return 0;  // Not a fuel level response
    }
    
    // Extract content length (2 hex digits in ASCII)
    contentLengthStr[0] = data[4];
    contentLengthStr[1] = data[5];
    contentLengthStr[2] = '\0';
    contentLength = (uint16_t)strtol(contentLengthStr, NULL, 16);
    
    nwy_dbg_log("CLS Response: ID=%s, Cmd=%c, ContentLen=%d", sensorIdStr, commandCode, contentLength);
    
    // Verify packet length matches: @ + ID(2) + Cmd(1) + Len(2) + Content + Checksum(2) + #
    // Total = 1 + 2 + 1 + 2 + contentLength + 2 + 1 = contentLength + 9
    if(dataLen != contentLength + 9)
    {
        nwy_dbg_log("CLS packet length mismatch: expected %d, got %d", contentLength + 9, dataLen);
        return 0;
    }
    
    // Extract checksum (last 2 chars before '#')
    checksumStr[0] = data[dataLen-3];
    checksumStr[1] = data[dataLen-2];
    checksumStr[2] = '\0';
    receivedChecksum = (uint8_t)strtol(checksumStr, NULL, 16);
    
    // Calculate checksum (from ID to end of content, excluding checksum and #)
    calculatedChecksum = CalculateCLSChecksum(data, 1, dataLen - 3);
    
    if(calculatedChecksum != receivedChecksum)
    {
        nwy_dbg_log("CLS checksum mismatch: calc=0x%02X, recv=0x%02X", calculatedChecksum, receivedChecksum);
        return 0;
    }
    
    // Valid CLS response - store entire response as-is (just like FUEL1 sensor)
    // Server can parse it if needed
    HandleSensorData(SENSOR_TYPE_CLS, data, dataLen, 
                    SENSOR_DATATYPE_ASCII, SENSOR_INTERVALTYPE_REGULAR, 
                    SensorsConfig.sensorTimeout);
    
    nwy_dbg_log("CLS sensor data stored: %s", data);
    return 1;
}