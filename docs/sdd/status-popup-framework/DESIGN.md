# Status Popup Framework — DESIGN.md

**Feature:** Reusable layer-shell popup framework for four top-bar status widgets.
**Scope:** Empty styled container with positioning, animation, all four dismissal paths, and multi-monitor support.
**Based on:** SPEC.md + codebase review of `PopupSurface`, `TooltipSurface`, `TrayMenuSurface`, `LayerSurface`, `TrayMenuPopup.qml`, `SessionSection.qml`, `SessionPopup.qml`, `StatusesSection.qml`, widget files, `ShellApplication.cpp`, `CMakeLists.txt`.

---

## 1. Overview & Component Inventory

### New files

| File | Type | Responsibility |
|------|------|----------------|
| `src/surfaces/StatusPopupSurface.h` | C++ header | Singleton surface class declaration |
| `src/surfaces/StatusPopupSurface.cpp` | C++ impl | Surface lifecycle, positioning, per-id sizing |
| `src/qml/Topbar/StatusPopup.qml` | QML | Styled empty popup container with Canvas notch, title, entry animation |
| `src/qml/Topbar/StatusPopupTriggerArea.qml` | QML | Drop-in click/active-state area placed in each widget |

### Modified files

| File | Change |
|------|--------|
| `src/app/ShellApplication.h` | Add `StatusPopupSurface* status_popup_surface_` member; forward-declare class |
| `src/app/ShellApplication.cpp` | Construct `StatusPopupSurface`, register under `"StatusPopupSurface"` via `reg()` lambda |
| `CMakeLists.txt` | Add two `.cpp`/`.h` pairs to `holonight_surfaces`; add two `.qml` files to `HOLONIGHT_QML_FILES` |
| `src/qml/Topbar/NetworkWidget.qml` | Add `StatusPopupTriggerArea { popupId: "network"; ... }` |
| `src/qml/Topbar/AudioWidget.qml` | Add `StatusPopupTriggerArea { popupId: "audio"; ... }` |
| `src/qml/Topbar/BatteryWidget.qml` | Add `StatusPopupTriggerArea { popupId: "battery"; ... }` |
| `src/qml/Topbar/KeyboardLayoutWidget.qml` | Add `StatusPopupTriggerArea { popupId: "keyboard-layout"; ... }` |

QRC paths (from the `BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/qml"` alias rule in CMakeLists.txt):
- `qrc:/HolonightShell/Topbar/StatusPopup.qml`
- `qrc:/HolonightShell/Topbar/StatusPopupTriggerArea.qml`

---

## 2. Component Architecture: `StatusPopupSurface`

### Class shape

```cpp
// src/surfaces/StatusPopupSurface.h
#pragma once
#include "LayerShell.h"
#include "LayerSurface.h"
#include <QMap>
#include <QObject>
#include <QQmlEngine>
#include <QQuickView>
#include <QSize>
#include <QString>

class StatusPopupSurface : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool popupVisible   READ isPopupVisible  NOTIFY popupVisibleChanged)
  Q_PROPERTY(QString activePopupId READ activePopupId NOTIFY activePopupChanged)
  Q_PROPERTY(int pointerX        READ pointerX        NOTIFY geometryChanged)

 public:
  explicit StatusPopupSurface(QObject* parent = nullptr);
  ~StatusPopupSurface() override;
  // delete copy/move

  [[nodiscard]] bool    isPopupVisible() const { return popup_visible_; }
  [[nodiscard]] QString activePopupId()  const { return active_popup_id_; }
  [[nodiscard]] int     pointerX()       const { return pointer_x_; }

  Q_INVOKABLE void toggle(const QString& popup_id, const QString& screen_name,
                          int anchor_x, int anchor_width);
  Q_INVOKABLE void show  (const QString& popup_id, const QString& screen_name,
                          int anchor_x, int anchor_width);
  Q_INVOKABLE void hide  ();

 Q_SIGNALS:
  void popupVisibleChanged();
  void activePopupChanged();
  void geometryChanged();

 private:
  [[nodiscard]] bool ensureSurface(const QString& popup_id, const QString& screen_name,
                                   int anchor_x, int anchor_width);
  void destroySurface();
  void setPopupVisible(bool visible);
  void setActivePopupId(const QString& id);
  void setPointerX(int val);

  static QSize sizeForPopupId(const QString& popup_id);

  LayerShell   shell_;
  QQuickView*  view_    = nullptr;
  LayerSurface* surface_ = nullptr;
  QString       active_popup_id_;
  int           pointer_x_            = 0;
  bool          popup_visible_        = false;
  bool          pending_show_         = false;
  QString       pending_popup_id_;
  QString       pending_screen_;
  int           pending_anchor_x_     = 0;
  int           pending_anchor_width_ = 0;
};
```

