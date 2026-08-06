# U-02 — Presentation Surfaces & Shell Composition Root — Deep Review Findings

**Task**: T-002 · **Skill**: `qt-cpp-review` (Phase 1 deterministic lint + Phase 2 six-agent deep analysis) · **Scope**: 54 files, read-only

## Scope

- `libs/holonight-surfaces/src/` — all 50 files: `BackgroundManager`, `DbusMenuClient`, `DbusMenuItem`, `IconImageProvider`, `LauncherSurface`, `LauncherSurfaceLifecycle`, `LayerShellManager`, `NotificationManager`, `NotificationToastSurface`, `PerMonitorLayerManager`, `PopupGeometry`, `ShellConstants`, `SidebarManager`, `SidebarSurfacePolicy`, `StatusPopupGeometry`, `StatusPopupSurface`, `TooltipSurface`, `TrayItem`, `TrayItemProperties`, `TrayMenuSurface`, `TrayModel`, `TrayWatcher`, `WidgetClock`, `WidgetCountdown`, `WidgetManager`, `WidgetSurfacePolicy`
- `apps/shell/app/` — `ControlServer.{h,cpp}`, `ShellApplication.{h,cpp}` (4 files)
- `apps/shell/main.cpp`

## Prior Context

Consulted per T-002 instructions:

- `docs/sdd/arch-restructure-roadmap/DESIGN.md` §3.1–3.4 (surfaces/app composition, C3 `LayerShellManager` ownership decision).
- `docs/sdd/poc-readiness-review/DESIGN.md` §5 preliminary flag on `ShellApplication`'s 40+ member-initialization concentration-of-risk.
- CLAUDE.md "Layer-Shell Surface Hiding" and "SingleShotConnection race on `configured()`" gotchas.

**Dual-surface-lifecycle acknowledgment**: verified as deliberately different, not an inconsistency. `PerMonitorLayerManager`-family surfaces (`BackgroundManager`, `WidgetManager`, `LayerShellManager` itself) stay mapped for the process lifetime and toggle QML-root `visible`. `SidebarManager`/`LauncherSurface` use a separate create-on-open/destroy-on-close lifecycle. Both patterns were confirmed correctly implemented for their respective category — no cross-contamination found (no `.hide()`/`.show()` misuse on a keep-mapped surface, no leaked `QQuickView` across sidebar/launcher open-close cycles).

**`ShellApplication` initialization-order concentration-of-risk**: confirmed as real and now has a concrete finding ([F-01] below) — the three-phase `registerQmlTypes()` → `startServices()` → `startShell()` sequencing is currently enforced only by a `Q_ASSERT` that compiles to nothing in Release builds.

## Tool Sign-off — Phase 1 Deterministic Lint

107 raw lint hits across 11 rule categories. Excluded as noise, same rationale as U-01:

- **VAR-3** (60 hits, brace-init style) — not reported.
- **HDR-3** (7 hits, Windows `std::min`/`max` macro safety) — irrelevant, Linux-only project — not reported.
- **DEP-7** (19 hits, `qMin`/`qMax`/`qBound` → `std::min`/`max`/`clamp`) — pure modernization, no behavioral difference, concentrated in `TrayMenuSurface.cpp`; not elevated to a standalone finding.

Remaining categories (`DEP-13` ×4, `DEP-11` ×4, `TMO-1` ×3, `DEP-2` ×3 QSharedPointer→shared_ptr, `PAT-9` ×2, `DEP-5` ×2 QPair→pair, `PAT-2` ×1) are low-severity modernization items, not elevated individually.

Two flagged lint hits were independently investigated by Phase 2 agents:

- **LCY-6** (`NotificationManager.cpp:18`, "`qDeleteAll` — verify grandchildren also cleaned"): not independently corroborated by any deep agent in this pass (out of assigned scope for all six missions). Left as an open, unescalated lint hit — worth a quick manual check in a future pass, but not asserted as a real defect here.
- **LCY-5** (`DbusMenuClient.cpp:43`, "`m_id` grows without size cap"): **false positive**, confirmed by the Ownership & Lifecycle agent. Line 43 of the current file is inside `activateItem()` (a D-Bus `Event` call helper) — there is no member named `m_id` anywhere in `DbusMenuClient`, and no unbounded-growth container exists at or near that line. The lint heuristic appears to have mismatched on an unrelated identifier pattern (possibly `item_id`). No action needed; lint rule should be reviewed for this false-trigger case.

## Confirmed Findings (confidence ≥ 80/100)

