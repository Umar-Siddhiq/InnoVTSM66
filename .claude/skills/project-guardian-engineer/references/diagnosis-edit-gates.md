# Diagnose-Before-Edit Gates

## Intent gate

Treat these as read-only by default:

- why
- check
- inspect
- analyze
- review
- investigate
- find the issue
- compare
- explain

Treat these as modification requests when the object is clear:

- fix
- implement
- change
- update
- apply
- replace
- refactor
- build/create the feature

If a prompt includes both diagnosis and a clear fix request, diagnose first, then implement.

## Evidence chain

For a defect, trace from symptom to first divergence:

`input/event -> parser/handler -> state -> business logic -> persistence/transport -> presentation/output`

For each hop capture:

- expected value/state
- actual value/state
- timestamp/order if timing-sensitive
- evidence source: code, log, packet, test, measurement, screenshot, specification

The first proven divergence is normally the best fix location.

## Stop conditions before editing

Do not edit yet when:

- the failure cannot be reproduced or traced at all
- the candidate fix contradicts a known invariant
- the affected code is protected and the user did not explicitly request changing it
- the diagnosis is based only on a comment/README that conflicts with current code
- hardware/network causes remain equally likely and no code evidence differentiates them
- the current working tree contains overlapping user changes that could be overwritten

## Minimum change rule

Before modifying code, state internally:

- exact acceptance criterion
- intended files/functions
- protected files/functions that must not change
- targeted validation
- likely regression surface

After the edit, compare the actual diff against that scope.

## Common agent failure pattern

Bad pattern:

1. See missing output.
2. Assume subsystem is off.
3. Change initialization/power/state code.
4. Accidentally disable or reset a working subsystem.

Correct pattern:

1. Prove whether the subsystem is producing data.
2. Trace where the specific field disappears.
3. Fix the first incorrect transformation/state propagation.
4. Leave working initialization/power logic untouched.
