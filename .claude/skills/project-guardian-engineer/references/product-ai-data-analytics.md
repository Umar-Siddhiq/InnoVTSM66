# Data Science, AI Analytics, and Chatbot Guide

## Contents

1. Analytics problem framing
2. Telematics data contracts
3. Feature engineering
4. Modeling and evaluation
5. Production ML
6. AI assistant architecture
7. Chatbot tool contract
8. Evaluation set
9. AI observability

## 1. Analytics problem framing

Convert broad requests such as “add AI analytics” into an operational decision.

Template:

- **User:** who consumes the result?
- **Decision:** what action changes because of it?
- **Prediction/score:** what exactly is produced?
- **Horizon:** now, next trip, next 7 days, etc.
- **Cost of false positive / false negative:** operational consequences.
- **Baseline:** rules/manual process/simple statistics.
- **Success metric:** measurable business + model metric.

Do not deploy ML when a transparent rule reliably solves the problem.

## 2. Telematics data contracts

For each source, document:

- device/vehicle/driver/tenant keys
- event timestamp and receive timestamp
- units and coordinate convention
- sampling frequency
- expected nulls and sentinel values
- firmware/protocol variants
- duplication/out-of-order behavior
- retention and aggregation levels
- privacy/access class

Validate impossible coordinates, time reversals, speed spikes, stale readings, duplicate packets, and device identity changes before feature generation.

## 3. Feature engineering

Common categories include:

- trip duration/distance
- stop/idle duration
- speed distribution and overspeed exposure
- harsh braking/acceleration/cornering rates normalized by distance/time
- route deviation and geofence events
- signal/device health and data gaps
- battery/voltage/engine diagnostic trends when available
- day/time/road/weather/context only when legally and operationally appropriate
- rolling trend/delta features instead of only absolute readings

Prevent future leakage: a feature used for a prediction at time `t` must not contain data from after `t`.

## 4. Modeling and evaluation

Order of effort:

1. deterministic business-rule baseline
2. simple interpretable statistical/ML baseline
3. more complex model only if it gives meaningful incremental value

For time-dependent fleet data:

- split by time, and when needed by vehicle/fleet, rather than random-row split
- keep preprocessing fitted only on training data
- evaluate across vehicle types, regions, firmware/device families, and data-quality strata when available
- report precision/recall or cost-weighted metrics that match the operational action; accuracy alone is often misleading
- calibrate thresholds against alert volume and review capacity
- provide feature attribution/explanation where managers need to act on the result

For forecasting, compare against naive seasonal/recent-value baselines.

## 5. Production ML

Require:

- versioned feature/data definitions
- reproducible training configuration
- model/version metadata
- fallback when input features are missing
- monitoring for input drift, prediction drift, calibration/precision changes, latency, and alert volume
- shadow/canary rollout for high-impact alerts
- human review for early deployments where mistakes are costly

Never silently change an operational scoring formula without versioning and release notes.

## 6. AI assistant architecture

Recommended flow:

`Chat UI -> authenticated the current product backend -> AI orchestrator -> policy/tool router -> approved domain tools -> API/semantic data layer -> response with evidence`

Keep provider API keys server-side. Pass user/tenant authorization context to every tool server-side; do not trust model-generated tenant IDs.

Use a small set of explicit domain tools such as:

- `get_fleet_summary(time_range, filters)`
- `get_vehicle_live_state(vehicle_id)`
- `get_vehicle_trip_history(vehicle_id, from, to)`
- `get_alerts(filters, time_range)`
- `get_driver_safety_summary(driver_id, time_range)`
- `get_device_health(filters)`
- `compare_metric(metric, group_by, time_range, filters)`

Tool names/arguments must match the actual application APIs. These are patterns, not mandatory endpoint names.

Use governed aggregate/query tools rather than unrestricted LLM-generated SQL in production. If text-to-SQL is explored, restrict it to read-only replicas/views, enforce tenant predicates independently, validate SQL AST/query cost, apply row/timeout limits, and test injection/cross-tenant attacks.

## 7. Chatbot tool contract

Every factual tool result should include enough metadata for the assistant to state:

- data scope/entity
- time range
- data freshness/as-of time
- units
- filters
- partial/missing data flags
- stable IDs or deep-link target

For write tools:

1. validate permission and entity ownership before model invocation where possible
2. return a preview/diff
3. require explicit user confirmation
4. execute idempotently
5. return durable action/audit ID

## 8. Evaluation set

Build a versioned test set covering at least:

### Grounded fleet questions

- Where is vehicle X and how fresh is the position?
- Which vehicles have been idle longest today?
- Show vehicles with no data in the last N minutes.
- Compare distance/utilization by group for a date range.
- Why is this vehicle marked offline/stale?
- Summarize this trip and its alerts.

### Ambiguity

- duplicate vehicle names
- unclear dates such as “yesterday” across timezone boundaries
- no-data/partial-data cases
- conflicting live cache vs historical data

### Authorization

- vehicle outside tenant
- user lacks video permission
- user lacks admin/write permission
- injected prompt asking to ignore access controls

### Reliability

- tool timeout
- stale data
- malformed tool result
- empty results
- model tries to invent a value absent from tool output

Measure answer correctness, groundedness, tool selection, authorization correctness, latency, and user usefulness.

## 9. AI observability

Track:

- model/provider/version
- prompt/tool versions
- token and monetary cost
- tool call count and latency
- total response latency
- tool failures/timeouts
- unsafe/unauthorized attempts
- groundedness/evaluation scores
- user feedback and task completion

Redact secrets and minimize sensitive location/driver content in logs. Define retention separately from product telemetry retention.