### [F-01] Integer overflow in tray-icon pixmap decoding bypasses the size guard, enabling multi-gigabyte allocation from untrusted D-Bus data
- **Severity**: Critical
- **Effort**: S
- **Location**: `libs/holonight-surfaces/src/TrayItem.cpp:44-100` (guard at `:55`, overflow at `:73`, bypassed check at `:74`, allocation at `:82`)
- **Rationale**: `decodePixmapList()` only rejects a pixmap when `width == 0 || height == 0` — negative or arbitrarily large dimensions from a third-party tray app's `IconPixmap`/`AttentionIconPixmap` D-Bus property pass through unchecked. `pixel_count = width * height` is computed in 32-bit `int`; dimensions around 50000×50000 overflow `INT_MAX` and wrap negative. The subsequent length guard (`data.size() < pixel_count * 4`) then compares against the wrapped negative value and is always false, silently bypassing the "payload too short" check. Execution proceeds to `QImage(width, height, Format_ARGB32)` with the original (non-wrapped) dimensions — e.g. ~9.5GB for 50000×50000 — attempted from a D-Bus message containing only a few bytes of actual pixel data. Any process on the system bus registering as a `StatusNotifierItem` (no special privilege required) can trigger this and OOM/crash the entire shell process across all monitors. Confidence 80/100.
- **Suggested Direction**: Reject non-positive width/height explicitly (`<= 0`, not just `== 0`); compute `pixel_count` as `qint64`/`qsizetype` to avoid overflow; clamp width/height to a sane maximum (a few hundred pixels) before allocating the `QImage`.

### [F-02] `SidebarManager::toggle()` tears down the currently-open sidebar before validating the requested monitor exists
- **Severity**: High
- **Effort**: S
- **Location**: `libs/holonight-surfaces/src/SidebarManager.cpp:88-96`, reachable via `apps/shell/app/ControlServer.cpp:43-45` (`sidebar:toggle:<monitor>` control-socket command)
- **Rationale**: `ControlServer` extracts `monitor_name` from raw, unvalidated socket bytes and forwards it straight to `SidebarManager::toggle()`. If the name doesn't match the currently-open monitor, `toggle()` unconditionally calls `closeAll()` first, then `openOnMonitor(monitor_name)`; if that name doesn't resolve to a real `QScreen`, `createSurface`/`findScreen` fails and nothing reopens. Net effect: any local process with access to `$XDG_RUNTIME_DIR/holonight-shell/control.sock` can blindly close the user's open sidebar by sending a single malformed or garbage monitor name, with no way for it to reopen automatically. Confidence 83/100.
- **Suggested Direction**: Resolve/validate the monitor via `findScreen()` first in `toggle()`/`openOnMonitor()` and no-op with a warning log if it doesn't resolve, before calling `closeAll()`.

### [F-03] `ShellApplication` three-phase startup ordering is enforced only by a release-mode no-op `Q_ASSERT`
- **Severity**: High
- **Effort**: M
- **Location**: `apps/shell/app/ShellApplication.cpp:247` (`startShell()`), depends on `registerQmlTypes()`/`startServices()` (`:187-240`) having already run; sole call site `apps/shell/main.cpp:72-74`
- **Rationale**: `startShell()` builds `LayerShellManager`/`BackgroundManager`, creates `SidebarManager`, registers QML singletons, and wires cross-service connections (e.g. `sidebarOpened` → `NotificationService::onSidebarOpened`) — all of which assume `registerQmlTypes()` and `startServices()` already ran. The only ordering guard is `Q_ASSERT(registered_ && services_started_)`, which compiles to nothing in Release/NDEBUG builds. All three phases are exposed as independent public methods, each individually idempotent, with nothing but calling convention in `main.cpp` enforcing correct sequence. A future refactor or alternate entry point calling `startShell()` first in a Release build would silently produce a shell with half-initialized services (e.g. the tray context menu never opens because `tray_model_->setMenuSurface()` wiring in `startServices()` never ran) — no crash, no diagnostic, just quietly broken features. Confidence 85/100. This directly substantiates the DESIGN.md §5 "concentration-of-risk" flag on `ShellApplication`.
- **Suggested Direction**: Make the dependency self-enforcing — have `startShell()` call `registerQmlTypes()`/`startServices()` itself if their guards are still false (both already idempotent), or replace the assert with a loud `qCritical` + early return so a Release-build ordering mistake fails visibly instead of silently degrading.

