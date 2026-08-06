# Launcher Finalize — Architecture Design

**Feature:** Launcher UI/UX Finalization
**Status:** Design
**Spec:** [SPEC.md](SPEC.md)

---

## 1. Component Map

### Files That Change

| File | Change |
|------|--------|
| `src/surfaces/LauncherSurface.h` | Add `notifyHideReady()` Q_INVOKABLE; add `initialized_` flag; remove `destroySurface()` from the toggle path; add `resetAndFocusQml()` private helper |
| `src/surfaces/LauncherSurface.cpp` | Full redesign of `show()` / `hide()` / `ensureSurface()`; implement keep-alive lifecycle |
| `src/qml/Launcher/Launcher.qml` | Add `panel` scale+opacity open/close animation; add `resetAndFocus()` function; call `LauncherSurface.notifyHideReady()` on close-animation completion instead of calling `hide()` directly |
| `src/qml/Launcher/LauncherResultRow.qml` | Add `required property bool isBestMatch`; conditional `height`, name text color |
| `src/qml/Launcher/LauncherSearchField.qml` | Replace `"x"` with `"×"` (U+00D7); increase font size to 20px; hover color via `HoloniightPalette` |

### Files That Stay the Same

| File | Reason |
|------|--------|
| `src/services/launcher/LauncherService.h/.cpp` | API is already complete; no changes needed |
| `src/platform/LayerSurface.h/.cpp` | `configured()` signal is already wired correctly |
| `src/qml/Launcher/LauncherSearchField.qml` (behavior) | Key handling, `forceInputFocus()`, HudFrame frame focus — logic unchanged; only glyph+color tweaks |

---

## 2. C++ Surface Lifecycle State Machine

### States

```
Uninitialized  ──first show()──>  Creating  ──configured()──>  Ready/Hidden
                                                                     │
                                                          show() ◄───┤───► hide()
                                                                     │
                                                               Ready/Visible
                                                                     │
                                                         notifyHideReady() fires
                                                                     │
                                                               Ready/Hidden
```

| State | Meaning |
|-------|---------|
| `Uninitialized` | `view_` is null; `surface_` is null; `initialized_` is false |
| `Creating` | `ensureSurface()` has been called; waiting for first `configured()` from compositor |
| `Ready/Hidden` | Surface mapped; QML root `visible = false`; no keyboard interactivity (or retained from initial config) |
| `Ready/Visible` | Surface mapped; QML root `visible = true`; keyboard exclusive |

### Transitions

**`Uninitialized` → `Creating`** (first `show()` call)
1. Resolve target `QScreen`.
2. Call `makeLauncherView()` to create `QQuickView`, obtain `wl_surface` and `wl_output`.
3. Call `shell_.get_layer_surface()` with namespace `"launcher"`, layer `top`.
4. Construct `LayerSurface`, set anchors all-four-sides, size 0×0, exclusive zone 0, `keyboard_interactivity_exclusive`.
5. Load QML: `view_->setSource(QUrl("qrc:/HolonightShell/Launcher/Launcher.qml"))`.
6. Commit the `wl_surface`.
7. Connect `surface_->configured()` via `Qt::SingleShotConnection` → enter `Ready/Hidden` then immediately promote to `Ready/Visible` (see transition below).
8. Set `initialized_ = true`.

**`Creating` → `Ready/Hidden` → `Ready/Visible`** (first `configured()`)
1. The `configured()` lambda fires once.
2. Call `resetAndFocusQml()`: sets `LauncherService::setQuery("")`, then calls `view_->rootObject()->metaObject()->invokeMethod(root, "resetAndFocus")`.
3. Set QML root `visible = true`.
4. Call `setVisible(true)` to update the `visible_` property and emit `visibleChanged()`.

**`Ready/Visible` → `Ready/Hidden`** (hide — triggered by `notifyHideReady()`)
1. QML animation has completed; QML calls `LauncherSurface.notifyHideReady()`.
2. C++ sets QML root `visible = false` (the animation already hid content; this prevents stale renders if the surface is refocused externally).
3. Call `setVisible(false)`.

**`Ready/Hidden` → `Ready/Visible`** (subsequent `show()`)
1. `initialized_` is true; skip surface creation.
2. Call `resetAndFocusQml()`.
3. Set QML root `visible = true`.
4. Call `setVisible(true)`.

**`toggle()`** — simply calls `hide()` if `visible_`, else `show()`. No special handling needed because the state machine above is idempotent for repeated calls in the same state.

