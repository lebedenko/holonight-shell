# topbar-session — Architecture Design

**Document ID**: topbar-session  
**Phase**: MVP  
**Status**: Final  
**Date**: 2026-05-21  
**Spec**: `docs/sdd/topbar-session/SPEC.md`

---

## 1. Overview

The feature adds a canvas-drawn power button at the right edge of the topbar. Clicking it toggles a fly-out popup with four session actions: Lock, Logout, Reboot, Shutdown. Actions delegate to a new C++ singleton `SessionService` that fires fire-and-forget `QProcess::startDetached` calls.

No new Wayland protocol, D-Bus dependency, or background thread is required. The implementation is the simplest in the topbar series.

---

## 2. New Files

### C++ (service)

| File | Purpose |
|---|---|
| `src/SessionService.h` | Class declaration; QML singleton registration macros |
| `src/SessionService.cpp` | Four `Q_INVOKABLE` method bodies |

### QML

| File | QRC alias | Purpose |
|---|---|---|
| `src/qml/Topbar/SessionSection.qml` | `Topbar/SessionSection.qml` | Trigger button (Canvas power symbol) + popup host |
| `src/qml/Topbar/SessionPopup.qml` | `Topbar/SessionPopup.qml` | Fly-out rectangle with four action items |

### Assets (already present, need CMake wiring)

| Asset path | QRC alias |
|---|---|
| `assets/bar-icons/system-lock-screen-symbolic.svg` | `bar-icons/system-lock-screen-symbolic.svg` |
| `assets/bar-icons/system-log-out-symbolic.svg` | `bar-icons/system-log-out-symbolic.svg` |
| `assets/bar-icons/system-reboot-symbolic.svg` | `bar-icons/system-reboot-symbolic.svg` |
| `assets/bar-icons/system-shutdown-symbolic.svg` | `bar-icons/system-shutdown-symbolic.svg` |

Note: `system-log-out-symbolic.svg` is not present in `assets/bar-icons/` as of this writing — it must be sourced or substituted before implementation. The other three exist. See Known Risks.

---

## 3. CMakeLists.txt Changes

### 3a. Add SessionService to the executable source list

```cmake
qt6_add_executable(holonight-shell
    ...
    src/SessionService.h
    src/SessionService.cpp
)
```

### 3b. QT_RESOURCE_ALIAS for the two new QML files

```cmake
set_source_files_properties(src/qml/Topbar/SessionSection.qml
    PROPERTIES QT_RESOURCE_ALIAS "Topbar/SessionSection.qml")
set_source_files_properties(src/qml/Topbar/SessionPopup.qml
    PROPERTIES QT_RESOURCE_ALIAS "Topbar/SessionPopup.qml")
```

Add both paths to the `QML_FILES` list inside `qt6_add_qml_module`.

### 3c. Bundle the four SVG icons as QRC resources

Icons are static assets, not QML files, so they go in a separate `qt6_add_resources` call (or a `RESOURCES` argument to `qt6_add_qml_module`). The cleanest approach that matches the existing module setup is adding a dedicated resource set:

```cmake
qt6_add_resources(holonight-shell "bar_icons"
    PREFIX "/HolonightShell"
    FILES
        assets/bar-icons/system-lock-screen-symbolic.svg
        assets/bar-icons/system-log-out-symbolic.svg
        assets/bar-icons/system-reboot-symbolic.svg
        assets/bar-icons/system-shutdown-symbolic.svg
    BASE assets
)
```

`BASE assets` causes the alias to be `bar-icons/<filename>.svg`, so the QRC path becomes `qrc:/HolonightShell/bar-icons/system-lock-screen-symbolic.svg`. QML references these as `"qrc:/HolonightShell/bar-icons/system-lock-screen-symbolic.svg"`.

---

## 4. SessionService (C++)

### 4a. Header — `src/SessionService.h`

```cpp
#pragma once

#include <QObject>
#include <QQmlEngine>

class SessionService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  explicit SessionService(QObject* parent = nullptr);

  SessionService(const SessionService&) = delete;
  SessionService& operator=(const SessionService&) = delete;
  SessionService(SessionService&&) = delete;
  SessionService& operator=(SessionService&&) = delete;

  Q_INVOKABLE void lockScreen();
  Q_INVOKABLE void logout();
  Q_INVOKABLE void reboot();
  Q_INVOKABLE void shutdown();
};
```

No properties or signals are needed — the service is command-only.

### 4b. Implementation — `src/SessionService.cpp`

