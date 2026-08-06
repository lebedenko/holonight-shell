# Right Sidebar — Architecture Design

**Feature:** Right Sidebar Panel with Tab Navigation
**Version:** 1.0
**Date:** 2026-06-13
**Status:** Design
**Spec:** [SPEC.md](SPEC.md)

---

## 1. Component Overview

### New Files

#### C++ (src/surfaces/)

| File | Role |
|------|------|
| `SidebarManager.h` | `QObject` registered by `ShellApplication` as the `HolonightShell.SidebarManager` QML singleton. Owns open/closing `QQuickView` + `LayerSurface` instances keyed by monitor name, exposes `toggle(monitorName)`, `close(monitorName)`, `closeAll()` as `Q_INVOKABLE`. Tracks per-monitor open state, active tab, and bounded panel height. |
| `SidebarManager.cpp` | Implements create-on-open / destroy-after-close lifecycle, fullscreen host setup, panel height coordination, tab-state caching, and monitor removal cleanup. |

#### QML (src/qml/RightSidebar/)

| File | Role |
|------|------|
| `RightSidebar.qml` | Fullscreen host component. Receives `barMonitorName` and `active` as required properties. Owns launcher-style outside-click dismissal, the panel event absorber, the open/close `SequentialAnimation`, and the tab-switch width animation. Hosts `SidebarTabBar` and `SidebarContent` inside a clipped right-aligned panel container. |
| `SidebarTabBar.qml` | Fixed 64px-wide `Column`. Renders one `SidebarTabButton` per tab. Emits `tabSelected(int index)` signal to `RightSidebar`. |
| `SidebarTabButton.qml` | One tab entry: a Canvas-drawn icon plus an active-state highlight rect. Properties: `tabIndex`, `isActive`, `iconName`. |
| `SidebarContent.qml` | A `ScrollView` wrapping a `Loader` that instantiates the active tab component. Receives `currentTab` (int) from `RightSidebar`. Exposes `contentOpacity` property that the fade animation drives. |
| `SidebarOverview.qml` | Overview tab, placeholder content. Declares `property int preferredWidth: 360`. |
| `SidebarCalendar.qml` | Calendar tab, placeholder content. Declares `property int preferredWidth: 400`. |
| `SidebarNotifications.qml` | Notifications tab, placeholder content. Declares `property int preferredWidth: 380`. |
| `SidebarSystem.qml` | System tab, placeholder content. Declares `property int preferredWidth: 340`. |
| `SidebarQuickSettings.qml` | Quick Settings tab, placeholder content. Declares `property int preferredWidth: 320`. |
| `SidebarMedia.qml` | Media tab, placeholder content. Declares `property int preferredWidth: 300`. |

### Relationships

```
ShellApplication
  └── SidebarManager (std::unique_ptr, declared after layer_shell_)
        └── [per open/closing monitor] SidebarSurface { QQuickView + LayerSurface }  <-- fullscreen sidebar host
              └── RightSidebar.qml (barMonitorName, active)
                    ├── fullscreen background MouseArea → close on outside click
                    └── right-aligned clipped panel
                          ├── panel MouseArea → absorb inside clicks
                          ├── SidebarTabBar.qml
                          │     └── SidebarTabButton.qml × 6
                          └── SidebarContent.qml
                                └── Loader → SidebarOverview/Calendar/… .qml
```

---

## 2. C++ Layer

### 2.1 SidebarManager Class

