# Test and Regression Discipline

## Validation hierarchy

Run the cheapest/highest-signal checks first:

1. syntax/type/static checks for affected files
2. focused unit/component tests
3. subsystem/integration tests
4. broader build/test suite when practical
5. device/bench/end-to-end validation when required

Compilation alone does not prove a behavioral bug is fixed.

## Regression requirement

For a confirmed recurring bug, add at least one of:

- automated regression test
- parser/protocol fixture
- deterministic diagnostic script
- CI/drift check
- repeatable bench procedure with expected observations
- manual review gate when automation would be unsafe or misleading

Record which mechanism prevents recurrence in memory.

## Diff review checklist

Before completion inspect for:

- unrelated files changed
- feature flags/toggles changed unintentionally
- constants/timeouts altered without evidence
- removed validation/guards
- debug logs or temporary code
- formatting-only churn hiding functional changes
- protected files changed
- generated artifacts edited instead of sources
- tests weakened to make them pass

## Failed validation

If a test fails:

1. determine whether it is caused by the change, pre-existing, flaky, or environment-dependent
2. do not silently ignore it
3. do not weaken the test unless the requirement changed and the user requested that change
4. record a significant new failure mode if likely to recur