### Per-id size lookup

A static private helper maps popup IDs to fixed QSize values:

```
"audio"           → QSize(900, 600)
"network"         → QSize(480, 320)
"battery"         → QSize(480, 320)
"keyboard-layout" → QSize(480, 320)
unknown ids       → QSize(480, 320)  (safe fallback)
```

This is called in `ensureSurface()` to pass to `surface_->set_size(sz.width(), sz.height())` and to drive the positioning arithmetic. C++ owns all sizing; QML never sets the window dimensions.

### Shell-active / pending pattern (mirrors PopupSurface exactly)

The constructor connects `shell_`'s `activeChanged` to a lambda that flushes the pending call:
```cpp
connect(&shell_, &QWaylandClientExtension::activeChanged, this, [this]() {
    if (shell_.isActive() && pending_show_) {
        pending_show_ = false;
        const bool created = ensureSurface(pending_popup_id_, pending_screen_,
                                           pending_anchor_x_, pending_anchor_width_);
        setPopupVisible(created);
    }
});
```
If `show()` / `toggle()` is called before the layer-shell global is active, parameters are stored in `pending_*` fields and `pending_show_` is set to `true`. The lambda fires once the compositor announces the global.

### toggle() flow

```
toggle(id, screen, ax, aw)
  if popup_visible_ && active_popup_id_ == id  →  hide()
  else                                          →  show(id, screen, ax, aw)
```

Calling `show()` while a different id is active (cross-widget switch) calls `show()` directly. The existing surface is destroyed at the top of `ensureSurface()` by calling `destroySurface()` first (same as `PopupSurface::show()` which always calls `destroySurface()` before `ensureSurface()`).

### show() flow

```
show(id, screen, ax, aw)
  if !shell_.isActive()  →  store pending_*, return
  nudge: PopupSurface::hide()    // one-way, REQ-F-006
  TooltipSurface::hide()         // dismiss any hover tooltip
  destroySurface()
  ensureSurface(id, screen, ax, aw)
  setPopupVisible(true) / setPopupVisible(false) on failure
```

The `PopupSurface` pointer is obtained directly from the module singleton (registered through the same `reg()` lambda pattern in `ShellApplication::registerQmlTypes()`). Because `holonight_surfaces` must not depend on `holonight_services`, and `PopupSurface` is in `holonight_surfaces`, the nudge is done by holding a raw `PopupSurface*` injected via constructor parameter or a static accessor. The cleanest approach matching the existing architecture is to accept `PopupSurface* popup_surface` in the constructor (mirroring how `TrayModel` receives `TrayMenuSurface*` via `setMenuSurface()`), then call `popup_surface_->hide()` directly. `StatusPopupSurface` holds:
```cpp
PopupSurface* session_popup_ = nullptr;  // set by ShellApplication after construction
```
`ShellApplication::startServices()` adds:
```cpp
status_popup_surface_->setSessionPopup(popup_surface_);
```

### ensureSurface() flow

Mirrors `TooltipSurface::ensureSurface()` step-for-step:

1. Resolve `QScreen*` by iterating `QGuiApplication::screens()` and matching `candidate->name() == screen_name`, falling back to `primaryScreen()` — identical to both `TooltipSurface` and `TrayMenuSurface`.
2. Compute `left_margin` and `pointer_x_` (see Section 4).
3. Create `QQuickView*` with `Qt::BypassWindowManagerHint`, `Qt::transparent`, `SizeRootObjectToView`, `create()`.
4. Obtain `wl_surface*` via `view_->nativeInterface<QNativeInterface::Private::QWaylandWindow>()->surface()`.
5. Obtain `wl_output*` via `screen->nativeInterface<QNativeInterface::QWaylandScreen>()->output()`.
6. Call `shell_.get_layer_surface(..., layer_top, "status-popup")`.
7. Construct `LayerSurface`, connect `closed` signal to `setPopupVisible(false)`.
8. Configure:
   ```cpp
   surface_->set_anchor(anchor_top | anchor_left);
   surface_->set_size(sz.width(), sz.height());
   surface_->set_exclusive_zone(0);
   surface_->set_margin(kBarGap, 0, 0, left_margin);
   ```
   where `kBarGap` (e.g., `kStatusPopupGap = 4`) is the visual gap below the bar — identical usage to `kTooltipGap = 4` in `TooltipSurface`.
9. Set keyboard interactivity (see Section 5 below).
10. Call `view_->setInitialProperties({{ "popupId", popup_id }})` so QML can render the correct title.
11. `view_->setSource(QUrl("qrc:/HolonightShell/Topbar/StatusPopup.qml"))`.
12. `wl_surface_commit(wlSurface)`.
13. Store `active_popup_id_`, emit `activePopupChanged()`.

### destroySurface()

