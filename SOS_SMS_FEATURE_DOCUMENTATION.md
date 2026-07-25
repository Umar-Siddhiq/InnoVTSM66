# SOS SMS Feature & SOSDISABLE Documentation

## Overview

The firmware supports protocol-independent SOS alerts and notifications (CDAC and non-CDAC protocols). It includes:

- **SOS SMS Notifications**: Sends short, single-part SMS alerts for SOS ON, SOS OFF, and Wire-cut/Tamper events to configured numbers.
- **SOS Disable Control (`SOSDISABLE`)**: Master device-level toggle to completely enable or disable all physical SOS button and tamper monitoring on the hardware.
- **SOS SMS Control (`SOSSMS`)**: Dynamic SMS control to turn SMS alert delivery on or off.
- **Firmware Compile Switches**: Permanent compile-time controls to override flash configurations when required.

---

## 1. Alert IDs & Types

| Alert ID | Alert Name | Trigger |
| --- | --- | --- |
| `10` | Emergency State ON | SOS button pressed (min push duration met) |
| `11` | Emergency State OFF | SOS timeout reached or cleared manually |
| `16` | Emergency wirecut | SOS line wire-cut / tamper detected |

---

## 2. Master SOS Disable Control (`SOSDISABLE`)

The device-level switch `SOSDISABLE` controls whether the firmware processes physical SOS button presses and tamper inputs.

### SMS / Server Command
- **Enable SOS System**:
  ```text
  SET SOSDISABLE 0
  ```
  Response: `SOS Disable set to 0`

- **Disable SOS System**:
  ```text
  SET SOSDISABLE 1
  ```
  Response: `SOS Disable set to 1`

- **Query Status**:
  ```text
  GET SOSDISABLE
  ```
  or
  ```text
  SOSDISABLE
  ```
  Response: `SOS Disable: 0` (or `1`)

### Firmware Execution Logic
- Configured via field `VTSData.DisableSOS` in `VTSTypedef` ([VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h#L592)).
- Managed inside `ProcessSOS()` in [custom/SOS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SOS.c#L78):
  - When `VTSData.DisableSOS == 1`, `ProcessSOS()` immediately clears any active SOS or Tamper states, turns off the SOS LED (`SLED_OFF`), and skips reading input pins.

---

## 3. SOS SMS Alert Delivery (`SOSSMS`)

SOS SMS delivery is independently controlled by configuration and compile switches.

### SMS / Server Command
- **Enable SOS SMS**:
  ```text
  SET SOSSMS 1
  ```
  Response: `SOS SMS Enabled`

- **Disable SOS SMS**:
  ```text
  SET SOSSMS 0
  ```
  Response: `SOS SMS Disabled`

- **Query Status**:
  ```text
  GET SOSSMS
  ```
  Response:
  - `SOS SMS: ON` (if enabled)
  - `SOS SMS: OFF` (if disabled)
  - `SOS SMS: DISABLED IN FIRMWARE` (if master compile switch is `0`)

- **Firmware Disabled Response**:
  If permanently disabled in code via `#define SOS_SMS_FEATURE_ENABLED 0`, sending `SET SOSSMS 1` will return:
  ```text
  SOS SMS Disabled In Firmware
  ```

---

## 4. Configure Recipient Numbers (`SOSSET`)

- **Command**:
  ```text
  SET SOSSET 9600696008,9655543732
  ```
  Response: `SOS Number Updated : +919600696008, +919655543732`

- Updates `Mob0` and `Mob1` in `VTSData.PhoneNumber`.
- 10-digit mobile numbers are automatically formatted with country code `+91`.
- Dummy numbers (e.g., `0000000`) are automatically skipped during SMS dispatch.

---

## 5. Compact SOS SMS Payload Format

To prevent multipart SMS delivery failures on mobile networks, the SMS payload is formatted to fit in a single SMS message (under 160 characters):

```text
ID:10 Emergency State ON
IMEI:861850061702714
LAT:12.345678N LON:77.123456E
http://maps.google.com/?q=12.345678,77.123456
T:15-06-2026 18:22:10 IST
```

- Converts UTC device timestamp to IST (+5:30) before appending.
- Includes direct Google Maps URL for immediate location tracking.

---

## 6. Compile-Time Switches ([custom/inc/VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h))

```c
/*
 * Set to 0 for builds where SOS SMS must remain disabled even when an old
 * flash configuration has the feature enabled. Set to 1 to permit the
 * SET SOSSMS command to control the persisted setting.
 */
#define SOS_SMS_FEATURE_ENABLED 0

/* Tamper/wire-cut SMS is deliberately independent of normal SOS SMS. */
#define SOS_WIRECUT_SMS_ENABLED 0

#define SOS_SMS_CONFIG_SIGNATURE 0x534F53534D533031ULL
```

- **`SOS_SMS_FEATURE_ENABLED`**: When set to `0`:
  - `SendSOSAlertSMS()` immediately returns without sending SMS.
  - `SET SOSSMS 1` cannot enable SOS SMS.
  - `GET SOSSMS` reports `SOS SMS: DISABLED IN FIRMWARE`.
  - `LoadConfig()` forces `VTSData.SOSSmsEnabled` to `0`.
- **`SOS_WIRECUT_SMS_ENABLED`**: Controls Alert ID `16` wire-cut SMS notifications independently without affecting SOS ON (`10`) or SOS OFF (`11`) alerts.

---

## 7. Flash Memory Configuration & Migration ([custom/File.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/File.c))

- Config structure fields:
  ```c
  uint8_t DisableSOS;
  uint8_t SOSSmsEnabled;
  uint64_t SOSSmsConfigSignature;
  ```
- **Migration**: When updating firmware via FOTA from older versions without `SOSSmsEnabled`, `LoadConfig()` checks the signature and file size (`legacySize`), preserving existing settings while safely initializing `SOSSmsEnabled` based on `SOS_SMS_FEATURE_ENABLED`.

---

## 8. Summary of File Locations

| Module | Location | Description |
| --- | --- | --- |
| Header & Macros | [custom/inc/VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h#L590-L610) | `VTSTypedef` struct & compile switch macros |
| Helper Function Header | [custom/inc/SMS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/SMS.h#L28) | Declares `SendSOSAlertSMS(uint8_t AlertNum)` |
| SMS Commands & Formatting | [custom/SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c#L79) | `SendSOSAlertSMS()`, `SOSSMS`, `SOSDISABLE`, `SOSSET` handlers |
| Button & Tamper State Machine | [custom/SOS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SOS.c#L78) | `ProcessSOS()` hardware state machine and `DisableSOS` check |
| CDAC Protocol Dispatch | [custom/Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c#L2582) | Routes alert numbers 10, 11, 16 in `SMSAlert()` |
| Flash Load & FOTA Migration | [custom/File.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/File.c#L185) | `LoadDefault()` & `LoadConfig()` flash migration logic |
