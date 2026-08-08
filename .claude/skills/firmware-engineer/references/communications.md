# Modem, GNSS, and Network Communications

## GSM/GPRS/LTE modem debugging

Trace the complete modem state machine rather than individual AT commands.

Verify:
- modem power/reset/boot readiness
- SIM presence/PIN/ICCID/IMSI when relevant
- operator selection and roaming
- `CREG`/`CGREG`/`CEREG` registration state
- signal metrics (`CSQ` and technology-specific metrics)
- APN/PDP activation
- DNS resolution
- socket open/connect/send/receive/close
- command timeout, retry, parser, and recovery state
- UART framing/buffer behavior

Do not treat `CSQ` alone as proof of registration. Handle `99`/unknown explicitly and distinguish temporary RF loss from sustained registration failure.

For SIM/operator/profile switching, inspect threshold/debounce timers, counter reset conditions, recovery confirmation, profile persistence, and interaction with registration/socket state.

## GNSS

Check NMEA/binary parser framing, fix-valid flags, date/time, latitude/longitude hemisphere and scaling, satellite count, HDOP, stale-data handling, and UART/DMA loss. Do not report stale coordinates as a fresh fix.

## TCP/UDP/MQTT/HTTP/custom protocols

Validate framing, length, partial/fragmented reads, delimiters, CRC/checksum, byte order, retries, ACK handling, duplicate handling, reconnect behavior, keepalive, timeout, and resource cleanup.

When debugging a server issue, separate modem registration, IP connectivity, DNS, socket state, application protocol, and server response layers.
