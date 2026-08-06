# Architecture Restructuring: Implementation Roadmap

**Project:** holonight-shell (C++23/Qt6 Wayland shell)  
**Version:** 1.3  
**Date:** 2026-05-27  
**Status:** Specification  

---

## Overview

This specification defines an implementation roadmap to restructure the holonight-shell codebase from a flat architecture into a modular, domain-driven organization. The restructuring is staged across five milestones, with clear acceptance criteria and no breaking changes to the public QML API or runtime binary.

**Revision history:**
- 2026-05-27 v1.0 — initial specification
- 2026-05-27 v1.1 — applied REVIEW-FINDINGS resolutions: collapsed to five C++ static-lib targets (no `holonight_qml`); reworked `ShellApplication` API (no accessors, owns LayerShellManager); fixed singleton count; moved `SystemInfo` to core; dropped `ServiceRegistry`; re-sequenced M3 to parallel with M1a; added requirements for test re-link and coverage instrumentation
- 2026-05-27 v1.2 — refined v1.1 open items: pinned binary-size baseline to debug-build-stripped; explicitly excluded a runtime perf gate; added lint acceptance criteria to M4 and M5; documented the constructor-discipline rule (60 LOC soft ceiling, construction-only) in DESIGN §2.3; added `platform/` nesting escape-hatch note in DESIGN §2.1; clarified that `surfaces/` houses tray plumbing alongside true layer-shell surfaces
- 2026-05-27 v1.3 — fixed implementability gaps: moved `ExtWorkspaceManager` to core and `LayerShellManager` to surfaces to avoid target cycles; strengthened generated-qmltypes validation; clarified split-test executable requirement; aligned audio APIs across backend/service/tasks; made binary-size checks strip a temporary copy only

**Core Principles:**
- One binary (`holonight-shell`) throughout all milestones
- Clear internal module boundaries via directory structure and CMake targets
- QML imports remain unchanged (`HolonightShell` module, `Holonight` theme)
- The `qt6_add_qml_module` invocation stays on the executable target, and generated qmltypes must list every exported C++ singleton
- All existing unit tests must pass
- Audio module expansion must complete before any pavucontrol-like popup feature work
- M3 (README correction) runs in parallel with M1a; M5 (test audit) runs in parallel with M1–M3

---

## Milestone 1: ShellApplication Extraction + src/ Domain Split

### Purpose
Move application initialization and service registration logic out of `main.cpp` into a dedicated `ShellApplication` class. Simultaneously restructure `src/` from a flat folder into domain-organized subdirectories.

### Requirements

#### REQ-F-101: ShellApplication constructor shall initialize all services
**Statement:** When `ShellApplication` is instantiated, the system shall construct and initialize all service instances.

**Acceptance criteria:**
- `ShellApplication` constructor accepts a `QObject* parent` and parents every owned service to `this`
- All 16 services are instantiated before the constructor returns: WorkspaceModel, ExtWorkspaceManager, HyprlandWorkspaceService, KeyboardLayoutService, ActiveWindowService, BatteryService, AudioService, NetworkService, SessionService, SystemInfoService, ThemeService, PopupSurface, TooltipSurface, TrayMenuSurface, TrayModel, TrayWatcher
- Of these 16, **13 are registered as QML singletons** by REQ-F-102; the remaining three (`ExtWorkspaceManager`, `HyprlandWorkspaceService`, `TrayWatcher`) are consumed only by C++ code and are not exposed to QML
- No QML registration occurs in the constructor itself (registration happens in `registerQmlTypes()`)
- Service construction order matches the original `main.cpp`: WorkspaceModel → ExtWorkspaceManager → HyprlandWorkspaceService → KeyboardLayoutService → … → TrayWatcher. Non-construction wiring, including `tray_model_->setMenuSurface(tray_menu_surface_)`, happens in `startServices()` before any `.start()` call.

#### REQ-F-102: ShellApplication shall register all QML singleton types
**Statement:** The `ShellApplication` class shall provide a public method to register all service instances as QML singleton types via the global Qt QML type registry.

**Acceptance criteria:**
- A public method named `registerQmlTypes()` exists with no parameters (the underlying `qmlRegisterSingletonType` API is a global registry, not engine-scoped — see DESIGN §4.2)
- All 13 singletons (WorkspaceModel, ActiveWindowService, KeyboardLayoutService, BatteryService, AudioService, NetworkService, SessionService, SystemInfoService, ThemeService, PopupSurface, TooltipSurface, TrayMenuSurface, TrayModel) are registered
- Each singleton is registered with the HolonightShell module (version 1.0) using `QQmlEngine::CppOwnership`
- The method is idempotent: a private `registered_` bool guards against re-registration, so calling it twice is a silent no-op (the underlying Qt API would log warnings)
- The method must be called before the first `QQuickView` is created (which happens in `startShell()`); ShellApplication asserts this ordering in `startShell()`

#### REQ-F-103: ShellApplication shall start all async services
**Statement:** The `ShellApplication` class shall provide a public method that starts all services requiring async initialization in the correct order.

