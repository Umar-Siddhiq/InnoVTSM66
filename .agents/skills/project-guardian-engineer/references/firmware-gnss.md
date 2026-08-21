# Firmware, GNSS, Modem, and Hardware-Aware Debugging

## Firmware evidence order

Prefer evidence in this order:

1. current source and build configuration
2. device logs with timestamps/state transitions
3. raw UART/AT/NMEA/protocol data
4. reproducible bench behavior
5. electrical/RF measurements
6. server/network traces
7. historical notes

Do not let a historical note override current code or fresh measurements.

## GPS/GNSS satellite-count trace

For "satellite count not showing", trace the value without touching GNSS enable/state code first:

1. Is GNSS powered/enabled and producing raw data?
2. Does the raw response/NMEA sentence contain satellites-in-use or satellites-in-view?
3. Does the parser recognize the correct sentence/AT response and field index?
4. Is the parsed value range-checked and stored?
5. Can another task overwrite/zero/stale the value?
6. Is the value copied into the telemetry/protocol/API structure?
7. Is the receiver/UI/log reading the correct field/version?
8. Are update intervals/timeouts causing a stale display?

Typical satellite fields differ by protocol/sentence. Verify the actual modem/GNSS output; do not assume a field mapping from memory.

## Do not "fix" count display by

- disabling/re-enabling GPS on every read
- resetting the modem/GNSS engine without evidence
- changing GNSS power sequencing
- changing constellation configuration
- removing validity/fix checks merely to show a number
- forcing a constant/non-zero satellite count

## Modem/network diagnosis

Separate:

- RF/signal quality
- SIM registration and operator/profile state
- PDP/data-session state
- socket/protocol state
- application retry/state machine
- server acknowledgement

A low/zero CSQ problem is not automatically a parser bug. A server timeout is not automatically a modem-registration problem.

## Embedded change discipline

For C/C++ firmware:

- inspect buffer bounds, string termination, integer width/sign, concurrency, ISR/task ownership, timeout units and wraparound
- avoid hidden dynamic allocation in long-running paths unless the architecture already uses it deliberately
- preserve watchdog servicing and timing behavior
- verify UART/DMA ring-buffer ownership before changing parsers
- treat modem/GNSS/network input as untrusted

## Hardware-aware conclusion labels

Use:

- `CONFIRMED FIRMWARE`
- `CONFIRMED HARDWARE/RF`
- `LIKELY FIRMWARE`
- `LIKELY HARDWARE/RF/NETWORK`
- `NEEDS BENCH VERIFICATION`

Do not claim a firmware root cause when the evidence only shows a symptom at the firmware boundary.
