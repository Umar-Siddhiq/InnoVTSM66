#include "SystemRecovery.h"
#include "Alert.h"
#include "GPRS.h"
#include "Hardware.h"
#include "custom_feature_def.h"
#include "LOG.h"

extern uint8_t GPS_GetState(void);
extern uint8_t GPS_IsSimulationActive(void);
extern uint8_t SleepConfig_IsEnabled(void);
extern uint8_t TCP_IsAnySocketConnected(void);
extern int MCOMM_SendSleep(uint16_t sleeptime);
/* DIAGNOSTIC LOGGING SYSTEM — Added 2026-05-13
 * Diag.h (NOT File.h) is used here to avoid circular include chain:
 *   File.h -> Systic.h -> GPRS.h -> SDK headers redefining 'bool'
 * which conflicts with 'bool' already in scope from SystemRecovery.h.
 * Diag.h has zero includes and uses only primitive C types. */
#include "Diag.h"
#include "ql_error.h"
#include "ql_power.h"
#include "ql_stdlib.h"
#include "ql_system.h"
#include "ql_wtd.h"
#include "ril_network.h"
#include "ril_system.h"

#if SYSTEM_WATCHDOG_ENABLE
#if SYSTEM_WATCHDOG_TIMEOUT_MS < 400
#error SYSTEM_WATCHDOG_TIMEOUT_MS must be at least 400 ms.
#endif
#if SYSTEM_WATCHDOG_FEED_MS < 200
#error SYSTEM_WATCHDOG_FEED_MS must be at least 200 ms.
#endif
#endif

/* Recovery/watchdog internals are compiled only when enabled. Current
 * production config disables them; direct reset fallbacks remain at call sites. */
#if SYSTEM_RECOVERY_ENABLE
static bool s_recoveryInitialized = FALSE;
#endif
static s32 s_watchdogId = -1;
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_WATCHDOG_ENABLE
static u64 s_lastWatchdogFeedMs = 0;
static bool s_watchdogServiceInitAttempted = FALSE;
static s32 s_watchdogServiceInitRet = QL_RET_ERR_PARAM;
/* ------------------------------------------------------------------
 * FIX: s_watchdogStartAttempted replaced with s_watchdogStartRetries
 * DATE  : 2026-05-14
 *
 * OLD PROBLEM:
 *   s_watchdogStartAttempted was set to TRUE BEFORE Ql_WTD_Start() was
 *   called. One failure → watchdog dead for the whole session, no retry.
 *
 * NEW BEHAVIOUR:
 *   s_watchdogStartRetries counts attempts made. StartWatchdog() tries
 *   the full interval fallback table (3000->2000->1200 ms) in the single
 *   valid pre-message-loop call site. FeedWatchdog() only feeds an already
 *   started WDT; it never retries startup from the wrong context.
 *
 * REVERT:
 *   Restore:  static bool s_watchdogStartAttempted = FALSE;
 *   Remove:   static u8 s_watchdogStartRetries = 0;
 *   Restore the original StartWatchdog() body (see comment block there).
 * ------------------------------------------------------------------ */
/* OLD CODE: static bool s_watchdogStartAttempted = FALSE; */
static u8 s_watchdogStartRetries = 0;
#define WDT_START_MAX_RETRIES  4U
static bool s_watchdogStartFailurePending = FALSE;
#endif
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_FIELD_STATUS_LOG_ENABLE
static u64 s_lastFieldStatusLogMs = 0;
#endif
static bool s_voltageCritical = FALSE;
#if SYSTEM_RECOVERY_ENABLE
static bool s_underVoltagePowerDown = FALSE;
#endif
#if SYSTEM_WDTTEST_COMMAND_ENABLE
/* TEST ONLY: runtime WDT fault injection.
 * Compiled out in production because watchdog/recovery are disabled in this
 * build and these commands deliberately stop feeds or lock the scheduler. */
static u8 s_watchdogRuntimeTestMode = WDT_TEST_NONE;
static u64 s_watchdogRuntimeTestArmMs = 0;
static u32 s_watchdogRuntimeTestDelayMs = 0;
static bool s_watchdogRuntimeTestFired = FALSE;
#endif
#if SYSTEM_WATCHDOG_TEST_SKIP_FEED_AFTER_MS > 0
static bool s_watchdogTestSkipLogged = FALSE;
#endif

#if SYSTEM_RECOVERY_ENABLE || SYSTEM_WDTTEST_COMMAND_ENABLE
static void SystemRecovery_RS232Log(const char *format, ...);
#endif
#if SYSTEM_RECOVERY_ENABLE || SYSTEM_WDTTEST_COMMAND_ENABLE
static const char *SystemRecovery_PowerOnReasonName(s32 reason);
#endif

#if SYSTEM_RECOVERY_ENABLE && SYSTEM_WATCHDOG_ENABLE
/* Task check-in timestamps - Added 2026-05-25 */
static u64 s_lastTaskCheckInMs[WDT_TASK_MAX] = {0};
#endif

void SystemRecovery_CheckInTask(WdtTaskId taskId)
{
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_WATCHDOG_ENABLE
    if (taskId < WDT_TASK_MAX)
    {
        s_lastTaskCheckInMs[taskId] = Ql_GetMsSincePwrOn();
    }
#else
    (void)taskId;
#endif
}


#if SYSTEM_WDTTEST_COMMAND_ENABLE
static const char *SystemRecovery_WdtTestName(u8 mode)
{
    switch (mode)
    {
    case WDT_TEST_FEED_STOP:
        return "FEEDSTOP";
    case WDT_TEST_SYSTIC_BLOCK:
        return "REDLOCK";
    case WDT_TEST_RED_STUCK:
        return "REDSTUCK";
    default:
        return "NONE";
    }
}

void SystemRecovery_ArmWatchdogTest(u8 mode, u32 delayMs)
{
    if (mode != WDT_TEST_FEED_STOP &&
        mode != WDT_TEST_SYSTIC_BLOCK &&
        mode != WDT_TEST_RED_STUCK)
    {
        return;
    }

    if (delayMs < 1000)
    {
        delayMs = 5000;
    }

    s_watchdogRuntimeTestMode = mode;
    s_watchdogRuntimeTestArmMs = Ql_GetMsSincePwrOn();
    s_watchdogRuntimeTestDelayMs = delayMs;
    s_watchdogRuntimeTestFired = FALSE;

    LOGData(TAG_WDT,
            "TEST armed: mode=%s delay=%lu ms active=%d",
            SystemRecovery_WdtTestName(mode),
            delayMs,
            s_watchdogId >= 0 ? 1 : 0);
    SystemRecovery_RS232Log("$RCV,WDT,TEST_ARM,MODE=%s,DELAY=%lu,ACTIVE=%d",
                            SystemRecovery_WdtTestName(mode),
                            delayMs,
                            s_watchdogId >= 0 ? 1 : 0);
}

