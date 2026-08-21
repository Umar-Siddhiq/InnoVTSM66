# Quality, Security, Privacy, and Release Checks

## Security boundaries

Review any affected path for:

- authentication bypass
- object-level authorization / IDOR
- cross-tenant data exposure
- insecure direct access to video or location endpoints
- secrets in source, client bundles, logs, error payloads, or screenshots
- SQL/command/template injection
- XSS from vehicle/driver/user-entered fields
- CSRF for cookie-authenticated writes
- unsafe file/media upload or path traversal
- SSRF through remote URL/stream/import functionality
- overly permissive CORS
- missing rate limits on costly/search/AI/video endpoints
- missing timeout/retry limits

## Privacy

Live location, trips, driver behavior, camera media, and identity data can be sensitive. Apply least privilege, purpose limitation, retention controls, and auditability appropriate to the product's obligations.

Do not send more data to an AI provider than the task requires. Prefer IDs and minimal structured facts over raw bulk telemetry or video transcripts when possible.

## Multi-tenant checklist

- tenant comes from trusted authenticated context
- backend query adds tenant scope independently of client/model input
- cache key cannot collide across tenants
- media URLs cannot be guessed/shared indefinitely
- exports/reports preserve tenant scope
- AI retrieval/tools cannot cross tenant boundaries
- background jobs carry tenant context explicitly

## Observability

For important distributed flows, log structured events with:

- correlation/request ID
- safe tenant/user identifier when appropriate
- device/vehicle ID (avoid unnecessary personal fields)
- operation/stage
- duration
- outcome/error code
- dependency status

Metrics should cover request rate, failures, latency percentiles, queue/backlog depth, cache behavior, DB pool/query latency, live connection count, stream failures, AI tool/model latency, and alert pipeline health as relevant.

## Release test matrix

For each change, choose rows proportionate to risk:

| Area | Minimum check |
|---|---|
| API | valid, invalid, unauthorized, not-found, timeout/dependency error |
| DB | expected query result, empty set, large set/pagination, migration compatibility |
| Cache/live | fresh, stale, missing, out-of-order update |
| UI | loading, success, empty, error, permission, narrow viewport |
| Map | large marker set, selected item, filters, stale location |
| Video | connect, unauthorized, invalid channel, reconnect, cleanup |
| AI | grounded answer, no-data, unauthorized entity, tool failure, adversarial prompt |
| Export/report | filters match UI, large output behavior, tenant scope |

## Severity guide for reviews

- **Critical:** cross-tenant/security breach, data corruption, destructive failure, major safety/compliance risk.
- **High:** incorrect core fleet state/analytics, frequent outage, broken auth, major performance failure.
- **Medium:** material workflow defect with workaround, inconsistent calculations, accessibility issue blocking some users.
- **Low:** minor UX/developer-quality issue with limited operational impact.

Always include file/function evidence for findings when the code is available.