```cpp
#include "SessionService.h"
#include <QProcess>

SessionService::SessionService(QObject* parent) : QObject(parent) {}

void SessionService::lockScreen() {
  // No-op in this phase. Future: invoke swaylock or hyprlock.
}

void SessionService::logout() {
  QProcess::startDetached("hyprctl", {"dispatch", "exit"});
}

void SessionService::reboot() {
  QProcess::startDetached("systemctl", {"reboot"});
}

void SessionService::shutdown() {
  QProcess::startDetached("systemctl", {"poweroff"});
}
```

`QProcess::startDetached` is non-blocking; the child process is detached from the parent and runs independently. This satisfies REQ-NF-001.

### 4c. QML registration in `main.cpp`

Follow the exact pattern used for every other service:

```cpp
#include "SessionService.h"
...
auto* session = new SessionService(&app);
QQmlEngine::setObjectOwnership(session, QQmlEngine::CppOwnership);
qmlRegisterSingletonType<SessionService>("HolonightShell", 1, 0, "SessionService",
    [session](QQmlEngine*, QJSEngine*) -> QObject* { return session; });
```

The `QML_ELEMENT` + `QML_SINGLETON` macros in the header are required for qmllint type resolution. The actual runtime singleton binding is done via `qmlRegisterSingletonType` with the lambda, which returns the same C++ instance to every QML engine (one per monitor) — identical to the AudioService and NetworkService pattern.

---

## 5. QML Component Tree

```
TopBar.qml (RowLayout)
└── SessionSection.qml  (BarSection subtype)
    ├── Canvas  (power symbol)
    ├── MouseArea  (toggle popupOpen)
    └── SessionPopup.qml  (child Item, anchored above trigger)
        ├── MouseArea (background dismiss overlay, z: -1)
        └── Rectangle (popup background)
            └── Column
                ├── SessionActionItem { label: "Lock";     icon: "system-lock-screen-symbolic.svg" }
                ├── SessionActionItem { label: "Logout";   icon: "system-log-out-symbolic.svg" }
                ├── SessionActionItem { label: "Reboot";   icon: "system-reboot-symbolic.svg" }
                └── SessionActionItem { label: "Shutdown"; icon: "system-shutdown-symbolic.svg" }
```

`SessionActionItem` is an inline `Component` or a repeated `Row` delegate inside `SessionPopup.qml` — not a separate file, since it is used only once and has no reuse value.

---

## 6. SessionSection.qml

`SessionSection` extends `BarSection` (a `Rectangle` with implicit sizing and a `wheeled` signal). It holds:

- A fixed `implicitWidth` (e.g., `42`) matching its visual footprint.
- A `property bool popupOpen: false` that drives popup visibility.
- A `Canvas` that draws the power symbol (arc + vertical tick).
- A `MouseArea` covering the canvas that toggles `popupOpen`.
- A `SessionPopup` as a direct child, positioned via `anchors`.

### Canvas power symbol

The symbol is a circle arc (roughly 300° sweep, gap at top) with a short vertical line extending upward through the gap. Drawn in `onPaint` using `context.arc` and `context.moveTo/lineTo`. Stroke color is `HoloniightPalette.onSurface` at rest; transitions to `HoloniightPalette.primary` when `popupOpen` is true or on hover.

Using `Canvas` rather than an SVG `Image` avoids a hard dependency on a "power button" icon that is not in the standard icon theme and keeps the geometry consistent with the bar's scale. The arc can be redrawn when the palette changes by calling `requestPaint()` in an `onPopupOpenChanged` handler.

### Popup anchoring

`SessionPopup` is placed as a **child of `SessionSection`**, not a child of `TopBar` or a top-level window. This keeps coordinate math simple — no `mapToItem` required.

```qml
SessionPopup {
    id: popup
    open: root.popupOpen
    anchors {
        right: parent.right
        bottom: parent.top
        bottomMargin: 4
    }
    onDismiss: root.popupOpen = false
}
```

`anchors.bottom: parent.top` places the popup immediately above the trigger button. `anchors.right: parent.right` right-aligns it to the button edge. A `bottomMargin: 4` provides a small gap.

Because `SessionSection` is in a `RowLayout` with `rightMargin: 12` on the bar, the popup right edge lands ~12 px from the screen right edge, which is visually correct.

---

## 7. SessionPopup.qml

### Properties

```qml
property bool open: false
signal dismiss()
```

`open` is data-bound from `SessionSection.popupOpen`. `dismiss` is emitted by the background overlay click so the parent can set `popupOpen: false` (avoids two-way binding).

### Structure

```qml
Item {
    id: root
    width: 180
    height: contentCol.implicitHeight + 24
    visible: opacity > 0   // keep in layout only when animating

    // Background dismiss overlay — covers the entire topbar Item
    // z: -1 keeps it behind the popup rectangle but in front of bar content
    MouseArea {
        z: -1
        // width/height mapped to cover topbar; see Section 8
        onClicked: root.dismiss()
    }

    Rectangle {
        anchors.fill: parent
        color: "#1f2335"        // REQ-F-202 hardcoded exception
        border.color: "#bb9af7" // REQ-F-202 hardcoded exception
        border.width: 1
        radius: 8

        Column {
            id: contentCol
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 12 }
            spacing: 4
            // Four Row items (icon + label) here
        }
    }

    // Animation — see Section 9
}
```