void SystemRecovery_CancelWatchdogTest(void)
{
    s_watchdogRuntimeTestMode = WDT_TEST_NONE;
    s_watchdogRuntimeTestArmMs = 0;
    s_watchdogRuntimeTestDelayMs = 0;
    s_watchdogRuntimeTestFired = FALSE;
    LOGData(TAG_WDT, "TEST cancelled");
    SystemRecovery_RS232Log("$RCV,WDT,TEST_CLEAR");
}

void SystemRecovery_GetWatchdogTestStatus(char *out, u32 outLen)
{
    if (out == NULL || outLen == 0)
    {
        return;
    }

    s32 reason = Ql_GetPowerOnReason();
    const char *reasonName = SystemRecovery_PowerOnReasonName(reason);

    Ql_memset(out, 0, outLen);
    Ql_sprintf(out,
               "WDTTEST recovery=%d watchdog=%d mode=%s active=%d armed=%d fired=%d delayMs=%lu lastBootReason=%d(%s) totalBoots=%lu totalWdtResets=%lu",
               (int)SYSTEM_RECOVERY_ENABLE,
               (int)SYSTEM_WATCHDOG_ENABLE,
               SystemRecovery_WdtTestName(s_watchdogRuntimeTestMode),
               s_watchdogId >= 0 ? 1 : 0,
               s_watchdogRuntimeTestMode != WDT_TEST_NONE ? 1 : 0,
               s_watchdogRuntimeTestFired ? 1 : 0,
               s_watchdogRuntimeTestDelayMs,
               (int)reason,
               reasonName ? reasonName : "UNKNOWN",
               Diag_GetBootCount(),
               Diag_GetWdtResetCount());
}
#endif

#if SYSTEM_RECOVERY_ENABLE || SYSTEM_WDTTEST_COMMAND_ENABLE
static void SystemRecovery_RS232Log(const char *format, ...)
{
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_RECOVERY_RS232_LOG_ENABLE
#ifdef ENABLE_RS232_PRINT
    char buffer[220];
    va_list args;

    if (format == NULL)
    {
        return;
    }

    Ql_memset(buffer, 0, sizeof(buffer));
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    buffer[sizeof(buffer) - 3] = '\0';
    Ql_strncat(buffer, "\n", sizeof(buffer) - Ql_strlen(buffer) - 1);
    SendRS232String(buffer);
#else
    (void)format;
#endif
#else
    (void)format;
#endif
}
#endif

#if SYSTEM_RECOVERY_ENABLE || SYSTEM_WDTTEST_COMMAND_ENABLE
static const char *SystemRecovery_PowerOnReasonName(s32 reason)
{
    switch (reason)
    {
    case PWRKEYPWRON:
        return "PWRKEYPWRON";
    case CHRPWRON:
        return "CHRPWRON";
    case RTCPWRON:
        return "RTCPWRON";
    case CHRPWROFF:
        return "CHRPWROFF";
    case WDTRESET:
        return "WDTRESET";
    case ABNRESET:
        return "ABNRESET";
    case USBPWRON:
        return "USBPWRON";
    case USBPWRON_WDT:
        return "USBPWRON_WDT";
    case PRECHRPWRON:
        return "PRECHRPWRON";
    case HWSYSRST:
        return "HWSYSRST";
    case UNKNOWN_PWRON:
        return "UNKNOWN_PWRON";
    default:
        return "UNKNOWN";
    }
}
#endif

#if SYSTEM_RECOVERY_ENABLE
static const char *SystemRecovery_VoltageIndName(u32 voltageInd)
{
    switch (voltageInd)
    {
    case VBATT_UNDER_WRN:
        return "VBATT_UNDER_WRN";
    case VBATT_UNDER_PDN:
        return "VBATT_UNDER_PDN";
    case VBATT_OVER_WRN:
        return "VBATT_OVER_WRN";
    case VBATT_OVER_PDN:
        return "VBATT_OVER_PDN";
    default:
        return "UNKNOWN";
    }
}
#endif

#if SYSTEM_RECOVERY_ENABLE && (VTS_DEBUG_LOG_ENABLE || SYSTEM_RECOVERY_RS232_LOG_ENABLE)
static void SystemRecovery_LogExceptionSummary(void)
{
    static EX_LOG_T exceptionBuf;
    u8 validCnt = 0;
    s32 ret;

    Ql_memset(&exceptionBuf, 0, sizeof(exceptionBuf));
    ret = Ql_GetExceptionRecords(&exceptionBuf, &validCnt, 1);
#if VTS_DEBUG_LOG_ENABLE
    LOGData(TAG_RECOVERY, "Exception records: ret=%d, valid=%d", ret, validCnt);
#endif
    SystemRecovery_RS232Log("$RCV,EXC,RET=%d,VALID=%d", ret, validCnt);
    if (validCnt > 0)
    {
#if VTS_DEBUG_LOG_ENABLE
        LOGData(TAG_RECOVERY, "Last exception: type=%d, serial=%d, diagnosis=%d",
                exceptionBuf.header.ex_type,
                exceptionBuf.header.ex_serial_num,
                exceptionBuf.diaginfo.diagnosis);
#endif
        SystemRecovery_RS232Log("$RCV,EXC2,TYPE=%d,SERIAL=%d,DIAG=%d",
                                exceptionBuf.header.ex_type,
                                exceptionBuf.header.ex_serial_num,
                                exceptionBuf.diaginfo.diagnosis);
    }
}
#endif

#if SYSTEM_RECOVERY_ENABLE
static void SystemRecovery_RaiseLowBatteryAlert(const char *source)
{
#if SYSTEM_VOLTAGE_RECOVERY_ENABLE
    /* Defer delivery until the hardware loop has completed its ADC settling
     * period.  This avoids emitting a low-battery event from an unverified
     * early-boot reading; it is not a reset workaround. */
    if (!VAlert[BATT_LOW_ALERT].Enable)
    {
        VAlert[BATT_LOW_ALERT].Enable = 1;
        LOGData(TAG_RECOVERY, "Low battery alert flagged by %s (deferred to HW loop)", source);
        SystemRecovery_RS232Log("$RCV,VLOW,SRC=%s,ALERT=FLAGGED", source);
    }
    else
    {
        LOGData(TAG_RECOVERY, "Low battery alert already active, source=%s", source);
        SystemRecovery_RS232Log("$RCV,VLOW,SRC=%s,ALERT=ACTIVE", source);
    }
#else
    (void)source;
#endif
}
#endif

