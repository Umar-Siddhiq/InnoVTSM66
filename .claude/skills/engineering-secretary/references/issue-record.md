# Issue Record Template

Use this structure for deep issue documentation.

# ISSUE-### - [Short descriptive title]

## Metadata
| Field | Value |
|---|---|
| Status | CONFIRMED / PARTIALLY VERIFIED / NEEDS VERIFICATION / FAILED / NOT TESTED |
| Severity | Critical / High / Medium / Low / Informational |
| Component | |
| Reported date | |
| Resolved date | |
| Firmware/software version | |
| Hardware/device | |
| Branch/commit | |

## Executive summary
Explain the problem, root cause, fix, and final status in plain language.

## Problem reported
Record the original issue as reported. Preserve important exact values, error codes, timings, packet samples, or symptoms.

## Expected behavior
Describe the intended behavior and, when relevant, cite the specification, requirement, datasheet, or established behavior.

## Actual behavior
Describe what was observed. Separate direct observation from interpretation.

## Reproduction conditions
Document environment, hardware, firmware, network/SIM/server state, inputs, timing, and steps required to reproduce.

## Investigation timeline
| Step | Action / Observation | Evidence | Conclusion |
|---|---|---|---|

## Code and system path investigated
List files, functions, tasks, interrupts, state machines, services, protocols, hardware signals, or dependencies traced.

## Root cause
State `CONFIRMED ROOT CAUSE` only when evidence supports it. Otherwise use `CURRENT HYPOTHESIS` and explain what remains unverified.

## Failed or rejected approaches
Document important attempts that did not solve the issue and why.

## Changes implemented
| File / Component | Function / Area | Change | Reason |
|---|---|---|---|

## Technical explanation
Explain how the previous behavior produced the failure and how the new behavior changes that execution path.

## Build and static-analysis result
Record build command/toolchain when known, warnings/errors, and final outcome.

## Test plan and results
| # | Test case | Setup / Input | Expected | Actual | Result | Evidence |
|---|---|---|---|---|---|---|

## Final working state
Describe what now works, under which verified conditions, and which version/build was tested.

## Remaining risks and pending verification
List untested conditions, field tests, hardware validation, performance checks, or edge cases.

## Reproduction / verification guide
Give concise steps another engineer can follow to verify the final state.

## Related records
Link commits, logs, specifications, test reports, tickets, packet captures, or other issue documents.

## Revision history
| Date | Change | Author/Agent |
|---|---|---|
