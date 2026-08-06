# System Tray Full Implementation — SPEC.md

**Date**: 2026-05-25  
**Project**: holonight-shell (C++23/Qt6 Wayland shell)  
**Scope**: Complete SNI (Status Notifier Item) and DBusMenu context menu implementation  
**Version**: 1.1 — finalized  

---

## Overview

This specification defines the complete system tray feature for holonight-shell, including:
- Overflow handling with a max of 3 visible non-passive slots
- Urgent item priority with slot preemption and queuing
- SNI ToolTip fetching and fallback chain
- Full `com.canonical.dbusmenu` context menu parsing and rendering
- Mouse actions (left/middle/right click, scroll) and SNI method calls
- Direct SNI signal subscriptions for property updates
- Urgent badge visual (violet dot with glow)

All color values are sourced from `HoloniightPalette` (system HoloNight theme). Popup menus render in a dedicated `TrayMenuSurface` layer-shell window.

---

## 1. Overflow & Visibility

### REQ-F-001 (Ubiquitous)
**The system** shall display a maximum of 3 consecutive visible non-passive tray item slots in the topbar.

**Acceptance Criterion**: Verify that when 5 non-passive SNI items are registered and none are urgent, exactly 3 items render as visible slots and a `+2` overflow button appears as a 4th slot.

### REQ-F-002 (Ubiquitous)
**The system** shall immediately hide all tray items with `status == "Passive"` and exclude them from the 3-slot cap.

**Acceptance Criterion**: Register 2 passive items and 4 non-passive items; verify that only 3 non-passive items and 1 overflow button render, and the passive items are not visible in the bar.

### REQ-F-003 (Event-driven)
**When** a non-passive SNI item transitions from hidden (overflow) to visible, **the system** shall animate it sliding in from the right edge of the 3-slot region with a duration of 100 ms.

**Acceptance Criterion**: Move an item from overflow into slot 3 by closing a higher-priority item; capture frame timings and confirm the slide duration is 95–105 ms.

### REQ-F-004 (Ubiquitous)
**The system** shall render the overflow button with the label `+N` where `N` is the count of non-passive items exceeding the 3-slot cap, including items demoted by urgent priority.

**Acceptance Criterion**: With 7 non-passive items total, 3 visible, and 0 urgent, the overflow button displays `+4`. When urgency shifts items, the count updates correctly.

### REQ-F-005 (State-driven)
**If** the count of non-passive items is ≤ 3, **then** no overflow button shall appear.

**Acceptance Criterion**: Register 3 non-passive items, verify no overflow button; register a 4th, verify overflow button appears.

---

## 2. Urgent Slot Priority

### REQ-F-006 (Event-driven)
**When** a non-passive SNI item transitions to `status == "NeedsAttention"`, **the system** shall immediately move it to the leftmost visible slot (slot 1), displacing rightmost non-urgent items into overflow until all urgent items occupy the leftmost slots (up to 3).

**Acceptance Criterion**: Register 3 non-passive non-urgent items in slots 1, 2, 3. Add a 4th non-passive item with urgent status; verify it moves to slot 1, the original slot 3 item moves to overflow, and the count reflects `+1`.

### REQ-F-007 (State-driven)
**If** more than 3 non-passive items have `status == "NeedsAttention"`, **then** the first 3 shall occupy the visible slots and the remainder shall queue in overflow, maintaining urgent priority.

**Acceptance Criterion**: Register 5 urgent items; verify the first 3 are visible in slots 1–3 and the remaining 2 appear in overflow behind all non-urgent items. Update the model's sort to confirm urgent items have priority in the queue.

### REQ-F-008 (Event-driven)
**When** a non-passive SNI item transitions from `status == "NeedsAttention"` to any other status, **the system** shall vacate its slot and, if items are queued in overflow, promote the highest-priority queued item into the vacated slot with a 100 ms slide-in animation from the left.

**Acceptance Criterion**: With 4 non-passive items (first urgent and visible, next 3 non-urgent and 2 visible + 1 in overflow), clear the urgency flag on item 1; verify it moves to overflow, item 4 slides into slot 1, and item 3 slides into slot 2, completing within 100 ms.

### REQ-F-009 (Conditional)
**If** an urgent item is promoted from overflow into a visible slot, **and** another urgent item is already queued in overflow, **then** the next urgent item shall slide into the vacated visible slot in queued order on the subsequent urgency-clear event.

