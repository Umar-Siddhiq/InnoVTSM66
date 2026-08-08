# Comprehensive CDAC (CD1) Protocol Implementation & Porting Specification

---

## 1. Overview & Build Architecture

This document provides an exhaustive, production-grade technical specification of the **CDAC (CD1) AIS-140 Vehicle Tracking & Monitoring Protocol** as implemented in this codebase. It is designed to serve as a complete reference for porting the CDAC protocol to another hardware platform or firmware codebase without requiring prior familiarity with the source tree.

### 1.1 Compile-Time Selection & Feature Control

The CDAC protocol is activated via a single primary compile flag in `custom/config/custom_proto_cfg.h`:

```c
#ifndef PROTO_CDAC
#define PROTO_CDAC        // Active build variant for CDAC (CD1)
#endif
```

#### Compile-Time Truth Macro & Feature Dependencies
In `custom/inc/VTS.h`:
```c
#ifdef PROTO_CDAC
#define IS_PROTO_CDAC()   (1)
#else
#define IS_PROTO_CDAC()   (0)
#endif
```

#### Related Build Control Macros

| Macro | Header File | CDAC Setting / Effect |
| :--- | :--- | :--- |
| `PROTO_CDAC` | `custom_proto_cfg.h` | Core protocol switch. |
| `HTTP_QUEUE` | `custom_feature_def.h` | **Auto-enabled by CDAC.** Activates RAM/Flash HTTP packet queueing. |
| `PRF_AUTOSWITCH` | `custom/inc/VTS.h` | Enables multi-SIM fallback and SIM profile watchdog. |
| `KEEP_ALIVE` | `custom/inc/VTS.h` | Enables HTTP connection keep-alive mode. |
| `PROTO_TAG` | `custom/inc/VTS.h` | Set to `"CD1"`. Included in `FQ_CD1_1.5.8` version strings. |
| `ENABLE_UNIFIED_FIRMWARE` | `custom_feature_def.h` | **MUST BE OFF (undefined).** If enabled, forces `IS_PROTO_CDAC()` to 0. |
| `ENABLE_BATTERY_MONITOR` | `custom_feature_def.h` | **MUST BE OFF (undefined).** Prevents floating-point trace crashes in battery polling. |

---

## 2. Server Architecture & Server3 (3rd Server / VLT Secondary) Deep Dive

The CDAC implementation supports a multi-server architecture with specialized dual-transmission handling across primary and secondary endpoints.

```
                         ┌──────────────────────────────────────────┐
                         │              ServerThread                │
                         └───────────────────┬──────────────────────┘
                                             │
                                   SendDataToServer()
                                             │
                       ┌─────────────────────┴─────────────────────┐
                       ▼                                           ▼
            Server 1 (Primary CDAC)                     Server 3 (Secondary VLT / 3rd Server)
       surakshamitr.org/CDC:443 (HTTP)                78.46.190.117/vlt:50024 (HTTP or TCP)
```

### 2.1 Default Server Configuration & Structs

In `custom/inc/VTS.h`:
```c
#ifdef PROTO_CDAC
  #define DEFAULT_IP1   "http://surakshamitr.org/CDC"   // Primary CDAC Endpoint (Server 1)
  #define DEFAULT_PORT1 "443"

  #define DEFAULT_IP2   "NA"                            // Unused in CDAC (single primary)

  #define DEFAULT_IP3   "http://78.46.190.117/vlt"      // Secondary 3rd Server Endpoint (Server 3)
  #define DEFAULT_PORT3 "50024"
#endif
```

#### Data Storage Structure (`VTSData.ServerData`)
In `custom/inc/VTS.h`:
```c
typedef struct {
    char IP1[128];    // Primary URL (Server 1)
    char Port1[6];
    char IP2[50];     // Secondary IP (Unused in CDAC)
    char Port2[6];
    char IP3[128];    // Secondary 3rd Server URL/IP (Server 3)
    char Port3[6];
    char IP4[50];
    char Port4[6];
    uint8_t IPConfig[4];
} ServerConfigTypedef;
```

