# CDAC Firmware — CURRENT vs REFERENCE Deep Comparison

- **CURRENT (broken, active build):** `D:\QUICKTEL\InnoVTSM66_VY\InnoVTSM66\custom\` — branch `OG-FIRMWARE`, NIC1 / `PROTO_CDAC`
- **REFERENCE (known-good):** `D:\QUICKTEL\InnoVTSM66-main (1)\InnoVTSM66-AI\custom\`

Scope compared: HTTP + ril_http, Server/CDAC send orchestration, GPRS, SMS/SOS, RS232/RS485, and the logging framework.

Legend for priority: **P1** = fixes a confirmed runtime failure in the log; **P2** = correctness/robustness parity with reference; **P3** = optional/behavioral.

---

## 0. Executive summary — why Server 3 fails and the modem "hangs"

The `AT+QHTTPPOST … ret=-1` and the ~25 s watchdog hang are **not** a missing AT command (ril_http.c/.h are byte-identical to the reference). They are caused by CURRENT-only regressions that corrupt the **single shared M66 QHTTP session**:

1. **Reentrant callback (prime cause).** `HTTP_SetReadyState()` fires `ServerSocket[0].OnConnect(0)` → `ConnectedCallback` → sets `SendLogin1=1`, injecting a *second* send into the shared QHTTP session while a POST is mid-flight → the concurrent `QHTTPURL`/`QHTTPPOST` is rejected with ERROR (`-1`). Reference's `HTTP_SetReadyState` only sets two state variables.
2. **Missing `volatile`** on `HTTPState`/`IsHTTPRes`/`HTTPConnectFlag` → the HTTP thread and server thread race on the session state.
3. **Server 3 (VLT) mis-initialized as a TCP socket** holding the literal URL `http://78.46.190.117/vlt`, producing the `DNS failed for socket 2, errCode=117` / 60 s retry loop.
4. **GPRS steady-state log regression** floods the UART ~5×/sec, drowning real logs.

Items 1–2 are the direct cause of your `QHTTPPOST -1`. The primitive fixes already applied last session (session drain, 150 ms settle, `https,0`, GPRS gate, `QHTTPPOST` 30,30, retry 3→1) are correct and necessary but insufficient without 1–2.

---

## 1. HTTP layer (`custom/HTTP.c`, `ril/src/ril_http.c`)

**ril_http.c / ril_http.h are functionally identical** — the POST AT sequence (`QHTTPURL` → CONNECT → write → `QHTTPPOST=len,30,30` → CONNECT → write → `QHTTPREAD`) is the same in both. **Reference sends no AT command CURRENT lacks** (neither sends `QHTTPCFG contextid/requestheader/responseheader` nor an explicit `QIACT`). All real differences are in `HTTP.c`:

| # | Difference | CURRENT | REFERENCE | Pri |
|---|---|---|---|---|
| H1 | Reentrant OnConnect in `HTTP_SetReadyState` | fires `OnConnect(0)` ([HTTP.c:178-180](custom/HTTP.c:178)) | sets state vars only | **P1** |
| H2 | `volatile` on shared state | missing ([HTTP.c:44-46](custom/HTTP.c:44)) | `volatile` on all three | **P1** |
| H3 | `IsSMS`/`DecodeOTAData` reentrancy inside QHTTP retry loops | present (SetUrl/Post/Read retry) | absent | **P2** |
| H4 | `HTTPThreadEntry` GPRS re-gating | re-arms on `HTTPConnectFlag` | re-checks `HTTP_IsGprsReady()` with throttled log | P2 |
| H5 | `readTimeoutSec` default | 30 | 10 (limits stuck-read to ~10 s) | **P1** (hang) |
| — | Already ported last session: `HTTP_ConfigureSSL`(`https,0`), 150 ms settle, `HTTP_Close` drain, `HTTP_IsGprsReady` gate | ✔ | ✔ | done |

**Changes:** strip the callback from `HTTP_SetReadyState` (H1); add `volatile` (H2); remove the `IsSMS` reentrancy from the three retry loops (H3); `readTimeoutSec` 30→10 (H5).

---

## 2. Server / CDAC orchestration (`custom/Server.c`, `TCP.c`, `HttpQueue.c`)

`TCP.c` and `HttpQueue.c` are correct/identical to reference (queue tunables byte-identical: SIZE 10, BURST 4, STUCK 25000 ms, MAX_RETRIES 2). All divergence is in `Server.c`.

### 2a. Server 3 socket-2 DNS bug (`InitSockets`)