**Acceptance Criterion**: Queue 5 items with statuses [urgent-active, urgent-queued, non-urgent, urgent-queued, non-urgent]. Clear first item's urgency; verify second urgent item slides to slot 1, third urgent item moves toward slot 2 on next clear, and order is maintained.

---

## 3. Urgent Badge

### REQ-F-010 (State-driven)
**If** a non-passive SNI item has `status == "NeedsAttention"`, **then** a violet circular dot (diameter ~6 px, color `HoloniightPalette.accentViolet`) shall appear in the bottom-right corner of the item's icon.

**Acceptance Criterion**: Register a tray item with urgent status; measure the badge dot dimensions to confirm 6 ± 1 px diameter and color matches `HoloniightPalette.accentViolet` via pixel inspection.

### REQ-F-011 (State-driven)
**If** a non-passive SNI item displays an urgent badge, **then** the badge shall have a slow pulsing `MultiEffect` glow (matching the glow pattern defined in the HoloNight design reference) that is disabled when the item's status is not `"NeedsAttention"`.

**Acceptance Criterion**: Register an urgent item and visually confirm a steady pulsing glow on the violet dot. Clear urgency and confirm the glow ceases.

### REQ-F-012 (Unwanted behaviour)
**The system** shall NOT render a red border around urgent tray items; the red `borderUrgent` property is removed entirely from the item frame.

**Acceptance Criterion**: Inspect the QML/C++ rendering code; confirm no `borderUrgent` property exists and no red border is applied to tray items.

---

## 4. Tooltip

### REQ-F-013 (Ubiquitous)
**The system** shall fetch the SNI `ToolTip` property on item registration and on any `NewToolTip` signal, parsing the struct fields: `icon_name` (string), `icon_pixmap` (array of byte arrays), `title` (string), `description` (string).

**Acceptance Criterion**: Register a tray item with a non-empty ToolTip struct; introspect the D-Bus call and confirm all four fields are extracted and stored in the model.

### REQ-F-014 (Ubiquitous)
**The system** shall expose tooltip content via three new tray item model roles: `tooltipTitle`, `tooltipDescription`, `tooltipIconName`.

**Acceptance Criterion**: Query the tray item model for roles on an item with a ToolTip; confirm all three roles return the fetched values.

### REQ-F-015 (Conditional)
**If** a tray item's ToolTip `title` field is non-empty, **then** the QML tooltip shall display it; otherwise, **it** shall fall back to the item's `title` property.

**Acceptance Criterion**: Register two items—one with a ToolTip `title` and one without; hover over each and confirm the tooltip shows the ToolTip `title` for the first and the item `title` for the second.

### REQ-F-016 (Conditional)
**If** a tray item's ToolTip `description` field is non-empty, **then** the QML tooltip shall display it; otherwise, **it** shall fall back to the item's `status` string (e.g., "Active", "Attention", "Passive").

**Acceptance Criterion**: Register an item with a ToolTip `description` and hover to verify it displays; register an item without one and confirm the status string appears instead.

### REQ-F-017 (Ubiquitous)
**The system** shall use the existing `TooltipSurface` implementation with a 450 ms hover delay to display tooltip content above the tray item.

**Acceptance Criterion**: Hover over a tray item; measure the delay to tooltip appearance and confirm it is 450 ± 50 ms.

---

## 5. Context Menu (DBusMenu)

### REQ-F-018 (Event-driven)
**When** a non-passive SNI item receives a right-click event, **the system** shall fetch the item's `ContextMenu` property (a D-Bus object path) and call `GetLayout` with ID=0 to retrieve the full menu tree structure.

**Acceptance Criterion**: Right-click a tray item with a context menu; verify a `GetLayout` call is made on the object path and the menu tree is returned without error.

### REQ-F-019 (Ubiquitous)
**The system** shall recursively parse the DBusMenu `GetLayout` response, extracting menu items with the following properties: label (string), type (e.g., "org.gtk.Action.MenuItem" for normal items, "org.gtk.Separator.MenuItem" for separators), icon name (string), enabled/visible flags, submenu (recursively), checkbox/radio state.

**Acceptance Criterion**: Parse a multi-level menu structure from a real tray application; verify all item types, labels, and submenu references are correctly extracted and indexed.

### REQ-F-020 (Conditional)
**If** a DBusMenu item has `type == "org.gtk.Separator.MenuItem"`, **then** it shall render as a thin horizontal divider line; no label or icon shall appear.

**Acceptance Criterion**: Render a menu with separators; confirm dividers appear at the correct positions with no text.

