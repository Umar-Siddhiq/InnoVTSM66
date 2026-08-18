# RS232 Serial Protocol Specification

**Product:** InnoVTSM66 AIS-140 Vehicle Tracking Unit
**Protocol name:** VTS-RS232 ASCII Line Protocol
**Protocol revision:** 1.0
**Applies to firmware:** `1.5.8`, build `PROTO_OG` (`PROTO_TAG = "OG1"`)
**Document status:** Baseline — describes implemented behaviour, not intended behaviour
**Date:** 2026-08-12

---

## Revision history

| Rev | Date | Firmware | Change |
|---|---|---|---|
| 1.0 | 2026-08-12 | 1.5.8 / OG1 | Initial baseline. Documents `$INF`, `$CEL`, `$PER`, `$GPD`, `$BATT`, `$RES`, host command grammar, and the reserved/non-operative frame set. |

---

## 1. Scope and conventions

### 1.1 Scope

This document specifies the ASCII line protocol carried on the **RS232 port** of the
InnoVTSM66 tracking unit. It is the interface contract between the unit and any host —
an in-vehicle display, a diagnostic jig, a fleet-body controller, or a test harness.

It defines only what crosses the RS232 connector. The internal MCOMM binary transport
between the M66 module and the companion MCU is **out of scope**; it is documented
separately in [RS232_INTERFACE_SPEC.md](RS232_INTERFACE_SPEC.md).

### 1.2 Requirement language

| Term | Meaning |
|---|---|
| **MUST** / **MUST NOT** | Absolute requirement for conformance |
| **SHOULD** / **SHOULD NOT** | Recommended; deviate only with a documented reason |
| **MAY** | Optional |
| **RESERVED** | Defined in this document but **not operative** in the referenced firmware build |

### 1.3 Notation

Packet syntax is given in a restricted BNF:

```
<name>        a named field
[ x ]         optional element
{ x }         element repeated zero or more times
( a | b )     alternatives
"literal"     literal characters
<LF>          0x0A
<CR>          0x0D
```

### 1.4 Terminology

| Term | Definition |
|---|---|
| **Unit** | The InnoVTSM66 tracking device |
| **Host** | Equipment connected to the unit's RS232 port |
| **Frame** | One complete protocol line, terminator included |
| **Status frame** | Unsolicited periodic frame from unit to host (§4) |
| **Event frame** | Unsolicited asynchronous frame from unit to host (§5) |
| **Command** | Host-to-unit request (§6) |
| **Response** | `$RES` frame returned for a command (§7) |
| **Slot** | One 1.2 s transmit opportunity in the unit's output scheduler (§3.4) |

---

## 2. Physical and character layer

### 2.1 Electrical

| Property | Value |
|---|---|
| Signalling | EIA/TIA-232-F, 3-wire (TXD, RXD, GND) |
| Flow control | None |
| Line parameters | Set by the companion MCU firmware, **not** by the tracker application. Confirm against the MCU build before integration. |

> The unit's application processor never touches the RS232 UART directly; RS232 framing,
> baud rate and parity are terminated by a companion microcontroller. This document
> therefore does not assert a baud rate.

### 2.2 Character encoding

| Rule | Requirement |
|---|---|
| Character set | 7-bit printable US-ASCII (0x20–0x7E) plus `<LF>` |
| Field separator | Comma `,` (0x2C) |
| Frame start | Dollar `$` (0x24) for all protocol frames |
| Frame terminator, unit → host | Single `<LF>` (0x0A). `<CR>` is **not** emitted |
| Frame terminator, host → unit | `<CR>`, `<LF>`, `<CR><LF>` or none — all accepted (§6.2) |
| Decimal separator | Period `.` |
| Numeric sign | Leading `-` where applicable; no `+` |

> **Non-conformance note.** Source comments in the firmware describe `\r\n` terminators.
> The implementation appends `"\n"` only. Hosts **MUST** tolerate a bare `<LF>` and
> **SHOULD** tolerate `<CR><LF>` for forward compatibility.

### 2.3 Frame size limits

| Direction | Limit | Behaviour at limit |
|---|---:|---|
| Unit → host, `$INF` | 329 bytes | Silently truncated |
| Unit → host, `$CEL` | 299 bytes | Silently truncated |
| Unit → host, `$GPD` | 249 bytes | Silently truncated |
| Unit → host, `$PER` | 199 bytes | Silently truncated |
| Unit → host, `$RES` | 299 bytes total | Silently truncated |
| Host → unit | 500 bytes payload | Excess discarded by the transport |

Hosts **MUST NOT** assume a frame is complete because it parsed; a truncated frame has no
terminator and **MUST** be discarded on the next `$`.

---

## 3. Protocol model

### 3.1 Roles

The unit is the **primary talker**. It transmits status frames continuously without being
polled. The host is a **listener** that **MAY** inject commands at any time.

### 3.2 Frame taxonomy

| Class | Prefix | Direction | Solicited | Section |
|---|---|---|---|---|
| System information | `$INF` | Unit → Host | No | §4.2 |
| Cell information | `$CEL` | Unit → Host | No | §4.3 |
| Peripheral state | `$PER` | Unit → Host | No | §4.4 |
| GNSS data | `$GPD` | Unit → Host | No | §4.5 |
| Battery event | `$BATT` | Unit → Host | No | §5.1 |
| Recovery event | `$RCV` | Unit → Host | No | §5.2 — **RESERVED** |
| Command response | `$RES` | Unit → Host | Yes | §7 |
| Sensor query | `@…#` | Unit → Host | No | §5.3 |
| Sensor reply | `@…#` | Host → Unit | — | §6.5 |
| Command | *(no prefix)* | Host → Unit | — | §6 |

