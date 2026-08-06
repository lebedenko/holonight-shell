# Test Coverage Review - 2026-06-19

## Executive Summary

Current automated coverage is useful for core data models, pure parsing/mapping logic, notification behavior, launcher desktop-entry parsing/cache behavior, and several service state transitions. It is not yet strong enough to protect broad global refactoring across UI/surface orchestration, external backends, and runtime integration code.

Generated baseline:

- Command: `task coverage`
- HTML report: `build/coverage/index.html`
- JSON report generated for this review: `build/coverage/coverage.json`
- Line coverage: 42.1% (4099 / 9729)
- Function coverage: 44.6% (559 / 1254)
- Branch coverage: 25.0% (4368 / 17462)
- Coverage gate in `tests/CMakeLists.txt`: 35% line, 23% branch

`task coverage` completed successfully. The run skipped five `HyprlandIpcClient` socket tests because `QLocalServer::listen` failed in the current environment, and skipped `PulseAudioBackend.StartIsIdempotent`. Treat the platform/backend numbers below as slightly environment-dependent.

## Test Inventory

- C++ test files: 34
- CTest-discovered tests: 613
- QML tests: one broad smoke test suite, plus `qmllint`
- QML files: 95
- C++ source/header files under `src/`: 134

Strongly covered areas:

- `src/core/WorkspaceModel.cpp`: 82.28% line, 72.73% branch
- `src/core/ConfigService.cpp`: 86.67% line, 49.25% branch
- `src/platform/HyprlandIpc.cpp`: 93.8% line, 56.25% branch
- `src/services/weather-icon/WeatherIconMapper.cpp`: 99.28% line, 68.16% branch
- `src/services/weather-icon/MoonPhaseCalculator.cpp`: 100% line, 66.67% branch
- `src/surfaces/WidgetClock.cpp`: 94.34% line, 67.11% branch
- `src/surfaces/WidgetCountdown.cpp`: 100% line, 62.96% branch
- `src/surfaces/TrayItemProperties.cpp`: 85.04% line, 50% branch

## Coverage By Area

| Area | Lines covered | Line coverage | Branches covered | Branch coverage |
|---|---:|---:|---:|---:|
| `src/core` | 1096 / 1459 | 75.12% | 1164 / 2375 | 49.01% |
| `src/platform` | 365 / 539 | 67.72% | 373 / 1018 | 36.64% |
| `src/services` | 2161 / 4866 | 44.41% | 2016 / 8465 | 23.82% |
| `src/surfaces` | 477 / 2543 | 18.76% | 815 / 5100 | 15.98% |
| `src/app` | 0 / 322 | 0% | 0 / 504 | 0% |

## Most Critical Gaps Before Global Refactoring

### Critical: Surface and shell orchestration are mostly unprotected

Evidence:

| File | Line coverage | Branch coverage |
|---|---:|---:|
| `src/app/ShellApplication.cpp` | 0 / 322 | 0 / 504 |
| `src/surfaces/SidebarManager.cpp` | 0 / 265 | 0 / 510 |
| `src/surfaces/TrayMenuSurface.cpp` | 0 / 260 | 0 / 318 |
| `src/surfaces/WidgetManager.cpp` | 0 / 182 | 0 / 284 |
| `src/surfaces/StatusPopupSurface.cpp` | 0 / 167 | 0 / 196 |
| `src/surfaces/LauncherSurface.cpp` | 0 / 121 | 0 / 256 |
| `src/surfaces/TooltipSurface.cpp` | 0 / 103 | 0 / 134 |
| `src/surfaces/NotificationToastSurface.cpp` | 0 / 89 | 0 / 116 |
| `src/surfaces/PopupSurface.cpp` | 0 / 81 | 0 / 82 |

Why this matters:

This is the code most likely to regress during global refactoring because it wires lifetimes, QML engines, singleton registration, layer-shell surfaces, popup dismissal, monitor routing, and cross-service coordination. Current QML smoke coverage proves some components load, but it does not validate most shell orchestration behavior.

Recommended coverage increase:

1. Add narrow lifecycle tests around manager classes using fake/minimal collaborators where possible.
2. Extract pure routing decisions from surface classes into small testable helpers only where it removes real coupling.
3. Add QML smoke variants for launcher, sidebar, popup, and toast surfaces with fake singleton states.
4. Add a small integration test for `ShellApplication::registerQmlTypes()` / `startServices()` invariants if a full Wayland session is not required.

