# Design: POC Readiness Review — Architecture Review & Review Unit Inventory

| Field | Value |
|---|---|
| Document ID | POC-Readiness-Review/DESIGN.md |
| Version | 1.0 (SDD Stage 2 — Design) |
| Companion | SPEC.md (same directory) |
| Status | Ready for Stage 3 (Unit-by-Unit Analysis) |
| Next Stage | Stage 3 — apply `qt-cpp-review` / `qt-qml-review` to each unit below, sequentially, with a human checkpoint after each |

---

## 0. Scope & File-Count Reconciliation

SPEC.md's executive summary cites "~438 C++/QML source files." A direct count confirms and decomposes this figure:

| Scope | File count | In REQ-F-1 partition? |
|---|---|---|
| `libs/*` (C++ only) | 232 | Yes |
| `apps/*` (C++ + QML) | 129 | Yes |
| **Subtotal: `libs/` + `apps/`** | **361** | **Yes — this is the 100%-coverage partition target** |
| `tests/*` (C++ + QML test harness) | 75 | No — covered as a narrative cross-cutting concern per the Stage 2 brief, not a review unit |
| `qml/HoloNight/*` (shared component plugin, repo root, outside `libs/`/`apps/`) | 2 | No — outside REQ-F-1's literal scope (neither `libs/` nor `apps/`); noted in §1 as an adjacent dependency of both apps |
| **Total (matches SPEC's ~438 estimate)** | **438** | — |

All review-unit accounting below is against the **361-file `libs/`+`apps/` partition**, per REQ-F-1's acceptance criterion ("Every source directory in libs/ and apps/ appears in exactly one unit"). `qml/HoloNight/` (currently 2 files: `Components/ContentSeparator.qml`, `Components/ExternalIcon.qml`, plus its `CMakeLists.txt`) is built as `holonight_componentsplugin` and linked by both `holonight-shell` and `holonight-settings` — it is architecturally relevant (see §1.4) but is intentionally excluded from the unit checklist because it sits outside the two directories REQ-F-1 names.

---

## 1. Architecture & Component Review

### 1.1 Layering

The codebase is a clean, strictly-layered dependency graph enforced at the CMake level (verified against every `libs/*/CMakeLists.txt` and `apps/*/CMakeLists.txt`):

```
holonight_platform   (Wayland/Hyprland/D-Bus transport primitives — depends on nothing internal)
       ^
holonight_config     (toml++ parsing/writing of on-disk config — depends on nothing internal)
       ^
holonight_core       (shared models/state: WorkspaceModel, BatteryState, AudioState, SystemInfo,
                       Logger, ConfigService — PUBLIC-links config + platform)
       ^
holonight_services   (14 documented subsystem subdirectories PLUS 16 additional classes living
                       directly at libs/holonight-services/src/ root — PUBLIC-links core + platform)
       ^
holonight_surfaces   (layer-shell surface orchestration, tray, launcher/sidebar/notification/
                       widget presentation — PUBLIC-links core + platform + services)
       ^
holonight_app        (apps/shell/app/: ShellApplication composition root + ControlServer —
                       PUBLIC-links all four libraries above)
       ^
holonight-shell       (executable; apps/shell/main.cpp; owns qt6_add_qml_module for all shell QML)
```

`apps/settings` is a **separate, parallel** executable (`holonight-settings`) that links only `holonight_config` (for shared TOML structs) plus an **external** package `HolonightQt::Config`/`HolonightQt::Theme` (a sibling project, `holonight-qt`, found via `find_package(HolonightQt REQUIRED)` — not part of this repo) and the shared `holonight_componentsplugin`. It does **not** link `holonight_core`/`services`/`surfaces`/`app` at all — it is architecturally isolated from the shell's runtime service graph, reading/writing the same on-disk TOML config the shell consumes.

This is a genuinely well-maintained layering. Every static-lib `CMakeLists.txt` declares its dependency edges explicitly and consistently as `PUBLIC`; there is no evidence of a lower layer reaching upward (e.g., `holonight_platform` including a `holonight_services` header). One intentional, narrow exception is documented and *tooled*: `holonight_surfaces` links `holonight_services` PUBLIC for "narrow presentation orchestration" — enforced at commit time by `scripts/check-architecture-boundaries.sh`, which greps every `#include "..."` under `libs/holonight-surfaces/src/` and fails the build if it names any `holonight-services` header **other than** the two explicitly allow-listed (`MonitorOccupancyService.h`, `NotificationService.h`). This passed cleanly when re-run during this review (`task architecture-check` equivalent, exit 0). This is a strong, unusual-for-a-POC discipline: an architectural rule is enforced by a script wired into `task architecture-check`, not just documented in prose.

### 1.2 Technology Stack

- **Qt6/C++23** throughout; `CMAKE_CXX_STANDARD 23` at the top level, `cxx_std_23` propagated via `target_compile_features(... PUBLIC cxx_std_23)` on every static lib.
- **Wayland**: `wlr-layer-shell-unstable-v1`, `ext-workspace-v1`, `ext-idle-notify-v1`, and `xdg-shell` protocol XML compiled via `wayland-scanner` + `qtwaylandscanner` in the top-level `CMakeLists.txt` (custom fallback function is defined inline when the Qt-provided CMake macro isn't found — a real Qt6/distro packaging compatibility hazard the project has already had to work around).
- **D-Bus is the dominant integration mechanism**: UPower (battery), NetworkManager, `org.freedesktop.UPower.PowerProfiles`/`net.hadess.PowerProfiles` (power profiles), `org.freedesktop.Notifications` (the shell **is** the notification daemon, not just a consumer — `NotificationServer` claims the bus name), `org.kde.StatusNotifierWatcher`/`com.canonical.dbusmenu` (tray), `org.freedesktop.portal.*` (XDG desktop portal), logind (`org.freedesktop.login1`), libsecret (credential storage for CalDAV). CLAUDE.md documents numerous hard-won D-Bus gotchas (QDBusArgument read-mode traps, `SpecificObject` vs `Devices`, logind session resolution via `loginctl show-seat` shell-out rather than the D-Bus calls that assert-crash) — these are real, previously-hit landmines, not speculative concerns.
- **libpulse** (direct C API, not PipeWire-pulse D-Bus) wrapped behind `PulseAudioBackend`/`PulseAudioSystem`, called from a `pa_threaded_mainloop` and marshalled back to the Qt main thread via `Qt::QueuedConnection` (per `PulseAudioBackend.h` doc comment).
- **SQLite via Qt6::Sql** for the launcher desktop-entry cache and the calendar cache (`CalendarCache`), each using UUID-named per-thread `QSqlDatabase` connections per the project's memory notes.
- **tomlplusplus** (`pkg_check_modules(TOMLPLUSPLUS REQUIRED tomlplusplus)`) for all on-disk shell/theme configuration, wrapped by `holonight_config`'s free-function parsers/writer (`ConfigParsers.cpp`, `ConfigWriter.cpp`) rather than a class hierarchy.
- **libsecret** for CalDAV credential storage (`LibsecretCredentialStorage`).
- **GTest + QtQuickTest/CTest** for testing; a custom `FakeQmlServices` harness globally registers mock singletons for the QML test engine (`test_holonight_qml_harness`).

### 1.3 Cross-Cutting Patterns Actually Observed

1. **Abstract backend + concrete/Null implementation, injected via constructor.** This is the single most consistent pattern in the codebase, applied uniformly across independently-developed subsystems: `SessionBackend` (abstract; `HyprlandSessionBackend`/`LogindSessionBackend` concrete, `CommandRunner`+`ProcessEnvironment` as further test seams), `BrightnessBackend` (abstract; `SysfsBackend`/`NullBrightnessBackend`), `IdleBackend` (abstract; `ExtIdleNotifyBackend`/`NullIdleBackend`), `IPortalDBus` (abstract, injected into `PortalService` via a second constructor overload), `ICalendarProvider` (abstract; `CalDavProvider`/`IcsProvider`). Every one of these ships a `Null*`/fake implementation used both as a graceful runtime fallback (e.g., no backlight device present) *and* as the test-injection seam — the same abstraction serves both purposes, which is an efficient, deliberate design choice rather than incidental duplication.
2. **QML singleton facade over a service.** Nearly every service header carries `QML_ELEMENT` + `QML_SINGLETON` and is registered by hand in `ShellApplication::registerQmlTypes()` via a generic `reg(obj, name)` lambda that calls `QQmlEngine::setObjectOwnership(CppOwnership)` + `qmlRegisterSingletonType<T>("HolonightShell", 1, 0, name, [obj](...) { return obj; })`. As of this pass, `ShellApplication::registerQmlTypes()` inline-registers **29** singletons this way, plus `WeatherIconBridge` registered separately (it needs a real per-engine factory since it derives from `QDateTime`-based state, not a shared instance) and `SidebarManager`, which is registered later, inside `startLayerSurfaces()`, because it requires a live `LayerShell&` not available at construction time. This mirrors exactly what the `arch-restructure-roadmap` DESIGN.md (§2.3, `registerQmlTypes()`) specified as the resolution to that cycle's C2 finding (global-registry semantics, idempotency via a `registered_` bool) — the pattern has held up and scaled from 13 singletons at that time to 29+ today without drift.
3. **D-Bus protocol adapter classes that own no state.** `NotificationServer` (`org.freedesktop.Notifications`) and `TrayWatcher` (`org.kde.StatusNotifierWatcher`) both follow the same shape: a thin `QObject`+`QDBusContext` class with `Q_CLASSINFO("D-Bus Interface", ...)`, PascalCase slots/signals matching the wire protocol (explicitly `NOLINT`-annotated against the project's camelCase convention), and all real logic delegated to a paired `*Service`/`*Model` object. `NotificationServer.h`'s doc comment explicitly says it "Mirrors the TrayWatcher protocol-adapter pattern" — this is a *named*, intentionally-repeated pattern, not a coincidence.
4. **Composition root owns everything; DI is manual constructor injection.** `ShellApplication` (`apps/shell/app/ShellApplication.h/.cpp`) is the single composition root: ~40 owned members (raw `QObject*` parented to `this`, plus a handful of `std::unique_ptr` for objects that must control teardown order relative to `LayerShell`), constructed in one long member-initializer list, wired together with three explicit lifecycle phases (`registerQmlTypes()` → `startServices()` → `startShell()`), each idempotency-guarded by a private bool. There is no `ServiceRegistry`, no DI container, no reflection — this matches the arch-restructure-roadmap's explicit decision (§6, "Why no `ServiceRegistry` helper class") to keep wiring inline and readable rather than add an indirection layer for a single-binary shell.
5. **Layer-shell surface lifecycle is a first-class, repeatedly-reused concern.** `PerMonitorLayerManager` is a shared base for `LayerShellManager` (topbar), `BackgroundManager` (wallpaper), and `WidgetManager` (desktop widgets) — three otherwise-independent presentation features reuse the same per-monitor `QQuickView`+`LayerSurface` creation/teardown machinery. CLAUDE.md's "Layer-Shell Surface Hiding" and "SingleShotConnection race on `configured()`" gotchas describe subtle, previously-hit bugs in exactly this shared code path — a strength (shared machinery, not copy-pasted per feature) with a corresponding sharp edge (a bug here affects three features at once).
6. **Activity-gate fan-out for power-sensitive background work.** `IActivityGate` is implemented by `CalendarService`, `WeatherService`, and `SuspendInhibitorService`, all registered with a single `ActivityGateManager` that fans out lid-state changes (`LidStateMonitor`) to every gate. This is a real, working mediator pattern — one of the few places in the codebase where an explicit interface + registry replaces ad hoc pairwise wiring (contrast with pattern 4, where most wiring *is* ad hoc pairwise `connect()` calls in `ShellApplication`).
7. **`ConfigService` as the shared mutable read/write surface.** Multiple otherwise-unrelated services (`ExtWorkspaceManager`, `AppearanceService`, `WeatherService`, `TrayModel`, `WidgetManager`'s definitions, `SidebarManager` indirectly) all take a `ConfigService*` constructor argument rather than reading TOML independently — config plumbing is centralized, not duplicated per subsystem, unlike (per the SPEC's own cross-cutting-issue example) a scenario where "all services implement their own caching strategy." This specific anti-pattern does *not* appear to be present for configuration; whether it is present for other cross-cutting concerns (caching, retry/backoff) is a Stage 3/4 question — see §5.

### 1.4 Known Strengths Worth Preserving

- **The CMake target boundaries are real, not aspirational.** Five static libraries with consistent `PUBLIC`/`PRIVATE` link discipline, one automated boundary-check script, and a documented, narrow, intentional exception. This is uncommon rigor for a "pre-production POC" per SPEC.md's own framing, and Stage 4 should be careful not to recommend re-flattening it.
- **The abstract-backend/test-seam pattern (§1.3.1) is applied with real consistency**, not just in one or two flagship subsystems — it appears independently in session, brightness, idle, portal, and calendar code that plausibly weren't developed by the same "session" of work (different SDD feature cycles: `session-lock-backend`, `brightness-service`, `idle-management`, `portal-service`, `calendar` cycles are all separate `docs/sdd/` directories). Convergent reuse of the same idiom across independently-scoped features suggests the pattern is either well-documented internally or genuinely idiomatic to whoever authored these — either way, it's worth explicitly codifying (e.g., in CLAUDE.md) rather than leaving it as tacit convention, since a remediation pass could easily "clean up" one instance into a different shape and fragment the convention.
- **The QML singleton registration mechanism has scaled cleanly** from 13 (at the arch-restructure-roadmap's snapshot) to 29+ today with the same idiom and no signs of ad hoc exceptions — a genuine validation that the earlier restructuring cycle's design decision was sound.
- **CLAUDE.md itself is an architectural asset.** Dozens of narrow, empirically-verified gotchas (Wayland surface hiding, D-Bus read-mode traps, per-monitor `Screen.name` breakage, QML `required property` per-Loader-branch trap) are captured centrally rather than being tribal knowledge or re-discovered per feature. Stage 3/4 reviewers should cross-check candidate findings against this file before flagging something as a defect — several plausible-looking "bugs" (e.g., a `QProcess` shell-out instead of a "proper" D-Bus call for `ActiveSession`) are *documented, deliberate workarounds* for real Qt/dbus/libdbus defects, not oversights.

---

## 2. Prior Restructuring Status

`docs/sdd/arch-restructure-roadmap/` (SPEC/DESIGN/REVIEW-FINDINGS/TASKS, dated 2026-05-27/28) documents a completed restructuring from a flat, two-target `src/` (`holonight_core` STATIC + `holonight-shell` executable, 58 files) into the five-static-library layout this review now audits. **All 29 tasks in `TASKS.md` are checked `[x]`; zero unchecked items remain** — this cycle fully landed, not partially.

Cross-checking DESIGN.md's final (v1.3) target-architecture claims against the current tree confirms a faithful landing on every major point:

| Roadmap decision (DESIGN.md v1.1–v1.3) | Current reality | Landed? |
|---|---|---|
| C1: no separate `holonight_qml` target; `qt6_add_qml_module` stays on the executable | `apps/shell/CMakeLists.txt` calls `qt6_add_qml_module(holonight-shell ...)` directly on the executable; no `holonight_qml` target exists anywhere | ✅ Yes |
| C2: `registerQmlTypes()` — no-parameter, `registered_`-bool idempotency guard | `ShellApplication::registerQmlTypes()` matches exactly, including the guard | ✅ Yes |
| C3: `LayerShellManager` ownership moves into `ShellApplication::startShell()`; no public service accessors | `LayerShellManager` is `std::make_unique`'d inside `ShellApplication::startShell()`; `ShellApplication.h` exposes no accessor for it | ✅ Yes |
| M2/M6: coverage instrumentation covers all five static libs + `holonight_app` | Root `CMakeLists.txt`'s `ENABLE_COVERAGE` block iterates exactly `holonight_config holonight_platform holonight_core holonight_services holonight_surfaces holonight_app` | ✅ Yes |
| M8: `SystemInfo` (free functions) lives in `core/`; `SystemInfoService` (QML adapter) lives in `services/` | `libs/holonight-core/src/SystemInfo.{h,cpp}` + `libs/holonight-services/src/SystemInfoService.{h,cpp}` — exactly this split | ✅ Yes |
| M9: `Qt6::Qml` added to `holonight_core` PUBLIC deps | `libs/holonight-core/CMakeLists.txt` links `Qt6::Qml` PUBLIC | ✅ Yes |
| M1: `AudioState` stays in `core/` permanently; `AudioTypes.h` lives in `services/audio/` | `libs/holonight-core/src/AudioState.{h,cpp}` + `libs/holonight-services/src/audio/AudioTypes.h` — matches | ✅ Yes |

**One internal inconsistency in the roadmap's own documents, now resolved in favor of the more detailed text — worth flagging so it isn't mistaken for current-code drift.** The roadmap's v1.1 revision-history bullet says C4 "Dropped the `holonight_surfaces → holonight_services` link edge" entirely. But DESIGN.md's own §2.2 dependency graph and §3.4 component description (which read as a later, more carefully reasoned pass than the terse revision-history bullet) instead describe a **narrowed, tooled** dependency: `holonight_surfaces` links `holonight_services` PUBLIC for "narrow presentation orchestration," explicitly gated by `scripts/check-architecture-boundaries.sh`'s allow-list. **Current code matches the detailed §2.2/§3.4 text, not the terse v1.1 bullet** — `libs/holonight-surfaces/CMakeLists.txt` links `holonight_services` PUBLIC, and the boundary script currently allow-lists exactly two headers (`MonitorOccupancyService.h`, `NotificationService.h`), which passes with zero violations today. This is CLAUDE.md's documented intentional exception, not an undocumented regression. Stage 4 should **not** re-flag "surfaces depends on services" as a fresh finding — it is a known, tooled, and deliberately narrow exception, already caught by CI-equivalent tooling (`task architecture-check`).

**What has changed since the roadmap's snapshot (and is *not* covered by that roadmap's scope, since it predates ~35 subsequent SDD feature cycles):** `holonight_surfaces` has grown from the roadmap's documented 11 files (`LayerShellManager`, `PopupSurface`→now `StatusPopupSurface`+`TooltipSurface`+`TrayMenuSurface`, `TrayModel`/`TrayItem`/`TrayItemProperties`/`TrayWatcher`, `DbusMenuItem`/`DbusMenuClient`, `IconImageProvider`) to **50 files today**, adding `BackgroundManager`, `LauncherSurface`+`LauncherSurfaceLifecycle`, `NotificationManager`+`NotificationToastSurface`, `PerMonitorLayerManager`, `SidebarManager`+`SidebarSurfacePolicy`, `StatusPopupGeometry`+`PopupGeometry`, and `WidgetClock`+`WidgetCountdown`+`WidgetManager`+`WidgetSurfacePolicy`. `libs/holonight-services/src/` has similarly grown well beyond the roadmap's 7-file "services" domain list to 150 files across 14 subdirectories plus 16 root-level classes. None of this growth is a "roadmap violation" — the roadmap only committed to a *target structure*, and new features have consistently landed inside the five established targets, at the correct layer, following the established patterns (§1.3). The roadmap's job (fix the target boundaries) is done; the subsequent growth is ordinary feature work built correctly on top of it.

`docs/sdd/project-structure-apps-libs/DESIGN.md` (a separate, later SDD cycle) documents the final `apps/`+`libs/` directory convention (as opposed to the flatter `src/` layout the arch-restructure-roadmap operated on) — this document's description matches the current tree exactly, directory-for-directory (`apps/shell/qml/Topbar/TopBar.qml` → `qrc:/HolonightShell/Topbar/TopBar.qml`, etc.), confirming this second, smaller migration also fully landed.

---

## 3. Review Unit Inventory

11 units (within the required 10–14 range). Average file count: 361 ÷ 11 ≈ 32.8, within the 31–44/unit target band from REQ-NF-1. Two units fall slightly under the 20-file soft floor and carry explicit justification per REQ-NF-1; none exceed 60.

| ID | Name | Files | C++? | QML? | Tooling |
|---|---|---|---|---|---|
| U-01 | Foundational Core, Platform & Config | 32 | Yes | No | `qt-cpp-review` |
| U-02 | Presentation Surfaces & Shell Composition Root | 55 | Yes | No | `qt-cpp-review` |
| U-03 | Power, Idle & Brightness Management | 35 | Yes | No | `qt-cpp-review` |
| U-04 | Session, Window Identity, Network & Audio Services | 36 | Yes | No | `qt-cpp-review` |
| U-05 | Desktop Integration, Appearance & Portal | 18 | Yes | No | `qt-cpp-review` |
| U-06 | Launcher & Notifications Services | 30 | Yes | No | `qt-cpp-review` |
| U-07 | Calendar & Weather Services | 31 | Yes | No | `qt-cpp-review` |
| U-08 | Topbar QML | 27 | No | Yes | `qt-qml-review` |
| U-09 | RightSidebar + Popups QML | 47 | No | Yes | `qt-qml-review` |
| U-10 | Overlay Content & Shared QML Building Blocks | 30 | No | Yes | `qt-qml-review` |
| U-11 | Settings Application | 20 | Yes | Yes | Both |
| | **Total** | **361** | | | |

### U-01 — Foundational Core, Platform & Config (32 files)

**Purpose**: the bottom two layers of the dependency graph — Wayland/Hyprland/D-Bus transport primitives and shared, service-agnostic models/state.

**Paths**:
- `libs/holonight-platform/src/` (9 files: `LayerShell.h`, `LayerSurface.{h,cpp}`, `HyprlandIpc.{h,cpp}`, `HyprlandIpcClient.{h,cpp}`, `DbusPropertyClient.{h,cpp}`)
- `libs/holonight-core/src/` (18 files: `ConfigService.{h,cpp}`, `WorkspaceModel.{h,cpp}`, `ExtWorkspaceManager.{h,cpp}`, `HyprlandWorkspaceService.{h,cpp}`, `KeyboardLayoutService.{h,cpp}`, `BatteryState.{h,cpp}`, `AudioState.{h,cpp}`, `SystemInfo.{h,cpp}`, `Logger.{h,cpp}`)
- `libs/holonight-config/include/holonight_config/` + `libs/holonight-config/src/` (5 files: `config_parsers.h`, `config_structs.h`, `config_writer.h`, `ConfigParsers.cpp`, `ConfigWriter.cpp`)

**Rationale**: `holonight_core` depends on both `holonight_platform` and `holonight_config` and nothing depends *below* either — this is the acyclic floor of the graph. Reviewing them together surfaces the transport→model translation boundary (e.g., `ExtWorkspaceManager`/`HyprlandWorkspaceService` both drive the same `WorkspaceModel` from two different transports) in one pass.

### U-02 — Presentation Surfaces & Shell Composition Root (55 files)

**Purpose**: layer-shell surface lifecycle/orchestration (bars, popups, tray, launcher, sidebar, notifications, desktop widgets, background) plus the top-of-stack composition root that constructs and wires all of it.

**Paths**:
- `libs/holonight-surfaces/src/` (50 files, flat — no subdirectories)
- `apps/shell/app/` (4 files: `ControlServer.{h,cpp}`, `ShellApplication.{h,cpp}`)
- `apps/shell/main.cpp` (1 file)

**Rationale**: `ShellApplication::startShell()`/`startLayerSurfaces()` directly `new`/`make_unique`s `LayerShellManager`, `BackgroundManager`, `SidebarManager`, and `WidgetManager` — all four defined in `holonight_surfaces` — making the composition root's most architecturally load-bearing code inseparable from the surfaces it wires. At 55 files this is the largest unit; still under the 60-file ceiling. Reviewing `ShellApplication`'s ~40-member construction/teardown-order invariants (documented in the class's own comments — see the `layer_shell_`/`layer_shell_manager_` destruction-order comment) alongside the surface classes those invariants protect is more useful than reviewing wiring code divorced from what it wires.

### U-03 — Power, Idle & Brightness Management (35 files)

**Purpose**: the activity-gate/idle/suspend/lid/battery/backlight/power-profile cluster — everything that decides "is the system awake, should it stay awake, and what power state is it in."

**Paths**:
- `libs/holonight-services/src/ActivityGateManager.{h,cpp}`, `IActivityGate.h`, `LidStateMonitor.{h,cpp}`, `LowBatteryMonitor.{h,cpp}`, `SuspendInhibitorService.{h,cpp}`, `InhibitorModel.{h,cpp}`, `BatteryService.{h,cpp}`, `PowerProfilesService.{h,cpp}` (13 root-level files)
- `libs/holonight-services/src/idle/` (12 files)
- `libs/holonight-services/src/brightness/` (8 files)

**Rationale**: `ActivityGateManager` fans out `LidStateMonitor::lidStateChanged` to every registered `IActivityGate` (`CalendarService`, `WeatherService`, `SuspendInhibitorService` — the first two live in U-07, cross-referenced there); `IdleService` and `ScreenSaverAdaptor` are wired together in `ShellApplication::startServices()`; `LowBatteryMonitor` directly consumes `BatteryService`. This is the codebase's most interdependent small-service cluster and matches the "small interdependent services grouped together" grouping axis; `PowerProfilesService` (a sibling D-Bus power concern, CLAUDE.md-documented) and `brightness/` (a related but not code-coupled power concern) are folded in for topical cohesion and to reach a healthy unit size.

### U-04 — Session, Window Identity, Network & Audio Services (36 files)

**Purpose**: "what session am I in, which window/monitor has focus, what network am I on, what's my audio state" — QML-facing OS/session-identity facades, each backed by its own abstract-backend seam.

**Paths**:
- `libs/holonight-services/src/SessionService.{h,cpp}` + `libs/holonight-services/src/session/` (2 + 12 = 14 files: `CommandRunner`, `HyprlandSessionBackend`, `Locker`, `LogindSessionBackend`, `ProcessEnvironment`, `SessionBackend`)
- `libs/holonight-services/src/ActiveWindowService.{h,cpp}`, `MonitorOccupancyService.{h,cpp}` (4 files)
- `libs/holonight-services/src/SystemInfoService.{h,cpp}` (2 files)
- `libs/holonight-services/src/NetworkService.{h,cpp}` + `libs/holonight-services/src/network/` (2 + 4 = 6 files: `NetworkManagerBackend`, `WifiNetworkModel`)
- `libs/holonight-services/src/audio/` (10 files: `AudioDeviceModel`, `AudioService`, `AudioStreamModel`, `AudioTypes.h`, `PulseAudioBackend`, `PulseAudioSystem.h`)

**Rationale**: `MonitorOccupancyService` directly includes `ActiveWindowService.h` and `WorkspaceModel.h` (per-monitor focus tracking, the exact mechanism CLAUDE.md's "Per-Monitor Active Window" section documents); `SessionService` is a thin façade over the `session/` adapter subsystem (documented in project memory as "SessionBackend Architecture — the template for all desktop-integration pipelines"); `NetworkService`/`AudioService` are architecturally the same shape (QML singleton + backend adapter over a native/D-Bus API) even though topically distinct from session — grouped here as the remaining mid-sized "hardware/session status" services rather than left as several sub-10-file fragments.

### U-05 — Desktop Integration, Appearance & Portal (18 files — below the 20-file soft floor; justified)

**Purpose**: XDG desktop-portal bridging, system appearance (color-scheme/accent-color/theme constants), and diagnostic-only KDE/mime desktop-entry compatibility shims.

**Paths**:
- `libs/holonight-services/src/AppearanceService.{h,cpp}`, `ThemeService.{h,cpp}` (4 files)
- `libs/holonight-services/src/portal/` (6 files: `NullPortalBackend`, `PortalService`, `SettingsPortalBackend`)
- `libs/holonight-services/src/kde-compat/` (2 files)
- `libs/holonight-services/src/mime/` (2 files)
- `libs/holonight-services/src/session-integration/` (4 files: `ApplicationCacheRebuilder`, `SessionIntegrationService`)

**Justification for <20 files**: `ThemeService.cpp` includes `SettingsPortalBackend.h` directly (theme constants are portal-sourced), and `AppearanceService`/`ThemeService`/`PortalService` correspond one-to-one to four related SDD feature cycles (`system-appearance-portal`, `appearance-service-split`, `theme-conf-switcher`, `theme-variant-catalog`, `portal-service`). Splitting this cluster further would fragment a single coherent "how does the shell find out what the desktop wants it to look like, and what portal/compat shims does that require" concern into pieces smaller than a cohesive review pass; `kde-compat`/`mime`/`session-integration` are diagnostic-only, low-file-count desktop-integration siblings folded in rather than left as three separate 2–4-file fragments elsewhere.

### U-06 — Launcher & Notifications Services (30 files)

**Purpose**: the two largest "standalone" service subsystems — app-launcher (desktop-entry scan/cache/model/command execution) and the notification daemon (the shell **is** `org.freedesktop.Notifications`, not merely a client).

**Paths**:
- `libs/holonight-services/src/launcher/` (14 files: `CategoryMapper.h`, `DesktopEntryCache`, `DesktopEntryScanner`, `DesktopEntrySerializer.h`, `LauncherCommand`, `LauncherModel`, `LauncherService`, `RecentAppsTracker`)
- `libs/holonight-services/src/notifications/` (16 files: `NotificationFilter`, `NotificationPolicy`, `NotificationRuleModel`, `NotificationRuleStore`, `NotificationServer`, `NotificationService`, `NotificationStore`, `NotificationTypes`)

**Rationale**: per the grouping guidance's own example ("larger standalone ones like launcher/portal/notifications/calendar possibly solo or paired"), these two are paired rather than split further: both are the codebase's largest single-subsystem services, both maintain SQLite-or-file-backed persistent state independent of `ConfigService`, and `SessionIntegrationService` (U-05) explicitly triggers a `launcher_->reload()` callback after desktop-cache rebuilds, giving the two a light but real coupling reason to review back-to-back.

### U-07 — Calendar & Weather Services (31 files)

**Purpose**: the two external-data, activity-gated background sync services — CalDAV/ICS calendar sync and OpenWeatherMap-backed weather, both feeding topbar/sidebar widgets.

**Paths**:
- `libs/holonight-services/src/CalendarService.{h,cpp}` + `libs/holonight-services/src/calendar/` (2 + 16 = 18 files: `CalDavProvider`, `CalendarCache`, `CalendarEventModel`, `CalendarSyncManager`, `CalendarTypes.h`, `ICalendarProvider.h`, `ICalParser`, `IcsProvider`, `LibsecretCredentialStorage`)
- `libs/holonight-services/src/weather/` (5 files: `WeatherData.h`, `WeatherProvider`, `WeatherService`)
- `libs/holonight-services/src/weather-icon/` (8 files: `MoonPhase.h`, `MoonPhaseCalculator`, `WeatherIconBridge`, `WeatherIconLayer.h`, `WeatherIconMapper`)

**Rationale**: both `CalendarService` and `WeatherService` implement `IActivityGate` and register with the same `ActivityGateManager` (U-03) — they are cross-referenced there, not duplicated here. `weather-icon/` is included with `weather/` rather than with QML (despite compositing PNG layers for on-screen display) because, per its own module documentation, it is "Qt6::Core only, no QML/GUI deps" except for the one `WeatherIconBridge` adapter file — it is a C++ mapping/calculation library, not QML.

### U-08 — Topbar QML (27 files)

**Purpose**: the always-on top bar — workspace pills, active window title, clock, weather/battery/audio/network widgets, tray section, logo.

**Paths**: `apps/shell/qml/Topbar/` (27 files: `TopBar.qml`, `ActiveWindowSection.qml`, `AppWindowIcon.qml`, `AudioWidget.qml`, `BarBackground.qml`, `BarSection.qml`, `BarSeparator.qml`, `BarTooltipArea.qml`, `BatteryIndicator.qml`, `BatteryWidget.qml`, `ClockSection.qml`, `KeyboardLayoutWidget.qml`, `LogoSection.qml`, `NetworkWidget.qml`, `NotificationsWidget.qml`, `PrecipitationGraph.qml`, `SpecialWorkspaceDot.qml`, `StatusesSection.qml`, `StatusPopupTriggerArea.qml`, `TemperatureGraph.qml`, `WeatherIconRoles.qml`, `WeatherSection.qml`, `WeatherWidget.qml`, `WorkspaceEdgeArrow.qml`, `WorkspacePill.qml`, `WorkspacePillStrip.qml`, `WorkspaceSection.qml`)

**Rationale**: the single largest and most cohesive QML UI region — every file renders inside the persistent, always-mapped topbar layer-shell surface, sharing `barMonitorName`-based per-monitor routing (CLAUDE.md's "Per-Monitor Active Window" propagation chain runs entirely through this directory).

### U-09 — RightSidebar + Popups QML (47 files)

**Purpose**: the two families of on-demand, layer-shell overlay QML surfaces that the topbar/sidebar trigger — the full-height right sidebar (with its six tabs) and every status/detail popup.

**Paths**:
- `apps/shell/qml/RightSidebar/` including `Tabs/` (19 files: `RightSidebar.qml`, `SidebarContent.qml`, `SidebarTabBar.qml`, `SidebarTabButton.qml`, plus `Tabs/{Calendar,Media,Notifications,Overview,QuickSettings,System}/`)
- `apps/shell/qml/Popups/` including all seven subdirectories (28 files: `Audio/` 8, `Battery/` 2, `Network/` 7, `Status/` 2, `Tooltip/` 1, `Tray/` 1, `Weather/` 7)

**Rationale**: matches the grouping guidance's explicit example ("Sidebar+Popups together"). Both are create-on-open `QQuickView`s managed by C++ counterparts in U-02 (`SidebarManager`, `StatusPopupSurface`, `TooltipSurface`, `TrayMenuSurface`) and share the dismiss-overlay/animation-teardown patterns CLAUDE.md documents ("Full-screen overlay input region," "SingleShotConnection race on `configured()`"). At 47 files this is the largest QML unit but stays under the 60-file ceiling; splitting Sidebar from Popups was considered but rejected because several popups (e.g., `Popups/Weather/`) are opened *from* sidebar tab content, not just from the topbar, so the two overlay families are not cleanly independent.

### U-10 — Overlay Content & Shared QML Building Blocks (30 files)

**Purpose**: the launcher palette, notification toast stack, tray context-menu content, and the shared leaf components/desktop-widget QML with no other natural home.

**Paths**:
- `apps/shell/qml/Launcher/` (7 files)
- `apps/shell/qml/Notifications/` (4 files: `ToastActionBar`, `ToastActionButton`, `ToastItem`, `ToastStack`)
- `apps/shell/qml/Tray/` (4 files: `TrayItem`, `TrayMenuDismissOverlay`, `TrayMenuItem`, `TraySection`)
- `apps/shell/qml/Controls/` (8 files: `AudioIcon`, `BarIcon`, `BatteryIcon`, `HudFrame`, `NetworkIcon`, `SessionIcon`, `SignalStrengthIcon`, `UtilityIcon`)
- `apps/shell/qml/Widgets/` (4 files: `ClockWidget`, `StableDigitsText`, `TimeToEventWidget`, `WidgetSurface`)
- `apps/shell/qml/Background/` (1 file: `Background.qml`)
- `apps/shell/qml/Utility/` (1 file: `ThemeReloadBridge.qml`)
- `apps/shell/qml/WeatherIcon/` (1 file: `WeatherIconCompositor.qml`)

**Rationale**: this deliberately **merges** the two smaller example groupings offered in the Stage 2 brief ("Launcher+Notifications+Tray QML together" and "shared Widgets/Background/Controls/Utility/WeatherIcon together" — 15 files each) into one 30-file unit. Kept separate, each would sit at 15 files, well under even a relaxed floor, and reviewing eight nearly-empty directories in two passes yields less signal than one pass over "everything that isn't the always-on topbar or an on-demand sidebar/popup overlay." This is the one unit-boundary judgment call in this design that most diverges from the brief's example groupings; flagged here explicitly per REQ-NF-1's deviation-justification requirement. Note `apps/shell/qml/WeatherIcon/WeatherIconCompositor.qml` is the QML consumer of U-07's `weather-icon/` C++ mapper — the two are in different units (C++/QML axis takes precedence per the grouping guidance) but should be reviewed with awareness of each other; per the Weather Icon Compositor project memory, this component is presently **not wired into any production screen** (the topbar still uses the older SVG path), which Stage 3/4 should treat as a known, deliberate state, not a defect.

### U-11 — Settings Application (20 files)

**Purpose**: the standalone `holonight-settings` binary — a small, architecturally separate GUI for editing the same on-disk TOML config the shell reads.

**Paths**:
- `apps/settings/src/` (13 files: `ConfigFileService`, `FontListModel`, `main.cpp`, `SettingsApplication`, `SettingsEditModel`, `ShellStatusService`, `ThemeConfigFile`)
- `apps/settings/qml/` (7 files: `SettingsWindow.qml`, `NavPanel.qml`, `ContentStack.qml`, `PlaceholderPage.qml`, `FooterBar.qml`, `AppearancePage.qml`, `BarPage.qml`)

**Rationale**: a complete, self-contained application (own `main.cpp`, own QML window, own `SettingsEditModel` current/snapshot dirty-tracking pattern per project memory) that happens to be small — reviewed as a single unit covering both its C++ and QML rather than artificially split. It is architecturally isolated from the shell's five-library service graph (see §1.1), so nothing is lost by not co-locating it with any shell unit.

---

## 4. Full Directory Coverage Checklist

Every subdirectory under `libs/` and `apps/` that contains source files, mapped to exactly one unit. Container directories that hold only a `CMakeLists.txt` (no source files of their own) are listed once for completeness and marked "(container)."

### `libs/`

| Directory | Unit |
|---|---|
| `libs/holonight-config/` (container) | U-01 |
| `libs/holonight-config/include/holonight_config/` | U-01 |
| `libs/holonight-config/src/` | U-01 |
| `libs/holonight-core/` (container) | U-01 |
| `libs/holonight-core/src/` | U-01 |
| `libs/holonight-platform/` (container) | U-01 |
| `libs/holonight-platform/src/` | U-01 |
| `libs/holonight-services/` (container) | — (split; see subdirs below) |
| `libs/holonight-services/src/` (root-level files only, not recursive) | split: `ActiveWindowService.*`, `MonitorOccupancyService.*` → U-04; `SessionService.*` → U-04; `SystemInfoService.*` → U-04; `NetworkService.*` → U-04; `ActivityGateManager.*`, `IActivityGate.h`, `LidStateMonitor.*`, `LowBatteryMonitor.*`, `SuspendInhibitorService.*`, `InhibitorModel.*`, `BatteryService.*`, `PowerProfilesService.*` → U-03; `AppearanceService.*`, `ThemeService.*` → U-05; `CalendarService.*` → U-07 |
| `libs/holonight-services/src/audio/` | U-04 |
| `libs/holonight-services/src/brightness/` | U-03 |
| `libs/holonight-services/src/calendar/` | U-07 |
| `libs/holonight-services/src/idle/` | U-03 |
| `libs/holonight-services/src/kde-compat/` | U-05 |
| `libs/holonight-services/src/launcher/` | U-06 |
| `libs/holonight-services/src/mime/` | U-05 |
| `libs/holonight-services/src/network/` | U-04 |
| `libs/holonight-services/src/notifications/` | U-06 |
| `libs/holonight-services/src/portal/` | U-05 |
| `libs/holonight-services/src/session/` | U-04 |
| `libs/holonight-services/src/session-integration/` | U-05 |
| `libs/holonight-services/src/weather/` | U-07 |
| `libs/holonight-services/src/weather-icon/` | U-07 |
| `libs/holonight-surfaces/` (container) | U-02 |
| `libs/holonight-surfaces/src/` | U-02 |

### `apps/`

| Directory | Unit |
|---|---|
| `apps/settings/` (container) | U-11 |
| `apps/settings/qml/` | U-11 |
| `apps/settings/src/` | U-11 |
| `apps/shell/` (container: `main.cpp`, `version.h.in`) | U-02 (for `main.cpp`) |
| `apps/shell/app/` | U-02 |
| `apps/shell/qml/` (container) | — (split; see subdirs below) |
| `apps/shell/qml/Background/` | U-10 |
| `apps/shell/qml/Controls/` | U-10 |
| `apps/shell/qml/Launcher/` | U-10 |
| `apps/shell/qml/Notifications/` | U-10 |
| `apps/shell/qml/Popups/` (container) | U-09 |
| `apps/shell/qml/Popups/Audio/` | U-09 |
| `apps/shell/qml/Popups/Battery/` | U-09 |
| `apps/shell/qml/Popups/Network/` | U-09 |
| `apps/shell/qml/Popups/Status/` | U-09 |
| `apps/shell/qml/Popups/Tooltip/` | U-09 |
| `apps/shell/qml/Popups/Tray/` | U-09 |
| `apps/shell/qml/Popups/Weather/` | U-09 |
| `apps/shell/qml/RightSidebar/` (container) | U-09 |
| `apps/shell/qml/RightSidebar/Tabs/` (container: `Calendar/`, `Media/`, `Notifications/`, `Overview/`, `QuickSettings/`, `System/`) | U-09 |
| `apps/shell/qml/Topbar/` | U-08 |
| `apps/shell/qml/Tray/` | U-10 |
| `apps/shell/qml/Utility/` | U-10 |
| `apps/shell/qml/WeatherIcon/` | U-10 |
| `apps/shell/qml/Widgets/` | U-10 |

**Arithmetic check** (REQ-NF-1): U-01 32 + U-02 55 + U-03 35 + U-04 36 + U-05 18 + U-06 30 + U-07 31 + U-08 27 + U-09 47 + U-10 30 + U-11 20 = **361**, matching the independently-counted `libs/`+`apps/` total exactly. No directory is omitted; no directory is double-assigned (the only directories listed against more than one unit are `libs/holonight-services/src/` and `apps/shell/qml/`, both of which are explicitly split *by file*, not by directory, with every constituent file's unit stated in §3's per-unit path lists). **Coverage: 100%. No blocker per REQ-NF-2.**

---

## 5. Preliminary Architecture-Level Observations

These are pre-Stage-4 flags — things noticed in this pass that deserve extra attention during deep unit review, not findings in their own right (no severity/effort/location triage is attached; that is Stage 4's job per REQ-F-5/F-6).

1. **`ShellApplication`'s single 40+ member constructor is a concentration-of-risk point, not yet a proven defect.** The member-initializer list in `ShellApplication::ShellApplication` constructs services in a specific order with implicit ordering dependencies (e.g., `occupancy_` is constructed from `model_` and `aws_`, which must already exist; `notification_service_` needs `notification_rule_model_`). This is currently correct (it compiles and, per CLAUDE.md's testing notes, the shell runs), but it is exactly the shape of code where a future contributor adding one new service in the wrong list position silently breaks initialization order with no compiler diagnostic. Worth a focused correctness pass in U-02, specifically checking every cross-reference between adjacent initializer entries against declaration order in the header.

2. **`holonight_surfaces`'s narrow `holonight_services` dependency is exactly two headers today (`MonitorOccupancyService.h`, `NotificationService.h`) — worth checking this hasn't silently grown.** The allow-list in `scripts/check-architecture-boundaries.sh` is a manually maintained array; nothing prevents a future PR from adding a third service include and simply extending the array rather than reconsidering whether the orchestration belongs in `surfaces` at all. Stage 3's review of U-02 should specifically audit whether `MonitorOccupancyService`/`NotificationService` usage inside `holonight-surfaces/src/` still reads as "narrow presentation orchestration" or has grown into something that should move.

3. **Two independent per-monitor layer-shell surface families (`SidebarManager` and the `PerMonitorLayerManager`-based `WidgetManager`/`LayerShellManager`/`BackgroundManager`) implement create-on-open vs. always-mapped lifecycles differently**, per CLAUDE.md's explicit warning that hiding a manually-role-bound layer-shell surface with `QQuickView::hide()` breaks it permanently. `SidebarManager` creates-on-open and destroys-on-close (`destroySurface`); the `PerMonitorLayerManager` family keeps the surface mapped and toggles QML-root `visible`. Both are individually documented as correct for their use case, but a reviewer unfamiliar with *why* they differ could "simplify" one to match the other and reintroduce the exact bug class CLAUDE.md warns about. Flag for Stage 4: confirm this dual-lifecycle split is called out explicitly wherever it's reviewed, so a "consistency" recommendation doesn't silently break one of the two surfaces.

4. **The `holonight-services/src/` root-level file group (16 classes, 31 files, spread across U-03/U-04/U-05/U-07) is not a directory in its own right** — it is a flat pile of files sitting alongside 14 well-organized subdirectories. This is itself worth a Stage 4 architecture-gap candidate: nothing structurally prevents `ActiveWindowService`, `BatteryService`, `NetworkService`, etc., from each having been given their own subdirectory the way `audio/`, `brightness/`, and `calendar/` were. The inconsistency (some services get a subdirectory, some don't, with no evident rule for which) is a real, verifiable pattern — not a per-file defect, but a directory-organization convention that's been applied inconsistently across ~16 SDD feature cycles.

5. **`ThemeService`'s properties were documented (arch-restructure-roadmap AUDIT.md) as "All properties are `CONSTANT` string/int literals; trivial accessors,"** yet `ThemeService.cpp` now includes `SettingsPortalBackend.h` — worth Stage 3 confirming whether `ThemeService` is still the trivial-constant class the 2026-05 audit described, or whether it has grown live portal-sourced state that the "trivial, no test needed" framing from that audit no longer fits (a doc-drift candidate per REQ-F-2/REQ-NF-4, against a document in a *different* SDD cycle than the one currently being read for a given unit — Stage 3 reviewers should check the appearance/portal-related SDD docs, not just arch-restructure-roadmap, when reviewing U-05).

6. **Two SDD feature directories (`weather-icon-compositor`, `hn-icon`/`exact-hn-icon-adoption`) explicitly document components that are deliberately not wired into production** (`WeatherIconCompositor.qml`'s standalone status is documented project memory; a Stage 4 reviewer should distinguish "unused component, by design, documented" from "dead code, a maintainability finding" — the SPEC's REQ-C-4/confidence-threshold discipline should apply here specifically, since an over-eager qt-qml-review pass could flag an intentionally-standalone component as unreachable/dead code at high confidence when it is neither.

7. **Testing coverage is uneven and was last formally audited in 2026-05-28** (`docs/sdd/arch-restructure-roadmap/AUDIT.md`), covering 205 tests across 20 files as of that date, with several classes explicitly marked "untestable without a live Wayland compositor/D-Bus/PA session" and one (`KeyboardLayoutService`, at the time) marked as the highest-priority coverage gap. This audit is now roughly five weeks and ~35 SDD feature cycles old; the services and surfaces added since (launcher, notifications, calendar, sidebar, widgets, portal, power extensions — the bulk of U-02/U-03/U-05/U-06/U-07) are **not** covered by that audit at all. Stage 4's synthesis should treat current test coverage as substantially unknown for anything outside the original `holonight_core`/`holonight_platform` audio/tray/workspace surface, rather than extrapolating the 2026-05 audit's relatively healthy picture forward.

---

*End of DESIGN.md — proceed to Stage 3 (Unit-by-Unit Analysis), starting with U-01 per REQ-NF-5's sequential-execution requirement, pausing for human checkpoint after each unit per REQ-NF-3.*
