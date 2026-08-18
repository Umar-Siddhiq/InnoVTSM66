# RS232 / RS485 Interface Specification — InnoVTSM66

**Firmware:** `1.5.8` (`FIRMWAREVERSION`, [custom/inc/VTS.h:92](custom/inc/VTS.h:92))
**Active protocol build:** `PROTO_OG` → `PROTO_TAG = "OG1"` ([custom/config/custom_proto_cfg.h:8](custom/config/custom_proto_cfg.h:8), [custom/inc/VTS.h:410](custom/inc/VTS.h:410))
**Branch:** `OG-FIRMWARE`
**Document date:** 2026-08-12
**Status:** Reflects the working tree (uncommitted changes included), not the last commit.

**Companion document:** [RS232_PROTOCOL.md](RS232_PROTOCOL.md) — the host-facing protocol
specification. Use that one to integrate equipment against the RS232 port; use this one to
work on the firmware behind it.

---

## 0. Reading guide

| If you want to… | Go to |
|---|---|
| Understand the wiring / who talks to whom | §1 |
| Decode a raw hex dump on the M66↔MCU link | §2, §3 |
| Parse `$INF` / `$CEL` / `$PER` / `$GPD` on a PC terminal | §5 |
| Understand alert frames on RS232 (`$BATT`, `$RCV`) | §6 |
| Send a command into the device over RS232 | §7 |
| Understand `$RES` replies | §7.5 |
| Interpret firmware trace logs (`HARDWARE:`, `MCU:`, `OTA:` …) | §8 |
| Know what is broken / disabled / misleading | §10 |
| **Understand why `GET RSTEST` returns nothing** | **§10.11** |
| Bench-test the interface | §11 |

---

## 1. Physical & logical architecture

There is **no direct RS232 transceiver on the M66 module**. All RS232/RS485 traffic is
tunnelled through an external companion MCU over the M66's `UART_PORT1` using a private
binary framing protocol called **MCOMM**.

```
                     ┌─────────────────────────────────────────────┐
                     │   Quectel M66  (OpenCPU application)         │
                     │                                             │
  RS232 DB9 ◄──┐     │  UART_PORT1  115200 8N1 FC_NONE  ── MCOMM  ─┼──┐
               │     │  UART_PORT2  115200  → Ql_Debug_Trace logs  │  │
  RS485 A/B ◄──┼──┐  │  UART_PORT3  9600/115200 → GNSS NMEA        │  │
               │  │  └─────────────────────────────────────────────┘  │
               │  │                                                    │
        ┌──────┴──┴────────────────────────────────────────────────────┘
        │  External companion MCU
        │   • terminates RS232 (function 6 TX / 7 RX)
        │   • terminates RS485 (function 4 TX / 5 RX)
        │   • samples IGN / IP1 / IP2 / mains ADC / battery ADC / tilt / MEMS
        └───────────────────────────────────────────────────────────────
```

### 1.1 Port map

| Port | Baud | Purpose | Registered in |
|---|---|---|---|
| `UART_PORT1` (`MCOMM_UART_PORT`) | 115200, FC_NONE | **MCOMM link to companion MCU** — carries RS232 + RS485 payloads, peripheral polling, MCU FOTA | [custom/MCU.c:95](custom/MCU.c:95) |
| `UART_PORT2` (`DEBUG_UART`) | 115200 | Firmware debug trace (`Ql_Debug_Trace`) | [custom/main.c:116](custom/main.c:116) |
| `UART_PORT3` (`GPS_UART_PORT`) | 9600 → 115200 | GNSS receiver NMEA | [custom/GPS.c:615](custom/GPS.c:615) |

> **Critical distinction:** the `LOGData()` trace lines (`HARDWARE: …`, `MCU: …`) go out on
> **UART_PORT2**. The `$INF`/`$CEL`/`$PER`/`$GPD` frames go out on **RS232 via the MCU on
> UART_PORT1**. They are two physically different cables. Trace lines that *contain* a `$`
> frame (e.g. `HARDWARE: $PER,…`) are a *copy* emitted by `LOGVerbose` for diagnostics —
> the authoritative frame is the one on RS232.

### 1.2 Threads involved

| Thread | File | RS232 responsibility |
|---|---|---|
| `mcu_rcv_thread` | [custom/MCU.c:385](custom/MCU.c:385) | Drains MCOMM RX buffer, parses frames, dispatches, maintains `IsMCU` liveness |
| `HardwareThreadEntry` | [custom/Hardware.c:701](custom/Hardware.c:701) | 200 ms loop; drives the 4-frame RS232 status rotation and `$RES` flush |
| Server thread | [custom/Server.c:5234](custom/Server.c:5234) | Polls `RS232_DataAvailable` / `RS485_DataAvailable` and runs the command decoders |
| `Systic` (1 Hz) | [custom/Systic.c:158](custom/Systic.c:158) | Schedules the CLS fuel-sensor query on RS232 |

---

## 2. MCOMM link layer (M66 ↔ companion MCU)

### 2.1 Frame format

```
┌────────┬──────────┬──────────────────────────┬────────┐
│ HEADER │ FUNCTION │  PAYLOAD (0..502 bytes)  │ FOOTER │
│  0x26  │  0x00..  │  fixed size per function │  0x7E  │
└────────┴──────────┴──────────────────────────┴────────┘
   [0]       [1]              [2 ..]              [n-1]
```

| Constant | Value | Source |
|---|---|---|
| `MCOMM_COM_HEADER` | `0x26` (`'&'`) | [custom/inc/MCU.h:34](custom/inc/MCU.h:34) |
| `MCOMM_COM_FOOTER` | `0x7E` (`'~'`) | [custom/inc/MCU.h:35](custom/inc/MCU.h:35) |
| `MCOMM_COM_HEADER_INDEX` | 0 | |
| `MCOMM_COM_FUNCTION_INDEX` | 1 | |
| `MCOMM_COM_DATA_INDEX` | 2 | |
| `MCOMM_COM_LEN_EXTRAS` | 3 (header + function + footer) | [custom/inc/MCU.h:105](custom/inc/MCU.h:105) |

**There is no length field and no checksum.** Frame length is looked up from a static
table indexed by the function code (`LENTABLE`). This means:

* A frame is **fixed length per function**, always — including RS232/RS485 payload frames,
  which are always **505 bytes on the wire regardless of how many payload bytes are used**.
* A corrupted function byte desynchronises the parser until the buffer is flushed.
* Integrity relies solely on header/footer position matching the table length.

### 2.2 Function codes

Defined in `MCOMMComFunctionTypedef`, [custom/inc/MCU.h:40](custom/inc/MCU.h:40).

| Code | Name | Direction | Payload struct | Total frame bytes |
|---:|---|---|---|---:|
| 0 | `MCOMM_COM_FUNCTION_ERROR` | MCU → M66 | — | 3 |
| 1 | `MCOMM_COM_FUNCTION_SUCCESS` | MCU → M66 | — | 3 |
| 2 | `MCOMM_COM_FUNCTION_GETPH` | M66 → MCU (req, 3 B) / MCU → M66 (resp) | `MCUPepheralTypedef` | 17 |
| 3 | `MCOMM_COM_FUNCTION_SETPH` | M66 → MCU | `MCUPepheralTypedef` | 17 |
| 4 | `MCOMM_COM_FUNCTION_485TX` | M66 → MCU | `UartExchangetypedef` | 505 |
| 5 | `MCOMM_COM_FUNCTION_485RX` | MCU → M66 | `UartExchangetypedef` | 505 |
| 6 | `MCOMM_COM_FUNCTION_232TX` | M66 → MCU | `UartExchangetypedef` | 505 |
| 7 | `MCOMM_COM_FUNCTION_232RX` | MCU → M66 | `UartExchangetypedef` | 505 |
| 8 | `MCOMM_COM_FUNCTION_FWSTART` | M66 ↔ MCU | 2-byte chunk count | 5 |
| 9 | `MCOMM_COM_FUNCTION_FWUPDATE` | M66 ↔ MCU | 2-byte chunk number (**big-endian**) | 5 |
| 10 | `MCOMM_COM_FUNCTION_FWEND` | M66 ↔ MCU | — | 3 |
| 11 | `MOMMM_COM_FUNCTION_VERSION` | M66 → MCU (req) / MCU → M66 (resp) | `FirmExchangetypedef` | 13 |
| 12 | `MCOMM_COM_FUNCTION_SLEEP` | M66 → MCU | `ModemSleepTypedef` | 5 |

> Note the inconsistency: function 9 (`FWUPDATE`) chunk number is decoded **big-endian**
> (`data[n]<<8 | data[n+1]`, [custom/MCU.c:310](custom/MCU.c:310)) while every struct field is
> **little-endian** (native ARM). This is intentional in the current code but is a trap for
> anyone writing the MCU side.

### 2.3 Payload structures

#### `UartExchangetypedef` — RS232 / RS485 payload carrier
[custom/inc/MCU.h:68](custom/inc/MCU.h:68)

| Offset | Field | Type | Notes |
|---:|---|---|---|
| +0 | `datalen` | `uint16_t` LE | Valid bytes in `data[]`; must be `1..500` |
| +2 | `data[500]` | `uint8_t[]` | `MCOMM_COM_URT_EXG_BUFF_SIZE = 500` |

`sizeof = 502`. Frame = `0x26 | func | 502 bytes | 0x7E` = **505 bytes**.
Unused trailing bytes are whatever was in the sender's stack/struct — the receiver must
honour `datalen` and ignore the remainder.

#### `MCUPepheralTypedef` — peripheral snapshot
[custom/inc/MCU.h:80](custom/inc/MCU.h:80)

