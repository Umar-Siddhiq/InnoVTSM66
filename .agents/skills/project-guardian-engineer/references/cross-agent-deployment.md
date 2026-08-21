# Cross-Agent Deployment

## Portable core

Use the open Agent Skills pattern: a folder containing `SKILL.md` plus optional scripts/references/assets. Keep the main body concise and load detailed references on demand.

## Project-local locations

Use real directories/copies rather than depending on symlinks for maximum host compatibility:

- `.agents/skills/project-guardian-engineer/` - shared location for Codex, Google Antigravity, GitHub Copilot and OpenCode-compatible discovery
- `.claude/skills/project-guardian-engineer/` - Claude Code location; also recognized by OpenCode

The bundled installer can create both copies.

## Always-on files

A skill is task-triggered. For rules that must apply at the start of every coding chat, also maintain short repository instruction files:

- `AGENTS.md` - cross-agent rules and startup contract
- `CLAUDE.md` - Claude Code startup contract

The bootstrap script adds/replaces only a clearly marked Project Guardian block and preserves other content.

## Host notes

### Claude Code

Skills and `CLAUDE.md` are useful, but do not rely on auto-memory as the sole enforcement mechanism. Optional lifecycle hooks can add startup/completion checks. Treat hooks as reinforcement because runtime/model behavior can vary by version.

### Codex

Use `.agents/skills` and `AGENTS.md`. Codex supports project instruction discovery and skills. Hooks are available in current versions but may require trust/review and have had version-specific regressions; do not make safety depend exclusively on them.

### Google Antigravity

Workspace skills live under `.agents/skills`. Use the same portable skill there.

### OpenCode

OpenCode discovers `.agents/skills` and `.claude/skills` on demand. This makes it a good host for local Ollama models.

### Ollama

Ollama provides model serving, system prompts, context windows, tool-calling capable APIs and structured outputs, but a bare model does not browse the filesystem or discover skills by itself. Run Ollama behind an agent host such as OpenCode and let the host manage tools, project instructions and skills.

For coding agents, configure a context window appropriate to the model/hardware. Larger context increases memory use; do not substitute giant context for project mapping and targeted retrieval.

## Installation validation

After installing:

1. verify the skill appears in the host's discovered skills
2. start a fresh project chat
3. ask a diagnosis-only question and confirm no edit is attempted
4. add a temporary protected path and confirm the guardrail blocks incidental modification
5. make a safe test change and confirm a memory record is required/written
6. search memory from a new session and confirm the record is retrievable