---

### 2.2 3rd Server (`SET SU`) Configuration Logic

The 3rd server is dynamically configured using the `SU` key via OTA SMS, Server HTTP response, RS232, or RS485 interfaces.

#### Endpoint Normalization Algorithm (`ServerNormalizeEndpoint`)
When `SET SU` or `UpdateSecondaryURL()` is called, the endpoint string is passed through `ServerNormalizeEndpoint()` to automatically handle scheme prefixes, ports, and transport mode selection:

```c
void UpdateSecondaryURL(char* value) {
    char url[128];
    char port[sizeof(VTSData.ServerData.Port3)];

    if (!value) return;

    // Preserve existing port if not specified in value string
    ServerCopyText(port, sizeof(port), VTSData.ServerData.Port3);
    
    // Normalize string: extracts scheme, host, port, and path
    ServerNormalizeEndpoint(value, url, sizeof(url), port, sizeof(port));

    // Save normalized IP3 and Port3 into persistent configuration
    Ql_strncpy(VTSData.ServerData.Port3, port, sizeof(VTSData.ServerData.Port3) - 1);
    VTSData.ServerData.Port3[sizeof(VTSData.ServerData.Port3) - 1] = '\0';
    
    Ql_strncpy(VTSData.ServerData.IP3, url, sizeof(VTSData.ServerData.IP3) - 1);
    VTSData.ServerData.IP3[sizeof(VTSData.ServerData.IP3) - 1] = '\0';

    // Auto-detect transport mode: 0 = HTTP POST Mode, 1 = Raw TCP Socket Mode
    ServerSocket[2].isEnabled = IsHttpUrl(VTSData.ServerData.IP3) ? 0 : 1;
    ServerSocket[2].SocketState = SOCKET_CLOSED;
    
    Ql_strncpy(ServerSocket[2].DNSorIP, VTSData.ServerData.IP3, sizeof(ServerSocket[2].DNSorIP) - 1);
    ServerSocket[2].DNSorIP[sizeof(ServerSocket[2].DNSorIP) - 1] = '\0';
    ServerSocket[2].Port = Ql_atoi(VTSData.ServerData.Port3);

    LOGData(TAG_SERVER, "Secondary (3rd Server) URL updated: %s:%s (mode=%s)", 
            VTSData.ServerData.IP3, VTSData.ServerData.Port3, 
            ServerSocket[2].isEnabled ? "TCP" : "HTTP");
}
```

#### Dual Transport Modes for Server 3
1. **HTTP Mode (`isEnabled = 0`):** Activated if `IP3` contains `http://` or `https://` (e.g., `http://78.46.190.117/vlt:50024`). The firmware encapsulates packets in HTTP POST requests with prefix `vltdata=`.
2. **TCP Mode (`isEnabled = 1`):** Activated if `IP3` is a bare IP or domain without HTTP scheme (e.g., `78.46.190.117:50024`). The firmware sends raw binary/ASCII socket frames directly over a raw TCP socket connection.

---

### 2.3 `SU` OTA Command Specifications (Set, Get, Clear)

| Command Syntax | Function | Execution Flow | ACK Response Output |
| :--- | :--- | :--- | :--- |
| `SET SU:<URL>` | Sets 3rd Server URL/IP & Port | Calls `UpdateSecondaryURL()`, updates Flash NVM | `ACK,12,SU:http://78.46.190.117/vlt:50024*` |
| `SET SU:<IP>:<PORT>` | Sets 3rd Server IP and Port | Normalizes endpoint, sets socket mode | `ACK,12,SU:78.46.190.117:50024*` |
| `GET SU` | Reads current 3rd Server config | Reads `IP3` and `Port3` from `VTSData.ServerData` | `ACK,12,SU:http://78.46.190.117/vlt:50024*` |
| `CLR SU` | Resets 3rd Server to defaults | Resets `IP3` -> `DEFAULT_IP3`, `Port3` -> `DEFAULT_PORT3` | `ACK,12,SU:http://78.46.190.117/vlt:50024*` |

