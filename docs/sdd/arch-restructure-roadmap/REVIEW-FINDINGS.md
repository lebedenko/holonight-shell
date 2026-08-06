# Arch Restructure Roadmap — Review Findings

**Reviewer:** assistant
**Date:** 2026-05-27
**Scope:** `docs/project-architecture-review.md` + `docs/sdd/arch-restructure-roadmap/{SPEC,DESIGN,TASKS}.md`, cross-checked against the current `src/`, `CMakeLists.txt`, `main.cpp`, and `README.md`.

Findings are ranked **Critical → Major → Minor**. Each entry has a concrete suggested edit.

---

## V1.3 FOLLOW-UP RESOLUTIONS

The v1.2 roadmap fixed the original findings below, but a second consistency pass found additional implementation gaps. These are now resolved in SPEC/DESIGN/TASKS v1.3:

- `ExtWorkspaceManager` is assigned to `src/core/` / `holonight_core`, not `platform`, because it includes and mutates `WorkspaceModel`.
- `LayerShellManager` is assigned to `src/surfaces/` / `holonight_surfaces`, not `platform`, because it creates `QQuickView` instances and consumes `TrayModel`/image providers.
- `holonight_platform` is explicitly low-level only and must not include core, services, surfaces, or app headers.
- Generated QML metadata is now validated directly: `build/HolonightShell/holonight-shell.qmltypes` must contain exported HolonightShell C++ types; `Module {}` is a failure even if runtime singleton registration works.
- The test split now requires module-level test executables. One omnibus `holonight_tests` executable cannot prove target boundaries.
- `PulseAudioBackend`, `AudioService`, and TASKS now list the same device/stream control API, including default input, device mute, stream volume/mute, and input/output stream moves.
- `AudioState` stays in `src/core/` permanently; stale M4 move text was removed.
- `ShellApplication` constructor is construction-only. `tray_model_->setMenuSurface(...)` is wired in `startServices()` before async service starts.
- Binary-size validation strips a temporary copy (`build/holonight-shell.sizecheck`) instead of mutating `build/holonight-shell`.
- M5 audit work is explicitly parallel/incremental with M1-M3, not strictly after M4.

---

## CRITICAL

### C1. The QML registration story is broken — `QML_ELEMENT` + `qt6_add_qml_module` are invisible to the design

**Observation.** Every C++ singleton in the codebase carries both `QML_ELEMENT` and `QML_SINGLETON` in its header (`AudioService.h`, `WorkspaceModel.h`, `ThemeService.h`, `BatteryService.h`, `NetworkService.h`, `SessionService.h`, `SystemInfoService.h`, `KeyboardLayoutService.h`, `ActiveWindowService.h`, `PopupSurface.h`, `TooltipSurface.h`, `TrayMenuSurface.h`, `TrayModel.h`, `DbusMenuItem.h`). The current `CMakeLists.txt` calls `qt6_add_qml_module(holonight-shell URI HolonightShell VERSION 1.0 QML_FILES ...)` on the **executable**. This runs `qmltyperegistrar` over the executable's sources so the C++ `QML_ELEMENT` types end up in `HolonightShell` qmltypes — that is what makes `import HolonightShell 1.0` resolve in qmllint and at runtime.

**Why this matters for the plan.**
- REQ-F-206 / DESIGN §3.5: `holonight_qml` is a static lib created via `qt6_add_resources()` with prefix `/HolonightShell/` and "Defines no C++ code; exists only to organize resources." But `qt6_add_resources()` does **not** generate qmltypes and does **not** read `QML_ELEMENT` macros. Replacing `qt6_add_qml_module` with `qt6_add_resources` will silently drop the C++→QML type registration; qmllint will start reporting `[unqualified]` for `WorkspaceModel`, `AudioService`, etc. The runtime will still work *only* because of the `qmlRegisterSingletonType<T>(...)` lambdas in `main.cpp` — but the static type system breaks.
- DESIGN §5 (M2b–M2c, line ~757) and §8 (line ~923) note this risk and suggest a fallback: "keep `qt6_add_qml_module(holonight-shell ...)` attached to the executable target". That is **incompatible** with REQ-F-206. If both `qt6_add_qml_module` (executable) and `qt6_add_resources` (`holonight_qml`) bundle the same `src/qml/*.qml` files under prefix `/HolonightShell/`, the build either fails on duplicate QRC aliases or one silently shadows the other.
- The qmltyperegistrar pass needs to see the C++ headers (`AudioService.h`, etc.). If those headers move to a separate `holonight_services` STATIC target, then `qt6_add_qml_module(holonight-shell ...)` on the executable won't see them unless the `.h`/`.cpp` files are also listed there — but then they're compiled twice.

