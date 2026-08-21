# Ecosystem Source Notes - August 2026

These are design inputs, not runtime dependencies. Re-check upstream docs before changing host-specific installation/configuration.

## Agent Skills and codebase harness patterns

- Anthropic Agent Skills repository: https://github.com/anthropics/skills
- GitHub Agent Skills documentation: https://docs.github.com/en/copilot/concepts/agents/about-agent-skills
- GitHub awesome-copilot skills, especially `harness-engineering` and `acquire-codebase-knowledge`: https://github.com/github/awesome-copilot
- Google Antigravity skills docs: https://antigravity.google/docs/skills
- OpenCode skills docs: https://opencode.ai/docs/skills/

Key pattern adopted here: progressive loading, concise skill descriptions, high-signal gotchas, deterministic scripts, always-on repository instructions, failure memory, and codebase maps instead of giant prompts.

## Codex

- OpenAI Codex repository: https://github.com/openai/codex
- Codex uses `AGENTS.md` for durable repository instructions and `.agents/skills` for repo skills in current implementations.
- Current Codex code also includes lifecycle hook support, but trust/version-specific behavior should be verified before relying on it.

## MemU

- https://github.com/NevaMind-AI/memU
- Current releases emphasize cross-agent memory, host adapters, session bridging and automatic reusable-skill extraction.

## Reflexio

- https://github.com/ReflexioAI/reflexio
- Reflexio learns from corrections and successful paths, exposes search/retrieval and supports local services.
- `claude-smart` demonstrates correction-to-project-skill behavior across Claude Code, Codex and OpenCode: https://github.com/ReflexioAI/claude-smart

## Ollama

- https://github.com/ollama/ollama
- Ollama supports system prompts, configurable context, tool-calling capable chat APIs and structured output. Project file access/skill discovery must come from the host application.