#### Legacy Commands Mapped to 3rd Server (`PIP` & `SERVER3`)
In addition to standard `SU` commands, the codebase supports legacy command formats that map to `IP3`/`Port3`:
- **`SET PIP#<IP>,<PORT>;` / `GET PIP` / `CLR PIP`:** Parsed in `custom/SMS.c` (~892), updates `VTSData.ServerData.IP3` and `Port3`.
- **`SERVER3 <IP>,<PORT>`:** Legacy multi-server configuration string parsed in `custom/SMS.c` (~3027), updates 3rd server parameters directly.

---

### 2.4 Dual Server Transmission Engine (`SendDataToServer`)

When transmitting telemetry (`NRM`), health (`HLM`), full status (`FUL`), or alerts (`EPB`/`CRT`/`ALT`), `SendDataToServer` transmits to **Server 1** first, followed by **Server 3** (the 3rd Server).

```c
uint8_t SendDataToServer(char* data, uint8_t KeepAlive, uint16_t currentIntervalSec) {
    uint8_t isGood = 0;   // Server 1 delivery status flag
    uint8_t vlt_good = 0; // Server 3 (3rd Server) delivery status flag

    if(GSM.GSMState != GPRS_ACTIVE) return 0;

    // 1. Calculate dynamic interval-scaled HTTP timeouts
    uint32_t connect_tmout = 3000, response_tmout = 3000;
    if(currentIntervalSec <= 5) {
        connect_tmout = 1500; response_tmout = 3000;
    } else if(currentIntervalSec <= 30) {
        connect_tmout = 2000;
        response_tmout = (currentIntervalSec * 1000UL);
        response_tmout = (response_tmout > 2000) ? response_tmout - 2000 : 1000;
    } else {
        connect_tmout = 3000; response_tmout = 30000;
    }

    // 2. Transmit to Server 1 (Primary CDAC HTTP Endpoint)
    if(HTTP_Setup(ServerSocket[0].DNSorIP, (uint16_t)ServerSocket[0].Port)) {
        if(ServerSocket[0].SocketState == SOCKET_CONNECTED) {
            if(HTTP_Post(KeepAlive, 0, data, Ql_strlen(data), 0)) {
                if(IsHTTPRes) {
                    isGood = 1;
                    LOGData(TAG_SERVER, "Parsing Server 1 Data...");
                    DecodeOTAData(ServerSocket[0].rxBuffer, 1);
                }
            }
        }
    }

    // 3. Transmit to Server 3 (3rd Server / VLT Secondary)
    if(IsHttpUrl(ServerSocket[2].DNSorIP) && ServerSocket[2].Port > 0) {
        LOGData(TAG_SERVER, "VLT HTTP: Sending to 3rd Server %s:%d", 
                ServerSocket[2].DNSorIP, ServerSocket[2].Port);
        
        // M66 QHTTP AT session is shared. Close Server 1 session to prevent host mismatch.
        HTTP_Close(0);
        ServerSocket[0].SocketState = SOCKET_IDLE;
        ThreadSleep(500);

        for(int lp3 = 0; lp3 < 3; lp3++) { // Up to 3 attempts for 3rd server
            LOGData(TAG_SERVER, "VLT HTTP: attempt %d/3", lp3 + 1);
            if(!HTTP_Setup(ServerSocket[2].DNSorIP, (uint16_t)ServerSocket[2].Port)) {
                HTTPConnectFlag = 1;
                continue;
            }
            if(ServerSocket[0].SocketState == SOCKET_CONNECTED) {
                if(HTTP_Post(KeepAlive, 0, data, Ql_strlen(data), 0)) {
                    if(IsHTTPRes) {
                        vlt_good = 1;
                        LOGData(TAG_SERVER, "Parsing Server 3 Data...");
                        // Parse inbound OTA commands returned by 3rd server
                        DecodeOTAData(ServerSocket[0].rxBuffer, OTA_SRC_SCK_3);
                        Ql_memset(ServerSocket[0].rxBuffer, 0, ServerSocket[0].rxSizeMAX);
                        if(!KeepAlive) HTTP_Close(0);
                        break; // Success on 3rd server!
                    }
                }
            }
        }
    }
    
    HTTPConnectFlag = 1; // Force clean session re-establishment on next pass
    
    // Return SUCCESS (1) if EITHER Primary (Server 1) OR Secondary (Server 3) delivered
    return (isGood || vlt_good);
}
```