| Offset | Field | Type | Meaning |
|---:|---|---|---|
| +0 | `IP1` | `uint8_t` | Digital input 1 |
| +1 | `IP2` | `uint8_t` | Digital input 2 |
| +2 | `MainVolt` | `uint16_t` LE | Raw 10-bit ADC of external supply |
| +4 | `BattVolt` | `uint16_t` LE | Raw ADC of battery (**not used by firmware**, see §10.4) |
| +6 | `ADCVal1` | `uint16_t` LE | Analogue input 1 → `PeriPheralVal.AN1` |
| +8 | `ADCVal2` | `uint16_t` LE | Analogue input 2 → `PeriPheralVal.AN2` |
| +10 | `IsFlash` | `uint8_t` | MCU external flash healthy |
| +11 | `IsMems` | `uint8_t` | MEMS accelerometer present |
| +12 | `IsTilt` | `uint8_t` | Tilt asserted → drives `TILT_ALERT` |
| +13 | *(pad)* | — | Struct padded to 14 bytes |

`sizeof = 14`. Frame total 17 bytes.

**Mains voltage conversion** — [custom/MCU.c:20](custom/MCU.c:20):

```
V_in = (ADC / 1023.0 * 3.3) * 7.32 + 0.67      // VREF 3.3 V, divider 7.32, diode 0.67 V
if (V_in < 3.0) V_in = 0;                       // dead-band → "no mains"
```

#### `FirmExchangetypedef`
`char FirmwareVersion[10]` → `PeriPheralVal.MCUFirmwareVersion`. `sizeof = 10`, frame 13.

#### `ModemSleepTypedef`
`uint16_t sleeptime` (seconds). `sizeof = 2`, frame 5.

### 2.4 Worked example — decoding a real GETPH exchange

Captured trace ([LOGS.txt:19](LOGS.txt:19)):

```
MCU: Sending data: len=3, isWait=1, waitFlag=2, timeout=1000
MCU: RAW UART RX[17]:
MCU: 26 02 01 01 EE 01 AD 08
MCU: 65 00 A6 0E 00 00 00 00
MCU: 7E
MCU: Function: GETPH
MCU: Mains ADC: 494 (12.3V) BattADC: 2221
```

Request (M66 → MCU): `26 02 7E` (3 bytes, function 2, no payload).
Response (MCU → M66), 17 bytes:

| Bytes | Field | Value |
|---|---|---|
| `26` | header | ✓ |
| `02` | function | GETPH |
| `01` | `IP1` | 1 |
| `01` | `IP2` | 1 |
| `EE 01` | `MainVolt` | 0x01EE = **494** → 12.3 V |
| `AD 08` | `BattVolt` | 0x08AD = **2221** |
| `65 00` | `ADCVal1` | 101 |
| `A6 0E` | `ADCVal2` | 3750 |
| `00` | `IsFlash` | 0 |
| `00` | `IsMems` | 0 |
| `00` | `IsTilt` | 0 |
| `00` | pad | — |
| `7E` | footer | ✓ |

---

## 3. Receive path (MCU → M66)

```
 UART1 IRQ ──► mcu_uart_cb()                    [MCU.c:142]
               EVENT_UART_READY_TO_READ
               └─ mcu_uart_read() drains into mcu_tmpdata[1024]
               └─ PrintHexBuffer("RAW UART RX", first 64 bytes)   ← LOGVerbose
               └─ if (mcu_data_available) → DROP, log "MCU Rcv %d While Processing Data"
               └─ memcpy → MCOMMRxBuff, mcu_data_available = totalBytes, MCUTimeout = 25

 mcu_rcv_thread (30 ms loop)                     [MCU.c:385]
   └─ while (ret && n < 10)  ret = ParseMCOMMString(ret)   ← walks concatenated frames
        └─ header check, function range check, bounds check, footer check
        └─ ProcessMCUData(frame)                 [MCU.c:192]
             └─ PrintHexBuffer("Rx", frame, expectedLen)
             └─ switch(function) …
             └─ MCOMMRcvFlags[function] = 1      ← releases any MCOMM_WaitFlag() waiter
   └─ ClearMCOMMRXBuffer(); mcu_data_available = 0
```

### 3.1 Validation performed on every inbound frame

| Check | Failure log | Source |
|---|---|---|
| `data[0] == 0x26` | `Invalid header or function: %02X %02X` | [MCU.c:202](custom/MCU.c:202) |
| `function < 13` | same as above / `Invalid Function: %02X` | [MCU.c:351](custom/MCU.c:351) |
| `frame + LENTABLE[fn] <= buffer end` | `Packet extends beyond buffer bounds` | [MCU.c:360](custom/MCU.c:360) |
| `data[len-1] == 0x7E` | `Invalid footer at index %d: %02X` | [MCU.c:213](custom/MCU.c:213) |
| ≤ 10 frames per drain | `Warning: Max packet limit reached in parsing` | [MCU.c:416](custom/MCU.c:416) |
| RS232/485 `datalen` in `1..500` | `232RX Data invalid size: %d` / `485RX …` | [MCU.c:298](custom/MCU.c:298) |

### 3.2 Per-function handling

| Function | Action | Log emitted |
|---|---|---|
| 0 `ERROR` | none | `MCU: Function: ERROR` |
| 1 `SUCCESS` | releases TX waiter | `MCU: Function: SUCCESS` *(verbose)* |
| 2 `GETPH` | copies into `PeriPheralVal` (`AN1`, `AN2`, `IP1`, `IP2`, `MainsVolt`, `IsFlash`, `IsMems`, `IsTilt`); **raises/clears `TILT_ALERT`** | `MCU: Function: GETPH`, `MCU: Mains ADC: %d (%d.%01dV) BattADC: %d` *(verbose)* |
| 3 `SETPH` | none | `MCU: Function: SETPH` |
| 5 `485RX` | `memcpy → RS485_Buffer`, `RS485_DataAvailable = 1` | `MCU: Function: 485RX` |
| 7 `232RX` | `memcpy → RS232_Buffer`, `RS232_DataAvailable = 1` | `MCU: Function: 232RX` |
| 8 `FWSTART` | none | `MCU: Function: FWSTART` |
| 9 `FWUPDATE` | sets `mota_rcv_chunk_num` | `MCU: Function: MOTA_PACKET, Chunk Num: %u` |
| 11 `VERSION` | `PeriPheralVal.MCUFirmwareVersion` | `MCU: Firmware Version: %s` |
| other | none | `MCU: Function: Unknown Handler %d` |

### 3.3 MCU liveness (`IsMCU`)

`MCUTimeout` is set to **25** by the RX callback and to **300** by `ProcessMCUData()`, then
decremented once per 30 ms thread tick. `IsMCU` is 1 while `MCUTimeout > 0`.
This flag is what `$PER` reports in its "IsMEMs" position (see §5.4 note).

---

## 4. Transmit path (M66 → MCU → RS232/RS485)

```
 SendRS232String(msg)        [Hardware.c:1154]   ── raw, no wrapper
 SendRS232Response(msg)      [SMS.c:3788]        ── wraps as "$RES,<msg>\n", QUEUED
 QueryCLSSensor()            [Sensors.c:696]     ── raw "@01E0006#"
          │
          ▼
 MCOMM_SendSerial(Is485=0, payload, len)          [MCU.c:512]
   • rejects len <= 0 or len > 500 → "Invalid payload or length: %d"
   • builds 505-byte frame: 0x26 | 0x06 | UartExchangetypedef | 0x7E
          │
          ▼
 MCOMM_SendData(buf, 505, isWait=1, waitFlag=SUCCESS, timeout=1000)  [MCU.c:452]
   • serialises all callers on volatile `IsSent`
   • bounded wait MCOMM_TX_BUSY_TIMEOUT_MS = 1500 ms, then force-claims
   • Ql_UART_Write(UART_PORT1, …)
   • waits up to 1000 ms for MCOMMRcvFlags[SUCCESS]
```

### 4.1 Transmit mutual exclusion

`IsSent` is a **non-atomic software claim**, not an OS mutex. Every thread that talks to
the MCU funnels through `MCOMM_SendData()`: the Hardware thread (1.2 s status rotation),
GPS (NMEA mirroring), Sensors (CLS query), and the Server thread (`$RES` replies).

The wait was previously an unbounded silent `while (IsSent) ThreadSleep(20);`. It is now
bounded at 1500 ms — chosen to exceed the longest per-send wait (1000 ms) so a legitimate
in-flight transfer is never stolen — after which the transmitter is force-claimed and this
is logged:

```
MCU: TX busy for 1500ms, force-claiming transmitter
```

Seeing this line means two threads collided or a previous sender died mid-send. It is a
symptom worth investigating, not a normal condition.

### 4.2 Failure logs on the TX path

| Condition | Log |
|---|---|
| `Ql_UART_Write` short write | `MCU: UART write error: %d` |
| No `SUCCESS` frame within 1000 ms | `MCU: Timeout waiting for flag %d` |
| `MCOMM_SendSerial` rejected input | `MCU: Invalid payload or length: %d` |
| Wrapper-level failure | `HARDWARE: Failed to send RS232 status message` |

### 4.3 Wire cost

Every RS232 write — even a 9-byte `@01E0006#` — puts **505 bytes** on UART1 at 115200 8N1
≈ **43.8 ms** of line time, plus the 3-byte `SUCCESS` acknowledgement. The 1.2 s rotation
therefore consumes roughly 3.7 % of UART1 bandwidth.

---

## 5. RS232 OUTBOUND — periodic status frames

### 5.1 The rotation scheduler

[custom/Hardware.c:1087](custom/Hardware.c:1087), inside the 200 ms `HardwareThreadEntry` loop:

```c
if (rs232_count++ >= 5 && !IsMotaProcessing && !SleepConfig.IsEnabled)
{
    rs232_count = 0;
    if (IsRS232ResponsePending) SendBufferedRS232Response();   // $RES has priority
    else switch (rs232_message_index++ % 4) {
        case 0: SystemInfoSend();      // $INF
        case 1: CellTowerInfoSend();   // $CEL
        case 2: PeripheralInfoSend();  // $PER
        case 3: GPSDataSend();         // $GPD
    }
}
```

