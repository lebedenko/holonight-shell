# SDD Tasks — arch-restructure-roadmap

**Revision:** v1.3 (2026-05-27) — fixed implementability gaps: `ExtWorkspaceManager` is core, `LayerShellManager` is surfaces, generated qmltypes content is checked explicitly, tests split into module executables for boundary proof, audio API tasks match SPEC, and binary-size validation strips a temporary copy.

Previous: v1.2 (2026-05-27) — closed v1.1 open items and red flags: binary-size baseline is **debug-build + stripped** (T-000 below); no runtime perf gate; constructor-discipline rule documented in DESIGN §2.3 (no new task); per-milestone lint enforcement folded into milestone acceptance criteria in SPEC (no new tasks).

Previous: v1.1 (2026-05-27) — applied REVIEW-FINDINGS resolutions: collapsed to five C++ static-lib targets, no `ServiceRegistry`, `ShellApplication` owns LayerShellManager via `startShell()`, no public accessors, M3 parallel with M1a.

**Parallel tracks:**
- M3 (README correction) runs in parallel with M1a.
- M5 (test audit) runs in parallel with M1–M3.

---

## Precondition

- [x] T-000: Record current binary size baseline before any milestone work begins
  - REQs: REQ-F-208 (5% binary-size acceptance)
  - Check: From a clean debug build on `main` (`task clean && task configure && task build`), run `cp build/holonight-shell build/holonight-shell.sizecheck && strip --strip-debug build/holonight-shell.sizecheck`, then `ls -l build/holonight-shell.sizecheck`. Record the byte count, the date, the current `main` commit SHA, and the host CPU/toolchain version into a new `BASELINE.md` file in this directory (or append to `REVIEW-FINDINGS.md`). Subsequent measurements at M2 acceptance MUST use the identical build flags, the same copy-and-strip invocation, and the same host. Release builds are not baselined (release size is not gated). Do not strip `build/holonight-shell` in place.

---

## Milestone 1 — ShellApplication extraction + src/ split

- [x] T-001: Create `src/app/` directory and `ShellApplication` skeleton
  - REQs: REQ-C-101, REQ-F-101, REQ-F-102, REQ-F-103, REQ-F-104
  - Check: `ShellApplication.h` declares ctor + `registerQmlTypes()` + `startServices()` + `startShell()` and the private member fields; `ShellApplication.cpp` has empty stub implementations; no `ServiceRegistry.h/cpp` is created; `task build` passes.

- [x] T-002: Move service construction logic from `main.cpp` to `ShellApplication` constructor
  - REQs: REQ-F-101
  - Check: `ShellApplication` constructor creates all 16 services in the same order as the original `main.cpp` (WorkspaceModel → ExtWorkspaceManager → HyprlandWorkspaceService → … → TrayWatcher); the constructor performs no dependency wiring or `.start()` calls; `task build` passes.

- [x] T-003: Implement `ShellApplication::registerQmlTypes()` (no parameter)
  - REQs: REQ-F-102, REQ-NF-101
  - Check: `registerQmlTypes()` calls `qmlRegisterSingletonType<T>("HolonightShell", 1, 0, ...)` for all 13 singletons (WorkspaceModel, ActiveWindowService, KeyboardLayoutService, BatteryService, AudioService, NetworkService, SessionService, SystemInfoService, ThemeService, PopupSurface, TooltipSurface, TrayMenuSurface, TrayModel); the function takes no `QQmlEngine*` parameter; a `registered_` guard makes second-call a no-op; `task qml-lint` passes; QML code resolves `import HolonightShell 1.0` types.

- [x] T-004: Implement `ShellApplication::startServices()` and `startShell()`
  - REQs: REQ-F-103, REQ-F-104
  - Check: `startServices()` first wires `tray_model_->setMenuSurface(tray_menu_surface_)`, then calls `.start()` on the seven async services in order; `startShell()` constructs `LayerShellManager(tray_model_, this)` via `std::unique_ptr`; both methods are idempotent (guarded by `services_started_` and `shell_started_` bools); `startShell()` asserts `registered_ && services_started_`; `task run` completes startup without hangs.

- [x] T-005: Reduce `main.cpp` to ≤ 12 lines
  - REQs: REQ-C-102
  - Check: `main.cpp` contains only `QGuiApplication app(argc, argv); ShellApplication shell(&app); shell.registerQmlTypes(); shell.startServices(); shell.startShell(); return QGuiApplication::exec();` plus include directives; line count excluding includes/blank lines/comments is ≤ 12; no `qmlRegisterSingletonType` or `LayerShellManager` symbols remain in `main.cpp`.