**Suggested edits.**
1. **Replace** REQ-F-206 with: "`holonight_qml` is **not** an independent target. The existing `qt6_add_qml_module(holonight-shell ...)` invocation stays on the executable. `holonight_qml` is dropped from the target list, and the executable target gets explicit `QML_FILES` + `SOURCES` linkage to the C++ targets carrying `QML_ELEMENT`."
2. **Alternative** (more invasive): keep `holonight_qml` but make it a true QML module via `qt6_add_qml_module(holonight_qml URI HolonightShell ... QML_FILES ... SOURCES ...)` and pass it the list of headers containing `QML_ELEMENT`. Then the executable links `holonight_qml`. This requires `qt6_policy(SET QTP0001 NEW)` and careful handling of the QML plugin export macros — much higher risk, not worth it for the size of this codebase.
3. **Update DESIGN §8** (QML import path breakage risk) to call out the qmltyperegistrar dependency explicitly, not just generically as a "fallback."
4. **Update DESIGN §2.2 dependency diagram** to remove `holonight_qml` as an internal target, OR explicitly state that `holonight_qml` only bundles non-QML assets from `assets/` (icons/fonts), and that QML files stay under `qt6_add_qml_module` on the executable.

---

### C2. `registerQmlSingletons(QQmlEngine*)` signature is wrong

**Observation.** REQ-F-102 and DESIGN §2.3 declare `void registerQmlSingletons(QQmlEngine* engine);`. The current `main.cpp` uses `qmlRegisterSingletonType<T>("HolonightShell", 1, 0, ...)` — this is a **global** registry, not engine-scoped. DESIGN §4.2 (lines 552-554) actually says this correctly: "Because `qmlRegisterSingletonType` uses the global type registry, all engines resolve the same C++ singleton instance via the lambda. This is why the factory-callback form is used instead of `qmlRegisterSingletonInstance` (which binds to exactly one engine)."

So the parameter is dead weight. Worse: REQ-F-102 acceptance criterion says "Calling the method twice on the same engine does not cause duplicates or errors." `qmlRegisterSingletonType` is global; calling it twice **does** cause warning-level diagnostics about overriding a type. The acceptance test as written cannot pass against the actual Qt API.

Also there is a timing issue swept under the rug in M1c (DESIGN line 666): "`registerQmlSingletons()` must be called before any `QQuickView` is created." In current `main.cpp`, `LayerShellManager` (which creates the `QQuickView`s) is constructed **after** the singleton registration. Fine. But the SPEC's `registerQmlSingletons(QQmlEngine*)` signature implies the caller has an engine — which only exists *after* `LayerShellManager` runs. The signature contradicts the timing constraint.

**Suggested edits.**
1. Change REQ-F-102 to: `void registerQmlSingletons();` (no parameter). The method calls `qmlRegisterSingletonType` for each service.
2. Update REQ-F-102 acceptance criterion: "Calling the method twice is a no-op (guarded by a `registered_` bool inside ShellApplication)." — i.e., the idempotency must be implemented in `ShellApplication`, not relied upon from the Qt API.
3. Update DESIGN §2.3 class interface to remove the `QQmlEngine*` parameter.
4. Update TASKS T-003 check criterion correspondingly.
5. Optional: rename to `registerQmlTypes()` for accuracy.

---

### C3. `LayerShellManager(TrayModel*)` versus const-only accessors

**Observation.** REQ-F-104 says all `ShellApplication` getters are `const` and return `const Type*`. DESIGN §2.3 enforces this in the class interface. But `LayerShellManager::LayerShellManager(TrayModel*, QObject*)` — verified in `src/LayerShellManager.h:18` — takes a **non-const** `TrayModel*`. `main.cpp` line 113 currently passes the heap pointer directly.

After M1, `main.cpp` will need either:
- A non-const accessor (violates REQ-F-104), or
- `const_cast<TrayModel*>(shell.trayModel())` in `main.cpp` (a code smell), or
- `LayerShellManager` constructed inside `ShellApplication`.