void SystemRecovery_LogBootReason(void)
{
#if SYSTEM_RECOVERY_ENABLE
    s32 reason = Ql_GetPowerOnReason();
    const char *reasonName = SystemRecovery_PowerOnReasonName(reason);

    /* ------------------------------------------------------------------
     * DIAGNOSTIC LOGGING SYSTEM — Enhanced boot reason logging
     * Added : 2026-05-13
     *
     * OLD CODE logged only TAG_RECOVERY at VTS_DEBUG_LOG_ENABLE level.
     * NEW CODE additionally:
     *   1. Logs with dedicated TAG_BOOT for easy grep filtering
     *   2. Updates DiagCounters.BootCount and WatchdogResetCount in RAM
     *      (saved to flash when LoadDiag()->PushDiagEvent() is called)
     *   3. Pushes a DIAG_EVT_BOOT or DIAG_EVT_WDT_RESET event to ring
     *   4. Records LastBootReason for post-mortem analysis
     *
     * HOW TO READ:
     *   Search logs for "BOOT:" prefix. Lines will show:
     *     BOOT: *** BOOT reason=5 (WDTRESET) WDT_resets_total=3 ***
     *   reason=5 means WDTRESET = device restarted because watchdog
     *   was not fed in time. With the current config this normally means
     *   the Systic/feed path was blocked longer than the active WDT timeout.
     *
     * OLD CODE: LOGData(TAG_RECOVERY, "Power-on reason: %d (%s)", reason, reasonName);
     * ------------------------------------------------------------------ */
    /* ------------------------------------------------------------------
     * Diag_OnBoot() handles all counter updates and ring buffer push.
     * isWdtReset is set if either WDT constant matches:
     *   WDTRESET      — standard watchdog timeout reset
     *   USBPWRON_WDT  — USB power-on triggered by watchdog reset
     * Both indicate the firmware was frozen before the reset.
     * Counter reads use getter functions because we include Diag.h
     * (no struct access) instead of File.h (circular include chain).
     * ------------------------------------------------------------------ */
    {
        int isWdt = (reason == WDTRESET) || (reason == USBPWRON_WDT);
        extern VTSStateTypedef VTSState;
        Diag_OnBoot(reason, (unsigned char)VTSState.CurrentProfile, isWdt, reasonName);

#if VTS_DEBUG_LOG_ENABLE
        /* LOG CLEANUP 2026-05-16: TAG_BOOT line already shows reason + name.
         * Two boot reason logs back-to-back was pure duplicate.
         * OLD CODE:
         *   LOGData(TAG_RECOVERY, "Power-on reason: %d (%s)", reason, reasonName);
         */
        LOGData(TAG_BOOT, "*** BOOT reason=%d (%s) WDT_resets_total=%lu BootCount=%lu ***",
                reason, reasonName, Diag_GetWdtResetCount(), Diag_GetBootCount());
#endif
        SystemRecovery_RS232Log("$RCV,BOOT,REASON=%d,%s", reason, reasonName);

        if (isWdt)
        {
            SystemRecovery_RS232Log("$RCV,WDT,RESET_BOOT,TOTAL=%lu", Diag_GetWdtResetCount());
        }
    }
#endif
}

void SystemRecovery_LogFieldStatus(const char *source)
{
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_FIELD_STATUS_LOG_ENABLE && (VTS_DEBUG_LOG_ENABLE || SYSTEM_RECOVERY_RS232_LOG_ENABLE)
    u32 capacity = 0;
    u32 moduleVoltageMv = 0;
    u32 rssi = 0;
    u32 ber = 0;
    s32 gsmRegState = -1;
    s32 gprsRegState = -1;
    s32 powerRet;
    s32 csqRet;
    s32 gsmRet;
    s32 gprsRet;
    u32 uptimeSec = (u32)(Ql_GetMsSincePwrOn() / 1000);

    if (source == NULL)
    {
        source = "periodic";
    }

    powerRet = RIL_GetPowerSupply(&capacity, &moduleVoltageMv);
    csqRet = RIL_NW_GetSignalQuality(&rssi, &ber);
    gsmRet = RIL_NW_GetGSMState(&gsmRegState);
    gprsRet = RIL_NW_GetGPRSState(&gprsRegState);

#if VTS_DEBUG_LOG_ENABLE
    LOGData(TAG_RECOVERY, "Field status[%s]: uptime=%u s, imei=%s", source, uptimeSec, NetWork.IMEI);
    /* LOG CLEANUP 2026-05-16: %.2f triggers "Ql_vsnprintf() is not supported"
     * spam every status log. Use integer millivolts (mV) instead — same data,
     * no SDK warning.
     * OLD CODE:
     *   LOGData(TAG_RECOVERY, "Voltage: cachedBatt=%.2f V, battPerc=%d, battThrs=%.2f V, mains=%.2f V, ...",
     *           PeriPheralVal.BattVolt, ...);
     */
    LOGData(TAG_RECOVERY, "Voltage: battMv=%u, battPerc=%d, battThrsMv=%u, mainsMv=%u, moduleRet=%d, moduleMv=%u, cap=%u",
            (u32)(PeriPheralVal.BattVolt * 1000.0),
            PeriPheralVal.BattPerc,
            (u32)(VTSData.BattThrs * 1000.0),
            (u32)(PeriPheralVal.MainsVolt * 1000.0),
            powerRet,
            moduleVoltageMv,
            capacity);
    LOGData(TAG_RECOVERY, "GSM: cachedState=%d, cachedCSQ=%d, regDenied=%d, network=%s, csqRet=%d, rssi=%u, ber=%u, gsmRet=%d, gsmReg=%d, gprsRet=%d, gprsReg=%d",
            GSM.GSMState,
            GSM.SignalStrength,
            GSM.IsRegDenied,
            NetWork.Network,
            csqRet,
            rssi,
            ber,
            gsmRet,
            gsmRegState,
            gprsRet,
            gprsRegState);
#endif
    SystemRecovery_RS232Log("$RCV,STAT,SRC=%s,UP=%u,IMEI=%s", source, uptimeSec, NetWork.IMEI);
    /* LOG CLEANUP 2026-05-16: Use mV integers to avoid %.2f issue.
     * OLD: "$RCV,VOLT,CB=%.2f,BP=%d,TH=%.2f,MN=%.2f,..." */
    SystemRecovery_RS232Log("$RCV,VOLT,CB=%u,BP=%d,TH=%u,MN=%u,MRET=%d,MMV=%u,CAP=%u",
                            (u32)(PeriPheralVal.BattVolt * 1000.0),
                            PeriPheralVal.BattPerc,
                            (u32)(VTSData.BattThrs * 1000.0),
                            (u32)(PeriPheralVal.MainsVolt * 1000.0),
                            powerRet,
                            moduleVoltageMv,
                            capacity);
    SystemRecovery_RS232Log("$RCV,GSM,ST=%d,CSQ=%d,DEN=%d,NET=%s,CRET=%d,RSSI=%u,BER=%u,GRET=%d,GSM=%d,PRET=%d,GPRS=%d",
                            GSM.GSMState,
                            GSM.SignalStrength,
                            GSM.IsRegDenied,
                            NetWork.Network,
                            csqRet,
                            rssi,
                            ber,
                            gsmRet,
                            gsmRegState,
                            gprsRet,
                            gprsRegState);
#else
    (void)source;
#endif
}