- [x] T-006: Create `src/platform/` and move Wayland/Hyprland/D-Bus transport files
  - REQs: REQ-C-103
  - Check: `LayerShell.h`, `LayerSurface.{h,cpp}`, `HyprlandIpc.{h,cpp}`, `HyprlandIpcClient.{h,cpp}`, `DbusPropertyClient.{h,cpp}` exist in `src/platform/`; no file in `src/platform/` includes `WorkspaceModel.h`, `TrayModel.h`, `IconImageProvider.h`, or any service/app header; `task build` passes after include path updates.

- [x] T-007: Create `src/core/` and move pure-logic domain model files
  - REQs: REQ-C-103
  - Check: `WorkspaceModel.{h,cpp}`, `ExtWorkspaceManager.{h,cpp}`, `HyprlandWorkspaceService.{h,cpp}`, `KeyboardLayoutService.{h,cpp}`, `BatteryState.{h,cpp}`, `AudioState.{h,cpp}`, `SystemInfo.{h,cpp}` exist in `src/core/`; `SystemInfo` is in `core/` not `services/` (it is pure logic, mirrors BatteryState/AudioState); all inter-file includes resolve correctly.

- [x] T-008: Create `src/services/` and move service implementations
  - REQs: REQ-C-103
  - Check: `ActiveWindowService.{h,cpp}`, `BatteryService.{h,cpp}`, `AudioService.{h,cpp}`, `NetworkService.{h,cpp}`, `SessionService.{h,cpp}`, `SystemInfoService.{h,cpp}`, `ThemeService.{h,cpp}` exist in `src/services/`; `SystemInfo.{h,cpp}` (the free-function utility) is **NOT** in `src/services/` (it lives in `src/core/`); `task build` succeeds.

- [x] T-009: Create `src/surfaces/` and move layer-shell surface + tray files
  - REQs: REQ-C-103
  - Check: `LayerShellManager.{h,cpp}`, `PopupSurface.{h,cpp}`, `TooltipSurface.{h,cpp}`, `TrayMenuSurface.{h,cpp}`, `TrayModel.{h,cpp}`, `TrayItem.{h,cpp}`, `TrayItemProperties.{h,cpp}`, `TrayWatcher.{h,cpp}`, `DbusMenuItem.{h,cpp}`, `DbusMenuClient.{h,cpp}`, `IconImageProvider.h` exist in `src/surfaces/`; `grep -r "#include" src/surfaces/ | grep -E "BatteryService|AudioService|NetworkService|ActiveWindowService|SessionService|SystemInfoService|ThemeService"` returns no results (confirms surfaces→services edge is unjustified, per REVIEW-FINDINGS C4); `task build` passes.

- [x] T-010: Update `CMakeLists.txt` include paths for M1 reorganization
  - REQs: REQ-C-103, REQ-NF-102
  - Check: `target_include_directories(holonight_core PUBLIC src/app src/core src/platform src/services src/surfaces ${CMAKE_CURRENT_BINARY_DIR})` covers all subdirs while still a single library; `task build` and `task test` pass; no `*.cpp`/`*.h` files remain in the `src/` root except `main.cpp`.

- [x] T-010a: Run lint/format checks at end of M1
  - REQs: REQ-NF-101 implicit
  - Check: `task format-check`, `task tidy`, `task qml-lint` all pass clean before opening the M1 PR.

---

## Milestone 2 — CMake target split (five C++ static libs)

- [x] T-011: Define `holonight_platform` STATIC library target
  - REQs: REQ-F-202, REQ-C-201, REQ-C-202
  - Check: `holonight_platform` contains all `src/platform/` files and only low-level platform files; `qt6_generate_wayland_protocol_client_sources()` is invoked exactly once (on this target); PUBLIC include `src/platform/` and `${CMAKE_CURRENT_BINARY_DIR}` so generated `qwayland-*.h` headers propagate; PUBLIC link Qt6::Core, Qt6::Gui, Qt6::DBus, Qt6::WaylandClient, wayland-client; it does not link `holonight_core`, `holonight_services`, `holonight_surfaces`, or `holonight_app`; `task build` succeeds.

- [x] T-012: Narrow `holonight_core` to `src/core/` files only
  - REQs: REQ-F-201, REQ-C-202
  - Check: `holonight_core` contains only `src/core/` files, including `ExtWorkspaceManager`; PUBLIC link Qt6::Core, **Qt6::Qml** (needed for `QML_ELEMENT` macro in WorkspaceModel.h), Qt6::DBus, tomlplusplus; PUBLIC link `holonight_platform`; compiles without errors; PUBLIC includes export `src/core/` only (not the whole `src/`).

