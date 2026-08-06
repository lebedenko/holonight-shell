# SPEC.md — topbar-tray: System Tray Widget

## Overview

The topbar tray widget displays active StatusNotifierItem (SNI) icons from D-Bus services in a horizontal section of the holonight-shell topbar. It dynamically registers as a StatusNotifierHost, discovers and tracks registered tray items, renders their icons with theme fallback and ARGB32 pixmap support, responds to activation clicks, and displays `NeedsAttention` items with a cyan glow pulse. The widget must degrade gracefully if another host already claims the watcher role, falling back to passive item discovery via the existing watcher.

---

## Requirements

### REQ-F-001: Watcher Registration

**Type:** Functional

**EARS:** The system shall attempt to register as `org.kde.StatusNotifierWatcher` on the session D-Bus.

**Acceptance criteria:**
- D-Bus registration attempt succeeds or logs a specific error (not silent failure)
- If registration succeeds, the process owns the watcher service name
- If registration fails (another host is active), the failure is caught and logged at info level

---

### REQ-F-002: Watcher Conflict Fallback

**Type:** Functional

**EARS:** If the system cannot register as `org.kde.StatusNotifierWatcher`, then the system shall read the list of registered items from the existing watcher's `RegisteredStatusNotifierItems` property.

**Acceptance criteria:**
- D-Bus call to fetch `RegisteredStatusNotifierItems` from the active watcher succeeds
- The property is parsed as a `QStringList` of service/path pairs (format: `service:path`)
- All items in the list are added to the local tray model
- Reading from the existing watcher does not prevent future `ItemRegistered` signal connection

---

### REQ-F-003: Host Registration

**Type:** Functional

**EARS:** The system shall register as `org.kde.StatusNotifierHost` on the session D-Bus.

**Acceptance criteria:**
- D-Bus registration of `org.kde.StatusNotifierHost` succeeds at startup
- The service is registered before any SNI app attempts to register itself (no race condition in practice)
- If registration fails, startup continues and a warning is logged (host registration is optional for visibility)

---

### REQ-F-004: Item Discovery via Watcher

**Type:** Functional

**EARS:** When the system successfully registers as the watcher, the system shall listen for `ItemRegistered` signals on `org.kde.StatusNotifierWatcher` and add new items to the tray.

**Acceptance criteria:**
- `ItemRegistered(serviceName: string, objectPath: ObjectPath)` signal connection is established
- Each new signal triggers a fetch of the item's properties (Status, IconName, IconPixmap, etc.)
- The item is added to the tray model only after successful property fetch
- Duplicate registrations (same service + path) do not create duplicate tray items

---

### REQ-F-005: Item Removal via Watcher

**Type:** Functional

**EARS:** When the system successfully registers as the watcher, the system shall listen for `ItemUnregistered` signals on `org.kde.StatusNotifierWatcher` and remove unregistered items from the tray.

**Acceptance criteria:**
- `ItemUnregistered(serviceName: string, objectPath: ObjectPath)` signal connection is established
- Each unregistered item is removed from the tray model and its QML component destroyed
- Unregistered items that are not in the tray model are silently ignored

---

### REQ-F-006: Service Disappearance Detection

**Type:** Functional

**EARS:** The system shall monitor the D-Bus name ownership of each tray item's service and remove the item if its service disappears.

**Acceptance criteria:**
- `QDBusConnection::connect()` on each item's service name with `QDBusConnection::NameOwnerChanged` is established
- When a service name is unregistered (loses its name owner), the corresponding tray item is removed
- Items are cleaned up even if `ItemUnregistered` signal is not emitted by the watcher

---

### REQ-F-007: Icon Rendering – Theme First

**Type:** Functional

**EARS:** The system shall attempt to load tray item icons using `QIcon::fromTheme(IconName)` where IconName is provided by the item's SNI properties.

**Acceptance criteria:**
- `IconName` property is queried from each tray item's D-Bus object
- If `IconName` is non-empty, `QIcon::fromTheme(IconName)` is called
- The theme icon is used if it exists in the system icon theme
- No fallback occurs if `IconName` is provided, even if the theme icon is not found (follows QIcon contract)

---

### REQ-F-008: Icon Rendering – Pixmap Fallback

**Type:** Functional

**EARS:** If no `IconName` is provided or theme lookup fails, the system shall decode and render the item's `IconPixmap` as a 22×22px icon.

