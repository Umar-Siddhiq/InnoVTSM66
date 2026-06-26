# InnoVTS M66 Firmware Modifications & System Documentation

This document compiles all firmware modifications, feature implementations, bug fixes, and verification plans applied to the InnoVTS M66 OpenCPU codebase.

---

## Table of Contents
1. [Mobile Number Formatting & SMS/GPRS Parser Improvements](#1-mobile-number-formatting--smsgprs-parser-improvements)
2. [Centralized LED Manager Implementation](#2-centralized-led-manager-implementation)
3. [HTTP Queue IsSendProcess Memory Corruption Fixes](#3-http-queue-issendprocess-memory-corruption-fixes)
4. [GPS Fault MCU Reset Logic](#4-gps-fault-mcu-reset-logic)
5. [ACTVR Format, VDETAIL Version Prefix, and FOTA/MOTA Watchdog Reset Protection](#5-actvr-format-vdetail-version-prefix-and-fotamota-watchdog-reset-protection)
6. [GPS Simulation Modes (CGPS and FGPS)](#6-gps-simulation-modes-cgps-and-fgps)
7. [GPS Heading Precision Tuning](#7-gps-heading-precision-tuning)
8. [Dynamic Custom/Virtual IMEI Configuration](#8-dynamic-customvirtual-imei-configuration)

---

## 1. Mobile Number Formatting & SMS/GPRS Parser Improvements

### Root Cause Identified
Your logs and tests showed that mobile numbers set without the `+91` prefix (e.g. `8489607555`) would fail to accept, update, or route SMS notifications. This was caused by:
1. **Strict SMSC / Modem Requirements**: IoT/M2M SIM cards route SMS traffic through Short Message Service Centers (SMSC) that require international dialing formatting (e.g. `+91XXXXXXXXXX` for India). Passing a 10-digit number directly to the modem AT command line fails to route/send.
2. **`SETSOSSET` Comma Parser Bug**: The original firmware code parsed `SETSOSSET` using `GetValueFromData` with strict comma delimiters (expecting `SETSOSSET <num1>,<num2>`). If only a single number without a comma was sent (e.g. `SETSOSSET 8489607555`), parsing returned `0` (failed), discarding the request entirely.

### Fixes Applied

#### Fix #1.1: Handle 10-digit Phone Numbers inside `UpdateMoblieNo`
* **File**: [custom/Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c) (line 3108)
* **Changes**: Checks if the parsed value is exactly 10 digits and automatically prepends `+91` before storing it in flash memory (`Mob0`–`Mob4`).
```c
void UpdateMoblieNo(char* value, uint8_t num)
{
	if (!value) return;
	char formattedValue[25];
	Ql_memset(formattedValue, 0, sizeof(formattedValue));

	if (Ql_strlen(value) == 10 && value[0] >= '0' && value[0] <= '9')
	{
		Ql_sprintf(formattedValue, "+91%s", value);
	}
	else
	{
		Ql_strncpy(formattedValue, value, sizeof(formattedValue) - 1);
		formattedValue[sizeof(formattedValue) - 1] = '\0';
	}
	
	uint16_t j=Ql_strlen(formattedValue);
	if((j >3) && (j < 21))
...
```
* **Impact**: Ensures all GPRS command phone number settings (such as `M0`–`M4`) automatically format 10-digit numbers into standard international format (+91).

#### Fix #1.2: Auto-prepend `+91` and Support Single-Number `SETSOSSET`
* **File**: [custom/SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c) (line 2577)
* **Changes**: Splits logic based on comma presence. Automatically handles single-number inputs (e.g. `SETSOSSET 8489607555`), formats the numbers by prepending `+91` if they are 10-digit numbers, and writes them to flash config.
```c
		ls=Ql_strstr(fn,"SOSSET");
		if(ls)
		{
			char* comma_ptr = Ql_strchr(ls, ',');
			if (comma_ptr)
			{
				i=GetValueFromData(ls,"SOSSET",' ',0,',',ss);
				if(i)
				{
					char formatted[25];
					if (Ql_strlen(ss) == 10 && ss[0] >= '0' && ss[0] <= '9')
						Ql_sprintf(formatted, "+91%s", ss);
					else
						Ql_strcpy(formatted, ss);
					Ql_strcpy(VTSData.PhoneNumber.Mob0,formatted);
				}
				i=GetValueFromData(ls,"SOSSET",',',1,'\0',ss);
				if(i)
				{
					char formatted[25];
					if (Ql_strlen(ss) == 10 && ss[0] >= '0' && ss[0] <= '9')
						Ql_sprintf(formatted, "+91%s", ss);
					else
						Ql_strcpy(formatted, ss);
					Ql_strcpy(VTSData.PhoneNumber.Mob1,formatted);
				}
			}
			else
			{
				i=GetValueFromData(ls,"SOSSET",' ',0,'\0',ss);
				if(i)
				{
					char formatted[25];
					if (Ql_strlen(ss) == 10 && ss[0] >= '0' && ss[0] <= '9')
						Ql_sprintf(formatted, "+91%s", ss);
					else
						Ql_strcpy(formatted, ss);
					Ql_strcpy(VTSData.PhoneNumber.Mob0,formatted);
					Ql_strcpy(VTSData.PhoneNumber.Mob1,formatted);
				}
			}
			UpdateConfigInFlash();
			Ql_sprintf(SimData,"SOS Number Updated : %s, %s",VTSData.PhoneNumber.Mob0,VTSData.PhoneNumber.Mob1);
			SendResponce(SMSSender,SimData,IsServer,1);
			return 1;
		}
```
* **Impact**: Fixes the `SETSOSSET` command to work with or without a comma and handles prefixing automatically.

#### Fix #1.3: Format Reply Numbers in `ACTV` & `HCHK`
* **Files**: [custom/SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c) (lines 1422 & 1471), and [custom/Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c) (lines 3320 & 3347)
* **Changes**: Checks if the parsed phone number `ss` is 10 digits and prepends `+91` to route the confirmation SMS correctly.
```c
char formatted_ss[25];
if (Ql_strlen(ss) == 10 && ss[0] >= '0' && ss[0] <= '9')
    Ql_sprintf(formatted_ss, "+91%s", ss);
else
    Ql_strcpy(formatted_ss, ss);
SendSMS(formatted_ss, SimData);
```
* **Impact**: Ensures confirmation responses are routed successfully even if the country code was omitted in the request.

#### Fix #1.4: Format Maharashtra `SOS#` Command Stored Number
* **File**: [custom/SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c) (line 666)
* **Changes**: Applies auto-prefixing formatting to `ss` if it is a 10-digit number.
```c
char formatted[25];
if (Ql_strlen(ss) == 10 && ss[0] >= '0' && ss[0] <= '9')
    Ql_sprintf(formatted, "+91%s", ss);
else
    Ql_strcpy(formatted, ss);
Ql_strcpy(VTSData.PhoneNumber.Mob0,formatted);
```

---

## 2. Centralized LED Manager Implementation

### Architecture Overview
Implemented a centralized LED management system that automatically monitors system states and updates LED patterns accordingly, eliminating edge cases where LEDs might miss state updates.

* **Previous Approach**: LED controls were scattered across multiple files (`GPRS.c`, `GPS.c`, `Hardware.c`, `SOS.c`), risking missed updates when code paths bypassed them.
* **New Approach**: Centrally monitors variables and schedules LED patterns from the Hardware thread (`LEDManager_Process()`) running every 200ms.

### Files Created
1. **[custom/LEDManager.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/LEDManager.c)**: Main implementation containing state determination, pattern lookup, and process loops.
2. **[custom/inc/LEDManager.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/LEDManager.h)**: Header file defining states, structures, and function prototypes.

### LED States Defined

#### GSM States
* `LED_GSM_INIT` - Initializing (fast blink: 3/6)
* `LED_GSM_SEARCHING` - Searching network (blink: 2/10)
* `LED_GSM_REGISTERED` - Network registered (blink: 5/10)
* `LED_GSM_GPRS_READY` - GPRS active (mostly on: 9/10)
* `LED_GSM_REG_DENIED` - Registration denied (very fast: 1/2)
* `LED_GSM_SLEEP` - Sleep mode (slow blink: 1/15)

#### GPS States
* `LED_GPS_SEARCHING` - Searching (always on: 10/10)
* `LED_GPS_FIXED` - Fix acquired (slow blink: 2/10)
* `LED_GPS_OFF` - GPS off

#### Battery States
* `LED_BATT_LOW` - Battery low (blink: 2/10)
* `LED_BATT_NORMAL` - Battery normal (off)

#### SOS States
* `LED_SOS_ACTIVE` - SOS active (always on: 10/10)
* `LED_SOS_OFF` - SOS off

### Integration Points

#### Hardware Thread (Hardware.c)
```c
void HardwareThreadEntry(s32 taskId)
{
    // ... initialization ...
    hw_init();  // Calls LEDManager_Init()
    
    while(1)
    {
        // ... peripheral processing ...
        LEDManager_Process();  // Main LED update call
        ThreadSleep(200);
    }
}
```

#### Monitored Variables
The LED Manager monitors:
* `GSM.GSMState` (from `GPRS.c`)
* `GPS.State` and `GPS.GPSFix` (from `GPS.c`)
* `PeriPheralVal.BattVolt` vs `VTSData.BattThrs` (from `Hardware.c`)
* `SOS.IsSOS` (from `SOS.c`)
* `SleepConfig.IsEnabled` (from `Hardware.c`)

---

## 3. HTTP Queue `IsSendProcess` Memory Corruption Fixes

### Root Cause Identified
Logs showed `IsSendProcess = 85` (instead of 0 or 1), indicating memory corruption. This was resolved via the following steps:
1. **`IsSendProcess` Not Declared Volatile**: Caused compiler caching in registers, inducing race conditions between Server and HTTP Queue threads.
2. **Missing Error Path Clears**: Lock failures in `HttpQueue.c` failed to reset `IsSendProcess = 0`, locking up the queue permanently.
3. **Duplicate Variable Declarations**: Duplicate declarations of variables in memory layouts led to overflows.

### Fixes Applied

#### Fix #3.1: Make `IsSendProcess` Volatile
* **Files**: [custom/Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c) (line 50) and [custom/inc/Server.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/Server.h) (line 143)
* **Changes**: Added `volatile` keyword to ensure all operations access main memory directly rather than registers.
```c
volatile uint8_t IsSendProcess;
```

#### Fix #3.2: Remove Duplicate Declarations
* **File**: [custom/Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c) (lines 50-51)
* **Changes**: Removed duplicate allocations of `IsCritical` and `IsPackeAlert` that could overflow into adjacent memory.

#### Fix #3.3: Safety Checks at `HttpQueue_Process` Entry
* **File**: [custom/HttpQueue.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/HttpQueue.c) (line 147)
* **Changes**: Automatically detects invalid values (> 1) and resets `IsSendProcess` to 0.
```c
void HttpQueue_Process(void)
{
    if(IsSendProcess > 1) {
        LOGData(TAG_SERVER, "WARNING: IsSendProcess corrupted! Value=%d, resetting to 0", IsSendProcess);
        IsSendProcess = 0;
    }
...
```

#### Fix #3.4: Clear `IsSendProcess` on Error/Lock Exit Paths
* **File**: [custom/HttpQueue.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/HttpQueue.c) (lines 160 and 192)
* **Changes**: Added `IsSendProcess = 0;` resets when mutex lock acquisitions fail in the queue processing loop.
```c
if(!HttpQueue_WaitLock(50)) {
    httpQueue.sending = 0;
    IsSendProcess = 0;
    LOGData(TAG_SERVER, "[FIX_LEAK] HttpQueue initial lock timeout, IsSendProcess cleared");
    return;
}
```

---

## 4. GPS Fault MCU Reset Logic

### Requirements
1. Comment out/disable the 3 soft resets (FOTA/MOTA resets in `SMS.c`) to avoid unexpected reboots during configuration.
2. Ensure that a system soft reset (`Ql_Reset(0)`) is triggered **only** when the GPS module reports a persistent hardware-level communication failure (`FLT` state / `GPS.State == 0`).
3. Ensure the module **never** resets when the GPS is merely searching for satellite locks (`NF` state / `GPS.GPSFix == 0`).

### Implementation Details
* **FOTA/MOTA Soft Resets (Commented Out)**: Verified in [custom/SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c) at lines 288, 375, and 407. They remain commented out.
* **MCU Reset on GPS FLT**: Modified `gps_reset_routine()` in [custom/GPS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/GPS.c) to execute `Ql_Reset(0)` when a persistent hardware fault is detected:
```c
void gps_reset_routine(void)
{
    LOGData(TAG_GPS,"GPS Reset Routine Triggered - Hardware Fault Detected\r\n");
    ThreadSleep(1000);
    if(GPSTimeout>0)
        return;
    
    // Trigger module soft reset since GPS remains in fault state
    LOGData(TAG_GPS,"GPS remains in FLT state, resetting MCU/Module...\r\n");
    ThreadSleep(1000);
    Ql_Reset(0);
}
```

---

## Verification & Expected Behaviors

### Compiled Successfully
All files compile and the binary target image is generated correctly:
```text
make.exe[1]: Entering directory `D:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66'
- GCC Compiling Finished Sucessfully.
make.exe[1]: Leaving directory `D:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66'
```

### Expected Commands & Output Matrix

| Command / Event | Input / Trigger | Stored Configuration | Expected Behavior / Response |
| :--- | :--- | :--- | :--- |
| **SMS** | `SETSOSSET 8489607555` | `Mob0: +918489607555`<br>`Mob1: +918489607555` | Replies: `SOS Number Updated : +918489607555, +918489607555` |
| **SMS** | `SETSOSSET +918489607555,9600696008` | `Mob0: +918489607555`<br>`Mob1: +919600696008` | Replies: `SOS Number Updated : +918489607555, +919600696008` |
| **SMS** | `ACTV,123456,8489607555` | N/A | Sends activation response SMS directly to `+918489607555` |
| **GPRS** | `SET M0:8489607555` | `Mob0: +918489607555` | Responds: `ACK,M0:+918489607555*` |
| **GPRS** | `SET M1:+919600696008` | `Mob1: +919600696008` | Responds: `ACK,M1:+919600696008*` |
| **GPS NF (No Fix)** | Indoor or no lock (`GPS.GPSFix == 0`, `GPS.State == 1`) | No action | **No reset**. Device runs normally and reports `GPS NF` via SMS. |
| **GPS FLT (Fault)** | GPS disconnected (`GPS.State == 0`) | Soft Reset | Module executes `Ql_Reset(0)` and reboots. |

# Firmware and Packet Protocol Modifications Documentation

This document describes the recent changes made to the parser logic and the format of the activation/health check packets (`ACTVR` / `HCHKR`).

---

## 1. Reverted Parser Protections

Previously implemented false-positive token boundary checks and parameter adjustments have been reverted to their original behaviors:

### A. SMS Command Parsing
In [SMS.c](file:///D:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c), the boundary checking logic (which matched `ACTV` and `HCHK` only when surrounded by whitespace or delimiters) was removed. It has been restored to simple substring checks:

```diff
-   ls = Ql_strstr(msg,"ACTV");
-   if(ls && (ls == msg || ls[-1] == ' ' || ls[-1] == '\r' || ls[-1] == '\n') &&
-       (ls[4] == ',' || ls[4] == ' ' || ls[4] == '\r' || ls[4] == '\n' || ls[4] == '\0'))
+   ls = Ql_strstr(msg,"ACTV");
+   if(ls)
```

```diff
-   ls = Ql_strstr(msg,"HCHK");
-   if(ls && (ls == msg || ls[-1] == ' ' || ls[-1] == '\r' || ls[-1] == '\n') &&
-       (ls[4] == ',' || ls[4] == ' ' || ls[4] == '\r' || ls[4] == '\n' || ls[4] == '\0'))
+   ls = Ql_strstr(msg,"HCHK");
+   if(ls)
```

### B. Server Command Parsing
In [Server.c](file:///D:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c), the identical boundary checking logic for `ACTV` and `HCHK` was also reverted:

```diff
-   fn = Ql_strstr(buff,"ACTV");
-   if(fn && (fn == buff || fn[-1] == ' ' || fn[-1] == '\r' || fn[-1] == '\n') &&
-       (fn[4] == ',' || fn[4] == ' ' || fn[4] == '\r' || fn[4] == '\n' || fn[4] == '\0'))
+   fn = Ql_strstr(buff,"ACTV");
+   if(fn)
```

### C. Server-side FOTA / MOTA Call Parameters
In [Server.c](file:///D:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c), the `SET FOTA` command parsing was reverted to pass the entire buffer `buff` into `FOTAPacket()` and `MOTAPacket()` rather than passing only the offset pointer (`ls`):

```diff
-       ls = Ql_strstr(fn,"FOTA");
-       if(ls)
-       {
-           FOTAPacket(ls,SMSSender,1);
-           return;
-       }
-       ls = Ql_strstr(fn,"MOTA");
-       if(ls)
-       {
-           MOTAPacket(ls,SMSSender,1);
+       if(Ql_strstr(fn,"FOTA"))
+       {
+           FOTAPacket(buff,SMSSender,1);
+           return;
+       }
+       if(Ql_strstr(fn,"MOTA"))
+       {
+           MOTAPacket(buff,SMSSender,1);
            return;
        }
```

---

## 2. Updated ACTVR / HCHKR Packet Format

The activation and health check response packets (`ACTVR` / `HCHKR`) generated by `MakeACTMessage()` in [SMS.c](file:///D:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c) were modified to match the updated formatting specifications:

### A. Removal of the 'V' Prefix from the Firmware Version
If `FirmVer` starts with `'V'`, the leading character is skipped. If it does not start with `'V'`, it is printed as-is:

```diff
-   #ifndef PROTO_CDAC 
-   if(FirmVer[0] != 'V')
-       InsertChar(SimData,'V');
-   #endif
-   Ql_strcat(SimData,FirmVer);
+   if(FirmVer[0] == 'V')
+       Ql_strcat(SimData,FirmVer+1);
+   else
+       Ql_strcat(SimData,FirmVer);
```

### B. Removal of Leading Zeroes in Latitude and Longitude
The formatting width has been changed from `%012.8f` to `%011.8f` to print exactly 2 digits before the decimal point instead of padding them with a leading zero:

```diff
-   StringAdd(SimData,"%012.8f",GPS.Latitude); //012.12345678
+   StringAdd(SimData,"%011.8f",GPS.Latitude); //011.12345678
...
-   StringAdd(SimData,"%012.8f",GPS.Longitude);
+   StringAdd(SimData,"%011.8f",GPS.Longitude);
```

### C. Removal of Leading Zeroes in the Alert ID Field
The hardcoded `",01,"` alert ID string was replaced with a dynamic field (`mode + 1`) formatted without leading zeroes:

```diff
-   Ql_strcat(SimData,",01,");
+   char alert_id_str[5];
+   Ql_sprintf(alert_id_str, ",%d,", mode + 1);
+   Ql_strcat(SimData, alert_id_str);
```
* `ACTVR` (`mode 0`): Outputs `1`
* `HCHKR` (`mode 1`): Outputs `2`

### D. Dynamic Ignition status suffix
The hardcoded `,NR` code at the end of the packet was replaced with a dynamic check based on the Ignition status of the vehicle:

```diff
-   #ifdef PROTO_ODISA1
-   Ql_strcat(SimData,",NR");
-   #else
-   Ql_strcat(SimData,",NR");
-   #endif
+   if (PeriPheralVal.IGN)
+       Ql_strcat(SimData,",IN");
+   else
+       Ql_strcat(SimData,",IF");
```
* Ignition ON: Suffix is `,IN`
* Ignition OFF: Suffix is `,IF`

---

## Example Packets Comparison

### Original Format:
```text
ACTVR,345855,APMG,V1.5.5,868329088626522,01,022.08427550,N,073.20458283,E,1,19062026 090234,183.14,00.0,14,404,0098,153F,1,1,04.2,000501,NR
```

### New Format:
```text
ACTVR,496849,APMG,1.2.1,865510088614401,1,22.08933780,N,69.27737450,E,1,19062026 083707,109.94,00.0,31,404,0098,20EA,0,0,00.0,000136,IF
```

---

## 5. ACTVR Format, VDETAIL Version Prefix, and FOTA/MOTA Watchdog Reset Protection

### 1. Updated ACTVR and HCHKR Packet Format
The packet generation format in `MakeACTMessage()` has been updated to the new specification:
* **Firmware Version:** Preserves or adds the `V` prefix (e.g., `V1.6.1`).
* **Latitude and Longitude:** Formatted to exactly 6 digits after the decimal point (`%.6f`).
* **Speed:** Formatted with width 3 (`%03.1f`), resulting in `0.0` when stationary instead of `00.0`.
* **Voltage:** Display the main voltage (`PeriPheralVal.MainsVolt`) instead of internal battery voltage (`PeriPheralVal.BattVolt`).

Example Output:
```text
ACTVR,163262,APMG,V1.6.1,868329086201534,1,15.025661,N,78.924909,E,1,22062026 104204,246.4,0.0,30,404,0040,1B73,1,1,11.5,000135,IN
```

### 2. State Tag Prefix in VDETAIL Version Response
In [SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c), the `GETVDETAILS` response version has been modified to prepend the state abbreviation tag (`PROTO_TAG`):
* **Format:** `FirVer = <PROTO_TAG>_<FirmVer>` (e.g. `TN1_1.5.5` or `TN1_V1.5.5`).

### 3. FOTA & MOTA Watchdog and Reset Protection
To ensure FOTA and MOTA update processes do not fail from unexpected watchdog timeouts or software resets, the following improvements have been made:
* **Correct Watchdog Pins for FOTA:** In [FTP.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/FTP.c), `FOTAUpdate` now initializes `ST_FotaConfig` using the real watchdog pins (`pinWtd1` and `pinWtd2` from `Ql_WTD_GetWDIPinCfg()`) instead of the dummy `LED_GPS_GPIO`. This ensures the FOTA bootloader correctly feeds the external hardware watchdog.
* **Bypass GPS Timeout Resets:** In [GPS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/GPS.c), `gps_reset_routine` is bypassed if `IsMotaProcessing || IsFotaProcessing` is active.
* **Bypass GPRS Profile Switching and Reboots:** In [GPRS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/GPRS.c), reboots in GPRS registration denied loop (`ProcessREGISTER`) and profile switching logic (`CheckprfReq`) are bypassed when `IsMotaProcessing || IsFotaProcessing` is active.

---

## 6. GPS Simulation Modes (CGPS and FGPS)

The firmware includes a built-in simulation engine that allows developers to override or supplement real GPS coordinates using OTA/SMS commands for testing and validation.

### 1. Conceptual Overview
* **FGPS (Fallback GPS Simulation):** Supplements the real GPS receiver. It overrides GPS data with simulated coordinates **only** if the real GPS module has no valid lock (`GPS.GPSFix == 0`) or if a GPS receive timeout occurs. Once the real GPS achieves a valid fix, the simulation is bypassed, and real coordinates are used.
* **CGPS (Continuous Forced GPS Simulation):** Unconditionally overrides the real GPS module. It completely skips NMEA parsing from the GPS UART stream, preventing real coordinates from being used regardless of hardware lock status.

### 2. Stationary Speed Lock (Zero Fluctuations)
* To prevent false movement logs, when simulated speed is set to `0` (`fGPSSpeed == 0`), the simulated speed is mapped directly (`GPS.Speed = 0.0`) without any random walk fluctuations.

### 3. Hardware Reset Bypass
* When simulation is active (`GPS_IsSimulationActive()`), the GPS thread's fault recovery routine (`gps_reset_routine()`) is bypassed to prevent cycling power or soft resetting the module during testing.

### 4. Implementation Details
* **Global parameters:** `fGPSLat`, `fGPSLong`, `fGPSAlt`, `fGPSpdop`, `fGPShdop`, `fGPSSats`, `fGPSSpeed`, `fGPSHeading`, and `fGPSForce` are defined in [Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c) and exported in [Server.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/Server.h).
* **SMS Command Format:** `SET FGPS lat,long,hdop,pdop,noofsat,altitude,speed,heading` or `SET CGPS ...`. Latitude/longitude are scaled by $10^6$, HDOP/PDOP by $100$.
* **Simulation Loop:** Inside `gps_thread_entry` in [GPS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/GPS.c), a periodic 1-second clock checks the status of `fGPSForce` and the real GPS fix status to determine whether to apply simulated coordinates via `ApplyFGPS()`.

---

## 7. GPS Heading Precision Tuning

Modified the precision of the GPS heading field in packets. The heading is formatted to exactly one decimal place (e.g. `355.0` instead of `355.00`) to match requirements and remove trailing redundant decimal zeros.

### Files Modified:
* **[GPS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/GPS.c)**: Updated formatting strings from `%3.2f` and `%03.2f` to `%3.1f` and `%03.1f` in `gps_parameter_init()`, `gps_gga_update()`, and GPS simulation logic.
* **[Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c)**: Updated `sHead` tracking buffer formatter in `PrepareFTKBuffer()` to output `%3.1f`.

---

## 8. Dynamic Custom/Virtual IMEI Configuration (DISABLED / COMMENTED OUT)

> [!WARNING]
> This feature and all corresponding structures, initialization logic, IMEI checking overrides, and SMS parsing commands (`SET IMIDISABLE`, `GET IMIDISABLE`, `SET IMI`, `GET IMI`, `CLR IMI`) have been **fully disabled and commented out** from the source code. The device will default to its hardware IMEI (or virtual IMEI `VIMEI` if `VIRTUAL_IMEI` is configured).

### Original Feature Details (For Reference)
Implemented a feature allowing a virtual IMEI to be set on the device dynamically using SMS/OTA commands. The custom IMEI is stored in non-volatile flash memory, used in place of the hardware IMEI in communications, and is cleared when the device is reverted to defaults. An option is also provided to disable or enable the custom IMEI commands entirely.

### SMS/OTA Commands (Now Disabled)
1. `SET IMIDISABLE <0/1>`: Enables (`0`) or Disables (`1`) the custom IMEI configuration commands.
2. `GET IMIDISABLE`: Returns the current command disable status (`IMI Disable: <0/1>`).
3. `SET IMI <15-digit IMEI>`: Sets the custom IMEI (only allowed if `IMIDISABLE` is `0`).
4. `SET IMI 0` / `CLR IMI`: Resets/Clears the custom IMEI back to hardware default (only allowed if `IMIDISABLE` is `0`).
5. `GET IMI`: Returns the current custom IMEI status (only allowed if `IMIDISABLE` is `0`).

### Files Modified & Commented Out:
* **[VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h)**: Defined `FIMEITypeDef` struct and added `uint8_t DisableImiCmd` and `FIMEITypeDef CustomImei` fields to `VTSTypedef` (now commented out).
* **[GPRS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/GPRS.h)**: Expose `GetDeviceIMEI()` function declaration globally.
* **[GPRS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/GPRS.c)**: Updated `GetDeviceIMEI()` function to load IMEI from `VTSData.CustomImei.Imei` if custom IMEI is enabled (now commented out, defaulting to hardware/virtual IMEI).
* **[File.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/File.c)**: Initialized `VTSData.DisableImiCmd = 1` (disabled by default) and cleared `CustomImei` parameters during defaults reload in `LoadDefault()` (now commented out).
* **[SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c)**: Parsed `SET IMIDISABLE`, `GET IMIDISABLE`, `SET IMI`, `GET IMI`, and `CLR IMI` commands, and restricted IMEI configuration commands based on the `VTSData.DisableImiCmd` flag (now commented out).