---

## 3. CDAC Alert Engine & Code Definitions

CDAC defines a 19-alert slot matrix (`ALERT_COUNT = 19`). Alert indices 0 to 18 are managed via three parallel lookup tables.

### 3.1 Alert Matrix & Parallel Lookup Tables

In `custom/inc/Alert.h` and `custom/Alert.c`:

```c
#ifdef PROTO_CDAC
#define ALERT_COUNT 19
#else
#define ALERT_COUNT 21
#endif

// Parallel Lookup Tables
static const uint8_t VTAlertHeaderType[ALERT_COUNT] = {0,0,1,1,1,2,1,2,2,2,1,1,1,2,2,3,2,2,2};
static const char    VTAlertPKT[ALERT_COUNT][3]     = {"10","11","16","03","22","09","17","13","14","15","23","20","21","18","19","12","06","04","05"};
static const uint8_t VTContType[ALERT_COUNT]        = {1,0,2,0,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0};
```

### 3.2 Master Alert Code Table

| Index | Constant Symbol | CD1 ID (`VTAlertPKT`) | Header Type | Header String | Repeat Mode (`VTContType`) | Event Description |
| :---: | :--- | :---: | :---: | :---: | :---: | :--- |
| **0** | `SOS_ON_ALERT` | `10` | 0 | `EPB` | `1` (URE) | Emergency / SOS Pressed |
| **1** | `SOS_OFF_ALERT` | `11` | 0 | `EPB` | `0` (None) | Emergency Deactivated |
| **2** | `SOS_TMP_ALERT` | `16` | 1 | `CRT` | `2` (Normal) | Emergency Wire Cut / Tamper |
| **3** | `MAINS_FAIL_ALERT` | `03` | 1 | `CRT` | `0` (None) | Main Battery Disconnected |
| **4** | `TILT_ALERT` | `22` | 1 | `CRT` | `2` (Normal) | Vehicle Tilt Detected |
| **5** | `TAMPER_ALERT` | `09` | 2 | `ALT` | `0` (None) | Case Open / Cover Tamper |
| **6** | `OVER_SPEED_ALERT` | `17` | 1 | `CRT` | `2` (Normal) | Overspeed Threshold Exceeded |
| **7** | `HARSH_BRK_ALERT` | `13` | 2 | `ALT` | `0` (None) | Harsh Braking Detected |
| **8** | `HARSH_ACC_ALERT` | `14` | 2 | `ALT` | `0` (None) | Harsh Acceleration Detected |
| **9** | `RASH_TURN_ALERT` | `15` | 2 | `ALT` | `0` (None) | Rash Turning Detected |
| **10** | `IMPACT_ALERT` | `23` | 1 | `CRT` | `0` (None) | Vehicle Collision / Impact |
| **11** | `GFIN_OS_ALERT` | `20` | 1 | `CRT` | `0` (None) | Overspeed Inside Geofence |
| **12** | `GFOUT_OS_ALERT` | `21` | 1 | `CRT` | `0` (None) | Overspeed Outside Geofence |
| **13** | `GFIN_ALERT` | `18` | 2 | `ALT` | `0` (None) | Geofence Entry |
| **14** | `GFOUT_ALERT` | `19` | 2 | `ALT` | `0` (None) | Geofence Exit |
| **15** | `CONF_CHANGE_ALERT` | `12` | 3 | `ACK` | `0` (None) | Parameter Config Change ACK |
| **16** | `MAINS_RES_ALERT` | `06` | 2 | `ALT` | `0` (None) | Main Power Restored |
| **17** | `BATT_LOW_ALERT` | `04` | 2 | `ALT` | `0` (None) | Internal Battery Low |
| **18** | `BATT_LOW_RES_ALERT` | `05` | 2 | `ALT` | `0` (None) | Internal Battery Restored |