```cpp
class SidebarManager : public QObject {
  Q_OBJECT
 public:
  SidebarManager(LayerShell& shell, QObject* parent = nullptr);

  void start();
  Q_INVOKABLE void toggle(const QString& monitor_name);
  Q_INVOKABLE void close(const QString& monitor_name);
  Q_INVOKABLE void closeAll();
  Q_INVOKABLE void onClosingAnimationFinished(const QString& monitor_name);
  Q_INVOKABLE void onContentHeightChanged(const QString& monitor_name, int height);
  Q_INVOKABLE void onCurrentTabChanged(const QString& monitor_name, int tab_index);

  [[nodiscard]] bool isOpen(const QString& monitor_name) const;

 Q_SIGNALS:
  void sidebarOpened(const QString& monitor_name);
  void sidebarClosed(const QString& monitor_name);

 private:
  struct SidebarSurface {
    QQuickView* view = nullptr;
    LayerSurface* surface = nullptr;
    wl_surface* wl_surface_ptr = nullptr;
  };

  void openOnMonitor(const QString& monitor_name);
  void closeOnMonitor(const QString& monitor_name);
  bool createSurface(const QString& monitor_name);
  void destroySurface(const QString& monitor_name);
  int boundedHeight(const QString& monitor_name, int requested_height) const;
  QScreen* findScreen(const QString& monitor_name) const;
  QQuickView* findView(const QString& monitor_name) const;

  LayerShell& shell_;                         // shared active layer-shell global
  bool started_ = false;
  QHash<QString, bool> open_state_;           // monitor_name → isOpen
  QHash<QString, int> stored_heights_;        // monitor_name → last bounded panel height
  QHash<QString, int> current_tabs_;          // monitor_name → last active tab
  QHash<QString, SidebarSurface> surfaces_;   // monitor_name → open/closing surface
};
```

**Key design notes:**

- `toggle(monitorName)`: if `open_state_[monitorName]` is true, calls `closeOnMonitor(monitorName)`; otherwise calls `closeAll()` then `openOnMonitor(monitorName)`.
- `closeAll()`: iterates `open_state_`, calls `closeOnMonitor()` on every entry where value is true.
- `openOnMonitor(name)`: creates the fullscreen host surface if needed, sets `open_state_[name] = true`, restores cached panel height/current tab into the root object, sets `active = true`, and emits `sidebarOpened(name)`.
- `closeOnMonitor(name)`: sets `open_state_[name] = false`, sets `active = false` on the root QML object (triggers fade-out animation), and emits `sidebarClosed(name)`.
- `onClosingAnimationFinished(name)`: called by `RightSidebar.qml` at the end of the close animation. It destroys the `LayerSurface` and `QQuickView` unless the sidebar has already been reopened.
- `onContentHeightChanged(name, height)`: stores the bounded panel height outside the QML view so recreate-on-open can restore it.
- `onCurrentTabChanged(name, index)`: stores the active tab outside the QML view so recreate-on-open can restore it.
- `findScreen()` / `findView()`: resolve monitor objects from `QGuiApplication::screens()` and the manager-owned `surfaces_` map.

### 2.2 createSurface

```cpp
bool SidebarManager::createSurface(const QString& monitor_name) {
  QScreen* screen = findScreen(monitor_name);
  // Create QQuickView, bind wlr-layer-shell, then configure a fullscreen host.
  surface.set_anchor(
    QtWayland::zwlr_layer_surface_v1::anchor_top |
    QtWayland::zwlr_layer_surface_v1::anchor_bottom |
    QtWayland::zwlr_layer_surface_v1::anchor_left |
    QtWayland::zwlr_layer_surface_v1::anchor_right);
  surface.set_size(0, 0);
  surface.set_exclusive_zone(0);
  surface.set_margin(0, 0, 0, 0);
  surface.set_keyboard_interactivity(
      QtWayland::zwlr_layer_surface_v1::keyboard_interactivity_exclusive);
}
```

The host surface is created only for an open or closing sidebar. It is destroyed after the QML close animation completes, matching the robust launcher lifecycle and ensuring the compositor releases focus/input ownership.

New constants in `ShellConstants.h`:

```cpp
inline constexpr int kSidebarTabBarWidth     = 64;
inline constexpr int kSidebarRightMargin     = 24;
inline constexpr int kSidebarBottomMargin    = 24;
inline constexpr int kSidebarTopMargin       = kBarHeight + 8;  // 72
inline constexpr int kSidebarMaxContentWidth = 400;  // max over all tab preferredWidths
```

