# Phase 22 — Bounded Portal D-Bus Probes: Tasks

**Status**: Complete — implementation, automated validation, and user verification passed.

## Pre-flight

- [x] T-171: Revalidate U-05 I-07 against current HEAD.
  - Result: `SystemPortalDBus::nameHasOwner()`, `introspectPortal()`,
    `listNames()`, and `readSetting()` each call `QDBusInterface::asyncCall()`
    without first setting an explicit timeout. `PortalService` already handles
    their failed replies, and `IPortalDBus` provides a stable test seam.

## Implementation and Tests

- [x] T-172: Apply one bounded timeout to portal probe and Settings-read calls.
  - REQs: REQ-F-01
  - Files: `libs/holonight-services/src/portal/NullPortalBackend.{h,cpp}`.
  - Check: use a five-second production default for `NameHasOwner`,
    `Introspect`, `ListNames`, and `Read`; leave `openFile()` and `openUri()`
    unchanged.
  - Result: `SystemPortalDBus` now applies its five-second default before
    every listed probe/read call. Its internal constructor accepts a timeout
    and portal-service name for isolated transport tests; user-initiated calls
    retain their prior timeout behavior.

- [x] T-173: Add deterministic timeout regression coverage.
  - REQs: REQ-F-01
  - Files: `tests/test_portal_service.cpp`.
  - Check: a delayed Settings reply exceeds a short injected timeout and
    produces Qt's resulting no-reply error without requiring a live portal
    broker.
  - Result: a threaded fake owns a unique portal service and delays `Read` for
    two seconds. The regression uses a 500ms timeout and observes Qt's
    `QDBusError::NoReply` before that reply can arrive, without colliding with
    the desktop session's real portal.

## Validation and Handoff

- [x] T-174: Run focused and project validation.
  - Check: focused portal tests, `task test`, `task format-check`, and
    `git diff --check`.
  - Record unrelated pre-existing failures separately.
  - Result: focused `SystemPortalDBusTest.*` and `PortalServiceTest.*` passed
    all 24 tests. `task test` passed all 938 tests. `task format-check` reports
    only the four pre-existing formatting violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp`; all Phase 22 C++
    files pass `clang-format --dry-run --Werror`. Final diff checks passed.

- [x] T-175: Record user verification and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the unscheduled Low
    backlog from 49 to 48 only after acceptance evidence is recorded.
  - Result: user verified the completed phase. `4e8fa56` (`fix: bound portal
    D-Bus probes`) implements U-05 I-07; the other 48 Low-severity candidates
    remain queued.
