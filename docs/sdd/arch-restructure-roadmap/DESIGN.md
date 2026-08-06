# Architecture Restructuring: Design Document

**Project:** holonight-shell (C++23/Qt6 Wayland shell)
**Version:** 1.3
**Date:** 2026-05-27
**Status:** Design
**Companion:** SPEC.md (same directory), REVIEW-FINDINGS.md (resolutions log)

**Revision history:**
- v1.0 (2026-05-27) — initial design
- v1.1 (2026-05-27) — applied review resolutions:
  - **C1:** Dropped `holonight_qml` static-library target. `qt6_add_qml_module` stays on the executable. Five C++ static-lib targets total.
  - **C2:** `registerQmlSingletons(QQmlEngine*)` → `registerQmlTypes()` (no parameter). The Qt API is a global registry.
  - **C3:** `LayerShellManager` ownership moved into `ShellApplication::startShell()`. No public service accessors. `main.cpp` ≤ 12 LOC.
  - **C4:** Dropped the `holonight_surfaces → holonight_services` link edge.
  - **M1:** `AudioState` stays in `src/core/` permanently. `AudioTypes.h` lives in `src/services/audio/`.
  - **M3:** Runs in parallel with M1a, covers both README occurrences.
  - **M7:** `ServiceRegistry` class dropped — registration lambdas inline in `ShellApplication::registerQmlTypes()`.
  - **M8:** `SystemInfo` (free functions) moved to `src/core/`; `SystemInfoService` stays in `src/services/`.
  - **M9:** `Qt6::Qml` added to `holonight_core` PUBLIC deps (required by `QML_ELEMENT` in headers).
- v1.2 (2026-05-27) — closed v1.1 open items and red flags:
  - **Binary-size gate:** pinned to **debug-build + `strip --strip-debug` on a temporary copy**; release size not gated. Updated §8 risk section with measurement convention.
  - **Runtime perf gate:** explicitly excluded. M5 acceptance smoke-test covers runtime regression.
  - **Constructor discipline:** §2.3 documents the 60-LOC soft ceiling and "construction-only" rule; any growth past the ceiling splits into private `constructDomain()` helpers — never a `ServiceRegistry`.
  - **`platform/` flat structure:** §2.1 documents the escape hatch (nest as `platform/{wayland,hyprland,dbus}/` if file count exceeds ~15).
  - **`surfaces/` scope:** §2.1 explicitly notes that tray plumbing (`IconImageProvider`, `DbusMenu*`) coexists with true layer-shell surfaces.
  - **Lint gate per milestone:** added to M4 and M5 acceptance criteria in SPEC (M1, M2 already had it).
- v1.3 (2026-05-27) — fixed implementability gaps:
  - **Target graph:** `ExtWorkspaceManager` moves to `core/` because it depends on `WorkspaceModel`; `LayerShellManager` moves to `surfaces/` because it creates QQuickViews and consumes tray image providers.
  - **QML metadata:** generated `holonight-shell.qmltypes` content is now an explicit acceptance gate; `task qml-lint` alone is not enough.
  - **Tests:** M2 requires module-level test executables instead of one omnibus `holonight_tests` target.
  - **Audio API:** backend, service, and tasks now list the same device/stream control methods.
  - **Binary-size gate:** validation strips a temporary copy, not `build/holonight-shell` in place.

---

## Table of Contents

