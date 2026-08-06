# Phase 32 — MIME Cache Update Detach Avoidance: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-221: Revalidate U-05 I-06 against current HEAD.
  - Check: confirm both `onQueryResult()` and successful `setDefaultAsync()`
    callbacks snapshot `mime_cache_` before a single-key mutation, and that
    `emitChangedSignals()` only consumes resolved role values from that
    snapshot.
  - Result: confirmed. Both paths copied `mime_cache_` immediately before a
    single-entry mutation, forcing an implicit-sharing detach; the old snapshot
    was used only to resolve pre-update role defaults.

## Implementation and Tests

- [x] T-222: Centralize MIME cache mutation and signal comparison without a
  full-cache snapshot.
  - REQs: REQ-F-01
  - Files: `libs/holonight-services/src/mime/MimeService.{h,cpp}`.
  - Check: query-result and successful setter paths share the helper; no
    `const QHash<QString, QString> old_cache = mime_cache_` remains; all
    existing role-specific signals retain their prior conditions.
  - Result: both paths use `updateMimeCache()`, which saves each resolved role
    default before `QHash::insert()` and compares it afterwards. No cache
    snapshot remains.

- [x] T-223: Add or update focused MIME-service regression coverage.
  - REQs: REQ-F-01
  - Files: `tests/test_mime_service_file_watch.cpp` as appropriate.
  - Check: changing a MIME association updates only the affected role value and
    preserves stale-query protection and role-specific signal behavior.
  - Result: extended `MimeServiceDefaults.SuccessfulSetUpdatesRoleAndIgnoresStaleQuery`
    to assert a text-editor update emits no terminal, file-manager,
    image-viewer, or video-player signal. Focused MIME coverage passes 10/10.

## Validation and Handoff

- [x] T-224: Run project validation.
  - Check: focused MIME-service tests, `task test`, `task architecture-check`,
    `task format-check`, and `git diff --check`; record unrelated existing
    failures separately.
  - Result: focused MIME coverage passed 10/10; `task architecture-check` and
    `git diff --check` pass; changed C++ files pass direct
    `clang-format --dry-run --Werror`. Full CTest coverage ran 943 tests: 916
    passed, one environment-gated monitor-validation test was skipped, and 26
    unrelated socket/D-Bus/network integration tests failed in the current
    environment (including the pre-existing keyboard-layout failure).
    `task format-check` reports only the four pre-existing formatting
    violations in `libs/holonight-core/src/HyprlandWorkspaceService.cpp`.

- [x] T-225: Record user acceptance and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the queued Low backlog
    only after acceptance evidence is recorded.
  - Result: user verified that the MIME default-role behavior is preserved.
    `9f9d232` (`perf: avoid MIME cache detaches`) implements U-05 I-06; the
    other 37 Low-severity candidates remain queued.