### [F-04] Duplicated "shadow padding" surface-sizing math in `TrayMenuSurface.cpp`
- **Severity**: Low
- **Effort**: S
- **Location**: `libs/holonight-surfaces/src/TrayMenuSurface.cpp:152-164` (`updateActiveGeometry`) and `:306-316` (`ensureSurface`)
- **Rationale**: The same 8-statement margin/padding-clamping computation is written twice with different local variable names, each independently declaring its own local `kShadowPadding = 12` instead of using the file's existing anonymous-namespace constant block. A future change to shadow-padding behavior is likely to be applied to only one call site, causing the initial menu open and a later submenu-driven resize to visually diverge. Confidence 85/100.
- **Suggested Direction**: Extract a small free function (e.g. `computeShadowedGeometry(origin, content, screen, shadow_padding)`) and call it from both sites; move `kShadowPadding` to the existing constant block.

### [F-05] Dead code: `PopupGeometry.{h,cpp}` has zero production callers
- **Severity**: Low
- **Effort**: S
- **Location**: `libs/holonight-surfaces/src/PopupGeometry.h:10`, `PopupGeometry.cpp:10-17`
- **Rationale**: `sessionPopupGeometry()` and the `PopupGeometry` struct compile into `holonight_surfaces` but are referenced only by their own dedicated unit test — no `.qml` file or C++ class constructs a `PopupGeometry` in production. Appears to predate `StatusPopupGeometry.cpp`'s generalized equivalent (which has materially different default-size fallbacks) and was left behind after that generalization. Confidence 85/100.
- **Suggested Direction**: Confirm no near-term feature needs it, then delete `PopupGeometry.h`/`.cpp` and its dedicated test, or wire it into whatever "session popup" surface it was originally meant for.

### [F-06] `kScreenEdgeMargin` constant independently declared three times across separate files
- **Severity**: Low
- **Effort**: S
- **Location**: `TooltipSurface.cpp:14`, `TrayMenuSurface.cpp:20`, `StatusPopupGeometry.cpp:6` (all `constexpr int kScreenEdgeMargin = 8`)
- **Rationale**: Same value, same semantic meaning (minimum popup-to-screen-edge distance), declared independently in three files despite `ShellConstants.h` already existing as the shared-constants home for exactly this kind of cross-cutting layout value (`kBarHeight`, `kSidebar*`). Confidence 83/100.
- **Suggested Direction**: Hoist into `ShellConstants.h` and reference from all three call sites.

### [F-07] `sidebarSurfaceWidth()` is unused in production code
- **Severity**: Low
- **Effort**: S
- **Location**: `libs/holonight-surfaces/src/SidebarSurfacePolicy.h:3`, `SidebarSurfacePolicy.cpp:7`
- **Rationale**: No caller anywhere in `libs/`/`apps/` (including QML) besides its own unit test. `SidebarManager::createSurface()` sizes the sidebar surface via a full-screen `set_size(0,0)` host with all-edge anchors — actual panel width is driven entirely from QML, not this C++ helper. Confidence 80/100.
- **Suggested Direction**: Either wire it into `SidebarManager` so C++ and QML share one width source of truth, or delete it with its test if width is meant to live purely in QML.

## Investigation Targets (confidence 60-79 — human verification needed)

Capped at 10 per skill protocol; three items at the 62/100 floor (a stale-async-reply TOCTOU race in `TrayWatcher`'s `GetAll` handling, a `DbusMenuClient` max-recursion-depth field-mis-consumption on the `av` D-Bus type, and `TrayModel::data()`'s redundant per-role override-matching) were dropped to stay within the cap.

#### [I-01] `TrayImageProvider` reads `TrayModel`'s row containers with no synchronization against main-thread mutation
- **Severity**: Medium · **Effort**: M · **Confidence**: 72/100
- **Location**: `TrayModel.h:86` (`TrayImageProvider`), `TrayModel.cpp:360-380` (`requestImage`), `:235-246` (`imageForKey`), `:77-78` (`rows_`, `index_by_key_`)
- **Rationale**: `TrayImageProvider` is a `QQuickImageProvider::Image`-type provider, which per Qt docs may have `requestImage()` invoked from a non-GUI thread and must be reentrant. It reads `rows_`/`index_by_key_` with zero locking while `TrayModel::addItem`/`updateItem`/`removeItem` mutate the same containers on the main thread, driven continuously by `TrayWatcher`'s D-Bus callbacks. Currently likely safe in practice — the consuming QML `Image` element doesn't set `asynchronous: true` (unlike sibling icon-loading `Image`s elsewhere in the codebase that do), so calls probably stay on the GUI thread today — but this is a live landmine: flipping that one QML property on any tray-icon `Image` would introduce an unsynchronized concurrent read/write.
- **Suggested Direction**: Guard `rows_`/`index_by_key_` with a mutex covering both the image-provider read path and the mutating methods, or have `TrayModel` publish a separately-locked icon cache decoupled from the row-ordering containers. At minimum, comment the containers as unsafe for `TrayImageProvider` access without synchronization.

