# Engineering Workflow

## Contents

1. Problem tracing
2. Architecture decisions
3. API and service design
4. Persistence and cache
5. Performance
6. Frontend integration
7. Streaming/live systems
8. Refactoring
9. Release discipline

## 1. Problem tracing

Trace from user-visible symptom backward through the full path. Build a small evidence chain:

`UI/event -> request/message -> API/controller -> service/domain -> database/cache/device/external service -> response/state update`

For bugs, record:

- expected state
- actual state
- first divergence
- conditions required to reproduce
- whether the bug is deterministic, timing-dependent, load-dependent, data-dependent, or environment-dependent

Do not patch the UI symptom if the first divergence is in backend state or data semantics.

## 2. Architecture decisions

Prefer existing boundaries. Introduce a new service/module only if it has a clear responsibility, repeated use, testability benefit, or isolation need.

For meaningful architectural changes, document:

- problem and constraints
- chosen option
- alternatives rejected
- migration/compatibility plan
- observability plan
- rollback path

Avoid simultaneous framework replacement and feature work unless the replacement is the task.

## 3. API and service design

- Keep request/response contracts explicit.
- Version or migrate breaking contracts deliberately.
- Validate IDs, coordinates, timestamps, pagination, filters, enums, file/media parameters, and tool arguments.
- Authorize by tenant/resource ownership in the backend even if the UI hides unauthorized options.
- Return stable machine-readable error codes plus safe human-readable messages.
- Use cancellation/timeouts for remote calls.
- Make retries bounded and only retry operations that are safe/idempotent or protected by idempotency keys.
- Do not place domain-critical calculations solely in JavaScript/UI code.

## 4. Persistence and cache

For PostgreSQL or other relational stores:

- filter, sort, aggregate, and paginate in SQL rather than loading large tables to memory
- inspect execution plans for slow/high-volume queries
- index real predicates/joins/orderings, not every column
- use projection rather than loading full entities when only a subset is needed
- bound report/export jobs and consider asynchronous generation for very large results

For Redis/live cache:

- define the authoritative owner of each key/value
- include update timestamp/source where freshness matters
- plan TTL/eviction behavior
- distinguish cache miss from device offline/stale state
- prevent stale writes from overwriting newer state
- avoid broad key scans in request paths

For schema changes:

- include forward migration and compatibility notes
- preserve old readers/writers during staged deployment where needed
- define backfill strategy and data validation

## 5. Performance

For each data-heavy feature, estimate cardinality and refresh frequency before coding.

Check:

- query count and rows scanned
- payload size
- serialization cost
- client rendering count
- polling/WebSocket/event frequency
- duplicate requests
- map marker count
- chart point count
- video bandwidth/connections
- cache hit/miss behavior

Prefer incremental updates for live screens. Avoid re-rendering entire map/list/dashboard for one vehicle update.

## 6. Frontend integration

Before changing frontend architecture, inspect whether the active area is Razor/MVC, SPA, mobile, or another client.

Keep UI state explicit:

- loading
- empty
- success/fresh
- stale
- offline/disconnected
- partial data
- permission denied
- recoverable error/retry
- terminal error

Do not show fake success after an API failure. Avoid hard-coded demo values in production paths.

## 7. Streaming/live systems

For GPS/websocket/video flows:

- define connection lifecycle and retry/backoff
- prevent multiple unintended subscriptions/players
- clean up event handlers/media resources on navigation
- correlate stream/device/channel identifiers explicitly
- handle slow networks and temporary gateway failures
- expose observable connection state to support/debugging
- measure end-to-end latency separately from player buffer latency

For video, confirm browser/app codec and transport compatibility before changing transcoding or gateway design.

## 8. Refactoring

Refactor only within the needed blast radius unless the user requested broader cleanup.

Safe order:

1. characterize existing behavior with tests or reproducible evidence
2. separate pure logic from side effects
3. introduce interface/adapter boundaries if needed
4. migrate callers incrementally
5. remove old path only after all callers/tests are updated

Avoid style-only churn in files touched for functional changes.

## 9. Release discipline

Before completion:

- inspect final diff and status
- confirm migrations/config/env variables are documented
- verify logs do not include secrets or excessive personal/location data
- note deployment order for cross-service changes
- include rollback steps for schema/protocol/streaming changes
- state untested paths explicitly
