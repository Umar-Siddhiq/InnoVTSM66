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
9. [FTP/FOTA UFS Storage & Disk Space Cleanup System](#9-ftpfota-ufs-storage--disk-space-cleanup-system)
10. [Sensors Subsystem Integration (RFID, Fuel, and Ultrasonic CLS)](#10-sensors-subsystem-integration-rfid-fuel-and-ultrasonic-cls)
11. [SOS SMS Alerts Feature & SOSDISABLE Configuration System](#11-sos-sms-alerts-feature--sosdisable-configuration-system)
12. [Geofence Device Alert Packets & History Storage Control](#12-geofence-device-alert-packets--history-storage-control)
13. [SMS Storage Leak Fix & UFS Disk Commands](#13-sms-storage-leak-fix--ufs-disk-commands)
14. [eSIM Carrier Switching Under Low CSQ and Flapping Signal Conditions](#14-esim-carrier-switching-under-low-csq-and-flapping-signal-conditions)
15. [UFS Disk Space Safety Reserve and Automated History Purging](#15-ufs-disk-space-safety-reserve-and-automated-history-purging)
16. [Rajasthan Protocol (NIC_RAJASTHAN) and Network/Battery Settings](#16-rajasthan-protocol-nic_rajasthan-and-networkbattery-settings)
17. [GPS Module Support Improvements (my_rand Redefinition & Fault Reset Bypass)](#17-gps-module-support-improvements-my_rand-redefinition--fault-reset-bypass)

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


---

## 9. FTP/FOTA UFS Storage & Disk Space Cleanup System

### Root Cause Identified
To perform Over-The-Air (OTA) firmware updates, the device downloads a firmware binary image from an FTP server. In previous configurations, storing this download in flash User File System (UFS) caused write aborts returning error code `-6` (Out of disk space) because old logs (`FOTA.txt`, `EVENT.txt`) or historical firmware binaries accumulated over time. An intermediate attempt resolved this by allocating a virtual RAM partition, but this ran risk of heap exhaustion on the module's 1.5MB RAM limit.

### Solutions Implemented

#### 1. Restored UFS Storage Targets
- **Files**: [FTP.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/FTP.h) & [FTP.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/FTP.c)
- **Changes**: Disabled `FTP_FILE_RAM` compilation flags globally. Restored `SERVER_FOTA_FILEPATH` to `"app.bin"` and `SERVER_MOTA_FILEPATH` to `"mcu.bin"` (repointing the download streams to persistent flash). Reverted file operations inside `FOTAUpdate` to use `Ql_FS_Open` directly.

#### 2. Pre-Download Automatic Disk Space Cleanup
- **File**: [FTP.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/FTP.c)
- **Changes**: Implemented `FTP_CleanupDiskSpace(uint32_t requiredSize)` which checks the available space on the flash drive using `Ql_FS_GetFreeSpace()`. If the free space is less than `requiredSize + 50KB` (safety margin), the system automatically deletes disposable update artifacts:
  - `"FOTA.txt"`
  - `"EVENT.txt"`
  - `"app.bin"`
  - `"mcu.bin"`
  - `"app_fota.bin"`
- **Integration**: Invoked inside `FTPDownloadFileWithStorage()` right after fetching the file size from the FTP server.

#### 3. Remote Diagnostic SMS Commands
- **File**: [SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c)
- **Changes**: Added parser logic inside `DecodeSMS()` to support manual storage queries and cleanup over SMS/GPRS:
  - `CLR DISK`: Calls `FTP_ClearRecoverableDiskData()` to remove internal history packets, batch queue files, and stale OTA/download artifacts. It preserves `UFSConfig.bin`, `State.bin`, and `FotaConfig.bin`; it does not format UFS.
  - The command is rejected while FOTA or MOTA is active, preventing removal of a file in use.
  - A successful reply includes free space and removal counts: `Disk Cleared. Free: X.XX MB (+Y.YY MB), H:<history> B:<batch> F:<files>`.
  - A failed reply includes the free space and error count: `Disk Clear Failed. Free: X.XX MB, Errors:<count>`.
  - `GET DISK`: Queries and reports remaining space: `UFS Disk Space: X.XX MB Free`.

---

## 10. Sensors Subsystem Integration (RFID, Fuel, and Ultrasonic CLS)

### Conceptual Design
Integrated a multi-sensor subsystem supporting split-processor interfacing, serial packet encapsulation, dynamic reporting intervals, and persistent isolated flash storage.

```
                  +----------------------------------+
                  |           NuMicro MCU            |
                  +----------------------------------+
                                    |
                                    v (UART1: 115200)
                                    | Wrapped frames: &#OTA,<payload>\r\n
                                    v
                 +------------------------------------+
                 |        Quectel M66 OpenCPU         |
                 |      (Sensors Subsystem Core)      |
                 +------------------------------------+
                   /                |               \
                  v                 v                v
            [RFID Data]        [Fuel Data]      [Ultrasonic CLS]
          (Interrupt Type)   (Regular Type)     (Query-Response)
                 \                  |                /
                  v                 v               v
             [SINT Packet]          |         [Query command]
             (Sent immediately)     v         (@01E0006# 3s offset)
                                    |               |
                                    v               v
                               [SENS Packet]  [Parse response]
                               (Sent regular) (@01E13...#)
```

### Components and Implementation

#### 1. Sensor Data Handlers and Packetizers
- **Files**: [Sensors.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/Sensors.h) [NEW] & [Sensors.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Sensors.c) [NEW]
- **Implementations**:
  - `ParseSensorData(const char* data, uint16_t len)`: Inspects incoming character streams. Identifies standard Fuel strings wrapped in `*` and `#` (regular interval) and RFID numbers (12 hex digits, interrupt type).
  - `MakeSensorPacket()`, `MakeInterruptPacket()`: Packages sensor telemetry into `$SENS` (regular) and `$SINT` (immediate interrupt) packet strings, calculating a simple XOR checksum.
  - `QueryCLSSensor()` / `ParseCLSResponse()`: Periodically polls the Ultrasonic CLS sensor (`@01E0006#`) and parses the response format (`@01E13...#`), validating the checksum.

#### 2. Configuration Flash Storage Isolation
- **File**: [Sensors.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Sensors.c)
- **Changes**: Implemented `SaveSensorConfigToFlash()` and `LoadSensorConfigFromFlash()` wrapping around standard OpenCPU `SaveToFlash` and `LoadFromFlash` methods in [File.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/File.c) to isolate sensor settings in a dedicated file (`"sensor_cfg.bin"`).

#### 3. Serial Packet Interception and SMS Setup
- **File**: [SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c)
- **Changes**: 
  - Intercepted incoming serial strings from the MCU (`OTA_SRC_RS232`) inside `DecodeSMS()` to pass them to `ParseSensorData()`.
  - Added administrative command decoders:
    - `GET SENS`: Returns active state and reporting intervals.
    - `SET SENSINT <DayIGN,NightIGN,DayOFF,NightOFF>`: Configures reporting intervals.
    - `SET SENSEN <0/1>`: Enables/Disables the sensor subsystem.
    - `SET SENSTOUT <seconds>`: Sets the disconnected sensor timeout.

#### 4. Dynamic Scheduling and 3-Second Query Offset
- **File**: [Systic.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Systic.c)
- **Changes**: Replaced standard sensor ticks in `UpdateTick()`. The system queries `GetCurrentSensorInterval()` to determine the target interval based on:
  - **Day hours (6:00 AM to 6:00 PM)** or **Night hours (6:00 PM to 6:00 AM)**.
  - **Ignition ON** (using `dayIGNITIONInterval`/`nightIGNITIONInterval`) or **Ignition OFF** (using `dayOFFInterval`/`nightOFFInterval`).
  - **CLS Polling Offset**: Ultrasonic CLS sensors require a query-response cycle. To account for serial transit latency, the query command (`@01E0006#`) is transmitted exactly **3 seconds before** the regular `$SENS` interval expires.

#### 5. Background Upload Thread
- **File**: [Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c)
- **Changes**: Registered `handleSensorData()` wrapper inside `ServerThreadEntry()` to periodically process and transmit pending sensors data (`$SENS` / `$SINT`) to server sockets.

#### 6. Startup Boot Initialization
- **File**: [main.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/main.c)
- **Changes**: Invokes `InitSensors()` and `LoadSensorConfigFromFlash()` on boot within `system_init()` (conditionally compiled out for `PROTO_CDAC` builds) to prepare the buffers and configuration.

---

## 11. SOS SMS Alerts Feature & SOSDISABLE Configuration System

### Feature Overview
The SOS management subsystem supports protocol-independent alert processing, master hardware SOS disable control, dynamic SMS alert dispatching, and persistent flash migration.

### 1. Alert IDs & Types
- `10`: Emergency State ON
- `11`: Emergency State OFF
- `16`: Emergency wirecut (controlled by `SOS_WIRECUT_SMS_ENABLED`)

### 2. Master Hardware SOS Disable Control (`SOSDISABLE`)
- **Commands**:
  - `SET SOSDISABLE 1`: Master disables all SOS button and tamper monitoring. Clears active SOS/Tamper states, turns off the SOS LED (`SLED_OFF`), and skips reading input pins in `ProcessSOS()`.
  - `SET SOSDISABLE 0`: Enables SOS system.
  - `GET SOSDISABLE` / `SOSDISABLE`: Returns `SOS Disable: <0/1>`.
- **Implementation**:
  - **Config Field**: `VTSData.DisableSOS` in `VTSTypedef` ([custom/inc/VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h#L592)).
  - **State Machine Check**: Handled at top of `ProcessSOS()` in [custom/SOS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SOS.c#L78).

### 3. SOS SMS Alert Delivery (`SOSSMS`)
- **Commands**:
  - `SET SOSSMS 1`: Enables SOS SMS notifications (returns `SOS SMS Enabled`).
  - `SET SOSSMS 0`: Disables SOS SMS notifications (returns `SOS SMS Disabled`).
  - `GET SOSSMS`: Returns `SOS SMS: ON`, `SOS SMS: OFF`, or `SOS SMS: DISABLED IN FIRMWARE`.
- **Firmware Overrides & Compile Switches**:
  - `#define SOS_SMS_FEATURE_ENABLED 0`: Overrides flash config to permanently disable SOS SMS delivery in code when required for legacy FOTA builds. `SET SOSSMS 1` returns `SOS SMS Disabled In Firmware`.
  - `#define SOS_WIRECUT_SMS_ENABLED 0`: Independent control for Alert ID `16` wirecut SMS delivery.
- **Payload Format**: Formats a compact (<160 char) single-part SMS with Alert ID, Alert Name, IMEI, Lat/Lon, Google Maps URL link, and UTC timestamp converted to IST (+5:30).
- **Command `SET SOSSET`**: Updates recipient mobile numbers `Mob0` and `Mob1`, automatically adding `+91` prefix for 10-digit numbers and skipping dummy numbers (e.g. `0000000`).
- **Alert Trigger Routing**: Replaced old dynamic Server 2 state check logic in [custom/SOS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SOS.c) with direct calls to `SendSOSAlertSMS()`, sending alerts directly to SOS numbers.

### 4. Configuration Persistence & FOTA Migration
- Config fields `DisableSOS`, `SOSSmsEnabled`, `SOSSmsConfigSignature` managed in [custom/File.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/File.c).
- `LoadConfig()` validates file sizes (`legacySize`) and signatures to cleanly migrate legacy flash layouts during FOTA updates without resetting user preferences.

---

## 12. Geofence Device Alert Packets & History Storage Control

### 1. Geofence Device Alert Packets
- **Real-Time Trigger**: Triggered when entering or exiting configured geofence boundaries.
  - Geofence In: `GFIN_ALERT` (ID 13)
  - Geofence Out: `GFOUT_ALERT` (ID 14)
  - Handled automatically inside the state machine of [custom/Geofence.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Geofence.c#L151) via `AddAlert(GFIN_ALERT)` and `AddAlert(GFOUT_ALERT)`.
- **Manual SMS Trigger**:
  - `TESTRIG 13` (or `SETTESTRIG 13` / `SETTESRIG 13`) triggers a real-time Geofence In alert packet.
  - `TESTRIG 14` (or `SETTESTRIG 14` / `SETTESRIG 14`) triggers a real-time Geofence Out alert packet.
  - Parsed at the top-level of [custom/SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c#L1566) directly on the incoming message string to handle all formats and typos.

### 2. History Storage & Transmission Control (`HISTORY` / `HIST`)
- **SMS / Server Commands**:
  - `SET HISTORY 1` (or `SET HIST 1`): Enables offline history packet storage (`VTSData.DisableHistory = 0`), saving configuration to flash. Responds `History Enabled`.
  - `SET HISTORY 0` (or `SET HIST 0`): Disables offline history packet storage (`VTSData.DisableHistory = 1`), saving configuration to flash. Responds `History Disabled`.
  - `GET HISTORY` / `GET HIST`: Returns `History: ENABLED` or `History: DISABLED`.
  - `CLR HISTORY` / `CLEAR HISTORY`: Clears stored history packets (`DeleteAllPackets()`) and responds `History Cleared`.
- **Logic Verification**:
  - **Suppress Save**: When `DisableHistory == 1`, history packets are not written to flash in `SendDatatoServer0()` and `SendDataToServer()` when disconnected from the server socket.
  - **Suppress Send**: When `DisableHistory == 1`, history packets reading and sending is skipped inside `ProcessHistoryPacket()`.
- **Configuration Field**: `uint8_t DisableHistory;` inside `VTSTypedef` ([custom/inc/VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h#L594)), default initialized to `0` (*History Enabled*) inside `LoadDefault()` in [custom/File.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/File.c#L187).

---

## 13. SMS Storage Leak Fix & UFS Disk Commands

### 1. SIM Card SMS Storage Leak Fix
- **Root Cause**: When new SMS messages arrived, the modem triggered the `URC_NEW_SMS_IND` event which invoked `Hdlr_RecvNewSMS()`. The function successfully read the text content into memory for parsing but never deleted the SMS index from SIM card storage. As user commands accumulated, the SIM card's fixed capacity (typically 20-30 messages) became fully exhausted, causing the network operator to halt all future SMS delivery to the device.
- **Fix Applied**: 
  - Called `RIL_SMS_DeleteSMS(nIndex, RIL_SMS_DEL_INDEXED_MSG)` immediately inside `Hdlr_RecvNewSMS()` in [custom/SMSLib.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMSLib.c#L225) after reading the SMS.
  - This keeps SIM storage slots continuously clear, preventing any future delivery blocking.

### 2. GET DISK & CLR DISK SMS Commands
- **Commands**:
  - `GET DISK`: Queries the remaining free UFS space on the module and sends back `UFS Disk Space: X.XX MB Free`.
  - `CLR DISK`: Clears recoverable UFS data and returns a result with free space plus history, batch, and transient-file counts.
- **Implementation**:
  - **Dedicated clear routine**: `FTP_ClearRecoverableDiskData()` in [custom/FTP.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/FTP.c) is intentionally separate from `FTP_CleanupDiskSpace()`, which remains the selective FOTA pre-download helper.
  - **History and batch cleanup**: `ClearHistoryStorage()` removes all `BK_*.bin` packets and the count file; `ClearBatchStorage()` removes `BatchFile.bin` and `BatchIndex.bin`.
  - **Safety and reporting**: Each existing file deletion is checked. The SMS handler returns failure instead of reporting a false success, and rejects cleanup during active FOTA/MOTA.

---

## 14. eSIM Carrier Switching Under Low CSQ and Flapping Signal Conditions

### 1. The Issue (Registration Flapping Starvation)
- **Problem**: When a SIM profile (like BSNL) has weak or flapping signal, the signal drops to `99` (no signal) for most of the time but momentarily recovers to a valid level (like `13-16`) for a few seconds before dropping back to `99`. 
- **Root Cause**: During the few seconds of valid signal, GPRS registers momentarily, and `GSM.GSMState` becomes `GPRS_ACTIVE`. Inside the connection watchdog logic in [custom/Systic.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Systic.c), the condition checked `GSM.GSMState < GPRS_ACTIVE`. Because GPRS was active, this check evaluated to `FALSE`, resetting the connection watchdog counter `IntervalTick.ProfileChangeCount` to `0`. 
- **Impact**: The watchdog never reached the connection timeout threshold because it was reset every time a momentary registration spike occurred. Consequently, the device remained permanently starved/stuck on the dead profile instead of failing over to another carrier.

### 2. Implemented Fixes
- **Removal of GPRS Active Reset Guard**: Removed the `&& GSM.GSMState < GPRS_ACTIVE` condition from the watchdog reset logic. The watchdog timer now continues to accumulate time unless a successful, stable connection to the server is actually established (`HTTPState == HTTP_STATE_SET` in CDAC mode or socket is connected in non-CDAC mode). 
- **Profile-Specific Connection Watchdog timeouts**: Replaced the static 10-minute timeout with dynamic, operator-specific connection timeout thresholds:
  - **Airtel** (`SIM_PROFILE_AIRTEL`): 5 minutes (`300` seconds)
  - **BSNL** (`SIM_PROFILE_BSNL`): 2 minutes (`120` seconds)
  - **VI** (`SIM_PROFILE_VI`): 10 minutes (`600` seconds)
- **Result**: Momentary signal spikes will no longer reset the watchdog. The device will cleanly fail over to the next carrier profile after the specific operator window has expired if no stable connection is achieved.

---

## 15. UFS Disk Space Safety Reserve and Automated History Purging

### 1. The Issue (Disk Exhaustion Blocking FOTA)
- **Problem**: The Quectel M66 module allocates a very small UFS partition (256 KB to 512 KB) for user file storage. Under prolonged periods of network disconnection, the device accumulates history tracking packets (each saved as a separate file like `BK_*.bin`). Because of filesystem sector overhead (4 KB minimum per file), storing even 40-50 packets completely exhausts the UFS disk space (`0 MB Free`).
- **Impact**: When the disk is 100% full, FTP downloads of FOTA firmware binaries fail due to insufficient space. `FTP_CleanupDiskSpace()` is selective to protect history where possible; `CLR DISK` is the deliberate field-service command that clears recoverable history and batch data to reclaim space.

### 2. Implemented Fixes
- **UFS Safety Reserve Watchdog (150 KB Reserve)**: Modified `SavePacket()` inside [custom/PktSave.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/PktSave.c#L204) to monitor free storage before writing any tracking packets. If free space is less than `153,600` bytes (150 KB), the oldest history packet is deleted (FIFO rollover) until the reserve is recovered. This guarantees that UFS never reaches 0 MB free space.
- **FOTA Cleanup Purge**: Updated `FTP_CleanupDiskSpace()` inside [custom/FTP.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/FTP.c) to dynamically purge the oldest history files. If `PROTO_CDAC` is defined, it clears the CDAC batch storage via `ClearFileTable()`. Otherwise, it purges the oldest history files in bulk via `DeleteFirstPacketsBulk(packetsToDelete)` to reclaim enough space for the requested FOTA binary size (with a 50 KB safety margin).
- **Header Declarations**: Declared public functions `DeleteFirstPacketsBulk` and `GetPacketCount` in [custom/inc/PktSave.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/PktSave.h).

---

## 16. Rajasthan Protocol (NIC_RAJASTHAN) and Network/Battery Settings

### 1. Firmware Version Bump
- **Change**: Bumped `FIRMWAREVERSION` from `"1.5.7"` to `"1.5.8"` inside [custom/inc/VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h#L55).

### 2. Rajasthan Operator Configuration (`NIC_RAJASTHAN`)
- **Macro Switch**: Disabled the default `NIC_VAHAN` compilation macro and enabled `NIC_RAJASTHAN` inside [custom/inc/VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h#L198-L202).
- **IP Endpoints**: Configured the target operator endpoints for the Rajasthan VLT System:
  - **IP1 (Primary)**: `vltspvt.rajasthan.gov.in` (Port `9031`)
  - **IP2 (Emergency)**: `vltsemg.rajasthan.gov.in` (Port `9032`)
  - **IP3 (Secondary / Diagnostic)**: `13.234.160.106` (Port `8224`)
  - **IP4 (Testing/Backup)**: `78.46.190.117` (Port `50011`)
- **Carrier Profile Auto-switching**: Handled automatically in the `PRF_AUTOSWITCH` profile switching loop.

### 3. Battery Voltage Upper Limit Modification
- **Threshold Adjustment**: Changed the maximum expected battery level `MAX_BATT` from `4.0f` to `4.2f` in [custom/inc/VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h#L106) to accurately account for fully charged Li-ion backup battery ranges and avoid false alerts.

---

## 17. GPS Module Support Improvements (my_rand Redefinition & Fault Reset Bypass)

### 1. Custom LCG Random Number Generator
- **Root Cause**: The standard Quectel library `Ql_rand()` function has been deprecated or behaves inconsistently on certain modules, causing build warnings and log spam.
- **Fix Applied**: Defined a local Linear Congruential Generator (LCG) random function `my_rand()` and redefined `Ql_rand` to use it in [custom/GPS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/GPS.c#L8):
  ```c
  static uint32_t s_rand_seed = 12345;
  static s32 my_rand(void)
  {
      s_rand_seed = s_rand_seed * 1103515245 + 12345;
      return (s32)((s_rand_seed / 65536) % 32768);
  }
  #define Ql_rand my_rand
  ```

### 2. GPS Fault Reset Routine & Log Suppression
- **Bypass Conditions**: In [custom/GPS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/GPS.c#L1091), the hardware `gps_reset_routine` now suppresses unnecessary fault log prints when GPS Simulation is active or FOTA/MOTA updates are in progress.
- **System State & GETVSTATUS Suppression**:
  - Suppressed reporting of `GPS: FLT` or `GPS FLT` text strings when GPS simulation mode (`CGPS` or `FGPS`) is running.
  - Applied inside `SystemStateSend` in [custom/Hardware.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Hardware.c#L406) and `DecodeSMS` parser (specifically `GETVSTATUS` response) in [custom/SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c#L1797).