**Acceptance criteria:**
- `IconPixmap` property (type: `a(iiay)`) is queried from the item's D-Bus object
- The array is parsed as a list of `(width, height, ARGB32_data_bytes)` tuples
- The largest pixmap (by area) is selected as the fallback
- ARGB32 data is byte-swapped from big-endian (D-Bus wire format) to native-endian before rendering
- The pixmap is scaled to 22×22px if its native size differs
- The pixmap is converted to a `QPixmap` and assigned to the tray item's icon

---

### REQ-F-009: Passive Items Hidden

**Type:** Functional

**EARS:** While an item's Status property is `Passive`, the system shall hide the item from the tray display.

**Acceptance criteria:**
- Items with `Status == "Passive"` (string value from SNI spec) have `visible: false` in QML
- Passive items are tracked internally but do not render in the UI
- If an item transitions from `Active` or `NeedsAttention` to `Passive`, it is immediately hidden
- If an item transitions from `Passive` to `Active` or `NeedsAttention`, it is immediately shown with opacity reset

---

### REQ-F-010: Active Items Display

**Type:** Functional

**EARS:** While an item's Status property is `Active`, the system shall display the item's icon in the tray with normal opacity.

**Acceptance criteria:**
- Items with `Status == "Active"` (string value from SNI spec) have `visible: true` in QML
- Icon opacity is 1.0 (no dimming or glow)
- The icon is rendered at 22×22px

---

### REQ-F-011: Needs Attention State – Glow Pulse

**Type:** Functional

**EARS:** While an item's Status property is `NeedsAttention`, the system shall display the item with a cyan glow pulse animation.

**Acceptance criteria:**
- Items with `Status == "NeedsAttention"` (string value from SNI spec) have `visible: true` in QML
- Glow color is cyan (`#00FFFF` or equivalent from HoloniightPalette)
- Glow opacity animates between 0 and 1 with a continuous loop
- Animation duration is 1.5 seconds per full cycle (0 → 1 → 0)
- Icon itself remains at normal opacity; only the glow fades in and out

---

### REQ-F-012: Attention Icon Preference

**Type:** Functional

**EARS:** When an item's Status is `NeedsAttention`, the system shall prefer the item's `AttentionIconName` or `AttentionIconPixmap` over its regular icon, if either is provided.

**Acceptance criteria:**
- `AttentionIconName` property is queried when Status transitions to `NeedsAttention`
- If `AttentionIconName` is non-empty, it is used instead of the regular icon
- If `AttentionIconName` is empty, `AttentionIconPixmap` is queried
- If `AttentionIconPixmap` is non-empty, it is decoded and scaled to 22×22px like regular pixmap fallback
- If both are empty, the regular icon is used with the glow pulse applied
- When Status transitions away from `NeedsAttention`, the regular icon is restored

---

### REQ-F-013: Left-Click Activation

**Type:** Functional

**EARS:** When a user clicks on a tray item, the system shall invoke the item's `Activate(x, y)` D-Bus method with coordinates `(0, 0)`.

**Acceptance criteria:**
- Mouse click on a tray item icon is detected by the QML `MouseArea`
- `Activate` method is called with `x=0` and `y=0` (no tooltip positioning in MVP)
- Method call is asynchronous (non-blocking) with error handling
- If the method call fails, the error is logged but does not crash the widget

---

### REQ-F-014: BarSection Integration

**Type:** Functional

**EARS:** The system shall provide a `TraySection` QML component that extends `BarSection` and is placed between `BatterySection` and `StatusSection` in `TopBar.qml`.

**Acceptance criteria:**
- `TraySection.qml` is a valid QML component that compiles without warnings
- The component inherits or wraps `BarSection` from the BarSection library
- In `TopBar.qml`, the component instantiation order is: WorkspaceSection, DateTimeSection, BatterySection, **TraySection**, StatusSection
- The section has implicit width calculated from the tray items and spacing

---

### REQ-F-015: Horizontal Row Layout

**Type:** Functional

**EARS:** The system shall arrange tray item icons in a horizontal row with uniform 6-pixel spacing between adjacent items.

**Acceptance criteria:**
- Tray items are children of a `Row` QML element with `spacing: 6`
- Items are rendered left-to-right in order of registration
- All items have the same height (22px) and are vertically centered
- Spacing is consistent between all adjacent pairs

---

### REQ-F-016: Opacity Fade Animation on Appear

**Type:** Functional

**EARS:** When a tray item becomes visible (transitions to `Active` or `NeedsAttention` from `Passive` or not-yet-discovered), the system shall animate its opacity from 0.0 to 1.0 over 100 milliseconds.

