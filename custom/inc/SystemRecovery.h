#ifndef __SYSTEM_RECOVERY_H__
#define __SYSTEM_RECOVERY_H__

#include "ql_type.h"

/* Task Health Watchdog Gating - Added 2026-05-25 */
typedef enum {
    WDT_TASK_MAIN = 0,
    WDT_TASK_GPRS,
    WDT_TASK_SYSTIC,
    WDT_TASK_MAX
} WdtTaskId;

void SystemRecovery_CheckInTask(WdtTaskId taskId);

/* ------------------------------------------------------------------
 * SystemRecovery_EarlyWatchdogStart
 * DATE  : 2026-05-16
 *
 * MUST be called from proc_main_task() BEFORE the while(TRUE) message
 * loop. This is the ONLY valid call site for Ql_WTD_Start() on the M66.
 *
 * ROOT CAUSE: Ql_WTD_Start() returns QL_RET_ERR_PARAM (-1) when called
 * from inside a message handler (MSG_ID_RIL_READY etc.) because the M66
 * OS is in "message processing" state at that point. ALL intervals fail,
 * including values as small as 1000ms. The official Quectel example
 * (example_watchdog.c) calls Ql_WTD_Start() before Ql_OS_GetMessage().
 *
 * OLD CODE: Ql_WTD_Start() was called inside SystemRecovery_Init() which
 * runs from MSG_ID_RIL_READY — the wrong context. Every interval failed.
 * ------------------------------------------------------------------ */
void SystemRecovery_EarlyWatchdogStart(void);
void SystemRecovery_Init(void);
void SystemRecovery_LogBootReason(void);
void SystemRecovery_LogFieldStatus(const char *source);
void SystemRecovery_StartWatchdog(void);
void SystemRecovery_FeedWatchdog(void);
void SystemRecovery_StopWatchdog(void);
void SystemRecovery_OnVoltageInd(u32 voltageInd);
void SystemRecovery_RequestReset(const char *reason);
bool SystemRecovery_IsWatchdogStarted(void);
bool SystemRecovery_IsVoltageCritical(void);

/* TEST ONLY:
 * Controlled watchdog fault injection for field validation.
 * These are one-shot runtime tests triggered by explicit SMS/RS232 commands.
 */
#define WDT_TEST_NONE          0
#define WDT_TEST_FEED_STOP     1
#define WDT_TEST_SYSTIC_BLOCK  2
#define WDT_TEST_RED_STUCK     3

void SystemRecovery_ArmWatchdogTest(u8 mode, u32 delayMs);
void SystemRecovery_CancelWatchdogTest(void);
void SystemRecovery_GetWatchdogTestStatus(char *out, u32 outLen);

#endif
