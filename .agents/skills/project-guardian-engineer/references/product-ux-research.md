# UI/UX and Product Research Guide

## Contents

1. UX audit
2. Fleet information architecture
3. Map-first operations
4. Dashboard and analytics design
5. Video and event review
6. Chat/AI UX
7. Design system
8. Competitor benchmarking
9. Acceptance checklist

## 1. UX audit

For an existing screen, capture:

- primary user role and task
- primary decision/action
- current entry points and exit points
- information hierarchy
- repeated or hidden actions
- cognitive load and number of context switches
- responsiveness and touch targets
- loading/empty/error/stale/permission states
- accessibility issues
- inconsistent components/tokens
- data freshness and confidence signals

Do not redesign merely to look modern. Tie each change to usability, speed, error reduction, or decision quality.

## 2. Fleet information architecture

A strong default hierarchy for fleet operations is:

1. **Fleet overview**: what needs attention now?
2. **Live operations/map**: where are assets and what state are they in?
3. **Vehicle/asset detail**: current state, trip/history, alerts, health, video.
4. **Drivers/safety**: behavior, events, coaching/actions.
5. **Trips/routes**: timeline, route, exceptions/events, media.
6. **Maintenance/device health**: upcoming, overdue, offline/diagnostic issues.
7. **Alerts/geofences**: rules, active incidents, history.
8. **Analytics/reports**: trends, comparisons, exports.
9. **AI assistant**: natural-language access across the above, not a disconnected novelty page.
10. **Administration**: users/roles, devices, integrations, settings.

Adapt this to the current product's actual roles and modules; do not force absent product areas.

## 3. Map-first operations

Design the map and list/detail panel as synchronized views.

Recommended behavior:

- selected item persists between list and map
- filters summarize active constraints and can be cleared quickly
- vehicle state has icon + text/color redundancy
- clusters communicate fleet density without hiding critical alerts
- area selection can answer “what happened here?” with vehicles/events/time window
- a vehicle drawer/panel shows essential state without navigating away
- historical trip playback separates route, stop/idle segments, alerts, and video/event evidence

Avoid overloaded markers with many tiny icons or tooltips that require precise pointer movement.

## 4. Dashboard and analytics design

Start with decisions, then metrics.

Every KPI card should define:

- metric name
- value and unit
- comparison period or target when meaningful
- freshness/time window
- drill-down destination
- behavior for no data/partial data

Use charts only when they reveal a pattern. Prefer tables for precise operational lookup. Keep color semantics consistent across dashboard, map, alerts, and exports.

Useful fleet patterns include:

- exception-first overview instead of a wall of equal-weight KPIs
- trend + threshold + actionable outliers
- driver/vehicle scorecards with transparent factor breakdown
- device/camera health alongside operational metrics
- unified trip timeline containing location, exceptions, alerts, and video

## 5. Video and event review

The reviewer should be able to answer quickly:

- which vehicle/driver?
- when and where?
- what triggered the event?
- what did telemetry show immediately before/after?
- which camera/channel is shown?
- how severe/confident is the classification?
- what action is required?
- has it been reviewed/coached/resolved?

Keep playback controls and event context visible together. Preserve filters and queue position when returning from an event.

## 6. Chat/AI UX

A fleet assistant should expose:

- suggested questions based on the user's role
- visible scope: fleet/group/vehicle/date range
- “data current as of” timestamp
- concise answer first, expandable evidence/details second
- links/actions into the relevant the current product screen
- source/tool evidence for operational facts
- confirmation UI for any write/action
- permission-safe refusals when data is outside scope
- feedback controls tied to conversation/tool traces

Avoid pretending uncertain AI output is a live telemetry fact. Use deterministic data tools for the fact and the model for interpretation.

## 7. Design system

Before introducing new styling, inventory existing:

- typography
- color/semantic states
- spacing
- radii/shadows
- buttons
- form fields
- tabs
- cards
- tables
- modals/drawers
- badges
- map controls
- charts
- icons

Consolidate repeated inline styles and near-duplicate components when the task's scope supports it. Ensure components support disabled/loading/error states and keyboard focus.

## 8. Competitor benchmarking

Benchmark workflows, not brands. For each competitor feature:

1. Record the official source and date.
2. Describe the user problem, not marketing language.
3. Identify required data/hardware/workflow.
4. Check whether the current product has the necessary data and operational maturity.
5. Estimate implementation complexity and risks.
6. Decide:
   - `adopt now`: high value, data ready, acceptable cost
   - `adapt`: useful pattern but needs the current product-specific implementation
   - `prototype`: promising but uncertain value/data/model accuracy
   - `defer`: valuable later; dependency missing
   - `reject`: poor fit, high risk, or weak user value

Do not copy screen layout pixel-for-pixel. Originality matters.

## 9. Acceptance checklist

A UI change should pass:

- primary task faster or clearer than before
- no hidden critical action caused by redesign
- keyboard/focus behavior works
- meaningful labels and accessible names exist
- contrast and status semantics are understandable
- mobile/narrow-screen overflow is handled
- loading/empty/error/stale/permission states are designed
- long names, large values, localization, and different date formats do not break layout
- map/video controls remain usable at target viewport sizes
- analytics specify units, periods, and freshness