```cpp
void StatusPopupSurface::destroySurface() {
    delete surface_;  surface_ = nullptr;
    delete view_;     view_    = nullptr;
}
```

Matching `PopupSurface::destroySurface()` exactly. On `hide()`:
```cpp
void StatusPopupSurface::hide() {
    if (view_ != nullptr) { view_->hide(); }
    destroySurface();
    setPopupVisible(false);
    setActivePopupId({});
}
```
Note `destroySurface()` is called (not just `view_->hide()`), matching `TrayMenuSurface::hide()`. This ensures a fresh `configure` cycle on the next `show()`.

---

## 3. Positioning Algorithm

Taken directly from `TooltipSurface::ensureSurface()`, generalised to use the per-id popup width.

### Constants

```cpp
static constexpr int kStatusPopupGap         = 4;   // matches kTooltipGap
static constexpr int kScreenEdgeMargin       = 8;   // matches both TooltipSurface and TrayMenuSurface
static constexpr int kPopupNotchRadius       = 8;   // panel corner radius; pointer must stay inside
```

### Computation (mirrors `TooltipSurface` lines 107–110)

```cpp
const QSize sz           = sizeForPopupId(popup_id);
const int   popup_width  = sz.width();
const int   screen_width = screen->geometry().width();

// Center popup under anchor, then clamp to screen.
const int centered_left  = anchor_x + (anchor_width / 2) - (popup_width / 2);
const int max_left       = std::max(kScreenEdgeMargin,
                                    screen_width - popup_width - kScreenEdgeMargin);
const int left_margin    = std::clamp(centered_left, kScreenEdgeMargin, max_left);

// Pointer X = widget center relative to popup left edge.
const int raw_pointer_x  = anchor_x + (anchor_width / 2) - left_margin;

// Clamp so notch stays within panel's rounded corners.
const int min_pointer    = kPopupNotchRadius + 1;
const int max_pointer    = popup_width - kPopupNotchRadius - 1;
pointer_x_               = std::clamp(raw_pointer_x, min_pointer, max_pointer);
```

The `left_margin` is passed to `surface_->set_margin(kStatusPopupGap, 0, 0, left_margin)`, placing the popup at `(left_margin, kStatusPopupGap)` relative to the screen's top-left. This is identical to how `TooltipSurface` uses `left_margin` in `set_margin(kTooltipGap, 0, 0, left_margin)`.

`pointer_x_` is exposed via the `Q_PROPERTY int pointerX` and read by `StatusPopup.qml`'s Canvas to draw the notch position.

### Multi-monitor correctness

`screen->geometry()` returns the monitor's own geometry (not a global position), consistent with `TooltipSurface`. The `anchor_x` passed from QML via `mapToGlobal(...).x` is in global coordinates; since the layer-shell `set_margin` left value is relative to the output's origin, and the screen geometry used here is the same screen's geometry, the arithmetic is correct. This also matches `TrayMenuSurface`'s pattern of computing `screen_local_request_x = screen_x - screen_geometry.x()`.

---

## 4. Keyboard Interactivity / Esc Dismissal

### Current `LayerSurface` state

`LayerSurface` (in `src/platform/LayerSurface.cpp`) only implements `zwlr_layer_surface_v1_configure` and `zwlr_layer_surface_v1_closed`. It does not call `set_keyboard_interactivity()`. The class inherits `QtWayland::zwlr_layer_surface_v1` which exposes `set_keyboard_interactivity(uint32_t keyboard_interactivity)` directly. No new parameter or method is needed in `LayerSurface` itself.

### How TrayMenuPopup achieves Esc

`TrayMenuPopup.qml` (line 46, `focus: true`) and (line 115–119):
```qml
focus: true
Keys.onEscapePressed: {
    if (root.menuClient) { root.menuClient.close() }
}
```
The `TrayMenuSurface` does not call `set_keyboard_interactivity()` explicitly, so the surface currently gets focus only if the compositor grants it opportunistically. However, if the compositor does focus the surface, the `focus: true` on the root item enables Qt to deliver the key event.

For `StatusPopupSurface` we must call:
```cpp
surface_->set_keyboard_interactivity(
    QtWayland::zwlr_layer_surface_v1::keyboard_interactivity_on_demand);
```
immediately after constructing `surface_` in `ensureSurface()`, before `wl_surface_commit`. This matches the layer-shell spec for transient popups (request focus when shown).

In `StatusPopup.qml`, the root `Item` sets `focus: true` and handles:
```qml
Keys.onEscapePressed: { StatusPopupSurface.hide() }
```
This is the direct analogue of `TrayMenuPopup.qml`'s pattern — same `focus: true`, same `Keys.onEscapePressed` handler, replacing `menuClient.close()` with `StatusPopupSurface.hide()`.

### Risk note

