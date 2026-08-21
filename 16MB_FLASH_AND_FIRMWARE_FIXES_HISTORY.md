# 16MB Flash Storage Migration and Firmware Fixes History

## Project: InnoVTSM66 (Quectel OpenCPU M66) & QuectelM031-main (Nuvoton M031 MCU)
**Date**: 2026-08-18

---

### 1. OpenCPU 16MB Flash Build Target (`custom/config/custom_feature_def.h`)
* **Feature Switch**: `#define FLASH_STORAGE_16MB` in `custom/config/custom_feature_def.h`.
* **Automated Behaviors**:
  * Defines `HISTORY_DISABLED` (disables UFS `BK_*.bin` history backup files).
  * Sets `VTS_DEBUG_LOG_ENABLE = 0` (disables UART debug trace logs).
  * **RS232 Untouched**: RS232 communications (`SendRS232Response`, `SendRS232String`) remain 100% active.
* **Firmware Size**: Reduces binary size from ~342 KB down to **187 KB** (`APPGS3MDM32A01.bin`), well within the 360 KB OpenCPU ROM ceiling.

---

### 2. Dynamic UFS Memory Percentage (`custom/PktSave.c`)
* **File**: `custom/PktSave.c`
* **Fix**: Replaced hardcoded `freeSpace / 7000` divisor with dynamic calculation:
  `MemoryPercent = (totalSpace > 0) ? (int)((freeSpace * 100) / totalSpace) : 0;` using `Ql_FS_GetTotalSpace(Ql_FS_UFS)`.

---

### 3. Nuvoton M031 MCU 16MB SPI Flash Layout (`QuectelM031-main`)
* **MCU OTA Base Address (`FW_FLASH_ADDR_BASE`)**:
  * Changed from `0x7F0000` (8MB top sector) to **`0x1F0000`** (2MB top sector for W25Q16 chip) in `Application/Header/MOTA.h` and `BootLoader/Header/W25Qxx.h`.
* **Flash Page Count (`W25Q_PAGES`)**:
  * Changed from `0x8000` (32,768 pages = 8MB) to **`0x2000`** (8,192 pages = 2MB) in `Application/Header/W25Qxx.h` and `BootLoader/Header/W25Qxx.h`.

---

### 4. Keil µVision Compiler LTO Error Fix (`QuectelM031-main`)
* **Error Resolved**: `error: use of LTO is disallowed in this variant of ARM Compiler`.
* **Fix**: In `Application/KEIL/Application.uvproj`, set `<v6Lto>0</v6Lto>` and `<AdsLtoi>0</AdsLtoi>`.
* **Build Verification**: Compiled cleanly via `build_app.bat` (0 Errors).

---

### 5. Dynamic Bluetooth / BLE IMEI Broadcast Update (`custom/BLE.c`)
* **Root Cause**: `BLE_Init()` ran at early boot before modem fetched `NetWork.IMEI`, causing Bluetooth device name to remain stuck as `VENDOR-NOIMEI`.
* **Fix**: In `ProcessBLE()`, added dynamic check that detects when `NetWork.IMEI` is populated and updates the Bluetooth device name via `RIL_BT_SetName()` with the 15-digit IMEI.