### 2.3 QML Source

```cpp
view->setInitialProperties({
    {QStringLiteral("barMonitorName"), monitor_name},
    {QStringLiteral("active"), false},
    {QStringLiteral("currentTab"), current_tabs_.value(monitor_name, 0)},
    {QStringLiteral("panelHeight"), boundedHeight(monitor_name, stored_or_default_height)},
});
view->setSource(QUrl(QStringLiteral("qrc:/HolonightShell/RightSidebar/RightSidebar.qml")));
```

The `active: false` initial property keeps the sidebar logically off until `openOnMonitor()` flips it to true. `currentTab` and `panelHeight` are restored from C++ caches because the QML view is recreated on each open.

### 2.5 Fullscreen Host And Dismissal

Configuration:
- Anchor: `top | bottom | left | right` (stretch to fill non-exclusive area, i.e. below the topbar)
- Size: `0, 0` (compositor fills to available space)
- Exclusive zone: `0`
- Margin: `0, 0, 0, 0`
- Namespace: `sidebar`
- Layer: `top`
- QML source: `qrc:/HolonightShell/RightSidebar/RightSidebar.qml`
- Initial property: `barMonitorName` (so the host can call `SidebarManager.close(barMonitorName)`)
- Keyboard interactivity: `exclusive` while open so the root can receive `Escape`

The sidebar uses a launcher-style single-surface model. `RightSidebar.qml` fills the host surface with a background `MouseArea` that closes on outside click. The right-aligned panel contains an inner `MouseArea` that absorbs clicks before they reach the background, so tab buttons, session actions, and calendar controls remain interactive. After the close animation finishes, `SidebarManager` destroys the host layer surface and QML view.

### 2.6 Wiring into ShellApplication

In `ShellApplication.h`, add after the existing manager declarations:

```cpp
// Declared after layer_shell_ so the sidebar surfaces tear down while LayerShell is alive.
std::unique_ptr<SidebarManager> sidebar_manager_;
```

Forward-declare `SidebarManager` in the header alongside the existing forward declarations.

In `ShellApplication.cpp`, `startLayerSurfaces()`:

```cpp
sidebar_manager_ = std::make_unique<SidebarManager>(*layer_shell_, this);
QQmlEngine::setObjectOwnership(sidebar_manager_.get(), QQmlEngine::CppOwnership);
qmlRegisterSingletonType<SidebarManager>(
    "HolonightShell", 1, 0, "SidebarManager",
    [this](QQmlEngine*, QJSEngine*) -> QObject* { return sidebar_manager_.get(); });
sidebar_manager_->start();
```

`SidebarManager` is instantiated and registered before topbar QML loads, so `ClockSection.qml` can call `SidebarManager.toggle(root.barMonitorName)`. It is destructed before `layer_shell_` due to declaration order (members destruct in reverse).

### 2.7 Mutual Exclusion

`toggle(monitorName)` calls `closeAll()` before `openOnMonitor()`:

```cpp
void SidebarManager::toggle(const QString& monitor_name) {
  if (open_state_.value(monitor_name, false)) {
    closeOnMonitor(monitor_name);
    return;
  }
  closeAll();
  openOnMonitor(monitor_name);
}

void SidebarManager::closeAll() {
  for (auto it = open_state_.begin(); it != open_state_.end(); ++it) {
    if (it.value()) {
      closeOnMonitor(it.key());
    }
  }
}
```

`closeOnMonitor()` sets `active = false` synchronously. The QML close animation runs asynchronously, but the `open_state_` entry is set to `false` immediately so a rapid double-toggle does not re-open a closing sidebar.

---

## 3. QML Layer

### 3.1 RightSidebar.qml

Root item. Fills the fullscreen host `QQuickView`; the right-aligned panel is constrained to `kSidebarTabBarWidth + current tab width`. All visual clipping and width animation happen inside this item.