### 3.3 Frame independence

Every frame is self-contained and stateless. There is no sequence number, no session, no
handshake, and no acknowledgement of unit-originated frames. A host **MAY** join or leave
the bus at any time and **MUST** be able to resynchronise on the next `$`.

### 3.4 Transmit scheduler

The unit maintains a single output scheduler with a **1.2 s slot period**. Each slot emits
exactly one frame, selected as follows:

```
slot elapsed (1.2 s)
   │
   ├─ if a $RES response is pending  ──►  transmit $RES, consume the slot
   │
   └─ else transmit the next status frame in the rotation:
          $INF → $CEL → $PER → $GPD → $INF → …
```

| Property | Value |
|---|---|
| Slot period | 1.2 s |
| Status rotation period | 4 slots = **≈ 4.8 s** |
| Observed field period | 4–6 s (scheduler jitter) |
| `$RES` effect on rotation | Displaces the slot; the rotation index does **not** advance |
| Scheduler suspended during | Companion-MCU firmware update, sleep mode |

Event frames (§5) bypass the scheduler entirely and are transmitted at the instant of the
event.

**Ordering guarantee:** within the status rotation, order is fixed. Between event frames
and status frames there is **no ordering guarantee** — an event frame **MAY** be interleaved
between any two status frames.

---

## 4. Status frames (unit → host)

### 4.1 Common structure

```
<status-frame> ::= "$" <tag> "," <imei> "," <field> { "," <field> } <LF>
<tag>          ::= "INF" | "CEL" | "PER" | "GPD"
<imei>         ::= 15 decimal digits
```

Field count is **positionally fixed** per tag. Absent values are transmitted as **empty
fields**, never omitted, so a positional parser remains valid in all states.

---

### 4.2 `$INF` — System and network information

#### Syntax

```
"$INF," <imei> "," <firmware> "," <gsm-state> "," <csq> "," <sim-make> ","
        <cur-profile> "," <def-profile> "," <sup-profile> "," <spn> ","
        <ccid> "," <imsi> "," <ep1> "," <ep2> "," <ep3> "," <ep4> <LF>

<endpoint> ::= <host> ":" <port>
```

#### Fields

| # | Name | Type | Range / format | Empty when |
|---:|---|---|---|---|
| 0 | Tag | literal | `$INF` | — |
| 1 | IMEI | digits(15) | — | never |
| 2 | Firmware | string | `FQ_<proto>_<version>`, e.g. `FQ_OG1_1.5.8` | never |
| 3 | GSM state | uint | `0`–`4`, see below | never |
| 4 | Signal | uint | CSQ `0`–`31`; `99` = unknown | no SIM |
| 5 | SIM make | string(≤8) | Compile-time SIM/STK profile, e.g. `ID3P` | no SIM |
| 6 | Current profile | uint | Active operator profile index | no SIM |
| 7 | Default profile | uint | Configured default profile index | no SIM |
| 8 | Supported profiles | uint | Bitcount/index from STK | no SIM |
| 9 | SPN | string(≤20) | Service provider name, e.g. `airtel` | not registered |
| 10 | CCID | string(≤20) | SIM ICCID | no SIM |
| 11 | IMSI | string(≤15) | Subscriber identity | no SIM |
| 12 | Endpoint 1 | endpoint | Primary PVT server | never |
| 13 | Endpoint 2 | endpoint | Secondary server | never |
| 14 | Endpoint 3 | endpoint | Tertiary server | never |
| 15 | Endpoint 4 | endpoint | Quaternary server. `NA:0` = unconfigured | never |

#### `<gsm-state>` enumeration

| Value | Meaning |
|---:|---|
| 0 | SIM not detected |
| 1 | SIM detected |
| 2 | Registered on network, GPRS not yet up |
| 3 | GPRS attached |
| 4 | Connected to primary server |

#### Examples

Normal operation, server connected:
```
$INF,868329083378285,FQ_OG1_1.5.8,4,16,ID3P,3,3,3,airtel,89919409129426686837,404940942668683,78.46.190.117:50011,78.46.190.117:50011,13.234.160.106:8224,NA:0
```

No SIM present — fields 4–11 empty, field count preserved:
```
$INF,868329083378285,FQ_OG1_1.5.8,0,,,,,,,,,78.46.190.117:50011,78.46.190.117:50011,13.234.160.106:8224,NA:0
```

#### Notes

* Field 15 is present only in builds with the extended-endpoint option. It is present in
  firmware 1.5.8/OG1. Hosts **SHOULD** accept 15 or 16 fields.
* Endpoint host **MAY** be an IPv4 literal or a DNS name. Parsers **MUST** split on the
  **last** `:` to remain safe if a name ever contains one.
* Field 2 uses the prefix `FQ_`. A different unit function emits `FV_` for the same value;
  hosts **SHOULD** accept either prefix.

---

### 4.3 `$CEL` — Serving and neighbour cell information

#### Syntax

```
"$CEL," <imei> "," <mcc> "," <mnc> "," <lac> "," <cid> "," <csq>
        { "," <nbr-tag> "," <mcc> "," <mnc> "," <lac> "," <cid> "," <csq> } <LF>

<nbr-tag> ::= "N1" | "N2" | "N3" | "N4"
```

#### Fields