**Acceptance criteria:**
- A public method named `startServices()` exists and is idempotent (guarded by `services_started_` bool)
- Before starting async services, `startServices()` wires constructed objects that depend on each other, including `tray_model_->setMenuSurface(tray_menu_surface_)`
- Services are started in order: HyprlandWorkspaceService, KeyboardLayoutService, ActiveWindowService, BatteryService, AudioService, NetworkService, TrayWatcher (seven of the sixteen services; the remaining nine are either command-only or use CONSTANT properties)
- The method returns after all `start()` calls complete (each is synchronous; async work happens later on the event loop)
- Calling startServices() twice does not crash or double-initialize services

#### REQ-F-104: ShellApplication shall own and construct LayerShellManager
**Statement:** The `ShellApplication` class shall provide a public method that constructs and owns the `LayerShellManager` instance.

**Rationale:** `LayerShellManager` takes a non-const `TrayModel*` and is constructed once after services have started. Owning it inside `ShellApplication` keeps `main.cpp` minimal and avoids exposing mutable accessors. `ShellApplication` has **no public service accessors** — services are owned privately. Tests instantiate services directly without going through `ShellApplication`.

**Acceptance criteria:**
- A public method named `startShell()` exists and is idempotent
- `startShell()` constructs a `LayerShellManager` (held via `std::unique_ptr` or Qt parent chain) using the privately-owned `TrayModel*`
- `startShell()` asserts `registered_ == true` and `services_started_ == true` to enforce call ordering
- `ShellApplication.h` declares zero public service accessors (no `audioService()`, no `trayModel()`, etc.); all services are private members

#### REQ-C-101: ShellApplication shall live in src/app/ directory
**Statement:** The `ShellApplication` class implementation shall reside in `src/app/ShellApplication.h` and `src/app/ShellApplication.cpp`.

**Acceptance criteria:**
- Files exist at the specified paths
- No other .cpp/.h files live in src/app/ initially (no `ServiceRegistry` — the 13 registration lambdas live inline inside `ShellApplication::registerQmlTypes()`)
- ShellApplication.h is included only by main.cpp and CMakeLists.txt

#### REQ-C-102: main.cpp shall be reduced to entry point boilerplate
**Statement:** After extraction, `src/main.cpp` shall contain no more than 12 lines of executable code.

**Acceptance criteria:**
- Line count in main.cpp (excluding comments, blank lines, and `#include` directives) is ≤ 12
- main.cpp only constructs `QGuiApplication`, instantiates `ShellApplication`, calls `registerQmlTypes()`, `startServices()`, `startShell()`, and returns from `QGuiApplication::exec()`
- All service construction, QML registration, and `LayerShellManager` setup is delegated to `ShellApplication`

#### REQ-C-103: src/ shall be split into five domain directories
**Statement:** All C++ source files except `main.cpp` shall be reorganized into `src/app/`, `src/core/`, `src/platform/`, `src/services/`, and `src/surfaces/` according to domain boundaries. `src/qml/` remains unchanged (QML files continue to be managed by `qt6_add_qml_module` on the executable target — see REQ-F-206).

**Acceptance criteria:**
- `src/app/` contains: ShellApplication.h/cpp (no other files)
- `src/core/` contains: WorkspaceModel, ExtWorkspaceManager, HyprlandWorkspaceService, KeyboardLayoutService, BatteryState, AudioState, SystemInfo (the free-function utility, *not* SystemInfoService)
- `src/platform/` contains only low-level platform adapters that do not include core, services, surfaces, or app headers: LayerShell, LayerSurface, HyprlandIpc, HyprlandIpcClient, DbusPropertyClient
- `src/services/` contains: ActiveWindowService, BatteryService, AudioService, NetworkService, SessionService, SystemInfoService, ThemeService
- `src/surfaces/` contains: LayerShellManager, PopupSurface, TooltipSurface, TrayMenuSurface, TrayModel, TrayItem, TrayItemProperties, TrayWatcher, DbusMenuItem, DbusMenuClient, IconImageProvider
- `src/qml/` remains unchanged
- `main.cpp` is the only .cpp/.h file in the root `src/` directory; it is compiled directly into the `holonight-shell` executable, not into any static library

#### REQ-NF-101: Directory restructure shall not affect QML import paths
**Statement:** After src/ is split into subdirectories, QML imports shall resolve identically to pre-restructure behavior.

**Acceptance criteria:**
- `import HolonightShell 1.0` statements in all .qml files work without modification
- `import Holonight` (theme — externally-installed module, see "External dependencies" below) statements continue to work
- No .qml files are moved or renamed
- QML type registration code path is unchanged: `qt6_add_qml_module(holonight-shell URI HolonightShell VERSION 1.0 ...)` remains on the executable target
- `qmltyperegistrar` sees all C++ headers carrying `QML_ELEMENT`/`QML_SINGLETON` — this is achieved by passing those headers to `qt6_add_qml_module(... SOURCES ...)` on the executable target or by another explicitly verified CMake mechanism that avoids double-compiling implementation files
- Generated `build/HolonightShell/holonight-shell.qmltypes` is not empty and contains all exported singleton/type names: WorkspaceModel, ActiveWindowService, KeyboardLayoutService, BatteryService, AudioService, NetworkService, SessionService, SystemInfoService, ThemeService, PopupSurface, TooltipSurface, TrayMenuSurface, TrayModel, and DbusMenuItem
- `task qml-lint` clean is required but is not sufficient by itself; the qmltypes content check is the boundary test for static QML metadata

**External dependencies note:** The `Holonight` palette module is installed outside this repo (typically under `$HOME/.local/lib/qt6/qml/Holonight/`). The restructure does not touch it. The qmllint shim at `tests/qmllint/Holonight/` is a stub for static analysis only.