**Acceptance criteria:**
- Icon opacity starts at 0.0 and linearly animates to 1.0
- Animation duration is 100 milliseconds
- Animation is applied on first appearance and on every visibility transition
- The glow pulse (if applicable) is not affected by this fade; only the icon itself fades

---

### REQ-F-017: Opacity Fade Animation on Disappear

**Type:** Functional

**EARS:** When a tray item becomes hidden (transitions to `Passive` from `Active` or `NeedsAttention`), the system shall animate its opacity from 1.0 to 0.0 over 100 milliseconds before setting `visible: false`.

**Acceptance criteria:**
- Icon opacity animates from 1.0 to 0.0 over 100 milliseconds
- After the animation completes, `visible: false` is set (so the item is removed from layout)
- If the item is unregistered or removed while the fade-out is in progress, the animation is aborted and the item is immediately destroyed

---

### REQ-F-018: No Overflow Cap for MVP

**Type:** Constraint

**EARS:** The system shall not limit the number of visible tray items in the topbar for the initial release.

**Acceptance criteria:**
- No maximum item count is enforced
- The tray section's implicit width grows as new items are added
- If the tray exceeds the available bar width, it is clipped by the compositor (not truncated by Qt)
- Future overflow handling (scrolling, popup) is deferred to post-MVP releases

---

### REQ-F-019: Property Change Monitoring

**Type:** Functional

**EARS:** The system shall listen for `PropertiesChanged` signals on each tray item and update the display when Status, IconName, IconPixmap, AttentionIconName, or AttentionIconPixmap properties change.

**Acceptance criteria:**
- `org.freedesktop.DBus.Properties.PropertiesChanged` signal is connected for each item
- When Status changes, visibility and glow state are updated (no icon reload unless Status is `NeedsAttention`)
- When IconName changes, the icon is reloaded from the theme
- When IconPixmap changes, the pixmap is re-decoded and re-rendered
- When AttentionIcon* properties change, the attention icon is updated if Status is `NeedsAttention`
- All property changes are applied without flickering or visual discontinuity

---

### REQ-NF-001: D-Bus Message Timeout

**Type:** Non-Functional

**EARS:** The system shall set a 5-second timeout on all D-Bus method calls (property fetch, Activate invocation).

**Acceptance criteria:**
- `QDBusInterface` or `QDBusMessage` is configured with `setTimeout(5000)` (milliseconds)
- If a method call exceeds 5 seconds, it returns a timeout error and is logged
- Timeout errors do not block or crash the widget; display remains responsive

---

### REQ-NF-002: Icon Rendering Performance

**Type:** Non-Functional

**EARS:** The system shall decode and render each tray item's pixmap once on discovery and cache the result until the pixmap property changes.

**Acceptance criteria:**
- Pixmap decoding is not repeated on every display update
- Icon is stored in the tray item model and reused by QML bindings
- Changing a property other than IconPixmap or IconName does not trigger a pixmap reload

---

### REQ-NF-003: Memory Cleanup on Item Removal

**Type:** Non-Functional

**EARS:** When a tray item is removed, the system shall deallocate all associated resources (D-Bus signal connections, pixmap, item object).

**Acceptance criteria:**
- QML Component is destroyed via `destroy()`
- D-Bus signal connections (PropertiesChanged, NameOwnerChanged) are disconnected
- Icon pixmap is released by the QPixmap destructor
- No lingering D-Bus connections remain after item removal

---

### REQ-C-001: ARGB32 Byte-Swap on x86

**Type:** Constraint

**EARS:** Where the system is compiled for x86/x86-64 little-endian, the system shall byte-swap ARGB32 IconPixmap data from big-endian (D-Bus wire format) to little-endian before rendering.

**Acceptance criteria:**
- ARGB32 data is read as big-endian from the D-Bus message (standard for all platforms)
- If the system's native byte order is little-endian, each 32-bit ARGB value is swapped (0xAARRGGBB → 0xBBGGRRAA)
- Rendered icon colors match the expected appearance in reference SNI apps (e.g., KDE Plasma, GNOME)
- Icons with alpha channel (transparency) render correctly

---

### REQ-C-002: HoloniightPalette Theming

**Type:** Constraint

**EARS:** The system shall use `HoloniightPalette.cyan` or equivalent from the HoloNight design system for the `NeedsAttention` glow color.

**Acceptance criteria:**
- Glow color is imported from `import Holonight` and accessed via `HoloniightPalette.<token>` (note double-i spelling)
- No hardcoded hex color values (`#00FFFF`) appear in QML source files
- Glow color is consistent with the HoloNight design system's attention state color

---

### REQ-C-003: QML Glow Implementation

**Type:** Constraint