`keyboard_interactivity_on_demand` requests focus; Hyprland may not grant it for `layer_top` surfaces depending on compositor policy. If testing reveals Esc fails, the outside-click overlay (Section 6) and widget-click toggle remain functional fallbacks.

---

## 5. Click-Outside Dismissal

### SessionService mechanism (the pattern to reuse)

`SessionPopup.qml` does NOT implement an overlay inside the popup QML file itself. Instead, `SessionSection.qml` uses `PopupSurface` (which owns its own `QQuickView`) for the popup content, and there is no separate dismiss-overlay surface in the current code. The `SessionSection.qml` `MouseArea` (id `hoverArea`) calls `PopupSurface.hide()` on click when `popupVisible` is true, and `PopupSurface.show()` otherwise. The popup's own size is small enough that clicks outside it naturally land on the bar or on the desktop (which the compositor handles to withdraw focus). There is no dedicated dismiss overlay surface in the current session popup implementation.

For the status popup, which is larger and can float above arbitrary content, an explicit dismiss mechanism is needed. The design uses a fullscreen transparent `MouseArea` as a separate `Item` placed at the root of `StatusPopup.qml`:

```qml
// StatusPopup.qml root Item
Item {
    id: root
    width: <popup_width>    // set by view_ dimensions
    height: <popup_height>
    focus: true

    // Dismiss overlay: fullscreen transparent capture layer BEHIND the panel.
    // Clicking outside the panel dismisses the popup.
    MouseArea {
        id: dismissOverlay
        // Fill the view (which is sized to the popup, not the screen).
        // Since the layer-shell surface is sized to the popup content,
        // this overlay only covers the popup area itself.
        // Therefore: use a separate dismiss surface approach (see below).
    }
}
```

Because `StatusPopupSurface` sizes the `QQuickView` to exactly the popup dimensions (via `set_size(sz.width(), sz.height())`), the surface boundary IS the popup boundary. A click outside the surface area is not received by this view at all — it falls through to whatever is beneath.

**The correct approach** (consistent with the popup-sized surface) is to set the popup window's size to be large enough to include a transparent overlay region. However, this would complicate the layer-shell sizing.

**Chosen approach (matching `TrayMenuSurface` pattern):** Size the layer-shell surface to be `screen_width x screen_height` (or a sufficiently large value), place the actual popup panel `Rectangle` at `x: left_margin, y: kStatusPopupGap` inside a full-screen transparent root `Item`. The `MouseArea` fills the root item but has a child `Item` at the panel position that `contains()` returns false for — clicks outside the panel dismiss via `StatusPopupSurface.hide()`, clicks inside do not.

Actually, examining `TrayMenuSurface` more carefully: it sizes the surface to just the content area (e.g., `kMenuWidth x surface_height`) and does not have a click-outside overlay — clicking outside the tray menu closes it via keyboard-interactivity focus loss. For status popups, the explicit dismiss overlay is required by the spec.

**Final design:** create a separate, fullscreen transparent dismiss-overlay surface using the same pattern as `StatusPopupSurface`'s own surface. Specifically:

The `StatusPopupSurface` manages two layer-shell resources:
1. The **dismiss surface**: `layer_bottom` (or `layer_top` with `exclusive_zone = -1`), fullscreen, transparent, anchored `top | bottom | left | right`, keyboard-interactivity none. Its QML is a single `MouseArea { anchors.fill: parent; onClicked: StatusPopupSurface.hide() }`.
2. The **popup surface**: `layer_top`, popup-sized, positioned as computed.

The dismiss surface is shown first. Clicking anywhere on screen that is NOT occluded by the popup surface hits the dismiss surface. The popup surface sits above it; clicks inside the popup do NOT reach the dismiss surface (Wayland pointer events go to the top-most surface).

Both `view_dismiss_` and `surface_dismiss_` are created and destroyed together with the popup in `ensureSurface()` / `destroySurface()`.

```cpp
// Additional members:
QQuickView*   view_dismiss_    = nullptr;
LayerSurface* surface_dismiss_ = nullptr;
```

`destroySurface()` deletes both pairs (surface before view, per the existing comment in `LayerShellManager`).

The dismiss QML (`StatusPopupDismissOverlay.qml`, flat in `src/qml/Topbar/`):
```qml
import QtQuick
import HolonightShell

Item {
    id: root
    MouseArea {
        anchors.fill: parent
        onClicked: StatusPopupSurface.hide()
    }
}
```

This QML is loaded into `view_dismiss_` via `view_dismiss_->setSource(QUrl("qrc:/HolonightShell/Topbar/StatusPopupDismissOverlay.qml"))`. Since the dismiss surface covers the whole output, any off-popup click is caught.