### 3.3 Alert Priority & Classification Logic

1. **Critical Alerts (`HeaderType < 2`):** Headers `EPB` and `CRT`. Executed immediately, bypassing regular location transmission schedules. Priority ranking: `EPB > CRT > ALT > ACK`.
2. **Emergency Queue Bypassing:** When `SOS_ON_ALERT` triggers, the HTTP send queue is force-aborted (`HttpQueue_AbortSend()`) to allow immediate transmission of packet `EPB10`.

### 3.4 SMS Alert Engine (`SMSAlert`)

Machine-readable SMS notifications generated in `custom/Server.c`:

```c
void SMSAlert(uint8_t AlertNum) {
    char sms_body[160] = {0};
    // Format: <AlertCode>,<IMEI>,<VehReg>,<DDMMYY>,<HHMMSS>,<Lat>,<LatDir>,<Lon>,<LonDir>
    Ql_sprintf(sms_body, "%02d,%s,%s,%s,%s,%s,%c,%s,%c",
               AlertNum, NetWork.IMEI, VTSData.VehicleData.VehicleRegNo,
               sDate, sTime, sLatitude, GPS.LatDir, sLongitude, GPS.LngDir);

    // Append human readable text
    switch(AlertNum) {
        case 10: Ql_strcat(sms_body, " Emergency ON"); break;
        case 11: Ql_strcat(sms_body, " Emergency OFF"); break;
        case 16: Ql_strcat(sms_body, " Wire Cut Alert"); break;
        case 03: Ql_strcat(sms_body, " Main Power Removed"); break;
        case 17: Ql_strcat(sms_body, " Over Speed Alert"); break;
        case 22: Ql_strcat(sms_body, " Tilt Alert"); break;
        case 23: Ql_strcat(sms_body, " Impact Alert"); break;
    }
    SendSMSToAllAdmins(sms_body);
}
```

### 3.5 SOS Emergency State & Wire-Cut Tamper Logic

The CDAC protocol enforces a strict Emergency State machine with priority handling for wire cut (tamper) detection and wire reconnection:

1. **SOS ON Priority Preservation**:
   * When a panic button wire cut (`SOS_TMP_ALERT` Code 16) is detected, the Emergency State (`SOS.IsSOS`) remains active, and `SOS_ON_ALERT` (Code 10) is **not** cleared from memory.
   * Both `SOS_ON_ALERT` (Priority 1) and `SOS_TMP_ALERT` (Priority 2) are transmitted in correct priority order over GPRS.

2. **Wire Reconnection State Preservation**:
   * Reconnecting the cut panic button wire clears the tamper flag (`SOS.IsSOSTamper = 0`) and removes `SOS_TMP_ALERT` (Code 16).
   * It **does not** trigger an immediate `SOS_OFF_ALERT` (Code 11) or abort the emergency state. The Emergency State (`SOS.IsSOS`) continues running its mandatory 30-minute timer.
   * `SOS_OFF_ALERT` is only sent when the full 30-minute timer expires or when the control room sends the `SET EO` OTA command.

---

## 4. CD1 Raw Data & Wire Packet Layouts

All CD1 packets transmitted over HTTP POST are ASCII character buffers prefixed with `vltdata=` (8 bytes, `dLen = 8`).

### 4.1 Fixed-Position ASCII Formatters

```c
void InsertStringValue(const char* val, uint16_t pos, uint16_t len, uint8_t replaceHyphen) {
    for(uint16_t i = 0; i < len; i++) {
        char c = (val && val[i]) ? val[i] : ' ';
        if(replaceHyphen && c == '-') c = '0';
        SendString[pos + i] = c;
    }
}

void InsertIntValueCDAC(uint16_t val, uint16_t pos, uint16_t len) {
    char buf[16];
    Ql_sprintf(buf, "%0*d", len, val);
    Ql_memcpy(&SendString[pos], buf, len);
}

void InsertFloatValueCDAC(double val, uint16_t pos, uint16_t len, const char* fmt) {
    char buf[32];
    Ql_sprintf(buf, fmt, val);
    Ql_memcpy(&SendString[pos], buf, len);
}
```