#if SYSTEM_RECOVERY_ENABLE && SYSTEM_WATCHDOG_ENABLE
static void SystemRecovery_InitWatchdogService(void)
{
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_WATCHDOG_ENABLE && SYSTEM_WATCHDOG_SERVICE_INIT_ENABLE
    if (s_watchdogServiceInitAttempted)
    {
        return;
    }

    s_watchdogServiceInitAttempted = TRUE;
    s_watchdogServiceInitRet = Ql_WTD_Init(0,
                                           SYSTEM_WATCHDOG_SERVICE_INIT_PIN,
                                           SYSTEM_WATCHDOG_SERVICE_INIT_MS);
    if (s_watchdogServiceInitRet == QL_RET_OK)
    {
        LOGData(TAG_WDT,
                "WDT service init OK: pin=%d, serviceFeed=%d ms",
                (int)SYSTEM_WATCHDOG_SERVICE_INIT_PIN,
                (int)SYSTEM_WATCHDOG_SERVICE_INIT_MS);
        SystemRecovery_RS232Log("$RCV,WDT,SERVICE_INIT,RET=0,PIN=%d,MS=%d",
                                (int)SYSTEM_WATCHDOG_SERVICE_INIT_PIN,
                                (int)SYSTEM_WATCHDOG_SERVICE_INIT_MS);
    }
    else
    {
        LOGData(TAG_WDT,
                "WDT service init FAILED: ret=%d, pin=%d, serviceFeed=%d ms",
                s_watchdogServiceInitRet,
                (int)SYSTEM_WATCHDOG_SERVICE_INIT_PIN,
                (int)SYSTEM_WATCHDOG_SERVICE_INIT_MS);
        SystemRecovery_RS232Log("$RCV,WDT,SERVICE_INIT_FAIL,RET=%d,PIN=%d,MS=%d",
                                s_watchdogServiceInitRet,
                                (int)SYSTEM_WATCHDOG_SERVICE_INIT_PIN,
                                (int)SYSTEM_WATCHDOG_SERVICE_INIT_MS);
    }
#endif
}
#endif

/* ------------------------------------------------------------------
 * SystemRecovery_EarlyWatchdogStart
 * DATE  : 2026-05-16
 *
 * MUST be called from proc_main_task() BEFORE the while(TRUE) message
 * loop — this is the only valid call site for Ql_WTD_Start() on M66.
 *
 * It delegates to StartWatchdog(), which attempts every fallback interval
 * immediately while this task is still in the only valid pre-message-loop
 * context. No later task/message handler is allowed to start the WDT.
 * ------------------------------------------------------------------ */
void SystemRecovery_EarlyWatchdogStart(void)
{
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_WATCHDOG_ENABLE
    /* FIX 2026-05-18: do not sleep before watchdog start.
     *
     * The old SYSTEM_WATCHDOG_BOOT_DELAY_MS sleep happened before starting
     * the watchdog. The M66 OS forbids Ql_WTD_Start() once Ql_OS_GetMessage()
     * has been entered, so we cannot defer until after MSG_ID_RIL_READY.
     * What we CAN do is delay inside proc_main_task before the message
     * loop begins — RIL initialisation, OS timer pool, and Hardware/Systic
     * thread bootstraps will be making progress in parallel during this
     * sleep so by the time Ql_WTD_Start runs, the timer pool is settled.
     *
     * The primary timeout is the Quectel example value (3000 ms), with
     * shorter fallbacks if the module rejects it. Once started, the WDT is
     * fed immediately here, then by the Systic fast tick after recovery init.
     */
    LOGData(TAG_WDT,
            "EarlyWatchdogStart: no pre-start sleep; configured delay=%d ms",
            (int)SYSTEM_WATCHDOG_BOOT_DELAY_MS);
    /* Do not sleep before Ql_WTD_Start(); doing so lets other tasks enter
     * their message loops and the M66 rejects the watchdog start. */

    LOGData(TAG_WDT,
            "EarlyWatchdogStart: calling Ql_WTD_Start() from pre-loop context "
            "(proc_main_task, before Ql_OS_GetMessage) — required M66 call site");
    SystemRecovery_StartWatchdog();
#endif
}