| Group | Name | Type | Format |
|---|---|---|---|
| Serving | MCC | uint | Decimal, e.g. `404` |
| Serving | MNC | uint | Decimal, **not zero-padded**, e.g. `40` |
| Serving | LAC | hex-string | Uppercase hex, no `0x`, e.g. `2B10` |
| Serving | Cell ID | hex-string | Uppercase hex, no `0x`, e.g. `25DB` |
| Serving | Signal | uint | CSQ `0`–`31` |
| Neighbour ×4 | marker | literal | `N1`…`N4`, always present |
| Neighbour ×4 | MCC / MNC / LAC / CID / Signal | as above | Empty if the slot is unused |

#### Example

Serving cell plus one neighbour:
```
$CEL,868329083378285,404,40,2B10,25DB,16,N1,404,40,2B10,25DE,11,N2,,,,,,N3,,,,,,N4,,,,,
```

#### Emission rules

1. The frame is **suppressed entirely** — no output in that slot — while `<gsm-state>` is
   `0` (SIM not detected). Hosts **MUST** treat a missing `$CEL` as "cell data unavailable",
   not as a fault.
2. All four neighbour markers are always emitted, whether or not populated.
3. The trailing comma of the frame is stripped before the terminator. As a result `N4`
   is followed by **five** commas where `N1`–`N3` are followed by six. Parsers **MUST**
   tolerate a missing final empty field.
4. Neighbour signal is reported in **CSQ units**, not dBm, despite the internal field name.

---

### 4.4 `$PER` — Peripheral and I/O state

#### Syntax

```
"$PER," <imei> "," <gnss-pwr> "," <gnss-fix> "," <hhmmss> "," <ddmmyy> ","
        <f6> "," <f7> "," <sos> "," <ignition> "," <out1> "," <out2> "," <in2> ","
        <mains-v> "," <batt-v> <LF>
```

#### Fields

| # | Name | Type | Format | Meaning |
|---:|---|---|---|---|
| 2 | GNSS power | bool | `0`/`1` | `1` = GNSS receiver responding |
| 3 | GNSS fix | bool | `0`/`1` | `1` = valid position fix |
| 4 | Time | digits(6) | `HHMMSS` | UTC |
| 5 | Date | digits(6) | `DDMMYY` | UTC. `120826` = 12 Aug 2026 |
| 6 | MCU link | bool | `0`/`1` | See caveat below |
| 7 | Flash status | bool | `0`/`1` | See caveat below |
| 8 | Emergency | bool | `0`/`1` | `1` = SOS/panic latched |
| 9 | Ignition | bool | `0`/`1` | Digital ignition sense |
| 10 | Output 1 | bool | `0`/`1` | Relay/driver output 1 |
| 11 | Output 2 | bool | `0`/`1` | Relay/driver output 2 |
| 12 | Input 2 | bool | `0`/`1` | Digital input 2 |
| 13 | Mains voltage | decimal | `%04.1f`, volts | External supply, e.g. `12.1` |
| 14 | Battery voltage | decimal | `%03.1f`, volts | Internal cell, e.g. `4.2` |

#### Example

```
$PER,868329083378285,1,0,114128,120826,1,1,0,1,0,0,1,12.1,4.2
```

Reads as: GNSS powered, no fix, 11:41:28 UTC on 12 Aug 2026, MCU link up, flash OK,
no emergency, ignition ON, both outputs off, input 2 asserted, 12.1 V supply, 4.2 V battery.

#### Field caveats — **normative for host implementers**

| Field | Documented name | Actual content | Host guidance |
|---:|---|---|---|
| 6 | "MEMS status" | **Companion-MCU link liveness.** `1` = an MCU frame was received within the last ~9 s. It does **not** report accelerometer presence. | Use as a **link-health** indicator. Do **not** display as "MEMS OK". |
| 7 | "Flash status" | **Constant `1`.** The value is hard-coded and never reflects real storage health. | **MUST NOT** be used for any diagnostic decision. Treat as reserved. |

Tilt state is **not** carried in `$PER` and has no RS232 representation.

Field 14 is sampled by the tracker's own ADC, not by the companion MCU. See §5.1 for the
limits on what battery state can be determined on this hardware.

---

### 4.5 `$GPD` — GNSS data

#### Syntax

```
"$GPD," <imei> "," <sats-used> "," <sats-visible> "," <hdop> "," <pdop> ","
        <lat> "," <lon> "," <speed> "," <alt> "," <heading> <LF>
```

#### Fields

| # | Name | Type | Format | Units |
|---:|---|---|---|---|
| 2 | Satellites used | uint | `%d` | Count in the navigation solution (GGA) |
| 3 | Satellites visible | uint | `%d` | Count in view (GSV, summed) |
| 4 | HDOP | decimal | `%.2f` | dimensionless |
| 5 | PDOP | decimal | `%.2f` | dimensionless |
| 6 | Latitude | decimal | `%.6f` | Signed decimal degrees, `+` = North |
| 7 | Longitude | decimal | `%.6f` | Signed decimal degrees, `+` = East |
| 8 | Speed | decimal | `%.2f` | km/h |
| 9 | Altitude | decimal | `%.2f` | metres above MSL |
| 10 | Heading | decimal | `%.2f` | degrees true, `0.00`–`359.99` |

#### Example

No fix — the frame is still emitted every rotation:
```
$GPD,868329083378285,0,0,0.00,0.00,0.000000,0.000000,0.00,0.00,0.00
```

#### Notes

* Position validity **MUST** be taken from `$PER` field 3 (`<gnss-fix>`), not inferred from
  non-zero coordinates in `$GPD`.