- [x] T-013: Define `holonight_services` STATIC library target
  - REQs: REQ-F-204, REQ-C-202
  - Check: contains all `src/services/` files; PUBLIC link Qt6::Core, Qt6::Qml, Qt6::DBus, Qt6::Network, Qt6::Concurrent, libpulse; PUBLIC link `holonight_core`, `holonight_platform`; compiles without errors.

- [x] T-014: Define `holonight_surfaces` STATIC library target
  - REQs: REQ-F-205, REQ-C-202
  - Check: contains all `src/surfaces/` files, including `LayerShellManager`; PUBLIC link Qt6::Core, Qt6::Qml, Qt6::Gui, Qt6::Quick, Qt6::DBus; PRIVATE link Qt6::GuiPrivate; PUBLIC link `holonight_platform`; **does NOT link** `holonight_services` (per C4); CMake reports `holonight_surfaces` link line has only `holonight_platform` among internal deps.

- [x] T-014a: Split and re-link existing tests against new minimal target subsets
  - REQs: REQ-C-203
  - Check: `tests/CMakeLists.txt` no longer builds one omnibus `holonight_tests` executable. Each test file or small module group builds as its own executable and links the smallest set of new targets — `test_workspace_model`/`test_ext_workspace_manager`/`test_hyprland_workspace_service`/`test_audio_state`/`test_battery_state`/`test_system_info` → `holonight_core`; `test_active_window_service`/`test_audio_service`/`test_battery_service`/`test_network_service` → `holonight_services`; `test_tray_item`/`test_tray_item_properties`/`test_tray_model`/`test_tray_watcher`/`test_dbusmenu_client`/`test_layer_shell_manager` → `holonight_surfaces`; `test_hyprland_ipc`/`test_hyprland_ipc_client` → `holonight_platform`; `test_qml_smoke` → executable target or a dedicated QML smoke-test target that owns/imports the QML module. Verify by removing one target from one test executable's link line and confirming that executable fails to link, then restore.

- [x] T-015: Define `holonight_app` STATIC library and verify executable owns the QML module
  - REQs: REQ-F-206, REQ-F-207, REQ-F-208, REQ-NF-201, REQ-NF-202
  - Check: `holonight_app` contains only `ShellApplication.{h,cpp}`; PUBLIC link `holonight_core`, `holonight_platform`, `holonight_services`, `holonight_surfaces`, Qt6::Core, Qt6::Gui, Qt6::Qml; executable target `holonight-shell` is created with `src/main.cpp` only, links `holonight_app` PRIVATE, and continues to own `qt6_add_qml_module(holonight-shell URI HolonightShell VERSION 1.0 QML_FILES ${HOLONIGHT_QML_FILES})`; QML-facing C++ headers are passed to qmltyperegistrar without double-compiling `.cpp` files; **no `holonight_qml` static library exists anywhere in CMakeLists.txt**; `cmake --graphviz=deps.dot build/` produces an acyclic dependency graph; binary size within 5% of T-000 baseline — measured via `cp build/holonight-shell build/holonight-shell.sizecheck && strip --strip-debug build/holonight-shell.sizecheck && ls -l build/holonight-shell.sizecheck` on the same machine and same debug-build flags as T-000; `task qml-lint` passes and `build/HolonightShell/holonight-shell.qmltypes` contains WorkspaceModel, AudioService, TrayModel, and the other exported HolonightShell C++ types.

- [x] T-015a: Update `ENABLE_COVERAGE` instrumentation to cover all five static libs
  - REQs: REQ-C-204
  - Check: `target_compile_options(... PRIVATE --coverage)` and `target_link_options(... PRIVATE --coverage)` applied to `holonight_core`, `holonight_platform`, `holonight_services`, `holonight_surfaces`, `holonight_app`, and `holonight-shell`; `task coverage` generates HTML report covering all five libraries; no `.cpp` file under `src/` is excluded.

- [x] T-016: Run end-to-end M2 verification
  - REQs: REQ-NF-201, REQ-NF-202
  - Check: `task build`, `task qml-lint`, `task tidy`, `task format-check`, `task test` all pass clean; manual Wayland session smoke test confirms bar appears and all widgets function; `cmake --graphviz` graph is acyclic; generated `holonight-shell.qmltypes` is non-empty and lists the expected C++ singleton/type names.

---

## Milestone 3 — README correction (parallel with M1a)

