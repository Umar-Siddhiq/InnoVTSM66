#ifndef _SENSORS_H
#define _SENSORS_H



#include <stdio.h>
#include "project.h"

#define MAX_SENSORS 8
#define SENSOR_STUCT_MAGIC 0x53454E52  //"SENS"

typedef struct 
{
    uint8_t dayIGNITIONInterval;
    uint8_t nightIGNITIONInterval;
    uint8_t dayOFFInterval;
    uint8_t nightOFFInterval;
}IntervalConfigTypedef;

typedef enum{
    SENSOR_TYPE_RFID=1,
    SENSOR_TYPE_DHT11,
    SENSOR_TYPE_ANALOG,
    SENSOR_TYPE_FUEL1,
    SENSOR_TYPE_CLS,     // CLS fuel level sensor (query-response based)
    SENSOR_TYPE_MAX
}SensorTypeEnum;

typedef enum{
    SENSOR_DATATYPE_ASCII=1,
    SENSOR_DATATYPE_HEX,
    SENSOR_DATATYPE_MAX
}SensorDataTypeEnum;

typedef enum{
    SENSOR_INTERVALTYPE_REGULAR=1,
    SENSOR_INTERVALTYPE_INTERRUPT,
    SENSOR_INTERVALTYPE_MAX
}SensorIntervalTypeEnum;

typedef struct 
{
    uint32_t magic;
    uint8_t isEnabled;
    IntervalConfigTypedef intervalConfig;
    uint16_t sensorTimeout;  // Timeout in seconds before sensor is considered disconnected (for REGULAR sensors)
}SensorsTypedef;

typedef struct Sensors
{
    SensorTypeEnum SensorType;
    uint8_t IsActive;
    SensorDataTypeEnum DataType;
    SensorIntervalTypeEnum IntervalType;
    uint8_t IsPending;
    uint8_t SensorData[100];
    uint8_t DataLen;
    uint16_t Timeout;
}SensorDataTypedef;



extern SensorDataTypedef SensorData[MAX_SENSORS];
extern SensorsTypedef SensorsConfig;

// Initialize sensor subsystem
void InitSensors(void);

// Handle incoming sensor data - updates existing sensor or adds new one
void HandleSensorData(SensorTypeEnum sensorIndex, const char* data, uint16_t dataLen, uint8_t dataType, SensorIntervalTypeEnum intervalType, uint16_t timeout);

// Clear sensor data for a specific sensor type
void ClearSensorData(SensorTypeEnum sensorIndex);

// Process sensor timeouts - call periodically to expire stale sensors
void ProcessSensorTimeouts(uint16_t timeoutThreshold);

// Make regular sensor packet - returns 1 if packet was made, 0 if no pending data
// Format: $SENS,IMEI,Date,Time,NoofSensors,Sensor1Type,Sensor1DataType,{Sensor1Data},...*CS\r\n
uint8_t MakeSensorPacket(char* dataBuffer, uint16_t bufferSize);

// Make interrupt sensor packet - returns SensorType if packet available, 0 otherwise
// Caller should call ClearSensorData(returnedType) after successful send
// Format: $SINT,IMEI,Date,Time,SensorType,SensorDataType,{SensorData}*CS\r\n
uint8_t MakeInterruptPacket(char* dataBuffer, uint16_t bufferSize);

// Check if any interrupt sensor data is pending (for fast polling)
uint8_t HasPendingInterrupt(void);

// Check if any regular sensor data is pending
uint8_t HasPendingRegularData(void);

// Get current sensor interval based on IGNITION state and time of day
// Returns interval in seconds, based on SensorsConfig.intervalConfig
// Day: 6:00 AM to 6:00 PM, Night: 6:00 PM to 6:00 AM
uint16_t GetCurrentSensorInterval(void);

// Process all sensor packets - call from server thread
// Handles interrupt packets immediately (high priority) and regular packets when IsSensPacket is set
// Returns: number of packets sent
uint8_t ProcessSensors(void);

// Parse incoming RS232 sensor data from MCU (called from DecodeSMS when OTASource == OTA_SRC_SERIAL)
// Returns: 1 if data was recognized as sensor data (RFID or FUEL), 0 otherwise
// Detection rules:
//   - FUEL: Starts with '*' and ends with '#' (e.g., "*XD,501J,05,1753,0200,8181,1753,0301#")
//   - RFID: 12 character hex string without delimiters (e.g., "3E0066F560CD")
uint8_t ParseSensorData(const char* data, uint16_t dataLen);

// Query CLS fuel level sensor - sends @01E0006# command via RS232
void QueryCLSSensor(void);

// Parse CLS sensor response - handles @01E13...# format
// Returns 1 if valid CLS response parsed, 0 otherwise
uint8_t ParseCLSResponse(const char* data, uint16_t dataLen);

// Save SensorsConfig to flash storage
void SaveSensorConfigToFlash(void);

// Load SensorsConfig from flash storage (called at startup after InitSensors)
// Returns: 1 if loaded successfully, 0 if file not found or invalid (defaults used)
uint8_t LoadSensorConfigFromFlash(void);
    
#endif                                // _SENSORS_H