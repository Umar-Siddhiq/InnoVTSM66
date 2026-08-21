# Protected Areas and Invariants

## Protection semantics

A protected area means: **do not modify it incidentally**.

Protection can come from:

- the user explicitly saying not to touch a file/function/subsystem
- a validated previous fix that must not be reverted
- high-risk code where unrelated changes have a large blast radius
- `.ai/PROTECTED_AREAS.json`
- `.ai/CRITICAL_INVARIANTS.md`

A later explicit user request to change the same protected area can override protection for that task only. Preserve all unrelated invariants.

## Protection levels

- `hard`: explicit user "do not touch" or safety-critical area; block incidental edits.
- `high-risk`: edit only after direct evidence that the root cause is inside the area.
- `watch`: changes are allowed, but require targeted regression validation.

The bundled memory/guardrail scripts currently enforce path patterns. Store symbol/subsystem protections as explanatory notes and map them to concrete paths whenever possible.

## Default high-risk domains

- GNSS/GPS enable, power, reset, startup and state machine
- GSM/LTE modem registration/profile switching and persistent NVM commands
- bootloader/FOTA/flash layout
- protocol packet framing/checksum/version logic
- authentication/authorization and secrets handling
- production database migrations
- cryptographic validation
- device provisioning/calibration/factory settings

## Before a protected edit

1. Search regression memory for the area.
2. Verify the user asked for a change that actually requires it.
3. Capture the current Git state.
4. Identify the invariant to preserve.
5. Add a targeted test or bench check.
6. Run `guardrail.py` before and after.

## After a protected edit

Record:

- why the edit was necessary
- what invariant remained true
- exact validation performed
- rollback/recovery path if applicable
- updated protection rule if the old rule was too broad or too narrow