#### [I-02] `TrayModel::roleNames()` / `DbusMenuModel::roleNames()` rebuild the role hash on every call
- **Severity**: Low · **Effort**: S · **Confidence**: 70/100
- **Location**: `TrayModel.cpp:112-128`, `DbusMenuItem.cpp:47-59`
- **Rationale**: Both construct a fresh `QHash` literal (13 and 9 entries) on every invocation. `DbusMenuModel` instances are additionally created per submenu and per top-level menu open, so each new instance repeats the allocation on first bind.
- **Suggested Direction**: Cache as a function-local `static const QHash`.

#### [I-03] Clamped screen-edge-centering geometry duplicated between `TooltipSurface` and `StatusPopupGeometry`
- **Severity**: Low · **Effort**: S · **Confidence**: 70/100
- **Location**: `TooltipSurface.cpp:107-110` vs `StatusPopupGeometry.cpp:38-51`
- **Rationale**: `StatusPopupGeometry.cpp` factors "center on anchor, clamp to screen edges" into a dedicated, independently unit-tested free function. `TooltipSurface` reimplements the identical formula inline instead of reusing/extracting a shared helper — a second copy that isn't unit-testable independent of the Wayland surface.
- **Suggested Direction**: Extract a shared `clampCenteredPosition(anchor_center, extent, screen_extent, edge_margin)` helper used by both.

#### [I-04] Layer-shell surface classes never check `QQuickView` load status after `setSource()` — a QML load failure produces a mapped-but-blank surface with no diagnostic signal
- **Severity**: Medium · **Effort**: M · **Confidence**: 68/100
- **Location**: `PerMonitorLayerManager.cpp:81-88`, pattern repeated in `StatusPopupSurface.cpp:172`, `TooltipSurface.cpp:119`, `LauncherSurface.cpp:121`, `TrayMenuSurface.cpp:337`, `NotificationToastSurface.cpp:109`, `SidebarManager.cpp:227`
- **Rationale**: Every surface-owning class calls `setSource(url)` and unconditionally proceeds to commit the surface without checking `QQuickView::status() == QQuickView::Error`. A broken QRC path or a runtime QML binding error after a refactor would map a surface that consumes screen space/exclusivity/input focus but renders nothing, diagnosable only via a `--verbose`-gated QML console warning. Systemic across every surface class in this unit, not a one-off.
- **Suggested Direction**: Connect to `statusChanged` (or check `status()` post-`Ready`/`Error`) and log `qCCritical` (optionally tear the surface down) on `Error`, instead of relying on default QML console diagnostics.

#### [I-05] `StatusPopupSurface`/`SidebarManager`/`LauncherSurface` teardown hides the `QQuickView` before the layer-surface protocol object is destroyed, inverting a documented ordering invariant
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `StatusPopupSurface.cpp:184-202`, same pattern at `SidebarManager.cpp:243-249`, `LauncherSurface.cpp:127-140`
- **Rationale**: `destroySurface()` calls `surface_->deleteLater()` (deferred) then immediately, synchronously calls `view_->hide()`. Per this project's own documented gotcha (CLAUDE.md "Layer-Shell Surface Hiding": hide destroys the `wl_surface` immediately), this inverts the ordering `PerMonitorLayerManager.h:42-43` documents as load-bearing ("surface must be destroyed before view"). No observed crash — a fresh `QQuickView`/`LayerSurface` pair is always created on next open, not the reuse scenario the gotcha specifically warns about — but it's a real deviation from the documented invariant, recurring identically across three independently-written surface classes, worth a compositor-log check during rapid open/close cycling.
- **Suggested Direction**: Either drop the explicit `view_->hide()` (the deferred delete chain already tears the window down) or destroy `surface_` synchronously before hiding/deleting the view, matching the pattern `TooltipSurface`/`TrayMenuSurface` already use. Verify via `task compositor-smoke-check` during rapid cycling.

#### [I-06] `decodePixmapList` decodes ARGB pixels one at a time via `QImage::setPixel`, and builds a debug string unconditionally for every candidate
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `TrayItem.cpp:44-100` (per-pixel loop `:84-91`, unconditional string build `:54`)
- **Rationale**: For a 128×128/256×256 tray icon, this is tens of thousands of bounds-checked `setPixel` calls plus unconditional `QString` formatting for a log line, on every icon-pixmap update.
- **Suggested Direction**: Use `scanLine()`/raw `bits()` with manual byte-order swap; gate the debug-string build behind `isInfoEnabled()`.