```qml
import QtQuick
import QtQuick.Effects
import Holonight
import HolonightShell

Item {
    id: root

    required property string barMonitorName
    required property bool active

    property int currentTab: 0  // 0..5, persisted in-memory across open cycles
    property bool ready: false

    Component.onCompleted: root.ready = true

    onActiveChanged: {
        if (!root.ready) return
        if (root.active) {
            closeAnimation.stop()
            tabSwitchAnimation.stop()
            openAnimation.start()
        } else {
            openAnimation.stop()
            tabSwitchAnimation.stop()
            closeAnimation.start()
        }
    }

    // Clip container — grows from 0 to full height during phase 1
    Item {
        id: clipContainer
        anchors.top: parent.top
        anchors.right: parent.right
        width: tabBar.width + contentArea.width
        height: 0  // animated
        clip: true

        SidebarTabBar {
            id: tabBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            currentTab: root.currentTab
            active: root.active
            onTabSelected: (idx) => root.currentTab = idx
        }

        SidebarContent {
            id: contentArea
            anchors.top: parent.top
            anchors.left: tabBar.right
            anchors.bottom: parent.bottom
            width: 0  // animated
            currentTab: root.currentTab
            active: root.active
        }
    }

    // Phase 1 grow + Phase 2 slide + Phase 3 fade
    SequentialAnimation {
        id: openAnimation
        NumberAnimation {
            target: clipContainer; property: "height"
            from: 0; to: root.height
            duration: 160; easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: contentArea; property: "width"
            from: 0; to: contentArea.preferredWidthForTab(root.currentTab)
            duration: 200; easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: contentArea; property: "contentOpacity"
            from: 0.0; to: 1.0
            duration: 120; easing.type: Easing.OutCubic
        }
    }

    // Phase 1 fade + Phase 2 slide-left + Phase 3 shrink-up
    SequentialAnimation {
        id: closeAnimation
        NumberAnimation {
            target: contentArea; property: "contentOpacity"
            from: 1.0; to: 0.0
            duration: 100; easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: contentArea; property: "width"
            from: contentArea.width; to: 0
            duration: 160; easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: clipContainer; property: "height"
            from: clipContainer.height; to: 0
            duration: 140; easing.type: Easing.InCubic
        }
        ScriptAction { script: SidebarManager.onClosingAnimationFinished(root.barMonitorName) }
    }

    // Tab switch: width-only animation, no grow/fade
    NumberAnimation {
        id: tabSwitchAnimation
        target: contentArea; property: "width"
        duration: 180; easing.type: Easing.OutCubic
    }

    function preferredWidthForTab(idx) {
        // Returns the preferredWidth of the tab at the given index.
        // Delegated to SidebarContent which resolves via its width table.
        return contentArea.preferredWidthForTab(idx)
    }

}
```

The close animation calls `SidebarManager.onClosingAnimationFinished()` to set `visible = false` on the root item after the close animation finishes. The C++ callback ignores stale close completions if the sidebar was reopened during the animation.

**`ready` guard**: `setInitialProperties` fires `onActiveChanged` before `Component.onCompleted`. Without the `ready` flag, the `active = false` initial property would start `closeAnimation` on an already-closed item.

### 3.2 SidebarTabBar.qml

```qml
import QtQuick
import Holonight
import HolonightShell

Item {
    id: root

    width: 64
    property int currentTab: 0
    property bool active: false

    signal tabSelected(int index)

    readonly property var tabMeta: [
        { iconName: "overview",        label: "Overview"       },
        { iconName: "calendar",        label: "Calendar"       },
        { iconName: "notifications",   label: "Notifications"  },
        { iconName: "system",          label: "System"         },
        { iconName: "quick-settings",  label: "Quick Settings" },
        { iconName: "media",           label: "Media"          }
    ]

    Column {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 4
        topPadding: 12

        Repeater {
            model: root.tabMeta
            SidebarTabButton {
                required property var modelData
                required property int index
                tabIndex: index
                iconName: modelData.iconName
                isActive: root.currentTab === index
                onClicked: root.tabSelected(index)
            }
        }
    }
}
```