#### REQ-NF-102: No circular includes between domain modules
**Statement:** The system shall prevent circular include dependencies between the five domain modules.

**Acceptance criteria:**
- `src/services/` does not include any header from `src/surfaces/` or `src/app/`
- `src/surfaces/` does not include any header from `src/services/` or `src/app/` (verified by `grep -r "BatteryService\|AudioService\|NetworkService" src/surfaces/` returning no results)
- `src/app/` includes from `src/app/`, `src/services/`, `src/surfaces/`, `src/platform/`, `src/core/` only
- `src/platform/` includes no headers from `src/core/`, `src/services/`, `src/surfaces/`, or `src/app/`
- Running `cmake` without warnings about cyclic dependencies

---

## Milestone 2: CMake Target Split

### Purpose
Split the monolithic `holonight_core` target into **five** logical CMake static-library targets to make internal boundaries explicit and improve build clarity. All targets remain linked into the single `holonight-shell` binary. The QML module continues to be defined on the executable target via `qt6_add_qml_module` — there is no separate `holonight_qml` target (this was considered and rejected; see DESIGN §8).

### Requirements

#### REQ-F-201: holonight_core target shall exist and contain core domain models only
**Statement:** The `holonight_core` target is **narrowed** (not expanded into an aggregate) to contain only `src/core/` files.

**Acceptance criteria:**
- `holonight_core` is a STATIC library
- Sources: WorkspaceModel, ExtWorkspaceManager, HyprlandWorkspaceService, KeyboardLayoutService, BatteryState, AudioState, SystemInfo
- PUBLIC link: Qt6::Core, Qt6::Qml (needed for `QML_ELEMENT`/`QML_SINGLETON` macros in WorkspaceModel.h), Qt6::DBus, tomlplusplus
- PUBLIC link: holonight_platform (so `HyprlandIpcTransport` and `DbusPropertyClient` interfaces propagate to consumers)
- No `qt6_add_qml_module` invocation on this target
- Compiles cleanly

#### REQ-F-202: holonight_platform target shall contain low-level Wayland, Hyprland, and D-Bus integrations
**Statement:** A new `holonight_platform` CMake target shall be created for low-level layer-shell protocol wrappers, Wayland protocol bindings, Hyprland IPC transport, and the D-Bus property helper. It shall not contain classes that know about application models or UI composition.

**Acceptance criteria:**
- Target is a STATIC library
- Contains: LayerShell, LayerSurface, HyprlandIpc, HyprlandIpcClient, DbusPropertyClient
- Does not contain `ExtWorkspaceManager` because it mutates `WorkspaceModel`; `ExtWorkspaceManager` belongs to `holonight_core`
- Does not contain `LayerShellManager` because it creates `QQuickView` instances and consumes `TrayModel`/image providers; `LayerShellManager` belongs to `holonight_surfaces`
- Wayland protocol files are generated into this target via `qt6_generate_wayland_protocol_client_sources()` (see REQ-C-201)
- PUBLIC link: Qt6::Core, Qt6::Gui, Qt6::DBus, Qt6::WaylandClient, wayland-client
- PUBLIC include: `${CMAKE_CURRENT_BINARY_DIR}` so generated `qwayland-*.h` headers propagate to consumers
- Compiles without warnings

#### REQ-F-204: holonight_services target shall contain service implementations
**Statement:** A new `holonight_services` CMake target shall be created for service implementations.

**Acceptance criteria:**
- Target is a STATIC library
- Contains: ActiveWindowService, BatteryService, AudioService, NetworkService, SessionService, SystemInfoService, ThemeService
- PUBLIC link: Qt6::Core, Qt6::Qml, Qt6::DBus, Qt6::Network, Qt6::Concurrent, libpulse
- PUBLIC link: holonight_core, holonight_platform
- After M4, `src/services/audio/` (AudioService, PulseAudioBackend, AudioDeviceModel, AudioStreamModel, AudioTypes) is part of this target
- Compiles cleanly

#### REQ-F-205: holonight_surfaces target shall contain bar, surface, and tray implementations
**Statement:** A new `holonight_surfaces` CMake target shall be created for the topbar `LayerShellManager`, popup, tooltip, tray-menu surfaces, and the tray model/watcher.

**Acceptance criteria:**
- Target is a STATIC library
- Contains: LayerShellManager, PopupSurface, TooltipSurface, TrayMenuSurface, TrayModel, TrayItem, TrayItemProperties, TrayWatcher, DbusMenuItem, DbusMenuClient, IconImageProvider
- PUBLIC link: Qt6::Core, Qt6::Qml, Qt6::Gui, Qt6::Quick, Qt6::DBus
- PUBLIC link: holonight_platform
- **Does NOT link** holonight_services (verified by `grep -r '#include' src/surfaces/` returning no service-header includes)
- PRIVATE link: Qt6::GuiPrivate
- Compiles cleanly

#### REQ-F-206: QML module shall remain on the executable target
**Statement:** The `qt6_add_qml_module(holonight-shell URI HolonightShell VERSION 1.0 ...)` invocation shall remain on the executable target. There is no `holonight_qml` static library.