### 4.2 Shared Telemetry Body (`DataPacket()`)

Relative offsets to base payload start `dLen` (Offset 8):

| Offset (`dLen+`) | Length | Field | Example Output | Format Description |
| :---: | :---: | :--- | :--- | :--- |
| **0** | 3 | Packet Header | `NRM`, `ALT`, `CRT`, `EPB` | ASCII Packet Type |
| **3** | 15 | IMEI | `864201040123456` | 15-digit Device IMEI |
| **18** | 2 | Packet Index | `01` | Incremental Packet Index |
| **20** | 1 | Packet Source | `L` / `H` | `L` = Live, `H` = History |
| **21** | 1 | GPS Fix Status | `1` / `0` | `1` = Valid 3D Fix, `0` = Invalid |
| **22** | 12 | IST Date & Time | `050826153000` | `DDMMYYHHMMSS` (IST +5:30) |
| **34** | 10 | Latitude | `1258.123456` | `%010.6f` DDMM.MMMMMM |
| **44** | 1 | Latitude Direction | `N` / `S` | `N` North, `S` South |
| **45** | 10 | Longitude | `07734.123456` | `%010.6f` DDDMM.MMMMMM |
| **55** | 1 | Longitude Direction| `E` / `W` | `E` East, `W` West |
| **56** | 3 | MCC | `404` | Mobile Country Code |
| **59** | 3 | MNC | `x45` / `xx7` | Mobile Network Code (left-padded `x`) |
| **62** | 4 | LAC | `1A2F` | Hex Location Area Code |
| **66** | 9 | Cell ID | `00001234A` | Hex Cell Identification |
| **75** | 6 | Vehicle Speed | `045.20` | `%06.2f` Speed in km/h |
| **81** | 6 | Heading Angle | `180.50` | `%06.2f` Heading in Degrees |
| **87** | 2 | Satellites Count | `12` | Tracked Satellites |
| **89** | 2 | HDOP | `01` | Horizontal Dilution of Precision |
| **91** | 2 | CSQ Signal Level | `24` | GSM Signal (0–31) |
| **93** | 1 | Ignition State | `1` / `0` | `1` = Ignition ON, `0` = OFF |
| **94** | 1 | Main Power State | `1` / `0` | `1` = Mains Present, `0` = Removed |
| **95** | 1 | Vehicle State Mode| `M` / `H` / `S` | `M`=Motion, `H`=Halt, `S`=Sleep |
| **96** | 7 | Altitude | `00540.2` | Meters Above Sea Level |
| **103** | 6 | Operator Code | `AIRTEL` | Network Operator (Padded with X) |

### 4.3 CD1 Packet Variants

1. **`LGN` (Login Packet):**
   `vltdata=LGN` + IMEI(15) + ActivationKey(16) + Lat(10)+Dir + Lon(10)+Dir + DateTime(12) + Speed(6)
2. **`NRM` (Normal Position Packet):**
   `vltdata=NRM01L` + `DataPacket()`
3. **`HLM` (Health Keep-Alive Packet):**
   `vltdata=HLM` + VendorID(6) + FirmVer(6) + IMEI(15) + MotionInterval(3) + HaltInterval(3) + Batt%(3) + LowBattThr(2) + `060` + Digital I/O States(4) + `01` + IST DateTime(12)
4. **`FUL` (Full Status Packet with CRC16):**
   `vltdata=FUL01L` + `DataPacket()` + Extra Diagnostics + **CRC16 (4-digit hex)** calculated over the 220-byte payload at `dLen+220`.
5. **`BTH` (Batch Offline Buffer Packet):**
   `vltdata=BTH` + IMEI(15) + PacketCount(3) + Concatenated Stored Records read from Flash memory (`01L` converted to `02H`).

---

## 5. OTA Command Engine Specifications