- [x] T-017: Update README.md audio technology description at all occurrences
  - REQs: REQ-F-301, REQ-NF-301, REQ-C-301
  - Check: README.md line 17 changed from "`| Audio | Volume level + muted state via PulseAudio D-Bus |`" to "`| Audio | Volume level + muted state via PipeWire/PulseAudio-compatible control (libpulse) |`"; README.md ~line 79 updated to "...PipeWire or PulseAudio (via libpulse)..." for consistency; `libpulse` build-dependency bullet (~line 39) is unchanged; `git diff README.md` shows exactly the two intended modifications and no other content changes; PR can be merged independently of M1/M2.

---

## Milestone 4 — Audio module expansion

- [x] T-018: Create `AudioTypes.h` in `src/services/audio/`
  - REQs: REQ-F-405, REQ-C-401
  - Check: `src/services/audio/AudioTypes.h` defines `struct AudioDevice` and `struct AudioStream` with required fields; defines `enum class AudioDeviceType { Sink, Source }`; **no `pa_*` types appear in this public header**; `AudioState.{h,cpp}` is NOT moved (it remains in `src/core/`).

- [x] T-019: Implement `PulseAudioBackend` (libpulse encapsulation)
  - REQs: REQ-F-403, REQ-C-401
  - Check: `src/services/audio/PulseAudioBackend.{h,cpp}` exists; public header includes only `<QObject>`, `<cstdint>`, and `AudioTypes.h` — no `pa_*` types; public methods include `setDeviceVolume()`, `setDeviceMuted()`, `setDefaultOutput()`, `setDefaultInput()`, `setStreamVolume()`, `setStreamMuted()`, and `moveStreamToDevice()`; thread marshalling via `Qt::QueuedConnection` ensures signals arrive on the main thread; `task build` and `task tidy` pass.

- [x] T-020: Implement `AudioDeviceModel` (`QAbstractListModel`)
  - REQs: REQ-F-401, REQ-C-401
  - Check: `src/services/audio/AudioDeviceModel.{h,cpp}` exists; implements `rowCount()`, `data()`, `roleNames()`; exposes roles for `id`, `name`, `description`, `volume`, `muted`, `isDefault`; emits `dataChanged()` on updates.

- [x] T-021: Implement `AudioStreamModel` (`QAbstractListModel`)
  - REQs: REQ-F-402, REQ-C-401
  - Check: `src/services/audio/AudioStreamModel.{h,cpp}` exists; tracks playback (sink-input) and recording (source-output) streams; emits `dataChanged()` per stream row.

- [x] T-022: Extend `AudioService` with device/stream properties and invokables
  - REQs: REQ-F-404, REQ-F-406, REQ-NF-401, REQ-NF-402
  - Check: `AudioService` (now in `src/services/audio/`) has new Q_PROPERTYs `outputs`, `inputs`, `playbackStreams`, `recordingStreams`; new Q_INVOKABLEs `setDefaultOutput()`, `setDefaultInput()`, `setDeviceVolume()`, `setDeviceMuted()`, `setStreamVolume()`, `setStreamMuted()`, `moveStreamToOutput()`, and `moveStreamToInput()` exist; existing `volume`/`muted` API unchanged; existing QML `AudioWidget.qml` continues to function without changes; `AudioState.h` is **NOT** moved (stays in `src/core/`).

- [x] T-023: Write unit tests for audio models and `PulseAudioBackend`
  - REQs: REQ-F-401, REQ-F-402, REQ-F-403, REQ-F-404, REQ-NF-401, REQ-NF-402, REQ-C-501
  - Check: `tests/test_audio_device_model.cpp`, `tests/test_audio_stream_model.cpp`, `tests/test_pulse_audio_backend.cpp` exist; tests use GoogleMock to isolate logic from real libpulse; tests link `holonight_services`; `task test` passes with all audio tests succeeding.

---

## Milestone 5 — Test audit and gap fill (parallel with M1–M3)

- [x] T-024: Audit existing unit tests and document coverage gaps
  - REQs: REQ-F-501, REQ-F-502, REQ-NF-502
  - Check: Audit document lists all 16 existing test files and pure-logic methods with zero coverage; audit performed in parallel with M1–M3, does not block them.

- [x] T-025: Add tests for critical untested pure-logic code
  - REQs: REQ-F-503, REQ-NF-501, REQ-C-501
  - Check: At least one new test per critical gap identified in audit; each test covers >10 lines, >1 branch, zero prior coverage; `task test` passes 100% across 10 consecutive runs.
