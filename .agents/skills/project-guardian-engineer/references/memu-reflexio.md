# MemU and Reflexio Integration

## Layered policy

Use three layers:

1. repository-local guardian memory - authoritative and version-controllable
2. MemU - semantic cross-session/cross-agent recall
3. Reflexio - corrections/success-path learning and playbooks

Never make a critical project invariant exist only in an external memory service.

## MemU

Current memU host adapters bind two seams: recording session logs for later distillation and injecting a standing retrieval instruction into the host instruction file. Dedicated adapters include `memu-codex` and `memu-claude-code`; `memu-agent` is the generic fallback for other supported hosts.

At the start of a task, when the adapter is installed, retrieve narrowly with the host adapter, for example:

```text
memu-codex retrieve "<project> <component> <symptom>"
memu-claude-code retrieve "<project> <component> <symptom>"
memu-agent retrieve "<project> <component> <symptom>"
```

Use the binary matching the current host. Do not call every adapter.

Run `<binary> doctor` when memory behavior is uncertain. For an unknown host, use `memu-agent detect` before assuming capture/retrieval is wired correctly.

MemU's scheduled bridge can distill session history into reusable Markdown memory/skills. Keep Project Guardian's `.ai` records anyway so critical engineering facts remain visible in the repository and reviewable in Git.

## Reflexio

Reflexio is designed to learn from real interactions, especially user corrections and successful execution paths. The CLI supports publishing corrected multi-turn interactions and searching extracted profiles/playbooks.

High-value publish events:

- user corrects a wrong technical assumption
- user says a file/subsystem must not be touched again
- a successful workflow avoids a previously repeated failure
- an expert/known-good answer is available for comparison

Search Reflexio for task-specific rules before repeating an uncertain workflow. When publishing a correction, include enough context to distinguish the bad action, the correction, and the final proven behavior.

Local Reflexio ports have changed between releases. Do not hardcode a port in project rules. Prefer the installed CLI/config (`REFLEXIO_URL`) or run the service's status/help command and use the active endpoint.

`claude-smart` is a useful host integration when automatic correction capture is desired for supported coding-agent workflows. Treat its learned project rules as reinforcement, not as a substitute for `.ai/CRITICAL_INVARIANTS.md` and regression memory.

## Automatic-memory rule

External memory should be automatic when the installed host adapter supports it, but completion must not depend on an asynchronous extractor finishing. Project Guardian writes the local structured record synchronously first; MemU/Reflexio may distill or retrieve it through their own integrations afterward.

This order prevents a race where a chat ends before an external learner saves the correction.

## Failure policy

If MemU or Reflexio is unavailable:

- do not block ordinary engineering work
- continue using `.ai` local memory
- do not invent retrieved memories
- mention the outage only when it materially affects recall or the user asked about memory health

## Privacy and secrets

Do not store credentials, tokens, private keys, certificates, SIM secrets, production secrets, or raw sensitive data in any memory layer. Store references to secure locations when needed, not the secret value.
