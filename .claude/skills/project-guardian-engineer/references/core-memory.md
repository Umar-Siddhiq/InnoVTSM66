# Durable Project Memory

## Purpose

Use repository-local memory as the durable, reviewable source for engineering lessons. External memory services are secondary recall layers.

## Startup read order

Read only the high-signal files first:

1. `.ai/CRITICAL_INVARIANTS.md`
2. `.ai/MEMORY_INDEX.md`
3. `.ai/PROTECTED_AREAS.json`
4. `.ai/CURRENT_STATE.md`
5. Search `.ai/memory/events.jsonl` for the current task's component/symptom

Do not load the entire event history unless the task requires it.

## Mandatory write triggers

Write memory without waiting for a reminder after:

- a validated bug fix
- a user correction of agent behavior or technical assumptions
- a regression and its recovery
- a discovered "do not change" invariant
- a new hardware/network/environment fact that source code cannot reveal
- a significant architecture decision
- a failed attempt that is dangerous or likely to be repeated
- a new build/test command proven to be the correct project path

## Record quality

Each useful record should answer:

- What happened?
- What was the root cause?
- What evidence proved it?
- What changed?
- What must remain unchanged?
- How was the fix validated?
- What prevents recurrence?

Use concise summaries. Store exact details in the append-only JSONL record instead of bloating always-on instructions.

## Boundary between instructions and memory

- Put **how the agent must behave** in `AGENTS.md` / `CLAUDE.md`.
- Put **critical project invariants** in `.ai/CRITICAL_INVARIANTS.md`.
- Put **facts learned from incidents/fixes** in `.ai/memory/events.jsonl`.
- Put **current architecture/status** in `.ai/CURRENT_STATE.md`.

Do not duplicate the same long rule in every surface.

## Correction rule

When the user says an earlier change was wrong:

1. Record the failed approach and why it was wrong.
2. Record the corrected behavior.
3. If the corrected area should no longer be touched incidentally, protect its paths/symbols.
4. Add a regression check or manual validation requirement.

This is more important than recording ordinary successful edits because repeated corrections are the highest-signal evidence of a missing guardrail.