The OTA engine parses commands from **6 independent sources** using `DecodeOTAData(buff, isserver)`:
- `0`: SMS
- `1`: Server 1 (Primary HTTP)
- `2`: Server 2 (TCP)
- `3`: Server 3 / 3rd Server (`OTA_SRC_SCK_3`)
- `4`: RS232 Local Port (`OTA_SRC_RS232`)
- `5`: RS485 Local Port (`OTA_SRC_RS485`)

### 5.1 Command Formats

- **`SET` Command Syntax:** `SET <KEY>:<VAL>,<KEY2>:<VAL2>`
- **`GET` Command Syntax:** `GET <KEY>,<KEY2>`
- **`CLR` Command Syntax:** `CLR <KEY>,<KEY2>`

### 5.2 OTA Key Reference Dictionary (Including Server Configuration)

| Key | Description | SET Format | GET Output Example | CLR Default Value |
| :---: | :--- | :--- | :--- | :--- |
| **`PU`** | Primary Server URL (Server 1) | `SET PU:http://surakshamitr.org/CDC:443` | `http://surakshamitr.org/CDC:443` | `DEFAULT_IP1:443` |
| **`SU`** | **Secondary 3rd Server URL/IP** | `SET SU:http://78.46.190.117/vlt:50024` | `http://78.46.190.117/vlt:50024` | `DEFAULT_IP3:50024` |
| `VN` | Vehicle Reg Number | `SET VN:KL01CA1234` | `KL01CA1234` | `DEFAULT_VEHREG` |
| `M0`–`M3`| Admin Mobiles 0 to 3 | `SET M0:9876543210` | `9876543210` | `DEFAULT_MOB0` |
| `OM` | Operator Mobile | `SET OM:9876543210` | `9876543210` | `0000000000` |
| `ED` | Emergency Timeout | `SET ED:300` | `300` | `180` seconds |
| `ST` | Sleep Interval | `SET ST:5` (minutes) | `5` | `300` (5 mins) |
| `HT` | Halt Interval | `SET HT:1` (minutes) | `1` | `60` (1 min) |
| `UR` | Motion Interval | `SET UR:10` (seconds) | `10` | `10` seconds |
| `SL` | Overspeed Limit | `SET SL:80` (km/h) | `80` | `80` km/h |
| `TA` | Tilt Threshold Angle | `SET TA:45` (degrees) | `45` | `45` deg |
| `HBT` | Harsh Brake Threshold | `SET HBT:12` | `12` | `12` |
| `HAT` | Harsh Accel Threshold| `SET HAT:12` | `12` | `12` |
| `RTT` | Rash Turn Threshold | `SET RTT:15` | `15` | `15` |
| `LBT` | Low Battery Threshold | `SET LBT:20` (%) | `20` | `15` % |
| `VID` | Vendor ID Code | `SET VID:APMG` | `APMG` | `APMG` |

### 5.3 Command Response & ACK Construction

Responses are formatted into `VAlert[CONF_CHANGE_ALERT].ACK` and dispatched back to the issuing interface:

```c
// Example Configuration Response Construction for SET SU
VAlert[CONF_CHANGE_ALERT].Enable = 1;
strcpy(VAlert[CONF_CHANGE_ALERT].Header, "ACK");
strcpy(VAlert[CONF_CHANGE_ALERT].ACK, "OU:http://surakshamitr.org/CDC,SU:http://78.46.190.117/vlt:50024*");

if(isserver == 0) SendSMS(SMSSender, VAlert[CONF_CHANGE_ALERT].ACK);
else if(isserver == OTA_SRC_RS232) SendRS232Response(VAlert[CONF_CHANGE_ALERT].ACK);
else if(isserver == OTA_SRC_RS485) SendRS485Response(VAlert[CONF_CHANGE_ALERT].ACK);
else {
    AddAlert(CONF_CHANGE_ALERT); // Queue as server ACK packet
}
```

---

## 6. Time Synchronization & IST (+5:30) Offset

All CD1 timestamps are required to be in **Indian Standard Time (IST, UTC+5:30)**.

