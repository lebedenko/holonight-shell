# Right Sidebar Feature Specification

**Feature Name:** Right Sidebar Panel with Tab Navigation
**Version:** 1.0
**Date:** 2026-06-13
**Status:** Specification

---

## Overview

The right sidebar is a transient per-monitor layer-shell panel hosted inside a fullscreen layer surface while open. It is triggered by clicking the clock widget in the top bar and displays six tabs: Overview, Calendar, Notifications, System, Quick Settings, and Media. The sidebar uses a left-side tab rail with content to the right, animates in three sequential phases (grow, slide, fade) totaling <=500ms, remembers the last-active tab within the session, and coordinates across monitors via mutual exclusion.

The sidebar follows the project-wide [shell overlay focus policy](../../shell-overlay-focus-policy.md): it owns keyboard input while open and closes when the visible workspace changes on any monitor.

---

## Surface & Lifecycle

### REQ-F-001: Sidebar Surface Creation
**Event-driven:** The system shall create a fullscreen `QQuickView` with `wlr-layer-shell` role for the target monitor when the sidebar opens.

**Acceptance Criterion:** Verify via Wayland protocol inspector that opening the sidebar creates a layer-shell surface with anchor `top | bottom | left | right` and the correct output assignment.

---

### REQ-F-002: Surface Positioning
**Ubiquitous:** The system shall position the visible sidebar panel at the top-right corner of its fullscreen host with a top margin of 8px below the compositor-reserved topbar area and a right margin of 24px from the screen right edge.

**Acceptance Criterion:** Measure the pixel coordinates of the visible sidebar panel on a screenshot and confirm it is right-aligned with a 24px margin and starts just below the topbar.

---

### REQ-F-003: Surface Sizing Pre-allocation
**Ubiquitous:** The system shall create a fullscreen host surface and size the visible panel from its content height, bounded by the available screen height.

**Acceptance Criterion:** Inspect the layer-shell surface geometry via `hyprctl clients -j` or Wayland logs and confirm the requested surface fills the monitor; inspect the visible panel and confirm its height tracks bounded content height.

---

### REQ-F-004: Surface Teardown
**Event-driven:** The system shall run the QML close animation, then destroy the sidebar `LayerSurface` and `QQuickView` so the compositor releases keyboard and pointer focus.

**Acceptance Criterion:** Confirm that after close animation completion the sidebar layer-shell surface disappears from Wayland/compositor inspection and the previously focused application can receive keyboard and pointer input again.

---

### REQ-NF-001: Process Stop While Closed
**State-driven:** While the sidebar is closed, no sidebar QML view shall remain alive, so internal QML timers, listeners, and active/occupancy gate polling stop.

**Acceptance Criterion:** Enable debug logging on sidebar tab content and confirm that monitor focus change events are not logged while the sidebar is closed.

---

### REQ-NF-002: Process Start When Shown
**Event-driven:** When the sidebar is shown, the system shall create the sidebar QML view and instantiate the active tab content.

**Acceptance Criterion:** Confirm that tab content is instantiated when the sidebar opens and destroyed when it closes.

---

## Trigger & Dismiss

### REQ-F-005: Clock Widget Click Trigger
**Event-driven:** When the user clicks the clock widget in a top bar, the system shall toggle the sidebar open state (open if closed, close if open) for that top bar's monitor.

**Acceptance Criterion:** Click a monitor's clock widget; that monitor's sidebar animates open. Click it again; the sidebar animates closed. Repeat on a different monitor; the previously open sidebar closes and the clicked monitor's sidebar opens.

---

### REQ-F-006: Click Outside to Dismiss
**Event-driven:** When the user clicks outside the sidebar surface while the sidebar is open, the system shall close the sidebar.

**Acceptance Criterion:** Click the clock to open the sidebar, then click on the desktop (or another panel) outside the sidebar bounds; the sidebar animates closed within 500ms.

---

### REQ-F-007: Escape Key to Dismiss
**Event-driven:** When the user presses the Escape key while the sidebar is open, the system shall close the sidebar.

**Acceptance Criterion:** Open the sidebar, press Escape, and confirm the sidebar animates closed.

---

### REQ-F-007A: Workspace Change Dismiss
**Event-driven:** When Hyprland reports that the visible workspace changed on any monitor, the system shall close any open sidebar.

**Acceptance Criterion:** Open the sidebar on monitor A, change the visible workspace on monitor A or B, and confirm the sidebar closes. Move the pointer to another monitor without changing its visible workspace and confirm the sidebar remains open.

---

### REQ-C-001: Dismiss Action Exclusivity
**Conditional:** Where the sidebar is open, the system shall treat a click outside the sidebar and an Escape key press as mutually exclusive dismiss actions within a single open cycle (only one dismiss event per open/close cycle is processed).

