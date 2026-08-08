# Git and Project Memory

## Git as source of truth

Use Git for exact change history. Before edits inspect status and relevant diffs. After edits review the final diff for accidental formatting, unrelated changes, debug code, changed constants, removed checks, and duplicated logic.

Do not commit/push unless requested. Never destroy uncommitted work to make the tree clean.

## Project memory

If the project already contains `PROJECT_MEMORY.md`, `CHANGELOG_AI.md`, `CLAUDE.md`, `AGENTS.md`, or equivalent project guidance, read and respect it.

After a meaningful validated change, update existing memory/change-log files only when the project convention calls for it or the user requests it. Record durable facts such as architecture decisions, hardware constraints, known modem/protocol behavior, fixes that should not be reverted, and pending technical work.

Never store secrets, credentials, private keys, production tokens, SIM secrets, or certificates in project memory.