/* ------------------------------------------------------------------
 * SystemRecovery_StartWatchdog - pre-loop interval fallback chain
 * DATE  : 2026-05-18
 *
 * ROOT CAUSE OF ORIGINAL FAILURE:
 *   Ql_WTD_Start(30000) returned QL_RET_ERR_PARAM (-1).
 *   The M66 internal watchdog has an undocumented maximum interval.
 *   Official Quectel example (example_watchdog.c) uses 3000 / 1200 ms.
 *   30 000 ms far exceeds any working value seen on M66.
 *   Worse, s_watchdogStartAttempted was set TRUE BEFORE the call,
 *   so one failure permanently disabled the watchdog for the session.
 *
 * HOW THIS FIX WORKS:
 *   Ql_WTD_Start() is only valid before Ql_OS_GetMessage() is entered.
 *   Therefore this function tries the full fallback table in one call:
 *     Attempt 1 -> SYSTEM_WATCHDOG_TIMEOUT_MS (default 3000 ms)
 *     Attempt 2 -> 2000 ms
 *     Attempt 3 -> 1200 ms (Quectel example uses this in a subtask)
 *   If all attempts fail, no post-loop retry is attempted because logs
 *   show those retries always return QL_RET_ERR_PARAM (-1) on M66.
 *
 * OLD CODE (for revert):
 *   static bool s_watchdogStartAttempted = FALSE;
 *   ...
 *   if (s_watchdogStartAttempted) return;
 *   s_watchdogStartAttempted = TRUE;
 *   wtdId = Ql_WTD_Start(SYSTEM_WATCHDOG_TIMEOUT_MS);
 *   if (wtdId < QL_RET_OK) { LOGData("...failed, ret=%d", wtdId); return; }
 * ------------------------------------------------------------------ */
void SystemRecovery_StartWatchdog(void)
{
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_WATCHDOG_ENABLE
    /* Latest device log rejected 30000 ms even from the corrected pre-loop
     * call site. Use the M66 SDK example values and keep all attempts in
     * this valid context. */
    static const u32 s_wdtIntervals[] = {
        SYSTEM_WATCHDOG_TIMEOUT_MS,  /* default: 5000 ms */
        3000U,
        2000U,
        1200U
    };
    static const u8 s_wdtIntervalsCount =
        (u8)(sizeof(s_wdtIntervals) / sizeof(s_wdtIntervals[0]));
    u32 interval;
    s32 wtdId;

    if (s_watchdogId >= 0)
    {
        return; /* already started */
    }

    SystemRecovery_InitWatchdogService();

    if (s_watchdogStartRetries >= s_wdtIntervalsCount)
    {
        static bool s_exhaustedLogged = FALSE;
        if (!s_exhaustedLogged)
        {
            s_exhaustedLogged = TRUE;
            LOGData(TAG_WDT,
                    "!!! CRITICAL: WDT start failed after %d attempts — "
                    "NO WATCHDOG ACTIVE. Deadlock will NOT trigger auto-reset !!!",
                    (int)s_wdtIntervalsCount);
            SystemRecovery_RS232Log("$RCV,WDT,CRITICAL,NO_WDT,RETRIES=%d",
                                    (int)s_wdtIntervalsCount);
            s_watchdogStartFailurePending = TRUE;
        }
        return;
    }

    while (s_watchdogStartRetries < s_wdtIntervalsCount)
    {
        interval = s_wdtIntervals[s_watchdogStartRetries];
        s_watchdogStartRetries++;

        LOGData(TAG_WDT, "WDT start attempt %d/%d: Ql_WTD_Start(%u ms)",
                (int)s_watchdogStartRetries, (int)s_wdtIntervalsCount, interval);

        wtdId = Ql_WTD_Start(interval);

        if (wtdId < QL_RET_OK)
        {
            LOGData(TAG_WDT,
                    "WDT start attempt %d FAILED: ret=%d "
                    "(QL_RET_ERR_PARAM=%d / QL_RET_ERR_TIMER_FULL=%d) "
                    "interval=%u ms — trying next fallback in pre-loop context",
                    (int)s_watchdogStartRetries,
                    wtdId, QL_RET_ERR_PARAM, QL_RET_ERR_TIMER_FULL,
                    interval);
            SystemRecovery_RS232Log("$RCV,WDT,START_FAIL,RET=%d,IV=%u,ATT=%d",
                                    wtdId, interval, (int)s_watchdogStartRetries);
            continue;
        }

        s_watchdogId = wtdId;
        Ql_WTD_Feed(s_watchdogId);
        s_lastWatchdogFeedMs = Ql_GetMsSincePwrOn();
        s_watchdogStartFailurePending = FALSE;
        LOGData(TAG_WDT,
                "WDT started OK: id=%d, timeout=%u ms (attempt %d/%d), initial feed done",
                s_watchdogId, interval,
                (int)s_watchdogStartRetries, (int)s_wdtIntervalsCount);
        LOGData(TAG_RECOVERY, "Internal watchdog started, id=%d, timeout=%u ms",
                s_watchdogId, interval);
        SystemRecovery_RS232Log("$RCV,WDT,START,ID=%d,TO=%u", s_watchdogId, interval);
        return;
    }

    s_watchdogStartFailurePending = TRUE;
    LOGData(TAG_WDT,
            "!!! CRITICAL: WDT start failed after %d pre-loop attempts — "
            "NO WATCHDOG ACTIVE. Deadlock will NOT trigger auto-reset !!!",
            (int)s_wdtIntervalsCount);
    SystemRecovery_RS232Log("$RCV,WDT,CRITICAL,NO_WDT,RETRIES=%d",
                            (int)s_wdtIntervalsCount);

#elif SYSTEM_RECOVERY_ENABLE
    LOGData(TAG_RECOVERY, "Internal watchdog disabled by configuration");
    SystemRecovery_RS232Log("$RCV,WDT,DISABLED");
#endif
}

