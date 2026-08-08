# Project Records

## PROJECT_OVERVIEW.md
Maintain stable system-level context only: purpose, major modules, architecture, build system, hardware/platform, external interfaces, and important conventions. Do not turn this into a chronological log.

## CURRENT_STATE.md
Keep this as the handover snapshot. Include:
- current working state
- active branch/version when known
- recently completed work
- open problems
- immediate next actions
- known test status
- important environment or hardware dependencies

## CHANGELOG_AI.md
Record meaningful AI-assisted engineering changes chronologically. Each entry should include date, task, files changed, reason, validation, and issue link if one exists.

## KNOWN_ISSUES.md
Maintain a compact index of unresolved issues. Include issue ID, short description, severity, status, owner if known, and link to the detailed issue record.

## TEST_HISTORY.md
Record important validation runs, especially regression, bench, integration, protocol, hardware, and field tests. Include date, build/version, setup, cases, result, and evidence location.

## DECISIONS.md
Record architectural or behavioral decisions that future engineers must understand. Include context, options considered, decision, reason, tradeoffs, and reversal conditions.

## Issue IDs
If the project already has an issue numbering system, use it. Otherwise use sequential `ISSUE-001`, `ISSUE-002`, etc. Do not reuse IDs.
