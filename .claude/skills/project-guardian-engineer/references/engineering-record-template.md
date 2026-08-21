# Engineering Change / Issue Record Template

Use this structure for non-trivial the current product implementations or incident fixes. Adapt headings when a section is truly not applicable; do not fill with vague placeholders.

```markdown
# <Short task / issue title>

Date: YYYY-MM-DD
Area: <frontend | backend | tracking | video | database | AI | analytics | mobile | infra>
Status: <implemented | analyzed | partially verified | blocked>

## 1. Request / user-visible problem
<What was requested or observed.>

## 2. Reproduction and evidence
- Environment / branch / commit:
- Steps or failing request:
- Logs / screenshots / query evidence:
- Expected:
- Actual:

## 3. Root cause
<First incorrect state/assumption and why it occurred.>

## 4. Solution
<What changed and why this approach was selected.>

### Alternatives considered
- <option>: <why rejected/deferred>

## 5. Files changed
| File | Change | Reason |
|---|---|---|
| ... | ... | ... |

## 6. Contract / schema / configuration impact
<API, database, cache key, env var, migration, protocol, or none.>

## 7. UI/UX impact
<Flow/states/responsive/accessibility details, or none.>

## 8. Security / privacy impact
<Auth, tenant isolation, sensitive data, AI/tool access, or none.>

## 9. Performance / reliability impact
<Query volume, cache, polling, stream lifecycle, batching, retries, or none.>

## 10. Tests performed
| Test/command | Result | Evidence/notes |
|---|---|---|
| ... | PASS/FAIL/NOT RUN | ... |

## 11. Remaining risks / known issues
- ...

## 12. Rollback / recovery
<Required for meaningful schema/protocol/infra behavior changes; otherwise concise.>

## 13. Final working behavior
<Describe what a user/system can now do, without overstating untested paths.>

## 14. Next recommended action
<One or more concrete follow-ups only when useful.>
```

## Final response pattern

For completed implementation, use a compact version:

1. **What changed**
2. **Root cause / design reason**
3. **Key files**
4. **Validation performed**
5. **Risks / not tested**
6. **Suggested next step**

Never use “fully fixed” or “production ready” when relevant validation was not run.
