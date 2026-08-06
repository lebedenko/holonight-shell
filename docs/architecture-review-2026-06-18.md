# Architecture Review Report

Date: 2026-06-18

## Executive Summary

`holonight-shell` is a Qt6/C++23 modular monolith with a good recent split into `platform`, `core`, `services`, `surfaces`, and `app` static targets. The project has unusually good test and tooling coverage for a personal shell project: CTest/GTest, qmllint, clang-format, clang-tidy, coverage, CI, and SDD docs are all present.

The main architectural risks are boundary drift, handwritten CMake/QML plumbing, growing manager/service classes, and singleton/global state that weakens testability as features expand. There are no critical findings in this review.

## Severity Summary

| Severity | Count |
|---|---:|
| Critical | 0 |
| High | 3 |
| Medium | 6 |
| Low | 3 |

## Findings

### 1. High: Target boundaries are documented but not enforced

**Confidence:** 0.92

**Evidence:**
- `CMakeLists.txt:344` says `holonight_surfaces` must not depend on services.
- `CMakeLists.txt:394` links `holonight_surfaces` to `holonight_services`.
- `src/surfaces/NotificationManager.cpp` includes `NotificationService`.
- `src/surfaces/WidgetManager.cpp` includes `MonitorOccupancyService`.

**Why it matters:**
The current dependency graph allows UI surface code to know service internals. That makes future service/presentation changes harder and weakens the value of target-level tests.

**Suggestion:**
Choose one architecture explicitly:

1. If `surfaces` is presentation-only, move `NotificationManager` and widget occupancy orchestration into `app` or a thin coordinator target, then inject QML-facing models into surfaces.
2. If `surfaces` intentionally owns tray/notification/widget presentation orchestration, update the CMake comments and README architecture so the dependency is honest.
3. Add a lightweight include-boundary check in CI, for example `rg '#include ".*Service' src/surfaces` with an allowlist.

### 2. High: `core` is not actually pure domain

**Confidence:** 0.88

**Evidence:**
- `CMakeLists.txt:213` describes `holonight_core` as "pure domain models".
- `CMakeLists.txt:236` links `holonight_core` to `holonight_platform`, `Qt6::DBus`, and `Qt6::Qml`.
- `holonight_core` contains `ExtWorkspaceManager` and `HyprlandWorkspaceService`.

**Why it matters:**
Domain/model code becomes coupled to Hyprland, Wayland, D-Bus, and QML concerns. That slows tests and makes it harder to reuse or reason about model behavior independently.

**Suggestion:**
1. Keep `WorkspaceModel`, `BatteryState`, `AudioState`, `SystemInfo`, config value types, and pure helpers in `core`.
2. Move Hyprland/ext-workspace integration into `services` or `platform/hyprland`.
3. Let `core` depend on `Qt6::Core`/`Qt6::Qml` only if QML annotations remain there; otherwise isolate QML adapters.

### 3. High: CMake/QML registration is powerful but fragile

**Confidence:** 0.86

**Evidence:**
- QML files are manually listed in `CMakeLists.txt:464`.
- The manual list is checked by glob at `CMakeLists.txt:557`.
- QML is registered with `NO_GENERATE_QMLTYPES` at `CMakeLists.txt:586`.
- C++ QML metatypes are custom-combined through Qt internal functions starting at `CMakeLists.txt:644`.

**Why it matters:**
This works, but it is coupled to Qt private/internal CMake APIs. Qt upgrades can break type registration, qmllint, or QML singleton visibility in non-obvious ways.

**Suggestion:**
1. Add a focused CI check that fails if generated `holonight-shell.qmltypes` is empty or misses known singleton types like `AudioService`, `WorkspaceModel`, and `NotificationService`.
2. Document the reason for `_qt_internal_*` calls in `docs/dev-setup.md`.
3. Long term, consider moving QML-exposed C++ types into a real `qt6_add_qml_module` target instead of synthetic metatype combining.

### 4. Medium: `ShellApplication` is becoming a composition root plus runtime controller

**Confidence:** 0.90

**Evidence:**
- `src/app/ShellApplication.cpp:84` constructs nearly every service/surface.
- `src/app/ShellApplication.cpp:117` registers QML singletons.
- `src/app/ShellApplication.cpp:158` starts services.
- `src/app/ShellApplication.cpp:296` owns the control socket.

**Why it matters:**
This is acceptable as a composition root, but adding more workflows will turn it into a hard-to-test god object.

**Suggestion:**
1. Keep construction in `ShellApplication`.
2. Extract small coordinators for control socket handling, QML registration, overlay closing, and widget rebuild logic.
3. Avoid adding feature business logic here; services should expose intent-level methods, and `ShellApplication` should wire them.

### 5. Medium: Config schema is centralized and will keep growing

**Confidence:** 0.87

**Evidence:**
- `src/core/ConfigService.h:14` contains many unrelated config structs.
- `src/core/ConfigService.h:177` owns parsing/watching/apply signals.
- `src/core/ConfigService.cpp` is about 958 lines.

**Why it matters:**
Every new feature touches the same file, increasing merge conflicts and making schema evolution risky.

**Suggestion:**
1. Split parsing by section: `AppearanceConfigParser`, `WeatherConfigParser`, `NotificationConfigParser`, `WidgetConfigParser`, etc.
2. Keep `ConfigService` as watcher/state publisher only.
3. Add table-driven parser tests per section.