### Action item structure (inline delegate)

Each of the four rows uses the same shape:

```qml
Row {
    spacing: 8
    width: parent.width

    MouseArea {
        anchors.fill: parent
        onClicked: {
            SessionService.<method>()
            root.dismiss()
        }
    }

    Image {
        width: 16; height: 16
        source: "qrc:/HolonightShell/bar-icons/<icon>.svg"
    }

    Text {
        text: "<Label>"
        color: HoloniightPalette.onSurface
        font.family: "JetBrains Mono"
        font.pixelSize: 13
    }
}
```

---

## 8. Popup Dismiss Logic

The background overlay `MouseArea` must cover the region that is *not* the popup rectangle itself, so that clicks anywhere on the bar (or the screen, if the bar window is full-width) close the popup. There are two viable approaches:

**Option A — Parent-relative overlay (chosen)**  
Map the overlay to the topbar's root Item using `parent.mapFromItem(topBarRoot, 0, 0)`. Because `SessionSection` is inside a `RowLayout` inside `TopBar`, use a `Connections` target to the topbar or pass a reference. This is slightly coupled but avoids any window-level layering.

**Option B — Full-screen overlay via a top-level `Window`**  
Create a transparent `Window` at `Qt.WindowFullScreen` to absorb click-away events. Overkill for a single-output Wayland bar with no floating windows.

Option A is used. The dismiss `MouseArea` inside `SessionPopup` is sized to cover the topbar's `Item` bounds (acquired via an `id` reference threaded from `TopBar` through `SessionSection` to `SessionPopup`), with `z: -1` keeping it below the popup rectangle. Only the popup rectangle catches events above the overlay.

Alternatively, if wiring the reference is cumbersome: set the overlay to a large fixed size (e.g., `width: 4000; height: 200`) anchored at `bottom: parent.bottom` with negative x offset. This works in practice because the bar is a single layer-shell surface.

The trigger button toggle (REQ-F-205) is handled by the `MouseArea` on the `Canvas`: `onClicked: root.popupOpen = !root.popupOpen`. When the popup is open, clicking the button sets `popupOpen` to false, which drives `open: false` in the popup, triggering the close animation.

---

## 9. Animation

Both opacity and vertical translation are animated with `Behavior` blocks on the popup `Item`:

```qml
// Inside SessionPopup Item:
property real slideOffset: open ? 0 : -12

opacity: open ? 1.0 : 0.0
y: slideOffset

Behavior on opacity {
    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
}
Behavior on y {
    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
}
```

`Behavior` is preferred over explicit `SequentialAnimation` / `ParallelAnimation` here because both open and close transitions reuse the same curve without duplicating duration/easing. The popup animates in (opacity 0→1, y -12→0) and out (opacity 1→0, y 0→-12) symmetrically.

`visible: opacity > 0` ensures the `Item` is hidden from hit-testing once fully closed, without needing an additional state machine.

The `slideOffset` property binds to `open`, so the y-Behavior fires automatically on state change. No explicit `state`/`transitions` block is required.

---

## 10. Key Decisions with Rationale

### Canvas vs. SVG Image for the power symbol

The power symbol is not included in standard Freedesktop icon themes or in `assets/bar-icons/`. Sourcing a custom SVG that exactly matches the HoloNight geometry requires a design asset that does not yet exist. Drawing the symbol on a `Canvas` avoids an asset dependency, allows color to be driven directly by palette tokens (no `ColorOverlay` layer needed), and lets the arc geometry be expressed as a function of the button's pixel size. It also matches the existing `BatteryWidget` pattern of drawing geometric primitives rather than relying on icon files for the primary indicator.

### Popup as child of SessionSection vs. top-level Item

Keeping the popup as a direct child of `SessionSection` means anchoring to `parent.top` / `parent.right` just works without coordinate mapping. A top-level `Item` inside `TopBar` or a floating `Window` would require `mapToItem` calls and state synchronization. The bar is a single layer-shell surface; popup overflow above the bar edge is rendered by the compositor at the top of the bar rect — acceptable for MVP. Panel-level popup support (drawing outside the bar surface bounds) is deferred to a future session.

### `Behavior` vs. explicit `NumberAnimation` targets

`Behavior` blocks are more concise and correctly handle the case where `open` changes multiple times before the animation completes (Qt automatically restarts the Behavior from the current value). Explicit `to`/`from` animations require manual `stop()` before restarting.