### Critical: External backend/service adapters have little or no coverage

Evidence:

| File | Line coverage | Branch coverage |
|---|---:|---:|
| `src/services/network/NetworkManagerBackend.cpp` | 0 / 431 | 0 / 1043 |
| `src/services/weather/WeatherService.cpp` | 0 / 398 | 0 / 816 |
| `src/services/weather/WeatherProvider.cpp` | 0 / 226 | 0 / 608 |
| `src/services/audio/PulseAudioBackend.cpp` | 34 / 369 | 20 / 441 |
| `src/platform/DbusPropertyClient.cpp` | 0 / 50 | 0 / 172 |
| `src/services/SystemInfoService.cpp` | 0 / 35 | 0 / 104 |

Why this matters:

These classes touch D-Bus, NetworkManager, PulseAudio, HTTP/weather APIs, timers, and external state. Refactors that alter constructor wiring, signal handling, retry behavior, request parsing, or error paths can break runtime behavior while tests remain green.

Recommended coverage increase:

1. Add fake backend/client seams for weather provider responses, NetworkManager property snapshots, and PulseAudio event streams.
2. Prioritize tests for error recovery, malformed external payloads, reconnect/retry paths, and property-change signals.
3. Keep true integration tests optional/manual, but cover parsing/state transitions with deterministic fakes.

### High: Surfaces area has many tests, but coverage is concentrated in pure model helpers

Evidence:

- `src/surfaces` overall: 18.76% line, 15.98% branch.
- Strong files: `TrayModel`, `TrayItemProperties`, `WidgetClock`, `WidgetCountdown`.
- Weak files: managers, popup surfaces, menu surfaces, notification/launcher/sidebar surfaces.

Why this matters:

The current surface test suite protects leaf logic well but not the orchestration layer where refactoring usually changes ownership, signal connections, object lifetime, and QML context setup.

Recommended coverage increase:

1. Before moving or renaming surface classes, add tests for expected public side effects: show/hide behavior, selected monitor routing, menu/popup replacement, and cleanup.
2. Keep tests at public-method/signal level rather than asserting private implementation details.

### High: Branch coverage remains low in active runtime services

Evidence:

| File | Line coverage | Branch coverage |
|---|---:|---:|
| `src/services/ActiveWindowService.cpp` | 25.71% | 18.18% |
| `src/services/audio/AudioService.cpp` | 38.41% | 21.53% |
| `src/platform/HyprlandIpcClient.cpp` | 56.32% | 26.73% |
| `src/surfaces/TrayWatcher.cpp` | 12.34% | 4.0% |
| `src/surfaces/DbusMenuClient.cpp` | 8.96% | 4.55% |
| `src/core/ExtWorkspaceManager.cpp` | 13.54% | 7.32% |

Why this matters:

These components are event-driven. Low branch coverage means many ordering, missing-data, stale-signal, and failure paths are untested. Refactoring event routing or signal connection code is risky without more scenario coverage.

Recommended coverage increase:

1. Add event-sequence tests for active window/workspace transitions and tray watcher registration/unregistration lifecycles.
2. Stabilize the skipped `HyprlandIpcClient` local-socket tests so they reliably run in CI/dev environments.
3. Add D-Bus menu update-path tests beyond static parsing.

### Medium: Launcher tests are useful but still miss ranking/filter edge cases

Evidence:

- `LauncherService.cpp`: 58.9% line, 35.04% branch.
- `LauncherModel.cpp`: 49.24% line, 34.63% branch.
- `RecentAppsTracker.cpp`: 65.38% line, 28.3% branch.
- `DesktopEntryScanner.cpp` and `DesktopEntryCache.cpp` are comparatively healthy.

Recommended coverage increase:

1. Add `LauncherModel` tests for browse category filtering, action-only matches, section ordering, article-stripped sort order, and empty/invalid active categories.
2. Add `RecentAppsTracker` tests for max-entry eviction, duplicate launch update order, invalid persisted JSON, and unavailable desktop-file pruning.
3. Add cache validation tests for modified file metadata and removed desktop files.

### Medium: QML coverage is shallow for a UI-heavy shell

Evidence:

- One QML smoke test loads topbar/tray/status components with fake services.
- `task qml-lint` is clean, but linting does not validate behavior or visual states.
- QML files are not represented in C++ gcov line coverage.

Recommended coverage increase:

1. Add smoke/component tests for launcher, right sidebar, network/audio/battery popups, notification toasts, and widget surfaces.
2. For refactor safety, focus on loading components with fake singleton state and verifying no missing properties/methods after API moves.
3. Keep visual pixel tests/manual Wayland checks for high-risk UI changes, not for every small refactor.

## Zero-Coverage Files With Significant Size

These are the largest files with at least 20 executable lines and 0 covered lines:

| File | Executable lines | Branches |
|---|---:|---:|
| `src/services/network/NetworkManagerBackend.cpp` | 431 | 1043 |
| `src/services/weather/WeatherService.cpp` | 398 | 816 |
| `src/app/ShellApplication.cpp` | 322 | 504 |
| `src/surfaces/SidebarManager.cpp` | 265 | 510 |
| `src/surfaces/TrayMenuSurface.cpp` | 260 | 318 |
| `src/services/weather/WeatherProvider.cpp` | 226 | 608 |
| `src/surfaces/WidgetManager.cpp` | 182 | 284 |
| `src/surfaces/StatusPopupSurface.cpp` | 167 | 196 |
| `src/surfaces/LauncherSurface.cpp` | 121 | 256 |
| `src/surfaces/TooltipSurface.cpp` | 103 | 134 |
| `src/surfaces/NotificationToastSurface.cpp` | 89 | 116 |
| `src/surfaces/PopupSurface.cpp` | 81 | 82 |
| `src/surfaces/PerMonitorLayerManager.cpp` | 58 | 90 |
| `src/platform/DbusPropertyClient.cpp` | 50 | 172 |
| `src/services/weather/WeatherData.h` | 50 | 118 |
| `src/services/weather-icon/WeatherIconBridge.cpp` | 49 | 49 |
| `src/surfaces/IconImageProvider.h` | 46 | 110 |
| `src/services/SystemInfoService.cpp` | 35 | 104 |
| `src/surfaces/BackgroundManager.cpp` | 31 | 28 |
| `src/surfaces/NotificationManager.cpp` | 26 | 24 |
| `src/services/MonitorOccupancyService.cpp` | 22 | 30 |

## Suggested Pre-Refactor Test Plan

1. Stabilize skipped tests.
   - Fix or isolate the `HyprlandIpcClient` `QLocalServer::listen` skip condition.
   - Decide whether `PulseAudioBackend.StartIsIdempotent` should be deterministic or explicitly excluded from coverage expectations.

2. Protect public API contracts before moving code.
   - Add tests around QML singleton invokables and properties used by QML.
   - Add smoke tests for launcher/sidebar/popup QML components before renaming services or changing registrations.

3. Add tests for event-driven managers.
   - Focus on `ShellApplication`, surface managers, popup dismissal/routing, tray watcher lifecycle, and active-window monitor routing.
   - Prefer fake collaborators and signal assertions over Wayland-dependent tests.

4. Add backend parser/state tests.
   - NetworkManager, weather, PulseAudio, and D-Bus property code should get deterministic fake-input coverage for success and error paths.

5. Raise coverage gates only after targeted gaps are addressed.
   - Current gates are intentionally low: 35% line, 23% branch.
   - A practical next ratchet after the critical gaps: 45% line and 30% branch.
   - Avoid chasing aggregate coverage by testing trivial glue while runtime-critical managers remain uncovered.

## Recommended Refactor Order

Safest first:

1. Pure logic with strong tests: config parsing, workspace model, battery/audio state models, weather icon mapping, moon phase, widget formatting.
2. Service logic with moderate tests: launcher scanner/cache/service, notification service/store, network service state builders.
3. Event-driven services with partial tests: active window, Hyprland IPC client, tray watcher, D-Bus menu client.
4. Highest risk: shell application wiring, surface managers, layer-shell surfaces, popup/menu/toast surfaces, external live backends.

## Not Covered By This Review

- Manual Wayland/Hyprland behavior.
- Visual correctness and pixel-level QML rendering.
- Performance, memory, and leak analysis.
- Security review of D-Bus or process-launching behavior.
- Generated build artifacts, generated Wayland protocol files, and resource cache output.