void SystemRecovery_FeedWatchdog(void)
{
    u64 nowMs = Ql_GetMsSincePwrOn();
#if !SYSTEM_WDTTEST_COMMAND_ENABLE && !SYSTEM_RECOVERY_ENABLE
    (void)nowMs;
#endif

#if SYSTEM_WDTTEST_COMMAND_ENABLE
    /* TEST ONLY:
     * Keep REDLOCK command-driven even in no-watchdog/no-recovery bench
     * builds. That lets the same command prove the A/B difference:
     * watchdog ON reboots, watchdog OFF stays locked until manual power
     * cycle. FEEDSTOP still only matters when a watchdog is active.
     */
    if (s_watchdogRuntimeTestMode != WDT_TEST_NONE &&
        (nowMs - s_watchdogRuntimeTestArmMs) >= s_watchdogRuntimeTestDelayMs)
    {
        if (!s_watchdogRuntimeTestFired)
        {
            s_watchdogRuntimeTestFired = TRUE;
            LOGData(TAG_WDT,
                    "TEST firing now: mode=%s recovery=%d watchdog=%d.",
                    SystemRecovery_WdtTestName(s_watchdogRuntimeTestMode),
                    (int)SYSTEM_RECOVERY_ENABLE,
                    (int)SYSTEM_WATCHDOG_ENABLE);
            SystemRecovery_RS232Log("$RCV,WDT,TEST_FIRE,MODE=%s",
                                    SystemRecovery_WdtTestName(s_watchdogRuntimeTestMode));
        }

        if (s_watchdogRuntimeTestMode == WDT_TEST_SYSTIC_BLOCK)
        {
            LED_GSM_ON;
            while (1)
            {
            }
        }

        if (s_watchdogRuntimeTestMode == WDT_TEST_RED_STUCK)
        {
            LED_GSM_ON;
            GSM.GSMState = GPRS_INIT;
            GSM.SignalStrength = 99;
            /* Keep scheduler and watchdog feed path alive. This recreates
             * field-like red LED stuck behavior, not a CPU hard lock. */
        }
        else
        {
            return;
        }

    }
#endif

#if SYSTEM_RECOVERY_ENABLE
    if (!s_recoveryInitialized)
    {
        /* Boot grace-period check - Added 2026-05-25
         * If system_init() hangs longer than 30 seconds before recovery initialization is complete,
         * stop feeding the watchdog and let it reset the hung system. */
        if (nowMs > 30000ULL)
        {
            static bool s_bootHangLogged = FALSE;
            if (!s_bootHangLogged)
            {
                s_bootHangLogged = TRUE;
                LOGData(TAG_WDT, "!!! CRITICAL: Boot initialization hung for 30s. Watchdog feed STOPPED !!!");
                SystemRecovery_RS232Log("$RCV,WDT,BOOT_HANG,STOP_FEED");
            }
            return; /* Let watchdog reset the hung main task */
        }

#if SYSTEM_WATCHDOG_ENABLE
        if (!s_underVoltagePowerDown &&
            s_watchdogId >= 0 &&
            (s_lastWatchdogFeedMs == 0 ||
             (nowMs - s_lastWatchdogFeedMs) >= SYSTEM_WATCHDOG_FEED_MS))
        {
            Ql_WTD_Feed(s_watchdogId);
            s_lastWatchdogFeedMs = nowMs;
        }
#endif
        return;
    }

#if SYSTEM_FIELD_STATUS_LOG_ENABLE
    if (s_lastFieldStatusLogMs == 0 || (nowMs - s_lastFieldStatusLogMs) >= SYSTEM_RECOVERY_STATUS_LOG_MS)
    {
        s_lastFieldStatusLogMs = nowMs;
        SystemRecovery_LogFieldStatus("periodic");
    }
#endif

#if SYSTEM_WATCHDOG_ENABLE
    if (s_underVoltagePowerDown)
    {
        return;
    }

    /* ------------------------------------------------------------------
     * WDT not started: do NOT retry from here — wrong context
     * DATE  : 2026-05-16
     *
     * Calling Ql_WTD_Start() from inside FeedWatchdog() (which runs in
     * the Systic thread, after the message loop is already running) returns
     * -1 on M66 — the SAME failure as calling it from MSG_ID_RIL_READY.
     * Both are "post-message-loop" contexts where the M66 OS rejects WDT
     * registration. The ONLY valid call site is proc_main_task() BEFORE
     * Ql_OS_GetMessage() — handled by SystemRecovery_EarlyWatchdogStart().
     *
     * OLD CODE (incorrect — retried from wrong context, always failed):
     *   if (s_watchdogId < 0) { SystemRecovery_StartWatchdog(); return; }
     * ------------------------------------------------------------------ */
    if (s_watchdogId < 0)
    {
        static bool s_feedNoWdtLogged = FALSE;
        if (!s_feedNoWdtLogged)
        {
            s_feedNoWdtLogged = TRUE;
            LOGData(TAG_WDT,
                    "FeedWatchdog: WDT not active (id=-1). "
                    "EarlyWatchdogStart() must run before the message loop. "
                    "NO auto-reset on deadlock — check main.c proc_main_task().");
            SystemRecovery_RS232Log("$RCV,WDT,NO_WDT,CHECK_EARLY_INIT");
        }
        return;
    }

    if (s_lastWatchdogFeedMs != 0 && (nowMs - s_lastWatchdogFeedMs) < SYSTEM_WATCHDOG_FEED_MS)
    {
        return;
    }

#if SYSTEM_WATCHDOG_TEST_SKIP_FEED_AFTER_MS > 0
    if (nowMs >= SYSTEM_WATCHDOG_TEST_SKIP_FEED_AFTER_MS)
    {
        if (!s_watchdogTestSkipLogged)
        {
            s_watchdogTestSkipLogged = TRUE;
            LOGData(TAG_RECOVERY, "Bench test: watchdog feed intentionally stopped at %u ms", (u32)nowMs);
            SystemRecovery_RS232Log("$RCV,WTD,TEST_SKIP,MS=%u", (u32)nowMs);
        }
        return;
    }
#endif

    /* Task Health Gating check - Added 2026-05-25
     * Systic task (current task) checks in automatically before feeding.
     * GPRS task must check in every 45 seconds to keep watchdog alive. */
    s_lastTaskCheckInMs[WDT_TASK_SYSTIC] = nowMs;
    
    if (s_lastTaskCheckInMs[WDT_TASK_GPRS] == 0 || 
        (nowMs - s_lastTaskCheckInMs[WDT_TASK_GPRS]) > (u64)SYSTEM_WATCHDOG_GPRS_STALL_MS)
    {
        static u64 s_lastGprsFailLogMs = 0;
        if (nowMs - s_lastGprsFailLogMs >= 5000ULL)
        {
            s_lastGprsFailLogMs = nowMs;
            LOGData(TAG_WDT, "Watchdog feed blocked: GPRS task checked in %u ms ago (timeout %u ms)!",
                    (u32)(nowMs - s_lastTaskCheckInMs[WDT_TASK_GPRS]),
                    (u32)SYSTEM_WATCHDOG_GPRS_STALL_MS);
            SystemRecovery_RS232Log("$RCV,WDT,BLOCK,TASK=GPRS,AGE=%u,TO=%u",
                    (u32)(nowMs - s_lastTaskCheckInMs[WDT_TASK_GPRS]),
                    (u32)SYSTEM_WATCHDOG_GPRS_STALL_MS);
        }
        return; /* Skip feeding, let watchdog reset! */
    }

    /* OLD CODES COMMENTED OUT AS REQUESTED:
    Ql_WTD_Feed(s_watchdogId);
    s_lastWatchdogFeedMs = nowMs;
    */
    // GPS Fault Watchdog Check (Grace period of 60s after boot, bypassed when GPS Simulation or FOTA is active)
    extern uint8_t IsFotaProcessing;
    extern uint8_t IsMotaProcessing;
    extern VTSTypedef VTSData;
    if (!VTSData.DisableGPSFaultReset && !GPS_IsSimulationActive() && !IsFotaProcessing && !IsMotaProcessing && (nowMs > 60000ULL))
    {
        static u64 s_lastGpsOkMs = 0;
        if (s_lastGpsOkMs == 0)
        {
            s_lastGpsOkMs = nowMs;
        }

        if (GPS_GetState() != 2) // GPS is operating normally (Fixed or Not Fixed)
        {
            s_lastGpsOkMs = nowMs;
        }
        else if (nowMs - s_lastGpsOkMs > 180000ULL) // 3 minutes of persistent Fault state
        {
            LOGData(TAG_WDT, "Watchdog: GPS in Fault state for > 3 minutes. Triggering system reset.");
            SystemRecovery_RS232Log("$RCV,WDT,GPS_FAULT_RESET,UP=%lu", (u32)(nowMs / 1000ULL));
            SystemRecovery_RequestReset("GPS fault watchdog timeout");
            return;
        }
    }

    // Server Connection Watchdog Check (Bypassed in sleep mode, resets timer on boot/wakeup)
    {
        static u64 s_lastServerOkMs = 0;
        static bool s_wasSleeping = FALSE;
        bool isSleeping = SleepConfig_IsEnabled() ? TRUE : FALSE;

        if (isSleeping)
        {
            s_wasSleeping = TRUE;
        }
        else
        {
            if (s_wasSleeping || s_lastServerOkMs == 0)
            {
                s_wasSleeping = FALSE;
                s_lastServerOkMs = nowMs; // Reset timer on boot or wakeup
            }

            if (TCP_IsAnySocketConnected() || IsFotaProcessing || IsMotaProcessing)
            {
                s_lastServerOkMs = nowMs; // Connection is active or FOTA/MOTA in progress, reset watchdog
            }
            else if (nowMs - s_lastServerOkMs > SYSTEM_CONNECTION_WATCHDOG_TIMEOUT_MS)
            {
                LOGData(TAG_WDT, "Watchdog: Server connection lost for > %llu ms while awake. Triggering system reset.",
                        (u64)SYSTEM_CONNECTION_WATCHDOG_TIMEOUT_MS);
                SystemRecovery_RS232Log("$RCV,WDT,SERVER_CONN_RESET,UP=%lu", (u32)(nowMs / 1000ULL));
                SystemRecovery_RequestReset("Server connection watchdog timeout");
                return;
            }
        }
    }

    Ql_WTD_Feed(s_watchdogId);
    s_lastWatchdogFeedMs = nowMs;

    /* ------------------------------------------------------------------
     * DIAGNOSTIC LOGGING SYSTEM — Periodic WDT feed-alive log
     * Added : 2026-05-13
     *
     * The watchdog is fed at SYSTEM_WATCHDOG_FEED_MS cadence.
     * Logging every feed would produce too much noise.
     * Instead, log once per minute to confirm the WDT is still alive.
     *
     * HOW TO READ:
     *   Search logs for "WDT:" prefix. You will see every 60 seconds:
     *     WDT: feed-alive id=0 uptime=120 s GSM_state=2
     *   If this line STOPS appearing, the Systic thread is blocked and
     *   the watchdog will fire within the active timeout after the last feed.
     *
     * OLD CODE: (no periodic WDT feed log existed)
     * ------------------------------------------------------------------ */
    {
        static u64 s_lastWdtLogMs = 0;
        extern VTSStateTypedef VTSState;
        if (s_lastWdtLogMs == 0 || (nowMs - s_lastWdtLogMs) >= 60000ULL)
        {
            s_lastWdtLogMs = nowMs;
            LOGData(TAG_WDT, "feed-alive id=%d uptime=%u s GSM_state=%d profile=%d",
                    s_watchdogId,
                    (u32)(nowMs / 1000ULL),
                    GSM.GSMState,
                    VTSState.CurrentProfile);
            SystemRecovery_RS232Log("$RCV,WDT,ALIVE,ID=%d,UP=%u,GSM=%d,PRF=%d",
                    s_watchdogId,
                    (u32)(nowMs / 1000ULL),
                    GSM.GSMState,
                    VTSState.CurrentProfile);
        }
    }
#endif
#endif
}