### 6. Medium: Global/singleton access weakens otherwise good test seams

**Confidence:** 0.80

**Evidence:**
- `ConfigService::instance()` is declared at `src/core/ConfigService.h:189`.
- Launcher records recent apps through `RecentAppsTracker::instance()` in `src/services/launcher/LauncherService.cpp:215`.

**Why it matters:**
Hidden dependencies make tests order-sensitive and behavior harder to override.

**Suggestion:**
1. Prefer constructor injection for optional collaborators.
2. For launcher, pass a `RecentAppsTracker*` or small `RecentAppRecorder` interface.
3. Deprecate `ConfigService::instance()` and keep it only for legacy convenience.

### 7. Medium: Some service classes mix domain policy, model storage, timers, persistence, and UI-facing APIs

**Confidence:** 0.84

**Evidence:**
- `src/services/notifications/NotificationService.h:20` explicitly owns lifecycle, queue placement, timers, action routing, history, and QML model APIs.

**Why it matters:**
High cohesion today can become high complexity tomorrow. Notifications are already policy-heavy and likely to grow.

**Suggestion:**
1. Extract pure policy helpers: placement/queue algorithm, timeout policy, history grouping.
2. Keep `NotificationService` as the QObject/QAbstractListModel adapter.
3. Unit-test extracted helpers without a Qt event loop where possible.

### 8. Medium: Desktop launcher command handling is pragmatic but incomplete against `.desktop` Exec semantics

**Confidence:** 0.76

**Evidence:**
- Launcher strips field codes and uses `QProcess::splitCommand` at `src/services/launcher/LauncherService.cpp:32`.

**Why it matters:**
`.desktop` Exec has edge cases around quoting, field codes, terminal apps, environment wrappers, and URLs. This may fail for real-world desktop files.

**Suggestion:**
1. Add tests with quoted args, escaped spaces, `env FOO=bar app`, terminal apps, `%U/%F`, and invalid Exec lines.
2. Consider using GLib/GIO launch APIs later if correctness becomes more important than avoiding dependencies.

### 9. Medium: QML smoke tests are broad but visual/runtime coverage remains shallow

**Confidence:** 0.78

**Evidence:**
- Tests are split cleanly by target in `tests/CMakeLists.txt:18`.
- QML smoke coverage exists at `tests/CMakeLists.txt:95`.
- Many layer-shell, compositor, popup positioning, and visual behaviors still require runtime/manual checks.

**Why it matters:**
This project's highest-risk regressions are UI integration and Wayland behavior, not just model logic.

**Suggestion:**
1. Keep unit tests narrow.
2. Add a small compositor integration checklist or script for popup positioning, sidebar open/close, tray menu, notifications, and widgets.
3. For QML, add focused tests for required properties and component instantiation with representative fake services.

### 10. Low: CI is solid but dependency install is duplicated

**Confidence:** 0.95

**Evidence:**
- Package lists are repeated in `.github/workflows/ci.yml:19` and `.github/workflows/ci.yml:72`.

**Why it matters:**
Duplicated setup drifts over time.

**Suggestion:**
Move package setup into a reusable shell script or composite action. This reduces drift between build/test and static-check jobs.

### 11. Low: Build directories and local artifacts are visible in repo scans

**Confidence:** 0.70

**Evidence:**
- Local scan found `build/`, `build-ci-check/`, `build-private-fallback-check/`, `aqtinstall.log`, and `nvim.log`.

**Why it matters:**
They may be ignored, but they increase local noise and can obscure review output.

**Suggestion:**
Ensure all generated build dirs and logs are in `.gitignore`; consider a documented `task clean-artifacts` for local-only cleanup.

### 12. Low: Modern C++23 usage is good, but Qt idioms dominate

**Confidence:** 0.82

**Evidence:**
- Code uses defaulted comparisons, `std::optional`, `std::unique_ptr`, deleted copy/move operations, and target-level C++23 configuration.
- QObject ownership and Qt containers are used consistently.

**Why it matters:**
For this project, Qt idioms are usually the correct default. Forcing C++23 abstractions at Qt boundaries would add complexity without much benefit.

**Suggestion:**
Do not force more STL/C++23 abstractions where Qt is the natural boundary. Focus modern C++ improvements on pure helper code: `std::span`, `std::expected`-like result types, or ranges only where they simplify non-QObject logic.

## Positive Observations

- Target split is much better than a single monolithic executable.
- Tests are organized by architecture layer and link relatively narrow targets.
- Many services already have test seams: `NetworkManagerBackend`, D-Bus property client, launcher backend, and weather provider separation.
- CI runs build, CTest, qmllint, format-check, and clang-tidy.
- Documentation is strong: README, `CLAUDE.md`, SDD docs, release docs, and config docs are useful.

## Suggested Next Actions

1. Decide and enforce the `surfaces -> services` dependency rule.
2. Clean up `core` so it is either truly pure or honestly renamed/documented.
3. Add CI validation for generated QML type metadata.
4. Split `ConfigService` parsing by config section.
5. Extract notification placement/history policy into pure helpers.

## Verification

This was a static architecture/code review only. I did not run `task test`, `task tidy`, or `task qml-lint`.
