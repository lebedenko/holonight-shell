# Phase 21 — Shared Desktop File Discovery: Design

**Input**: `poc-remediation-phase21/SPEC.md`
**Baseline**: Phase 20 accepted in `7ce92c0`.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `session-integration/DesktopFileUtils.{h,cpp}` and both current callers | `test_session_integration_service.cpp` temporary-directory regression |

## 2. Design Decisions

### 2.1 Extract the existing traversal verbatim

Move the current `QDirIterator` construction into a narrowly named helper in
the session-integration area. The helper owns only the shared question —
whether a directory recursively contains a regular `*.desktop` file — so both
callers retain responsibility for their distinct existence and writability
checks.

### 2.2 Keep filesystem reads live and call-local

The helper performs the same traversal each time it is called. Introducing a
cache would change freshness and invalidation semantics for a maintenance-only
deduplication; it is intentionally outside this phase.

### 2.3 Test traversal semantics at the shared boundary

Add a temporary-directory test for a nested matching file and a non-matching
near miss. Existing session-integration tests already exercise rebuild
selection and MIME diagnostics; retaining them ensures the extraction does
not alter either caller's observable contract.

## 3. Risks and Boundaries

- A non-recursive iterator or a relaxed filter would silently change which
  XDG application directories qualify, so the helper must preserve both flags
  exactly.
- The utility remains private implementation support, not a cross-library API.
- No directory result caching is introduced because callers can observe
  filesystem changes between calls today.