### 3.3 SidebarTabButton.qml

Each button is 48×48 within the 64px bar. The tab rail is on the left side of the sidebar, so the active state renders a 2px right-edge accent line pointing toward the content. The icon is drawn in a 48×48 `Canvas` using 48px coordinates; it is not scaled at paint time.

```qml
import QtQuick
import Holonight
import HolonightShell

Item {
    id: root

    width: 48; height: 48

    property int tabIndex: 0
    property string iconName: ""
    property bool isActive: false

    signal clicked()

    readonly property color _active: HoloniightPalette.accentCyan
    readonly property color _inactive: HoloniightPalette.textSubtle

    // Active right-side indicator
    Rectangle {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 2; height: 24
        radius: 1
        color: root._active
        visible: root.isActive
    }

    Canvas {
        id: iconCanvas
        anchors.centerIn: parent
        width: 48; height: 48
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            // dispatch to per-icon draw function based on root.iconName
            root.drawIcon(ctx)
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }

    onIsActiveChanged: iconCanvas.requestPaint()
    on_ActiveChanged: iconCanvas.requestPaint()
    on_InactiveChanged: iconCanvas.requestPaint()

    function drawIcon(ctx) {
        // Each iconName maps to a distinct draw function, all using
        // root._active / root._inactive palette tokens.
        // Implementations follow the same pattern as AudioIcon.qml:
        // ctx.beginPath(), ctx.moveTo(), ctx.lineTo(), ctx.strokeStyle, ctx.stroke()
    }
}
```

### 3.4 SidebarContent.qml

```qml
import QtQuick
import QtQuick.Controls as Controls
import HolonightShell

Item {
    id: root

    property int currentTab: 0
    property bool active: false
    property real contentOpacity: 0.0
    readonly property var defaultWidths: [360, 400, 380, 340, 320, 300]

    // Called by RightSidebar to determine animation target width
    function preferredWidthForTab(idx) {
        return defaultWidths[idx] ?? 360
    }

    Controls.ScrollView {
        anchors.fill: parent
        clip: true
        opacity: root.contentOpacity

        Loader {
            id: loader
            width: root.width - 16
            height: Math.max(implicitHeight, scrollView.availableHeight)
            active: root.active
            source: root.tabSource(root.currentTab)
        }
    }

    function tabSource(idx) {
        const sources = [
            "qrc:/HolonightShell/RightSidebar/SidebarOverview.qml",
            "qrc:/HolonightShell/RightSidebar/SidebarCalendar.qml",
            "qrc:/HolonightShell/RightSidebar/SidebarNotifications.qml",
            "qrc:/HolonightShell/RightSidebar/SidebarSystem.qml",
            "qrc:/HolonightShell/RightSidebar/SidebarQuickSettings.qml",
            "qrc:/HolonightShell/RightSidebar/SidebarMedia.qml"
        ]
        return sources[idx] ?? sources[0]
    }
}
```

The `Loader.active` property is bound to `root.active`: when the sidebar is hidden, the tab content item is destroyed, pausing all timers and signal connections (REQ-NF-001/002). On re-open, the Loader re-instantiates the component. `defaultWidths` mirrors the tab page `preferredWidth` declarations and must be kept in sync until the content width resolver is made dynamic.

### 3.5 Tab Page Components

Each tab page is structurally identical for this iteration:

```qml
// SidebarOverview.qml (same pattern for all six)
import QtQuick
import Holonight
import HolonightShell

Item {
    id: root

    property int preferredWidth: 360  // tab's declared content width

    // Placeholder content
    Text {
        anchors.centerIn: parent
        text: "Overview"
        color: HoloniightPalette.textPrimary
    }
}
```

