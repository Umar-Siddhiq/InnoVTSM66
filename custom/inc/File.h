#ifndef _FILE_H
#define _FILE_H

#include "VTS.h"
#include "Hardware.h"
#include "Utilities.h"
#include "ql_fs.h"
#include "Systic.h"
#include "Server.h"
/* DIAGNOSTIC LOGGING SYSTEM — Added 2026-05-13
 * Diag.h is included here so any module that includes File.h automatically
 * gets PushDiagEvent() and DIAG_EVT_* without an extra include.
 * Diag.h has no includes of its own so this adds zero include-chain risk. */
#include "Diag.h"

#ifndef TEST
#define     DEFVAL                  0xA5A2
#else
#define     DEFVAL                  0xB5C1
#endif

#define     DEFSTATE               0xA4A1


#define     CONFIG_FILE_PATH        "UFSConfig.bin\0"
#define     FOTA_CONFIG_PATH        "FotaConfig.bin\0"
#define     STATE_FILE_PATH         "State.bin\0"
#define     ACTIVE_PROFILE_FILE_PATH "ActiveProfile.bin\0"

#define     ACTIVE_PROFILE_REASON_NONE    0
#define     ACTIVE_PROFILE_REASON_GPRS    1
#define     ACTIVE_PROFILE_REASON_SERVER  2

/* -----------------------------------------------------------------------
 * DIAGNOSTIC LOGGING SYSTEM — Flash file path
 * Added : 2026-05-13
 *
 * Diag.bin is a SEPARATE file from State.bin and UFSConfig.bin.
 * It stores DiagCountersTypedef (event counters + ring buffer).
 * Corruption or absence of Diag.bin calls LoadDefaultDiag() which
 * zeros all counters and creates a fresh file — it does NOT touch
 * State.bin or UFSConfig.bin. Completely safe to add to existing devices.
 * ----------------------------------------------------------------------- */
#define     DIAG_FILE_PATH          "Diag.bin\0"
#define     LAST_GPS_FILE_PATH      "LastGPS.bin\0"


void LoadConfig(void);
void LoadState(void);
void LoadDefault(void);
void LoadDefaultState(void);
void UpdateConfigInFlash(void);
void UpdateStateInFlash(void);
uint8_t LoadFromFlash(char *filename, void *data, u32 size, void (*defaultFunc)(void));
uint8_t SaveToFlash(char *filename, void *data, u32 size);

/* Last active SIM profile persistence.
 * This is intentionally stored in ActiveProfile.bin, not VTSStateTypedef, so
 * existing State.bin files keep the same size after firmware upgrade.
 * "Active" means the profile reached GPRS IP or a server socket connected. */
void LoadActiveProfile(void);
void LoadDefaultActiveProfile(void);
uint8_t HasLastActiveProfile(void);
uint8_t GetLastActiveProfile(void);
uint8_t MarkActiveProfile(uint8_t profile, uint8_t reason);
uint16_t GetActiveProfileTimeout(void);
void SetActiveProfileTimeout(uint16_t timeoutSec);
void ResetActiveProfileTimeout(void);
uint8_t SaveActiveProfileState(void);

/* -----------------------------------------------------------------------
 * DIAGNOSTIC LOGGING SYSTEM — API
 * Added : 2026-05-13
 *
 * SaveDiagToFlash()   - persist DiagCounters to Diag.bin
 * LoadDiag()          - load Diag.bin; calls LoadDefaultDiag() on error
 * LoadDefaultDiag()   - zero all counters, write fresh Diag.bin
 * PushDiagEvent()     - append one event to the ring buffer + save flash
 *
 * Call LoadDiag() once at boot (alongside LoadConfig/LoadState).
 * Call PushDiagEvent() wherever a significant event occurs.
 * ----------------------------------------------------------------------- */
/* PushDiagEvent() and DIAG_EVT_* constants are declared in Diag.h (included above).
 * SaveDiagToFlash, LoadDiag, LoadDefaultDiag are File.c internal — declared below. */
void SaveDiagToFlash(void);
void LoadDiag(void);
void LoadDefaultDiag(void);
/* Diag.h interface helpers (implemented in File.c, declared in Diag.h):
 *   void PushDiagEvent(...)
 *   void Diag_OnBoot(...)
 *   unsigned long Diag_GetWdtResetCount(void)
 *   unsigned long Diag_GetBootCount(void)
 * No re-declaration needed here — Diag.h already covers them. */


#endif /* _FILE_H */