# Phase 45 — Weather and Calendar Data-Path Cleanup

## Status

Complete — implementation, automated validation, and user verification passed.

## Goal

Bundle the three remaining U-07 findings from the Phase 7 backlog. Preserve
weather presentation and calendar parsing while stopping futile authentication
retries, caching QML forecast conversions, and using direct calendar-property
dispatch.

## Scope

| Source | Phase 45 item | Required outcome |
|---|---|---|
| U-07 I-07 | Classify weather authentication failures | HTTP 401/403 failures are observable separately and do not retry until configuration or activity triggers a new request. |
| U-07 I-08 | Cache forecast variants | QML property reads reuse lists rebuilt only when forecast data changes or cache data loads. |
| U-07 I-09 | Dispatch iCalendar properties directly | Supported VEVENT properties retain their parsing behavior without a sequential name-comparison chain. |

## Acceptance Criteria

1. Transient weather failures retain bounded exponential retry behavior.
2. Authentication failures emit a dedicated service signal, mark data stale,
   and leave the refresh timer stopped.
3. Hourly and daily QML values are refreshed after network and disk-cache data
   changes without rebuilding their variants on each property read.
4. Every previously supported VEVENT field, including repeated and escaped
   values, parses identically.
5. Focused weather/calendar tests, formatting, architecture checks, and the full
   project suite pass apart from documented pre-existing failures.
6. Manual verification confirms normal weather updates and calendar display
   remain unchanged.

## Non-goals

- Adding UI for authentication failures or changing weather configuration.
- Changing weather refresh intervals, transient backoff, cache format, or
  provider endpoints.
- Expanding the set of supported RFC 5545 properties.
- Changing the already accepted Phase 12 U-09 visual remediations.

## Backlog Accounting

Phase 44 left three queued Low-severity candidates. Acceptance of this
three-item tranche reduces the queued Phase 7 backlog from three to zero.
