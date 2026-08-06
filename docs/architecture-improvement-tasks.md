# Architecture Improvement Tasks

Source review: [architecture-review-2026-06-18.md](architecture-review-2026-06-18.md)

## Active Order

- [x] T-001: Make the `surfaces -> services` dependency policy explicit and add a boundary check.
- [x] T-002: Validate generated QML type metadata in CI.
- [x] T-003: Document why QML metatype combining uses Qt internal CMake APIs.
- [x] T-004: Decide whether `core` remains a pragmatic Qt/platform-adjacent layer or is split into pure domain plus integration.
- [x] T-005: Extract `ShellApplication` control-socket handling into a small coordinator.
- [x] T-006: Split `ConfigService` parsing by configuration section.
- [x] T-007: Add table-driven parser tests for split config sections.
- [x] T-008: Replace launcher recent-app singleton access with constructor injection.
- [x] T-009: Extract notification placement and timeout policy into pure helpers.
- [x] T-010: Add launcher `.desktop` Exec parsing tests for real-world edge cases.
- [x] T-011: Add a compositor integration checklist or script for popup/sidebar/tray/notification/widget smoke checks.
- [x] T-012: Add focused QML component instantiation tests with representative fake services.
- [x] T-013: De-duplicate CI dependency installation into a reusable setup script.
- [x] T-014: Add `task clean-artifacts` for local build/log cleanup if generated artifacts keep showing up in scans.

## Task Details

### T-001: Surface-Service Boundary Policy

The current code intentionally lets `holonight_surfaces` depend on `holonight_services` for presentation orchestration that needs live service state. The dependency should be honest in CMake/docs, and new direct service includes from `src/surfaces` should require an explicit allowlist update.

Verification:

- `task architecture-check`

### T-002: QML Type Metadata Validation

Add a focused check that fails when `build/HolonightShell/holonight-shell.qmltypes` is empty or missing known QML-facing C++ types such as `AudioService`, `WorkspaceModel`, and `NotificationService`.

Verification:

- `task qmltypes-check`
- CI build/test job runs the check after build.

### T-003: Document Internal Qt CMake Usage

Document the reason for `_qt_internal_*` and manual metatype combining in `docs/dev-setup.md`, including the expected failure mode when generated qmltypes regress to an empty `Module {}`.

Verification:

- Documentation review only.

### T-004: Decide `core` Boundary

Chosen path: keep `core` as a pragmatic shared model/config and workspace integration layer for now, and document that it is not pure domain.

The larger alternative, moving Hyprland/ext-workspace integration out of `core`, remains available as a future refactor but is not required to resolve the architecture review's immediate honesty gap.

Verification:

- CMake comments, README architecture text, and SDD architecture notes agree.

### T-005: Extract Control Socket Coordinator

Move control-socket setup, command parsing, and dispatch out of `ShellApplication` into a small class owned by the app composition root.

Verification:

- Existing tests pass.
- Add focused command parsing tests if parsing is separable without Qt socket setup.

### T-006: Split Config Parsing

Move section-specific parsing out of `ConfigService.cpp` into small parser helpers for appearance, weather, notifications, widgets, and related sections.

Verification:

- Existing config behavior tests pass.
- No schema or default-value behavior changes.

### T-007: Config Parser Tests

Add table-driven tests per config section, covering defaults, valid overrides, invalid values, and legacy/missing fields where applicable.

Verification:

- `task test`

### T-008: Launcher Recent Apps Injection

Replace direct `RecentAppsTracker::instance()` usage in `LauncherService` with constructor injection of a tracker pointer or a tiny recorder interface.

Verification:

- Launcher tests cover recording enabled/disabled paths without global state.

### T-009: Notification Policy Helpers

Extract notification placement, timeout, and history grouping policy from `NotificationService` into pure helpers.

Verification:

- Helper tests run without QML or D-Bus.
- Existing notification model behavior remains unchanged.

### T-010: Desktop Exec Edge Cases

Add launcher command handling tests for quoted arguments, escaped spaces, `env FOO=bar app`, terminal applications, `%U/%F`, and invalid `Exec` lines.

Verification:

- `task test`

### T-011: Compositor Integration Checklist

Add a small documented manual checklist or script for popup positioning, sidebar open/close, tray menu, notifications, and widgets in a live Hyprland session.

Verification:

- Checklist is runnable from the current repository and names required commands/tools.
- `task compositor-smoke-check`

### T-012: Focused QML Tests

Add QML tests for required properties and component instantiation for high-risk components, using `FakeQmlServices`.

Verification:

- `task test`
- `task qml-lint`

### T-013: CI Dependency Setup Reuse

Move duplicated apt dependency lists from CI jobs into a shared script or composite action.

Verification:

- CI jobs still install the extra static-check tools only where needed.

### T-014: Local Artifact Cleanup

Only if local artifact noise persists, add an explicit `task clean-artifacts` that removes ignored build directories and logs after confirmation.

Verification:

- Dry-run or confirmation behavior prevents accidental destructive cleanup.
- `scripts/clean-artifacts.sh --dry-run`
- `task --list`