| Property | Value |
|---|---|
| Slot period | 6 × 200 ms = **1.2 s** |
| Full 4-frame cycle | **≈ 4.8 s** (field measurement: 4–6 s, [LOGS.txt](LOGS.txt)) |
| Suppressed while | MCU FOTA in progress (`IsMotaProcessing`) or sleep mode active |
| `$RES` behaviour | Consumes one slot, **displacing** that slot's status frame (not delaying it — the index does not advance) |
| Master gate | `#define ENABLE_RS232_PRINT` ([custom/inc/VTS.h:67](custom/inc/VTS.h:67)) — currently **enabled** |

All frames are **comma-separated ASCII terminated with a single `\n` (0x0A)**. Despite the
header comments in the source saying `\r\n`, the code appends `"\n"` only.

### 5.2 `$INF` — System / network information

**Format**
```
$INF,<IMEI>,<FW>,<GSMState>,<CSQ>,<SIMMake>,<CurProf>,<DefProf>,<SupProf>,<SPN>,<CCID>,<IMSI>,<IP1:Port1>,<IP2:Port2>,<IP3:Port3>,<IP4:Port4>\n
```

**Live sample** ([LOGS.txt:75](LOGS.txt:75))
```
$INF,868329083378285,FQ_OG1_1.5.8,4,16,ID3P,3,3,3,airtel,89919409129426686837,404940942668683,78.46.190.117:50011,78.46.190.117:50011,13.234.160.106:8224,NA:0
```

| # | Field | Source | Notes |
|---:|---|---|---|
| 0 | `$INF` | literal | |
| 1 | IMEI | `NetWork.IMEI` | 15 digits |
| 2 | Firmware | `FQ_<PROTO_TAG>_<FIRMWAREVERSION>` | e.g. `FQ_OG1_1.5.8`. **Prefix is `FQ_`**, not `FV_` |
| 3 | GSM state | derived | `0`=No SIM, `1`=SIM OK, `2`=SIM registered, `3`=GPRS OK, `4`=Server connected |
| 4 | Signal | `GSM.SignalStrength` | CSQ 0–31 |
| 5 | SIM make | `SIM_MAKE_STR` | Compile-time; `ID3P` in this build ([VTS.h:115](custom/inc/VTS.h:115)) |
| 6 | Current profile | `VTSState.CurrentProfile` | |
| 7 | Default profile | `VTSData.DefProfile` | `3` = Airtel in field config |
| 8 | Supported profiles | `STKdata.profiles_supported` | |
| 9 | SPN | `NetWork.Network` | Truncated to 20 chars |
| 10 | CCID | `NetWork.SIMNo` | Truncated to 20 chars |
| 11 | IMSI | `NetWork.IMSI` | Truncated to 15 chars |
| 12 | Server 1 | `IP1:Port1` | |
| 13 | Server 2 | `IP2:Port2` | |
| 14 | Server 3 | `IP3:Port3` | |
| 15 | Server 4 | `IP4:Port4` | **Only present when `EXTENDED_IPS` is defined** — it is, in this build ([VTS.h:145](custom/inc/VTS.h:145)). `NA:0` = unconfigured |

**Degraded form.** When `GSM.GSMState <= SIM_NOT_DETECTED`, fields 4–8 are emitted as five
consecutive empty fields (`,,,,,`), and fields 9/10/11 are empty when their respective
state gates are not met. Field *count* stays constant, so positional parsers remain valid.

Buffer: `char ss[330]`, built with bounded `Ql_strncat`. **Frame is truncated, not
corrupted, if it would exceed 329 bytes** — a plausible risk with long SPN + 4 endpoints.

### 5.3 `$CEL` — Serving + neighbour cell information

**Format**
```
$CEL,<IMEI>,<MCC>,<MNC>,<LAC>,<CELLID>,<CSQ>,N1,<MCC1>,<MNC1>,<LAC1>,<CELLID1>,<CSQ1>,N2,…,N3,…,N4,…\n
```

**Live sample** ([LOGS.txt:102](LOGS.txt:102))
```
$CEL,868329083378285,404,40,2B10,25DB,16,N1,404,40,2B10,25DE,11,N2,,,,,,N3,,,,,,N4,,,,,
```

| Field | Source | Notes |
|---|---|---|
| MCC | `GSM.MCC` | Decimal (`404` = India) |
| MNC | `GSM.MNC` | Decimal (`40` = Airtel). **Not zero-padded here** |
| LAC | `GSM.LAC` | ASCII **hex** string |
| CELLID | `GSM.CellID` | ASCII **hex** string |
| CSQ | `GSM.SignalStrength` | 0–31 |
| `N1`–`N4` | literal markers | Always emitted, even when empty |
| Neighbour MCC/MNC/LAC/CELLID/CSQ | `GSM.NeigbourCell[i]` | `CellDB` is already CSQ-converted (`DBM_IN_CSQ` in GPRS.c), **not** dBm |

**Emission rules**
* Frame is **skipped entirely** (no output at all) when `GSM.GSMState <= SIM_NOT_DETECTED`
  ([Hardware.c:1375](custom/Hardware.c:1375)) — that slot produces silence.
* A neighbour slot is populated only when `mcc > 0 && mnc >= 0`; otherwise `N<i>,,,,,,`
  (marker + five empty fields).
* The trailing comma of the last neighbour is stripped, so `N4` ends with **five** commas
  where `N2`/`N3` show six.
* Buffer `char cel[300]`.

### 5.4 `$PER` — Peripheral / I-O state

**Format**
```
$PER,<IMEI>,<GPSModemState>,<GPSFix>,<HHMMSS>,<DDMMYY>,<IsMEMs>,<IsFlash>,<IsSOS>,<Ignition>,<out1>,<out2>,<in2>,<MainsVolt>,<BattVolt>\n
```

**Live sample** ([LOGS.txt:16](LOGS.txt:16))
```
$PER,868329083378285,1,0,114128,120826,1,1,0,1,0,0,1,12.1,4.2
```

| # | Field | Source | Format | Notes |
|---:|---|---|---|---|
| 2 | GPS modem state | `GPS.State > 0 ? 1 : 0` | `%d` | 1 = GNSS receiver responding |
| 3 | GPS fix | `GPS.GPSFix` | `%d` | 0 = no fix |
| 4 | Time | `CurrentDateTime` | `%02d%02d%02d` | `HHMMSS`, UTC |
| 5 | Date | `CurrentDateTime` | `%02d%02d%02d` | `DDMMYY` (`120826` = 12-Aug-2026) |
| 6 | "IsMEMs" | **`IsMCU`** | `%d` | ⚠ Reports **MCU link liveness**, *not* `PeriPheralVal.IsMems` — see §10.1 |
| 7 | "IsFlash" | **literal `1`** | `%d` | ⚠ Hard-coded; never reflects real flash health — see §10.1 |
| 8 | SOS | `SOS.IsSOS` | `%d` | 1 = emergency latched |
| 9 | Ignition | `PeriPheralVal.IGN` | `%d` | |
| 10 | Output 1 | `PeriPheralVal.OP1` | `%d` | |
| 11 | Output 2 | `PeriPheralVal.OP2` | `%d` | |
| 12 | Input 2 | `PeriPheralVal.IP2` | `%d` | Input 1 is **not** in this frame |
| 13 | Mains voltage | `PeriPheralVal.MainsVolt` | `%04.1f` | Volts, from MCU ADC (§2.3) |
| 14 | Battery voltage | `PeriPheralVal.BattVolt` | `%03.1f` | Volts, from M66 `ADC_VBAT_GPIO` — **not** the MCU `BattVolt` field |

Tilt state is **not** carried in `$PER`; it surfaces only as a server alert and a log line.
Buffer `char per[200]`.

### 5.5 `$GPD` — GNSS data

**Format**
```
$GPD,<IMEI>,<TrackedSats>,<VisibleSats>,<HDOP>,<PDOP>,<Lat>,<Lon>,<Speed>,<Alt>,<Heading>\n
```

**Live sample** ([LOGS.txt:49](LOGS.txt:49))
```
$GPD,868329083378285,0,0,0.00,0.00,0.000000,0.000000,0.00,0.00,0.00
```

| # | Field | Source | Format | Units |
|---:|---|---|---|---|
| 2 | Tracked satellites | `GPS.NoOfSatalite` | `%d` | From GGA — used in solution |
| 3 | Visible satellites | `GPS.SatTotal` | `%d` | From GSV, summed over constellations. **May double-count if the receiver emits combined `GN` sentences** (noted in source, [Hardware.c:1544](custom/Hardware.c:1544)) |
| 4 | HDOP | `GPS.HDOP` | `%.2f` | |
| 5 | PDOP | `GPS.PDOP` | `%.2f` | |
| 6 | Latitude | `GPS.Latitude` | `%.6f` | Signed decimal degrees |
| 7 | Longitude | `GPS.Longitude` | `%.6f` | Signed decimal degrees |
| 8 | Speed | `GPS.Speed` | `%.2f` | km/h |
| 9 | Altitude | `GPS.Altitude` | `%.2f` | metres |
| 10 | Heading | `GPS.Heading` | `%.2f` | degrees true |

All-zero output means no fix; the frame is still emitted every cycle.
Buffer `char gpd[250]`.

---

## 6. RS232 OUTBOUND — event & alert frames

Unlike §5 these are **asynchronous**, emitted directly by `SendRS232String()` at the moment
of the event — they do **not** wait for a rotation slot.

### 6.1 `$BATT` — Battery status change

[custom/Hardware.c:376](custom/Hardware.c:376) (`UpdateBatteryStatus`), [custom/Hardware.c:1046](custom/Hardware.c:1046)

| Frame | Trigger |
|---|---|
| `$BATT,<STATUS>,<V.VV>V\n` | Any change in `PeriPheralVal.BatteryStatus`, plus once at first evaluation |
| `$BATT,DISCONNECTED\n` | Presence probe confirms absence |
| `$BATT,CONNECTED\n` | Presence probe confirms return |

`<STATUS>` values (`BatteryStatusTypedef`, [Hardware.h:98](custom/inc/Hardware.h:98)):