The dismiss surface configuration:
```cpp
surface_dismiss_->set_anchor(anchor_top | anchor_bottom | anchor_left | anchor_right);
surface_dismiss_->set_size(0, 0);  // stretch-fill the output
surface_dismiss_->set_exclusive_zone(-1);  // don't push other surfaces
surface_dismiss_->set_margin(0, 0, 0, 0);
// keyboard_interactivity: none (default)
```

This adds one more file to `HOLONIGHT_QML_FILES`:
- `src/qml/Topbar/StatusPopupDismissOverlay.qml`
- QRC path: `qrc:/HolonightShell/Topbar/StatusPopupDismissOverlay.qml`

---

## 6. QML Component Design

### `StatusPopup.qml`

**Location:** `src/qml/Topbar/StatusPopup.qml`
**QRC path:** `qrc:/HolonightShell/Topbar/StatusPopup.qml`

```qml
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import HolonightShell
import Holonight

Item {
    id: root

    // Injected via setInitialProperties from StatusPopupSurface::ensureSurface().
    required property string popupId

    readonly property var popupTitles: ({
        "audio":           "Audio",
        "network":         "Network",
        "battery":         "Battery",
        "keyboard-layout": "Keyboard Layout"
    })
    readonly property string displayTitle: root.popupTitles[root.popupId] ?? root.popupId

    // Notch height: transparent band at top where the notch triangle is drawn.
    readonly property int notchBandHeight: 10
    readonly property int panelRadius: 8

    focus: true

    Keys.onEscapePressed: {
        StatusPopupSurface.hide()
    }

    // Entry animation state.
    property real entryProgress: 0.0
    NumberAnimation on entryProgress {
        from: 0.0; to: 1.0
        duration: 250
        easing.type: Easing.OutCubic
        running: true
    }

    opacity: root.entryProgress
    transform: Translate { y: (1.0 - root.entryProgress) * 8 }

    // --- Canvas notch (top of popup, pointing upward) ---
    Canvas {
        id: notchCanvas
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: root.notchBandHeight
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const px = root.pointerX  // WRONG: must be qualified
            // All access qualified with root.* per CLAUDE.md / REQ-F-019:
            const ptrX = StatusPopupSurface.pointerX
            const bh   = root.notchBandHeight
            ctx.beginPath()
            ctx.moveTo(ptrX - bh, bh)
            ctx.lineTo(ptrX,      0)
            ctx.lineTo(ptrX + bh, bh)
            ctx.closePath()
            ctx.fillStyle = HoloniightPalette.surface
            ctx.fill()
            ctx.strokeStyle = HoloniightPalette.borderActive
            ctx.lineWidth = 1
            ctx.stroke()
        }
        Connections {
            target: StatusPopupSurface
            function onGeometryChanged() { notchCanvas.requestPaint() }
        }
        Component.onCompleted: requestPaint()
    }

    // --- Panel body ---
    Rectangle {
        id: panel
        anchors {
            top: notchCanvas.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        radius: root.panelRadius
        color: HoloniightPalette.surface
        border.color: HoloniightPalette.borderActive
        border.width: 1
    }

    // Glow: declared BEFORE title text (MultiEffect z-order rule from CLAUDE.md).
    MultiEffect {
        source: panel
        anchors.fill: panel
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentCyan
        shadowBlur: 0.4
        shadowOpacity: 0.22
        shadowScale: 1.03
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    // Placeholder title header.
    Text {
        id: titleText
        anchors {
            top: panel.top
            left: panel.left
            right: panel.right
            topMargin: 12
            leftMargin: 16
        }
        text: root.displayTitle
        color: HoloniightPalette.onSurface
        font.pixelSize: 14
        font.weight: Font.Medium
    }

    // Content area (empty for this iteration).
    Item {
        id: contentArea
        anchors {
            top: titleText.bottom
            topMargin: 8
            left: panel.left
            right: panel.right
            bottom: panel.bottom
            margins: 16
        }
    }
}
```

Key structural notes:
- `notchBandHeight: 10` creates a transparent strip at the very top of the window. The layer-shell window height includes this band. The Triangle is drawn in the Canvas; the region outside the triangle is transparent (not painted).
- The notch points **upward** — vertex at `(ptrX, 0)`, base at `(ptrX ± bh, bh)`.
- `MultiEffect` is declared before `titleText` per the z-order rule documented in CLAUDE.md.
- All Canvas `onPaint` property accesses use `StatusPopupSurface.pointerX` and `root.*` — no bare names.
- Entry animation uses `NumberAnimation on entryProgress` driving `opacity` and a `Translate` transform (y: 8px downward at start, slides up to 0).
- `popupId` is a `required property string` set by `view_->setInitialProperties({{ "popupId", popup_id }})`.

### `StatusPopupTriggerArea.qml`