DESIGN §1.3 line 119 leaves this open: "Responsibility 5 stays in `main.cpp` or moves to `ShellApplication`." This needs to be decided before T-002/T-005.

**Suggested edit.** Pick one explicitly in DESIGN §2.3:

> **Option A (recommended):** `LayerShellManager` ownership moves into `ShellApplication`. Add `void startShell()` (or similar) that constructs the `LayerShellManager` and stashes it. `main.cpp` calls `shell.startShell()` instead of constructing `LayerShellManager` itself. Drop the public `trayModel()` accessor — it's only used by `LayerShellManager`.
>
> **Option B:** Make `trayModel()` return `TrayModel*` (non-const), and weaken REQ-F-104 to permit non-const accessors for services that are passed by mutable reference into Qt APIs. Document the exceptions.

Without picking one, T-005 ("reduce main.cpp to ≤ 20 lines") cannot be implemented cleanly.

---

### C4. `holonight_qml` and the surfaces→services dependency are not justified

**Observation.** REQ-F-205 / DESIGN §3.4 says `holonight_surfaces` "Links holonight_platform, holonight_services". The rationale given is "(TrayModel references no service directly, but TrayWatcher uses D-Bus)". But D-Bus is provided by `Qt6::DBus`, not by `holonight_services`. Auditing the surfaces files:
- `PopupSurface`, `TooltipSurface`, `TrayMenuSurface` — use layer-shell (`holonight_platform`), no services.
- `TrayModel`, `TrayItem`, `TrayItemProperties`, `TrayWatcher`, `DbusMenuItem`, `DbusMenuClient` — use D-Bus directly, no `holonight_services` symbols.

So `holonight_surfaces` does **not** depend on `holonight_services`. The dependency edge is incorrect.

**Suggested edits.**
1. Remove the `holonight_surfaces → holonight_services` edge from DESIGN §2.2 graph and REQ-F-205.
2. Add a precondition check in T-014: "verify no `#include` in `src/surfaces/` references a header from `src/services/`."

---

## MAJOR

### M1. AudioState placement contradicts itself between milestones

REQ-F-203 (M2) puts `AudioState` in `holonight_core`. DESIGN §2.1 first lists `AudioState.h/.cpp` under `src/core/` (line 138), then after M4 says it moves to `src/services/audio/AudioState.h/.cpp` (line 178). TASKS T-022 ("Extend AudioService with device/stream properties") doesn't list the file move; T-018 only creates `AudioTypes.h`.

This means an implementer will:
1. In M1d (T-007), move `AudioState.h/.cpp` to `src/core/`.
2. In M2 (T-012), link it into `holonight_core`.
3. In M4c (T-022), find no explicit instruction to move it, leaving it in core forever despite DESIGN §2.1 saying it should move.

**Suggested edits.**
- Add an explicit T-022a: "Move `src/core/AudioState.{h,cpp}` to `src/services/audio/` and update `holonight_core` source list to remove it, `holonight_services` to add it."
- Or simply leave `AudioState` in `core/` and update DESIGN §2.1 (line 178) to drop the "(moved from services/ root or core/)" comment.

The second option is simpler. `AudioState` is a plain struct — there is no harm in living in `core/` permanently. `AudioTypes.h` (which carries the new richer types) can be in `services/audio/`.

---

### M2. Service count error: REQ-F-102 says "12 singletons" but lists 13

REQ-F-102 acceptance criterion: "All 12 singletons (WorkspaceModel, ActiveWindowService, KeyboardLayoutService, BatteryService, AudioService, NetworkService, SessionService, SystemInfoService, ThemeService, PopupSurface, TooltipSurface, TrayMenuSurface, TrayModel) are registered". Count: **13**. DESIGN §1.3 line 114 says 13. TASKS T-003 says 13. SPEC is the outlier.

**Suggested edit.** Change "12" → "13" in REQ-F-102.

Additionally, REQ-F-101 acceptance criterion says "All 16 services are instantiated". Counting `main.cpp`: `WorkspaceModel, ExtWorkspaceManager, HyprlandWorkspaceService, KeyboardLayoutService, ActiveWindowService, BatteryService, AudioService, NetworkService, SessionService, SystemInfoService, ThemeService, PopupSurface, TooltipSurface, TrayMenuSurface, TrayModel, TrayWatcher` = **16**. ✓. But the 3 non-singletons (`ExtWorkspaceManager`, `HyprlandWorkspaceService`, `TrayWatcher`) should be called out explicitly:

> "Of these 16, 13 are registered as QML singletons (REQ-F-102). The remaining three (`ExtWorkspaceManager`, `HyprlandWorkspaceService`, `TrayWatcher`) are non-QML services consumed only by C++ code."

---

### M3. M3 (README correction) is sequenced after M2 for no reason

SPEC §"Implementation Order (Strict Sequence)" puts M3 between M2d and M4a. The README change is a single-line edit with zero dependencies on the C++ refactor. Sequencing it after M2 means a structural defect in the README persists across the riskiest refactor steps.

Also: line 79 of `README.md` ("...PulseAudio...") is also inaccurate, but REQ-F-301 only fixes line 17. The reviewer doc (`docs/project-architecture-review.md` line 148) was explicit that the bullet is the issue, but a thorough fix should cover all occurrences.

**Suggested edits.**
1. Move M3 to run **immediately**, in parallel with M1a. Renumber if desired.
2. Expand REQ-F-301 acceptance criterion to cover all README occurrences of "PulseAudio D-Bus" and "PulseAudio" (in the runtime requirements section, line 79).

---

### M4. `qt6_add_qml_module` interaction with split targets is hand-waved

DESIGN §5 M2b–M2c line 757: "Moving QML module registration to `holonight_qml` static library requires understanding how Qt's QML type registration interacts with static libraries (registration may need to be explicitly imported via `Q_IMPORT_QML_PLUGIN` or `qt_import_qml_plugin` macro). If this causes issues, keep `qt6_add_qml_module(holonight-shell ...)` attached to the executable target..."

This is design-by-fallback. Either the plan is `qt6_add_qml_module(holonight_qml ...)` and the team commits to handling the `Q_IMPORT_QML_PLUGIN` machinery, or the plan is "keep it on the executable" and `holonight_qml` is dropped from the SPEC. **One must be picked before implementation starts**, not at the moment a CMake error appears.

See C1 above for the recommended resolution.

---

### M5. Tests must be re-linked after the target split — not mentioned anywhere

`tests/CMakeLists.txt` currently links every test against `holonight_core` (the monolithic library). After M2, each test should link only the targets it needs:
- `test_audio_service.cpp` → `holonight_services`
- `test_workspace_model.cpp` → `holonight_core`
- `test_tray_model.cpp` → `holonight_surfaces`
- etc.

If tests keep linking only `holonight_core`, M2's "no circular dependencies" claim becomes vacuous — the tests pull in the monolithic aggregate and bypass the boundaries.

**Suggested edit.** Add a task between T-014 and T-016:
> **T-015a:** Update `tests/CMakeLists.txt` so each test links the minimum set of new targets it needs. Verify that removing a target from one test's link line causes a link failure for that test (sanity check that boundaries are real).

---

### M6. Coverage instrumentation update is in DESIGN but not TASKS

Current CMakeLists.txt instruments `holonight_core` and `holonight-shell` only:
```cmake
target_compile_options(holonight_core PRIVATE --coverage)
target_link_options(holonight_core PRIVATE --coverage)
target_compile_options(holonight-shell PRIVATE --coverage)
target_link_options(holonight-shell PRIVATE --coverage)
```
After M2's six-target split, this misses `holonight_platform`, `holonight_services`, `holonight_surfaces`, `holonight_app`. DESIGN §5 M2b–M2c line 758 mentions this risk but no TASKS entry covers it.

**Suggested edit.** Add `T-016a: Update ENABLE_COVERAGE block to instrument all five C++ static targets (holonight_core, holonight_platform, holonight_services, holonight_surfaces, holonight_app) and the executable.`

---

### M7. `ServiceRegistry` requirement is ambiguous

SPEC REQ-C-103 lists `ServiceRegistry.h/cpp` as a required file in `src/app/`. DESIGN §2.4 says it "can be implemented as a static helper namespace or a non-QObject utility... If the implementation is simple enough, ServiceRegistry can be a `namespace` of free functions rather than a class. Either way it must not be a QML-registered type."

The SPEC says it's mandatory; the DESIGN says it's optional. T-001 ("Create src/app/ directory and ShellApplication class skeleton") doesn't mention `ServiceRegistry` at all.