* Field 3 sums per-constellation satellite counts. If the receiver emits combined `GN`
  sentences the same satellite can be counted more than once, so field 3 **MAY** exceed the
  true number in view. Hosts **SHOULD** treat it as an indicative figure.
* Coordinates are decimal degrees, **not** NMEA `ddmm.mmmm`.

---

## 5. Event frames (unit → host)

Event frames are asynchronous and bypass the 1.2 s scheduler.

### 5.1 `$BATT` — Battery status change

#### Syntax

```
"$BATT," <status> "," <voltage> "V" <LF>
"$BATT,DISCONNECTED" <LF>                        (RESERVED — see below)
"$BATT,CONNECTED" <LF>                           (RESERVED — see below)

<voltage> ::= <int> "." <2 digits>
```

#### `<status>` enumeration

| Code | Token | Meaning | Emitted in 1.5.8/OG1 |
|---:|---|---|---|
| 0 | `NO_BATTERY` | Rail below the absent-battery threshold | Yes |
| 1 | `CHARGING` | Charging in progress | **No** |
| 2 | `NOT_CHARGING` | Running on battery, external power absent | Yes |
| 3 | `FULL` | Charge complete | **No** |
| 4 | `UNKNOWN` | External power present; charge state not determinable | Yes |

#### Emission rule

Emitted **on change only**, plus once at first evaluation after boot. There is no periodic
repeat. A host that joins the bus late **MUST** read battery voltage from `$PER` field 14
until the next change occurs.

#### Example

```
$BATT,UNKNOWN,4.09V
$BATT,NOT_CHARGING,3.87V
```

#### Hardware limitation — **normative**

This hardware provides no charger STAT or EN feedback to the tracker. With external power
applied, the battery rail reads ≈ 4.09–4.20 V whether or not a cell is fitted. The unit
therefore reports `UNKNOWN` rather than asserting a charge state it cannot determine.

Consequently, in firmware 1.5.8/OG1:

* `CHARGING` and `FULL` are **never transmitted**.
* `$BATT,CONNECTED` and `$BATT,DISCONNECTED` are **never transmitted**.

Hosts **MUST NOT** interpret the absence of `CHARGING` as a fault, and **MUST NOT** display
a charge state derived from voltage alone. The status codes are reserved so that a future
hardware revision carrying a presence signal can populate them without a protocol change.

---

### 5.2 `$RCV` — Recovery and watchdog telemetry — **RESERVED**

#### Syntax

```
"$RCV," <class> "," <event> { "," <key> "=" <value> } <LF>
```

#### Status in firmware 1.5.8/OG1

> **This frame class is not transmitted.** The emitter is compiled out because its enabling
> macro is never defined. No `$RCV` frame will appear on RS232 in this build. The definition
> is retained here so that hosts written against it remain valid when the feature is enabled.

#### Defined classes

| Class | Purpose |
|---|---|
| `BOOT` | Power-on reason |
| `WDT` | Watchdog arm / feed / stop / block / test |
| `RESET` | Software reset request and execution |
| `EXC`, `EXC2` | Stored exception records |
| `VLOW`, `VURC`, `VWARN`, `VPDN` | Voltage warnings and shutdowns |
| `STAT`, `VOLT`, `GSM` | Periodic field-status snapshots |

#### Representative frames

```
$RCV,BOOT,REASON=1,POWER_ON
$RCV,WDT,START,ID=3,TO=5000
$RCV,WDT,ALIVE,ID=3,UP=3600,GSM=4,PRF=3
$RCV,WDT,BLOCK,TASK=GPRS,AGE=181000,TO=180000
$RCV,WDT,SERVER_CONN_RESET,UP=7200
$RCV,RESET,IMMINENT,DELAY=100,REASON=SMS_RESTART
$RCV,VWARN,UNDER,CRITICAL=1
$RCV,STAT,SRC=PERIODIC,UP=3600,IMEI=868329083378285
```

#### Parser requirement

Two frames are spelled `$RCV,WTD,…` (transposed) rather than `$RCV,WDT,…`. If this class is
enabled, hosts **MUST** accept both spellings.

Field order after `<event>` is **not** guaranteed stable across firmware revisions. Hosts
**MUST** parse the `KEY=VALUE` tail by key, not by position.

---

### 5.3 `@…#` — Fuel sensor query (unit → host)

The unit emits a CLS-protocol fuel-level poll on the same RS232 line:

```
@01E0006#
```

It is **not** `$`-framed and carries no terminator. It is emitted 3 seconds before each
sensor report is due, only while the sensor feature is enabled and the configured sensor
interval is ≥ 5 s.

Hosts that are not the fuel sensor **MUST** ignore any frame beginning with `@`.

### 5.4 Incoming-call notice

On an inbound voice call the unit emits a free-text line:

```
<-- Coming call, number:+919876543210, type:129 -->
```

This is diagnostic output, not a protocol frame. It has no `$` prefix and no stable format.
Hosts **MUST** ignore any line that does not begin with `$` or `@`.

### 5.5 Driving-behaviour and emergency alerts — **not available on RS232**

Over-speed, harsh acceleration, harsh braking, harsh cornering, impact, tilt, tamper,
geofence entry/exit, SOS on/off, mains fail/restore and ignition on/off are generated by
the unit and reported to the **backend server**. They have **no RS232 representation** in
firmware 1.5.8/OG1.

A host requiring these events **MUST** derive them from `$PER` (`<sos>`, `<ignition>`,
`<mains-v>`) or obtain them from the backend. Adding them to RS232 is a firmware change,
not a configuration change.

---

## 6. Host commands (host → unit)