**Location:** `src/qml/Topbar/StatusPopupTriggerArea.qml`
**QRC path:** `qrc:/HolonightShell/Topbar/StatusPopupTriggerArea.qml`

```qml
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import HolonightShell

Item {
    id: root

    required property string popupId
    required property string barMonitorName

    anchors.fill: parent

    readonly property bool isActivePopup: StatusPopupSurface.popupVisible
                                       && StatusPopupSurface.activePopupId === root.popupId

    MouseArea {
        anchors.fill: parent
        onClicked: {
            TooltipSurface.hide()
            const globalPos = root.mapToGlobal(0, 0)
            StatusPopupSurface.toggle(root.popupId,
                                      root.barMonitorName,
                                      Math.round(globalPos.x),
                                      Math.round(root.width))
        }
    }
}
```

The `isActivePopup` property is the binding point for widget active styling. Each widget reads `triggerArea.isActivePopup` (or binds directly to `StatusPopupSurface.popupVisible && StatusPopupSurface.activePopupId === "widget-id"`) to control its `hoverFrame` color and glow.

---

## 7. Widget Integration

Each widget file is modified by adding a `StatusPopupTriggerArea` instance and wiring the active-state styling. The `BarTooltipArea` and existing `HoverHandler` are preserved unchanged.

### Pattern (identical for all four widgets)

Inside each `BarSection { id: root; required property string barMonitorName; ... }`:

```qml
// Add after BarTooltipArea:
StatusPopupTriggerArea {
    id: popupTrigger
    popupId: "<id>"           // "network" | "audio" | "battery" | "keyboard-layout"
    barMonitorName: root.barMonitorName
}
```

The `hoverFrame` `Rectangle` already exists in all four widgets with a `color` driven by `hoverHandler.hovered`. The active-state overrides these bindings:

```qml
// hoverFrame.color — updated binding
color: popupTrigger.isActivePopup
    ? Qt.rgba(HoloniightPalette.surfaceVariant.r, HoloniightPalette.surfaceVariant.g,
              HoloniightPalette.surfaceVariant.b, 0.72)
    : Qt.rgba(HoloniightPalette.surfaceVariant.r, HoloniightPalette.surfaceVariant.g,
              HoloniightPalette.surfaceVariant.b, hoverHandler.hovered ? 0.5 : 0.0)

// hoverFrame.border.color — updated binding
border.color: popupTrigger.isActivePopup
    ? HoloniightPalette.borderActive
    : hoverHandler.hovered
        ? Qt.rgba(HoloniightPalette.borderActive.r, HoloniightPalette.borderActive.g,
                  HoloniightPalette.borderActive.b, 0.1)
        : "transparent"

// MultiEffect.visible (NetworkWidget, AudioWidget, BatteryWidget)
visible: hoverHandler.hovered || popupTrigger.isActivePopup
```

The active-state check is `StatusPopupSurface.activePopupId === "<id>"` (via `popupTrigger.isActivePopup`), not `PopupSurface.popupVisible`. This satisfies REQ-F-026 — session popup visibility has no effect on status widget highlight state.

`KeyboardLayoutWidget` currently lacks a `MultiEffect` for glow; its hover styling uses only the `hoverFrame` Rectangle, so only the color/border bindings are updated. The same active-state binding is applied.

---

## 8. Data Flow Diagram

```
USER CLICK on NetworkWidget
        |
        v
StatusPopupTriggerArea.MouseArea.onClicked
    TooltipSurface.hide()
    root.mapToGlobal(0, 0) → globalPos
    StatusPopupSurface.toggle("network", barMonitorName, globalPos.x, root.width)
        |
        v
StatusPopupSurface::toggle()
    popup_visible_ && active_popup_id_ == "network"?
    YES → hide()                              NO → show("network", ...)
              |                                          |
              v                                          v
        destroySurface()                    popup_surface_->hide()   // nudge PopupSurface
        setPopupVisible(false)              TooltipSurface::hide()
        setActivePopupId("")                destroySurface()
                                            ensureSurface("network", screen, ax, aw)
                                                |
                                                v
                                          resolve QScreen
                                          compute left_margin, pointer_x_
                                          create QQuickView + wl_surface
                                          shell_.get_layer_surface(...)
                                          new LayerSurface(...)
                                          set_keyboard_interactivity(on_demand)
                                          set_anchor(top|left)
                                          set_size(480, 320)
                                          set_exclusive_zone(0)
                                          set_margin(4, 0, 0, left_margin)
                                          [dismiss surface created in parallel]
                                          setInitialProperties({"popupId","network"})
                                          setSource("qrc:.../StatusPopup.qml")
                                          wl_surface_commit
                                                |
                                                v
                                      LayerSurface::zwlr_layer_surface_v1_configure()
                                          ack_configure(serial)
                                          window_->resize(w, h)
                                          window_->show()          // view_ shown here
                                                |
                                                v
                                        StatusPopup.qml renders:
                                          entryProgress animation
                                          Canvas notch at StatusPopupSurface.pointerX
                                          panel Rectangle + MultiEffect glow
                                          "Network" title text

DISMISSAL PATHS:
  [A] Widget click (active)    → toggle() → hide() path above
  [B] Different widget click   → show() with new id → destroySurface() + ensureSurface()
  [C] Outside click            → StatusPopupDismissOverlay.MouseArea.onClicked
                                    → StatusPopupSurface.hide()
  [D] Esc key                  → StatusPopup.qml Keys.onEscapePressed
                                    → StatusPopupSurface.hide()
```

