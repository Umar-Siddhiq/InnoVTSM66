# Master Project Memory & Overall Fixes History: InnoVTSM66

## Project Overview
- **Repository**: `InnoVTSM66` (Quectel OpenCPU M66 ARM7TDMI + Nuvoton M031 MCU Co-Processor)
- **Primary Protocol**: `PROTO_OG` (AIS-140 Amendment 3 / AMD3 Compliance)
- **Firmware Target**: `FQ_OG1_1.5.8` / `APPGS3MDM32A01.bin`

---

## Complete Historical Audit of All Fixes & Features (Git History & Codebase)

### 1. History Transmission & Packet Processing Fixes (Commit 2026-08-19)
- **Problem**: When TCP Socket 0 was offline, normal telemetry packets (`$PER`, `$GPD`, `$CEL`, `$INF`, `$NR`) were saved to flash (`BK_*.bin`). When Socket 0 reconnected, `ProcessHistoryPacket()` in `custom/Server.c` evaluated `if (!Ql_strstr(dataBuffer, "$PVT") || Ql_strlen(dataBuffer) < 150)`. Because `PROTO_OG` packets do not contain `$PVT` and are < 150 bytes, the firmware logged `Invalid Hitory Packet, deleting...` and deleted all saved history packets from flash without transmitting them to the server.
- **Fix**: Updated `ProcessHistoryPacket()` in [`Server.c`](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c#L4804) for `PROTO_OG` to validate standard telemetry packet headers (`$PER`, `$GPD`, `$CEL`, `$INF`, `$NR`, `$PVT`, `$EPB`) with length > 10. History packets now transmit cleanly to Server 1 (and Server 2 mirror) upon reconnection.

### 2. Reset Command (`$018:1`) Execution & Deferred Reset Safeguard (Commit 2026-08-19)
- **Problem**: Command `$868329088549211,549211,,SET,018:1*69` arriving over TCP socket deferred reset via `AIS140ResetPending = 2` in `custom/SMS.c` to allow sending the `$OA,12` reply first. In `custom/Server.c`, deferred reset execution was gated behind `responseSocket->SocketState == SOCKET_CONNECTED`. If the socket dropped or timed out during response framing, `AIS140ResetPending` was never executed or cleared, wedging the reset request permanently.
- **Fix**: Added fallback reset handling in [`Server.c`](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c#L5364). If `LastOTAResponse.Pending` times out or the socket is disconnected/unreachable, `AIS140ResetPending` executes `SystemRecovery_RequestReset("SMS_RESTART")` unconditionally.

### 3. Server OTA & Socket Handle Leak Fixes (Commit 2026-08-19)
- **Problem**: In `custom/TCP.c`, repeated socket creation calls after IP/Port reconfigurations (`SETSERVER1`) failed with `TCP: Socket 0 create failed` due to unclosed socket descriptors exhausting OpenCPU OS socket handles.
- **Fix**: Added an explicit pre-creation cleanup in [`TCP.c`](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/TCP.c#L327) (`if (socket->SocketIndex >= 0) Ql_SOC_Close(...)`) before `Ql_SOC_Create()`, preventing handle leaks and ensuring reliable socket re-establishment for Server OTA data.

### 4. 16MB Flash Storage Target & Footprint Optimization (Commit 4e319dca - 2026-08-18)
- **Feature Switch**: `#define FLASH_STORAGE_16MB` in `custom/config/custom_feature_def.h`.
- **Behavior**:
  - Automatically defines `HISTORY_DISABLED` to bypass flash wear on 16MB hardware.
  - Sets `VTS_DEBUG_LOG_ENABLE = 0` to silence UART debug trace logging.
  - Keeps RS232 interfaces 100% active and untouched.
- **Footprint**: Reduces ROM binary size from 342 KB down to 187 KB (`APPGS3MDM32A01.bin`).
- **Dynamic Memory Calculation**: Updated `custom/PktSave.c` to calculate free space using `Ql_FS_GetTotalSpace(Ql_FS_UFS)`.
- **External MCU Layout**: Updated Nuvoton M031 MCU SPI flash base address `FW_FLASH_ADDR_BASE` to `0x1F0000` for 2MB W25Q16 chip layout.

### 5. FTP / FOTA Cleanup System & Storage Safety (Commit 65cb8c40 & 5249dea3 - 2026-08-10)
- **Implementation**: Refactored `FTPStart()` in `custom/FTP.c` and `custom/PktSave.c`.
- **Storage Sweeping**: Clears `BK_*.bin` history files, `BkCount.bin`, batch files, and `FOTA_*.bin` prior to downloading firmware updates.
- **Stall Protection**: Added progress logging (`ClearHistoryStorage: sweeping 1..256`) to prevent hidden thread stalls during flash sweeps.

### 6. Nuvoton M031 MCU Co-Processor Subsystem (Commit 4e319dca - 2026-08-18)
- **Location**: `QuectelM031-main/`
- **Architecture**: Dual-chip design where Quectel M66 handles cellular GSM/GPRS & GPS, and Nuvoton M031 MCU handles low-power vehicle IOs, LSM6DSOX accelerometer, and hardware watchdog interfacing.
- **MOTA Protocol**: Microcontroller Over The Air update system over SPI/UART interface.

### 7. AIS-140 AMD3 Protocol & Security Architecture (Commit 4e319dca - 2026-08-18)
- **Parser**: `ParseStandardAIS140Command()` in `custom/SMS.c` supports 23 standard commands (`GET`, `SET`, `CLR`).
- **Security Validation**:
  - Layer 1: IMEI check against `NetWork.IMEI`.
  - Layer 2: Password verification against last 6 digits of IMEI.
  - Layer 3: XOR checksum calculation over frame payload.
- **Ack Framing**: Constructs dedicated `$OA,12` response frames or embeds responses into `$PVT` `OTAResp` fields.

### 8. Sensors Subsystem & Peripheral Integration (Commit a2921bef - 2026-07-25)
- **Sensors**: `custom/Sensors.c` handles RFID tag reading, fuel sensors, temperature sensors, and BLE peripherals.
- **Serial Protocol**: `RS232_INTERFACE_SPEC.md` defines frame structure (`0x26` header, command ID, payload, `0x7E` trailer) for MCU-to-OpenCPU serial communication.

### 9. IMEI Handling & Network Stability Fixes (Commit 3ef86974 - 2026-06-26)
- **Fix**: Refactored IMEI retrieval fallback when SIM/Network initialization is pending. Resolved null-pointer dereference issues in network status checks.

### 10. FOTA & MOTA Dual Update Architecture (Commit 3b62ad46 - 2026-06-24)
- **FOTA**: OpenCPU firmware update over GPRS via FTP (`__OCPU_FOTA_BY_FTP__`).
- **MOTA**: Nuvoton M031 MCU firmware update via OpenCPU UART/SPI bridge.
- **Safety**: Bypasses history saving during active FOTA/MOTA sessions.

### 11. System Recovery & Multi-Layer Watchdog System
- **Recovery Manager**: `custom/SystemRecovery.c` tracks system stability across 4 watchdogs:
  - Main task deadlock watchdog (5000ms timeout, 500ms feed).
  - GPRS connection stall watchdog (180s stall timeout).
  - Server connection timeout watchdog (2 hours without server communication).
  - GPS fault state watchdog (3 minutes without valid GPS lock).
- **Diagnostics**: Preserves boot reason codes and reset counters across unexpected power losses and watchdog resets.
