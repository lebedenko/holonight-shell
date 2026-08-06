# Phase 32 — MIME Cache Update Detach Avoidance

**Status**: Complete — automated checks and user verification passed.

## Objective

Remediate U-05 I-06: avoid copying and detaching `MimeService`'s complete MIME
cache whenever a single MIME association changes, while preserving its
QML-facing default-role values and change signals.

| Source | Phase 32 item | Impact |
|---|---|---|
| U-05 I-06 | Avoid `QHash` detach on single-key MIME-cache updates | Default-application refreshes retain their existing behavior without copying the whole cache. |

## Functional Requirements

### REQ-F-01 — Update the cache without a full snapshot

`MimeService` shall update one MIME association without retaining a shared
copy of the complete `mime_cache_` during that mutation.

- Both asynchronous query results and successful default-setting callbacks use
  the same update-and-notify path.
- Before the mutation, capture only the observable role-default values needed
  to decide which QML notifications changed.
- After the mutation, emit each existing role-specific change signal exactly
  when its resolved value differs from its pre-update value.

**Acceptance**: a single-key update does not copy `mime_cache_`; all default
role getters, stale-query protection, and role-specific signal behavior remain
unchanged.

## Constraints and Verification

- Keep the current `IMimeResolver` asynchronous contract and generation-based
  stale-query protection intact.
- Do not change browser-default handling, MIME role definitions, file watching,
  or QML API names.
- Keep the implementation local to `MimeService`; do not add dependencies or
  expose cache internals for testing.
- Run focused MIME-service coverage, `task test`, `task architecture-check`,
  `task format-check`, and `git diff --check`.

## Out of Scope

- MIME resolver command behavior or desktop-file association policy.
- Portal, session-integration, or launcher remediations.
- The remaining queued Phase 7 Low-severity candidates.