### 6.1 Command model

Commands are unframed ASCII text lines. There is no address field, no checksum, and no
command identifier — the unit matches on keyword substrings.

Only **one command may be outstanding at a time**. See §6.6.

### 6.2 Input normalisation

Every received RS232 payload is normalised before dispatch:

1. Leading and trailing `<CR>`, `<LF>`, space and tab are removed.
2. A single leading `$` is removed.
3. If the result is empty, the input is discarded silently.

Therefore `GETVSTATUS`, `$GETVSTATUS`, `  GETVSTATUS\r\n` and `GETVSTATUS<LF>` are
equivalent.

> **Consequence — normative.** Because step 2 strips the leading `$`, any command syntax
> that requires a literal `$` at the start of the frame **cannot be delivered over RS232**.
> See §8.1.

### 6.3 Command grammar

```
<command> ::= <verb-token> <selector> [ " " <value> ]
<verb-token> ::= "GET" | "SET" | "CLR"
```

> **Critical syntax rule.** The verb **MUST NOT** be followed by a space.
> `GETVSTATUS` is accepted. `GET VSTATUS` is **silently discarded** — no action, no
> response. See §8.2 for the reason. This applies to every `GET`/`SET`/`CLR` command in §6.4.

Where a command takes a value, the value follows the selector separated by a space:

```
SETAPN airtelgprs.com
SETVEHREG MH12AB1234
```

Matching is by substring and is **case-sensitive** (uppercase). Selectors are matched in
the order listed in §6.4; a selector that is a prefix of another **MUST** be assumed to
match the earlier entry.

### 6.4 Command catalogue

#### 6.4.1 Diagnostic and status queries (`GET`)

| Command | Response content |
|---|---|
| `GETRSTEST` | Transport self-test — returns the detected source. **Use this for bring-up.** |
| `GETVSTATUS` | Consolidated device status |
| `GETVINFO` | Vendor ID, IMEI, firmware |
| `GETVDETAIL` | Vehicle detail record |
| `GETVINTERVAL` | Configured reporting intervals |
| `GETVBEHAVE` | Driving-behaviour thresholds |
| `GETINPSTAT` | `Input 1 - <n>` / `Input 2 - <n>` |
| `GETOUTSTAT` | `Output 1 - <n>` / `Output 2 - <n>` |
| `GETPANIC` | `SOS - ON` \| `SOS - OFF` |
| `GETLOCATION` | Last known position |
| `GETOPERATOR` | Current network operator |
| `GETSIMMAKE` | Active STK/SIM profile |
| `GETPROFILE` / `GETPRF` | Operator profile state |
| `GETSERVERDETAIL` | Configured server endpoints |
| `GETIMI` | IMEI (or custom IMEI) |
| `GETHISTORY` / `GETHIST` | Stored-record count |
| `GETDISK` | Filesystem free space |
| `GETMCU` | Companion-MCU firmware version |
| `GETSENS` | Sensor configuration/value |
| `GETGEO` | Geofence configuration |
| `GETSOSTIMEOUT` | SOS auto-clear timeout |
| `GETSOSDISABLE` | SOS disable flag |
| `GETFTK` | Field-test key state |

#### 6.4.2 Configuration (`SET`)

| Command | Value | Effect |
|---|---|---|
| `SETSERVER …` | endpoint | Server endpoints. Endpoints **MUST** use the colon form `PU:IP:PORT`; the comma form is not supported |
| `SETAPN <apn>` | string | Access point name |
| `SETINTERVAL …` | seconds | Reporting intervals |
| `SETBEHAVE …` | thresholds | Driving-behaviour thresholds |
| `SETVEHREG <reg>` | string | Vehicle registration number |
| `SETVID <id>` | string | Vendor identifier |
| `SETSOSSET …` | — | SOS configuration |
| `SETSOSTIMEOUT <s>` | seconds | SOS auto-clear timeout |
| `SETSOSDISABLE <0\|1>` | flag | Disable SOS handling |
| `SETSOSSMS …` | — | SOS SMS recipients |
| `SETPROFILE <n>` / `SETPRF <n>` | index | Operator profile |
| `SETOPERATOR …` | — | Operator selection |
| `SETSIMMAKE …` | — | STK/SIM profile |
| `SETOUTSTAT <a> <b>` | `0`/`1` | Drive outputs 1 and 2 |
| `SETIPCON …` | — | IP connection mode |
| `SETFOTA …` / `SETFOTAUPDATE` | — | Trigger tracker firmware update |
| `SETMOTA …` | — | Trigger companion-MCU firmware update |
| `SETDFTP …` | — | FTP parameters |
| `SETEPO …` | — | GNSS assistance data |
| `SETFGPS` / `SETCGPS` | — | GNSS factory/cold start |
| `SETGF:…` | polygon | Program a geofence |
| `SETSLP <s>` | seconds | Sleep mode |
| `SETBTS …` | — | Bluetooth configuration |
| `SETHISTORY …` / `SETHIST …` | — | History configuration |
| `SETSENS …` | — | Sensor configuration |
| `SETVRESET` | — | Reset vehicle counters |
| `SETDEFAULT` | — | Restore factory defaults |
| `SETIMI …` / `SETIMIDISABLE …` | — | Custom IMEI control |
| `SETFTK …` | — | Field-test key |

#### 6.4.3 Clear operations (`CLR`)

| Command | Effect |
|---|---|
| `CLRHISTORY` / `CLRHIST` | Delete all stored records |
| `CLRGF` | Delete all geofences |
| `CLRSOS` | Clear latched emergency state |
| `CLRDISK` | Reclaim recoverable filesystem space |