In `custom/Utilities.c` and `custom/GPRS.c`:

```c
void AdjustGPSTimeToIST(ST_Time *time) {
    time->hour += 5;
    time->minute += 30;
    
    if(time->minute >= 60) {
        time->minute -= 60;
        time->hour += 1;
    }
    if(time->hour >= 24) {
        time->hour -= 24;
        time->day += 1;
        NormalizeCalendarDate(time);
    }
}
```

---

## 7. Diagnostic Logging System & Hardware Safety Rules

### 7.1 Diagnostic Log Tags

All diagnostic logs use `LOGData(TAG, format, ...)`:
- `TAG_SERVER`: Server connection, packet parsing, and HTTP transmission steps.
- `TAG_HTTP`: Low-level AT command transactions for HTTP POST/GET.
- `TAG_MAIN`: System initialization and task thread setup.
- `TAG_SYSTIC`: Periodic watchdog, SIM profile switching, and timer ticks.
- `TAG_GPS`: NMEA parsing and overspeed geofence classification.

### 7.2 Critical Hardware & OS Safety Rules (M66 OpenCPU)

> [!CAUTION]
> **RULE 1: NO `%f` FORMAT SPECIFIERS IN TRACE LOGS (`LOGData`)**  
> On the Quectel M66 OpenCPU platform, invoking `LOGData` or trace printing functions with floating-point `%f` specifiers inside thread timer loops causes an immediate **hardware exception reset**. Always format floats into string buffers with `Ql_sprintf()` or convert to integer representations (`%d.%02d`) prior to tracing.

> [!WARNING]
> **RULE 2: NO LOGGING INSIDE `AddAlert()` OR `RemoveAlert()`**  
> `AddAlert()` and `RemoveAlert()` are called directly from high-frequency timer interrupts and interrupt service handlers. Emitting `LOGData()` inside these routines triggers trace crashes before execution can return to the caller.

> [!IMPORTANT]
> **RULE 3: ALERT INDEX BOUNDS PROTECTION (`IsValidAlertIndex`)**  
> Every entry point accessing `VAlert[]` MUST call `IsValidAlertIndex(index)`. An out-of-bounds array access will corrupt internal RAM structures and result in unpredictable hardware reboots.

> [!NOTE]
> **RULE 4: PLAIN HTTP TRANSPORT ENFORCEMENT**  
> `HTTP_PrepareEndpoint()` explicitly sets `HTTPCurrentSecure = 0`. CDAC regional compliance requires plain HTTP transport (`http://`).

---

## 8. Step-by-Step Porting Checklist

1. **Set Up Build Definitions:** Add `#define PROTO_CDAC` in your global feature configuration header.
2. **Configure Task Table:** Allocate a dedicated HTTP transport thread (`HTTPThreadEntry`) with at least **6 KB stack size**.
3. **Initialize Alert System:** Set `ALERT_COUNT = 19`, instantiate parallel tables (`VTAlertHeaderType`, `VTAlertPKT`, `VTContType`), and enforce bounds checks on `VAlert`.
4. **Implement Time Sync:** Wire `AdjustGPSTimeToIST()` into UTC network time and GPS time parsing routines.
5. **Build Telemetry Builders:** Implement fixed-offset ASCII payload construction for `LGN`, `NRM`, `HLM`, `FUL`, `BTH`, and alert packets (`EPB`, `CRT`, `ALT`, `ACK`).
6. **Deploy Dual Server Transmission & 3rd Server Engine:** Configure primary endpoint `DEFAULT_IP1` and 3rd server secondary endpoint `DEFAULT_IP3`. Update `SendDataToServer()` to attempt delivery on both servers. Support dynamic `SET SU` URL normalization and dual HTTP/TCP modes.
7. **Integrate OTA Engine:** Connect `DecodeOTAData()` to SMS, HTTP response, RS232, and RS485 input handlers. Ensure `SU` key updates `VTSData.ServerData.IP3`/`Port3`.
8. **Enforce Safety Rules:** Verify that all trace logs are free of `%f` specifiers and that alert routines remain trace-free.