**Rationale:** Every singleton service header carries `QML_ELEMENT` + `QML_SINGLETON`. The qmltyperegistrar tool must scan those headers to emit `holonight-shell.qmltypes`, which is what makes `import HolonightShell 1.0` resolve in qmllint. Moving the QML module to a separate static library would require static QML plugin machinery (`Q_IMPORT_QML_PLUGIN`, `qt_import_qml_plugins`) that is fragile across Qt 6.x versions. Five C++ static-library targets + executable-owned QML module is the chosen end state.

**Acceptance criteria:**
- `qt6_add_qml_module(holonight-shell ...)` stays on the executable target as-is
- All headers carrying `QML_ELEMENT`/`QML_SINGLETON` are visible to the executable-owned qmltyperegistrar pass without double-compiling `.cpp` files
- `build/HolonightShell/holonight-shell.qmltypes` contains the exported HolonightShell C++ types listed in REQ-NF-101; `Module {}` alone is a failure even if runtime singleton registration still works
- `task qml-lint` continues to pass clean
- No `holonight_qml` target is defined anywhere in CMakeLists.txt
- Non-QML assets (`assets/bar-icons/*.svg`, etc.) are bundled via a separate `qt6_add_resources(holonight-shell ...)` call on the executable

#### REQ-F-207: holonight_app target shall contain ShellApplication only
**Statement:** A new `holonight_app` CMake target shall be created for `ShellApplication`.

**Acceptance criteria:**
- Target is a STATIC library
- Sources: ShellApplication.h, ShellApplication.cpp (no `ServiceRegistry`, no `main.cpp`)
- PUBLIC link: holonight_core, holonight_platform, holonight_services, holonight_surfaces
- PUBLIC link: Qt6::Core, Qt6::Gui, Qt6::Qml
- Compiles cleanly

#### REQ-F-208: holonight-shell executable shall link holonight_app and own the QML module
**Statement:** The `holonight-shell` executable shall be created via `qt6_add_executable()`, link `holonight_app` (which transitively pulls all other static libraries), and own the QML module declaration.

**Acceptance criteria:**
- Executable target sources: only `src/main.cpp`
- `qt6_add_qml_module(holonight-shell URI HolonightShell VERSION 1.0 QML_FILES ${HOLONIGHT_QML_FILES})` invoked on the executable (preserves current behavior)
- PRIVATE link: `holonight_app` (CMake resolves transitive dependencies automatically; do not hard-code an explicit link order)
- Optional separate `qt6_add_resources(holonight-shell "assets" PREFIX "/HolonightShell" BASE ".../assets" FILES ...)` for non-QML asset bundles
- Linking produces exactly one binary: `holonight-shell`
- Binary size is within 5% of pre-refactor baseline. Measurement convention: **debug build, temporary copy of the executable stripped via `strip --strip-debug`**. Baseline recorded in T-000 before M1 starts using the same build configuration and same stripping command. Release-build size is not gated.

#### REQ-NF-201: CMake targets shall have no circular dependencies
**Statement:** The system shall prevent circular link-time dependencies between the five CMake static-library targets.

**Acceptance criteria:**
- CMake configure step completes without cycle detection warnings
- Each target lists dependencies in a single direction (no A->B and B->A)
- Verified by static analysis (e.g., `cmake --graphviz` graph is acyclic)

#### REQ-NF-202: CMake target split shall not affect runtime behavior
**Statement:** After target split, the `holonight-shell` executable shall produce identical runtime output and feature behavior to the pre-refactor binary.

**Acceptance criteria:**
- Binary runs without crashes when launched in a Wayland session
- All service connections (D-Bus, Hyprland IPC) succeed identically
- QML singletons resolve with identical properties and methods
- No functionality is lost or degraded

#### REQ-C-201: Wayland protocol compilation shall occur in holonight_platform only
**Statement:** The `qt6_generate_wayland_protocol_client_sources()` CMake directive shall be invoked only in the `holonight_platform` target definition.

**Acceptance criteria:**
- Wayland protocol files (wlr-layer-shell, ext-workspace, xdg-shell) are compiled exactly once
- Generated header and source files are available to all dependent targets
- No duplicate protocol compilation occurs

#### REQ-C-202: PUBLIC include paths per target
**Statement:** Each CMake target shall export only its own subdirectory as a PUBLIC include path. Dependents pick up other includes transitively through the dependency graph.

**Acceptance criteria:**
- `holonight_core`: PUBLIC includes `src/core/`
- `holonight_platform`: PUBLIC includes `src/platform/` and `${CMAKE_CURRENT_BINARY_DIR}` (for generated `qwayland-*.h`)
- `holonight_services`: PUBLIC includes `src/services/`
- `holonight_surfaces`: PUBLIC includes `src/surfaces/`
- `holonight_app`: PUBLIC includes `src/app/`
- No target makes its entire `src/` subdirectory PUBLIC; specifically `target_include_directories(holonight_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)` (the current state) is removed in M2

#### REQ-C-203: Existing tests shall be re-linked against the new targets
**Statement:** After the M2 target split, tests shall be split into module-level executables so each executable links only the minimum set of static-library targets required to compile and run.

**Acceptance criteria:**
- `tests/CMakeLists.txt` no longer builds one `holonight_tests` executable that links every test against the same internal target set
- Each test file or small module group is compiled into its own executable and links the smallest set of new targets needed (e.g., `test_audio_service` → `holonight_services`; `test_workspace_model` → `holonight_core`; `test_tray_model` → `holonight_surfaces`)
- `task test` (after `task configure-tests`) passes 100%
- Sanity check: temporarily removing one target from a test's link line causes that test to fail to link — proves the boundary is real, not transitively pulled

