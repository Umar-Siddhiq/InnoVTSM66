# Walkthrough - InnoVTSM66 System Integrations

This document walks through the modifications made to build both the **FTP/FOTA Storage/Cleanup** system and the **Sensors Subsystem (RFID, Fuel, CLS)** integration.

---

## 1. FTP/FOTA UFS Storage and Disk Space Cleanup

We restored persistent UFS storage for FTP/FOTA downloads, added pre-download automated disk space cleanup, and implemented a manual remote disk clean command via SMS.

### Changes Made

- **[FTP.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/FTP.h)**: Removed the `FTP_FILE_RAM` toggle. Reverted `SERVER_FOTA_FILEPATH` to `"app.bin"` and `SERVER_MOTA_FILEPATH` to `"mcu.bin"`.
- **[FTP.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/FTP.c)**: `FTP_CleanupDiskSpace(uint32_t requiredSize)` remains the selective FOTA pre-download helper. `FTP_ClearRecoverableDiskData()` is the dedicated manual-clear path: it removes recoverable history, batch storage, and stale download artifacts while preserving configuration/state and pending FOTA configuration.
- **[SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c)**: Added decoding logic inside `DecodeSMS()` for remote diagnostics:
  * `CLR DISK`: Clears recoverable storage, refuses to run during active FOTA/MOTA, and reports free-space plus removal counts. It does not format UFS.
  * `GET DISK`: Queries and returns remaining free space.

---

## 2. Sensors Subsystem Integration (RFID, Fuel, Ultrasonic CLS)

We fully integrated the sensor subsystem which parses wrapped serial frames from the NuMicro MCU, formats regular/interrupt GPRS packet strings, implements persistent storage isolation, and supports administrative query/set SMS commands.

### Components Added/Modified

- **[Sensors.h](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/inc/Sensors.h)** [NEW]: Defines the API methods, enum typings, data buffers, and maps `SENSOR_CONFIG_PATH` to `"sensor_cfg.bin"`.
- **[Sensors.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Sensors.c)** [NEW]: Contains the complete sensor data handling logic:
  * `ParseSensorData()`: Parses raw serial frames (RFID hex numbers, standard Fuel delimiters, Ultrasonic CLS queries).
  * `MakeSensorPacket()`, `MakeInterruptPacket()`: Compiles `$SENS` and `$SINT` protocol payloads.
  * `SaveSensorConfigToFlash()`, `LoadSensorConfigFromFlash()`: Wraps around `SaveToFlash` and `LoadFromFlash` to isolate config data.
  * `QueryCLSSensor()`: Transmits the `@01E0006#` polling query.
  * `ParseCLSResponse()`: Inspects checksum and format of the Ultrasonic sensor feed.
- **[SMS.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/SMS.c)** [MODIFY]: Intercepts incoming serial messages from the MCU (`OTA_SRC_RS232`) to feed them to the parser. Implements diagnostic SMS commands:
  * `GET SENS`
  * `SET SENSINT <DayIGN,NightIGN,DayOFF,NightOFF>`
  * `SET SENSEN <0/1>`
  * `SET SENSTOUT <seconds>`
- **[Systic.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Systic.c)** [MODIFY]: Replaced the static sensor tick checker with the dynamic time-of-day interval query. Added the 3-second CLS query offset to poll the Ultrasonic CLS sensor exactly 3 seconds prior to the `$SENS` report transmission.
- **[Server.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/Server.c)** [MODIFY]: Declared `handleSensorData()` helper and integrated its execution inside the `ServerThreadEntry()` loop.
- **[main.c](file:///d:/QUICKTEL/InnoVTSM66_VY/InnoVTSM66/custom/main.c)** [MODIFY]: Called `InitSensors()` and `LoadSensorConfigFromFlash()` inside `system_init()` to read configuration on boot.

---

## 3. Verification Results

### Clean Compilation Check
We executed a complete rebuild of the project workspace:
```powershell
.\Make.bat new
```

**Output**:
```text
----------------------------------------------------
- GCC Compiling Finished Sucessfully.
- The target image is in the 'build\gcc' directory.
----------------------------------------------------
```
The codebase compiles cleanly with no errors, producing the final flashable application binary file.