| | CURRENT ([Server.c:332-341](custom/Server.c:332)) | REFERENCE (Server.c:334-376) |
|---|---|---|
| Socket 2 enable | `isEnabled=1` **unconditionally** when `IPConfig[2]`, DNSorIP = raw URL string | `isEnabled = IPConfig[2] && !IsHttpUrl(IP3)` — **0 for `http://` URLs** |
| Boot URL normalization | none (only via OTA/default set) | `#ifdef PROTO_CDAC UpdateURL(IP1); UpdateSecondaryURL(IP3);` at cold boot |
| Callbacks | always armed | NULL when disabled |

→ CURRENT hands `http://78.46.190.117/vlt` to `Ql_IpHelper_GetIPByHostName` → `errCode=117`. **P1.**

### 2b. `SendDataToServer()` CDAC path

| # | Difference | CURRENT | REFERENCE | Pri |
|---|---|---|---|---|
| S1 | VLT connect/response timeout | interval-derived (as low as 1500/3000 ms) | fixed **10000/10000 ms** | **P1** |
| S2 | Post-VLT session reset | only `HTTPConnectFlag=1` | also `ServerSocket[0].SocketState=SOCKET_IDLE` | **P1** |
| S3 | Return value | `isGood \|\| vlt_good` (`isGood`=socket-connect only) | `(isGood && ret) ? 1 : vlt_good` + early `!isGood&&!vlt_good→0` | **P1** (silent drops) |
| S4 | RS232/RS485 serviced during HTTP waits | **no** | yes, in both wait loops | **P1** (dropped cmds) |
| S5 | TCP-mirror else-branch (bare-IP Server 3) | absent | present | P2 |
| S6 | VLT failure-path close/log | none | `else{log}` + `if(!KeepAlive)HTTP_Close(0)` | P2 |

### 2c. `handleCDACProtocol()`

| # | Difference | CURRENT | REFERENCE | Pri |
|---|---|---|---|---|
| C1 | Socket-2 rx buffer cleared after parse | only `isRXData=0` (stale bytes re-parsed) | `Ql_memset(rxBuffer,0,…)` | P2 |
| C2 | RS232/RS485 vs server-RX parse order | server-RX first | RS232/485 first | P3 (equiv) |
| C3 | `SendLogin1` state machine | tri-state 1→2 | 1→0 | P3 |
| C4 | `IsCritical` reset location | deferred (half-wired) | reset in critical block | P3 |

### 2d. CDAC packet builders

- `DataPacket` MNC: REFERENCE uses `InsertMNCCDAC()` x-padding (`xx9`/`x09`); CURRENT uses plain `InsertIntValue`. **P3** (verify VLT backend expectation).
- `LoginPacket`: REFERENCE uses `wh=1` (`-`→`0`) for IMEI/ActivationKey; CURRENT `wh=0`. **P3**.

---

## 3. GPRS (`custom/GPRS.c`)

| # | Difference | CURRENT | REFERENCE | Pri |
|---|---|---|---|---|
| G1 | `STATE -> GPRS ACTIVE` log | unconditional in ACTIVE case, 200 ms loop → **~5×/sec flood** ([GPRS.c:1293](custom/GPRS.c:1293)) | ACTIVE case has **no log** | **P1** |
| G2 | `CSQ is %d` in `ProcessREGISTER` | present ([GPRS.c:625](custom/GPRS.c:625)), per-loop while SIM_DETECTED | removed | **P1** |
| G3 | `csq:` timestamp line | 1×/sec, gated `++count>4` | identical (kept) | leave |
| G4 | State-machine hardening (fail-count, profile blacklist/switch, GSM-hang detect) | absent | present | P3 (large; do not wholesale-port — CURRENT is newer NIC1) |

**Change:** add a `static uint8_t s_lastLoggedState` prev-state guard so each real transition logs once and the ACTIVE steady state is silent (G1); remove the CSQ spam (G2).

---

## 4. SMS / SOS (`custom/SMS.c`, `SMSLib.c`, `SOS.c`)

Both trees have `SMSLib.c`; the low-level `SMS_SendTextMessage` → RIL PDU path is equivalent. SOS/tamper SMS is now **re-enabled** in CURRENT (macros flipped last session) and **does** send to Mob0/Mob1 with a Google-Maps-link message.

