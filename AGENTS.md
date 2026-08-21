# AGENTS Project Instructions

<!-- PROJECT-GUARDIAN:START -->
## Project Guardian (always-on)

1. Read `.ai/CRITICAL_INVARIANTS.md`, `.ai/MEMORY_INDEX.md`, `.ai/PROTECTED_AREAS.json`, and `.ai/PROJECT_MAP.md` before engineering changes.
2. Search project memory for task/component terms before touching related code.
3. Treat `why`, `check`, `analyze`, `review`, `inspect`, and `find issue` as read-only; do not edit unless the user explicitly requests a change.
4. Never modify a protected area incidentally. Run the Project Guardian guardrail before protected/high-risk edits.
5. Diagnose from evidence before implementation; make the smallest complete change and preserve unrelated work.
6. After a validated fix, correction, discovered invariant, or high-value failed attempt, write project memory automatically. Do not wait for the user to ask.
7. A meaningful implementation is not complete until validation, final diff review, protected-area checks, and memory write-back pass.
8. Use the `project-guardian-engineer` skill for the detailed workflow when the host supports Agent Skills.
<!-- PROJECT-GUARDIAN:END -->