**Shell not yet active (pending show)**
Unchanged from current implementation: set `pending_show_ = true` and `pending_screen_`; process on `shell_.activeChanged`.

### What `destroySurface()` Becomes

`destroySurface()` is retained only for destructor use (`~LauncherSurface()`). It is **never called from `hide()`**. The implementation remains the same as today (`surface_->deleteLater()`, `view_->hide()`, `view_->deleteLater()`) but is only reached during process shutdown.

### Updated Header Sketch

```cpp
class LauncherSurface : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)

 public:
  Q_INVOKABLE void toggle(const QString& screen_name = {});
  Q_INVOKABLE void show(const QString& screen_name = {});
  Q_INVOKABLE void hide();
  // Called by QML after the close animation completes.
  Q_INVOKABLE void notifyHideReady();

 Q_SIGNALS:
  void visibleChanged();

 private:
  bool ensureSurface(const QString& screen_name);
  void destroySurface();          // destructor-only
  void resetAndFocusQml();        // setQuery("") + invokeMethod("resetAndFocus")
  void setVisible(bool visible);

  LayerShell shell_;
  QQuickView* view_{nullptr};
  LayerSurface* surface_{nullptr};
  QString pending_screen_;
  bool pending_show_{false};
  bool visible_{false};
  bool initialized_{false};
};
```

---

## 3. QML Animation Design

### Approach: SequentialAnimation on Close, `onVisibleChanged` trigger on Open

`Behavior` on `scale`/`opacity` is insufficient for the close sequence because the surface must stay `visible = true` (so the compositor keeps rendering the surface) throughout the animation, and only transition to `visible = false` after the last frame. A `Behavior` fires whenever the property changes from any cause, including programmatic resets, making it hard to distinguish an intentional close from a query reset. `SequentialAnimation` gives explicit control over when the final `LauncherSurface.notifyHideReady()` call fires.

### Open Animation

Trigger: `panel`'s parent `root` becomes `visible = true` (set by C++). The `onVisibleChanged` handler on `root` starts the open animation when `visible` becomes `true`.

```qml
// In root item
onVisibleChanged: {
    if (visible) {
        panel.scale = 0.95
        panel.opacity = 0.0
        openAnimation.start()
    }
}

ParallelAnimation {
    id: openAnimation
    NumberAnimation {
        target: panel; property: "scale"
        to: 1.0; duration: 150; easing.type: Easing.OutCubic
    }
    NumberAnimation {
        target: panel; property: "opacity"
        to: 1.0; duration: 150; easing.type: Easing.OutCubic
    }
}
```

Setting `panel.scale = 0.95` and `panel.opacity = 0.0` synchronously before starting the animation avoids a one-frame flash at full opacity.

### Close Animation

Close is triggered by three sources: `Keys.onEscapePressed`, `MouseArea.onClicked` (overlay), and `Connections { target: LauncherService; function onLaunched() }`. All three call the same `function startClose()` function on `root` rather than calling `LauncherSurface.hide()` directly.

```qml
function startClose() {
    closeAnimation.start()
}

SequentialAnimation {
    id: closeAnimation
    ParallelAnimation {
        NumberAnimation {
            target: panel; property: "scale"
            to: 0.95; duration: 150; easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: panel; property: "opacity"
            to: 0.0; duration: 150; easing.type: Easing.InCubic
        }
    }
    ScriptAction {
        script: LauncherSurface.notifyHideReady()
    }
}
```

The `ScriptAction` at the end of `SequentialAnimation` fires the C++ callback after the last animation frame is committed. C++ then sets `root.visible = false` and emits `visibleChanged`.

### `resetAndFocus()` QML Function

```qml
// Exposed on root so C++ can call it via QMetaObject::invokeMethod
function resetAndFocus() {
    LauncherService.setQuery("")
    root.forceActiveFocus()
    searchField.forceInputFocus()
}
```

This replaces the inline `Component.onCompleted` calls. `Component.onCompleted` still runs once at load time (cold start), but the C++-callable function is the authoritative reset path for subsequent shows.

---

## 4. Signal/Slot Wiring

### C++ → QML

| C++ action | QML effect |
|---|---|
| `view_->rootObject()->setVisible(true)` | `root.onVisibleChanged` fires → `openAnimation.start()` |
| `QMetaObject::invokeMethod(root, "resetAndFocus")` | `LauncherService.setQuery("")`, `searchField.forceInputFocus()` |
| `view_->rootObject()->setVisible(false)` (inside `notifyHideReady`) | panel is already at opacity 0 from close animation; root stops rendering |