---

## 9. Interfaces / APIs

### C++ public API (`StatusPopupSurface`)

```cpp
// Q_PROPERTYs (all READ-ONLY from QML):
bool    popupVisible  // NOTIFY popupVisibleChanged
QString activePopupId // NOTIFY activePopupChanged
int     pointerX      // NOTIFY geometryChanged

// Q_INVOKABLEs (callable from QML):
void toggle(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width)
void show  (const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width)
void hide  ()

// C++-only (not exposed to QML):
void setSessionPopup(PopupSurface* popup_surface)   // called from ShellApplication
```

### QML-facing contract

```qml
// From any QML file after `import HolonightShell`:
StatusPopupSurface.popupVisible       // bool
StatusPopupSurface.activePopupId      // string, "" when hidden
StatusPopupSurface.pointerX           // int, pixel offset within popup
StatusPopupSurface.toggle(id, screen, anchorX, anchorWidth)
StatusPopupSurface.show(id, screen, anchorX, anchorWidth)
StatusPopupSurface.hide()
```

### `StatusPopupTriggerArea` required props

```qml
required property string popupId        // "network"|"audio"|"battery"|"keyboard-layout"
required property string barMonitorName // propagated from BarSection root
```

### `StatusPopup` required props

```qml
required property string popupId  // injected via setInitialProperties
```

---

## 10. Key Decisions with Rationale

**Per-id sizing in C++, not QML**
Sizes live in `StatusPopupSurface::sizeForPopupId()`. The layer-shell surface must be sized before `wl_surface_commit` — QML cannot resize the window after the configure/ack cycle without creating a new surface. Centralising sizes in C++ keeps the QML content-agnostic and prevents future content PRs from breaking geometry.

**One-way nudge to `PopupSurface`, not a coordinator**
REQ-C-005 explicitly forbids a shared coordinator. `StatusPopupSurface` calls `popup_surface_->hide()` before showing. `PopupSurface` is never modified. This is a direct analogue of how `PopupSurface::show()` already calls `destroySurface()` unconditionally without consulting other surfaces.

**Flat file layout in `src/qml/Topbar/`**
Matches existing widgets (`NetworkWidget.qml`, `AudioWidget.qml`, etc.). A `StatusPopup/` subdirectory was rejected because all existing widget QML is flat in `Topbar/`. The SPEC's subdirectory path is overridden by this design per the stated correction.

**Canvas notch at TOP of panel, pointing upward**
The popup sits below the bar. The notch must point at the triggering widget above. The canvas occupies a `notchBandHeight`-pixel transparent band at the top of the window. The panel `Rectangle` begins below the band. The triangle vertex points toward `y: 0` (toward the bar).

**`keyboard_interactivity_on_demand`**
This is the correct value for transient popups — request focus when mapped, release when hidden. `keyboard_interactivity_exclusive` would steal focus from all other surfaces permanently. The `on_demand` value mirrors the intent of `TrayMenuPopup.qml`'s `focus: true` pattern.

**Dismiss overlay as a second layer-shell surface**
The popup `QQuickView` is sized to the popup content. There is no mechanism to receive clicks outside the layer-shell surface boundary. A full-screen transparent surface in `layer_bottom` catches all off-popup clicks without occluding desktop windows (they render above `layer_bottom`).

---

## 11. Alternatives Considered

**Extending `PopupSurface` instead of a new class**
`PopupSurface` is tied to `SessionPopup.qml` and hardcodes `anchor_top | anchor_right` and a fixed 160×150 size. Adding per-id sizing, pointer-x, `activePopupId`, and `anchor_left` logic would require significant refactoring and would complicate the session popup. A new class is cleaner and allows both to evolve independently.

**Shared `PopupCoordinator` singleton**
Rejected by REQ-C-005. A coordinator would create bidirectional coupling (session popup would need to know about status popups to hide them). The one-way nudge is simpler and sufficient.

**`rotated Rectangle` vs Canvas notch**
A rotated `Rectangle` (45° diamond clipped to triangle) has well-known z-order and clipping issues in QML and cannot easily have a border that matches the panel border. A `Canvas` is the established pattern in this codebase (used in `StatusesSection.qml` for the frame shape) and gives pixel-accurate control.