1. [Current State Analysis](#1-current-state-analysis)
2. [Target Architecture](#2-target-architecture)
3. [Component Descriptions](#3-component-descriptions)
4. [Data Flow](#4-data-flow)
5. [Migration Plan Per Milestone](#5-migration-plan-per-milestone)
6. [Key Decisions with Rationale](#6-key-decisions-with-rationale)
7. [Alternatives Considered](#7-alternatives-considered)
8. [Known Risks](#8-known-risks)

---

## 1. Current State Analysis

### 1.1 Current `src/` Flat Structure

All 58 C++ source files live directly in `src/` with no subdirectories. The table below groups them by the logical domain they belong to, which also maps to the intended target directory after M1.

**Domain: platform** — Low-level Wayland protocol wrappers, Hyprland IPC transport, and D-Bus helpers

| File | Responsibility |
|---|---|
| `LayerShell.h` | `QWaylandClientExtensionTemplate` wrapper for `zwlr_layer_shell_v1` |
| `LayerSurface.h/.cpp` | Wraps a single `zwlr_layer_surface_v1`; handles configure/ack/resize |
| `HyprlandIpc.h/.cpp` | Abstract `HyprlandIpcTransport` interface used by workspace and keyboard services |
| `HyprlandIpcClient.h/.cpp` | Concrete transport over Hyprland Unix domain sockets with exponential-backoff reconnect |
| `DbusPropertyClient.h/.cpp` | Abstract + concrete `QDBusConnection` helper (property reads, signal connects) |

**Domain: core** — Shared models, parsers, configuration, and workspace/keyboard integration

| File | Responsibility |
|---|---|
| `WorkspaceModel.h/.cpp` | `QAbstractListModel` of workspace entries; state machine (empty/occupied/focused/urgent) |
| `ExtWorkspaceManager.h/.cpp` | `QWaylandClientExtension` for `ext-workspace-v1`; drives `WorkspaceModel` from compositor events |
| `HyprlandWorkspaceService.h/.cpp` | Translates Hyprland IPC workspace events into `WorkspaceModel` mutations |
| `KeyboardLayoutService.h/.cpp` | Tracks current keyboard layout via Hyprland IPC `j/devices` events |
| `BatteryState.h/.cpp` | Plain data struct for battery snapshot (level, state, time remaining) |
| `AudioState.h/.cpp` | Plain data struct for audio snapshot (volume percent, muted flag) — permanent home; coexists with `services/audio/AudioTypes.h` after M4 |
| `SystemInfo.h/.cpp` | Free functions: `parseOsRelease()`, `systemInfoFromOsRelease()`, `findSystemLogoPath()` — pure logic, no D-Bus or IPC |

**Domain: services** — D-Bus service implementations and QML adapters

| File | Responsibility |
|---|---|
| `ActiveWindowService.h/.cpp` | Per-monitor active window tracking via Hyprland IPC `j/monitors` + `j/clients` |
| `BatteryService.h/.cpp` | UPower D-Bus cascade; `EnumerateDevices` + `PropertiesChanged` subscription |
| `AudioService.h/.cpp` | libpulse integration; `pa_threaded_mainloop`, sink info callbacks, volume control |
| `NetworkService.h/.cpp` | NetworkManager D-Bus; AP strength, SSID, VPN detection |
| `SessionService.h/.cpp` | Command-only service (power off, reboot, logout) — no async initialization |
| `SystemInfoService.h/.cpp` | QML singleton wrapping `SystemInfo` snapshot; all `CONSTANT` properties |
| `ThemeService.h/.cpp` | QML singleton exposing font family and size constants from theme configuration |

**Domain: surfaces** — Layer-shell popup surfaces and system tray

| File | Responsibility |
|---|---|
| `LayerShellManager.h/.cpp` | Creates one `QQuickView` + `LayerSurface` per `QScreen`; bootstraps the topbar bars |
| `PopupSurface.h/.cpp` | Manages the session popup `QQuickView` on the `OVERLAY` layer |
| `TooltipSurface.h/.cpp` | Manages the tooltip `QQuickView`; 450 ms hover delay via `BarTooltipArea` |
| `TrayMenuSurface.h/.cpp` | Manages the tray context menu `QQuickView` on the `OVERLAY` layer |
| `TrayModel.h/.cpp` | `QAbstractListModel` of `TrayItem`; image provider; activate/scroll/menu dispatch |
| `TrayItem.h/.cpp` | Immutable SNI item snapshot (icon, status, tooltip fields) |
| `TrayItemProperties.h/.cpp` | Mutable accumulator built from D-Bus `GetAll` results before snapshot is created |
| `TrayWatcher.h/.cpp` | `org.kde.StatusNotifierWatcher` D-Bus service; registers/deregisters items |
| `DbusMenuItem.h/.cpp` | Data node for a single DBus menu entry (label, icon, submenu) |
| `DbusMenuClient.h/.cpp` | Fetches `com.canonical.dbusmenu` layout from a tray item's menu path |
| `IconImageProvider.h` | `QQuickImageProvider` adapter for system icon theme lookup (header only) |

**Domain: qml** — All QML source files (unchanged throughout refactor)

All files under `src/qml/Topbar/` and `src/qml/Tray/` (31 `.qml` files).

### 1.2 Current CMake Target Structure

There are exactly two CMake targets:

```
holonight_core  (STATIC library)
    All 58 source files listed above (platform + core + services + surfaces)
    qt6_generate_wayland_protocol_client_sources for:
        wlr-layer-shell-unstable-v1.xml
        ext-workspace-v1.xml
        xdg-shell.xml
    PUBLIC links: Qt6::Core, Qt6::Gui, Qt6::Quick, Qt6::DBus, Qt6::Network,
                  Qt6::Concurrent, Qt6::WaylandClient, wayland-client,
                  tomlplusplus, libpulse
    PRIVATE links: Qt6::GuiPrivate

holonight-shell  (Executable)
    src/main.cpp
    qt6_add_qml_module: URI=HolonightShell, VERSION=1.0, 31 QML files
    PRIVATE links: holonight_core
```

Everything is funneled through `holonight_core`. There is no encapsulation: a service file can include a platform header or a surface header without any CMake-level visibility constraint.

### 1.3 What `main.cpp` Currently Does

`main.cpp` (115 lines) performs six distinct responsibilities that must be extracted into `ShellApplication`:

1. **Object construction** — Creates 16 service/model instances with `new` on the heap, parented to `QGuiApplication`.
2. **Dependency wiring** — Passes `model` to `ExtWorkspaceManager`, passes `tray_model` to `TrayWatcher` and `TrayModel::setMenuSurface()`.
3. **QML singleton registration** — Calls `QQmlEngine::setObjectOwnership()` + `qmlRegisterSingletonType()` for 13 instances (WorkspaceModel, ActiveWindowService, KeyboardLayoutService, BatteryService, AudioService, NetworkService, SessionService, SystemInfoService, ThemeService, PopupSurface, TooltipSurface, TrayMenuSurface, TrayModel).
4. **Async service start** — Calls `.start()` in order: HyprlandWorkspaceService, KeyboardLayoutService, ActiveWindowService, BatteryService, AudioService, NetworkService, TrayWatcher.
5. **LayerShellManager construction** — Creates `LayerShellManager` on the stack (creates one bar per screen).
6. **Event loop** — Calls `QGuiApplication::exec()`.

After M1, responsibilities 1–4 move to `ShellApplication`. Responsibility 5 stays in `main.cpp` or moves to `ShellApplication`. Responsibility 6 stays in `main.cpp`.

---

## 2. Target Architecture

### 2.1 Proposed `src/` Directory Tree After M1

```
src/
├── main.cpp                         (≤ 12 LOC; entry point only)
├── app/
│   ├── ShellApplication.h
│   └── ShellApplication.cpp
├── core/
│   ├── WorkspaceModel.h/.cpp
│   ├── ExtWorkspaceManager.h/.cpp    (Wayland ext-workspace adapter that mutates WorkspaceModel)
│   ├── HyprlandWorkspaceService.h/.cpp
│   ├── KeyboardLayoutService.h/.cpp
│   ├── BatteryState.h/.cpp
│   ├── AudioState.h/.cpp
│   └── SystemInfo.h/.cpp            (pure parser functions)
├── platform/
│   ├── LayerShell.h
│   ├── LayerSurface.h/.cpp
│   ├── HyprlandIpc.h/.cpp
│   ├── HyprlandIpcClient.h/.cpp
│   └── DbusPropertyClient.h/.cpp
├── services/
│   ├── ActiveWindowService.h/.cpp
│   ├── BatteryService.h/.cpp
│   ├── AudioService.h/.cpp           (M4 moves this to services/audio/)
│   ├── NetworkService.h/.cpp
│   ├── SessionService.h/.cpp
│   ├── SystemInfoService.h/.cpp      (QML adapter; SystemInfo helpers live in core/)
│   └── ThemeService.h/.cpp
├── surfaces/
│   ├── LayerShellManager.h/.cpp      (bar QQuickView factory; consumes TrayModel/image providers)
│   ├── PopupSurface.h/.cpp
│   ├── TooltipSurface.h/.cpp
│   ├── TrayMenuSurface.h/.cpp
│   ├── TrayModel.h/.cpp
│   ├── TrayItem.h/.cpp
│   ├── TrayItemProperties.h/.cpp
│   ├── TrayWatcher.h/.cpp
│   ├── DbusMenuItem.h/.cpp
│   ├── DbusMenuClient.h/.cpp
│   └── IconImageProvider.h
└── qml/
    ├── Topbar/   (all Topbar/*.qml, unchanged)
    └── Tray/     (all Tray/*.qml, unchanged)
```

**`platform/` flat structure — escape hatch.** `platform/` is intentionally flat in v1.x (5 logical files spanning low-level Wayland, Hyprland IPC, and a D-Bus utility). The original review (`docs/project-architecture-review.md`) proposed nesting as `platform/{wayland,hyprland,dbus}/`, but at the current file count the subdirs would each hold 1–2 files — the structural cost (longer paths in CMake `target_sources`, more `cd` friction during editing) outweighs the organizational benefit. **If `platform/` grows beyond ~15 files**, nest it then. Treat this paragraph as a documented exit condition, not a hard rule.

**`surfaces/` scope.** Despite the name, `surfaces/` houses the bar factory (`LayerShellManager`), layer-shell popup surface classes (`PopupSurface`, `TooltipSurface`, `TrayMenuSurface`), and tray-supporting plumbing (`TrayModel`, `TrayItem`, `TrayItemProperties`, `TrayWatcher`, `DbusMenu{Item,Client}`, `IconImageProvider`). The tray plumbing lives here because it is consumed exclusively by `LayerShellManager`, `TrayMenuSurface`, and tray QML; it has no service-layer consumers. Renaming the directory was considered (`surfaces-and-tray/`) and rejected as noise.

After M4, the audio subdomain expands. `AudioState` stays in `src/core/` (it's a POD used by the bar widget); the new rich types live alongside `AudioService`:

```
src/services/
├── audio/
│   ├── AudioService.h/.cpp          (moved from services/ root in M4c)
│   ├── AudioTypes.h                 (NEW: AudioDevice, AudioStream, AudioDeviceType)
│   ├── PulseAudioBackend.h/.cpp     (NEW: hides all pa_* symbols)
│   ├── AudioDeviceModel.h/.cpp      (NEW)
│   └── AudioStreamModel.h/.cpp      (NEW)
├── ActiveWindowService.h/.cpp
...

src/core/AudioState.h/.cpp           (UNCHANGED — bar-widget summary type)
```

`AudioState` (2-field POD: volume, muted) and `AudioTypes::AudioDevice` (rich per-device record) coexist — the bar widget reads `AudioState` for its summary; the popup reads `AudioDeviceModel` rows. No file move in M4 for `AudioState`.

### 2.2 Five CMake Targets After M2

Five C++ static-library targets, plus the executable that owns the QML module. No separate `holonight_qml` target — `qt6_add_qml_module(holonight-shell ...)` stays on the executable. The executable target must pass all QML-facing C++ headers to qmltyperegistrar via `SOURCES` or another explicitly verified CMake mechanism; generated qmltypes content is validated directly.

```
Dependency graph (A -> B means A depends on B):

holonight-shell (executable, owns qt6_add_qml_module)
       |
       +-PRIVATE-> holonight_app
                       |
                       +-PUBLIC-> holonight_services
                       |              |
                       |              +-PUBLIC-> holonight_core
                       |              |              |
                       |              |              +-PUBLIC-> holonight_platform
                       |              |
                       |              +-PUBLIC-> holonight_platform
                       |
                       +-PUBLIC-> holonight_surfaces
                       |              |
                       |              +-PUBLIC-> holonight_platform
                       |              (NO link to holonight_services — verified by
                       |               grep showing no service includes in surfaces/)
                       |
                       +-PUBLIC-> holonight_core
                       +-PUBLIC-> holonight_platform
```

Canonical layered view (acyclic):

```
holonight_platform   (depends on nothing internal)
       ^
holonight_core       (depends on platform)
       ^
holonight_services   (depends on core, platform)
       ^
holonight_surfaces   (depends on platform only — NOT services)
       ^
holonight_app        (depends on all four)
       ^
holonight-shell      (executable; links holonight_app; owns qt6_add_qml_module)
```

**Why no `holonight_qml` target.** Every singleton service header (`AudioService.h`, `BatteryService.h`, …, plus `WorkspaceModel.h`, `PopupSurface.h`, etc.) carries `QML_ELEMENT` + `QML_SINGLETON`. The `qmltyperegistrar` tool runs on the target that owns `qt6_add_qml_module`. Moving the QML module to a separate static library would require static QML plugin import machinery (`Q_IMPORT_QML_PLUGIN`, `qt_import_qml_plugins`) that is fragile across Qt 6.x versions. The chosen design keeps `qt6_add_qml_module` on the executable and explicitly validates that generated `holonight-shell.qmltypes` contains the expected C++ types. If linking static libraries alone produces `Module {}`, the implementation must pass QML-facing headers through the executable-owned QML module without double-compiling `.cpp` files.

Non-QML assets (`assets/bar-icons/*.svg`, etc.) are bundled via a separate `qt6_add_resources(holonight-shell "assets" PREFIX "/HolonightShell" ...)` invocation on the executable.

### 2.3 `ShellApplication` Class Interface

```cpp
// src/app/ShellApplication.h
#pragma once

#include <QObject>
#include <memory>

class WorkspaceModel;
class ExtWorkspaceManager;
class HyprlandWorkspaceService;
class KeyboardLayoutService;
class ActiveWindowService;
class BatteryService;
class AudioService;
class NetworkService;
class SessionService;
class SystemInfoService;
class ThemeService;
class PopupSurface;
class TooltipSurface;
class TrayMenuSurface;
class TrayModel;
class TrayWatcher;
class LayerShellManager;

class ShellApplication : public QObject {
  Q_OBJECT
 public:
  explicit ShellApplication(QObject* parent = nullptr);
  ~ShellApplication() override;

  // Register all QML singleton types with the global Qt QML type registry.
  // Idempotent: a guard bool makes the second call a silent no-op.
  // Must be called before startShell() (which creates QQuickViews).
  void registerQmlTypes();

  // Start all async services in dependency order. Idempotent.
  void startServices();

  // Construct the LayerShellManager (per-monitor QQuickView + LayerSurface).
  // Must be called after registerQmlTypes() and startServices().
  void startShell();

  // No public service accessors. ShellApplication is the sole owner of all
  // services; external code interacts with them via QML or not at all.

 private:
  // Owned via Qt parent chain (this) — destruction order is reverse of
  // construction order, which matches the original main.cpp.
  WorkspaceModel*           model_{nullptr};
  ExtWorkspaceManager*      manager_{nullptr};
  HyprlandWorkspaceService* workspace_service_{nullptr};
  KeyboardLayoutService*    keyboard_layout_{nullptr};
  ActiveWindowService*      aws_{nullptr};
  BatteryService*           battery_{nullptr};
  AudioService*             audio_{nullptr};
  NetworkService*           network_{nullptr};
  SessionService*           session_{nullptr};
  SystemInfoService*        system_info_{nullptr};
  ThemeService*             theme_{nullptr};
  PopupSurface*             popup_surface_{nullptr};
  TooltipSurface*           tooltip_surface_{nullptr};
  TrayMenuSurface*          tray_menu_surface_{nullptr};
  TrayModel*                tray_model_{nullptr};
  TrayWatcher*              tray_watcher_{nullptr};

  std::unique_ptr<LayerShellManager> lsm_;

  bool registered_{false};
  bool services_started_{false};
  bool shell_started_{false};
};
```

**Lifecycle invariants:**
1. `ShellApplication` is constructed on the stack in `main.cpp`, parented to the stack-allocated `QGuiApplication`.
2. `registerQmlTypes()` calls `qmlRegisterSingletonType<T>("HolonightShell", 1, 0, ...)` for each of the 13 QML singletons. Because `qmlRegisterSingletonType` is a **global registry**, the call does not need a `QQmlEngine*` and applies to every engine created afterwards (one per monitor inside `LayerShellManager`).
3. `startServices()` calls `start()` on the 7 async-init services in fixed order.
4. `startShell()` constructs `LayerShellManager(tray_model_, this)` and stores it via `unique_ptr`. This creates one `QQuickView` per `QScreen`.
5. On `QGuiApplication::exec()` return, `~ShellApplication()` runs: `lsm_` destroyed first (closes views), then Qt parent chain destroys all services in reverse construction order.

**Constructor discipline.** The constructor body is **construction-only**: each line allocates exactly one service via `new T(this)` (or `new T(other_ptr, this)` for the few that need a sibling pointer). No signal/slot wiring, no calls into services, no conditional construction. All dependency wiring (`tray_model_->setMenuSurface(...)`, `connect(...)` calls, future signal hookups) lives in `startServices()` or `startShell()` — not in the constructor. This keeps the constructor a flat list that maps 1:1 to the service inventory; cognitive complexity stays ~1 per line, well below the clang-tidy threshold of 25.

**Soft ceiling: 60 LOC for the constructor body.** If M4's audio expansion (or any later milestone) pushes the constructor past this ceiling, split into private member-init helpers named by domain — `constructAudio()`, `constructTray()`, `constructWorkspaces()`. Helpers are private methods of `ShellApplication`, not a separate class. **Do not** reintroduce a `ServiceRegistry`-style external coordinator; that pattern was rejected in v1.1 (see §6 decisions) and the same arguments apply to any equivalent abstraction. Private helpers keep all ownership inside `ShellApplication` while taming the length.

### 2.4 `main.cpp` After M1

```cpp
#include "app/ShellApplication.h"
#include <QGuiApplication>

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  ShellApplication shell(&app);
  shell.registerQmlTypes();
  shell.startServices();
  shell.startShell();
  return QGuiApplication::exec();
}
```

Eight lines of executable code (below the 12-line limit). No `ServiceRegistry`, no helper classes — the registration logic lives inline inside `ShellApplication::registerQmlTypes()` as 13 plain `qmlRegisterSingletonType` calls (~30 lines, repetitive but clear).

---

## 3. Component Descriptions

### 3.1 `holonight_platform`

**What it contains:**
- `LayerShell.h` — header-only Wayland extension template for `zwlr_layer_shell_v1`
- `LayerSurface.h/.cpp` — configure/ack/resize lifecycle for a single layer surface
- `HyprlandIpc.h/.cpp` — abstract transport interface (`HyprlandIpcTransport`)
- `HyprlandIpcClient.h/.cpp` — socket-level implementation with exponential-backoff reconnect
- `DbusPropertyClient.h/.cpp` — abstract D-Bus property reader + concrete `QtDbusPropertyClient`
- Generated Wayland protocol headers/sources (wlr-layer-shell, ext-workspace, xdg-shell)

**Public interface (key headers):**
- `LayerShell.h`, `LayerSurface.h`
- `HyprlandIpc.h` (abstract transport; consumers depend on this, not on `HyprlandIpcClient.h`)
- `DbusPropertyClient.h` (abstract interface; lets services mock D-Bus in tests)

**Dependencies:**
- `Qt6::WaylandClient`, `Qt6::Gui`, `wayland-client`
- `Qt6::Core`, `Qt6::DBus`

**Must NOT depend on:**
- `holonight_core`, `holonight_services`, `holonight_surfaces`, `holonight_app`
- No `WorkspaceModel`, no `TrayModel`, no `QQuickView`, no service headers, no QML singletons, no libpulse

### 3.2 `holonight_core`

**What it contains:**
- `WorkspaceModel.h/.cpp` — workspace list model, state machine, activateWorkspace dispatch
- `ExtWorkspaceManager.h/.cpp` — ext-workspace-v1 adapter; mutates `WorkspaceModel` from compositor events
- `HyprlandWorkspaceService.h/.cpp` — IPC event → `WorkspaceModel` mutation bridge
- `KeyboardLayoutService.h/.cpp` — keyboard layout tracking via Hyprland IPC
- `BatteryState.h/.cpp` — plain data struct (no Q_OBJECT)
- `AudioState.h/.cpp` — plain data struct (no Q_OBJECT); permanent home (not moved by M4)
- `SystemInfo.h/.cpp` — free functions for OS release parsing (`parseOsRelease()`, `findSystemLogoPath()`); pure logic, no D-Bus or IPC

**Public interface (key headers):**
- `WorkspaceModel.h` — consumed by workspace integration code and QML; carries `QML_ELEMENT`/`QML_SINGLETON`
- `ExtWorkspaceManager.h` — started by construction and consumes generated Wayland protocol headers from platform
- `HyprlandWorkspaceService.h`, `KeyboardLayoutService.h` — started by `ShellApplication`
- `SystemInfo.h` — consumed by `SystemInfoService` in `services/`

**Dependencies:**
- `holonight_platform` (PUBLIC — so generated protocol headers and `HyprlandIpcTransport`/`DbusPropertyClient` propagate to consumers)
- `Qt6::Core`, `Qt6::Qml` (required for `QML_ELEMENT`/`QML_SINGLETON` macros in WorkspaceModel.h), `Qt6::DBus`, `tomlplusplus`

**Boundary note:**
`holonight_core` is not a pure domain target. It deliberately contains workspace/keyboard integration that mutates shared models, plus QML annotations for exported model types. Pure helper code still belongs here when it is shared by services or tests, but low-level protocol/client primitives remain in `holonight_platform`.

**Must NOT depend on:**
- `holonight_services`, `holonight_surfaces`, `holonight_app`
- No D-Bus service implementations, no libpulse, no UI surfaces

### 3.3 `holonight_services`

**What it contains:**
- `ActiveWindowService.h/.cpp`
- `BatteryService.h/.cpp`
- `AudioService.h/.cpp` (after M4 moves to `services/audio/` subdirectory with `AudioTypes.h`, `PulseAudioBackend`, `AudioDeviceModel`, `AudioStreamModel`)
- `NetworkService.h/.cpp`
- `SessionService.h/.cpp`
- `SystemInfoService.h/.cpp` (QML singleton; consumes `SystemInfo` helpers from `core/`)
- `ThemeService.h/.cpp`

**Public interface (key headers):**
- One header per service, each exposing a `QObject` subclass carrying `QML_ELEMENT`/`QML_SINGLETON`

**Dependencies:**
- `holonight_core` (PUBLIC — services need `BatteryState`, `AudioState`, and `SystemInfo` helpers)
- `holonight_platform` (PUBLIC — services use `DbusPropertyClient` and `HyprlandIpcTransport`)
- `Qt6::Core`, `Qt6::Qml`, `Qt6::DBus`, `Qt6::Network`, `Qt6::Concurrent`, `libpulse`

**Must NOT depend on:**
- `holonight_surfaces`, `holonight_app`
- No `LayerSurface.h`, no `PopupSurface.h`

### 3.4 `holonight_surfaces`

**What it contains:**
- `LayerShellManager.h/.cpp` — multi-monitor bar factory; creates `QQuickView` + `LayerSurface` per screen; consumes `TrayModel` for tray image provider
- `PopupSurface.h/.cpp`
- `TooltipSurface.h/.cpp`
- `TrayMenuSurface.h/.cpp`
- `TrayModel.h/.cpp` (forward-declares `TrayMenuSurface`; includes only `DbusMenuClient` and `TrayItem`)
- `TrayItem.h/.cpp`
- `TrayItemProperties.h/.cpp`
- `TrayWatcher.h/.cpp`
- `DbusMenuItem.h/.cpp`
- `DbusMenuClient.h/.cpp`
- `IconImageProvider.h` (header only; QQuickImageProvider for system icon theme)

**Public interface (key headers):**
- `LayerShellManager.h` — constructed by `ShellApplication::startShell()`
- `PopupSurface.h`, `TooltipSurface.h`, `TrayMenuSurface.h` — `QML_ELEMENT`/`QML_SINGLETON` registered
- `TrayModel.h` — `QML_ELEMENT`/`QML_SINGLETON` registered
- `TrayWatcher.h` — started by `ShellApplication`

**Dependencies:**
- `holonight_platform` (PUBLIC — `LayerSurface`, `LayerShell`; surfaces create and manage layer-shell views)
- `Qt6::Core`, `Qt6::Qml`, `Qt6::Gui`, `Qt6::Quick`, `Qt6::DBus`
- `holonight_services` (PUBLIC — narrow presentation orchestration uses live service state; new direct service includes in `src/surfaces` are guarded by `scripts/check-architecture-boundaries.sh`)
- `Qt6::GuiPrivate` (PRIVATE — native QPA handle access)

**Must NOT depend on:**
- `holonight_app`
- No `ShellApplication.h`

### 3.5 (deleted — `holonight_qml` is not a CMake target)

The original v1.0 design proposed a separate `holonight_qml` static-library target. This was dropped in v1.1 (see §8 "Decision: QML module placement"). The QML module continues to be declared on the executable target via `qt6_add_qml_module(holonight-shell URI HolonightShell VERSION 1.0 QML_FILES ${HOLONIGHT_QML_FILES})`. The implementation must explicitly ensure qmltyperegistrar receives the QML-facing C++ headers, and must validate that generated qmltypes are not empty.

Non-QML assets (`assets/bar-icons/*.svg`, etc.) are bundled via a separate `qt6_add_resources(holonight-shell "assets" PREFIX "/HolonightShell" BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets" FILES ...)` call on the executable.

### 3.6 `holonight_app`

**What it contains:**
- `ShellApplication.h/.cpp` — composition root and runtime wiring
- `ControlServer.h/.cpp` — app-local control socket coordinator

**Note:** `main.cpp` is compiled directly into the `holonight-shell` executable, not into `holonight_app`. This avoids the unusual pattern of compiling `main()` into a static library, which can cause linker confusion with `_start` on some toolchains. `ServiceRegistry` is **not** a thing in v1.1 — the 13 `qmlRegisterSingletonType` lambdas live inline inside `ShellApplication::registerQmlTypes()`. Small app-local coordinators are allowed when they remove runtime-control mechanics from `ShellApplication` without exposing service accessors.

**Public interface (key headers):**
- `ShellApplication.h` — included only by `main.cpp`
- `ControlServer.h` — included by `main.cpp` for the control socket path and by tests for command parsing

**Dependencies:**
- PUBLIC link: `holonight_core`, `holonight_platform`, `holonight_services`, `holonight_surfaces`
- PUBLIC link: `Qt6::Core`, `Qt6::Gui`, `Qt6::Network`, `Qt6::Qml`

**Must NOT depend on:**
- External Qt internals beyond `Qt6::Gui` and `Qt6::Qml`
- No Wayland protocol headers directly (delegates to platform)
- No `holonight_qml` reference — that target does not exist

---

## 4. Data Flow

### 4.1 Service Initialization Sequence

```
main()
  |
  +-- QGuiApplication app(argc, argv)
  |
  +-- ShellApplication shell(&app)
  |     |
  |     +-- [ctor] new WorkspaceModel       (parent: shell)
  |     +-- [ctor] new ExtWorkspaceManager  (WorkspaceModel*, parent: shell)
  |     +-- [ctor] HyprlandWorkspaceService (WorkspaceModel*, parent: shell)
  |     +-- [ctor] new KeyboardLayoutService(parent: shell)
  |     +-- [ctor] new ActiveWindowService  (parent: shell)
  |     +-- [ctor] new BatteryService       (parent: shell)
  |     +-- [ctor] new AudioService         (parent: shell)
  |     +-- [ctor] new NetworkService       (parent: shell)
  |     +-- [ctor] new SessionService       (parent: shell)
  |     +-- [ctor] new SystemInfoService    (parent: shell)
  |     +-- [ctor] new ThemeService         (parent: shell)
  |     +-- [ctor] new PopupSurface         (parent: shell)
  |     +-- [ctor] new TooltipSurface       (parent: shell)
  |     +-- [ctor] new TrayMenuSurface      (parent: shell)
  |     +-- [ctor] new TrayModel            (parent: shell)
  |     +-- [ctor] new TrayWatcher          (TrayModel*, parent: shell)
  |     (constructor returns)
  |
  +-- shell.registerQmlTypes()           (no parameter — global Qt registry)
  |     |
  |     +-- QQmlEngine::setObjectOwnership(model_, CppOwnership)
  |     +-- qmlRegisterSingletonType<WorkspaceModel>("HolonightShell", 1, 0, ...)
  |     ... (13 registrations total)
  |     +-- registered_ = true
  |
  +-- shell.startServices()
  |     |
  |     +-- tray_model_->setMenuSurface(tray_menu_surface_)
  |     +-- workspace_service_->start()    (Hyprland IPC connects, initial query)
  |     +-- keyboard_layout_->start()      (Hyprland IPC connects)
  |     +-- aws_->start()                  (Hyprland IPC: j/monitors + j/clients)
  |     +-- battery_->start()              (D-Bus UPower enumeration)
  |     +-- audio_->start()                (pa_context connect + subscribe)
  |     +-- network_->start()              (D-Bus NM cascade)
  |     +-- tray_watcher_->start()         (D-Bus SNI watcher registration)
  |     (all start() calls return; async callbacks fire later on event loop)
  |     +-- services_started_ = true
  |
  +-- shell.startShell()
  |     |
  |     +-- assert(registered_ && services_started_)
  |     +-- lsm_ = std::make_unique<LayerShellManager>(tray_model_, this)
  |     +-- (per-screen QQuickView + LayerSurface created)
  |     +-- shell_started_ = true
  |
  +-- QGuiApplication::exec()             (event loop)
```

### 4.2 QML Singleton Registration Flow

The pattern used for every singleton is:

```
1. QQmlEngine::setObjectOwnership(ptr, CppOwnership)
      Prevents QML garbage collector from deleting the C++ object.

2. qmlRegisterSingletonType<T>("HolonightShell", 1, 0, "TypeName",
       [ptr](QQmlEngine*, QJSEngine*) -> QObject* { return ptr; })
      Binds the pre-constructed C++ object to the QML type name.
      Lambda captures raw pointer by value — safe because ShellApplication
      outlives all QML engines.

3. QML code: import HolonightShell 1.0
             WorkspaceModel.revision  // accesses the singleton
```

Multi-monitor note: `LayerShellManager` creates one `QQuickView` per screen, each with its own `QQmlEngine`. Because `qmlRegisterSingletonType` uses the global type registry, all engines resolve the same C++ singleton instance via the lambda. This is why the factory-callback form is used instead of `qmlRegisterSingletonInstance` (which binds to exactly one engine).

### 4.3 Audio Domain Data Flow (M4)

After M4, the audio subsystem has a layered architecture:

```
PulseAudio daemon (system process)
       |
       | pa_threaded_mainloop + pa_context
       v
PulseAudioBackend  (internal class, no Q_OBJECT)
  - pa_context_subscribe(SINK | SOURCE | SINK_INPUT | SOURCE_OUTPUT)
  - pa_context_get_sink_info_list / pa_context_get_source_info_list
  - pa_context_get_sink_input_info_list / pa_context_get_source_output_info_list
  - pa_context_set_sink_volume_by_index / pa_context_set_source_volume_by_index
  - pa_context_move_sink_input_by_index
       |
       | signals: deviceAdded(AudioDevice), deviceRemoved(uint32),
       |          deviceChanged(AudioDevice), streamAdded(AudioStream),
       |          streamRemoved(uint32), streamChanged(AudioStream)
       v
AudioService  (QObject, QML singleton)
  - Owns: AudioDeviceModel* outputs_
  -       AudioDeviceModel* inputs_
  -       AudioStreamModel* playback_streams_
  -       AudioStreamModel* recording_streams_
  - Existing API unchanged: volume(), muted(), available(), setVolume(int), applyVolume(int)
  - New Q_PROPERTYs: outputs, inputs, playbackStreams, recordingStreams
  - New Q_INVOKABLEs: setDefaultOutput(id), setDefaultInput(id),
  -                   setDeviceVolume(id, pct), setDeviceMuted(id, muted),
  -                   setStreamVolume(id, pct), setStreamMuted(id, muted),
  -                   moveStreamToOutput(sid, sinkId), moveStreamToInput(sid, sourceId)
       |
       +----------> AudioDeviceModel  (QAbstractListModel)
       |               roles: id, name, description, volume, muted, isDefault
       |               emits dataChanged() per device row
       |
       +----------> AudioStreamModel  (QAbstractListModel)
                      roles: id, name, application, volume, muted, currentDevice
                      emits dataChanged() per stream row
                                 |
                                 v
                           QML bindings
                     AudioWidget.qml (existing, no changes)
                     AudioPopup.qml  (future, new feature after M4)
```

`AudioTypes.h` (no Q_OBJECT) defines the shared data structs:

```
struct AudioDevice { uint32_t id; QString name; QString description;
                     uint8_t volume; bool muted; bool is_default; };
struct AudioStream  { uint32_t id; QString name; QString application;
                      uint32_t device; uint8_t volume; bool muted; };
enum class AudioDeviceType { Sink, Source };
```

No `pa_*` type appears in any public header outside `PulseAudioBackend.h/.cpp`.

---

## 5. Migration Plan Per Milestone

### M1a — Create `src/app/` and `ShellApplication` skeleton

**Files to create:**
- `src/app/ShellApplication.h` (class declaration with all methods and private members)
- `src/app/ShellApplication.cpp` (empty method stubs)

**CMake changes:**
- Add `src/app/ShellApplication.h` and `src/app/ShellApplication.cpp` to `holonight_core` source list
- Add `${CMAKE_CURRENT_SOURCE_DIR}/src/app` to `target_include_directories(holonight_core PUBLIC ...)`

**Verification before M1b:**
- `task build` passes with empty stubs
- `task tidy` passes (no method bodies to lint yet)

**Risks:** None — additive only.

---

### M1b — Move service construction logic to `ShellApplication`

**Files modified:**
- `src/app/ShellApplication.cpp` — implement constructor (all `new` calls only)
- `src/main.cpp` — remove all `new` calls and wiring; replace with `ShellApplication shell(&app)`

**Key concern — constructor order:** The constructor must mirror the exact construction order from `main.cpp` to preserve initialization dependencies:
1. `WorkspaceModel` first (no deps)
2. `ExtWorkspaceManager` (needs `WorkspaceModel*`)
3. `HyprlandWorkspaceService` (needs `WorkspaceModel*`)
4. `KeyboardLayoutService` (no deps)
5. All remaining services (no inter-service deps in ctors)
6. `TrayWatcher` last (needs `TrayModel*`)
7. No dependency wiring in the constructor; `tray_model_->setMenuSurface(tray_menu_surface_)` is deferred to `startServices()`

**Verification before M1c:**
- `task build` passes
- `task run` (in Wayland session): all constructed services are valid; full async startup verification happens after M1c
- Constructor destruction order is correct (child objects destroyed in reverse construction order automatically via Qt parent chain)

**Risks:** Qt parent chain ownership — all services receive `this` (ShellApplication) as parent, so they are destroyed when `ShellApplication` is destroyed. `ShellApplication` must outlive all `QQmlEngine` instances. Since `ShellApplication` is stack-allocated in `main()` after `QGuiApplication`, the destruction order is: `LayerShellManager` (stack, first destroyed) → `QGuiApplication::exec()` returns → `shell` destroyed → engines destroyed by engine shutdown. Verify this order does not cause use-after-free.

---

### M1c — Move QML type registration and `startShell()` to `ShellApplication`

**Files modified:**
- `src/app/ShellApplication.cpp` — implement `registerQmlTypes()` (no parameter) and `startShell()` (creates `LayerShellManager` via `unique_ptr`)
- `src/main.cpp` — remove all `qmlRegisterSingletonType` calls and the `LayerShellManager lsm(...)` line; replace with `shell.registerQmlTypes(); shell.startServices(); shell.startShell();`

**Why no `QQmlEngine*` parameter.** `qmlRegisterSingletonType` is a process-wide global registry (see DESIGN §4.2). It applies to every `QQmlEngine` created afterwards. Passing an engine would imply per-engine semantics that the underlying API does not provide.

**Ordering invariant.** `registerQmlTypes()` MUST be called before any `QQuickView` is created. `startShell()` (which creates the views) asserts `registered_ == true` to enforce this.

**Verification before M1d:**
- `task build` passes
- `task qml-lint` passes (the `QML_ELEMENT`/`QML_SINGLETON` headers must remain visible to qmltyperegistrar)
- In Wayland session: all QML singletons resolve, no "type not found" errors

**Risks:** If `registerQmlTypes()` is accidentally moved after `startShell()`, the assertion fires. Catch this at build time with the `assert()` in `startShell()`.

---

### M1d — Redistribute all `src/` files to domain subdirectories

**Files to move (by domain):**

To `src/platform/`:
```
LayerShell.h, LayerSurface.h/.cpp, HyprlandIpc.h/.cpp
HyprlandIpcClient.h/.cpp, DbusPropertyClient.h/.cpp
```

To `src/core/`:
```
WorkspaceModel.h/.cpp, ExtWorkspaceManager.h/.cpp, HyprlandWorkspaceService.h/.cpp
KeyboardLayoutService.h/.cpp, BatteryState.h/.cpp, AudioState.h/.cpp
SystemInfo.h/.cpp
```

To `src/services/`:
```
ActiveWindowService.h/.cpp, BatteryService.h/.cpp, AudioService.h/.cpp
NetworkService.h/.cpp, SessionService.h/.cpp
SystemInfoService.h/.cpp, ThemeService.h/.cpp
```

To `src/surfaces/`:
```
LayerShellManager.h/.cpp, PopupSurface.h/.cpp, TooltipSurface.h/.cpp, TrayMenuSurface.h/.cpp
TrayModel.h/.cpp, TrayItem.h/.cpp, TrayItemProperties.h/.cpp
TrayWatcher.h/.cpp, DbusMenuItem.h/.cpp, DbusMenuClient.h/.cpp
IconImageProvider.h
```

**CMake changes:**
- Update all file paths in `holonight_core` source list to include subdirectory prefixes
- Add `${CMAKE_CURRENT_SOURCE_DIR}/src/platform`, `/src/core`, `/src/services`, `/src/surfaces` to `target_include_directories(holonight_core PUBLIC ...)`

**Include path updates:**
- Every `#include "FileName.h"` that crosses a domain boundary must be updated to a relative path (e.g., `#include "WorkspaceModel.h"` → `#include "../core/WorkspaceModel.h"`) OR the include directories must cover all subdirs (simpler approach: add all four subdirs as PUBLIC include dirs on `holonight_core`, which mirrors the current flat behavior). The simpler approach is correct for M1 — include path encapsulation is enforced in M2 via separate targets.

**Verification:**
- `task build` passes — this is the critical check; any missed `#include` update fails the build
- `task test` passes — tests use the same headers; ensure test `CMakeLists.txt` points to new paths
- `task qml-lint` passes — QML does not reference C++ paths directly, so this is a sanity check
- `find /home/andrii/projects/holonight-shell/src -maxdepth 1 -name "*.cpp" -o -name "*.h"` returns only `main.cpp`

**Risks:** Missed `#include` updates. Mitigation: move all files first (git mv), attempt build, fix each compile error in order. Do not manually edit includes before the move — let compiler errors guide the fixes.

---

### M2a — Define `holonight_platform` CMake target

**CMake changes:**
- In `CMakeLists.txt`: add `add_library(holonight_platform STATIC ...)` with all platform files
- Move `qt6_generate_wayland_protocol_client_sources()` to target `holonight_platform`
- Add `target_link_libraries(holonight_platform PUBLIC Qt6::WaylandClient wayland-client Qt6::Gui Qt6::Core Qt6::DBus)`
- Add `target_include_directories(holonight_platform PUBLIC src/platform ${CMAKE_CURRENT_BINARY_DIR})`

**Verification:**
- `task build` — holonight_platform compiles standalone
- Generated headers (e.g., `qwayland-wlr-layer-shell-unstable-v1.h`) are in `${CMAKE_BINARY_DIR}` and accessible to dependent targets

**Risks:** Generated protocol headers are placed in `${CMAKE_BINARY_DIR}` by the wayland scanner. `target_include_directories(holonight_platform PUBLIC ${CMAKE_CURRENT_BINARY_DIR})` propagates this to all consumers. Verify the propagation works for the other targets before proceeding.

---

### M2b–M2d — Define remaining targets and refactor `holonight_core`

**CMake changes (five C++ static libraries):**
- `holonight_core` narrowed to `src/core/` only; includes `ExtWorkspaceManager`; PUBLIC link `holonight_platform`, `Qt6::Core`, `Qt6::Qml`, `Qt6::DBus`, `tomlplusplus`
- `holonight_services`: `src/services/` files; PUBLIC link `holonight_core`, `holonight_platform`, `Qt6::Network`, `Qt6::Concurrent`, `libpulse`, `Qt6::Qml`
- `holonight_surfaces`: `src/surfaces/` files, including `LayerShellManager`; PUBLIC link `holonight_platform` only (no `holonight_services` — verified by grep)
- `holonight_app`: `src/app/ShellApplication.{h,cpp}` plus app-local coordinators; PUBLIC link all four other libs
- Executable `holonight-shell`: `src/main.cpp` + `qt6_add_qml_module(holonight-shell URI HolonightShell VERSION 1.0 QML_FILES ...)` + PRIVATE link `holonight_app`
- Optional: `qt6_add_resources(holonight-shell "assets" PREFIX "/HolonightShell" BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets" FILES ...)` for non-QML asset bundle

**Test re-linking (M2d).** Replace the current omnibus `holonight_tests` executable with module-level test executables. Each test file, or a small group of tests in the same module, links the minimal set of new targets:
- `test_workspace_model`, `test_ext_workspace_manager`, `test_hyprland_workspace_service`, `test_audio_state`, `test_battery_state`, `test_system_info` → `holonight_core`
- `test_active_window_service`, `test_audio_service`, `test_battery_service`, `test_network_service` → `holonight_services`
- `test_tray_item`, `test_tray_item_properties`, `test_tray_model`, `test_tray_watcher`, `test_dbusmenu_client`, `test_layer_shell_manager` → `holonight_surfaces`
- `test_hyprland_ipc`, `test_hyprland_ipc_client` → `holonight_platform`
- `test_qml_smoke` → executable target or a dedicated QML smoke-test target that owns/imports the QML module

**Coverage instrumentation.** Update `ENABLE_COVERAGE` block to add `target_compile_options/target_link_options(... PRIVATE --coverage)` for all five static libs and the executable.

**Verification:**
- `cmake --graphviz=deps.dot build/` — inspect graph; must be acyclic
- `task build` — single binary produced
- `cp build/holonight-shell build/holonight-shell.sizecheck && strip --strip-debug build/holonight-shell.sizecheck && ls -l build/holonight-shell.sizecheck` — within 5% of T-000 debug-stripped baseline (same machine, same flags)
- `task test` — all tests pass; sanity check by removing one target link from one module-level test executable and confirming that executable fails to link
- `task qml-lint` — clean, and `build/HolonightShell/holonight-shell.qmltypes` contains the expected C++ type names; `Module {}` alone is a failure
- Manual Wayland smoke test

**Risks:**
- If `qt6_add_qml_module` is accidentally moved off the executable, or the executable-owned QML module is not given the QML-facing C++ headers, `qmltyperegistrar` can produce empty qmltypes while runtime registration still works. The fix is always: keep `qt6_add_qml_module` on the executable and feed it the relevant headers without double-compiling implementation files.
- Generated `qwayland-*.h` headers must propagate from `holonight_platform`'s `${CMAKE_CURRENT_BINARY_DIR}` PUBLIC include to all consumers.

---

### M3 — README audio description correction (runs in parallel with M1a)

**Sequencing.** This is a single-line documentation fix with zero deps on the C++ refactor. It is sequenced to run alongside M1a so the README inaccuracy doesn't persist through the risky milestones.

**Files modified:**
- `README.md` line 17 — `| Audio | Volume level + muted state via PulseAudio D-Bus |` → `| Audio | Volume level + muted state via PipeWire/PulseAudio-compatible control (libpulse) |`
- `README.md` ~line 79 — replace "...PulseAudio..." with "...PipeWire or PulseAudio (via libpulse)..." for consistency

**Verification:**
- `git diff README.md` shows two-line modification (lines 17 and ~79); no other content changes

**Risks:** None.

---

### M4a — Define `AudioTypes.h` and `PulseAudioBackend`

**Files to create:**
- `src/services/audio/AudioTypes.h` — `AudioDevice`, `AudioStream`, `AudioDeviceType`
- `src/services/audio/PulseAudioBackend.h/.cpp` — wraps all `pa_*` calls; no `pa_*` in public header

**Key design constraint:** `PulseAudioBackend` must not expose `pa_context*`, `pa_sink_info*`, or any `pa_*` type in its public header. The header includes only `<QObject>`, `<cstdint>`, and `AudioTypes.h`.

**CMake changes:**
- Add new files to `holonight_services` source list

**Verification:** `task build`, `task tidy`

---

### M4b — Implement `AudioDeviceModel` and `AudioStreamModel`

**Files to create:**
- `src/services/audio/AudioDeviceModel.h/.cpp`
- `src/services/audio/AudioStreamModel.h/.cpp`

**Verification:** Unit tests for model `rowCount()`, `data()`, and `dataChanged()` emission.

---

### M4c — Extend `AudioService`

**Files modified:**
- `src/services/audio/AudioService.h/.cpp` (moved from `src/services/`)
- `src/core/AudioState.h/.cpp` remains in place and is not moved

**Verification:**
- Existing `AudioWidget.qml` still functions with no QML changes
- New properties and invokables are registered and accessible from QML introspection

---

### M4d — Audio tests

**Files to create:**
- `tests/test_audio_device_model.cpp`
- `tests/test_audio_stream_model.cpp`
- `tests/test_pulse_audio_backend.cpp` (uses a mock `pa_context` or test-double)

**Verification:** `task test` — all audio tests pass

---

### M5 — Test audit and gap fill

**Audit approach:**
1. Run `task coverage` to generate HTML report
2. Identify methods in `src/core/`, `src/services/`, `src/platform/` with 0% branch coverage
3. Filter to methods with >10 lines and >1 branch (complexity threshold from SPEC REQ-F-502)
4. For each gap: add a `TEST(ClassName, DescriptiveName)` in the corresponding test file

**Verification:**
- `task test` passes 100% with no flakes
- Coverage delta: each new test increases line coverage for its target class

---

## 6. Key Decisions with Rationale

### Why `ShellApplication` instead of a service locator pattern

A service locator (global registry keyed by type name or ID) would allow any code in any translation unit to retrieve any service without explicit dependency injection. This would make it impossible to reason about which component uses which service, and would undermine the goal of establishing clear domain boundaries.

`ShellApplication` instead makes dependencies explicit at construction time. `main.cpp` owns one `ShellApplication` instance. Every component that needs a service receives it via constructor parameter. No global state is introduced beyond what `qmlRegisterSingletonType` already provides for QML.

`ShellApplication` does **not** expose service accessors. All services are private members; external code reaches them only through their QML singleton registrations or through the `start*()` methods. `LayerShellManager` is constructed inside `startShell()` so its `TrayModel*` argument never crosses the public interface.

### Why no `ServiceRegistry` helper class (v1.1 revision)

v1.0 of this design proposed an optional `ServiceRegistry` namespace/class to hold the 13 `qmlRegisterSingletonType` lambdas. v1.1 drops it. The 13 lambdas live inline inside `ShellApplication::registerQmlTypes()` — ~30 lines, repetitive but trivially scannable. A separate class would add indirection without reducing the boilerplate.

### Why five targets instead of two or ten (v1.1 revision)

Two targets (`holonight_core` + executable) is the current state. It fails to encode any domain boundary — the linker resolves every symbol from every file, so nothing prevents `AudioService.cpp` from including `LayerSurface.h`. Circular includes are discovered only when they cause compile errors, not at design time.

Ten or more targets (one per class or one per logical service) would create excessive CMake boilerplate, make `add_library` dependency graphs hard to read, and provide no practical benefit. Grouping by domain (platform, core, services, surfaces, app) maps directly to the intended dependency hierarchy and produces exactly the encapsulation needed.

v1.0 proposed six targets including a separate `holonight_qml` static-library for QML files and assets. That was dropped in v1.1 because `qt6_add_resources()` (the proposed mechanism) does not run `qmltyperegistrar`, so the C++ `QML_ELEMENT` macros in service/surface headers would lose their QML-type registration. Keeping `qt6_add_qml_module(holonight-shell ...)` on the executable is the only way to make qmltyperegistrar continue to see those headers across all the new static libs. Hence five C++ static libs + executable-owned QML module.

### Why keep one binary (not split into daemons)

Splitting services into separate daemon processes (e.g., a Wayland surface daemon, a D-Bus aggregator daemon, an IPC bridge) would require designing an IPC protocol between them, add latency to all inter-service calls, and complicate startup ordering. It would also break the current QML architecture which depends on in-process C++ singletons.

The holonight-shell use case (a desktop shell bar) does not benefit from inter-process isolation. All services are trusted code running under the same user. A single binary with clear internal module boundaries achieves the same organizational goals with none of the operational overhead.

### Why audio expansion must precede popup feature work

The audio popup (pavucontrol-like UI) requires `AudioDeviceModel`, `AudioStreamModel`, and the new `AudioService` Q_PROPERTYs to exist before any QML for it can be written. If popup QML is developed against the old `AudioService` API (single volume property), it will need to be partially rewritten when M4 lands. Mandating M4 completion first eliminates this rework entirely.

There is also a structural risk: if popup QML imports new audio types that do not yet exist in the C++ layer, `qmllint` and the QML type system will produce false-positive errors during development, making it harder to tell real bugs from missing-type errors.

### Why test audit is a separate milestone (not bundled into M1)

Bundling test gap-fill with the M1 file reorganization would intermix two types of changes: structural moves (no logic change) and new test logic. This makes code review harder — reviewers cannot easily verify that no logic change occurred during M1 if new test code is mixed in.

Separating M5 also allows the audit to proceed without blocking the structural work. The initial inventory can run in parallel with M1-M3, while final gap classification is refreshed after each refactor milestone so findings match the current directory and target layout.

---

## 7. Alternatives Considered

### Alternative: Extract services into a separate daemon process

Each major service (BatteryService, AudioService, NetworkService, etc.) could be compiled into a separate helper daemon. The main `holonight-shell` process would communicate with these daemons via Unix sockets, D-Bus, or a custom IPC protocol.

**Rejected because:**
- Requires designing and maintaining an IPC protocol for each service boundary
- Increases startup time (each daemon must start and register before the shell is functional)
- Complicates error handling: the shell must handle daemon crashes and reconnects for services it currently trusts unconditionally
- QML singletons would become proxy objects wrapping IPC calls instead of direct C++ bindings — all property change notifications become async, making the QML binding model significantly more complex
- No reliability or security benefit for a trusted single-user shell process

### Alternative: Use Qt's plugin system for service modules

Each domain could be a Qt plugin (`.so`), loaded dynamically by the shell at startup. This would enforce binary ABI boundaries between modules.

**Rejected because:**
- Qt plugin ABI requires stable vtable layouts across plugin boundaries — C++23 template-heavy code (QWaylandClientExtensionTemplate, etc.) is not ABI-stable across independent compilations
- Plugin discovery and load-order management would add complexity equivalent to the daemon approach
- Debug information becomes fragmented across multiple `.so` files, making stack traces harder to read
- The goal is clean internal boundaries, not runtime replaceability. CMake targets provide compile-time boundary enforcement without runtime overhead
- Wayland protocol-generated code (`qwayland-*.h`) would need to be shared across plugin boundaries or duplicated, creating protocol version mismatch risks

### Alternative: Merge `app` and `services` CMake targets

`holonight_app` and `holonight_services` could be merged into a single `holonight_app_and_services` target, since `ShellApplication` directly instantiates all services.

**Rejected because:**
- Merging them would allow service implementation files to include `ShellApplication.h`, enabling circular dependencies (a service calling back into the application layer)
- The separation enforces that services are dumb domain objects that know nothing about their orchestrator — they accept dependencies via constructor parameters only, never by querying the application object
- Testing services independently requires that they can be instantiated without `ShellApplication`. Keeping the targets separate makes this constraint explicit at the CMake level

---

## 8. Known Risks

### Circular include risk during file moves (M1d)

**Risk:** During M1d, a file in `src/services/` may include a header from `src/surfaces/` (or vice versa), which currently compiles because all files share the same include directory. After the move with updated include paths, the compiler will still accept this cross-domain include — the CMake target separation in M2 is the only gate that enforces the boundary.

**Mitigation:** Before M2, manually audit all `#include` directives in the moved files. Flag any include that crosses the intended domain boundary. Verify each cross-domain include is legitimate (e.g., `TrayModel` including `TrayMenuSurface` is a surfaces-to-surfaces include, which is fine) versus accidental (e.g., a service including a surface header).

Use `grep -r "PopupSurface\|TooltipSurface\|TrayMenuSurface" src/services/` to find any service files that improperly reference surface types. The current codebase does not appear to have this issue, but it must be confirmed after M1d.

### QML type registration breakage risk (M1d, M2b)

**Risk:** The `qt6_add_qml_module(holonight-shell URI HolonightShell ...)` invocation is the only thing that runs `qmltyperegistrar` for the `HolonightShell` module. If this invocation is accidentally moved off the executable target, or if the executable-owned QML module is not given the C++ headers that carry `QML_ELEMENT` (`AudioService.h`, `WorkspaceModel.h`, …), `qmltyperegistrar` can produce empty qmltypes while runtime `qmlRegisterSingletonType` calls still make the shell appear to work.

**Decision (v1.1).** The `qt6_add_qml_module` invocation stays on the executable throughout all milestones. There is no `holonight_qml` static library. This is captured in REQ-F-206.

**Mitigation:**
- Run `task qml-lint` after every CMake change in M2.
- Inspect `build/HolonightShell/holonight-shell.qmltypes` after every QML/CMake change. It must contain the expected C++ type names; `Module {}` is a failure even if `task qml-lint` is clean.
- If static-library linkage alone does not populate qmltypes, pass QML-facing headers to `qt6_add_qml_module(holonight-shell ... SOURCES ...)` while keeping `.cpp` implementation files compiled only in their owning static libraries.
- Verify by inspection: `qt6_add_qml_module(holonight-shell ...)` appears exactly once in `CMakeLists.txt` and names the executable as the first argument.
- Non-QML assets bundle via a separate `qt6_add_resources(holonight-shell "assets" ...)` call — not via the QML module call.

### Binary size regression risk (M2d)

**Risk:** Splitting into five static libraries may cause symbol duplication if Qt headers emit inline code into each compilation unit, inflating the final binary size beyond the 5% threshold.

**Measurement convention.** The 5% gate is applied to a **temporary copy** of the debug build after running `strip --strip-debug` on the copy. T-000 captures one number (debug-stripped size) on `main` before M1a starts; the same number is captured again at M2 acceptance using the same build flags on the same machine. Release builds are not gated — the refactor does not change release-build code paths, and the debug-stripped measurement is what reflects shipped object-file size most cleanly. Do not strip `build/holonight-shell` in place during validation.

**Mitigation.** If size grows >5% after M2:
1. First, confirm symbol duplication via `nm --print-size --size-sort build/holonight-shell | tail -50` and look for two symbols with the same demangled name.
2. If duplication is confirmed, audit `target_link_libraries` PUBLIC/PRIVATE keywords — duplicated symbols usually mean an inline body is being compiled into two static libs because both depend on the same header without re-using the upstream lib.
3. Only as a last resort, enable LTO via `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`. LTO is **not** a default because it changes debug-build link times noticeably and complicates debugger stack traces. Document any LTO toggle as a separate roadmap item.

**No runtime perf gate.** This refactor introduces no new code paths, no new threads, and no service-init reordering. A startup-time or FPS regression gate was considered and rejected as flaky/low-value for a pure structural refactor. M5 acceptance smoke-test is the runtime regression check.

### Wayland protocol generation duplication risk (M2a)

**Risk:** If `qt6_generate_wayland_protocol_client_sources()` is invoked on `holonight_platform` but the generated headers are not properly propagated to `holonight_core` and `holonight_surfaces` (which both include generated Wayland headers), those targets will fail to find `qwayland-wlr-layer-shell-unstable-v1.h` etc.

**Mitigation:** Ensure `holonight_platform` uses `PUBLIC` include directories so `${CMAKE_CURRENT_BINARY_DIR}` (where generated headers land) propagates to all consumers. Verify with a clean build (`task clean && task configure && task build`) that no "file not found" errors appear for generated headers.

Do not invoke `qt6_generate_wayland_protocol_client_sources()` on any target other than `holonight_platform` (REQ-C-201). If a dependent target cannot find a generated header, the fix is to adjust include propagation, not to add a second invocation.

### Audio API backwards compatibility risk (M4)

**Risk:** The refactored `AudioService` in M4 may subtly change the behavior of the existing `volume`, `muted`, and `available` properties if the `PulseAudioBackend` extraction is not a pure refactor. For example, the PA subscription callback chain currently writes directly to `AudioService` member variables. After extraction, signals from `PulseAudioBackend` must arrive on the Qt main thread (since `pa_threaded_mainloop` runs on a separate thread) — failure to marshal to the main thread will cause property updates to emit `NOTIFY` signals from the wrong thread, breaking QML bindings.

**Mitigation:** Keep `PulseAudioBackend` responsible for thread marshalling — it should use `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` or emit signals via `Qt::QueuedConnection` to ensure all state updates arrive on the main thread. Add a test that verifies `volumeChanged()` and `mutedChanged()` are emitted on the main thread. The existing `test_audio_service.cpp` should verify this behavior does not regress.
