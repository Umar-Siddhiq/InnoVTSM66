"""Regression test for OG PVT packet formatting.

Validates:
1. Heading (Course Over Ground) is formatted as an integer (%u), e.g. 30 or 126.
2. Analog/distance fields format as 0.00,0.00,0.00 (DeltaDis %.2f).
3. Packet terminal ends with a single set of brackets '()' before '*<CS>\\r\\n' when no RFID sensor is active.
4. Packet terminal includes ',TAG...' when an active RFID sensor is present, with no second '()'.
5. Checksum is valid and passes GetXORChecksum.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "custom/Server.c").read_text()


OG_MARKER = "#ifdef PROTO_OG\n/* AMD3 sect4 - $PVT packet */"
og_start = source.index(OG_MARKER)

def function(signature, from_idx=0):
    start = source.index(signature + "\n{", from_idx) if signature + "\n{" in source[from_idx:] else source.index(signature + " {", from_idx)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


init_buffer = function("void InitBuffer(uint8_t alt)", og_start)

# Extract helper functions needed by InitBuffer
bounded_helpers = source[source.index("static uint8_t PVTAppendBounded", og_start):source.index("void InitBuffer(uint8_t alt)", og_start)]

program = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PROTO_OG
#define DATA_MAX_BUFF 1024
#define MAX_SENSORS 5
#define SENSOR_TYPE_RFID 1
#define SOS_ON_ALERT 0
#define SOS_OFF_ALERT 1
#define TAG_SERVER "SERVER"

#define Ql_strstr strstr
#define Ql_strlen strlen
#define Ql_sprintf sprintf
#define Ql_snprintf snprintf
#define Ql_strcpy strcpy
#define Ql_memcpy memcpy
#define Ql_memset memset
#define LOGData(...) ((void)0)

char dataBuffer[DATA_MAX_BUFF];
uint8_t lastcrc;
uint32_t FrameNumber = 6200;
uint16_t DeltaDis = 0;
char FirmVer[16] = "01.05.09";
char sLatitude[20] = "28.360014";
char sLongitude[20] = "76.927682";

struct {
    char VendorID[10];
    struct { char VehicleRegNo[20]; } VehicleData;
} VTSData;

struct {
    char IMEI[16];
    char Network[16];
} NetWork;

struct {
    uint8_t GPSFix;
    char LatDir;
    char LngDir;
    double Speed;
    double Heading;
    uint32_t NoOfSatalite;
    double Altitude;
    double PDOP;
    double HDOP;
} GPS;

struct {
    uint8_t Hour, Min, Sec;
    uint8_t Date, Month, Year;
} CurrentDateTime;

struct {
    uint8_t IGN, IsMain;
    float MainsVolt, BattVolt;
    uint8_t IsCoverOpen;
    uint8_t IP1, IP2, OP1, OP2;
} PeriPheralVal;

struct {
    uint8_t Enable;
} VAlert[5];

struct {
    uint8_t CellDB[8];
    char LAC[8];
    char CellID[8];
} Neighbor;

struct {
    uint8_t SignalStrength;
    uint32_t MCC, MNC;
    char LAC[8];
    char CellID[8];
    struct {
        char CellDB[8];
        char LAC[8];
        char CellID[8];
    } NeigbourCell[4];
} GSM;

struct {
    uint8_t SensorType;
    uint8_t IsActive;
    uint8_t SensorData[32];
} SensorData[MAX_SENSORS];

struct {
    int Pending;
    int Status;
    char Source[64];
    char Mode[16];
    char CmdId[16];
    char Value[64];
} LastOTAResponse;

#define INPUT_SOS_VAL 0

uint8_t GetReportedSignalStrength(void) {
    return 19;
}

uint8_t GetXORChecksum(const char *buf, int len) {
    uint8_t cs = 0;
    for (int i = 0; i < len; i++)
        cs ^= (uint8_t)buf[i];
    return cs;
}
'''

program += bounded_helpers + "\n" + init_buffer + "\n"

program += r'''
void setup_default_state(void) {
    memset(dataBuffer, 0, sizeof(dataBuffer));
    memset(&VTSData, 0, sizeof(VTSData));
    memset(&NetWork, 0, sizeof(NetWork));
    memset(&GPS, 0, sizeof(GPS));
    memset(&CurrentDateTime, 0, sizeof(CurrentDateTime));
    memset(&PeriPheralVal, 0, sizeof(PeriPheralVal));
    memset(VAlert, 0, sizeof(VAlert));
    memset(&GSM, 0, sizeof(GSM));
    memset(SensorData, 0, sizeof(SensorData));
    memset(&LastOTAResponse, 0, sizeof(LastOTAResponse));

    strcpy(VTSData.VendorID, "HTEC");
    strcpy(VTSData.VehicleData.VehicleRegNo, "UNKNOWN");
    strcpy(NetWork.IMEI, "861329085922533");
    strcpy(NetWork.Network, "airtel");

    GPS.GPSFix = 1;
    GPS.LatDir = 'N';
    GPS.LngDir = 'E';
    GPS.Speed = 0.3;
    GPS.Heading = 29.52;   /* Course over ground float input */
    GPS.NoOfSatalite = 13;
    GPS.Altitude = 301.9;
    GPS.PDOP = 1.8;
    GPS.HDOP = 1.5;

    CurrentDateTime.Date = 16;
    CurrentDateTime.Month = 9;
    CurrentDateTime.Year = 26;
    CurrentDateTime.Hour = 9;
    CurrentDateTime.Min = 53;
    CurrentDateTime.Sec = 38;

    PeriPheralVal.IGN = 1;
    PeriPheralVal.IsMain = 1;
    PeriPheralVal.MainsVolt = 12.7;
    PeriPheralVal.BattVolt = 4.2;
    PeriPheralVal.IsCoverOpen = 0;
    PeriPheralVal.IP1 = 1;
    PeriPheralVal.IP2 = 1;

    GSM.MCC = 404;
    GSM.MNC = 10;
    strcpy(GSM.LAC, "0209");
    strcpy(GSM.CellID, "4333");
    for (int i = 0; i < 4; i++) {
        strcpy(GSM.NeigbourCell[i].CellDB, "17");
        strcpy(GSM.NeigbourCell[i].LAC, "0209");
        strcpy(GSM.NeigbourCell[i].CellID, "750E");
    }

    FrameNumber = 6200;
    DeltaDis = 0;
}

int main(void) {
    /* Test 1: Normal packet without RFID, heading = 29.52 */
    setup_default_state();
    InitBuffer(1);

    printf("Generated packet:\n%s\n", dataBuffer);

    /* 1. Course Over Ground check: must be integer (30), no decimal point */
    assert(strstr(dataBuffer, ",000.3,30,13,") != NULL);

    /* 2. Presentation format check: must be 0.00,0.00,0.00 */
    assert(strstr(dataBuffer, ",006200,0.00,0.00,0.00,()") != NULL);

    /* 3. Bracket check: only once '()' before '*<CS>\r\n' */
    assert(strstr(dataBuffer, ",0.00,0.00,0.00,()*") != NULL);
    assert(strstr(dataBuffer, "(),()") == NULL);

    /* 4. Checksum check */
    char *star = strchr(dataBuffer, '*');
    assert(star != NULL);
    unsigned cs;
    assert(sscanf(star + 1, "%2x", &cs) == 1);
    assert(cs == GetXORChecksum(dataBuffer + 1, star - dataBuffer - 1));

    /* Test 2: Sample heading 126 (from checklist sample in image) */
    setup_default_state();
    GPS.Heading = 126.0;
    InitBuffer(1);
    assert(strstr(dataBuffer, ",000.3,126,13,") != NULL);
    assert(strstr(dataBuffer, ",006200,0.00,0.00,0.00,()*") != NULL);

    /* Test 3: RFID Tag active */
    setup_default_state();
    SensorData[0].SensorType = SENSOR_TYPE_RFID;
    SensorData[0].IsActive = 1;
    strcpy((char*)SensorData[0].SensorData, "8520XYZ123");
    InitBuffer(1);
    printf("RFID packet:\n%s\n", dataBuffer);
    assert(strstr(dataBuffer, ",0.00,0.00,0.00,(),TAG8520XYZ123*") != NULL);
    assert(strstr(dataBuffer, "(),()") == NULL);

    /* Test 4: OTA response pending (alt = 12) */
    setup_default_state();
    LastOTAResponse.Pending = 1;
    strcpy(LastOTAResponse.Source, "78.46.190.117:50011");
    strcpy(LastOTAResponse.Mode, "SET");
    strcpy(LastOTAResponse.CmdId, "002");
    strcpy(LastOTAResponse.Value, "192.168.1.1");
    LastOTAResponse.Status = 1;
    InitBuffer(12);
    printf("OTA packet:\n%s\n", dataBuffer);
    assert(strstr(dataBuffer, ",0.00,0.00,0.00,(78.46.190.117:50011|SET|002:192.168.1.1:1)*") != NULL);
    assert(strstr(dataBuffer, "(),") == NULL);

    puts("All OG PVT format regression assertions passed successfully!");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="og_pvt_") as tmp:
    c = Path(tmp) / "test.c"
    exe = Path(tmp) / "test.exe"
    c.write_text(program)
    subprocess.run(["gcc", "-std=c99", "-Wall", str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