### REQ-F-021 (Conditional)
**If** a DBusMenu item has a `submenu` reference, **then** a small right-arrow icon shall appear to the right of the label, and hovering over the item shall expand the submenu inline to the right.

**Acceptance Criterion**: Render a menu with submenu items; hover and confirm the submenu expands to the right with items visible.

### REQ-F-022 (Event-driven)
**When** a DBusMenu popup is about to display, **the system** shall call `AboutToShow(item_id)` on the menu for each root-level menu item; only after all calls complete (or timeout after 1 second) shall the menu be rendered.

**Acceptance Criterion**: Open a context menu and verify `AboutToShow` is called for all root items; monitor timing and confirm the menu displays after all calls or after a 1-second timeout.

### REQ-F-023 (Event-driven)
**When** a user clicks a DBusMenu item, **the system** shall call `Event(item_id, "clicked", variant, timestamp)` on the menu object and await the call to complete before closing the popup.

**Acceptance Criterion**: Click a menu item; verify `Event` is called with the correct item ID and "clicked" action, and the popup closes after the call completes.

### REQ-F-024 (Conditional)
**If** a DBusMenu item is disabled (`enabled == false`), **then** it shall appear grayed out and shall not respond to clicks.

**Acceptance Criterion**: Render a menu with disabled items; confirm they appear with reduced opacity and do not trigger `Event` on click.

### REQ-F-025 (Conditional)
**If** a DBusMenu item is hidden (`visible == false`), **then** it shall not appear in the rendered menu.

**Acceptance Criterion**: Render a menu with hidden items; verify they do not occupy space or appear in the popup.

### REQ-F-026 (Ubiquitous)
**The system** shall render the DBusMenu popup in a dedicated `TrayMenuSurface` layer-shell window, positioned directly below the parent tray item and aligned to the item's left edge while clamping to screen bounds.

**Acceptance Criterion**: Open a context menu and verify it appears in a separate layer-shell surface below the tray item, remains on-screen near the display edge, and follows the HoloNight shell menu styling.

### REQ-F-027 (Event-driven)
**When** a user clicks outside the DBusMenu popup or presses Escape, **the system** shall close the popup and return focus to the shell.

**Acceptance Criterion**: Open a menu, click the bar background, and verify the menu closes; repeat with Escape key.

### REQ-F-028 (Ubiquitous)
**The system** shall refetch the DBusMenu tree on each right-click event to ensure the menu reflects the latest application state.

**Acceptance Criterion**: Open a context menu, close it, modify application state (e.g., add/remove menu items), right-click again, and verify the menu reflects the changes.

### REQ-F-029 (Ubiquitous)
**The system** shall support a DBusMenu recursion depth limit of 5 levels; submenus beyond depth 5 shall be ignored.

**Acceptance Criterion**: Construct a menu with 6 levels of nesting; verify items at depth ≤ 5 render and those at depth > 5 are ignored.

### REQ-F-030 (Ubiquitous)
**The system** shall render DBusMenu item icons at 16×16 px (matching system icon size convention) using the system icon theme.

**Acceptance Criterion**: Render a menu with icon names; verify icons appear at 16×16 px and match the active system icon theme.

---

## 6. Mouse Actions & SNI Calls

### REQ-F-031 (Event-driven)
**When** a user left-clicks a non-passive tray item, **the system** shall call `Activate(x, y)` on the SNI object, passing the item's absolute screen coordinates.

**Acceptance Criterion**: Left-click a tray item and verify `Activate` is called with correct screen coordinates (obtained via QML `mapToGlobal`).

### REQ-F-032 (Event-driven)
**When** a user right-clicks a non-passive tray item, **the system** shall open the DBusMenu context menu (see REQ-F-018).

**Acceptance Criterion**: Right-click a tray item; confirm the menu appears and `GetLayout` is called.

### REQ-F-033 (Event-driven)
**When** a user middle-clicks a non-passive tray item, **the system** shall call `SecondaryActivate(x, y)` on the SNI object, passing the item's absolute screen coordinates.

**Acceptance Criterion**: Middle-click a tray item and verify `SecondaryActivate` is called with correct screen coordinates.

### REQ-F-034 (Event-driven)
**When** a user scrolls vertically (up/down) on a non-passive tray item, **the system** shall call `Scroll(delta, "vertical")` where `delta` is the scroll wheel delta (positive for up, negative for down).

**Acceptance Criterion**: Scroll up/down on a tray item; verify `Scroll` is called with the correct delta and "vertical" direction.