### `qmlRegisterSingletonType` lambda vs. `QML_SINGLETON` factory

All existing services use `qmlRegisterSingletonType` with a lambda capturing the pre-constructed instance. `QML_SINGLETON` alone would require a `create()` static factory method and prevents the C++ side from holding a pointer to pass the instance to other systems. The lambda approach is used for consistency.

### No `SessionService` properties or signals

Unlike `AudioService` or `NetworkService`, `SessionService` has no observable state to reflect back to QML — it is a pure command object. Adding `Q_PROPERTY` or signals would create dead interface surface. The service has four methods and nothing else.

---

## 11. Alternatives Considered

| Alternative | Why not chosen |
|---|---|
| Use `image://icon/system-power-symbolic` for trigger | Not present in standard icon theme; would render as blank on many systems |
| Popup as a top-level `Window` | Requires Wayland surface management; far more complexity than needed for an anchored child Item |
| `SequentialAnimation` / `ParallelAnimation` for popup | More verbose than `Behavior` with no benefit for a simple two-property toggle |
| QProcess with `waitForFinished` | Blocks the event loop; explicitly prohibited by REQ-NF-001 |
| `system("hyprctl dispatch exit")` | Inherits shell environment, blocks, returns int — all inferior to `QProcess::startDetached` |
| Separate `SessionActionItem.qml` file | Single use, no reuse value; inline delegation keeps the component count low |

---

## 12. Known Risks

### Missing logout icon

`assets/bar-icons/system-log-out-symbolic.svg` does not exist as of this design. Either:
- Source the SVG from Papirus or Adwaita icon themes and copy to `assets/bar-icons/`, or
- Fall back to `image://icon/system-log-out-symbolic` (system icon theme lookup) for the logout item only, consistent with how `AudioSection` uses `image://icon/`.

The `image://icon/` provider is already wired in the binary. Using it for all four popup icons (dropping the QRC SVG approach) is a valid alternative that eliminates the asset management issue entirely, at the cost of system-theme-dependent icon appearance.

### Popup overflow above the bar surface

The bar is a `wlr-layer-shell` surface with a fixed height. If the popup content (4 items × ~32 px + padding ≈ ~160 px) exceeds the space available above the bar, the popup will be clipped by the compositor. The bar is at the top of the screen; the popup anchors *above* it (`anchors.bottom: parent.top`), which means it extends off the top of the screen. This is a fundamental constraint of layer-shell: surfaces cannot draw outside their allocated region.

Mitigations for MVP: render the popup *inside* the bar surface (anchor it below the trigger button with `anchors.top: parent.bottom`) so it overlaps bar content but stays within the surface bounds. This deviates from the spec's "above" requirement but is visually equivalent at the bar's y=0 position.

The correct long-term fix (popup as a separate layer-shell surface or an overlay layer) is deferred to topbar-hud-polish (session 9).

### `systemctl reboot` / `systemctl poweroff` privilege requirements

On a typical Arch/systemd system with a seat session (logind), `systemctl reboot` and `systemctl poweroff` do not require sudo for the logged-in seat user. If polkit is not configured or the user is not in the `wheel` group equivalents, the commands will fail silently (fire-and-forget). A future phase may want to check the exit code via a connected `QProcess` rather than `startDetached`, but that is out of scope per REQ-NF-001 / REQ-C-301.

---

## 13. Integration Checklist (for implementation)

- [ ] `src/SessionService.h` and `src/SessionService.cpp` created
- [ ] `CMakeLists.txt`: `SessionService.h/cpp` added to `qt6_add_executable`
- [ ] `CMakeLists.txt`: `QT_RESOURCE_ALIAS` set for `SessionSection.qml` and `SessionPopup.qml`
- [ ] `CMakeLists.txt`: both QML files added to `QML_FILES` in `qt6_add_qml_module`
- [ ] `CMakeLists.txt`: `qt6_add_resources` block for four SVG icons (or icon paths resolved via `image://icon/`)
- [ ] `src/main.cpp`: `SessionService` instantiated and registered with `qmlRegisterSingletonType`
- [ ] `src/qml/Topbar/SessionSection.qml` created
- [ ] `src/qml/Topbar/SessionPopup.qml` created
- [ ] `src/qml/Topbar/TopBar.qml`: `SessionSection` added after `StatusSection`
- [ ] `system-log-out-symbolic.svg` sourced or substituted
- [ ] `task build` passes clean
- [ ] `task qml-lint` passes clean
- [ ] Manual: popup appears above (or inside-overlapping) trigger button on click
- [ ] Manual: fade-in + slide animation visible
- [ ] Manual: click-away dismisses popup
- [ ] Manual: Logout terminates Hyprland session