### QML → C++

| QML event | C++ path |
|---|---|
| `Keys.onEscapePressed` in `root` | `root.startClose()` → `closeAnimation` → `ScriptAction: LauncherSurface.notifyHideReady()` |
| `MouseArea.onClicked` (background overlay) | `root.startClose()` |
| `LauncherService.launched()` signal | `Connections.onLaunched` → `root.startClose()` |
| `LauncherSearchField.onCloseRequested` | `root.startClose()` |
| `closeAnimation` `ScriptAction` | `LauncherSurface.notifyHideReady()` → C++ sets root `visible = false`, calls `setVisible(false)` |

### LauncherSurface `configured()` → Open (first show only)

```cpp
connect(surface_, &LayerSurface::configured, this, [this]() {
    resetAndFocusQml();
    view_->rootObject()->setVisible(true);
    setVisible(true);
}, Qt::SingleShotConnection);
```

`Qt::SingleShotConnection` ensures this fires only for the initial compositor configure. Subsequent shows go through `show()` directly setting root `visible`.

---

## 5. Key Decisions with Rationale

### Keep-Alive over Recreate

The current code calls `destroySurface()` on every `hide()`, which calls `view_->hide()` and `view_->deleteLater()`. This destroys the `wl_surface`, losing the layer-shell role binding. On the next `show()`, a new surface must negotiate a new configure round-trip with the compositor before the QML can appear. The round-trip introduces a visual delay (50–200ms depending on compositor) and produces a black flash on monitors with `wl_output` geometry negotiation.

Keep-alive avoids both: the surface stays mapped at all times; only the QML root's `visible` property toggles. This matches the established sidebar pattern (`SidebarManager`) and satisfies REQ-F-001 and REQ-NF-004.

### `notifyHideReady()` over `Behavior`

A `Behavior` on `visible` would require `visible` to change mid-animation, which is the opposite of what we need: we need `visible = true` during the close animation so the compositor keeps rendering the surface. `Behavior` on `scale`/`opacity` would trigger on every property change, including the pre-animation initialization step (`panel.scale = 0.95`), making the close sequence unreliable for rapid toggles. `SequentialAnimation` with a terminal `ScriptAction` gives a single, deterministic callback when the animation is fully done.

### `resetAndFocus()` Called from C++, Not `Component.onCompleted`

`Component.onCompleted` fires only once at load time and can race with the first `configured()` signal. On subsequent shows, the QML is already loaded; `Component.onCompleted` does not re-run. C++ calling `QMetaObject::invokeMethod(root, "resetAndFocus")` before setting `visible = true` guarantees the field is cleared and focused on every open, not just the first.

### `Qt::SingleShotConnection` on `configured()` for First Show

The `configured()` signal fires on every compositor reconfigure (e.g., monitor resolution change). If the connection were permanent, every reconfigure would reset the query and steal focus. `Qt::SingleShotConnection` limits the behavior to the initial surface setup. Post-initial shows are handled directly in `show()` without any signal.

### `SequentialAnimation` over `States`/`Transitions`

QML `State`/`Transition` machinery is designed for property states, but the close sequence has a side effect (calling `notifyHideReady()`) that must fire exactly once after the animation. `Transition`'s `onRunningChanged` is less predictable when multiple transitions fire in quick succession (e.g., open → close before open completes). Two named `Animation` objects (`openAnimation`, `closeAnimation`) started explicitly are simpler to reason about and easier to guard against rapid toggle races.

### `isBestMatch` as Delegate Property

The delegate in `Launcher.qml` already has access to `index` as a `required property`. Setting `isBestMatch: index === 0 && LauncherService.query.length > 0` at the delegate level avoids threading any extra state into the `LauncherResultRow` component itself; the component simply reacts to a boolean it receives.

---

## 6. Alternatives Considered

### Alternative: Recreate surface but pre-warm with `QQuickView::show()` before hiding

The `QQuickView` could be kept alive in a hidden state and a new layer-shell surface created on each `show()`. This avoids the `wl_surface` destruction problem but still incurs a configure round-trip cost on every open. Rejected because the round-trip latency is the core problem (REQ-NF-004), not the view allocation.

### Alternative: Use `opacity: 0` + `enabled: false` to fake hide without `visible = false`