### REQ-F-035 (Event-driven)
**When** a user scrolls horizontally (left/right) on a non-passive tray item, **the system** shall call `Scroll(delta, "horizontal")` where `delta` reflects the scroll direction.

**Acceptance Criterion**: Scroll left/right on a tray item; verify `Scroll` is called with the correct delta and "horizontal" direction.

### REQ-F-036 (Unwanted behaviour)
**The system** shall NOT call `Activate` or any SNI method on passive (`status == "Passive"`) tray items; clicking passive items shall have no effect.

**Acceptance Criterion**: Register a passive tray item and attempt all mouse actions; verify no SNI methods are called.

---

## 7. SNI Signal Subscriptions

### REQ-F-037 (Event-driven)
**When** an SNI item emits `NewIcon`, **the system** shall fetch the updated `IconName` and `IconPixmap` properties and update the item's icon roles/cache.

**Acceptance Criterion**: Register a tray item, emit `NewIcon` via D-Bus introspection, and verify the icon name or pixmap-backed icon role is updated within 100 ms.

### REQ-F-038 (Event-driven)
**When** an SNI item emits `NewAttentionIcon`, **the system** shall fetch the updated `AttentionIconName` and `AttentionIconPixmap` properties.

**Acceptance Criterion**: Verify both attention icon properties are fetched on the signal, regardless of whether the icon is used.

### REQ-F-039 (Event-driven)
**When** an SNI item emits `NewStatus(status)`, **the system** shall update the item's `status` role and re-evaluate urgent priority rules (REQ-F-006 through REQ-F-008).

**Acceptance Criterion**: Register a tray item, emit `NewStatus("NeedsAttention")`, and verify the status role updates and urgent priority is applied (item moves to slot 1 if visible).

### REQ-F-040 (Event-driven)
**When** an SNI item emits `NewTitle`, **the system** shall call `Get("Title")` to fetch the updated title and update the item's `title` role.

**Acceptance Criterion**: Register a tray item, emit `NewTitle` via D-Bus, and verify the title role updates.

### REQ-F-041 (Event-driven)
**When** an SNI item emits `NewToolTip`, **the system** shall fetch the updated `ToolTip` property and update roles `tooltipTitle`, `tooltipDescription`, and `tooltipIconName` (REQ-F-013).

**Acceptance Criterion**: Register a tray item, emit `NewToolTip` with a new struct, and verify all three tooltip roles are updated.

### REQ-F-042 (Ubiquitous)
**In addition to** `PropertiesChanged` signals, **the system** shall subscribe to all item-level signals (`NewIcon`, `NewAttentionIcon`, `NewStatus`, `NewTitle`, `NewToolTip`) upon item registration and maintain subscriptions for the item's lifetime.

**Acceptance Criterion**: Introspect D-Bus signal subscriptions for a registered item; verify all five signals are subscribed.

---

## 8. Non-Functional & Constraints

### REQ-NF-001 (Performance)
**The system** shall fetch SNI properties and DBusMenu structures asynchronously (off the main QML thread) using `QtConcurrent` or equivalent, to prevent UI blocking.

**Acceptance Criterion**: Register 10 tray items and open 5 context menus in rapid succession; verify the bar remains responsive and no frame drops occur.

### REQ-NF-002 (Robustness)
**The system** shall handle D-Bus service crashes gracefully: if an SNI item's service disconnects, the item shall be unregistered and removed from the tray; the watcher shall remain operational for future items.

**Acceptance Criterion**: Simulate a D-Bus service crash for a registered tray item; verify the item is removed and other items remain unaffected.

### REQ-NF-003 (Styling)
**All colors shall be sourced from `HoloniightPalette` (system HoloNight theme); no hardcoded hex values shall appear in QML or C++ rendering code.**

**Acceptance Criterion**: Audit all tray-related QML and C++ code for hardcoded color values; confirm none exist.

### REQ-NF-004 (Animation)
**All transitions and animations (slot slide-in, glow pulse) shall use QML animation timings consistent with existing holonight-shell animations (e.g., 100 ms for transitions, smooth easing curves).**

**Acceptance Criterion**: Measure animation durations in release builds; verify they match the specified timings within 10 %.

### REQ-NF-005 (Accessibility)
**Tray items shall have accessible names and descriptions for screen readers; tooltips shall be exposed via accessibility properties.**

**Acceptance Criterion**: Verify accessible names and descriptions are set on all tray items and reported by accessibility tools.