#### 6.4.4 Bare commands

| Command | Effect |
|---|---|
| `RESET` | Restart the unit |
| `INFO` | Device information summary |
| `LOCATION` | Position report |
| `SOS` | Emergency control |
| `HCHK` | Health check |
| `ACTV,…` | Device activation |
| `TESTRIG` / `TESRIG` | Production test mode |

#### 6.4.5 Short-form commands

An alternative compact syntax is accepted:

```
+S*R:<GET|SET|CLR>:<TAG>#<value>
```

| Tag | Parameter | Tag | Parameter |
|---|---|---|---|
| `GIP#` | Primary server endpoint | `RST#` | Reset |
| `EIP#` | Emergency server endpoint | `FOTA#` | Firmware update |
| `PIP#` | Tertiary server endpoint | `OSL#` | Over-speed limit |
| `APN#` | APN | `OVT#` | Over-speed duration |
| `SOS#` | SOS number | `HBT#` | Harsh-brake threshold |
| `VRN#` | Vehicle registration | `HAT#` | Harsh-accel threshold |
| `LOGS#` / `LOG2#` | Log server endpoints | `RTT#` | Rash-turn threshold |
| `HPTI#` / `EPTI#` / `EMTD#` | Interval and emergency timers | `PGF#` | Program geofence |
| `IMON#` / `IMOFF#` | Immobiliser control | `EPSTOP#` | Stop GNSS assistance |

Example:
```
+S*R:SET:GIP#13.234.160.106,8224
```

### 6.5 Fuel sensor reply (host → unit)

A CLS-protocol fuel sensor on the same line replies to the §5.3 poll:

```
@<id:2><cmd:1><len:2 hex><payload><checksum:2>#
```

Example: `@01E130644332180384091068E0#`

| Requirement | Value |
|---|---|
| Minimum length | 20 bytes |
| First byte | `@` |
| **Last byte** | `#` — **MUST** be the final byte |
| Command code | `E` (byte index 3) — fuel level |
| Length consistency | total length = content length + 9 |
| Checksum | 8-bit sum of the content range |

> **Constraint — normative.** The reply **MUST NOT** be followed by `<CR>` or `<LF>`.
> A trailing terminator causes the length validation to fail and the reply to be rejected.
> See §8.3.

### 6.6 Command concurrency and flow control

| Rule | Requirement |
|---|---|
| Outstanding commands | **1.** A host **MUST** wait for `$RES`, or for the 1.5 s timeout in §7.3, before sending the next command |
| Overrun behaviour | A command arriving before the previous one is consumed **overwrites** it. Both the lost command and its response are discarded **silently** — no error is emitted |
| Recommended inter-command delay | ≥ 1.5 s |
| Recommended host timeout | 2.0 s |

Hosts **MUST NOT** pipeline commands. There is no negative acknowledgement for a dropped
command; a missing `$RES` is the only symptom.

---

## 7. Command responses

### 7.1 `$RES` syntax

```
"$RES," <text> <LF>
```

| Property | Value |
|---|---|
| Maximum total frame | 299 bytes including `$RES,` and the terminator |
| `<text>` character set | Printable ASCII **and `<LF>`** — see §7.2 |
| Latency | 0 – 1.2 s after the command is processed |
| Queue depth | 1 |

### 7.2 Embedded newlines — **normative parsing requirement**

`<text>` **MAY** contain embedded `<LF>` characters. For example, `GETOUTSTAT` returns:

```
$RES,Output 1 - 0
Output 2 - 0
```

This is **one** `$RES` frame containing an embedded `<LF>`, not two frames.

Hosts **MUST NOT** use a naive line reader to delimit `$RES` frames. A conforming parser
**MUST** either:

* treat everything from `$RES,` up to the **next line that begins with `$` or `@`** as a
  single response body; or
* apply the §7.3 response timeout and treat all text received in that window as one body.

Status frames (§4) never contain embedded `<LF>` and **MAY** be line-delimited normally.

### 7.3 Response timing

| Event | Timing |
|---|---|
| Command received → processed | Bounded by the unit's server-task poll interval |
| Processed → `$RES` transmitted | 0 – 1.2 s (next scheduler slot) |
| Recommended host timeout | 2.0 s from end of command transmission |

If two responses are generated within one 1.2 s window, only the **later** is transmitted.

### 7.4 Error response

```
$RES,ERROR: Unknown command
```

Emitted when the unit fails to recognise a command **and** the input contains one of the
tokens `SET`, `GET`, `CLR`, `APN`, `SETSOS`, `SETSNS`, `SETSENSOR`. This filter suppresses
spurious replies to line noise.

> **Important:** an unrecognised command that contains none of those tokens produces
> **no response at all**. Absence of `$RES` is therefore ambiguous between "not recognised",
> "dropped by overrun", and "response displaced". Hosts **MUST** treat a response timeout
> as an inconclusive result and retry rather than infer a specific cause.

### 7.5 Response examples

| Command | Response |
|---|---|
| `GETRSTEST` | `$RES,RS232 Test OK - Source: 6` |
| `GETINPSTAT` | `$RES,Input 1 - 1` + `<LF>` + `Input 2 - 0` |
| `GETPANIC` | `$RES,SOS - OFF` |
| `CLRDISK` | `$RES,Disk Cleared. Free: 1.24 MB (+0.31 MB), H:12 B:3 F:5` |
| `CLRHISTORY` | `$RES,History Cleared` |
| `CLRSOS` | `$RES,SOS Data Cleared` |
| `GETNONSENSE` | `$RES,ERROR: Unknown command` |
| `ZZZZ` | *(no response)* |