**Acceptance Criterion:** Open the sidebar, press Escape (sidebar closes), then immediately press Escape again before the close animation completes; confirm the sidebar does not re-open.

---

## Layout & Sizing

### REQ-F-008: Tab Bar Width
**Ubiquitous:** The system shall render the tab bar at a fixed width of 64px, positioned on the left edge of the sidebar.

**Acceptance Criterion:** Measure the tab bar width on a screenshot; confirm it is exactly 64px.

---

### REQ-F-009: Tab Content Area Positioning
**Ubiquitous:** The system shall position the content area to the right of the tab bar, with its width equal to the selected tab's preferred width.

**Acceptance Criterion:** Switch between tabs (each with different declared preferred widths) and confirm the content area width animates to each tab's preferred width.

---

### REQ-F-010: Minimum Sidebar Width
**Ubiquitous:** The system shall enforce a minimum total sidebar width of 64px (tab bar only, content area at 0px) during the grow phase of the open animation.

**Acceptance Criterion:** Trigger sidebar open and observe the first animation phase; confirm the sidebar width does not exceed 64px before the slide phase begins.

---

### REQ-F-011: Maximum Sidebar Height
**Ubiquitous:** The system shall constrain the maximum sidebar height to `screen.height - topbar.height - 8px - 24px`.

**Acceptance Criterion:** On a display with a known resolution, calculate the maximum sidebar height and verify via screenshot that the sidebar bottom edge is at least 24px from the screen bottom.

---

### REQ-F-012: Content Area Scrolling
**Conditional:** Where the selected tab's content is taller than the available sidebar height, the system shall render the tab bar as fixed (unmoved) and make the content area scrollable.

**Acceptance Criterion:** Open a tab with content exceeding the available height, scroll the content area, and confirm the tab bar remains stationary while content moves.

---

## Animation

### REQ-F-013: Open Animation — Phase 1 (Grow)
**Event-driven:** When the sidebar transitions from closed to open, the system shall first execute a grow-downward phase: the QML clip geometry animates from top (0px height) to bottom (maximum height) while the sidebar remains at its minimum width (64px).

**Acceptance Criterion:** Record the sidebar opening and measure the animation duration of phase 1; confirm it completes before phase 2 begins and the sidebar height visibly grows downward.

---

### REQ-F-014: Open Animation — Phase 2 (Slide)
**Event-driven:** After the grow phase completes, the system shall execute a slide-in phase: the content area expands horizontally from 0px to the selected tab's preferred width.

**Acceptance Criterion:** Record the sidebar opening and measure the animation duration of phase 2; confirm it begins after phase 1 completes and the content area visibly expands to its preferred width.

---

### REQ-F-015: Open Animation — Phase 3 (Fade)
**Event-driven:** After the slide phase completes, the system shall execute a content fade-in phase: the tab content opacity animates from 0 to 1.

**Acceptance Criterion:** Record the sidebar opening and measure the animation duration of phase 3; confirm it begins after phase 2 completes and the content opacity visibly increases.

---

### REQ-NF-003: Total Open Animation Duration
**Ubiquitous:** The system shall complete the entire open animation (all three phases) in ≤500ms.

**Acceptance Criterion:** Record the sidebar opening and measure the total time from first visible motion to full opacity; confirm it is ≤500ms.

---

### REQ-F-016: Close Animation — Reverse Sequence
**Event-driven:** When the sidebar transitions from open to closed, the system shall execute the animation phases in reverse order: fade-out (opacity 1 → 0), slide-left (width to 0px), then shrink-up (height to 0px).

**Acceptance Criterion:** Record the sidebar closing and confirm the three phases occur in reverse order with the same timing as the open animation.

---

### REQ-NF-004: Total Close Animation Duration
**Ubiquitous:** The system shall complete the entire close animation in ≤500ms.

**Acceptance Criterion:** Record the sidebar closing and measure the total duration; confirm it is ≤500ms.

---

### REQ-F-017: Tab Switch Animation
**Event-driven:** When the user switches to a different tab while the sidebar is open, the system shall animate only the slide phase: the content area width animates from the current tab's preferred width to the new tab's preferred width (no fade or grow phases).

**Acceptance Criterion:** Open the sidebar, switch between two tabs with different preferred widths, and confirm only the width animates (no opacity or grow animations).

---

## Tab System

### REQ-F-018: Six Tabs
**Ubiquitous:** The system shall provide six tabs in the sidebar: Overview, Calendar, Notifications, System, Quick Settings, and Media.

**Acceptance Criterion:** Click through all tabs in the tab bar and confirm each tab is clickable and displays its corresponding content area.

---

### REQ-F-019: Initial Tab Selection
**Event-driven:** When the sidebar opens for the first time in a session, the system shall display the Overview tab.