#### REQ-C-204: Coverage instrumentation shall cover all C++ targets
**Statement:** When `ENABLE_COVERAGE=ON`, gcov instrumentation shall be applied to every C++ static library and to the executable.

**Acceptance criteria:**
- `target_compile_options(... PRIVATE --coverage)` and `target_link_options(... PRIVATE --coverage)` are applied to `holonight_core`, `holonight_platform`, `holonight_services`, `holonight_surfaces`, `holonight_app`, and `holonight-shell`
- `task coverage` produces an HTML report covering all five static libraries
- No `.cpp` file under `src/` is excluded from instrumentation when `ENABLE_COVERAGE=ON`

---

## Milestone 3: README Correction

### Purpose
Update the README.md to accurately reflect the audio technology stack used in the implementation. **Sequencing:** M3 runs in parallel with M1a — it is a single-line documentation fix with zero dependencies on the C++ refactor, so it lands as soon as a reviewer approves it. It does not block, and is not blocked by, M1/M2/M4.

### Requirements

#### REQ-F-301: README audio description shall be corrected at all occurrences
**Statement:** Every occurrence in `README.md` describing the audio technology stack shall use accurate terminology.

**Acceptance criteria:**
- The bullet at README.md line 17 ("`| Audio | Volume level + muted state via PulseAudio D-Bus |`") is replaced with `| Audio | Volume level + muted state via PipeWire/PulseAudio-compatible control (libpulse) |`
- The runtime-requirements paragraph (README.md ~line 79, "...PulseAudio...") is reviewed and updated to "...PipeWire or PulseAudio (via libpulse)..." for consistency
- The build-dependency bullet `libpulse` (~line 39) stays as-is — it correctly names the library
- No other README sections are modified by this milestone

#### REQ-NF-301: Audio description shall be accurate to implementation
**Statement:** The audio description in README.md shall match the actual technology used in the codebase.

**Acceptance criteria:**
- AudioService implementation uses libpulse (confirmed in CMakeLists.txt: `pkg_check_modules(LIBPULSE ...)`)
- libpulse provides a compatible API regardless of whether the system uses PipeWire or PulseAudio backend
- No other audio library (e.g., ALSA, Jack) is used in the current implementation

#### REQ-C-301: README update shall not change other sections
**Statement:** The audio description update shall not affect any other README content.

**Acceptance criteria:**
- Only the two audio-related lines identified in REQ-F-301 are modified
- Feature list order, project purpose, build instructions remain unchanged
- No other technology descriptions are altered

---

## Milestone 4: Audio Module Expansion

### Purpose
Restructure the audio service from a simple volume widget backend into a rich domain model suitable for advanced audio control UIs (pavucontrol-like popup). This milestone must complete before any pavucontrol popup feature implementation begins.

### Requirements

#### REQ-F-401: AudioDeviceModel shall track input and output devices
**Statement:** A new `AudioDeviceModel` class shall aggregate all PulseAudio devices and expose them to QML.

**Acceptance criteria:**
- AudioDeviceModel inherits QAbstractListModel
- Implements rowCount(), data(), and roleNames() for QML binding
- Tracks sink (output) and source (input) devices from PulseAudio
- Each device exposes properties: id, name, description, currentVolume (0-100), isMuted, isDefault
- Emits dataChanged() when PulseAudio sends PropertiesChanged signals
- QML can bind to and iterate over devices via role-based access (name, description, currentVolume)

#### REQ-F-402: AudioStreamModel shall track playback and recording streams
**Statement:** A new `AudioStreamModel` class shall aggregate all active PulseAudio streams and expose them to QML.

**Acceptance criteria:**
- AudioStreamModel inherits QAbstractListModel
- Tracks sink-input (playback) and source-output (recording) streams
- Each stream exposes properties: id, name, application, currentVolume, isMuted, currentDevice (id)
- Emits dataChanged() when streams are created, destroyed, or properties change
- QML can bind to list of active playback and recording streams

#### REQ-F-403: PulseAudioBackend shall encapsulate libpulse interaction
**Statement:** A new `PulseAudioBackend` class shall wrap all libpulse API calls and abstract away raw pa_* functions.

**Acceptance criteria:**
- PulseAudioBackend is a private (internal) class, not exposed to QML
- Provides public methods: setDeviceVolume(), setDeviceMuted(), setDefaultOutput(), setDefaultInput(), setStreamVolume(), setStreamMuted(), moveStreamToDevice()
- All pa_context operations (device enumeration, volume changes, default routing) delegate to this class
- Error handling for PA connection loss and recovery is encapsulated
- No pa_* pointers or types leak to public headers

#### REQ-F-404: AudioService shall coordinate device and stream models
**Statement:** The `AudioService` class shall be extended with new Q_PROPERTY members and Q_INVOKABLE methods to support the expanded audio model.

