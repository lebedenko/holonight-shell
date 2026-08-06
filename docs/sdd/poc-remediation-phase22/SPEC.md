# Phase 22 — Bounded Portal D-Bus Probes

**Status**: Complete — implementation, automated validation, and user verification passed.

## Objective

Remediate U-05 I-07: bound the asynchronous D-Bus calls used to discover the
desktop portal and read its appearance settings, instead of relying on the
long library default timeout.

| Source | Phase 22 item | Impact |
|---|---|---|
| U-05 I-07 | Explicit portal D-Bus timeout | An unresponsive portal broker stops delaying availability and appearance-state recovery after the chosen bounded interval. |

## Functional Requirements

### REQ-F-01 — Use a bounded probe timeout

`SystemPortalDBus` shall apply one explicit five-second timeout to its
asynchronous `NameHasOwner`, `Introspect`, `ListNames`, and Settings `Read`
calls.

- The timeout applies only to portal discovery and settings-read traffic; file
  chooser and URI requests retain their existing semantics.
- `PortalService` shall continue to handle failed replies through its current
  unavailable, partial-probe, and settings-error paths.
- The production default shall be five seconds, while an internal constructor
  seam may accept a shorter timeout for deterministic automated coverage.
- No QML-facing API, portal capability interpretation, or retry policy changes.

**Acceptance**: a session-bus test delays a portal Settings reply beyond a
short injected timeout and observes the resulting D-Bus no-reply error before
the delayed reply could arrive; existing portal service tests retain their
current startup and settings behavior.

## Constraints and Verification

- Keep the change scoped to U-05 I-07; do not rename portal abstractions,
  change MIME caching, or add retries.
- Reuse the session-bus test infrastructure; do not require a live portal
  broker.
- Run focused portal tests, `task test`, `task format-check`, and
  `git diff --check`.

## Out of Scope

- The other 49 queued Low-severity candidates after this planned tranche.
- Portal naming cleanup (U-05 I-05), MIME-cache copy-on-write cleanup
  (U-05 I-06), and portal color-scheme constants (U-05 I-08).
- Timeout changes for user-initiated file chooser or URI requests.
