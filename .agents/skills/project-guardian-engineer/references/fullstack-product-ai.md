# Full-Stack, Product, UI/UX, Data, and AI Work

## Full-stack tracing

Trace behavior end-to-end:

`UI -> request/event -> controller/API -> service/domain -> cache/database/external system -> response -> UI state`

Do not patch a UI symptom if backend state or contract semantics are wrong.

## Safe implementation

- preserve API compatibility unless migration is part of the request
- validate input at trust boundaries
- enforce authorization server-side
- avoid hard-coded demo values in production paths
- keep loading/empty/stale/offline/error states explicit
- avoid full-table/client-side processing for large datasets
- preserve existing design tokens/components before inventing new primitives

## UI/UX workflow

Before redesigning:

1. identify target user role and job-to-be-done
2. inspect the existing workflow and constraints
3. define the primary decision/action the screen must support
4. consider loading, empty, error, offline, permission and responsive states
5. research current patterns only when useful and distinguish evidence from proposals
6. implement with accessibility and data-density checks

## Data/AI workflow

Before adding ML/AI:

- define measurable operational outcome
- define data contract and freshness
- establish a deterministic/simple baseline
- avoid target leakage and evaluate with realistic time/entity splits
- define thresholds and false-positive/false-negative costs
- provide explainability appropriate to the decision
- define production monitoring, drift and rollback

Use deterministic code for calculations and policy enforcement. Use LLMs for interpretation, orchestration, summarization, or natural-language interaction where appropriate.

## AI chatbot safety

For an in-product assistant:

- enforce tenant/user authorization at every tool boundary
- prefer approved APIs/tools over unrestricted database access
- start read-only
- require explicit confirmation for writes/operational actions
- include freshness/time range/source context in answers
- defend against prompt injection and cross-tenant data leakage
- evaluate common questions and adversarial authorization cases offline