**Acceptance criteria:**
- AudioService has new properties: outputs (AudioDeviceModel*), inputs (AudioDeviceModel*), playbackStreams (AudioStreamModel*), recordingStreams (AudioStreamModel*)
- All four properties are exposed to QML via Q_PROPERTY
- AudioService provides new Q_INVOKABLE methods: setDefaultOutput(id), setDefaultInput(id), setDeviceVolume(id, percent), setDeviceMuted(id, muted), setStreamVolume(id, percent), setStreamMuted(id, muted), moveStreamToOutput(streamId, sinkId), moveStreamToInput(streamId, sourceId)
- All methods validate input and emit signals on success
- Existing volume widget API (setVolume, setMuted) continues to work unchanged

#### REQ-F-405: AudioTypes.h shall define common audio data types
**Statement:** A new header `AudioTypes.h` shall define domain types for audio entities.

**Acceptance criteria:**
- Defines struct AudioDevice with fields: id (uint32), name (QString), description (QString), volume (uint8), muted (bool)
- Defines struct AudioStream with fields: id (uint32), name (QString), application (QString), device (uint32), volume (uint8), muted (bool)
- Defines enum AudioDeviceType { Sink, Source }
- Types are used consistently across AudioDeviceModel, AudioStreamModel, and PulseAudioBackend
- No Q_OBJECT in these types (plain data structs)

#### REQ-F-406: Audio module shall not break existing QML audio widget
**Statement:** After audio expansion, the existing audio widget in the topbar shall continue to function without QML changes.

**Acceptance criteria:**
- The topbar AudioWidget.qml binds to AudioService.volume and AudioService.isMuted as before
- Volume adjustment via existing widget calls setVolume() method and updates the bar correctly
- No QML in AudioWidget.qml is modified
- Feature parity with pre-expansion: volume display, mute toggle, icon updates

#### REQ-NF-401: Audio module shall support concurrent device and stream updates
**Statement:** The audio module shall handle rapid PropertiesChanged signals from PulseAudio without dropping updates.

**Acceptance criteria:**
- Models queue PropertiesChanged events and process them in order
- No updates are lost if two signals arrive within 10ms
- Model emits dataChanged() for each logical change (e.g., one per device, not per property)
- No crashes or undefined behavior under sustained PulseAudio chatter (tested with `pactl` spam)

#### REQ-NF-402: AudioService shall reconnect gracefully to PulseAudio after disconnect
**Statement:** If the PulseAudio connection is lost, the system shall attempt reconnection and restore state.

**Acceptance criteria:**
- If pa_context disconnects, reconnect is attempted within 1 second
- Exponential backoff caps reconnect interval at 5 seconds
- On reconnect, all devices and streams are re-enumerated
- QML observers are not notified of transient disconnects (internal retries only)
- After successful reconnect, all properties match PulseAudio state

#### REQ-C-401: Audio module shall be in src/services/audio/
**Statement:** The expanded audio code shall reside in the `src/services/audio/` directory.