Setting `opacity: 0; enabled: false` on the root keeps the surface mapped and avoids the `wl_surface` destruction issue. However, it does not release keyboard exclusivity (`keyboard_interactivity_exclusive` means the compositor routes all keyboard input to the surface regardless of `enabled`). Users would be unable to type in other applications while the launcher is "hidden". This also has subtle Qt rendering behavior: fully transparent items can still consume GPU compositing passes.

The correct approach is `root.visible = false`, which tells the Qt scene graph to skip rendering entirely and notifies the Wayland client that the surface content is not updated, giving the compositor permission to optimize it away.

### Alternative: `Behavior { NumberAnimation { ... } }` on `panel.scale` and `panel.opacity`

`Behavior` activates on any change to the target property. The open animation sets `panel.scale = 0.95` as the starting value synchronously before starting `openAnimation`. With `Behavior` active, that assignment would trigger its own 150ms animation from wherever `scale` was to 0.95, before the open animation begins — producing a double-animation. Suppressing `Behavior` for the initial assignment requires `enabled: false` toggling, which adds fragile state. Explicit named animations with explicit `start()` calls are unambiguous.

### Alternative: Close via `LauncherSurface.hide()` directly from QML

The current `Launcher.qml` calls `LauncherSurface.hide()` directly on Esc and launch events. If `hide()` immediately sets `visible = false`, the close animation never completes before the surface disappears. Inserting the animation before calling `hide()` (i.e., animating then calling `hide()` in `onCompleted`) would work, but it couples the QML to the C++ hide implementation timing. The `notifyHideReady()` pattern inverts this: QML decides when it is done; C++ responds. This is a cleaner interface boundary and matches the sidebar's `onClosingAnimationFinished()` convention.

---

## 7. Known Risks

### `configured()` Race on First Show

If the Wayland event loop delivers the `configured()` callback before `Qt::SingleShotConnection` is established (possible if `wl_surface_commit` and the subsequent `QCoreApplication::processEvents()` call land in the same event dispatch batch), the callback fires before the connection is made and the surface stays invisible. Mitigation: `Qt::SingleShotConnection` is registered immediately after `new LayerSurface(...)` and before `wl_surface_commit()`, matching the pattern used by `SidebarManager::configureSurface()`. The Wayland protocol guarantees that `configure` arrives asynchronously after at least one round-trip, so the connection will be established before the event arrives.

### Focus Timing on `resetAndFocusQml()`

`QMetaObject::invokeMethod(root, "resetAndFocus")` is called before `root->setVisible(true)`. On some Qt builds, `forceActiveFocus()` on an invisible item silently fails. Mitigation: call `resetAndFocusQml()` after setting `visible = true` (in `notifyHideReady()` path, this is correct by construction). For the first-show path (inside the `configured()` lambda), set `visible = true` first, then call `resetAndFocusQml()`.

### Animation During Rapid Toggle

If the user triggers `toggle()` while `closeAnimation` is running, `openAnimation.start()` should stop `closeAnimation` and start from the current `scale`/`opacity` values. Qt animates from the current value when `from` is not specified, so the open animation will correct the partially-closed state correctly. However, if `closeAnimation`'s `ScriptAction` has already been queued (it fires after the `ParallelAnimation` sub-block), it may fire after the open animation has begun, triggering `notifyHideReady()` spuriously. Mitigation: guard `notifyHideReady()` in C++:

```cpp
void LauncherSurface::notifyHideReady() {
    if (!closing_) {
        return;  // open animation already ran; ignore stale close callback
    }
    // ...
}
```

This matches the guard pattern used by `SidebarManager::onClosingAnimationFinished()`.

### `keyboard_interactivity_exclusive` Stays Set When Hidden

The layer-shell surface retains `keyboard_interactivity_exclusive` even when `root.visible = false`. This means the compositor still routes keyboard events to the launcher surface, but since the QML root is invisible, no items have focus and events are silently consumed. For a purely modal launcher this is acceptable: the user cannot interact with anything behind it while the surface exists. If this causes problems in practice (e.g., hotkey to re-open the launcher not firing), the fix is to call `surface_->set_keyboard_interactivity(keyboard_interactivity_none)` + `wl_surface_commit` in `notifyHideReady()`, and restore `keyboard_interactivity_exclusive` + commit in `show()`. This is omitted from the initial design to keep the diff minimal; the sidebar uses the same approach (toggling interactivity on open/close).