**EARS:** The system shall use `QtQuick.Effects.MultiEffect` with `shadowEnabled: true` to render the glow effect on tray items, not `Qt5Compat.GraphicalEffects.Glow`.

**Acceptance criteria:**
- `import QtQuick.Effects` is used (not `Qt5Compat.GraphicalEffects`)
- Glow is achieved via `MultiEffect` with shadow radius tuned for a soft cyan halo
- The older `Glow` component (which ignores the `color` property on this Qt build) is not used

---

### REQ-C-004: QRC Path Conventions

**Type:** Constraint

**EARS:** All tray-related QML files and assets shall use QRC paths with the `/HolonightShell/` prefix and directory-based layout under `src/qml/Tray/`.

**Acceptance criteria:**
- QML file path: `src/qml/Tray/TraySection.qml` → QRC alias: `qrc:/HolonightShell/Tray/TraySection.qml`
- Each QML file has a `QT_RESOURCE_ALIAS` CMake property stripping the `src/qml/` prefix
- No flat `.qml` files; all components live in a dedicated directory
- Asset references use `qrc:/HoloniightShell/...` paths

---

### REQ-C-005: TopBar.qml Insertion Point

**Type:** Constraint

**EARS:** The `TraySection` component shall be instantiated in `TopBar.qml` between `BatterySection` and `StatusSection`.

**Acceptance criteria:**
- In `TopBar.qml`, the element order is: `WorkspaceSection { ... }`, `DateTimeSection { ... }`, `BatterySection { ... }`, `TraySection { ... }`, `StatusSection { ... }`
- No other sections are inserted between Battery and Status
- TraySection is a direct child of the topbar's root Row or equivalent container

---

### REQ-C-006: Per-Item Logging

**Type:** Constraint

**EARS:** The system shall use `qCInfo` for any diagnostic output related to tray item registration, removal, or icon loading.

**Acceptance criteria:**
- No `qCDebug` is used for tray operations (debug logging is off by default and would be invisible in production)
- New items, unregistered items, and icon loading failures are logged at info level with the service and object path
- Log messages include enough context for manual debugging (e.g., "Registered tray item: org.example.App /StatusNotifierItem")

---

### REQ-C-007: StatusNotifierHost Service Metadata

**Type:** Constraint

**EARS:** The system shall register `org.kde.StatusNotifierHost` with a unique identity (e.g., `org.kde.StatusNotifierHost-holonight-<pid>`) to prevent conflicts with other hosts on the same session bus.

**Acceptance criteria:**
- Service name includes the process ID or a UUID to ensure uniqueness
- The watcher does not reject the registration due to a duplicate name
- If multiple holonight-shell instances run on the same session bus, each registers with a distinct identity

---

## Traceability

| Requirement | Category | Summary |
|---|---|---|
| REQ-F-001 | Functional | Watcher registration attempt |
| REQ-F-002 | Functional | Fallback to existing watcher |
| REQ-F-003 | Functional | Host registration |
| REQ-F-004 | Functional | Item discovery via ItemRegistered |
| REQ-F-005 | Functional | Item removal via ItemUnregistered |
| REQ-F-006 | Functional | Service disappearance detection |
| REQ-F-007 | Functional | Theme icon lookup |
| REQ-F-008 | Functional | ARGB32 pixmap fallback |
| REQ-F-009 | Functional | Passive items hidden |
| REQ-F-010 | Functional | Active items display |
| REQ-F-011 | Functional | NeedsAttention glow pulse |
| REQ-F-012 | Functional | Attention icon preference |
| REQ-F-013 | Functional | Left-click activation |
| REQ-F-014 | Functional | BarSection integration |
| REQ-F-015 | Functional | Horizontal row layout |
| REQ-F-016 | Functional | Appear fade animation |
| REQ-F-017 | Functional | Disappear fade animation |
| REQ-F-018 | Constraint | No overflow cap for MVP |
| REQ-F-019 | Functional | Property change monitoring |
| REQ-NF-001 | Non-Functional | D-Bus timeout |
| REQ-NF-002 | Non-Functional | Icon caching |
| REQ-NF-003 | Non-Functional | Memory cleanup |
| REQ-C-001 | Constraint | ARGB32 byte-swap |
| REQ-C-002 | Constraint | HoloniightPalette theming |
| REQ-C-003 | Constraint | MultiEffect glow |
| REQ-C-004 | Constraint | QRC paths |
| REQ-C-005 | Constraint | TopBar insertion point |
| REQ-C-006 | Constraint | Per-item logging |
| REQ-C-007 | Constraint | Host service identity |
