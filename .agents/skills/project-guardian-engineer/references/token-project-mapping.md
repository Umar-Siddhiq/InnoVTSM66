# Token-Efficient Project Mapping

## Principle

Do not keep the entire repository or entire history in the prompt. Keep a compact navigational map and retrieve details only when needed.

## Project map layers

- `.ai/PROJECT_MAP.md`: human/LLM-readable compact map, intended for startup
- `.ai/PROJECT_MAP.json`: detailed machine-readable inventory, not normally loaded wholesale
- `.ai/MEMORY_INDEX.md`: short recall index
- `.ai/memory/events.jsonl`: detailed history searched on demand

## Efficient task startup

1. Read the compact map and critical invariants.
2. Search for task-specific symbols/keywords.
3. Open only the relevant implementation and nearest tests.
4. Search memory with the same keywords.
5. Use Git diff/history only for the affected area.

## Refresh conditions

Refresh the project map when:

- it is missing
- major folders/manifests/entry points changed
- a structural refactor occurred
- dependencies/build system changed
- the current map is older than significant repository changes

Do not regenerate it after every one-line change unless the structure changed.

## What the map should capture

- repository root/branch/HEAD/status summary
- dominant languages/file counts
- manifests/build/test/CI/config files
- top source directories
- candidate entry points
- largest/high-churn source files as attention signals
- current dirty paths

Treat these as navigational signals, not architecture claims. Confirm architecture from code before stating it as fact.

## Token budget rule

Prefer:

- filenames and one-line summaries over copied source
- exact targeted grep/search over recursive reading
- diff hunks over whole changed files when enough
- latest relevant memory records over complete history
- references loaded only for the active domain