**Acceptance Criterion:** Open the sidebar immediately after application startup and confirm the Overview tab is active.

---

### REQ-F-020: Tab Memory Within Session
**State-driven:** While the application is running, the system shall remember the last-active tab for each per-monitor sidebar between close and re-open cycles (in-memory only, not persisted to disk).

**Acceptance Criterion:** Open the sidebar, navigate to the Notifications tab, close the sidebar, open the sidebar again, and confirm the Notifications tab is still active. Restart the application and open the sidebar; confirm it returns to Overview (no disk persistence).

---

### REQ-F-021: Tab Icon Rendering
**Ubiquitous:** The system shall render each tab's icon using Qt `Canvas` drawing primitives following the same pattern as existing status icons (network, audio, battery).

**Acceptance Criterion:** Inspect the QML source of the tab bar and confirm each tab declares a Canvas-based icon draw function.

---

### REQ-F-022: Tab Icon Appearance
**Ubiquitous:** The system shall use sensible symbolic icon defaults for each tab and color them according to the HoloNight theme via `HoloniightPalette` tokens.

**Acceptance Criterion:** Take a screenshot and confirm all tab icons are visible, distinct, and match the overall theme color scheme.

---

## Content & Scrolling

### REQ-F-023: Tab Content Placeholder
**Ubiquitous:** The system shall display placeholder content in all six tabs for this initial implementation.

**Acceptance Criterion:** Switch to each tab and confirm some visible placeholder content (text, layout, or shape) is rendered in the content area.

---

### REQ-F-024: Preferred Width Declaration
**Ubiquitous:** Each tab shall declare its own preferred content width in QML via a `preferredWidth` property.

**Acceptance Criterion:** Inspect the QML source of each tab component and confirm each declares a `preferredWidth` property.

---

### REQ-F-025: Scroll Behavior
**Conditional:** Where a tab's content exceeds the available height, the system shall scroll the content area vertically while keeping the tab bar fixed.

**Acceptance Criterion:** Populate a tab with content taller than the sidebar, scroll it, and confirm the tab bar does not move.

---

## Multi-monitor

### REQ-F-026: Per-Monitor Sidebar Surfaces
**Ubiquitous:** The system shall maintain one sidebar surface per monitor, each with independent state (open/closed, active tab).

**Acceptance Criterion:** On a dual-monitor setup, open the sidebar on monitor A, then click the clock on monitor B; confirm monitor A's sidebar closes and monitor B's sidebar opens.

---

### REQ-F-027: Sidebar Mutual Exclusion
**Event-driven:** When the user opens the sidebar on one monitor, the system shall close any open sidebar on other monitors.

**Acceptance Criterion:** On a dual-monitor setup, open the sidebar on monitor A, open the sidebar on monitor B; confirm monitor A's sidebar immediately closes.

---

### REQ-F-028: SidebarManager Singleton
**Ubiquitous:** The system shall implement a C++ `SidebarManager` object registered as the `HolonightShell.SidebarManager` QML singleton to coordinate multi-monitor sidebar state and enforce mutual exclusion.

**Acceptance Criterion:** Inspect the C++ source and confirm `SidebarManager` is registered with `qmlRegisterSingletonType`, has process-lifetime ownership through `ShellApplication`, and exposes methods to query and set sidebar open state per monitor.

---

## Icon Rendering

### REQ-F-029: Canvas Icon Implementation
**Ubiquitous:** The system shall implement all tab bar icons using Qt `Canvas` drawing primitives (path drawing, stroke/fill) rather than pixmap assets.

**Acceptance Criterion:** Inspect the QML tab bar component and confirm each icon uses `Canvas { onPaint { ... } }` rather than `Image` elements.

---

### REQ-F-030: Icon Palette Integration
**Ubiquitous:** All icon stroke and fill colors shall be sourced from the HoloNight theme via `HoloniightPalette` tokens; no hardcoded hex values shall appear.

**Acceptance Criterion:** Search the tab bar QML source for the string `#[0-9a-f]{6}` (hex color); confirm no matches are found (all colors use palette tokens).

---

## Clock Widget Cleanup

### REQ-F-031: Remove Temporary Clock Popup
**Ubiquitous:** The clock widget shall no longer open the temporary clock popup path once the sidebar is implemented. Shared popup infrastructure used by other widgets, such as the session popup, shall remain.

**Acceptance Criterion:** Inspect `ClockSection.qml` and confirm its click handler calls `SidebarManager.toggle(root.barMonitorName)` rather than opening `PopupSurface`.

---

## Cross-cutting Concerns

### REQ-NF-005: Keyboard Focus
**Deferred:** Keyboard focus within sidebar content is not part of the placeholder implementation. The fullscreen sidebar host shall handle Escape while the sidebar is open.

