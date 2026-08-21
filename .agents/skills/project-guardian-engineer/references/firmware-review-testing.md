# Review and Testing

## Review severity

- Critical: bricking, unsafe operation, unrecoverable corruption/security failure.
- High: crash/reset/data corruption/major communication failure.
- Medium: intermittent fault, weak recovery, timing/resource risk, maintainability defect likely to cause future failures.
- Low: clarity/style/minor optimization with low operational risk.

Review for buffer bounds, lifetimes, concurrency, ISR safety, watchdog impact, timeout/retry logic, return-value handling, integer conversion, protocol framing, and persistent-state changes.

## Tests

Cover normal, boundary, recovery, and failure scenarios. Prefer tests that reproduce the reported bug.

Typical fault injection:
- low/unknown/flapping RF signal
- registration loss / roaming / operator denial
- SIM absent or modem reboot
- DNS/socket/server failure
- partial/corrupt packets
- GNSS no-fix/stale/invalid frames
- power loss/brownout/backup battery transition
- watchdog/reset/reboot loops
- UART overflow / peripheral timeout
- flash/storage full/corrupt where relevant

## Static analysis

Use project-supported compiler warnings, clang-tidy, cppcheck, Semgrep, or vendor analyzers when available. Do not suppress warnings merely to obtain a clean build.