| Enum | String | Meaning |
|---:|---|---|
| 0 | `NO_BATTERY` | Rail below `BATT_ABSENT_VOLT`, or presence flag cleared |
| 1 | `CHARGING` | Mains present, below full threshold |
| 2 | `NOT_CHARGING` | Running on battery (`IsMain == 0`) |
| 3 | `FULL` | Mains present and `BattVolt >= BATT_FULL_VOLT` (4.15 V) |
| 4 | `UNKNOWN` | Mains present and presence is undeterminable |

**Current build behaviour.** `BATT_PRESENCE_DETECT_SUPPORTED == 0`
([Hardware.h:129](custom/inc/Hardware.h:129)) because this hardware has no charger STAT/EN
signal — the rail sits at ~4.09–4.2 V with or without a cell fitted. Consequently:

* `CHARGING` and `FULL` are **never emitted**. With mains present the firmware reports
  **`UNKNOWN`**; on battery it reports `NOT_CHARGING`.
* `$BATT,CONNECTED` / `$BATT,DISCONNECTED` are **compiled out entirely** — the whole
  presence-probe block sits inside `#if BATT_PRESENCE_DETECT_SUPPORTED`.

Enum numbering is deliberately append-only so existing `$BATT` parsers stay valid if a
presence signal is added later.

### 6.2 `$RCV` — System recovery / watchdog telemetry

[custom/SystemRecovery.c:199](custom/SystemRecovery.c:199)

**Format:** `$RCV,<CLASS>,<EVENT>[,<KEY>=<VALUE>]…\n` — buffer `char buffer[220]`.

> ### ⚠ Currently produces no output
> The emitter is guarded by:
> ```c
> #if SYSTEM_RECOVERY_ENABLE && SYSTEM_RECOVERY_RS232_LOG_ENABLE
> #ifdef ENABLE_RS232_PRINT
> ```
> `SYSTEM_RECOVERY_RS232_LOG_ENABLE` is **never `#define`d anywhere in the tree** — it is
> only ever *tested*. Under C preprocessor rules an undefined identifier in `#if` evaluates
> to `0`, so **every `$RCV` frame is compiled out of the current build**. The table below
> documents the designed frame set; to activate it, add
> `#define SYSTEM_RECOVERY_RS232_LOG_ENABLE 1` to
> [custom/config/custom_feature_def.h](custom/config/custom_feature_def.h). See §10.2.

| Frame | Meaning | Line |
|---|---|---|
| `$RCV,BOOT,REASON=<n>,<name>` | Power-on reason at startup | [SystemRecovery.c:382](custom/SystemRecovery.c:382) |
| `$RCV,WDT,RESET_BOOT,TOTAL=<n>` | This boot followed a watchdog reset | [:386](custom/SystemRecovery.c:386) |
| `$RCV,WDT,START,ID=<id>,TO=<ms>` | Watchdog armed | [:659](custom/SystemRecovery.c:659) |
| `$RCV,WDT,START_FAIL,RET=<r>,IV=<ms>,ATT=<n>` | Arm attempt failed | [:644](custom/SystemRecovery.c:644) |
| `$RCV,WDT,ALIVE,ID=<id>,UP=<s>,GSM=<st>,PRF=<p>` | Periodic feed heartbeat | [:936](custom/SystemRecovery.c:936) |
| `$RCV,WDT,STOP,ID=<id>` | Watchdog stopped | [:957](custom/SystemRecovery.c:957) |
| `$RCV,WDT,DISABLED` / `$RCV,WTD,DISABLED` | Watchdog disabled (note the **`WTD` typo** at [:1141](custom/SystemRecovery.c:1141)) | [:673](custom/SystemRecovery.c:673) |
| `$RCV,WDT,ACTIVE,ID=<id>` | Watchdog confirmed active post-init | [:1124](custom/SystemRecovery.c:1124) |
| `$RCV,WDT,CRITICAL,NO_WDT,RETRIES=<n>` | Could not obtain a watchdog | [:618](custom/SystemRecovery.c:618) |
| `$RCV,WDT,NO_WDT,CHECK_EARLY_INIT` / `,POST_INIT` | Watchdog missing at named stage | [:799](custom/SystemRecovery.c:799) |
| `$RCV,WDT,BLOCK,TASK=GPRS,AGE=<ms>,TO=<ms>` | GPRS task stalled past `SYSTEM_WATCHDOG_GPRS_STALL_MS` (180 s) | [:837](custom/SystemRecovery.c:837) |
| `$RCV,WDT,GPS_FAULT_RESET,UP=<s>` | GNSS fault forced a reset | [:867](custom/SystemRecovery.c:867) |
| `$RCV,WDT,SERVER_CONN_RESET,UP=<s>` | No server connectivity for `SYSTEM_CONNECTION_WATCHDOG_TIMEOUT_MS` (2 h) | [:899](custom/SystemRecovery.c:899) |
| `$RCV,WDT,BOOT_HANG,STOP_FEED` | Boot hang detected; feeding stopped to force reset | [:743](custom/SystemRecovery.c:743) |
| `$RCV,WDT,SERVICE_INIT[_FAIL],RET=,PIN=,MS=` | External WDT service-pin init | [:494](custom/SystemRecovery.c:494) |
| `$RCV,WDT,TEST_ARM,MODE=,DELAY=,ACTIVE=` / `TEST_FIRE,MODE=` / `TEST_CLEAR` / `TEST_SKIP,MS=` | Watchdog self-test command flow | [:155](custom/SystemRecovery.c:155) |
| `$RCV,WDT,RESET_KEEP_ACTIVE,ID=<id>` | Reset requested, watchdog left armed | [:1072](custom/SystemRecovery.c:1072) |
| `$RCV,RESET,REASON=<str>` | Software reset requested | [:1061](custom/SystemRecovery.c:1061) |
| `$RCV,RESET,IMMINENT,DELAY=<ms>,REASON=<str>` | Reset about to execute | [:1081](custom/SystemRecovery.c:1081) |
| `$RCV,EXC,RET=<r>,VALID=<n>` | Exception record read | [:291](custom/SystemRecovery.c:291) |
| `$RCV,EXC2,TYPE=,SERIAL=,DIAG=` | Exception detail | [:300](custom/SystemRecovery.c:300) |
| `$RCV,VLOW,SRC=<s>,ALERT=FLAGGED\|ACTIVE` | Low-voltage condition | [:319](custom/SystemRecovery.c:319) |
| `$RCV,VURC,TYPE=<n>,<name>` | Voltage URC from modem | [:983](custom/SystemRecovery.c:983) |
| `$RCV,VWARN,UNDER,CRITICAL=1` / `$RCV,VWARN,OVER` | Under/over-voltage warning | [:992](custom/SystemRecovery.c:992) |
| `$RCV,VPDN,UNDER,SHUTDOWN=START` / `$RCV,VPDN,OVER` | Voltage-triggered shutdown | [:999](custom/SystemRecovery.c:999) |
| `$RCV,STAT,SRC=<s>,UP=<s>,IMEI=<imei>` | Periodic field-status snapshot | [:447](custom/SystemRecovery.c:447) |
| `$RCV,VOLT,CB=,BP=,TH=,MN=,MRET=,MMV=,CAP=` | Voltage detail snapshot | [:450](custom/SystemRecovery.c:450) |
| `$RCV,GSM,ST=,CSQ=,DEN=,NET=,CRET=,RSSI=,BER=,GRET=,GSM=,PRET=,GPRS=` | GSM detail snapshot | [:458](custom/SystemRecovery.c:458) |

### 6.3 Vehicle-behaviour alerts — *not* on RS232

`HARSH_ACC_ALERT`, `HARSH_BRK_ALERT`, `RASH_TURN_ALERT`, `OVER_SPEED_ALERT`,
`SOS_ON/OFF/TMP`, `MAINS_FAIL/RES`, `TILT_ALERT`, `TAMPER_ALERT`, `IMPACT_ALERT`,
geofence in/out, `BATT_LOW*`, `IGN_ON/OFF` (see `ALERT_COUNT = 21`,
[custom/inc/Alert.h:34](custom/inc/Alert.h:34)) are raised via `AddAlert()` and travel to the
**TCP server** as packet alerts.

The RS232 `SendRS232String()` calls that used to mirror them are **commented out** at every
site — [Hardware.c:489](custom/Hardware.c:489), [:497](custom/Hardware.c:497),
[:504](custom/Hardware.c:504), and ~15 sites in [Server.c:4305–4543](custom/Server.c:4305).
They appear on the **debug trace UART only**, e.g.:

```
SYSTIC: ********* GPS HARSH BRAKE ALERT (dS=-3.42)********
SYSTIC: ********* GPS RASH TURN ALERT********
MCU: [TILT_DEBUG] TILT DETECTED! Calling AddAlert(TILT_ALERT=4)
```

**Consequence:** an RS232-attached display or test jig currently sees battery status
(`$BATT`) and command replies (`$RES`) as its only event traffic — it cannot observe
driving-behaviour or SOS alerts. Enabling them means uncommenting those calls **and**
accepting an extra 505-byte MCOMM frame per alert.

### 6.4 CLS fuel-sensor query (outbound, raw)

[custom/Sensors.c:696](custom/Sensors.c:696) — `SendRS232String("@01E0006#")`

Scheduled by `Systic` **3 seconds before each sensor packet is due**, and only when
`SensorsConfig.isEnabled` and the sensor interval ≥ 5 s ([Systic.c:156](custom/Systic.c:156)).
Not `$`-framed; it is the CLS vendor protocol verbatim.

### 6.5 Disabled outbound paths

