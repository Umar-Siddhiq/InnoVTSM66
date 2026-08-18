# AIS-140 OG Amendment 3 Protocol — Complete Implementation Reference

**Firmware:** PROTO_OG v1.5.8 | **Device:** Quectel M66 OpenCPU (ARM7TDMI) | **Date:** 2026-08-11

---

## Table of Contents

1. [Protocol Overview](#1-protocol-overview)
2. [Packet Frame Format & Checksum](#2-packet-frame-format--checksum)
3. [LGN — Login Packet](#3-lgn--login-packet)
4. [PVT — Position/Velocity/Time Packet](#4-pvt--positionvelocitytime-packet)
5. [EPB — Emergency Packet](#5-epb--emergency-packet)
6. [HLM — Health / Heartbeat Packet](#6-hlm--health--heartbeat-packet)
7. [OTA Command Frame — Inbound](#7-ota-command-frame--inbound)
8. [OTA Security Model](#8-ota-security-model)
9. [OTA Command Reference — All 23 Commands](#9-ota-command-reference--all-23-commands)
10. [OTA Response — Embedded in PVT](#10-ota-response--embedded-in-pvt)
11. [SMS Fallback — Emergency (EPB via SMS)](#11-sms-fallback--emergency-epb-via-sms)
12. [Alert Index Table](#12-alert-index-table)
13. [PVT Packet Type Codes](#13-pvt-packet-type-codes)
14. [OTA Source Channel Codes](#14-ota-source-channel-codes)
15. [IO / Digital Pin Encoding](#15-io--digital-pin-encoding)
16. [Full Raw Packet Examples](#16-full-raw-packet-examples)
17. [Key Firmware Functions](#17-key-firmware-functions)

---

## 1. Protocol Overview

AMD3 (Amendment 3 to AIS-140) is a GSM/GPRS-based vehicle tracking protocol. The device communicates over TCP with a primary server (PVT/health/emergency) and a secondary server (emergency mirror). The device also accepts OTA configuration commands inbound from the server or via SMS, RS232, RS485, or BLE.

**Transport:** TCP socket, ASCII text frames terminated `\r\n`  
**Checksum:** XOR of all bytes between `$` and `*` (exclusive)  
**Encoding:** All fields ASCII; floats formatted as fixed-width strings  

**Packet types sent by device:**

| Packet | Function | Trigger |
|--------|----------|---------|
| `$LGN` | Login / registration | On connect |
| `$PVT` | Normal / alert position report | Periodic or on event |
| `$EPB` | Emergency (SOS) | SOS button / tamper / alert |
| `$HLM` | Health heartbeat | Periodic |

---

## 2. Packet Frame Format & Checksum

### General Frame

```
$<HEADER>,<field1>,<field2>,...,<fieldN>*<XX>\r\n
```

- `$` — start delimiter (NOT included in checksum)
- `*XX` — two-hex-digit XOR checksum
- `\r\n` — CRLF terminator

### XOR Checksum Algorithm

```c
/* Computes XOR of bytes from buf[0] to buf[len-1] */
/* Called as: GetXORChecksum(dataBuffer + 1, strlen(dataBuffer) - 1) */
/* i.e., skips the leading '$' */

uint8_t GetXORChecksum(const char* buf, int len)
{
    uint8_t cs = 0;
    for (int i = 0; i < len; i++)
        cs ^= (uint8_t)buf[i];
    return cs;
}
```

**Usage in code:**
```c
checksum = GetXORChecksum(dataBuffer + 1, Ql_strlen(dataBuffer) - 1);
Ql_sprintf(ss, "*%02X\r\n", checksum);
Ql_strcat(dataBuffer, ss);
```

---

## 3. LGN — Login Packet

### Purpose
Sent once when TCP socket connects. Registers the device with the server.

### Format
```
$LGN,<VehicleRegNo>,<IMEI>,<SIMNo>,<FirmVer>,0100,<Latitude>,<LatDir>,<Longitude>,<LngDir>*<XX>\r\n
```

### Field Table

| # | Field | Format | Example | Source |
|---|-------|--------|---------|--------|
| 1 | VehicleRegNo | Up to 20 chars | `MH12AB1234` | `VTSData.VehicleData.VehicleRegNo` |
| 2 | IMEI | 15 digits | `123456789012345` | `NetWork.IMEI` |
| 3 | SIMNo | ICCID / phone no | `8991101200003204769` | `NetWork.SIMNo` |
| 4 | FirmVer | String | `1.5.8` | `FirmVer` constant |
| 5 | Literal | `0100` | `0100` | Hardcoded |
| 6 | Latitude | DDMM.MMMM | `1845.1234` | `sLatitude` |
| 7 | LatDir | `N` or `S` | `N` | `GPS.LatDir` (default `N`) |
| 8 | Longitude | DDDMM.MMMM | `07312.5678` | `sLongitude` |
| 9 | LngDir | `E` or `W` | `E` | `GPS.LngDir` (default `E`) |

### Raw Example
```
$LGN,MH12AB1234,123456789012345,8991101200003204769,1.5.8,0100,1845.1234,N,07312.5678,E*4F\r\n
```

### Code Reference — `LoginString()` in `Server.c:562`
```c
Ql_sprintf(dataBuffer,"$LGN,%s,%s,%s,%s,0100,%s,%c,%s,%c",
    VTSData.VehicleData.VehicleRegNo,
    NetWork.IMEI,
    NetWork.SIMNo,
    FirmVer,
    sLatitude,
    GPS.LatDir ? GPS.LatDir : 'N',
    sLongitude,
    GPS.LngDir ? GPS.LngDir : 'E'
);
```

---

## 4. PVT — Position/Velocity/Time Packet

### Purpose
Main periodic data packet. Also used for alert events and OTA acknowledgements.

### Format
```
$PVT,<VendorID>,<FirmVer>,<PktType>,<PktTypeNum>,L,<IMEI>,<VehicleRegNo>,<GPSFix>,<Date>,<Time>,<Latitude>,<LatDir>,<Longitude>,<LngDir>,<Speed>,<Heading>,<Satellites>,<Altitude>,<PDOP>,<HDOP>,<Network>,<IGN>,<MainsPower>,<MainsVolt>,<BattVolt>,<SOSFlag>,<CoverStatus>,<SignalStrength>,<MCC>,<MNC>,<LAC>,<CellID>,<NC1_RSSI>,<NC1_LAC>,<NC1_CID>,<NC2_RSSI>,<NC2_LAC>,<NC2_CID>,<NC3_RSSI>,<NC3_LAC>,<NC3_CID>,<NC4_RSSI>,<NC4_LAC>,<NC4_CID>,<DINs>,<DOUTs>,<FrameNo>,<AN1>,<AN2>,<DeltaDist>,<OTAResp>,<TrailerID>*<XX>\r\n
```

### Field Table

| # | Field | Format | Width | Example | Notes |
|---|-------|--------|-------|---------|-------|
| 1 | VendorID | String | var | `QUECKTEL` | `VTSData.VendorID` |
| 2 | FirmVer | String | var | `1.5.8` | `FirmVer` |
| 3 | PktType | 2-char code | 2 | `NR` | See §13 |
| 4 | PktTypeNum | Integer | var | `1` | See §13 |
| 5 | Live/Stored | `L` or `S` | 1 | `L` | Always `L` for live |
| 6 | IMEI | 15 digits | 15 | `123456789012345` | `NetWork.IMEI` |
| 7 | VehicleRegNo | Padded string | 5–12 | `MH12AB1234` | `AppendVariableString` min=5,max=12 |
| 8 | GPSFix | `0`/`1` | 1 | `1` | `GPS.GPSFix` |
| 9 | Date | DDMMYYYY | 8 | `11082026` | `InsertCurrentDateTime(buf,0)` |
| 10 | Time | HHMMSS | 6 | `143000` | `InsertCurrentDateTime(buf,1)` |
| 11 | Latitude | DDMM.MMMM | 10 | `1845.1234` | `AppendFixString` len=10 |
| 12 | LatDir | `N`/`S` | 1 | `N` | `GPS.LatDir` |
| 13 | Longitude | DDDMM.MMMM | 10 | `07312.5678` | `AppendFixString` len=10 |
| 14 | LngDir | `E`/`W` | 1 | `E` | `GPS.LngDir` |
| 15 | Speed | km/h | `%05.1f` | `060.5` | `GPS.Speed` |
| 16 | Heading | degrees | `%06.2f` | `180.00` | `GPS.Heading` |
| 17 | Satellites | count | `%02d` | `08` | `GPS.NoOfSatalite` |
| 18 | Altitude | metres | `%03.1f` | `123.4` | `GPS.Altitude` |
| 19 | PDOP | DOP | `%04.1f` | `01.2` | `GPS.PDOP` |
| 20 | HDOP | DOP | `%04.1f` | `00.9` | `GPS.HDOP` |
| 21 | Network | String | var | `AIRTEL` | `NetWork.Network` |
| 22 | IGN | `0`/`1` | 1 | `1` | `PeriPheralVal.IGN` |
| 23 | MainsPower | `0`/`1` | 1 | `1` | `PeriPheralVal.IsMain` |
| 24 | MainsVolt | Volts | `%04.1f` | `12.0` | `PeriPheralVal.MainsVolt` |
| 25 | BattVolt | Volts | `%03.1f` | `4.1` | `PeriPheralVal.BattVolt` |
| 26 | SOSFlag | `0`/`1` | 1 | `0` | SOS alert active |
| 27 | CoverStatus | `O`/`C` | 1 | `C` | Open/Closed tamper |
| 28 | SignalStrength | RSQ | `%2d` | `18` | `GSM.SignalStrength` |
| 29 | MCC | Mobile CC | `%02d` | `404` | `GSM.MCC` |
| 30 | MNC | Mobile NC | `%02d` | `20` | `GSM.MNC` |
| 31 | LAC | Location Area | 4–5 hex | `00D6` | `GSM.LAC` |
| 32 | CellID | Cell ID | 4–5 hex | `CFBD` | `GSM.CellID` |
| 33–44 | NC1..NC4 | Neighbor cells | — | `15,00D6,CFBD` | 4 × (RSSI, LAC, CellID) |
| 45 | DINs | 4-bit string | 4 | `1010` | IP1,IP2,IGN,SOS |
| 46 | DOUTs | 2-bit string | 2 | `00` | OP1,OP2 |
| 47 | FrameNo | Count | `%06d` | `000042` | `FrameNumber` (auto-inc) |
| 48 | AN1 | Analog 1 | float | `0.00` | Hardcoded `0.00` |
| 49 | AN2 | Analog 2 | float | `0.00` | Hardcoded `0.00` |
| 50 | DeltaDist | Distance m | `%05.1f` | `00123.4` | `DeltaDis` since last packet |
| 51 | OTAResp | OTA response | `(...)` or `()` | `(SCK1\|SET\|002:192.168.1.1:1)` | See §10 |
| 52 | TrailerID | RFID tag | `TAG<id>` or `()` | `TAG1234ABCD` | From sensor data |

### DINs Field Detail
```
Bit3 Bit2 Bit1 Bit0
 IP1  IP2  IGN  SOS
```
- `IP1` = Digital Input 1
- `IP2` = Digital Input 2  
- `IGN` = Ignition input
- `SOS` = SOS button (INPUT_SOS_VAL)

### DOUTs Field Detail
```
Bit1 Bit0
 OP1  OP2
```

### Raw Example — Normal Packet (no alerts, no OTA pending)
```
$PVT,QUICKTEL,1.5.8,NR,1,L,123456789012345,MH12AB1234,1,11082026,143000,1845.1234,N,07312.5678,E,060.5,180.00,08,123.4,01.2,00.9,AIRTEL,1,1,12.0,4.1,0,C,18,404,20,00D6,CFBD,15,00D6,CFBD,14,00D5,CFBC,12,00D4,CFBB,10,00D3,CFBA,1010,00,000042,0.00,0.00,00123.4,(),()
*XX\r\n
```

### Raw Example — Ignition ON Alert
```
$PVT,QUICKTEL,1.5.8,IN,7,L,123456789012345,MH12AB1234,1,11082026,143005,1845.1234,N,07312.5678,E,000.0,180.00,08,123.4,01.2,00.9,AIRTEL,1,1,12.0,4.1,0,C,18,404,20,00D6,CFBD,15,00D6,CFBD,14,00D5,CFBC,12,00D4,CFBB,10,00D3,CFBA,1010,00,000043,0.00,0.00,00000.0,(),()
*XX\r\n
```

### Raw Example — OTA Response Embedded
```
$PVT,QUICKTEL,1.5.8,OA,12,L,123456789012345,MH12AB1234,1,11082026,143010,...,00000.0,(SCK1|SET|002:192.168.1.100:1),()
*XX\r\n
```

---

## 5. EPB — Emergency Packet

### Purpose
Sent on SOS activation (`$EPB,EMR`) or SOS clear (`$EPB,SEM`). Also sent to secondary emergency server.

### Format
```
$EPB,<Type>,<IMEI>,<StoredFlag>,<Date><Time>,<GPSValidity>,<Latitude>,<LatDir>,<Longitude>,<LngDir>,<Altitude>,<Speed>,<DeltaDist>,G,<VehicleRegNo>,<EmergencyPhone>*<XX>\r\n
```

### Field Table

| # | Field | Format | Example | Notes |
|---|-------|--------|---------|-------|
| 1 | Type | `EMR`/`SEM` | `EMR` | EMR=SOS ON, SEM=SOS Clear |
| 2 | IMEI | 15 digits | `123456789012345` | `NetWork.IMEI` |
| 3 | StoredFlag | `SP`/`NM` | `NM` | SP=stored, NM=normal/live |
| 4 | DateTime | DDMMYYYYHHMMSS | `11082026143000` | No separator between date and time |
| 5 | GPSValidity | `A`/`V` | `A` | A=valid, V=invalid |
| 6 | Latitude | DDMM.MMMM | `1845.1234` | `AppendFixString` |
| 7 | LatDir | `N`/`S` | `N` | `GPS.LatDir` |
| 8 | Longitude | DDDMM.MMMM | `07312.5678` | `AppendFixString` |
| 9 | LngDir | `E`/`W` | `E` | `GPS.LngDir` |
| 10 | Altitude | metres | `%05.1f` → `00123.4` | `GPS.Altitude` |
| 11 | Speed | km/h | `%05.1f` → `060.5` | `GPS.Speed` |
| 12 | DeltaDist | metres | `%05.1f` → `00045.0` | `DeltaDis` |
| 13 | Literal | `G` | `G` | Hardcoded device type |
| 14 | VehicleRegNo | String | `MH12AB1234` | `VTSData.VehicleData.VehicleRegNo` |
| 15 | EmergencyPhone | 9–14 digits | `9876543210` | `VTSData.PhoneNumber.Mob1` |

### Raw Example — SOS ON
```
$EPB,EMR,123456789012345,NM,11082026143000,A,1845.1234,N,07312.5678,E,00123.4,060.5,00045.0,G,MH12AB1234,9876543210*XX\r\n
```

### Raw Example — SOS Clear (via OTA command 20)
```
$EPB,SEM,123456789012345,NM,11082026143500,A,1845.1234,N,07312.5678,E,00123.4,000.0,00000.0,G,MH12AB1234,9876543210*XX\r\n
```

### Code Reference — `EmergencyPacket()` in `Server.c:1961`
```c
if(IsOff)
    Ql_strcat(dataBuffer,"$EPB,EMR,");
else
    Ql_strcat(dataBuffer,"$EPB,SEM,");
```
Note: `IsOff=1` → EMR (SOS active), `IsOff=0` → SEM (SOS clear).

---

## 6. HLM — Health / Heartbeat Packet

### Purpose
Periodic health status. Reports battery, memory, intervals, IO state.

### Format
```
$HLM,<VendorID>,<FirmVer>,<IMEI>,<BattPercent>,<LowBatThreshold>,<MemoryPercent>,<IgnitionInterval>,<DataInterval>,<IOStatus>,<AN1>,<AN2>*<XX>\r\n
```

### Field Table

| # | Field | Format | Example | Notes |
|---|-------|--------|---------|-------|
| 1 | VendorID | String | `QUICKTEL` | `VTSData.VendorID` |
| 2 | FirmVer | String | `1.5.8` | `FirmVer` |
| 3 | IMEI | 15 digits | `123456789012345` | `NetWork.IMEI` |
| 4 | BattPercent | `%03d` | `085` | `PeriPheralVal.BattPerc` |
| 5 | LowBatThreshold | `%03d` | `020` | `LOW_BAT_THRS_PER` |
| 6 | MemoryPercent | `%04.1f` | `12.5` | Flash used % |
| 7 | IgnitionInterval | seconds | `30` | `VTSData.IntervalData.IgnitionInterval` |
| 8 | DataInterval | seconds | `60` | `VTSData.IntervalData.DataInterval` |
| 9 | IOStatus | 8-char string | `10100000` | IP1,IP2,IGN,SOS,OP1,OP2,0,0 |
| 10 | AN1 | Voltage | `0.00` | Hardcoded |
| 11 | AN2 | Voltage | `0.00` | Hardcoded |

### IOStatus Field Breakdown
```
Pos: 0  1  2  3  4  5  6  7
     IP1 IP2 IGN SOS OP1 OP2  0   0
```

### Raw Example
```
$HLM,QUICKTEL,1.5.8,123456789012345,085,020,12.5,30,60,10100000,0.00,0.00*XX\r\n
```

### Code Reference — `HealthPacket()` in `Server.c:2314`
```c
Ql_sprintf(dataBuffer, "$HLM,%s,%s,%s,%03d,%03d,", 
    VTSData.VendorID, FirmVer, NetWork.IMEI,
    PeriPheralVal.BattPerc, LOW_BAT_THRS_PER);
InsertFloatValue(dataBuffer, memPer, "%04.1f");
InsertChar(dataBuffer, ',');
InsertIntValue(dataBuffer, VTSData.IntervalData.IgnitionInterval, "%d");
InsertChar(dataBuffer, ',');
InsertIntValue(dataBuffer, VTSData.IntervalData.DataInterval, "%d");
```

---

## 7. OTA Command Frame — Inbound

### Purpose
Configuration commands sent from server to device (or via SMS/RS232/RS485/BLE).

### Frame Format
```
$<IMEI>,<PASSWORD>,<MODE>,<CMD_ID>:<VALUE>,...*<XX>\r\n
```

### Multi-Command Frame (same mode)
```
$<IMEI>,<PASSWORD>,<MODE>,<CMD_ID1>:<VALUE1>,<CMD_ID2>:<VALUE2>,...*<XX>\r\n
```

### Field Definitions

| Field | Description | Format | Example |
|-------|-------------|--------|---------|
| IMEI | Target device IMEI | 15 digits | `123456789012345` |
| PASSWORD | Auth token | Last 6 digits of IMEI | `012345` |
| MODE | Operation type | `SET`, `GET`, `CLR` | `SET` |
| CMD_ID | Command number | 3-digit zero-padded | `002` |
| VALUE | Command value (SET only) | String | `192.168.1.100` |

### Mode Codes

| Mode String | Type Int | Meaning |
|-------------|----------|---------|
| `GET` | 1 | Read current value |
| `SET` | 2 | Write new value |
| `CLR` | 3 | Reset to default |

### Raw Examples

**GET Firmware Version:**
```
$123456789012345,012345,GET,001*XX\r\n
```

**SET PVT Server IP:**
```
$123456789012345,012345,SET,002:192.168.1.100*XX\r\n
```

**SET Multiple Commands (PVT IP + Port):**
```
$123456789012345,012345,SET,002:192.168.1.100,003:5000*XX\r\n
```

**CLR (Reset) APN:**
```
$123456789012345,012345,CLR,007*XX\r\n
```

**Restart Device:**
```
$123456789012345,012345,SET,018:1*XX\r\n
```

**GET IMEI:**
```
$123456789012345,012345,GET,019*XX\r\n
```

**CLR Emergency (SOS clear):**
```
$123456789012345,012345,CLR,020*XX\r\n
```

---

## 8. OTA Security Model

### Authentication Layers (from `SMS.c:ParseStandardAIS140Command`)

#### Layer 1 — IMEI Validation (HARD REJECT)
```c
if (Ql_strcmp(cmd_imei, NetWork.IMEI) != 0)
{
    LOGData(TAG_OTA, "AMD3 IMEI MISMATCH: got=%s expected=%s", cmd_imei, NetWork.IMEI);
    return; // HARD REJECT — no response sent
}
```

#### Layer 2 — Password Validation (HARD REJECT)
Password must equal the last 6 digits of the device IMEI.
```c
char expected_pwd[7];
int imei_len = Ql_strlen(NetWork.IMEI);
Ql_strncpy(expected_pwd, NetWork.IMEI + imei_len - 6, 6);
expected_pwd[6] = '\0';

if (Ql_strcmp(cmd_pwd, expected_pwd) != 0)
{
    LOGData(TAG_OTA, "AMD3 PWD MISMATCH");
    return; // HARD REJECT
}
```

#### Layer 3 — XOR Checksum (SOFT WARNING ONLY)
```c
uint8_t computed = GetXORChecksum(raw + 1, star_pos - raw - 1);
uint8_t received = (uint8_t)Ql_strtol(star_pos + 1, NULL, 16);
if (computed != received)
{
    LOGData(TAG_OTA, "AMD3 checksum WARN: computed=%02X received=%02X (continuing)", 
            computed, received);
    // NOT a hard reject — command still executes
}
```

**Important:** Checksum mismatch is a warning log only. Command processing continues regardless. Only IMEI and password mismatches cause hard rejection with no response.

### Security Summary

| Check | Failure Action | Response Sent |
|-------|---------------|---------------|
| IMEI mismatch | Hard reject, return | No |
| Password mismatch | Hard reject, return | No |
| Checksum mismatch | Log warning, continue | Yes (normal response) |

---

## 9. OTA Command Reference — All 23 Commands

### Command Table

| CMD | Name | GET | SET | CLR | Value Type | Range / Notes |
|-----|------|-----|-----|-----|------------|---------------|
| 001 | Firmware Version | Returns `FirmVer` | — | — | String | Read-only |
| 002 | PVT Server IP | Returns `IP1` | Sets `IP1`, calls `InitSockets()` | Resets to `DEFAULT_IP1` | String | 5–49 chars |
| 003 | PVT Server Port | Returns `Port1` | Sets `Port1`, calls `InitSockets()` | Resets to `DEFAULT_PORT1` | String | 2–9 chars |
| 004 | Emergency Server IP | Returns `IP2` | Sets `IP2`, calls `InitSockets()` | Resets to `DEFAULT_IP2` | String | 5–49 chars |
| 005 | Emergency Server Port | Returns `Port2` | Sets `Port2`, calls `InitSockets()` | Resets to `DEFAULT_PORT2` | String | 2–9 chars |
| 006 | Control Centre Number | Returns `Mob0` | Sets `Mob0` | Resets to `DEFAULT_MOB0` | Phone | 6–14 chars |
| 007 | APN | Returns `mAPN` | Sets `mAPN`, disables AutoAPN, resets | Enables AutoAPN, resets | String | 3–29 chars |
| 008 | Sleep/Data Interval | Returns `DataInterval` | Sets `DataInterval` | Resets to `DEFAULT_INV_STB` | Integer (s) | 5–3600 |
| 009 | Overspeed Limit | Returns `OverSpeed` | Sets `OverSpeed` | Resets to `DEFAULT_OVERSPEED` | Integer (km/h) | 11–199 |
| 010 | Harsh Acceleration Threshold | Returns `HarshAcc` | Sets `HarshAcc` | Resets to `DEFAULT_HA` | Integer (mg) | Any |
| 011 | Harsh Braking Threshold | Returns `HarshBreak` | Sets `HarshBreak` | Resets to `DEFAULT_HB` | Integer (mg) | Any |
| 012 | Harsh Cornering Threshold | Returns `RashTurn` | Sets `RashTurn` | Resets to `DEFAULT_RT` | Integer (mg) | Any |
| 013 | Vehicle Registration Number | Returns `VehicleRegNo` | Sets `VehicleRegNo` | Resets to `DEFAULT_VEHREG` | String | 3–19 chars |
| 014 | Ignition On Interval | Returns `IgnitionInterval` | Sets `IgnitionInterval` | Resets to `DEFAULT_INV_IGN` | Integer (s) | 5–3600 |
| 015 | Ignition Off Interval | Returns `DataInterval` | Sets `DataInterval` | Resets to `DEFAULT_INV_STB` | Integer (s) | 5–3600 |
| 016 | Emergency Interval | Returns `SOSInterval` | Sets `SOSInterval` | Resets to `DEFAULT_INV_SOS` | Integer (s) | 1–300 |
| 017 | Emergency Auto-Clear Timeout | Returns `SOSTimeOut` | Sets `SOSTimeOut` | Resets to `0` | Integer (s) | 0–3600 |
| 018 | Device Restart | — | Sends OTA ACK, waits 1s, resets | — | Ignored | SET only |
| 019 | IMEI | Returns `NetWork.IMEI` | — | — | String | GET only, read-only |
| 020 | Clear Emergency (SOS) | — | — | Calls `EmergencyPacket(0)` + `ResetSOS()` | — | CLR only |
| 021 | Emergency SMS Centre Number | Returns `Mob1` | Sets `Mob1` | Resets to `DEFAULT_MOB1` | Phone | 7–13 chars |
| 022 | Cellular Signal Strength | Returns `GSM.SignalStrength` | — | — | Integer (RSQ) | GET only |
| 023 | RFID Tag Value | Returns active RFID sensor data | — | — | String | GET only |

### Detailed Command Notes

**CMD 007 — APN:**  
SET disables AutoAPN (`VTSData.AutoAPN=0`) and triggers full system reset via `SystemRecovery_RequestReset("SMS_SET_APN")`. CLR re-enables AutoAPN and also resets the device.

**CMD 018 — Restart:**  
Sends the OTA ACK packet first (via `InitBuffer(12)` and TCP), waits 1000ms, then calls `SystemRecovery_RequestReset("SMS_RESTART")`. Only `SET` type is handled; GET and CLR are ignored.

**CMD 020 — Clear Emergency:**  
Only `CLR` type is handled. Calls `EmergencyPacket(0)` which sends `$EPB,SEM,...` (SOS clear packet) then calls `ResetSOS()` to clear the SOS state in firmware.

**CMD 023 — RFID:**  
Scans `SensorData[]` array for `SENSOR_TYPE_RFID` with `IsActive=1`. Returns the first found. Returns `NA` if no active RFID sensor.

---

## 10. OTA Response — Embedded in PVT

When an OTA command is processed, the response is embedded in the next `$PVT` packet in the `OTAResp` field.

### Response Format
```
(<Source>|<Mode>|<CmdId>:<Value>:<Status>)
```

### Field Definitions

| Field | Description | Example |
|-------|-------------|---------|
| Source | Source channel code | `SCK1`, `SCK2`, `SMS` |
| Mode | `GET`, `SET`, or `CLR` | `SET` |
| CmdId | 3-digit command number | `002` |
| Value | Returned or set value | `192.168.1.100` |
| Status | `1`=success, `0`=failure | `1` |

### No Pending Response
```
()
```

### Response Examples

```
(SCK1|GET|001:1.5.8:1)        ← GET firmware version → success
(SMS|SET|002:192.168.1.1:1)   ← SET PVT IP via SMS → success
(SCK2|CLR|007::1)             ← CLR APN via socket 2 → success
(SCK1|SET|009:250:0)          ← SET overspeed 250 → fail (out of range)
(SCK1|GET|019:123456789012345:1) ← GET IMEI → success
```

### Source Channel Strings

| OTA_SRC value | String in Response | Physical channel |
|---------------|--------------------|-----------------|
| 0 | `SMS` | SMS |
| 1 | `SCK1` | TCP Socket 1 (PVT server) |
| 2 | `SCK2` | TCP Socket 2 (emergency server) |
| 3 | `SCK3` | TCP Socket 3 |
| 4 | `SCK4` | TCP Socket 4 |
| 5 | `BLE` | Bluetooth Low Energy |
| 6 | `RS232` | Serial RS232 |
| 7 | `RS485` | RS485 bus |

### OTAResponseTypeDef (from `SMS.h`)
```c
typedef struct {
    char Source[64];   // Channel string: "SCK1", "SMS", etc.
    char Mode[8];      // "SET", "GET", "CLR"
    char CmdId[8];     // "001", "002", etc.
    char Value[64];    // Response value (read value or set value)
    uint8_t Status;    // 1=success, 0=failure
    uint8_t Pending;   // 1=response waiting to send, cleared after PVT transmit
} OTAResponseTypeDef;
```

### Code Reference — `InitBuffer()` in `Server.c:1478`
```c
if (LastOTAResponse.Pending)
{
    char ota_buf[256];
    Ql_sprintf(ota_buf, "(%s|%s|%s:%s:%d)", 
        LastOTAResponse.Source,
        LastOTAResponse.Mode,
        LastOTAResponse.CmdId,
        LastOTAResponse.Value,
        LastOTAResponse.Status);
    Ql_strcat(dataBuffer, ota_buf);
    LastOTAResponse.Pending = 0;
}
else
{
    Ql_strcat(dataBuffer, "()");
}
```

---

## 11. SMS Fallback — Emergency (EPB via SMS)

When the TCP socket is not connected during an SOS event, the device sends an emergency SMS to the control centre number (`Mob0`).

### SMS Format (AMD3)
```
$EPB,EMR,<IMEI>,<Latitude>,<LatDir>,<Longitude>,<LngDir>,<GPSValidity>,<Speed>,<CellID>,<LAC>,<Date><Time>*<XX>\r\n
```

**Note:** The SMS EPB format differs from the TCP EPB format — it omits altitude, delta distance, vehicle reg no, and emergency phone number. It includes CellID and LAC instead.

### Field Table

| # | Field | Format | Example |
|---|-------|--------|---------|
| 1 | Type | `EMR` | `EMR` |
| 2 | IMEI | 15 digits | `123456789012345` |
| 3 | Latitude | DDMM.MMMM | `1845.1234` |
| 4 | LatDir | `N`/`S` | `N` |
| 5 | Longitude | DDDMM.MMMM | `07312.5678` |
| 6 | LngDir | `E`/`W` | `E` |
| 7 | GPSValidity | `A`/`V` | `A` |
| 8 | Speed | `%05.1f` | `060.5` |
| 9 | CellID | 4–5 hex | `CFBD` |
| 10 | LAC | 4–5 hex | `00D6` |
| 11 | DateTime | DDMMYYYYHHMMSS | `11082026143000` |

### Raw SMS Example
```
$EPB,EMR,123456789012345,1845.1234,N,07312.5678,E,A,060.5,CFBD,00D6,11082026143000*XX\r\n
```

### Code Reference — `SendSOSSMS()` in `SMS.c:74`
```c
Ql_sprintf(SimData, "$EPB,EMR,");
Ql_strncat(SimData, NetWork.IMEI, 15);
/* latitude, latdir, longitude, lngdir, gpsvalidity, speed, cellid, lac, datetime */
StringAdd(SimData, "%05.1f", GPS.Speed);  // Note: uses StringAdd (512-byte stack)
```

> **Known issue:** `SendSOSSMS()` in `SMS.c:95` still uses `StringAdd` for the Speed field. This allocates 512 bytes on the stack inside the SMS task context. While the SMS task has a larger stack than the server task, this should eventually be migrated to `InsertFloatValue` for consistency.

---

## 12. Alert Index Table

**PROTO_OG: `ALERT_COUNT = 21`**

| Index | Define | Description | PVT PktType |
|-------|--------|-------------|-------------|
| 0 | `SOS_ON_ALERT` | SOS button pressed | `EA,10` |
| 1 | `SOS_OFF_ALERT` | SOS button released | `EA,11` |
| 2 | `SOS_TMP_ALERT` | SOS tamper alert | `DT,16` |
| 3 | `MAINS_FAIL_ALERT` | External power lost | `BD,3` |
| 4 | `TILT_ALERT` | Vehicle tilt detected | `TL,24` |
| 5 | `TAMPER_ALERT` | Box tamper | `TA,9` |
| 6 | `OVER_SPEED_ALERT` | Overspeed | `OS,17` |
| 7 | `HARSH_BRK_ALERT` | Harsh braking | `HB,13` |
| 8 | `HARSH_ACC_ALERT` | Harsh acceleration | `HA,14` |
| 9 | `RASH_TURN_ALERT` | Rash/harsh cornering | `RT,15` |
| 10 | `IMPACT_ALERT` | Impact/collision | — |
| 11 | `GFIN_OS_ALERT` | Geofence in (overspeed) | — |
| 12 | `GFOUT_OS_ALERT` | Geofence out (overspeed) | — |
| 13 | `GFIN_ALERT` | Geofence in | `GI,18` |
| 14 | `GFOUT_ALERT` | Geofence out | `GO,19` |
| 15 | `CONF_CHANGE_ALERT` | Config change OTA | `OA,12` |
| 16 | `MAINS_RES_ALERT` | External power restored | `BR,6` |
| 17 | `BATT_LOW_ALERT` | Battery low | `BL,4` |
| 18 | `BATT_LOW_RES_ALERT` | Battery low restored | `BC,5` |
| 19 | `IGN_ON_ALERT` | Ignition ON | `IN,7` |
| 20 | `IGN_OFF_ALERT` | Ignition OFF | `IF,8` |

---

## 13. PVT Packet Type Codes

These are encoded in fields 3 and 4 of the `$PVT` packet: `<TypeCode>,<TypeNum>,L,`

| `alt` arg | Code | Num | Description |
|-----------|------|-----|-------------|
| 1 | `NR` | 1 | Normal periodic packet |
| 3 | `BD` | 3 | Mains fail (battery disconnect) |
| 4 | `BL` | 4 | Battery low |
| 5 | `BC` | 5 | Battery low restored |
| 6 | `BR` | 6 | Mains restored |
| 7 | `IN` | 7 | Ignition ON |
| 8 | `IF` | 8 | Ignition OFF |
| 9 | `TA` | 9 | Box tamper |
| 10 | `EA` | 10 | SOS ON (emergency) |
| 11 | `EA` | 11 | SOS OFF |
| 12 | `OA` | 12 | OTA command ACK |
| 13 | `HB` | 13 | Harsh braking |
| 14 | `HA` | 14 | Harsh acceleration |
| 15 | `RT` | 15 | Rash/harsh cornering |
| 16 | `DT` | 16 | SOS tamper |
| 17 | `GI` | 18 | Geofence IN |
| 18 | `GO` | 19 | Geofence OUT |
| 23 | `OS` | 17 | Overspeed |
| 24 | `TL` | 24 | Vehicle tilt |
| default | `NR` | 1 | Normal (fallback) |

---

## 14. OTA Source Channel Codes

Defined in `custom/SMS.h`:

```c
#define OTA_SRC_SMS     0
#define OTA_SRC_SCK_1   1
#define OTA_SRC_SCK_2   2
#define OTA_SRC_SCK_3   3
#define OTA_SRC_SCK_4   4
#define OTA_SRC_BLE     5
#define OTA_SRC_RS232   6
#define OTA_SRC_RS485   7
```

---

## 15. IO / Digital Pin Encoding

### DINs (4-bit string in PVT field 45)
```c
char dins[10];
Ql_sprintf(dins, "%d%d%d%d", 
    PeriPheralVal.IP1,          // bit 3 (leftmost)
    PeriPheralVal.IP2,          // bit 2
    PeriPheralVal.IGN,          // bit 1
    INPUT_SOS_VAL ? 1 : 0);    // bit 0 (rightmost)
```

| Char position | Signal | Description |
|---------------|--------|-------------|
| 0 | IP1 | Digital Input 1 |
| 1 | IP2 | Digital Input 2 |
| 2 | IGN | Ignition input |
| 3 | SOS | SOS button |

### DOUTs (2-bit string in PVT field 46)
```c
char douts[10];
Ql_sprintf(douts, "%d%d", 
    PeriPheralVal.OP1,   // bit 1 (leftmost)
    PeriPheralVal.OP2);  // bit 0
```

### IOStatus in HLM (8-char string)
```c
Ql_sprintf(iostatus, "%d%d%d%d%d%d00", 
    PeriPheralVal.IP1,
    PeriPheralVal.IP2,
    PeriPheralVal.IGN,
    INPUT_SOS_VAL ? 1 : 0,
    PeriPheralVal.OP1,
    PeriPheralVal.OP2);
// Last two chars always '0','0'
```

---

## 16. Full Raw Packet Examples

### LGN — Login
```
$LGN,MH12AB1234,123456789012345,8991101200003204769,1.5.8,0100,1845.1234,N,07312.5678,E*4F
```

### PVT — Normal Packet (IGN ON, GPS valid, connected)
```
$PVT,QUICKTEL,1.5.8,NR,1,L,123456789012345,MH12AB1234,1,11082026,143000,1845.1234,N,07312.5678,E,060.5,180.00,08,0123.4,01.2,00.9,AIRTEL,1,1,12.0,4.1,0,C,18,404,20,00D6,CFBD,15,00D6,CFBD,14,00D5,CFBC,12,00D4,CFBB,10,00D3,CFBA,1010,00,000042,0.00,0.00,00123.4,(),()
*XX
```

### PVT — Ignition ON Alert
```
$PVT,QUICKTEL,1.5.8,IN,7,L,123456789012345,MH12AB1234,1,11082026,143005,1845.1234,N,07312.5678,E,000.0,000.00,06,0050.0,02.1,01.5,AIRTEL,1,1,12.1,4.1,0,C,18,404,20,00D6,CFBD,15,00D6,CFBD,14,00D5,CFBC,12,00D4,CFBB,10,00D3,CFBA,1010,00,000043,0.00,0.00,00000.0,(),()
*XX
```

### PVT — OTA Response (SET IP success)
```
$PVT,QUICKTEL,1.5.8,OA,12,L,123456789012345,MH12AB1234,1,11082026,143010,1845.1234,N,07312.5678,E,000.0,000.00,06,0050.0,02.1,01.5,AIRTEL,1,1,12.1,4.1,0,C,18,404,20,00D6,CFBD,15,00D6,CFBD,14,00D5,CFBC,12,00D4,CFBB,10,00D3,CFBA,1010,00,000044,0.00,0.00,00000.0,(SCK1|SET|002:192.168.1.100:1),()
*XX
```

### EPB — SOS Active (TCP)
```
$EPB,EMR,123456789012345,NM,11082026143015,A,1845.1234,N,07312.5678,E,00123.4,060.5,00045.0,G,MH12AB1234,9876543210*XX
```

### EPB — SOS Clear (TCP, via OTA CLR 020)
```
$EPB,SEM,123456789012345,NM,11082026143500,A,1845.1234,N,07312.5678,E,00123.4,000.0,00000.0,G,MH12AB1234,9876543210*XX
```

### EPB — SOS via SMS fallback
```
$EPB,EMR,123456789012345,1845.1234,N,07312.5678,E,A,060.5,CFBD,00D6,11082026143000*XX
```

### HLM — Health Packet
```
$HLM,QUICKTEL,1.5.8,123456789012345,085,020,12.5,30,60,10100000,0.00,0.00*XX
```

### OTA Inbound — GET Firmware Version
```
$123456789012345,012345,GET,001*XX
```

### OTA Inbound — SET PVT Server IP
```
$123456789012345,012345,SET,002:203.0.113.10*XX
```

### OTA Inbound — SET Data Interval to 30s
```
$123456789012345,012345,SET,008:30*XX
```

### OTA Inbound — Device Restart
```
$123456789012345,012345,SET,018:1*XX
```

### OTA Inbound — CLR SOS (via socket)
```
$123456789012345,012345,CLR,020*XX
```

---

## 17. Key Firmware Functions

| Function | File | Purpose |
|----------|------|---------|
| `LoginString()` | `Server.c:562` | Build LGN packet into `dataBuffer` |
| `InitBuffer(alt)` | `Server.c:1298` | Build PVT packet; `alt` selects type code |
| `EmergencyPacket(IsOff)` | `Server.c:1961` | Build EPB packet; 1=EMR, 0=SEM |
| `HealthPacket()` | `Server.c:2314` | Build HLM packet |
| `ParseStandardAIS140Command()` | `SMS.c` | Parse and execute inbound OTA frame |
| `SendSOSSMS(isFall)` | `SMS.c:74` | Build and send EPB via SMS |
| `GetXORChecksum(buf, len)` | `SMS.c` | XOR checksum over buffer |
| `InsertFloatValue(buf, val, fmt)` | `Utilities.c` | Append formatted float (20-byte stack safe) |
| `InsertIntValue(buf, val, fmt)` | `Utilities.c` | Append formatted integer |
| `InsertChar(buf, ch)` | `Utilities.c` | Append single character |
| `InsertCurrentDateTime(buf, IsTime)` | `Utilities.c` | Append date (0) or time (1) |
| `AppendFixString(buf, src, len, def)` | `Utilities.c` | Append fixed-length padded string |
| `AppendVariableString(buf, src, max, min, def)` | `Utilities.c` | Append variable-length bounded string |
| `UpdateConfigInFlash()` | `Hardware.c` | Persist `VTSData` to flash |
| `InitSockets()` | `TCP.c` | Re-initialize all TCP sockets |
| `SystemRecovery_RequestReset(reason)` | `SystemRecovery.c` | Graceful firmware reset |
| `ResetSOS()` | `SOS.c` | Clear SOS state after CLR command |

---

*Document generated from source code: `custom/Server.c`, `custom/SMS.c`, `custom/SMS.h`, `custom/inc/VTS.h`, `custom/inc/Alert.h`, `custom/config/custom_proto_cfg.h`. All packet formats reconstructed from actual code paths — not inferred from specification alone.*
