# Phase 45 — Weather and Calendar Data-Path Cleanup: Design

## Structured weather failures

`WeatherProvider` classifies completed weather replies as transient or
authentication failures while it still has access to the HTTP status. Its
failure signal carries that enum alongside the existing human-readable detail.
`WeatherService` keeps the existing stale-state and logging behavior for both
classes, but only transient failures advance backoff and arm the refresh timer.
For HTTP 401/403 it emits `authenticationFailed` and waits for an external
configuration or activity transition instead of issuing the same rejected
request indefinitely.

## Forecast variant cache

Keep the typed `QList<HourlyEntry>` and `QList<DailyEntry>` as the authoritative
weather state used for persistence. Maintain parallel `QVariantList` snapshots
for QML and rebuild them at the two state-replacement boundaries: successful
fetch and cache load. Property getters then return the prepared snapshot rather
than converting every typed entry on each read.

## Calendar property dispatch

Map supported upper-cased VEVENT property names to a compact enum in one static
`QHash`. A single lookup rejects unknown extensions; an exhaustive switch
applies the existing per-property parsing. This preserves RFC behavior while
replacing up to 28 sequential string comparisons with direct lookup and keeps
the supported-name contract visible in one table.

## Verification Strategy

- Exercise transient and authentication failure policies through the injected
  weather provider.
- Verify forecast variant snapshots update after replacement.
- Run the existing all-fields iCalendar coverage, including escaped and
  repeated properties.
- Run formatting, architecture checks, and the full project test suite.
- Manually verify ordinary weather and calendar surfaces.