| Function | State | Location |
|---|---|---|
| `SendNMEAToRS232()` — raw NMEA mirroring | Implemented, **call site commented out** | [Hardware.c:1118](custom/Hardware.c:1118) |
| `SystemStateSend()` — human-readable one-line status (`FV_` prefix, `Server OK`, `Prof:`, `ccid:`, `csq:`, `SPN:`, `GPS:`, `SOS:`) | Implemented, **call site commented out** | [Hardware.c:1125](custom/Hardware.c:1125) |
| Incoming-call notification `<-- Coming call, number:… -->` | **Active** — sent via `MCOMM_SendSerial(0,…)` on RS232 | [main.c:373](custom/main.c:373) |

---

## 7. RS232 INBOUND — command interface

### 7.1 Ingest chain

```
 MCU frame func=7 (232RX) ──► RS232_Buffer{datalen,data}, RS232_DataAvailable = 1
                                        │
                    polled by the Server thread:
                      • handleIncomingMessages()  [Server.c:5250]   ← PROTO_OG path
                      • handleCDACProtocol()      [Server.c:5925]   ← PROTO_CDAC path
                      • inside HTTP/TCP response wait loops
                        [Server.c:5065], [Server.c:5161]  ← keeps commands alive
                        while the thread is blocked waiting on a server reply
                                        │
                                        ▼
                            ProcessRS232OTAData()   [SMS.c:3854]
```

`RS232_DataAvailable` is a **single-slot flag with no queue**. If a second `232RX` frame
arrives before the Server thread consumes the first, the buffer is overwritten and the
first command is silently lost. There is no log line for this case.

Likewise, at the link layer, if a new UART burst arrives while `mcu_data_available` is
still set, the burst is dropped with:
```
MCU: MCU Rcv %d While Processing Data
```

### 7.2 Normalisation

`NormalizeRS232Command()` ([SMS.c:3830](custom/SMS.c:3830)) is applied to every inbound
RS232 payload (RS485 does **not** get this treatment):

1. Payload is NUL-terminated at `datalen` (or at byte 499 if `datalen >= 500`).
2. Leading and trailing `\r`, `\n`, space, tab are trimmed.
3. A single leading `$` is stripped, so `$SET APN airtelgprs.com` and
   `SET APN airtelgprs.com` are equivalent.
4. Empty result → the flag is cleared and nothing is processed (silent, no reply).

### 7.3 Dispatch

```c
if (d[0] == '@' && ParseCLSResponse(d, RS232_Buffer.datalen))   → CLS fuel sensor, done
else if (strncmp(d,"SET ",4)|"GET "|"CLR ")                     → DecodeOTAData(d, OTA_SRC_RS232)
else                                                            → DecodeSMS(d, OTA_SRC_RS232)
```

`OTA_SRC_RS232 = 6`, `OTA_SRC_RS485 = 7` ([custom/inc/SMS.h:24](custom/inc/SMS.h:24)).
This source code is threaded through the whole command stack so replies return to the
originating transport.

> ⚠ **In the `PROTO_OG` build the first branch is a dead end.** `DecodeOTAData()` for
> `PROTO_OG` is an 8-line wrapper ([Server.c:3777](custom/Server.c:3777)) that acts only on
> `$`-prefixed AMD3 frames — and the normaliser has already stripped the `$`. Every
> `SET `/`GET `/`CLR `-prefixed RS232 command is therefore a **silent no-op**. Use the
> no-space form (`GETRSTEST`) to reach the working decoder. Full analysis in §10.11.

Log lines produced on this path (tag `OTA`):
```
OTA: Processing RS232 data, length: %d
OTA: RS232 data handled as CLS Sensor response
OTA: RS232 OTA command: %s
OTA: OTA Data: %s
SERVER: \r\nParsing RS232 OTA Data...
```

### 7.4 CLS fuel-sensor response (inbound)

`ParseCLSResponse()` ([custom/Sensors.c:715](custom/Sensors.c:715))

**Format:** `@<ID:2><CMD:1><LEN:2 hex ASCII><payload…><CHK:2><#>`
**Example:** `@01E130644332180384091068E0#`

| Check | Rule |
|---|---|
| Minimum length | `dataLen >= 20` |
| Framing | `data[0] == '@'` and `data[dataLen-1] == '#'` |
| Command code | `data[3] == 'E'` (fuel level) — anything else is rejected |
| Length consistency | `dataLen == contentLength + 9` |
| Checksum | Byte-sum & 0xFF over the content range |

Failure logs (via `nwy_dbg_log`, tag `SENSOR`):
```
SENSOR: CLS Response: ID=%s, Cmd=%c, ContentLen=%d
SENSOR: CLS packet length mismatch: expected %d, got %d
```

> ⚠ The length argument passed from the RS232 path is the **pre-normalisation** `datalen`,
> which rejects any CLS reply that ends in CR/LF. See §10.9.

### 7.5 `$RES` — command reply frame

**Format:** `$RES,<response text>\n` — built in `SendRS232Response()`
([SMS.c:3788](custom/SMS.c:3788)), buffer `char formatted_response[300]`.

**Delivery is deferred, single-slot, last-writer-wins:**

```
SendRS232Response(text)
  └─ QueueRS232Response("$RES,text\n")        [Hardware.c:1173]
       └─ memset + Ql_strncpy into RS232ResponseBuffer[350]
       └─ IsRS232ResponsePending = 1
       └─ LOG: "HARDWARE: RS232 Response Queued: %s"

  … up to 1.2 s later, in the Hardware rotation slot …

  SendBufferedRS232Response()                  [Hardware.c:1193]
       └─ SendRS232String(RS232ResponseBuffer)
       └─ LOG: "HARDWARE: RS232 Response Sent: %s"
       └─ buffer cleared, flag reset
```

| Property | Value / consequence |
|---|---|
| Queue depth | **1** — a second reply within the same 1.2 s window **overwrites** the first |
| Latency | 0–1.2 s after the command is processed |
| Max length | 349 bytes after `$RES,` prefix (truncated by `Ql_strncpy`) |
| Slot cost | Displaces one status frame from the rotation |
| RS485 equivalent | `SendRS485Response()` sends **immediately**, no queue ([SMS.c:3807](custom/SMS.c:3807)) |

**Error reply.** If the decoder returns 0 (unrecognised), an error is returned **only when
the input looked like a command** — this suppresses replies to line noise
([SMS.c:3894](custom/SMS.c:3894)):

```
$RES,ERROR: Unknown command\n
```
Emitted only if the text contains one of `SETSENSOR`, `SETSOS`, `SETSNS`, `SET`, `GET`,
`CLR`, `APN`. RS485 has no such guard — it always replies with the error.

### 7.6 Response routing by source

`SendResponce(Sender, Resp, IsServer, IsSET)` ([custom/Server.c:4109](custom/Server.c:4109))

| `IsServer` | Non-SET command (`IsSET=0`) | SET command (`IsSET=1`) |
|---|---|---|
| `OTA_SRC_SMS` (0) | SMS to sender | SMS + param-change packet to all servers |
| `OTA_SRC_SCK_1/2/3` | Param-change string to that socket | + broadcast to all servers |
| `OTA_SRC_BLE` (5) | `BLE_SendReply()` | + broadcast |
| **`OTA_SRC_RS232` (6)** | **`SendRS232Response()` → `$RES,…`** | `$RES,…` **and** `MakeParamChangeString("RS232",…)` broadcast to all connected TCP servers |
| **`OTA_SRC_RS485` (7)** | **`SendRS485Response()` → `$RES,…`** | `$RES,…` + broadcast tagged `"RS485"` |

So a `SET` issued over RS232 is acknowledged **locally on RS232 and reported to the
backend** — the server learns the parameter changed and that it came from RS232.

Under `PROTO_CDAC` the routing collapses to SMS / RS232 / RS485 only
([Server.c:4232](custom/Server.c:4232)).

### 7.7 AMD3 `SET`/`GET`/`CLR` command set (`ParseStandardAIS140Command`)

> ⚠ **Reachable over TCP and SMS only — not over RS232 or RS485.** Documented here because
> the response it produces (`LastOTAResponse`) is tagged with the RS232/RS485 source strings,
> which is misleading. See §10.11.

**Wire format:** `$<IMEI>,<PASSWORD>,<MODE>,<CMD_ID>[:<VALUE>][,<CMD_ID>[:<VALUE>]…]*<XX>\r\n`
([SMS.c:1394](custom/SMS.c:1394))

Three authentication layers, checked in order:

| Layer | Check | On failure |
|---:|---|---|
| 1 | `<IMEI>` equals `NetWork.IMEI` | `OTA: AMD3 IMEI mismatch: got=%s exp=%s`, frame dropped |
| 2 | `<PASSWORD>` equals last 6 digits of the IMEI | `OTA: AMD3 PWD mismatch`, frame dropped |
| 3 | XOR checksum of the body vs `*<XX>` | `OTA: AMD3 chksum WARN: exp=%02X got=%02X` — **advisory only, command still executes** |

* **Mode** = `GET` | `SET` | `CLR`.
* Multiple `CMD_ID:VALUE` pairs may be comma-separated before the `*` terminator; each is
  processed in turn and each overwrites `LastOTAResponse`, so only the **last** pair's result
  is reported.
* Parse trace: `OTA: AMD3 cmd=%d mode=%d val=%s src=%d`
* Unknown id: `OTA: AMD3: unknown cmd %d`
* On completion the parser raises `CONF_CHANGE_ALERT` to trigger the acknowledgement packet.

**Result reporting.** The outcome is stored in `LastOTAResponse`
({`Source`, `Mode`, `CmdId`, `Value`, `Status`, `Pending`}) with `Source = "RS232"`, and is
embedded in the next server PVT/OA frame as ([Server.c:1357](custom/Server.c:1357)):

```
(RS232|SET|17:80:1)
     │    │   │  │ └─ status: 1 = success, 0 = failure
     │    │   │  └──── value applied / read back
     │    │   └─────── command id
     │    └─────────── mode
     └──────────────── originating transport
```

**Complete command-ID table** ([SMS.c:1531](custom/SMS.c:1531)–[SMS.c:1885](custom/SMS.c:1885)).
`✓` = supported mode; blank = the mode is a no-op returning `Status = 0`.