**Suggested edit.** Either drop `ServiceRegistry` from REQ-C-103's mandatory file list (treat it as an internal implementation detail of `ShellApplication`), or commit to it as a separate class and add a task for it. Recommended: **drop it**. The 13 singleton-registration calls inline in `ShellApplication::registerQmlTypes()` are clear enough; an extra indirection adds nothing.

---

### M8. `SystemInfo` (free functions) belongs in `core`, not `services`

DESIGN §2.1 puts both `SystemInfo.h/.cpp` (free functions: `parseOsRelease()`, `findSystemLogoPath()`) and `SystemInfoService.h/.cpp` (QML singleton) under `src/services/`. But `SystemInfo` is pure logic — no D-Bus, no async, no IPC. It is the same shape as `BatteryState`/`AudioState` (pure data/utilities).

Splitting them across two layers also makes the test coverage easier:
- `src/core/SystemInfo.{h,cpp}` — pure, easy to unit test
- `src/services/SystemInfoService.{h,cpp}` — QML adapter, no further test needed

**Suggested edit.** In DESIGN §2.1 / REQ-C-103, move `SystemInfo.h/.cpp` to `src/core/`. Update the existing `test_system_info.cpp` to link `holonight_core` only.

---

### M9. `Qt6::Quick` dependency placement

`holonight_core` currently links `Qt6::Quick`. After the split, REQ-F-203 says `holonight_core` links only `Qt6::Core, Qt6::DBus, tomlplusplus`. But `WorkspaceModel.h` includes `<QtQml/qqml.h>` for `QML_ELEMENT`/`QML_SINGLETON` macros — which lives in `Qt6::Qml`, not `Qt6::Quick`. And these macros only matter when scanned by `qmltyperegistrar` (see C1 above).

The DESIGN doesn't list `Qt6::Qml` in `holonight_core` deps either. So `WorkspaceModel.h` won't compile after the split because `<QtQml/qqml.h>` is unreachable.

**Suggested edit.** Add `Qt6::Qml` to `holonight_core` PUBLIC deps in REQ-F-203 (and to every other target that has `QML_ELEMENT` in its headers — services, surfaces). Don't add `Qt6::Quick` to core; that's UI-only.

---

## MINOR

### m1. `DbusPropertyClient` placement is debatable

`DbusPropertyClient.h/cpp` is a D-Bus utility, not Wayland or Hyprland-specific. Putting it under `src/platform/` lumps unrelated transports together. The reviewer doc (`project-architecture-review.md` line 28-31) suggested:
```
platform/
  wayland/
  hyprland/
  dbus/
  xdg/
```
The SPEC/DESIGN flattens `platform/` into one dir, losing this structure. Not a blocker, but worth noting that a future split into `platform/{wayland,hyprland,dbus}/` is foreseeable.

**Suggested edit (optional).** Either nest now (small effort, ~6 files moved) or add a follow-up item to the roadmap for after M5.

---

### m2. `IconImageProvider.h` placement

It's in `src/surfaces/` per DESIGN §2.1 but it's a `QQuickImageProvider`, used by `TrayModel` for icon theme lookup. It's not a "surface" itself. Fine to group with tray code, but the domain label is inaccurate.

**Suggested edit.** Optional: mention in DESIGN §2.1 that `surfaces/` includes "tray support" (image provider, dbus menu) in addition to true layer-shell surfaces. Or rename the dir to `surfaces-and-tray/`. Probably leave as is.

---

### m3. Holonight theme module is not mentioned in the restructure

`CLAUDE.md` documents that all colors come from `import Holonight` / `HoloniightPalette.<token>`. The `tests/qmllint/Holonight/qmldir` shim exists for qmllint, but the actual palette QML source is **not** inside this repo (it must be a separately-installed module or live under `$HOME/.local/lib/qt6/qml/Holonight/`, referenced from CMakeLists.txt qml-lint target line 309).

This is fine, but the SPEC's "QML import path stability" section (line 502-503) should explicitly call this out so it doesn't surprise an implementer:

> "The `Holonight` palette module is **external** to this repo (installed system-wide or under user qml path). The restructure does not touch it. Verify after each milestone that `import Holonight` continues to resolve in qmllint."

---

### m4. Lint/format checks not in TASKS

