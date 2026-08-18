#ifndef GPS_H
#define GPS_H
#include "VTS.h"
#include "Hardware.h"
#include "Systic.h"
#include "Utilities.h"
#include "NMeaParser.h"


#define GPS_UART_PORT UART_PORT3
#define GPS_UART_BAUDRATE 115200

#define GPS_UART_BUFFER_SIZE 4096

#define GPS_RCV_TIMEOUT  100 // 100*30ms = 3s (GPS thread sleeps 30ms per loop)


#define GPS_RESET_PIN   PINNAME_CTS
#define GPS_RESET_ON    Ql_GPIO_SetLevel(GPS_RESET_PIN,PINLEVEL_HIGH)
#define GPS_RESET_OFF   Ql_GPIO_SetLevel(GPS_RESET_PIN,PINLEVEL_LOW)
#define GPS_RESET_VAL   Ql_GPIO_GetLevel(GPS_RESET_PIN)
#define GPS_RESET_INIT  Ql_GPIO_Init(GPS_RESET_PIN, PINDIRECTION_OUT, PINLEVEL_LOW, PINPULLSEL_DISABLE);

typedef struct
{
    uint8_t State;       
	uint8_t GPSFix;			// 1 or 0
	double Latitude;
	char LatDir;
	double Longitude;
	char LngDir;
	double Speed;
	double Heading;
	uint8_t NoOfSatalite;
	double Altitude;
	double PDOP;
	double HDOP;
	struct NMEA_TIME Time;
	struct NMEA_DATE Date;
     // Satellite counts
    int SatTotal;
    int SatGPS;     // GP
    int SatGLONASS; // GL
    int SatGalileo; // GA
    int SatBeiDou;  // GB
    int SatQZSS;    // GQ
    int SatNavIC;   // GI
    int SatMixed;   // GN (combined multi-constellation sentences)
	
}GPS_Typedef;

extern char	sLatitude[];
extern char sLongitude[];
extern char sAltitude[];
extern char sSpeed[];
extern char sPDOP[];
extern char sHDOP[];
extern char sHeading[];

extern GGATypedef GGAData;
extern RMCTypedef RMCData;
extern GSVTypedef GSVData;
extern ZDATypedef ZDAData;
extern VTGTypedef VTGData;
extern GLLTypedef GLLData;
extern GSTTypedef GSTData;
extern  GPS_Typedef GPS;
extern _RTC GPSDateTime;
int gps_init(void);
void gps_thread_init(u32 taskId);


typedef enum {
    GPS_CMD_NONE = 0,
    GPS_CMD_REBOOT,
    GPS_CMD_UART_CONFIG,
    GPS_CMD_READ_CONFIG,
    // Add other command types here
} GPS_CmdType;



typedef struct {
    GPS_CmdType type;
    const char* cmd_prefix;    // Command identifier (e.g., "$PQTMCFGUART")
    bool response_pending;
    bool response_received;
    bool response_ok;
    char response[100];
} GPS_CmdStatus;
extern GPS_CmdStatus gps_cmd;

void GPS_UartSendString(const char *str);
int GPS_UartSendRaw(const uint8_t* data, uint16_t len);

// Request a GNSS reboot/re-init from the GPS thread (safe, non-blocking).
// Intended use: after Flash EPO injection, so receiver reloads EPO from flash.
void GPS_RequestReinit(void);

void GPS_BinaryAckReset(void);
bool GPS_WaitBinaryAck(uint16_t ackForMsgId, uint32_t timeoutMs);
void gps_thread_entry(s32 taskId);
void SendNMEAToRS232(void);
void gps_overspeed_check(void);
void ApplyFGPS(void);
bool GPS_IsSimulationActive(void);
uint8_t GPS_GetState(void);

#endif // GPS_H