| ID | Parameter | GET | SET | CLR | SET validation | CLR restores |
|---:|---|:--:|:--:|:--:|---|---|
| 1 | Firmware version | ✓ | | | — | — |
| 2 | PVT server IP | ✓ | ✓ | ✓ | length 5–49 | `DEFAULT_IP1` |
| 3 | PVT server port | ✓ | ✓ | ✓ | length 2–9 | `DEFAULT_PORT1` |
| 4 | Emergency server IP | ✓ | ✓ | ✓ | length 5–49 | `DEFAULT_IP2` |
| 5 | Emergency server port | ✓ | ✓ | ✓ | length 2–9 | `DEFAULT_PORT2` |
| 6 | Control-centre number (Mob0) | ✓ | ✓ | ✓ | length 6–14 | `DEFAULT_MOB0` |
| 7 | APN | ✓ | ✓ | ✓ | length 3–29 | auto-APN |
| 8 | Data / sleep interval | ✓ | ✓ | ✓ | 5–3600 s | `DEFAULT_INV_STB` |
| 9 | Over-speed limit | ✓ | ✓ | ✓ | 11–199 | `DEFAULT_OVERSPEED` |
| 10 | Harsh-acceleration threshold | ✓ | ✓ | ✓ | ⚠ **none** | `DEFAULT_HA` |
| 11 | Harsh-braking threshold | ✓ | ✓ | ✓ | ⚠ **none** | `DEFAULT_HB` |
| 12 | Harsh-cornering threshold | ✓ | ✓ | ✓ | ⚠ **none** | `DEFAULT_RT` |
| 13 | Vehicle registration number | ✓ | ✓ | ✓ | length 3–19 | `DEFAULT_VEHREG` |
| 14 | Ignition-ON interval | ✓ | ✓ | ✓ | 5–3600 s | `DEFAULT_INV_IGN` |
| 15 | Ignition-OFF / data interval | ✓ | ✓ | ✓ | 5–3600 s | `DEFAULT_INV_STB` |
| 16 | Emergency (SOS) interval | ✓ | ✓ | ✓ | 1–300 s | `DEFAULT_INV_SOS` |
| 17 | Emergency auto-clear timeout | ✓ | ✓ | ✓ | 0–3600 s | `0` (disabled) |
| 18 | Device restart | | ✓ | | — | — |
| 19 | IMEI | ✓ | | | — | — |
| 20 | Clear emergency state | | | ✓ | — | — |
| 21 | Emergency SMS number (Mob1) | ✓ | ✓ | ✓ | length 7–13 | `DEFAULT_MOB1` |
| 22 | Signal strength (CSQ) | ✓ | | | — | — |
| 23 | RFID tag (`NA` if none active) | ✓ | | | — | — |

Notes:
* IDs 2–5 apply `UpdateConfigInFlash()` **and** `InitSockets()` — a server change forces
  socket re-establishment immediately.
* ID 7 GET returns `AUTO:<apn>` when the APN is auto-selected, otherwise the manual value.
* ID 18 replies `1`, sleeps 1 s, then calls `SystemRecovery_RequestReset("SMS_RESTART")`.
* **IDs 10, 11 and 12 accept any integer without range checking** — `Ql_atoi` result is cast
  straight to `uint16_t`. A negative or out-of-range value wraps silently.

> **Endpoint syntax constraint (known trap):** endpoints must use the **colon** form
> `PU:IP:PORT`. The comma form is broken by design in this tree and its reference tree.

### 7.8 Legacy `DecodeSMS` command set over RS232

Any RS232 line that is not `SET `/`GET `/`CLR `-prefixed and not a CLS response is handed to
`DecodeSMS(d, OTA_SRC_RS232)` ([SMS.c:1948](custom/SMS.c:1948)) — the **full SMS command
surface is reachable from RS232**, with replies returned as `$RES,…`.

**`+S*R:` short-command family** (`SRDecode`, [SMS.c:555](custom/SMS.c:555)) — `PROTO_OG` /
`PROTO_MAHARASHTRA1` only. Syntax `+S*R:<GET|SET|CLR>:<TAG>#<value>`:

| Tag | Parameter |
|---|---|
| `GIP#` | Primary server IP/port |
| `EIP#` | Emergency/secondary server |
| `PIP#` | Tertiary server |
| `APN#` | APN |
| `SOS#` | SOS number |
| `VRN#` | Vehicle registration number |
| `LOGS#` / `LOG2#` | Log server endpoints |
| `HPTI#` / `EPTI#` / `EMTD#` | Interval / emergency-mode timers |
| `IMON#` / `IMOFF#` | Immobiliser on/off |
| `RST#` | Reset |
| `FOTA#` | Firmware update trigger |
| `OSL#` / `HBT#` / `HAT#` / `RTT#` / `OVT#` | Over-speed limit, harsh brake, harsh accel, rash turn, over-speed time |
| `PGF#` / `EPSTOP#` | Geofence program / EPO stop |

**`GET …` family** ([SMS.c:2128](custom/SMS.c:2128)):
`SIMMAKE`, `OUTSTAT`, `INPSTAT`, `OPERATOR`, `PRF`, `PROFILE`, `SOSTIMEOUT`, `SOSDISABLE`,
`IMIDISABLE`, `IMI`, `VDETAIL`, `VSTATUS`, `HISTORY`/`HIST`, `FTK`, `SERVERDETAIL`,
`LOCATION`, `RSTEST`, `PANIC`, `VINFO`, `VINTERVAL`, `VBEHAVE`, `MCU`, `SENS`, `GEO`, `DISK`

**`SET …` family** ([SMS.c:2587](custom/SMS.c:2587)):
`EPO`, `FGPS`/`CGPS`, `SIMMAKE`, `OUTSTAT`, `IPCON`, `IMIDISABLE`, `IMI`, `DFTP`, `FOTA`,
`MOTA`, `PRF`, `OPERATOR`, `PROFILE`, `VID`, `SERVER`, `APN`, `INTERVAL`, `BEHAVE`,
`VEHREG`, `SOSSET`, `FOTAUPDATE`, `VRESET`, `DEFAULT`, `SOSCLR`, `SOSTIMEOUT`,
`SOSDISABLE`, `SOSSMS`, `HISTORY`/`HIST`, `FTK`, `BTS`, `SLP`, `GF:`, `SENS`

**`CLR …` family** ([SMS.c:3707](custom/SMS.c:3707)):
`HISTORY`/`HIST`, `GF`, `SOS`, `DISK`

**Bare commands:** `ACTV`, `HCHK`, `TESTRIG`/`TESRIG`, `SOS`, `INFO`, `LOCATION`, `RESET`

**Transport self-test** — the most useful bring-up command
([SMS.c:2408](custom/SMS.c:2408)):

| Send on RS232 | Reply |
|---|---|
| `GET RSTEST` | `$RES,RS232 Test OK - Source: 6\n` |
| `GET RSTEST` (on RS485) | `$RES,RS485 Test OK - Source: 7\n` |

**Diagnostic example** — `CLR DISK` returns a formatted cleanup summary
([SMS.c:3749](custom/SMS.c:3749)):
```
$RES,Disk Cleared. Free: 1.24 MB (+0.31 MB), H:12 B:3 F:5
```

---

## 8. Logging system

### 8.1 Mechanism

`LOGData(TAG, FMT, …)` ([custom/inc/LOG.h:77](custom/inc/LOG.h:77)):

```c
LogData_Lock();                                     // OS mutex "VTS_LOG"
Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN);              // 512 bytes
snprintf(DBG_BUFFER, DBG_BUF_LEN-1, "%s: " FMT "\n", TAG, …);
Ql_Debug_Trace("%s", DBG_BUFFER);                   // payload as ARGUMENT, not format
LogData_Unlock();
```

Two deliberate hardening details, both worth preserving:
* The mutex prevents interleaved/torn lines from concurrent tasks sharing `DBG_BUFFER`.
* `Ql_Debug_Trace("%s", buf)` — **never** `Ql_Debug_Trace(buf)`. Packet payloads are
  server-influenced; a `%s` inside one would make the trace engine dereference a garbage
  pointer ([main.c:104](custom/main.c:104)).

Hard limit: **512 bytes per line** (`DBG_BUF_LEN`, an M66 `Ql_Debug_Trace` constraint).
Longer strings must go through `print_long_string()` ([main.c:78](custom/main.c:78)), which
chunks at 100 bytes and prefixes `[<start>/<total>]`.

### 8.2 Verbosity gates

| Macro | Default | Effect |
|---|---|---|
| `VTS_DEBUG_LOG_ENABLE` | `1` ([custom_feature_def.h:24](custom/config/custom_feature_def.h:24)) | Master switch. `0` compiles **all** `LOGData`/`LOGVerbose` to `do{}while(0)` and removes `DBG_BUFFER` |
| `VTS_VERBOSE_LOG_ENABLE` | `1` ([LOG.h:90](custom/inc/LOG.h:90)) | Gates `LOGVerbose` only. Added specifically to stop RS232/MCU log flooding |

`LOGVerbose` is used for the high-rate lines: the hex dumps in `PrintHexBuffer`, the
`$INF`/`$CEL`/`$PER`/`$GPD` echoes, `MCU: Sending data: …`, `MCU: Function: SUCCESS`, and
the GETPH voltage line. Set `VTS_VERBOSE_LOG_ENABLE 0` to get a quiet log that still shows
errors and state changes.

### 8.3 Tags relevant to RS232

Full list at [custom/inc/LOG.h:32](custom/inc/LOG.h:32).