**In-popup overlay (single surface) for click-outside**
If the popup surface covered the entire screen (full-screen transparent layer), a `MouseArea` inside the popup could dismiss on off-panel clicks. However, this would capture pointer events globally, blocking scrolling and clicks on all desktop windows while the popup is open. A dedicated `layer_bottom` dismiss surface is more composable.

---

## 12. Known Risks

**Risk 1: Esc focus on Hyprland layer_top**
`keyboard_interactivity_on_demand` is sent before the compositor issues `configure`. Hyprland 0.55.2 may ignore or defer focus for `layer_top` surfaces with `on_demand`, meaning `Keys.onEscapePressed` in `StatusPopup.qml` never fires. Mitigation: the outside-click dismiss surface (Path C) and widget-click toggle (Path A) remain functional regardless. Document as a known limitation if Esc fails in manual testing.

**Risk 2: Pointer X at screen edges on multi-monitor layouts**
`anchor_x` is a global X coordinate passed from `mapToGlobal(0, 0).x`. The screen geometry returned by `screen->geometry()` may have a non-zero `x()` origin on multi-monitor setups. The computation `anchor_x + anchorWidth/2 - left_margin` is correct only if `left_margin` is relative to the screen's own origin — which it is, since `set_margin(top, right, bottom, left)` in the layer-shell protocol positions relative to the output's own bounds. Verify manually on a multi-monitor session; TrayMenuSurface explicitly computes `screen_local_request_x = screen_x - screen_geometry.x()` as a safeguard — apply the same correction.

**Risk 3: Dismiss surface and popup surface ordering**
`surface_dismiss_` must be created and committed before `surface_` (popup) to ensure the dismiss surface is below the popup in compositor stacking. In practice, layer shells are stacked by layer (bottom < top), so `layer_bottom` dismiss is always below `layer_top` popup regardless of creation order. This is inherent to the Wayland layer-shell protocol.

**Risk 4: `notchBandHeight` transparent strip visibility**
The transparent notch band at the top of the popup window may show background compositor content (desktop wallpaper) between the bar and the notch body. This is intentional — the notch triangle is drawn against this transparent background. However, if the bar's exclusive zone has already displaced the output area, the gap between bar bottom and popup top is only `kStatusPopupGap = 4` pixels, which should be occupied by the notch drawing.

**Risk 5: All four dismissal paths under combined use**
Per the spec's Risk 5, all four paths must be tested in combination. Cross-widget switching (Path B) calls `show()` with a new id, which calls `destroySurface()` + `ensureSurface()`, potentially leaving the dismiss surface from the previous popup alive for a frame. Verify that `destroySurface()` deletes both `surface_dismiss_` and `surface_` atomically.

---

## 13. CMake / Registration Changes

### `CMakeLists.txt` — `HOLONIGHT_QML_FILES`

Add the following three entries (must keep the list alphabetically sorted to match `HOLONIGHT_DISCOVERED_QML_FILES`):

```cmake
src/qml/Topbar/StatusPopup.qml
src/qml/Topbar/StatusPopupDismissOverlay.qml
src/qml/Topbar/StatusPopupTriggerArea.qml
```

Sorted insertion position: after `src/qml/Topbar/SessionSection.qml` and before `src/qml/Topbar/TooltipPopup.qml` (alphabetical by filename).

### `CMakeLists.txt` — `holonight_surfaces` target

Add to the `add_library(holonight_surfaces STATIC ...)` source list:

```cmake
src/surfaces/StatusPopupSurface.h
src/surfaces/StatusPopupSurface.cpp
```

### `src/app/ShellApplication.h`

Add forward declaration and member:
```cpp
class StatusPopupSurface;     // forward declaration, alongside existing PopupSurface

// In private members:
StatusPopupSurface* status_popup_surface_ = nullptr;
```

### `src/app/ShellApplication.cpp`

1. Add `#include "StatusPopupSurface.h"` alongside existing surface includes.
2. Construct in initializer list:
   ```cpp
   status_popup_surface_(new StatusPopupSurface(this)),
   ```
   (after `popup_surface_` and `tooltip_surface_`, before `tray_menu_surface_`).
3. Register in `registerQmlTypes()`:
   ```cpp
   reg(status_popup_surface_, "StatusPopupSurface");
   ```
   This uses the same `reg()` lambda already in use for all other singletons, which calls `qmlRegisterSingletonType<T>("HolonightShell", 1, 0, name, [obj](...) -> QObject* { return obj; })`.
4. Wire session-popup nudge in `startServices()`:
   ```cpp
   status_popup_surface_->setSessionPopup(popup_surface_);
   ```