#### [I-07] `TooltipSurface` positions against `anchor_x` without subtracting the target screen's origin, unlike its two structural siblings
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `TooltipSurface.cpp:107-110` vs `StatusPopupGeometry.cpp:44` and `TrayMenuSurface.cpp:41`
- **Rationale**: The two siblings explicitly convert a global/desktop coordinate to screen-local before clamping (`anchor_x - screen_origin_x`); `TooltipSurface` clamps `anchor_x` directly, treating it as already screen-local. Actual runtime impact depends on which QML caller path is exercised — `BarTooltipArea.qml` mixes `mapToGlobal` and window-local coordinates depending on a flag — so this could not be fully verified from the C++ side alone.
- **Suggested Direction**: Either document why `TooltipSurface` is intentionally exempt, or add the same screen-origin normalization used by its siblings, verified with a multi-monitor manual test (secondary monitor positioned right of a nonzero-width primary).

#### [I-08] `ControlServer` has no message framing or size bound, and only processes the first `readyRead` chunk
- **Severity**: Medium · **Effort**: S · **Confidence**: 65/100
- **Location**: `apps/shell/app/ControlServer.cpp:49-58`
- **Rationale**: `handleConnection` reads once on first `readyRead` and immediately disconnects — no accumulation loop, no max size, no read timeout. A command split across two writes gets truncated and misparsed; a large one-shot payload forces an application-unbounded heap allocation sized only by kernel socket buffers.
- **Suggested Direction**: Accumulate until `disconnected` (or a delimiter), cap accepted size (commands are short fixed strings — a few hundred bytes is ample), reject oversized connections.

#### [I-09] `WidgetManager::viewForMonitor` linear-scans the per-monitor surface map on every occupancy/workspace change
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `WidgetManager.cpp:205-212`, invoked from `applyVisibility` (`:112-134`) on every `onOccupancyChanged`
- **Rationale**: `surfaces()` is keyed by `QScreen*`, but monitor identity arrives as a `QString`; lookup does a full linear scan calling `screen->name()` per entry, on every occupancy event for every `WidgetManager` instance. Real-world cost is small given typical 1-4 monitor counts, but the pattern repeats per-instance per-event.
- **Suggested Direction**: Maintain a `QHash<QString, QScreen*>` alongside `surfaces()`, updated in `handleScreenAdded`/`handleScreenRemoved`.

#### [I-10] `TrayWatcher` uses `QStringList::contains()`/`removeOne()` (O(n)) for item-key membership checks on every property-fetch completion
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `TrayWatcher.cpp:138-166` (`registerTrayItemKey`/`unregisterTrayItemKey`), guard checks at `:292,402,426,472`
- **Rationale**: `registered_items_` is a `QStringList`; every async property-fetch completion lambda (IconName, IconPixmap, AttentionIconName/Pixmap, Title, ToolTip) linear-scans it via `contains()`. A single tray registration triggers several of these.
- **Suggested Direction**: Back the membership check with a `QSet<QString>` mirror or the existing `QHash`-style container, keeping `QStringList` only where ordered D-Bus property export is required.

## Summary

| Category | Lint (reported) | Deep (confirmed ≥80) | Investigation (60-79) | Total |
|---|---|---|---|---|
| Model Contracts | 0 | 0 | 0 (1 dropped for cap) | 0 |
| Ownership & Lifecycle | 1 (LCY-5, refuted) | 1 | 1 | 2 |
| Thread Safety | 0 | 0 | 1 | 1 |
| API & C++ Correctness | 0 | 2 | 1 | 3 |
| Error Handling & Validation | 0 | 2 | 2 (1 dropped for cap) | 4 |
| Performance & Code Quality | DEP-7 (19, noted) | 2 | 5 (1 dropped for cap) | 7 |
| Unescalated lint | LCY-6 (not corroborated) | — | — | — |
| **Total** | **107 raw / 1 refuted / rest low-value** | **7** | **10** | **17 actionable** |

17 actionable items (7 confirmed + 10 capped investigation targets). This is the highest-severity unit reviewed so far: **[F-01]** (untrusted-D-Bus-triggered multi-GB allocation via tray icon pixmap decoding) is a genuine remote-triggerable crash/DoS from any process able to register a `StatusNotifierItem` — no special privilege required — and should be prioritized first. **[F-02]** (control-socket sidebar DoS) and **[F-03]** (Release-mode-silent startup-order enforcement) are both real, concrete instances of the DESIGN.md-flagged risk areas for this unit (untrusted local IPC surface, composition-root concentration-of-risk) and confirm those preliminary architecture observations were well-founded rather than speculative.