### REQ-C-001 (Constraint – DBusMenu Complexity)
**The DBusMenu subsystem is the largest implementation component. A full recursive parser, `AboutToShow` coordination, and `Event` dispatch require a dedicated C++ class with async D-Bus calls. This work is estimated at 40–50 % of the total feature effort.**

**Rationale**: DBusMenu is a complex protocol with dynamic updates, item enables/visibility, and submenu recursion. Underestimating scope risks schedule slip.

### REQ-C-002 (Constraint – Urgent Priority Sorting)
**Urgent slot priority requires a new sorting/filtering layer in the tray item model. Current model provides flat enumeration; the new layer must maintain separate urgent and non-urgent queues, coordinate slot allocation, and trigger animations. This logic is independent of the SNI/DBusMenu subsystems and requires careful testing.**

**Rationale**: The sorting layer is critical for correct urgent behavior and queue ordering. Bugs in this layer manifest as missed animations, incorrect promotion order, or visual inconsistency.

### REQ-C-003 (Constraint – TrayMenuSurface Positioning)
**Context menu popups must be positioned using absolute screen coordinates derived from the tray item's QML geometry (via `mapToGlobal`). `TrayMenuSurface` must support coordinate-based positioning, not attachment to a parent widget.**

**Rationale**: Layer-shell surfaces do not support automatic parent-relative positioning. Coordinates must be calculated and passed explicitly.

### REQ-C-004 (Constraint – Hyprland Urgency Limitation)
**The Hyprland compositor does not emit the urgent bit in `ext-workspace` notifications, so urgency is detected entirely via SNI `status == "NeedsAttention"`. This is expected and is not a defect.**

**Rationale**: Documented limitation in the ExtWorkspaceManager memory. Urgent badges are driven solely by SNI status.

### REQ-C-005 (Constraint – Icon Pixmap Format)
**SNI items may provide icons as pixmap arrays (via `IconPixmap`) or icon names (via `IconName`). The system shall prioritize icon names and fall back to pixmaps if icon name lookup fails.**

**Acceptance Criterion**: Register items with both icon names and pixmaps; verify correct icon is displayed in all cases.

### REQ-C-006 (Constraint – Passive Item Caching)
**Passive items are never visible. The model should cache passive items in memory (for future transition to non-passive) but exclude them from the visibility calculation and rendering pipeline to minimize overhead.**

**Rationale**: Some applications toggle passive/non-passive status; keeping items cached avoids re-registration overhead.

---

## Verification Checklist

- [x] **Overflow & Visibility** (REQ-F-001 through REQ-F-005): 3-slot cap, passive exclusion, animations, overflow label
- [x] **Urgent Priority** (REQ-F-006 through REQ-F-009): Slot preemption, queuing, urgency transitions
- [x] **Urgent Badge** (REQ-F-010 through REQ-F-012): Violet dot, glow animation, no red border
- [x] **Tooltip** (REQ-F-013 through REQ-F-017): SNI `ToolTip` fetch, model roles, fallback chain, `TooltipSurface` delay
- [x] **Context Menu** (REQ-F-018 through REQ-F-030): `GetLayout`, recursion, separators, submenus, `AboutToShow`, `Event`, disabled/hidden items, popup surface, refetch, depth limit, icon size
- [x] **Mouse Actions** (REQ-F-031 through REQ-F-036): Left/right/middle click, vertical/horizontal scroll, SNI calls, passive items ignored
- [x] **SNI Signals** (REQ-F-037 through REQ-F-042): `NewIcon`, `NewAttentionIcon`, `NewStatus`, `NewTitle`, `NewToolTip` subscriptions
- [x] **Non-Functional** (REQ-NF-001 through REQ-NF-005): Async fetching, crash handling, no hardcoded colors, animation timings, accessibility
- [x] **Constraints** (REQ-C-001 through REQ-C-006): DBusMenu complexity, urgent sorting, popup positioning, Hyprland limitation, icon formats, passive caching

## Completion Notes

Final validation for this SDD ran on 2026-05-25:

- `task build`
- `task qml-lint`
- `QT_QPA_PLATFORM=offscreen build/tests/holonight_tests --gtest_filter='Tray*:DbusMenu*:QmlSmoke.*' --gtest_also_run_disabled_tests` — 48 passed
- Manual Wayland session validation: Bluetooth tray DBusMenu opens on-screen, follows shell styling, activates menu items without the null `QDBusVariant` marshalling error, and sits close to the top bar.
