# Phase 21 — Shared Desktop File Discovery: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-166: Revalidate U-05 I-03 against current HEAD.
  - Result: `ApplicationCacheRebuilder.cpp` and
    `SessionIntegrationService.cpp` still independently construct identical
    recursive `QDirIterator` scans using `*.desktop`, `QDir::Files`, and
    `QDirIterator::Subdirectories`. Their callers impose different, valid
    policy around that common query.

## Implementation and Tests

- [x] T-167: Add a focused shared desktop-file discovery helper and migrate
  both callers.
  - REQs: REQ-F-01
  - Files: `libs/holonight-services/src/session-integration/DesktopFileUtils.{h,cpp}`, `ApplicationCacheRebuilder.cpp`, `SessionIntegrationService.cpp`.
  - Check: retain recursive regular-file filtering and each caller's existing
    surrounding eligibility policy.
  - Result: `DesktopFileUtils::containsDesktopFiles()` now owns the unchanged
    recursive regular-file scan. The cache rebuilder retains its writable-dir
    predicate and diagnostics retain their existing-dir predicate.

- [x] T-168: Cover shared traversal behavior with the existing
  session-integration test target.
  - REQs: REQ-F-01
  - Files: `tests/test_session_integration_service.cpp`.
  - Check: verify a nested `.desktop` file is found and a non-matching file is
    ignored; preserve current caller coverage.
  - Result: a deterministic temporary-directory test covers a nested matching
    desktop file and a `.desktop.bak` near miss. Existing rebuild and
    diagnostics tests continue to cover their callers.

## Validation and Handoff

- [x] T-169: Run focused and project validation.
  - Check: focused session-integration tests, `task test`, `task format-check`,
    and `git diff --check`.
  - Record unrelated pre-existing failures separately.
  - Result: the focused `DesktopFileUtilsTest.*`,
    `ApplicationCacheRebuilderTest.*`, and `SessionIntegrationServiceTest.*`
    selection passed all 19 tests. `task test` passed all 937 tests.
    `task format-check` reports only the four pre-existing formatting
    violations in `libs/holonight-core/src/HyprlandWorkspaceService.cpp`; all
    Phase 21 C++ files pass `clang-format --dry-run --Werror`. Final diff
    checks passed.

- [x] T-170: Record user verification and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the unscheduled Low
    backlog from 50 to 49 only after acceptance evidence is recorded.
  - Result: user verified the completed phase. `96fdc5f` (`fix: share desktop
    file discovery`) implements U-05 I-03; the other 49 Low-severity
    candidates remain queued.