| # | Difference | CURRENT | REFERENCE | Pri |
|---|---|---|---|---|
| M1 | SOS SMS recipients | Mob0, Mob1 (2) | Mob0–Mob4 (5) + machine-format packet SMS | P3 |
| M2 | SOS-server-down SMS fallback (`SendSOSSMS`) | none | present | P3 |
| M3 | SMS delete timing | immediate on receipt (risks dropping multi-segment) | after successful decode | P2 |
| M4 | SOS debounce | no `RequireRelease` latch → repeat/false triggers | `RequireRelease` latch, NC-circuit support | P2 |
| M5 | `SETSENSOR`/`GETSENSOR` over SMS | not handled | handled | P3 (AIS140 sensor surface) |
| M6 | `SOS` struct volatility | non-volatile | `volatile` | P2 |

---

## 5. RS232 / RS485 (`custom/SMS.c`, `MCU.c`)

Both define `ProcessRS232OTAData`/`ProcessRS485OTAData` (neither stubbed). Problems are in routing and call sites.

| # | Difference | CURRENT | REFERENCE | Pri |
|---|---|---|---|---|
| R1 | Serviced during HTTP waits | **no** (commands dropped) | yes | **P1** (same as S4) |
| R2 | Command routing | always `DecodeSMS`, ERROR on any unrecognized bytes | `SET/GET/CLR`→`DecodeOTAData`, `@`→`ParseCLSResponse`, ERROR only for command-like text | P2 |
| R3 | `@`-prefixed CLS fuel-sensor responses | misrouted into DecodeSMS | parsed by `ParseCLSResponse` | P2 |
| R4 | RS232 command normalizer (strip `$`/`@`) | none | `NormalizeRS232Command()` | P2 |
| R5 | `MCU_SetRS232Baud`/`SetRS485Baud` | absent | present | P3 |

---

## 6. Logging framework (`custom/inc/LOG.h`, floods)

CURRENT's `LOGVerbose`/`VTS_VERBOSE_LOG_ENABLE` tier is a CURRENT-only addition (reference has none). Reference keeps quiet by (a) not logging steady states, (b) in-code rate-limits, (c) shipping production with `VTS_DEBUG_LOG_ENABLE=0`.

| # | Site | Message | Freq | Pri | Action |
|---|---|---|---|---|---|
| L1 | GPRS.c:1293 | `STATE -> GPRS ACTIVE` | ~5×/s | **P1** | prev-state guard (see G1) |
| L2 | GPRS.c:625 | `CSQ is %d` | per-loop | **P1** | remove (see G2) |
| L3 | MCU.c:162-165 | `RAW UART RX` hex dump | per MCU pkt | **P1** | gate `LOGVerbose` |
| L4 | MCU.c:218-219 | `Rx` hex dump | per MCU pkt | **P1** | gate `LOGVerbose` |
| L5 | Batch.c:58,97 | `Reading Batch Table` / `Batch: N files` | per send | P2 | `LOGVerbose` / log-on-change |
| L6 | MCU.c:245 | `[TILT_DEBUG] TILT DETECTED` | per tilt | P2 | gate `LOGVerbose`; also add missing `RemoveAlert(TILT_ALERT)` |
| — | HttpQueue Status (10 s), LED (on-change), TCP/socket events, SIM/INIT transient states | — | low | leave | matches reference |

---

## Consolidated implementation plan

**P1 — restore Server 1/3 HTTP + kill floods (fixes the reported failures):**
1. `HTTP.c`: remove reentrant `OnConnect` from `HTTP_SetReadyState` (H1); add `volatile` (H2); `readTimeoutSec` 30→10 (H5).
2. `Server.c InitSockets`: gate socket 2 by `!IsHttpUrl(IP3)`, NULL callbacks when disabled, and add cold-boot `UpdateURL/UpdateSecondaryURL` (2a).
3. `Server.c SendDataToServer`: fixed 10 s VLT timeouts (S1); post-VLT `SocketState=IDLE` (S2); return `(isGood&&ret)?1:vlt_good` + early exit (S3); service RS232/RS485 in both wait loops (S4/R1).
4. `GPRS.c`: prev-state guard for STATE logs / silent ACTIVE (G1); remove `CSQ is %d` (G2).
5. `MCU.c`: gate the two `PrintHexBuffer` dumps and `[TILT_DEBUG]` under `LOGVerbose` (L3/L4/L6).

**P2 — robustness parity:** H3, S5, S6, C1, M3, M4, M6, R2, R3, R4, Batch log gating (L5), tilt-alert clear.

**P3 — optional/behavioral (confirm intent first):** SOS 2→5 recipients + machine SMS + fallback (M1/M2), sensor-over-SMS/RS232 (M5), MNC/`wh` packet-format changes (2d), GPRS state-machine hardening (G4), MCU baud setters (R5).

All P1/P2 items are faithful ports from the working reference and low-risk. Build after each tier with `./Make.bat new`.