**Acceptance criteria:**
- AudioService.h/cpp, AudioDeviceModel.h/cpp, AudioStreamModel.h/cpp, PulseAudioBackend.h/cpp, AudioTypes.h exist in src/services/audio/
- `AudioState.h/cpp` remains in `src/core/` (it is a 2-field POD that serves the bar widget's volume summary; the richer `AudioTypes.h` types serve the popup. They coexist permanently — no cross-milestone move)
- CMakeLists.txt includes the new src/services/audio/ files in `holonight_services` target only

#### REQ-C-402: Audio expansion shall complete before pavucontrol popup work
**Statement:** No pull request or feature branch implementing a pavucontrol-like popup shall be merged until the audio module expansion (Milestone 4) is complete and merged.

**Acceptance criteria:**
- Audio expansion is merged to main branch before any popup feature branch is created
- Popup feature work only begins after Milestone 4 acceptance testing passes
- No interleaving of structural refactor (M1–M3) and popup feature work

---

## Milestone 5: Test Audit and Gap Fill

### Purpose
Audit existing unit tests, identify gaps in coverage for pure-logic code that moves during restructuring, and add tests for critical gaps.

### Requirements

#### REQ-F-501: Audit shall identify all existing unit tests
**Statement:** A comprehensive audit shall enumerate all existing unit tests in the holonight-shell codebase.

**Acceptance criteria:**
- A document lists all test files (GTest .cpp files under src/ or tests/)
- Each test lists the class/function it covers
- Tests are categorized by module: workspace, active window, battery, audio, network, keyboard, session, theme, tray, surfaces
- Current pass/fail status is recorded for each test under the baseline build

#### REQ-F-502: Audit shall identify untested pure-logic code
**Statement:** The audit shall identify all pure-logic, non-UI code that will move during Milestones 1–3 and has zero test coverage.

**Acceptance criteria:**
- For each service class and model, identify methods with no corresponding test
- Flag only methods with business logic (parsing, state transitions, data transforms), not trivial getters
- Flag only code in classes that will move to new src/ subdirectories or new CMake targets
- Examples of targeted code: AudioService::setVolume logic, HyprlandWorkspaceService::onWorkspaceEvent state machine, NetworkService::parseSSID parsing

#### REQ-F-503: Critical gaps shall be filled with new tests
**Statement:** New unit tests shall be added for critical untested pure-logic code identified in the audit.

**Acceptance criteria:**
- At least one test is added for each identified critical gap
- "Critical gap" is defined as: pure-logic code (no D-Bus/Hyprland IPC/file I/O) with more than 10 lines, >1 branch, and zero coverage
- Each new test follows GTest conventions: TEST(ClassName, DescriptiveTestName)
- Tests use mocking (GoogleMock) to isolate pure logic from external dependencies (D-Bus, Hyprland IPC)
- All new tests pass on the main branch after Milestones 1–3

#### REQ-NF-501: Test suite shall pass clean after restructure
**Statement:** All tests (existing + new) shall pass without warnings or flakes after Milestones 1–3 are complete.

**Acceptance criteria:**
- Running `task test` after Milestone 3 produces exit code 0
- No test fails intermittently (flakes) when run 10 consecutive times
- All assertions are deterministic (no time-dependent tests without stable mocks)
- CTest output shows all tests passing, no xfail or skip markers

#### REQ-NF-502: Test audit shall not delay restructure milestones
**Statement:** Test audit and gap-fill work shall not block Milestones 1–4 from merging.

**Acceptance criteria:**
- Audit is performed in parallel with Milestones 1–3 development
- New tests are added incrementally and merged alongside refactor commits
- If a critical gap is discovered after M1–M3 complete, a follow-up PR is created within one sprint

#### REQ-C-501: Test code shall follow project conventions
**Statement:** All new tests shall adhere to the holonight-shell testing conventions.

**Acceptance criteria:**
- Tests use GTest framework (matching existing test files)
- Mock objects use GoogleMock (gmock)
- Test file names match source file names: Source.cpp → Source.test.cpp or SourceTest.cpp
- Test organization: one test suite per class, one test case per method or scenario
- Each test is independent (no shared state between tests)

---

## Acceptance Criteria Summary

### Milestone 1 Complete When:
- `main.cpp` is ≤ 12 lines (excluding includes/blanks/comments)
- `src/` is split into five domain directories (`app/`, `core/`, `platform/`, `services/`, `surfaces/`); only `main.cpp` remains in the `src/` root
- All existing tests pass
- `task build`, `task qml-lint`, `task tidy`, `task format-check` pass clean
- QML imports resolve unchanged (`import HolonightShell 1.0`, `import Holonight`)
- ShellApplication exposes exactly four public methods: ctor, `registerQmlTypes()`, `startServices()`, `startShell()` — no service accessors

### Milestone 2 Complete When:
- Five C++ static-library targets defined: `holonight_core`, `holonight_platform`, `holonight_services`, `holonight_surfaces`, `holonight_app`
- `qt6_add_qml_module` continues to live on the executable target (no `holonight_qml` static lib)
- Binary size is within 5% of pre-refactor baseline — temporary copy of the debug-build executable after `strip --strip-debug`, same machine and flags as T-000
- `cmake --graphviz` produces an acyclic dependency graph
- Tests are re-linked against minimal subsets of new targets (REQ-C-203)
- Coverage instrumentation covers all five static libraries (REQ-C-204)
- Runtime behavior is identical to pre-refactor
- `task build`, `task qml-lint`, `task tidy`, `task format-check`, `task test` pass clean

### Milestone 3 Complete When:
- All README audio description occurrences (lines 17 and ~79) use accurate libpulse terminology
- Change is reviewed and merged (runs in parallel with M1a; not gated by M1/M2)
- No other README sections are altered

### Milestone 4 Complete When:
- AudioDeviceModel and AudioStreamModel are fully implemented and tested
- AudioService exposes new properties and methods to QML
- Existing audio widget continues to work without changes
- All new audio tests pass
- No other features are implemented on audio until this milestone passes acceptance
- `task build`, `task qml-lint`, `task tidy`, `task format-check`, `task test` pass clean

### Milestone 5 Complete When:
- Test audit document is finalized and reviewed
- All critical gaps have corresponding tests
- Test suite passes 100% with no flakes
- New tests follow project conventions
- `task build`, `task qml-lint`, `task tidy`, `task format-check`, `task test` pass clean

---

## Test Strategy

### Structural Refactor (M1–M3)
- Run existing test suite after each major refactor
- No new functionality, only rearrangement; tests should not change
- If a test fails after refactor, it indicates a structural regression

### Audio Module Expansion (M4)
- Add new tests for AudioDeviceModel and AudioStreamModel using GoogleMock
- Mock PulseAudio backend to avoid real PA connection during testing
- Test state machines and error recovery paths (PA disconnect, rapid updates)
- Verify QML bindings work without running full shell (use qmlplugindump if available)

### Test Audit (M5)
- Use code coverage tools (gcov) to identify untested branches in pure-logic code
- Prioritize tests for methods called by multiple callers (refactor risk)
- Add tests incrementally; do not batch all tests into one large PR

---

## Red Flags and Constraints

### Circular Dependencies
If a circular include is discovered during Milestone 1, refactor the involved modules to introduce a third module or move shared types to `src/core/`.

### Audio Service Shared Consumption
The existing `AudioService` is consumed by the topbar widget. During Milestone 4 expansion, the existing API must remain unchanged. New APIs are additive only.

### QML Import Path Stability
The `HolonightShell` module URI and `Holonight` theme imports must not change. The `qt6_add_qml_module` invocation stays on the executable target throughout (REQ-F-206). The `Holonight` palette module is externally installed (typically `$HOME/.local/lib/qt6/qml/Holonight/`); the qmllint shim under `tests/qmllint/Holonight/` is the only repo-side reference to it. If a CMake modification breaks `task qml-lint`, revert that change and restructure differently.

### Binary Size
Measurement is **debug build, temporary copy stripped via `strip --strip-debug`**, taken on the same machine with the same compiler flags before and after the refactor. The build artifact itself must not be mutated by validation. The 5% threshold is generous on purpose — the refactor adds no new code paths, so static-lib splitting alone should not materially affect the post-strip binary. If size grows >5%, investigate symbol duplication first (`nm --print-size build/holonight-shell | sort -k2 -rh | head -50`); LTO (`-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`) is a fallback, not a default.

### Runtime Performance Gate (intentionally absent)
This refactor does **not** add a startup-time or FPS regression gate. The restructure introduces no new code paths, no new threads, and no service-init reordering — runtime behavior is structurally identical pre- and post-refactor. Manual smoke-test during M5 acceptance is the regression-detection mechanism. A perf gate was considered and rejected as flaky/low-value for a pure structural refactor.

### Wayland Protocol Generation
If wayland-scanner invocation fails after Milestone 2, verify that `holonight_platform` target generates protocols correctly and all dependent targets can include the generated headers via `${CMAKE_CURRENT_BINARY_DIR}` propagated as a PUBLIC include directory.

---

## Definition of Done

Each milestone is considered **done** when:
1. All requirements in the milestone are satisfied (acceptance criteria met)
2. All existing unit tests pass without regression
3. Code review is complete (no outstanding requested changes)
4. `task build`, `task qml-lint`, `task test`, and `task tidy` pass clean
5. A merge commit is created with milestone completion message

Example commit message:
```
feat(arch): complete milestone 1 - shell application extraction and src/ split

- Extract main.cpp logic into ShellApplication class
- Restructure src/ into five domain directories (app, core, platform, services, surfaces)
- All services initialized and registered by ShellApplication via 4 public methods
- main.cpp reduced to ~10 lines
- All existing tests pass
- QML imports unchanged

Closes #<issue-number>
```

---

## Implementation Order

**T-000** (precondition): record baseline binary size before any milestone begins.

**M3** runs in parallel with M1a (single-line README fix, zero deps). **M5 audit work starts in parallel with M1–M3** and can add gap-fill tests incrementally. The implementation dependency chain is M1 → M2 → M4; M4 must complete before any pavucontrol-popup feature work.

1. **T-000:** Record `ls -l build/holonight-shell` size on current main into the roadmap docs.
2. **M1a / M3** (parallel): Create `src/app/` + `ShellApplication` skeleton, and update README.
3. **M1b:** Move service construction logic from main.cpp to ShellApplication constructor.
4. **M1c:** Move QML singleton registration to `ShellApplication::registerQmlTypes()`. Add `startShell()` that owns LayerShellManager.
5. **M1d:** Redistribute remaining `src/` files to `core/`, `platform/`, `services/`, `surfaces/`.
6. **M2a:** Define `holonight_platform` CMake target with Wayland/Hyprland integration.
7. **M2b:** Define `holonight_services`, `holonight_surfaces`, `holonight_app` targets.
8. **M2c:** Narrow `holonight_core` to `src/core/` only.
9. **M2d:** Re-link tests against minimal target subsets; update coverage instrumentation to cover all five static libs.
10. **M2e:** Verify executable still owns `qt6_add_qml_module`; `task build`, `task qml-lint`, `task test`, `task tidy` clean.
11. **M4a:** Implement `AudioTypes.h` and `PulseAudioBackend` in `src/services/audio/`.
12. **M4b:** Implement `AudioDeviceModel` and `AudioStreamModel`.
13. **M4c:** Extend `AudioService` with new properties and invokables (existing API unchanged).
14. **M4d:** Write tests for audio models and `PulseAudioBackend`.
15. **M5a (parallel with M1–M3):** Run test audit and document coverage gaps.
16. **M5b (parallel/incremental):** Add tests for critical gaps as they are identified.

---

## Appendix: Glossary

- **holonight_core:** CMake target (static library) containing core domain models and workspace integration — WorkspaceModel, ExtWorkspaceManager, HyprlandWorkspaceService, KeyboardLayoutService, BatteryState, AudioState, SystemInfo.
- **holonight_platform:** CMake target containing low-level Wayland, Hyprland, and D-Bus integration — LayerShell, LayerSurface, HyprlandIpc(Client), DbusPropertyClient.
- **holonight_services:** CMake target containing service implementations — ActiveWindowService, BatteryService, AudioService, NetworkService, SessionService, SystemInfoService, ThemeService.
- **holonight_surfaces:** CMake target containing UI surface, bar bootstrap, and tray implementations — LayerShellManager, PopupSurface, TooltipSurface, TrayMenuSurface, TrayModel, TrayItem, TrayItemProperties, TrayWatcher, DbusMenu{Item,Client}, IconImageProvider.
- **holonight_app:** CMake target containing application logic — `ShellApplication` only. `main.cpp` is compiled directly into the executable, not into this library.
- **ShellApplication:** C++ class that encapsulates all service construction, QML registration, and layer-shell startup. Public surface: ctor + `registerQmlTypes()` + `startServices()` + `startShell()`. No service accessors.
- **HolonightShell:** QML module URI used by all QML singletons (unchanged throughout refactor). Owned by the executable target via `qt6_add_qml_module`.
- **Holonight:** External QML theme module providing palette tokens, installed outside this repo (typically `$HOME/.local/lib/qt6/qml/Holonight/`). The restructure does not touch it.