### 7.6 Configuration-change reporting

A successful `SET` command received over RS232 is acknowledged **twice**:

1. Locally as a `$RES` frame on RS232.
2. Remotely as a parameter-change record transmitted to every connected backend server,
   tagged with source `RS232`.

Hosts **MUST NOT** assume a `SET` over RS232 is invisible to the backend.

---

## 8. Reserved and non-operative features

This section records protocol elements that are **defined but not functional** in firmware
1.5.8/OG1. Each is listed so integrators do not design against them.

### 8.1 AMD3 / AIS-140 structured command frame — not reachable over RS232

The unit implements a structured command frame used by the backend:

```
$<IMEI>,<PWD>,<MODE>,<CMD_ID>[:<VALUE>]{,<CMD_ID>[:<VALUE>]}*<XX><CR><LF>
```

with three authentication layers — IMEI match, password equal to the last 6 digits of the
IMEI, and an XOR checksum (advisory only). Commands cover firmware version, server
endpoints, APN, intervals, behaviour thresholds, registration number, SOS parameters,
restart, IMEI and signal strength.

**This frame cannot be delivered over RS232.** The RS232 input normaliser (§6.2) strips the
leading `$`, and the frame dispatcher requires a `$` as the first character. An AMD3 frame
injected on RS232 falls through to the legacy decoder and is answered with
`$RES,ERROR: Unknown command`.

AMD3 commands **MUST** be delivered over TCP or SMS. The full AMD3 command-ID table is
therefore documented with the server protocol, not here.

### 8.2 Space-separated `SET` / `GET` / `CLR` — silently discarded

The RS232 dispatcher routes any input beginning with `SET `, `GET ` or `CLR ` (verb followed
by a space) to the AMD3 handler. In this build that handler accepts only `$`-prefixed
frames, so such input is **discarded with no action and no response**.

| Input | Result |
|---|---|
| `GET RSTEST` | Silently discarded — **no response** |
| `GETRSTEST` | Executed — `$RES,RS232 Test OK - Source: 6` |
| `SET APN airtelgprs.com` | Silently discarded |
| `SETAPN airtelgprs.com` | Executed |

Hosts **MUST** use the no-space form for every command in §6.4. This is the single most
common integration error against this interface.

### 8.3 CLS sensor replies with trailing CR/LF are rejected

The fuel-sensor validator is given the pre-normalisation byte count while being handed the
post-normalisation string. A reply terminated with `<CR><LF>` is trimmed to *n* bytes but
validated against *n+2*, so both the `#`-terminator test and the length test fail and the
reply is discarded.

Sensors **MUST** send `#` as the final byte with no trailing terminator.

### 8.4 `$RCV` recovery telemetry — compiled out

See §5.2.

### 8.5 `$BATT,CONNECTED` / `DISCONNECTED` and `CHARGING` / `FULL` — not emitted

See §5.1.

### 8.6 Driving-behaviour alerts — not emitted

See §5.5.

### 8.7 NMEA passthrough — not enabled

The unit is capable of mirroring raw GNSS NMEA sentences to RS232. The feature is present
but not activated in this build. Position data **MUST** be taken from `$GPD`.

---

## 9. Host implementation guidance

### 9.1 Recommended receive algorithm

```
loop:
    read bytes into an accumulation buffer
    on <LF>:
        line := buffer, buffer := empty
        if line does not start with '$':
            if line starts with '@':  pass to sensor handler
            else if a $RES body is open: append to that body
            else: discard          # diagnostic text, §5.4
            continue
        tag := line up to first ','
        dispatch on tag:
            "$INF","$CEL","$PER","$GPD"  -> close any open $RES body; parse positionally
            "$BATT"                      -> close any open $RES body; parse event
            "$RES"                       -> open a $RES body (may span lines, §7.2)
            default                      -> discard unknown tag, do not error
```

### 9.2 Robustness requirements

| Requirement | Rationale |
|---|---|
| **MUST** resynchronise on `$` after any parse failure | Truncated frames carry no terminator (§2.3) |
| **MUST** ignore unknown `$` tags without error | Forward compatibility with new frame classes |
| **MUST** tolerate a missing `$CEL` | Suppressed without a SIM (§4.3) |
| **MUST** tolerate empty fields in any position | Degraded-state encoding (§4.1) |
| **MUST NOT** treat field counts as fixed across revisions | `$INF` endpoint 4 is build-dependent (§4.2) |
| **SHOULD** validate IMEI in field 1 against the expected unit | No addressing exists in the protocol |
| **SHOULD** treat a stale `$PER` timestamp as a liveness fault | No heartbeat frame exists |

### 9.3 Deriving state the protocol does not report directly

| Desired state | Derivation |
|---|---|
| Unit alive | Any status frame within 10 s |
| Companion MCU alive | `$PER` field 6 = `1` |
| Position valid | `$PER` field 3 = `1`, then read `$GPD` |
| Backend connected | `$INF` field 3 = `4` |
| Ignition change | Edge on `$PER` field 9 (≤ 4.8 s latency) |
| Emergency active | `$PER` field 8 = `1`, or `GETPANIC` |
| External power lost | `$PER` field 13 drops to `0.0` |
| Battery state | `$PER` field 14 for voltage; `$BATT` for status, with §5.1 limits |

### 9.4 Bandwidth and latency budget