void SystemRecovery_StopWatchdog(void)
{
#if SYSTEM_RECOVERY_ENABLE && SYSTEM_WATCHDOG_ENABLE
    if (s_watchdogId < 0)
    {
        return;
    }

    Ql_WTD_Stop(s_watchdogId);
    LOGData(TAG_RECOVERY, "Internal watchdog stopped, id=%d", s_watchdogId);
    SystemRecovery_RS232Log("$RCV,WDT,STOP,ID=%d", s_watchdogId);
    s_watchdogId         = -1;
    s_lastWatchdogFeedMs = 0;
    /* Reset retry counter so WDT can be restarted if StopWatchdog()
     * was called for a deliberate stop-then-restart (e.g. before reset).
     * OLD CODE: s_watchdogStartAttempted = FALSE; */
    s_watchdogStartRetries = 0;
#endif
}

bool SystemRecovery_IsWatchdogStarted(void)
{
    return (s_watchdogId >= 0) ? TRUE : FALSE;
}

bool SystemRecovery_IsVoltageCritical(void)
{
    return s_voltageCritical;
}

void SystemRecovery_OnVoltageInd(u32 voltageInd)
{
#if SYSTEM_RECOVERY_ENABLE
#if VTS_DEBUG_LOG_ENABLE
    LOGData(TAG_RECOVERY, "Voltage indication: %u (%s)", voltageInd, SystemRecovery_VoltageIndName(voltageInd));
#endif
    SystemRecovery_RS232Log("$RCV,VURC,TYPE=%u,%s", voltageInd, SystemRecovery_VoltageIndName(voltageInd));
    SystemRecovery_LogFieldStatus("voltage-urc");

    switch (voltageInd)
    {
    case VBATT_UNDER_WRN:
        s_voltageCritical = TRUE;
        SystemRecovery_RaiseLowBatteryAlert("under-voltage warning");
        LOGData(TAG_RECOVERY, "Under-voltage warning: avoid FOTA and non-critical flash writes");
        SystemRecovery_RS232Log("$RCV,VWARN,UNDER,CRITICAL=1");
        break;
    case VBATT_UNDER_PDN:
        s_voltageCritical = TRUE;
        s_underVoltagePowerDown = TRUE;
        SystemRecovery_RaiseLowBatteryAlert("under-voltage power down");
        LOGData(TAG_RECOVERY, "!!! UNDER-VOLTAGE CRITICAL (< 3.2V) !!! Starting graceful hardware power down...");
        SystemRecovery_RS232Log("$RCV,VPDN,UNDER,SHUTDOWN=START");
        
        // 1. Force peripheral outputs and charger to safe OFF states
        CONTROL_CHARGER_OFF;
        OUTPUT_1_OFF;
        OUTPUT_2_OFF;

        // 2. Command Nuvoton MCU into deep sleep permanently
        MCOMM_SendSleep(65535);

        // 3. Close GPS and MCU UART lines to prevent reverse current leakage
        Ql_UART_Close(UART_PORT3); // GPS UART
        Ql_UART_Close(UART_PORT1); // MCU UART

        // 4. Stop watchdog
        SystemRecovery_StopWatchdog();

        // 5. Short sleep to flush logs and allow pins to settle
        Ql_Sleep(1000);

        // 6. Actively command the PMIC to shut down the M66 module
        Ql_PowerDown(1);
        break;
    case VBATT_OVER_WRN:
        LOGData(TAG_RECOVERY, "Over-voltage warning received");
        SystemRecovery_RS232Log("$RCV,VWARN,OVER");
        break;
    case VBATT_OVER_PDN:
        LOGData(TAG_RECOVERY, "Over-voltage power down indication received");
        SystemRecovery_RS232Log("$RCV,VPDN,OVER");
        break;
    default:
        break;
    }
#else
    (void)voltageInd;
#endif
}