**Acceptance Criterion:** Open the sidebar and press Escape; confirm the sidebar closes without sending Escape to the previously focused application. Full Tab focus traversal is deferred until real interactive tab content is implemented.

---

### REQ-NF-006: Monitor Awareness
**Ubiquitous:** The system shall route all sidebar operations (open/close, tab switching, content interaction) to the correct monitor by passing the monitor name through the QML component hierarchy and C++ invokable interfaces.

**Acceptance Criterion:** On a dual-monitor setup, click the clock on monitor B and confirm only monitor B's sidebar opens (monitor A is unaffected).

---

### REQ-C-002: Layer-Shell Anchoring
**Ubiquitous:** The system shall configure each sidebar layer-shell surface with anchor flags `top | right` to position it at the top-right corner of its output.

**Acceptance Criterion:** Inspect Wayland protocol logs or `hyprctl clients -j` and confirm the sidebar surface anchor bits match `top | right`.

---

### REQ-C-003: No Hardcoded Colors
**Ubiquitous:** All colors used in the sidebar QML shall be sourced from the HoloNight theme via `HoloniightPalette` tokens; no hardcoded hex values shall appear in source.

**Acceptance Criterion:** Search the entire `src/qml/RightSidebar/` directory for the regex `#[0-9a-f]{6}` or `rgb(` with literal numbers; confirm no matches are found.

---

### REQ-C-004: QML Module Path
**Ubiquitous:** All sidebar QML files shall reside in `src/qml/RightSidebar/` and be registered in the `HolonightShell` QML module via `CMakeLists.txt` entries in `HOLONIGHT_QML_FILES`.

**Acceptance Criterion:** List all QML files in `src/qml/RightSidebar/` and confirm each appears in `HOLONIGHT_QML_FILES` in CMakeLists.txt.

---

### REQ-C-005: QRC Path Convention
**Ubiquitous:** All sidebar QML imports and asset references shall use the QRC prefix `/HolonightShell/RightSidebar/` (e.g., `qrc:/HolonightShell/RightSidebar/TabBar.qml`).

**Acceptance Criterion:** Inspect all sidebar QML files for `qrc:/` paths and confirm they all use the `/HolonightShell/RightSidebar/` prefix.

---

### REQ-NF-007: Accessibility
**Deferred:** The system shall eventually provide keyboard navigation for sidebar interactions (tab switching via arrow keys, content interaction via Tab, dismiss via Escape). This iteration only requires Escape dismissal.

**Acceptance Criterion:** Test Escape dismissal. Keyboard-only tab switching and content navigation are deferred.

---

## Non-functional Requirements Summary

| ID | Category | Requirement |
|----|-----------|----|
| REQ-NF-001 | Lifecycle | Pause processes while hidden |
| REQ-NF-002 | Lifecycle | Resume processes when shown |
| REQ-NF-003 | Animation | Open animation ≤500ms |
| REQ-NF-004 | Animation | Close animation ≤500ms |
| REQ-NF-005 | UX | Keyboard focus management (deferred except Escape dismiss) |
| REQ-NF-006 | Architecture | Per-monitor routing |
| REQ-NF-007 | Accessibility | Keyboard navigation (deferred except Escape dismiss) |

---

## Acceptance Test Checklist

- [x] All six tabs render and are selectable
- [x] Clock widget click toggles sidebar open/close on the clicked monitor
- [x] Click outside closes sidebar
- [x] Escape key closes sidebar
- [x] Open animation executes three phases in ≤500ms
- [x] Close animation executes three phases (reverse) in ≤500ms
- [x] Tab switching animates content area width only
- [x] Sidebar remembers last tab within session (not persisted to disk)
- [x] Multi-monitor mutual exclusion works: opening on monitor B closes monitor A
- [x] Tab bar icons are Canvas-drawn and use HoloniightPalette colors
- [x] Content scrolls when exceeding available height
- [x] No hardcoded colors in QML source
- [x] All QML files registered in CMakeLists.txt
- [x] Layer-shell surface correctly anchored top-right
- [x] Escape dismiss works; Tab and arrow-key navigation are deferred
- [x] Sidebar surface maintains `visible` toggle (not `hide()`/`show()`)

---

## Glossary

- **Layer-shell**: Wayland protocol for desktop UI layers (panels, docks, notifications)
- **Tab bar**: Fixed-width (64px) vertical strip of clickable tab icons on the left edge
- **Content area**: Right-side region containing the selected tab's content
- **Preferred width**: Declared width for a tab's content area (set per-tab in QML)
- **HoloniightPalette**: Project theme token provider (note: double-i spelling)
- **SidebarManager**: ShellApplication-owned C++ object registered as the QML singleton coordinating multi-monitor sidebar state
- **Canvas-drawn**: Qt Quick Canvas primitive rendering (not pixmap assets)
