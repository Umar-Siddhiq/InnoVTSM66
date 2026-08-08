#ifndef __DIAG_H__
#define __DIAG_H__

/* =======================================================================
 * Diag.h — Minimal Diagnostic Interface (No Includes / No bool)
 * Added : 2026-05-13
 * =======================================================================
 *
 * PURPOSE:
 *   This header provides the public API for the diagnostic logging system
 *   to modules that CANNOT include File.h due to circular include chains
 *   or bool-type redefinition conflicts:
 *
 *     SystemRecovery.c: File.h -> Systic.h -> GPRS.h -> SDK headers that
 *                        redefine 'bool' differently, conflicting with the
 *                        'bool' already in scope from SystemRecovery.h.
 *
 *     Systic.c:         File.h -> Systic.h  (circular — Systic.c IS Systic.h)
 *
 *   SOLUTION:
 *     This header has NO #include statements. It uses ONLY primitive C types
 *     (unsigned char, unsigned long, int, const char*). This guarantees zero
 *     include chain side-effects regardless of where it is included.
 *
 * USAGE:
 *   #include "Diag.h"    <- use this instead of #include "File.h"
 *   PushDiagEvent(DIAG_EVT_PRF_TIMEOUT, profile, 0, "12-min-timer");
 *
 * DIAG_EVT_* codes MUST match the DiagEventType enum values in VTS.h.
 * The implementation of all functions below is in File.c.
 * ======================================================================= */

/* -----------------------------------------------------------------------
 * Event type constants — primitive int literals (not enum) to avoid
 * any typedef dependency that would require an include.
 *
 * Match exactly: DiagEventType enum in custom/inc/VTS.h
 * ----------------------------------------------------------------------- */
#define DIAG_EVT_BOOT           0   /* device booted: p1=reason, p2=profile            */
#define DIAG_EVT_PRF_SWITCH     1   /* profile switched: p1=from, p2=to                */
#define DIAG_EVT_PRF_FAIL       2   /* profile switch failed: p1=profile, p2=fail_count */
#define DIAG_EVT_GSM_HANG       3   /* GSM hang detected: p1=CSQ, p2=0                 */
#define DIAG_EVT_GSM_RESET      4   /* AT+CFUN=1,1 sent: p1=reset_count, p2=0          */
#define DIAG_EVT_WDT_RESET      5   /* boot was watchdog reset: p1=reason, p2=0        */
#define DIAG_EVT_GPRS_FAIL      6   /* GPRS fail threshold: p1=profile, p2=fail_count  */
#define DIAG_EVT_REG_DENIED     7   /* registration denied: p1=profile, p2=CSQ         */
#define DIAG_EVT_STATE_CHG      8   /* GSM state change: p1=old_state, p2=new_state    */
#define DIAG_EVT_FLASH_ERR      9   /* flash error: p1=0, p2=0                         */
#define DIAG_EVT_PRF_TIMEOUT   10   /* 12-min timer fired: p1=profile, p2=0            */
#define DIAG_EVT_SYS_RESET     11   /* software reset: p1=0, p2=0                      */

/* -----------------------------------------------------------------------
 * PushDiagEvent
 *
 * Appends one entry to the persistent ring buffer (Diag.bin) and saves
 * to flash immediately. Implementation in File.c.
 *
 * Parameters use 'unsigned char' which is compatible with uint8_t
 * without requiring any include. const char* is always available in C.
 * ----------------------------------------------------------------------- */
void PushDiagEvent(unsigned char evtType,
                   unsigned char p1,
                   unsigned char p2,
                   const char   *detail);

/* -----------------------------------------------------------------------
 * Diag_OnBoot
 *
 * Call once at boot from SystemRecovery_LogBootReason().
 * Increments BootCount, records LastBootReason and LastProfile,
 * increments WatchdogResetCount if isWdtReset is non-zero, and
 * pushes the appropriate ring buffer event.
 *
 * Parameters:
 *   reason      — Ql_GetPowerOnReason() value (int, no enum needed)
 *   profile     — VTSState.CurrentProfile at boot time
 *   isWdtReset  — pass 1 if reason == QL_RESET_WDT, 0 otherwise
 *                 (the caller already knows QL_RESET_WDT, File.c need not)
 *   reasonName  — human-readable name string from PowerOnReasonName()
 * ----------------------------------------------------------------------- */
void Diag_OnBoot(int           reason,
                 unsigned char profile,
                 int           isWdtReset,
                 const char   *reasonName);

/* -----------------------------------------------------------------------
 * Diag_GetWdtResetCount / Diag_GetBootCount
 *
 * Safe reads of persistent counters for logging in modules that cannot
 * access DiagCounters directly (due to include restrictions).
 * Implementation in File.c.
 * ----------------------------------------------------------------------- */
unsigned long Diag_GetWdtResetCount(void);
unsigned long Diag_GetBootCount(void);

#endif /* __DIAG_H__ */