The acceptance criteria sections at the bottom of each milestone mention `task qml-lint` / `task tidy`, but T-001 through T-025 don't include lint as a check step. A reviewer reading TASKS in isolation might skip them.

**Suggested edit.** Add a final task per milestone, e.g., `T-016b: Run task format-check, task tidy, task qml-lint after M2 completes; all must pass clean.`

---

### m5. `holonight-shell` link order claim is meaningless

REQ-F-208: "Link order is: `holonight_app -> holonight_services -> holonight_surfaces -> holonight_platform -> holonight_core -> holonight_qml`". CMake auto-resolves link order from PUBLIC/PRIVATE deps; specifying the order manually is fragile and won't affect anything once dependencies are declared correctly.

**Suggested edit.** Replace this acceptance criterion with: "Executable links `holonight_app` only; CMake resolves transitive deps from PUBLIC link lines on the static libs."

---

### m6. Binary-size acceptance criterion (±5%) has no measurement baseline

REQ-F-208 / REQ-NF-202 / DESIGN §8 say binary must stay within 5% of "pre-refactor baseline". Nothing in TASKS captures the baseline. By the time the implementer measures, the baseline has shifted (other commits land between M0 and M2).

**Suggested edit.** Add `T-000: Record current binary size (build/holonight-shell) on main as a number in this REVIEW-FINDINGS doc (or a dedicated baseline file).` Do this before M1 starts.

---

### m7. Missing risk: `Q_IMPORT_QML_PLUGIN` requirements for static QML modules

Tied to C1. If at any milestone the implementer ends up calling `qt6_add_qml_module(holonight_qml ...)` (or any name other than the executable), they will need to add an explicit `qt_import_qml_plugins(holonight-shell)` or `Q_IMPORT_QML_PLUGIN` macro in `main.cpp` to ensure the static plugin's symbols are linked. DESIGN §8 mentions this generically ("registration may need to be explicitly imported") but doesn't give the exact incantation.

**Suggested edit.** If you keep `holonight_qml` as a target (against my C1 recommendation), DESIGN §3.5 should show the exact code lines needed in `main.cpp` to import the static QML module.

---

## SUMMARY TABLE

| # | Severity | Topic | Action |
|---|----------|-------|--------|
| C1 | Critical | `holonight_qml` breaks `qt6_add_qml_module`/qmltyperegistrar | Drop `holonight_qml` target or commit to true QML-module wiring |
| C2 | Critical | `registerQmlSingletons(QQmlEngine*)` signature wrong | Remove parameter; document global registry semantics |
| C3 | Critical | `LayerShellManager(TrayModel*)` vs const accessors | Decide owner explicitly before T-002 |
| C4 | Critical | `surfaces → services` link is unjustified | Drop edge from dep graph and REQ-F-205 |
| M1 | Major | `AudioState` placement contradiction | Pin to `core/` permanently, or add explicit M4 move task |
| M2 | Major | "12 singletons" vs "13" off-by-one | Fix to 13 |
| M3 | Major | M3 (README) sequenced after M2 with no reason | Move M3 to parallel with M1a; cover line 79 too |
| M4 | Major | `qt6_add_qml_module` plan is "fallback first" | Pick one approach in DESIGN (see C1) |
| M5 | Major | Tests not re-linked after target split | Add T-015a |
| M6 | Major | Coverage instrumentation missed in TASKS | Add T-016a |
| M7 | Major | `ServiceRegistry` mandatory in SPEC, optional in DESIGN | Drop from SPEC |
| M8 | Major | `SystemInfo` belongs in `core` not `services` | Re-categorize in DESIGN §2.1 |
| M9 | Major | `Qt6::Qml` missing from new `holonight_core` deps | Add to PUBLIC link list |
| m1 | Minor | `DbusPropertyClient` placement | Optional nest into `platform/dbus/` |
| m2 | Minor | `IconImageProvider.h` placement | Just label correctly |
| m3 | Minor | Holonight theme module is external | Add a note in SPEC stability section |
| m4 | Minor | Lint/format not in TASKS checklist | Add per-milestone lint task |
| m5 | Minor | Link order acceptance criterion meaningless | Replace with "executable links app only" |
| m6 | Minor | Binary-size baseline not captured | Add T-000 baseline-recording task |
| m7 | Minor | `Q_IMPORT_QML_PLUGIN` not documented | Tie to C1 decision |