Preferred widths per tab:
- `SidebarOverview.qml`: 360
- `SidebarCalendar.qml`: 400
- `SidebarNotifications.qml`: 380
- `SidebarSystem.qml`: 340
- `SidebarQuickSettings.qml`: 320
- `SidebarMedia.qml`: 300

`kSidebarMaxContentWidth` in `ShellConstants.h` remains 400 (the maximum of these values) for policy calculations and panel-width constraints; the host surface itself is fullscreen.

### 3.6 Outside Click Dismissal

```qml
import QtQuick
import HolonightShell

Item {
    id: root

    required property string barMonitorName
    required property bool active

    MouseArea {
        anchors.fill: parent
        enabled: root.active
        onClicked: SidebarManager.close(root.barMonitorName)
    }

    Item {
        id: panel

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        // SidebarTabBar and SidebarContent live here.
    }

    Keys.onEscapePressed: SidebarManager.close(root.barMonitorName)
}
```

The fullscreen host receives outside clicks directly. The panel-level `MouseArea` accepts all buttons and prevents clicks inside the sidebar from reaching the background dismiss area. `SidebarManager` enables keyboard interactivity while open and destroys the host surface after the close animation.

### 3.7 ClockSection.qml Cleanup

Replace the existing `MouseArea.onClicked` handler in `ClockSection.qml`:

**Before:**
```qml
MouseArea {
    onClicked: {
        if (PopupSurface.popupVisible) {
            PopupSurface.hide();
        } else {
            PopupSurface.show(root.barMonitorName);
        }
    }
}
```

**After:**
```qml
MouseArea {
    onClicked: SidebarManager.toggle(root.barMonitorName)
}
```

The clock no longer opens `PopupSurface`; it opens the right sidebar instead. `PopupSurface` itself is retained because it backs the session popup (`SessionSection.qml`) and is still used by `StatusPopupSurface` for session-popup mutual exclusion.

---

## 4. Data Flow Diagram

```
User clicks clock widget on monitor "DP-1"
        │
        ▼
ClockSection.qml  (barMonitorName = "DP-1")
  MouseArea.onClicked
        │
        ▼  Q_INVOKABLE call
SidebarManager::toggle("DP-1")
        │
        ├── closeAll()                 ← closes any open sidebar on other monitors
        │     └── closeOnMonitor("HDMI-A-1")
        │           ├── open_state_["HDMI-A-1"] = false
        │           └── view->rootObject()->setProperty("active", false)
        │
        └── openOnMonitor("DP-1")
              ├── open_state_["DP-1"] = true
              ├── createSurface("DP-1")                              [fullscreen host captures input]
              ├── view->rootObject()->setProperty("currentTab", cached_tab)
              ├── view->rootObject()->setProperty("panelHeight", cached_height)
              ├── view->rootObject()->setProperty("active", true)    [triggers onActiveChanged]
              └── emit sidebarOpened("DP-1")

        Within RightSidebar.qml on monitor "DP-1":
        onActiveChanged (active = true, ready = true)
              │
              ▼
        openAnimation.start()
              │
              ├── Phase 1: clipContainer.height  0 → screen_height - 72 - 24  (160ms, OutCubic)
              ├── Phase 2: contentArea.width      0 → preferredWidthForTab(0)  (200ms, OutCubic)
              └── Phase 3: contentArea.contentOpacity  0.0 → 1.0              (120ms, OutCubic)

User clicks outside sidebar
        │
        ▼
RightSidebar.qml background MouseArea.onClicked
  SidebarManager.close("DP-1")           ← Q_INVOKABLE
        │
        ▼
SidebarManager::closeOnMonitor("DP-1")
        ├── open_state_["DP-1"] = false
        └── view->rootObject()->setProperty("active", false)

        Within RightSidebar.qml:
        onActiveChanged (active = false)
              │
              ▼
        closeAnimation.start()
              │
              ├── Phase 1: contentArea.contentOpacity  1.0 → 0.0   (100ms, InCubic)
              ├── Phase 2: contentArea.width  current → 0           (160ms, InCubic)
              ├── Phase 3: clipContainer.height  current → 0        (140ms, InCubic)
              └── ScriptAction: SidebarManager.onClosingAnimationFinished("DP-1")
                        │
                        └── SidebarManager destroys the LayerSurface and QQuickView unless reopened
```

