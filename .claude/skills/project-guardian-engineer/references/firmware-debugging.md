# Firmware Debugging Workflow

## Start with observables

Write down observed vs expected behavior. Correlate timestamps, state transitions, reset reason, modem/GNSS events, task activity, and server/network responses.

## Trace before patching

Build the execution path from trigger to failure:

input/event -> parser/ISR -> state update -> task/state machine -> peripheral/network action -> response handling -> next state

Find the first divergence from expected behavior.

## Common fault classes

For resets/hangs/crashes inspect:
- HardFault/exception registers and reset cause
- watchdog starvation
- stack overflow / high-water marks
- heap corruption or fragmentation
- invalid pointer/lifetime
- buffer overrun
- race/deadlock/priority inversion
- unbounded loops or blocking calls
- brownout/power/reset-line issues

For intermittent long-run failures inspect counters, wraparound, leaked resources, stale state, retry exhaustion, queue growth, socket lifecycle, and periodic timers.

## Logs

Preserve chronology. Separate the initiating event from cascading symptoms. Do not infer causality from proximity alone; verify the code path.

## Fix validation

After a fix, reproduce the original condition when possible and add a regression test or targeted diagnostic. Do not merely confirm that compilation succeeds.