| Metric | Value |
|---|---|
| Status frames per minute | ≈ 50 (4 per 4.8 s) |
| Worst-case event latency (`$PER`-derived) | 4.8 s |
| Command round trip, typical | < 1.5 s |
| Command round trip, worst case | 2.0 s |

Hosts requiring sub-second event latency **MUST NOT** use this interface for that purpose.

---

## 10. Conformance test vectors

Execute in order against a unit running firmware 1.5.8/OG1. Every expected result below is
derived from the implementation, not from intent.

### 10.1 Passive reception

| # | Action | Expected result |
|---:|---|---|
| P1 | Listen for 30 s | ≥ 5 complete status frames observed |
| P2 | Inspect frame sequence | Cycle `$INF` → `$CEL` → `$PER` → `$GPD` repeating |
| P3 | Measure `$PER`-to-`$PER` interval | 4–6 s |
| P4 | Inspect terminators | Bare `<LF>`, no `<CR>` |
| P5 | Count `$PER` fields | Exactly 15 including the tag |
| P6 | Remove the SIM, listen 30 s | `$CEL` absent; `$INF` field 3 = `0`; fields 4–11 empty; field count unchanged |

### 10.2 Command interface

| # | Send | Expected result |
|---:|---|---|
| C1 | `GETRSTEST` | `$RES,RS232 Test OK - Source: 6` within 2.0 s |
| C2 | `$GETRSTEST` | Identical to C1 — leading `$` is stripped |
| C3 | `  GETRSTEST\r\n` | Identical to C1 — whitespace trimmed |
| C4 | `GET RSTEST` | **No response.** Confirms §8.2 |
| C5 | `GETNONSENSE` | `$RES,ERROR: Unknown command` |
| C6 | `ZZZZ` | No response. Confirms the noise filter §7.4 |
| C7 | `GETOUTSTAT` | `$RES,Output 1 - <n>` followed by `Output 2 - <n>` on the next line — **one** frame (§7.2) |
| C8 | `GETRSTEST` ×3 with no delay | Fewer than 3 responses. Confirms the single-slot overrun §6.6 |
| C9 | `GETRSTEST` ×3 at 2 s spacing | Exactly 3 responses |

### 10.3 Event frames

| # | Action | Expected result |
|---:|---|---|
| E1 | Remove external power | `$BATT,NOT_CHARGING,<v>V` within a few seconds; `$PER` field 13 → `0.0` |
| E2 | Restore external power | `$BATT,UNKNOWN,<v>V`. **`CHARGING` MUST NOT appear** (§5.1) |
| E3 | Listen 10 min in steady state | No `$RCV` frame (§5.2) |
| E4 | Trigger a harsh-braking event | No RS232 frame (§5.5) |

### 10.4 Negative tests

| # | Send | Expected result |
|---:|---|---|
| N1 | 600 bytes of text | Truncated at the transport; no crash, status rotation continues |
| N2 | Binary bytes 0x00–0x1F | Discarded; status rotation continues |
| N3 | `@01E1306443321803840910` (no `#`) | Discarded; no response |
| N4 | `$868329083378285,378285,GET,001*7A` | `$RES,ERROR: Unknown command` — confirms §8.1 |

---

## 11. Quick reference

### 11.1 Frame index

| Frame | Direction | Cadence | Section |
|---|---|---|---|
| `$INF` | Unit → Host | ≈ 4.8 s | §4.2 |
| `$CEL` | Unit → Host | ≈ 4.8 s, suppressed without SIM | §4.3 |
| `$PER` | Unit → Host | ≈ 4.8 s | §4.4 |
| `$GPD` | Unit → Host | ≈ 4.8 s | §4.5 |
| `$BATT` | Unit → Host | On change | §5.1 |
| `$RES` | Unit → Host | Per command, ≤ 1.2 s | §7 |
| `$RCV` | Unit → Host | **Never in this build** | §5.2 |
| `@01E0006#` | Unit → Host | Before each sensor report | §5.3 |
| `@…#` | Host → Unit | Sensor reply | §6.5 |
| `GETxxx` / `SETxxx` / `CLRxxx` | Host → Unit | On demand | §6 |
| `+S*R:…` | Host → Unit | On demand | §6.4.5 |

### 11.2 Integration checklist

- [ ] Parse `$` frames positionally; never assume a field is non-empty
- [ ] Use the **no-space** command form (`GETRSTEST`, not `GET RSTEST`) — §8.2
- [ ] Handle embedded `<LF>` inside `$RES` — §7.2
- [ ] Serialise commands, one at a time, ≥ 1.5 s apart — §6.6
- [ ] Treat a response timeout as inconclusive, and retry — §7.4
- [ ] Take fix validity from `$PER` field 3, not from `$GPD` coordinates — §4.5
- [ ] Do not use `$PER` field 7 (flash status) for anything — §4.4
- [ ] Read `$PER` field 6 as MCU link health, not MEMS status — §4.4
- [ ] Do not expect `CHARGING` / `FULL` / `CONNECTED` / `DISCONNECTED` — §5.1
- [ ] Do not expect behaviour or SOS alerts on RS232 — §5.5
- [ ] Ignore lines that begin with neither `$` nor `@` — §5.4

---

## 12. Related documents

| Document | Content |
|---|---|
| [RS232_INTERFACE_SPEC.md](RS232_INTERFACE_SPEC.md) | Internal MCOMM transport, firmware implementation detail, defect register |
| [AMD3_PROTOCOL_IMPLEMENTATION.md](AMD3_PROTOCOL_IMPLEMENTATION.md) | Backend AMD3 / AIS-140 server protocol |