---

## 5. Key Decisions with Rationale

### Why custom create-on-open manager

The sidebar uses a fullscreen interactive layer surface that owns keyboard and pointer focus while open. Keeping that surface mapped after close caused Hyprland to keep routing input away from the previously focused application in practice. `SidebarManager` therefore follows the robust launcher/status-popup lifecycle: create the layer surface on open, keep it alive through the close animation, then destroy the `LayerSurface` and `QQuickView`. Per-monitor state that should survive recreation, such as the active tab and last bounded content height, is stored in C++.

### Why fullscreen host (no surface resize during animation)

Wayland layer-shell resizes require a round-trip configure cycle between client and compositor. During an animation this would cause visible tearing or a stutter as the compositor reflows the surface. The fullscreen host means the QML clip and width animations operate entirely within an already-sized surface, avoiding resize round-trips while the sidebar is visible. The surface is transparent outside the visible content area.

### Why QML clip animation for the grow phase (not a Wayland surface resize)

The `clipContainer` uses `clip: true` and its `height` property is animated from 0 to full height. From the compositor's perspective the surface geometry never changes while it is mapped — only the rendered pixels inside the surface change.

### Why `active` property on QML root

`active` drives both the animation state machine (via `onActiveChanged`) and the `Loader.active` binding in `SidebarContent` (which destroys/creates tab page instances, pausing all their timers and signal connections). Keeping the "is the sidebar open" logic in C++ and the "what to do when it opens/closes" logic in QML keeps C++ thin and QML testable in isolation.

### Why dismissal is handled inside the sidebar host

The sidebar host surface is fullscreen while open, matching `LauncherSurface`. This avoids the fragile two-surface approach where a fullscreen dismiss surface needs a manually maintained input-region hole over the panel. The root background handles outside clicks; the panel absorbs inside clicks. When the sidebar is closed, the host surface is destroyed so it cannot retain compositor focus or intercept desktop input.

---

## 6. Alternatives Considered

### Create-on-demand vs persistent surface

**Create-on-demand (chosen):** Destroys the `QQuickView` on close and recreates it on each open. Surfaces only exist when needed, and the compositor reliably releases focus/input ownership after close. Active tab and panel height state are cached in C++.

**Persistent surface (rejected):** Pre-allocating at startup and hiding via QML root visibility made the implementation fast, but Hyprland could keep keyboard/pointer ownership on the mapped layer surface after close. Manual empty input regions and keyboard interactivity toggles did not reliably restore focus, so persistence is not robust enough for this interactive fullscreen host.

### D-Bus vs singleton signal for clock trigger

**D-Bus:** `SidebarManager` registers an interface on the session bus; `ClockSection.qml` calls a D-Bus method. Usable from any process (e.g. a keybinding daemon).

**Rejected because:** The clock and the sidebar are both in-process. Adding a D-Bus interface introduces serialization overhead, an extra async round-trip, and requiring `Q_CLASSINFO("D-Bus Interface", ...)` boilerplate. For the in-process case, a direct `Q_INVOKABLE` call on a QML-registered singleton is the established pattern in this codebase.

**Registered QML singleton Q_INVOKABLE (chosen):** `ShellApplication` owns one `SidebarManager` instance and registers it with `qmlRegisterSingletonType` as `HolonightShell.SidebarManager`. `ClockSection.qml` calls `SidebarManager.toggle(root.barMonitorName)` directly. This matches the existing manually registered service/surface singleton pattern in the project.

---

## 7. Known Risks

### Multi-phase SequentialAnimation correctness