| Tag | Emitted by | What you see |
|---|---|---|
| `MCU` | [MCU.c](custom/MCU.c) | Hex dumps, function decode, TX/flag/timeout errors |
| `HARDWARE` | [Hardware.c](custom/Hardware.c) | `$INF`/`$CEL`/`$PER`/`$GPD` echoes, `$RES` queue/send, `BATT_STATUS`, `BATT_CHK` |
| `OTA` | [SMS.c](custom/SMS.c) | Inbound command text, AMD3 parse results, response queueing |
| `SERVER` | [Server.c](custom/Server.c) | `Parsing RS232 OTA Data...` dispatch markers |
| `SYSTIC` | [Hardware.c](custom/Hardware.c), [Systic.c](custom/Systic.c) | Behaviour alerts, sensor interval scheduling |
| `RECOVERY` | [SystemRecovery.c](custom/SystemRecovery.c) | Watchdog / reset telemetry (trace twin of `$RCV`) |
| `GPS` | [GPS.c](custom/GPS.c) | NMEA forwarding status |
| `SENSOR` | [Sensors.c](custom/Sensors.c) via `nwy_dbg_log` | CLS parse results, RFID, sensor dispatch. Note: `TAG_SENSOR` is defined **locally** at [Sensors.c:15](custom/Sensors.c:15), not in `LOG.h` |

### 8.4 `PrintHexBuffer` format

[custom/MCU.c:61](custom/MCU.c:61) — 8 bytes per line, `%02X ` separated:

```
MCU: RAW UART RX[17]:
MCU: 26 02 01 01 EE 01 AD 08
MCU: 65 00 A6 0E 00 00 00 00
MCU: 7E
```

* `RAW UART RX` is truncated to the **first 64 bytes** of the burst
  ([MCU.c:164](custom/MCU.c:164)); `Rx` prints the full `LENTABLE` frame length.
* For a 505-byte RS232 frame that is **64 lines** of output at `LOGVerbose` level — the
  main reason `VTS_VERBOSE_LOG_ENABLE` exists.
* ⚠ `char line[128]` holds 8 bytes × 3 chars = 24 used; the write index `offset` is a
  `uint8_t` reset every 8 bytes, so it never overflows in practice.

---

## 9. End-to-end timing summary

| Path | Latency |
|---|---|
| MCU asserts data → `RS232_DataAvailable` set | ≤ 30 ms (RX thread tick) |
| `RS232_DataAvailable` → command decoded | Server-thread poll interval |
| Command decoded → `$RES` on the wire | 0–1.2 s (rotation slot) |
| `$RES` frame transmit time on UART1 | ≈ 43.8 ms (505 B @ 115200) |
| Status frame period (each of 4) | ≈ 4.8 s |
| Any single status frame slot | 1.2 s |
| Peripheral poll (`GETPH`) round trip | ≤ 1000 ms, typically < 30 ms |
| MCU-link liveness timeout (`IsMCU` → 0) | 300 ticks × 30 ms = 9 s after last frame |

---

## 10. Known issues, mislabels and disabled paths

Each item below is a statement about the code as it stands, with the line that proves it.
None are speculative.

### 10.1 `$PER` field mislabels

* **Field 6 is labelled `IsMEMs` but carries `IsMCU`** ([Hardware.c:1472](custom/Hardware.c:1472)).
  The real MEMS status arrives from the MCU as `mcuData->IsMems` and is stored in
  `PeriPheralVal.IsMems` ([MCU.c:242](custom/MCU.c:242)) — but never transmitted.
  A `1` here means "MCU link alive", not "accelerometer present".
* **Field 7 `IsFlash` is hard-coded to `1`** ([Hardware.c:1477](custom/Hardware.c:1477),
  comment: *"Assume 1 for now"*). `PeriPheralVal.IsFlash` is populated from the MCU and
  discarded. Any test procedure that checks flash health via `$PER` is checking nothing.
* **Tilt is absent from `$PER`** despite being polled every `GETPH`.

### 10.2 `$RCV` telemetry is compiled out

`SYSTEM_RECOVERY_RS232_LOG_ENABLE` is referenced in four `#if` expressions in
[SystemRecovery.c](custom/SystemRecovery.c) and **defined nowhere**. It therefore evaluates
to `0` and all 40-plus `$RCV` frames are removed at compile time. The equivalent
information still reaches the debug trace UART under `TAG_RECOVERY`.

Additional detail: two frames are spelled `$RCV,WTD,…` instead of `WDT`
([:816](custom/SystemRecovery.c:816), [:1141](custom/SystemRecovery.c:1141)) — parsers must
accept both if the feature is enabled.

### 10.3 Single-slot buffers on both directions

| Buffer | Depth | Overflow behaviour | Logged? |
|---|---:|---|---|
| `RS232_Buffer` / `RS232_DataAvailable` | 1 | Newer command overwrites older | **No** |
| `RS232ResponseBuffer` / `IsRS232ResponsePending` | 1 | Newer reply overwrites older | **No** |
| `MCOMMRxBuff` / `mcu_data_available` | 1 burst | Newer burst dropped | Yes — `MCU Rcv %d While Processing Data` |

A multi-command burst on RS232 (e.g. a test script firing three `GET`s back to back) will
lose commands and/or replies with no diagnostic. This is the single most likely cause of
"the device ignored my command" reports.

### 10.4 MCU-reported battery voltage is unused

`MCUPepheralTypedef.BattVolt` is received and logged (`BattADC: 2221`) but never assigned to
`PeriPheralVal.BattVolt` ([MCU.c:256](custom/MCU.c:256)). `$PER` field 14 comes from the M66's
own `ADC_VBAT_GPIO` sampling ([Hardware.c:383](custom/Hardware.c:383)). The two sources can
disagree; the MCU value is diagnostic only.

### 10.5 Battery presence is not determinable on this hardware

`BATT_PRESENCE_DETECT_SUPPORTED = 0`. With mains applied the rail reads ~4.09–4.2 V whether
or not a cell is fitted, so `$BATT` reports `UNKNOWN` rather than asserting `CHARGING`/`FULL`.
Resolving this requires a hardware STAT/EN signal from the charger IC — it cannot be fixed
in firmware.

### 10.6 Fixed 505-byte framing

Every RS232/RS485 payload frame is 505 bytes on UART1 irrespective of `datalen`. A 9-byte
CLS query costs the same wire time as a 500-byte response. If UART1 throughput ever becomes
a constraint, adding a length-aware short frame is the fix — but it is a **breaking MCOMM
change** requiring simultaneous MCU firmware update.

### 10.7 No integrity check on MCOMM

No CRC, no checksum, no sequence number. A single corrupted function byte causes the parser
to mis-length the frame, fail the footer check, and discard up to 10 frames' worth of buffer.

### 10.8 `$INF` firmware prefix inconsistency

`$INF` emits `FQ_<tag>_<ver>` ([Hardware.c:1228](custom/Hardware.c:1228)) while the disabled
`SystemStateSend()` emits `FV_<tag>_<ver>` ([Hardware.c:521](custom/Hardware.c:521)). If
`SystemStateSend` is ever re-enabled, host parsers must accept both.

### 10.9 CLS parse is given a stale length

[SMS.c:3876](custom/SMS.c:3876) calls:

```c
if (d[0] == '@' && ParseCLSResponse(d, RS232_Buffer.datalen))
```

`d` is the **normalised** string (trimmed of `\r\n`/space/tab, and of a leading `$`), but
the length passed is the **pre-normalisation** `RS232_Buffer.datalen`. `ParseCLSResponse()`
then tests `data[dataLen-1] != '#'` and `dataLen != contentLength + 9`
([Sensors.c:731](custom/Sensors.c:731), [:757](custom/Sensors.c:757)).

A CLS sensor that terminates its reply with CR/LF — i.e. `@01E…#\r\n` — is normalised to
`@01E…#` (length *n*) but validated against length *n+2*, so both checks fail and the
response is rejected. It then falls through to `DecodeSMS()` and is discarded. The parse
only succeeds when the sensor sends `#` as the true last byte with no trailing whitespace.

Fix is one line: pass `Ql_strlen(d)` instead of `RS232_Buffer.datalen`. The equivalent call
in [Sensors.c:668](custom/Sensors.c:668) (`ParseSensorData` path) already uses a length
consistent with its own buffer.

### 10.10 RS485 lacks the RS232 hardening

`ProcessRS485OTAData()` ([SMS.c:3903](custom/SMS.c:3903)) does **not** call
`NormalizeRS232Command()` and does **not** apply the looks-like-a-command guard before
replying `ERROR: Unknown command`. RS485 line noise therefore generates spurious replies,
and RS485 commands must not carry a leading `$` or stray whitespace.

---

### 10.11 `SET `/`GET `/`CLR ` over RS232 is a silent no-op in the `PROTO_OG` build

This is the most consequential finding in this document, and it invalidates the obvious
bring-up procedure.

**Chain of evidence**

1. `ProcessRS232OTAData()` routes on a space-terminated verb
   ([SMS.c:3885](custom/SMS.c:3885)):
   ```c
   if (Ql_strncmp(d,"SET ",4)==0 || Ql_strncmp(d,"GET ",4)==0 || Ql_strncmp(d,"CLR ",4)==0) {
       DecodeOTAData(d, OTA_SRC_RS232);
       result = 1;                       // ← unconditional success
   } else {
       result = DecodeSMS(d, OTA_SRC_RS232);
   }
   ```
2. The large legacy `DecodeOTAData()` at [Server.c:3315](custom/Server.c:3315) is enclosed by
   `#if defined(PROTO_CDAC)` opened at [Server.c:2511](custom/Server.c:2511) and closed at
   [Server.c:3773](custom/Server.c:3773). It is **not compiled** in this build. *(The
   `#ifdef PROTO_OG` block nested at [Server.c:3333](custom/Server.c:3333) is unreachable dead
   code — `PROTO_CDAC` and `PROTO_OG` are mutually exclusive.)*
3. The `DecodeOTAData()` that **is** compiled for `PROTO_OG`
   ([Server.c:3777](custom/Server.c:3777)) is:
   ```c
   void DecodeOTAData(char* buff, uint8_t isserver)
   {
       if (buff && buff[0] == '$' && Ql_strstr(buff, NetWork.IMEI))
           ParseStandardAIS140Command(buff, isserver);
   }
   ```