void SystemRecovery_RequestReset(const char *reason)
{
#if SYSTEM_RECOVERY_ENABLE
    if (reason == NULL)
    {
        reason = "unspecified";
    }

    LOGData(TAG_RECOVERY, "Software reset requested: %s", reason);
    /* ------------------------------------------------------------------
     * DIAGNOSTIC LOGGING SYSTEM — Log reset event before reboot
     * Added : 2026-05-13
     *
     * Push the reset event to the ring buffer and save to flash BEFORE
     * the device reboots. This way the next boot's log dump will show
     * WHY the device was reset (e.g., "GSM hang: unresponsive after CFUN").
     *
     * HOW TO READ:
     *   After reboot search DIAG: logs for type=11(SYS_RESET).
     *   The detail field will show the reason string passed here.
     * ------------------------------------------------------------------ */
    LOGData(TAG_BOOT, "!!! SYSTEM RESET — reason=[%s] !!!", reason);
    PushDiagEvent(DIAG_EVT_SYS_RESET, 0, 0, reason);   /* saved to flash here */
    SystemRecovery_RS232Log("$RCV,RESET,REASON=%s", reason);
    SystemRecovery_LogFieldStatus("software-reset");
    /* Keep the WDT active across the reset request. The delay before
     * Ql_Reset() is short, and an active WDT gives a fallback if the reset
     * call or modem firmware stalls. */
#if SYSTEM_WATCHDOG_ENABLE
    if (s_watchdogId >= 0)
    {
        Ql_WTD_Feed(s_watchdogId);
        s_lastWatchdogFeedMs = Ql_GetMsSincePwrOn();
        LOGData(TAG_WDT, "WDT kept active for software reset: id=%d", s_watchdogId);
        SystemRecovery_RS232Log("$RCV,WDT,RESET_KEEP_ACTIVE,ID=%d", s_watchdogId);
    }
#endif
    /* FIX 2026-05-17: explicit log immediately before Ql_Reset(0).
     * Required so post-mortem analysis can correlate the LAST line of
     * the dying log session with the boot-reason of the next session. */
    LOGData(TAG_RECOVERY,
            "!!! Ql_Reset(0) IMMINENT in %d ms — reason: %s !!!",
            (int)SYSTEM_RESET_DELAY_MS, reason);
    SystemRecovery_RS232Log("$RCV,RESET,IMMINENT,DELAY=%d,REASON=%s",
                            (int)SYSTEM_RESET_DELAY_MS, reason);
    Ql_Sleep(SYSTEM_RESET_DELAY_MS);
    Ql_Reset(0);
#else
    (void)reason;
#endif
}

void SystemRecovery_Init(void)
{
#if SYSTEM_RECOVERY_ENABLE
    if (s_recoveryInitialized)
    {
        return;
    }

    s_recoveryInitialized = TRUE;

#if SYSTEM_WATCHDOG_ENABLE
    /* Seed task check-in timestamps to prevent premature watchdog blocks - Added 2026-05-25 */
    {
        u64 bootMs = Ql_GetMsSincePwrOn();
        int i;
        for (i = 0; i < WDT_TASK_MAX; i++)
        {
            s_lastTaskCheckInMs[i] = bootMs;
        }
    }
#endif

    SystemRecovery_LogBootReason();
#if VTS_DEBUG_LOG_ENABLE
    SystemRecovery_LogExceptionSummary();
#endif
#if !VTS_DEBUG_LOG_ENABLE && SYSTEM_RECOVERY_RS232_LOG_ENABLE
    SystemRecovery_LogExceptionSummary();
#endif
    SystemRecovery_LogFieldStatus("boot");
#if SYSTEM_WATCHDOG_ENABLE
    if (s_watchdogId >= 0)
    {
        LOGData(TAG_WDT, "WDT active at recovery init: id=%d", s_watchdogId);
        SystemRecovery_RS232Log("$RCV,WDT,ACTIVE,ID=%d", s_watchdogId);
    }
    else
    {
        LOGData(TAG_WDT,
                "!!! CRITICAL: WDT inactive at recovery init. "
                "Pre-loop start failed; post-loop start intentionally skipped on M66. !!!");
        SystemRecovery_RS232Log("$RCV,WDT,NO_WDT,POST_INIT");
        if (s_watchdogStartFailurePending)
        {
            PushDiagEvent(DIAG_EVT_GSM_HANG,
                          0, (unsigned char)WDT_START_MAX_RETRIES, "WDT-start-fail");
            s_watchdogStartFailurePending = FALSE;
        }
    }
#else
    LOGData(TAG_RECOVERY, "Internal watchdog disabled by configuration");
    SystemRecovery_RS232Log("$RCV,WTD,DISABLED");
#endif
#endif
}