`SequentialAnimation` runs animations in sequence but the `from` value of a subsequent animation may need to be the `to` value of the prior one. Qt resolves `from` at the time the `NumberAnimation` starts within the sequence, not at sequence start time — so chaining works correctly as long as the `from` fields are omitted on interior animations (letting Qt read the current property value). The open animation's phase 2 must have no explicit `from` field; the close animation's phase 2 and 3 must similarly omit `from`. Test for edge case: interrupting an opening animation mid-phase (user clicks again before `openAnimation` finishes) — `closeAnimation.start()` will animate from the current mid-point value rather than from the fully-open state. This is visually correct.

### `setInitialProperties` firing `onActiveChanged` during construction

`SidebarManager` calls `view->setInitialProperties({..., {"active", false}})` before `setSource()`. Qt delivers `onActiveChanged` to the root object during component construction, before `Component.onCompleted` runs. Without the `ready` flag pattern, `closeAnimation.start()` would fire on a sidebar that has never been open, animating `clipContainer.height` from an indeterminate value to 0. The `ready` flag (set in `Component.onCompleted`) gates all `onActiveChanged` reactions.

### Host keyboard interactivity

The fullscreen host requests `keyboard_interactivity_exclusive`. While it is mapped, the compositor routes keyboard input to the sidebar so Escape can close it. The host is destroyed after the close animation, which releases keyboard focus back to the compositor. `SidebarManager.close()` is idempotent, so duplicate Escape/outside-click dismiss requests are harmless.

### Tab switch animation `from` value

`tabSwitchAnimation` must read `contentArea.width` at switch time (not a hardcoded `from`). Omitting `from` is correct here too. `SidebarContent.preferredWidthForTab()` currently resolves widths from a table that mirrors the placeholder tab page `preferredWidth` declarations. If tab widths become dynamic later, this should be replaced with a resolver that reads the loaded tab component or shared metadata from one source.

---

## 8. File Checklist

### C++ files to create

- `src/surfaces/SidebarManager.h`
- `src/surfaces/SidebarManager.cpp`

### C++ files to modify

- `src/surfaces/ShellConstants.h` — add `kSidebarTabBarWidth`, `kSidebarRightMargin`, `kSidebarBottomMargin`, `kSidebarTopMargin`, `kSidebarMaxContentWidth`
- `src/app/ShellApplication.h` — add `SidebarManager` forward declaration and `std::unique_ptr<SidebarManager> sidebar_manager_` member
- `src/app/ShellApplication.cpp` — instantiate and start `sidebar_manager_` in `startLayerSurfaces()`

### C++ files intentionally retained

- `src/surfaces/PopupSurface.h`
- `src/surfaces/PopupSurface.cpp`
- `ShellApplication::popup_surface_`
- `StatusPopupSurface::setSessionPopup(popup_surface_)`

These remain because `PopupSurface` is the session popup surface, not only the old clock popup path.

### QML files to create

- `src/qml/RightSidebar/RightSidebar.qml`
- `src/qml/RightSidebar/SidebarTabBar.qml`
- `src/qml/RightSidebar/SidebarTabButton.qml`
- `src/qml/RightSidebar/SidebarContent.qml`
- `src/qml/RightSidebar/SidebarOverview.qml`
- `src/qml/RightSidebar/SidebarCalendar.qml`
- `src/qml/RightSidebar/SidebarNotifications.qml`
- `src/qml/RightSidebar/SidebarSystem.qml`
- `src/qml/RightSidebar/SidebarQuickSettings.qml`
- `src/qml/RightSidebar/SidebarMedia.qml`

### QML files to modify

- `src/qml/Topbar/ClockSection.qml` — replace the clock click's `PopupSurface` show/hide behavior with `SidebarManager.toggle(root.barMonitorName)`

### CMakeLists.txt

Add all 11 new QML files to `HOLONIGHT_QML_FILES`. Add `SidebarManager.cpp` to the source list.