4. `NormalizeRS232Command()` has already stripped the leading `$`
   ([SMS.c:3848](custom/SMS.c:3848)), so `buff[0]` can never be `'$'` on this path.

**Result:** the command is discarded with no action. And because `result = 1` is assigned
unconditionally, the error-reply guard at [SMS.c:3894](custom/SMS.c:3894) is skipped too — so
there is **no `$RES` at all**, not even an error.

**Observable behaviour**

| Input on RS232 | Path taken | Result |
|---|---|---|
| `GET RSTEST` | `DecodeOTAData` wrapper → `buff[0] != '$'` | **Nothing.** No action, no reply |
| `GETRSTEST` | `DecodeSMS` → `strstr(msg,"GET")`, `fn+=3`, `strstr(fn,"RSTEST")` | `$RES,RS232 Test OK - Source: 6` |
| `SET APN airtelgprs.com` | `DecodeOTAData` wrapper | **Nothing** |
| `SETAPN airtelgprs.com` | `DecodeSMS` | Executed, `$RES` returned |

**Scope.** The defect is specific to RS232 and RS485. SMS and BLE call `DecodeSMS()`
directly from `handleIncomingMessages()` ([Server.c:5238](custom/Server.c:5238)) with no
verb pre-routing, so `GET RSTEST` over SMS works normally. The pre-routing was added to
carry AMD3 frames onto the serial transports; it instead removed the legacy spaced command
form from those two transports only.

**Secondary consequence — AMD3 is unreachable over RS232.** An AMD3 frame
`$<IMEI>,<PWD>,GET,001*XX` is normalised to `<IMEI>,<PWD>,GET,001*XX` (leading `$` stripped),
does not begin with a spaced verb, and so goes to `DecodeSMS()`. No handler matches, the
error guard fires because the text contains `GET`, and the host receives
`$RES,ERROR: Unknown command`. AMD3 commands can only be delivered over TCP or SMS.

**Minimal fix options** (not applied — this pass is documentation only):

* *Least invasive:* in `ProcessRS232OTAData()`, fall through to `DecodeSMS()` when
  `DecodeOTAData()` does not consume the input, instead of setting `result = 1`
  unconditionally.
* *Restores AMD3 too:* stop stripping `$` in `NormalizeRS232Command()` when the payload
  contains the device IMEI, and route on the presence of `$`+IMEI rather than on the verb.

Either change alters the RS232 command contract and needs a protocol-revision bump in
[RS232_PROTOCOL.md](RS232_PROTOCOL.md).

---

## 11. Verification procedures

### 11.1 Bench setup

* RS232 DB9 → USB-serial → terminal at the RS232 line rate configured on the companion MCU
  (the **115200** figure in this document is the M66↔MCU link, not necessarily the DB9 rate —
  confirm against the MCU firmware).
* Optionally a second USB-serial on the M66 `UART_PORT2` at 115200 for the trace log.

### 11.2 Confirm the outbound rotation

Expect, roughly every 1.2 s, one of four frames, cycling with a ~4.8 s period:

```
$PER,868329083378285,1,0,114128,120826,1,1,0,1,0,0,1,12.1,4.2
$GPD,868329083378285,0,0,0.00,0.00,0.000000,0.000000,0.00,0.00,0.00
$INF,868329083378285,FQ_OG1_1.5.8,4,16,ID3P,3,3,3,airtel,899194…,404940…,78.46.190.117:50011,78.46.190.117:50011,13.234.160.106:8224,NA:0
$CEL,868329083378285,404,40,2B10,25DB,16,N1,404,40,2B10,25DE,11,N2,,,,,,N3,,,,,,N4,,,,,
```

**A missing `$CEL` is expected, not a fault**, when the SIM is not detected (§5.3).

### 11.3 Confirm the inbound path

| Step | Send | Expect on RS232 | Expect in trace |
|---|---|---|---|
| 1 | `GETRSTEST\r\n` | `$RES,RS232 Test OK - Source: 6` within 1.2 s | `OTA: RS232 OTA command: GETRSTEST` |
| 2 | `$GETRSTEST\r\n` | identical (leading `$` stripped) | same |
| 3 | `GETVSTATUS\r\n` | `$RES,<status text>` | `HARDWARE: RS232 Response Queued: …` |
| 4 | `GET RSTEST\r\n` | **nothing** — the space routes it to the no-op `DecodeOTAData` wrapper | `OTA: RS232 OTA command: GET RSTEST` and nothing after |
| 5 | `ZZZZ\r\n` | *(nothing)* | `OTA: RS232 OTA command: ZZZZ` |
| 6 | `GETNONSENSE\r\n` | `$RES,ERROR: Unknown command` | as above |

Step 1 vs 4 is the decisive check for §10.11 — the **no-space form works, the spaced form is
a silent no-op**. Step 5 vs 6 exercises the noise guard: only text containing
`SET`/`GET`/`CLR`/`APN` earns an error reply.

### 11.4 Confirm the MCOMM link

Watch the trace UART for the 1 Hz peripheral poll:

```
MCU: Sending data: len=3, isWait=1, waitFlag=2, timeout=1000
MCU: RAW UART RX[17]:
MCU: 26 02 …
MCU: Function: GETPH
MCU: Mains ADC: 494 (12.3V) BattADC: 2221
```

Absence of `Function: GETPH` for > 9 s drives `IsMCU` to 0, which shows up as `$PER` field 6
flipping to `0`.

### 11.5 Red flags in the trace

| Line | Meaning |
|---|---|
| `MCU: TX busy for 1500ms, force-claiming transmitter` | Two threads contended for the MCU link, or a sender died mid-send |
| `MCU: Timeout waiting for flag 1` | MCU did not acknowledge a `232TX`/`485TX` — link or MCU firmware fault |
| `MCU: MCU Rcv %d While Processing Data` | Inbound burst dropped; commands may have been lost |
| `MCU: Invalid footer at index %d` | Framing desync — check baud/wiring or a partial write |
| `MCU: 232RX Data invalid size: %d` | MCU sent `datalen` outside 1..500 |
| `HARDWARE: Failed to send RS232 status message` | `MCOMM_SendSerial` returned non-1 |

---

## 12. Configuration reference

| Symbol | Value | File | Effect on RS232 |
|---|---|---|---|
| `ENABLE_RS232_PRINT` | defined | [VTS.h:67](custom/inc/VTS.h:67) | **Master gate.** Undefine and every RS232 emitter early-returns; the inbound path still works |
| `ENABLE_RS232_FAST` | commented out | [VTS.h:69](custom/inc/VTS.h:69) | Reserved; no effect today |
| `MCOMM_UART_PORT` | `UART_PORT1` | [MCU.h:10](custom/inc/MCU.h:10) | Link to companion MCU |
| `MCOMM_UART_BAUDRATE` | `115200` | [MCU.h:11](custom/inc/MCU.h:11) | |
| `MCOMM_RX_BUFF_SIZE` | `1024` | [MCU.h:13](custom/inc/MCU.h:13) | Holds at most 2 × 505-byte frames |
| `MCOMM_COM_URT_EXG_BUFF_SIZE` | `500` | [MCU.h:67](custom/inc/MCU.h:67) | Max RS232/RS485 payload |
| `MCOMM_TX_BUSY_TIMEOUT_MS` | `1500` | [MCU.c:450](custom/MCU.c:450) | Transmitter force-claim threshold |
| `RS232_RESPONSE_BUFFER_SIZE` | `350` | [Hardware.c:37](custom/Hardware.c:37) | `$RES` queue slot |
| `VTS_DEBUG_LOG_ENABLE` | `1` | [custom_feature_def.h:24](custom/config/custom_feature_def.h:24) | All logging |
| `VTS_VERBOSE_LOG_ENABLE` | `1` | [LOG.h:90](custom/inc/LOG.h:90) | Hex dumps + frame echoes |
| `DBG_BUF_LEN` | `512` | [LOG.h:17](custom/inc/LOG.h:17) | Hard M66 trace limit |
| `EXTENDED_IPS` | defined | [VTS.h:145](custom/inc/VTS.h:145) | Adds the 4th endpoint field to `$INF` |
| `SIM_MAKE_STR` | `"ID3P"` | [VTS.h:115](custom/inc/VTS.h:115) | `$INF` field 5 |
| `PROTO_TAG` | `"OG1"` | [VTS.h:410](custom/inc/VTS.h:410) | `$INF` field 2 |
| `BATT_PRESENCE_DETECT_SUPPORTED` | `0` | [Hardware.h:129](custom/inc/Hardware.h:129) | Disables `$BATT,CONNECTED/DISCONNECTED` and `CHARGING`/`FULL` |
| `SYSTEM_RECOVERY_RS232_LOG_ENABLE` | **undefined → 0** | — | Disables all `$RCV` frames |

---

## 13. Frame index (quick reference)

| Frame | Direction | Cadence | Section |
|---|---|---|---|
| `$INF,…` | Device → RS232 | Every ~4.8 s | §5.2 |
| `$CEL,…` | Device → RS232 | Every ~4.8 s (skipped without SIM) | §5.3 |
| `$PER,…` | Device → RS232 | Every ~4.8 s | §5.4 |
| `$GPD,…` | Device → RS232 | Every ~4.8 s | §5.5 |
| `$BATT,…` | Device → RS232 | On battery-status change | §6.1 |
| `$RCV,…` | Device → RS232 | On recovery events — **currently compiled out** | §6.2 |
| `$RES,…` | Device → RS232 | Reply to an inbound command, ≤ 1.2 s | §7.5 |
| `@01E0006#` | Device → RS232 | 3 s before each sensor packet | §6.4 |
| `<-- Coming call, … -->` | Device → RS232 | On incoming voice call | §6.5 |
| `@01E…#` | RS232 → Device | CLS fuel sensor reply | §7.4 |
| `SET`/`GET`/`CLR …` | RS232 → Device | Operator command | §7.7 |
| `+S*R:…`, `ACTV`, `HCHK`, … | RS232 → Device | Legacy SMS command set | §7.8 |
