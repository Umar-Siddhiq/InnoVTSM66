# Geofence Alert Packets & History Storage Control Documentation

## Overview

This documentation details the implementation of real-time Geofence In/Out device alert packet transmission and offline history packet storage enable/disable controls.

---

## 1. Geofence Device Alert Packets

Real-time Geofence alert packets are transmitted directly to the server socket.

### Alert ID Definitions
- **Geofence In**: `GFIN_ALERT` (ID `13`)
- **Geofence Out**: `GFOUT_ALERT` (ID `14`)

### Automatic Subsystem Trigger
When entering or exiting configured geofence boundaries, transitions are monitored inside [custom/Geofence.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Geofence.c):
- Boundary entry triggers `AddAlert(GFIN_ALERT)`.
- Boundary exit triggers `AddAlert(GFOUT_ALERT)`.

This ensures real-time alert packets are generated and dispatched to the server socket immediately.

### Manual SMS Trigger
For testing or manual triggering, dispatch any of the following format variations:
- **Trigger Geofence In Device Alert**:
  ```text
  TESTRIG 13
  SETTESTRIG 13
  SETTESRIG 13
  TESRIG 13
  ```
- **Trigger Geofence Out Device Alert**:
  ```text
  TESTRIG 14
  SETTESTRIG 14
  SETTESRIG 14
  TESRIG 14
  ```

---

## 2. History Storage & Transmission Control (`HISTORY`)

To manage storage space and network overhead, the storing and reading of offline packets can be dynamically controlled via SMS.

### SMS Commands

- **Enable Offline History Saving**:
  ```text
  SET HISTORY 1
  ```
  or
  ```text
  SET HIST 1
  ```
  Response: `History Enabled`

- **Disable Offline History Saving**:
  ```text
  SET HISTORY 0
  ```
  or
  ```text
  SET HIST 0
  ```
  Response: `History Disabled`

- **Query History Status**:
  ```text
  GET HISTORY
  ```
  or
  ```text
  GET HIST
  ```
  Response: `History: ENABLED` (or `History: DISABLED`)

- **Clear Offline History Storage**:
  ```text
  CLR HISTORY
  ```
  or
  ```text
  CLEAR HISTORY
  ```
  Response: `History Cleared`

---

## 3. Implementation Details & Architecture

### Configuration Field
- Stored as `uint8_t DisableHistory;` inside `VTSTypedef` ([custom/inc/VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h#L594)).
- Initialized to `0` (*History Enabled*) on device default reversion (`LoadDefault()`) in [custom/File.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/File.c#L187).

### Suppression Mechanism
- **History Save Check**: Inside [custom/Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c), before calling `SavePacket()` (when `HISTORY_INTERNAL` is active), the system checks:
  ```c
  if(!VTSData.DisableHistory) {
      // Save history data
  }
  ```
- **History Send Check**: Inside `ProcessHistoryPacket()` in [custom/Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c#L4581), if `VTSData.DisableHistory == 1`, the function exits immediately without reading or sending history packets.
- **Offline Alerts History Save**: When the device is offline (Server 1 disconnected), `SaveOfflineAlerts()` in [custom/Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c#L4238) formats any pending alerts (such as geofence transitions GFIN/GFOUT, harsh acceleration/braking, box tamper, etc.) using `InitBuffer()` and converts them using `ChangeToHistoryPacket(dataBuffer)` before writing to flash via `SavePacket()`.
- **Formatting directly on Save**: Regular packets and alerts are passed through `ChangeToHistoryPacket()` (or `ChangeToHistoryEPB()`) prior to calling `SavePacket()`, meaning packets are stored directly with history formatting (`NR,02` and `H` indicators) inside flash.
- **Real-time Status Count**: The `GETVSTATUS` response in [custom/SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c#L1813) queries current stored backup count directly from flash via `CheckPacketCount()` and prints `PacketConfig.LastPkt`, ensuring precise statistics reporting.

### Compile Flags Configured
- `HISTORY_DISABLED` has been commented out in [custom/inc/VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h#L38) to enable compilation of history components.
- `HISTORY_INTERNAL` has been uncommented in [custom/inc/VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h#L40) to compile the local Quectel M66 UFS flash module rather than trying to send external MCU packets.

---

## 4. Summary of Affected Code Files

| File | Section | Change Description |
| --- | --- | --- |
| [custom/inc/VTS.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/VTS.h) | `VTSTypedef` | Added `DisableHistory` configuration field |
| [custom/File.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/File.c) | `LoadDefault()` | Initialized `DisableHistory = 0` |
| [custom/Geofence.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Geofence.c) | Boundary Check | Invokes `AddAlert()` directly on Geofence IN/OUT transitions |
| [custom/SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c) | command parser | Added `HISTORY`/`HIST` command handling & unconditionally triggers `AddAlert` for Geofence in `TESTRIG` |
| [custom/Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c) | History process | Checks `!VTSData.DisableHistory` before saving/sending history |
