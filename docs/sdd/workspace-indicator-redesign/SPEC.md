# Workspace Indicator Redesign — EARS Requirements Specification

**Status:** Specification Draft  
**Date:** 2026-07-01  
**Project:** holonight-shell (`apps/shell/qml/Topbar/WorkspaceSection.qml`, `WorkspacePill.qml`, `WorkspaceModel`)

---

## 1. Scope

This specification redesigns the topbar workspace indicator to:
- Fix reserved-space bugs (hidden elements occupying layout width).
- Add per-monitor window centering for independent workspace navigation on multi-monitor systems.
- Introduce dynamic special-workspace (non-numeric) indicators.
- Unify urgent styling across all workspace elements.
- Improve visual feedback for multi-monitor workspace focus.

**Current implementation context:**
- `WorkspaceModel` (QML singleton) holds `WorkspaceEntry{id, name, state, on_monitor}` rows from `ExtWorkspaceManager`/`ext-workspace-v1` protocol.
- `WorkspaceState` enum: `Empty, Occupied, FocusedActiveMonitor (Active), FocusedInactiveMonitor, Urgent`.
- `WorkspacePill.qml` renders N pills (where N = `displayCount` from config) plus two hidden overflow/urgent pills (bug: occupy layout space when invisible).
- No per-monitor active-workspace query exists; current `effectiveState()` uses single global `focused_workspace_id_`.
- Special workspaces (non-numeric names like `"special:scratch"`) currently break: `toInt()` fails, collide on `id == 0`, are invisible.

**In scope:** Specification of functional + non-functional + constraint requirements in EARS format.  
**Out of scope:** Implementation code, test specifics, Hyprland protocol versioning, per-monitor special-workspace scoping.

---

## 2. Functional Requirements

### REQ-F-001: No Reserved Space When Idle
**Type:** Ubiquitous  
**Statement:** The workspace section's layout width shall reflect only the currently-visible elements; no invisible elements (pills, arrows, separators, dots) shall occupy fixed space.

**Rationale:** Current hidden-but-laid-out overflow pills waste bar space. Dynamic visibility requires dynamic layout (no reserved width for future elements).

**Acceptance Criteria:**
1. When no special workspaces exist, the special-workspace separator and all special-workspace dots are not rendered in the DOM (not merely `opacity: 0`).
2. When all numbered pills, edge arrows, and special indicators are measured, their combined width matches the `Row` layout's reported width (no unaccounted-for gaps or reserved slots).
3. When the active workspace is within the visible window `[start, start + N - 1]`, the right-edge arrow is visible only if an occupied/urgent workspace exists beyond the window (not visible just because higher IDs are theoretically possible); otherwise it is not rendered in the DOM.
4. When the active workspace on a monitor is at ID 1 or within a window centered on ID 1 (left-clamped), the left-edge arrow is not rendered in the DOM.

---

### REQ-F-002: Constant Numbered Pill Count
**Type:** Ubiquitous  
**Statement:** Exactly N numbered workspace pills shall be rendered and visible at all times, where N = `ConfigService::barWorkspaceCount()` (or equivalent config property, default 5), regardless of workspace state, occupancy, or urgency.

**Rationale:** Fixed pill count provides consistent UI real estate; dynamic pills would be jarring.

**Acceptance Criteria:**
1. Exactly N `WorkspacePill` elements are instantiated in the QML scene for every monitor's `WorkspaceSection`.
2. Every numbered pill (IDs 1 through N) is rendered visible (not `opacity: 0` or `visible: false`) at all times during normal operation.
3. If the config value N changes at runtime (e.g., user edits settings), the pill count adjusts on the next bar redraw; no other elements need respond immediately.

---

### REQ-F-003: Per-Monitor Centered Sliding-Window Scroll
**Type:** Event-driven + State-driven  
**Statement:** Each monitor's `WorkspaceSection` shall independently maintain a visible window of exactly N consecutive workspace IDs, `[window_start, window_start + N - 1]`. The window shall be recomputed to center on the currently-active workspace for that specific monitor (not the global compositor focus). When the window changes, pills shall slide to their new positions via smooth animation.

**Rationale:** Multi-monitor systems need independent navigation per monitor; per-monitor active-workspace tracking is prerequisite.

**Window Centering Algorithm:**
```
active_id_for_this_monitor = WorkspaceModel.activeWorkspaceForMonitor(barMonitorName)
target_window_start = clamp(
    active_id_for_this_monitor - floor((N - 1) / 2),
    1,  // left-clamped at workspace ID 1
    +∞  // unbounded on the right
)
window_start = target_window_start
```

**Worked Examples (N = 5):**
- Active=1: `[1*][2][3][4][5]` — left-clamped, no scroll.
- Active=3: `[1][2][3*][4][5]` — already centered, no scroll.
- Active=4 (previously at active=3): `[2][3][4*][5][6]` — window slides left, pills animate position.
- Active=6: `[4][5][6*][7][8]` — window slides right.

**Acceptance Criteria:**
1. `WorkspaceModel` exposes a new signal/function `activeWorkspaceForMonitor(QString monitorName)` that returns the currently-active workspace ID for that monitor (returns 0 or invalid if no workspace is focused on that monitor).
2. `WorkspaceSection.qml` computes `window_start` using the algorithm above, clamped as specified.
3. When `activeWorkspaceForMonitor(barMonitorName)` emits a change for this monitor, `window_start` is recalculated and pills' model indices shift accordingly.
4. As pills' model indices shift, their visual `x` position animates smoothly over 150–250 ms (duration consistent with existing `Behavior on x` patterns in the codebase) using a smooth easing curve (e.g., `Easing.OutCubic` or `Easing.InOutSine`).
5. Clicking any numbered pill shall activate that workspace on the focused monitor (existing behavior unchanged); this triggers `activeWorkspaceForMonitor(focusedMonitor)` to update, which re-centers the window if necessary.

---

### REQ-F-004: Edge Arrows with Context-Aware Activation
**Type:** Event-driven + Conditional  
**Statement:** Left and right edge arrows shall appear/disappear based on window position and occupancy beyond the edges. Arrow visibility shall indicate scrollability; arrow behavior (urgent activation vs. pan-without-switch) shall depend on whether an urgent workspace lies beyond that edge.

**Left Arrow Visibility:**
- Visible if and only if `window_start > 1` (workspace ID 1 is scrolled out of view).
- Hidden when `window_start == 1` (workspace 1 is in the visible window).

**Right Arrow Visibility:**
- Visible if and only if at least one workspace with state `Occupied` or `Urgent` exists at ID ≥ `window_start + N` (i.e., beyond the right edge of the visible window).
- Hidden if all workspaces beyond the right edge are `Empty` or do not exist.

**Arrow Styling & Behavior:**

| Condition | Arrow Color | Arrow Glow | Behavior on Click |
|-----------|-------------|-----------|-------------------|
| Edge has ≥1 `Urgent` workspace | `HoloniightPalette.accentViolet` | Pulsing glow (continuous animation, 0.5–1.5s cycle) | Immediately activate the first/nearest `Urgent` workspace beyond that edge; focus that monitor. Workspace activation is global (same as clicking a pill). |
| Edge has occupied (non-urgent) workspaces only | `HoloniightPalette.workspaceOccupied` or equivalent neutral color | No glow | Pan the window by exactly ±1 step in that direction (increment/decrement `window_start` by 1, clamped as above). This is a **peek without switching**; the active workspace and monitor focus do not change. |
| Edge has no occupied/urgent workspaces (all empty or non-existent) | N/A (arrow hidden) | N/A | N/A |

**Pan-Reset Rule:**
A manual pan (achieved via edge arrow click to peek) is discarded and the window re-centers as soon as:
- The active workspace on this monitor changes (by any means: pill click, compositor keybind, workspace switch, etc.).

A manual pan is NOT discarded by:
- Incidental occupancy/urgency changes on other monitors or in non-active workspaces.
- Changes to special-workspace state.
- Other cosmetic model updates unrelated to this monitor's own focus.

**Acceptance Criteria:**
1. Left arrow is rendered (not in DOM) if and only if `window_start > 1`.
2. Right arrow is rendered if and only if `max(state[ID] for ID in [window_start + N, ∞)) ∈ {Occupied, Urgent}`.
3. When either arrow's hidden edge (left: IDs `[1, window_start)`; right: IDs `[window_start + N, ∞)`) contains an `Urgent` workspace, that arrow's glyph renders in `accentViolet` with a pulsing glow (at least 2 intensity levels, animating continuously, no manual trigger needed). This applies symmetrically to both arrows.
4. When an arrow's hidden edge contains only `Occupied` (non-urgent) workspaces, that arrow renders in `workspaceOccupied` color (or a neutral workspace color) without glow.
5. Clicking the left arrow when no `Urgent` workspace exists in `[1, window_start)` decrements `window_start` by 1 (clamped at 1) and pills animate position; active workspace and focused monitor do not change.
6. Clicking the left arrow when an `Urgent` workspace exists in `[1, window_start)` activates the highest-ID (nearest) `Urgent` workspace in that range (same dispatch mechanism as clicking a pill) instead of panning.
7. Clicking the right arrow when urgent workspaces exist beyond the edge activates the lowest-ID (nearest) `Urgent` workspace beyond the edge (same dispatch mechanism as clicking a pill).
8. Clicking the right arrow when only occupied (non-urgent) workspaces exist beyond the edge increments `window_start` by 1 and pills animate position; active workspace and focused monitor do not change.
9. When the active workspace on this monitor changes (detected via `activeWorkspaceForMonitor(barMonitorName)` signal), the window is re-centered (REQ-F-003), discarding any active manual pan.

---

### REQ-F-005: Special Workspace Indicators
**Type:** Ubiquitous + Event-driven  
**Statement:** For each currently-existing special workspace (Wayland `ext_workspace_handle_v1` object whose `name` does not parse as an integer — e.g., `"special:scratch"`), a circular dot indicator shall be rendered with state-dependent styling. Special-workspace state shall be derived from the same active/urgent bits from `ExtWorkspaceHandle::ext_workspace_handle_v1_state()` already parsed for numbered workspaces. State is **global** (identical across all monitors' bars, not per-monitor-scoped).

**State Definitions for Special Workspaces:**
- **Hidden**: created, not currently visible on any monitor. Radius 4 px, color `HoloniightPalette.textDisabled`, no glow.
- **Active** (visible): currently shown/toggled. Radius 8 px, color `HoloniightPalette.accentCyan`, subtle static glow (soft shadow/radiance effect).
- **Urgent**: priority indicator set in Wayland protocol. Color `HoloniightPalette.accentViolet`, intense **pulsing** glow (same as urgent edge arrows: continuous animation, 0.5–1.5s cycle, at least 2 intensity levels).

**Dot Interaction:**
- Each dot is clickable; clicking activates that special workspace (dispatches workspace activation; compositor determines visibility/toggle behavior).
- Visual feedback on hover (e.g., subtle scale change or brightness increase) is recommended but not mandated.

**Acceptance Criteria:**
1. `ExtWorkspaceManager` parses each `ext_workspace_handle_v1.name` via `QString::toInt(&ok)` to distinguish numeric vs. special workspaces; special workspaces are stored separately in the model (not colliding on `id == 0`).
2. For each special workspace, store: `name` (e.g., `"special:scratch"`), `active` state (from protocol), `urgent` state (from protocol), and a unique identifier (e.g., name itself, or a separate incrementing ID).
3. `WorkspaceModel` exposes a list of special workspaces (e.g., `specialWorkspaceList() : QList<SpecialWorkspaceEntry>`) that is updated live when the Wayland compositor reports new/destroyed special workspaces.
4. For each special workspace, render one circular dot (via `Rectangle { radius: 50%; ... }` or Canvas circle).
5. Dot radius is 4 px when state = `Hidden`, 8 px when state = `Active` or `Urgent`; radius changes are animated (smooth transition ~100–150 ms).
6. Dot color follows the state table above (textDisabled for Hidden, accentCyan for Active, accentViolet for Urgent).
7. When state = `Urgent`, the dot renders a pulsing glow (identical continuous animation as urgent edge arrows).
8. Clicking a dot invokes `WorkspaceModel.activateWorkspace(specialWorkspaceName)` or similar (same dispatch path as numbered pills); the compositor handles the actual toggle/show behavior.
9. When a new special workspace appears (Wayland reports a new `ext_workspace_handle_v1` with non-numeric name), a new dot is added live to all monitors' bars (no app restart needed).
10. When an existing special workspace is destroyed (Wayland `ext_workspace_handle_v1::destroy`), its dot is immediately removed from all monitors' bars.

---

### REQ-F-006: Visual Separator and Dynamic Grouping
**Type:** Conditional + State-driven  
**Statement:** When at least one special workspace exists, a visual separator (vertical line or small gap marker) shall render immediately after the numbered-pill / edge-arrow section and immediately before the first special-workspace dot. When the special-workspace count returns to zero, both separator and all dots shall disappear, reclaiming layout space.

**Rationale:** Visually groups numbered workspaces (left) from special workspaces (right); dynamic removal respects REQ-F-001 (no reserved space).

**Acceptance Criteria:**
1. When `specialWorkspaceList().count() >= 1`, a separator element (e.g., `Rectangle` with `width: 1, height: 24` and color `HoloniightPalette.borderPassive` or similar) is rendered between the numbered-pill / arrow section and the special-workspace dots.
2. When `specialWorkspaceList().count() == 0`, the separator is not rendered in the DOM (not merely `opacity: 0`).
3. Horizontal spacing between the separator and the first dot, and between successive dots, is consistent (e.g., 8–12 px gaps) and does not vary by state.
4. When the first special workspace is created, the separator appears smoothly (fade-in or layout animation).
5. When the last special workspace is destroyed, the separator disappears smoothly (fade-out or layout animation).

---

### REQ-F-007: Unified Urgent Color — Eliminate `borderUrgent` and `error`
**Type:** Ubiquitous  
**Statement:** All workspace indicators (numbered pills, edge arrows, special-workspace dots) shall use `HoloniightPalette.accentViolet` exclusively for urgent state styling. No workspace indicator component shall reference `HoloniightPalette.borderUrgent` or `HoloniightPalette.error` tokens for any purpose.

**Rationale:** Consistent visual language for urgency across the shell; eliminates redundant palette tokens from workspace context.

**Specific Changes:**
- **`WorkspacePill.qml` urgent state:** Change border color from `HoloniightPalette.borderUrgent` → `HoloniightPalette.accentViolet`; change text color from `HoloniightPalette.error` → `HoloniightPalette.accentViolet`.
- **Edge arrows urgent style:** Use `accentViolet` (specified in REQ-F-004).
- **Special-workspace dots urgent style:** Use `accentViolet` (specified in REQ-F-005).

**Acceptance Criteria:**
1. `WorkspacePill.qml` contains no references to `HoloniightPalette.borderUrgent` or `HoloniightPalette.error` in any code path.
2. All edge-arrow rendering code uses `HoloniightPalette.accentViolet` for urgent styling.
3. All special-workspace dot rendering code uses `HoloniightPalette.accentViolet` for urgent styling.
4. Running a text search for `borderUrgent\|error` in `WorkspaceSection.qml`, `WorkspacePill.qml`, and any new special-workspace-indicator component file yields zero matches related to workspace urgency.

---

### REQ-F-008: Active-on-Inactive-Monitor Pill Styling
**Type:** State-driven  
**Statement:** When a numbered pill represents the currently-active workspace on a **non-focused** monitor (state = `FocusedInactiveMonitor`), the pill shall render with no border (border width = 0) and fill/text colors matching the `Occupied` state exactly. Additionally, a small colored dot shall appear at the pill's top-center to indicate "this workspace is active on another monitor."

**Rationale:** Visual distinction between "active here" (FocusedActiveMonitor) and "active elsewhere" (FocusedInactiveMonitor); clearer multi-monitor awareness.

**Styling Details:**
- **Fill color:** `HoloniightPalette.workspaceOccupied` (same as `Occupied` state).
- **Text color:** `HoloniightPalette.textSecondary` (same as `Occupied` state).
- **Border:** Removed entirely for this state (border width 0); no `borderActive` or other border color applied.
- **Top-center dot:** Small circular indicator, radius 3–4 px, color `HoloniightPalette.accentCyan`, positioned 2–4 px above the top edge of the workspace number text, centered horizontally on the pill.

**Acceptance Criteria:**
1. A `WorkspacePill` in state `FocusedInactiveMonitor` renders with `border.width: 0` (no border).
2. Fill color is `HoloniightPalette.workspaceOccupied` (verify against current `_style` block's `Occupied` value).
3. Text color is `HoloniightPalette.textSecondary` (verify against current `_style` block's `Occupied` value).
4. A small dot (radius 3–4 px) is rendered at the pill's top-center, colored `HoloniightPalette.accentCyan`.
5. Dot position does not interfere with the workspace number text; visual hierarchy is clear.
6. When the pill's state changes away from `FocusedInactiveMonitor` (e.g., focus shifts to this monitor or workspace becomes occupied-but-not-active), the dot is removed.
7. When the pill's state changes to `FocusedInactiveMonitor`, the dot appears (with fade-in animation over ~100 ms if possible, or instant if animation is complex).

---

## 3. Non-Functional Requirements

### REQ-NF-001: Config Reuse — No New Configuration Keys
**Type:** Ubiquitous  
**Statement:** The window size N (number of visible numbered pills) shall be driven by the existing `ConfigService::barWorkspaceCount()` configuration property (or equivalent). No new configuration keys (e.g., `barWorkspaceScrollMode`, `specialWorkspaceVisibility`) shall be introduced for this feature.

**Rationale:** Minimizes config sprawl; reuses existing user settings.

**Acceptance Criteria:**
1. `WorkspaceModel` reads N from the existing config property on startup and listens for runtime changes.
2. No new properties are added to `holonight-config` or `holonight-settings` for workspace indicator behavior.

---

### REQ-NF-002: Animation Smoothness and Consistency
**Type:** Ubiquitous  
**Statement:** All animated transitions (pill sliding, dot radius changes, glow pulsing, separator fade) shall use smooth, visually-consistent easing curves and durations. Animation durations shall fall in the range 100–250 ms for position/size changes and 0.5–1.5 s for continuous glow pulsing (urgent indicators).

**Rationale:** Polished UI feel; consistency with existing shell animations.

**Acceptance Criteria:**
1. Pill position changes (sliding window) animate over 150–250 ms with an easing curve such as `Easing.OutCubic` or `Easing.InOutSine`.
2. Dot radius changes (Hidden ↔ Active/Urgent) animate over 100–150 ms.
3. Dot color transitions (state changes) animate over 100–150 ms.
4. Urgent glow pulsing (edge arrows, urgent dots) cycles continuously over 0.5–1.5 s with at least 2 intensity levels (e.g., glowRadius 0 → 8 → 0 over the cycle).
5. All animations use `Behavior on <property>` or explicit `NumberAnimation` blocks (not property bindings alone).

---

### REQ-NF-003: Palette-Only Color Usage
**Type:** Ubiquitous  
**Statement:** Every color in the workspace indicator section (numbered pills, edge arrows, special-workspace dots, separator) shall come from `HoloniightPalette` tokens. No hardcoded hexadecimal color values (e.g., `"#ffffff"`) or computed RGB values shall appear in QML or C++ workspace indicator code.

**Rationale:** Ensures consistency with HoloNight theme; allows theme-wide color changes without code edits.

**Acceptance Criteria:**
1. Grep search for `['"](#[0-9a-fA-F]{6}|rgb)` in `WorkspaceSection.qml`, `WorkspacePill.qml`, and any new special-workspace-indicator files yields zero matches (no hardcoded colors).
2. All color assignments explicitly reference `HoloniightPalette.<token>` or computed values built from palette tokens (e.g., `HoloniightPalette.accentCyan.toString()`).
3. No inline color definitions (e.g., `color: Qt.lighter(...)` without a base palette token) for workspace-specific states.

---

## 4. Constraints

### REQ-C-001: Per-Monitor Active-Workspace Resolution Architecture
**Type:** Architecturally significant  
**Statement:** Per-monitor active-workspace query (required by REQ-F-003, REQ-F-004) does not exist in the current codebase. Implementing this feature requires:

1. Extending `ExtWorkspaceGroup` and/or `ExtWorkspaceManager` to map Wayland `wl_output*` pointers to monitor output names (e.g., `"DP-3"`, `"HDMI-1"`). Precedent exists in `libs/holonight-surfaces/src/PerMonitorLayerManager.cpp` (uses `QNativeInterface::QWaylandScreen` to resolve output names).
2. Storing per-monitor active-workspace ID (when available from the Wayland protocol, or synthesizing it from compositor hints).
3. Exposing a new `WorkspaceModel::activeWorkspaceForMonitor(QString monitorName)` signal/function that emits/returns the current active workspace ID for that monitor.

This is a **prerequisite for correctness**, not an optional optimization. The design of REQ-F-003 (per-monitor window centering) is invalidated if per-monitor active-workspace tracking is not implemented.

**Acceptance Criteria:**
1. `ExtWorkspaceManager` includes a private mapping from `wl_output*` (or a unique output ID) to monitor output name.
2. When the Wayland compositor reports output bind/unbind events (via `wl_registry` or similar), the mapping is updated.
3. `WorkspaceModel::activeWorkspaceForMonitor(QString)` is implemented and callable from QML.
4. When the active workspace on any monitor changes (detected via `ext_workspace_handle_v1` state updates or compositor protocol), the signal `activeWorkspaceForMonitor(monitorName)` is emitted with the new active ID for that monitor.
5. In single-monitor scenarios, the implementation correctly identifies the single monitor's name and returns its active workspace.

---

### REQ-C-002: Empirical Verification — Special Workspace Protocol Support
**Type:** Empirical risk  
**Statement:** This specification assumes that Hyprland (and future Wayland compositors implementing `ext-workspace-v1`) expose special workspaces (non-numeric-named `ext_workspace_handle_v1` objects) via the protocol with usable `active` and `urgent` state bits. **This assumption has not been verified in a live Wayland session.**

**Required Verification:**
Before marking this feature complete, a developer must:
1. Launch holonight-shell on a live Hyprland session with ≥1 special workspace created (e.g., `hyprctl dispatch movetoworkspace special:scratch`).
2. Observe that the special-workspace indicator dots appear on the topbar.
3. Verify that dot state changes (Hidden ↔ Active ↔ Urgent, if possible) correctly reflect the protocol-reported state.
4. Verify that clicking a special-workspace dot successfully activates/toggles that workspace.
5. Document findings in a verification report (see REQ-C-004).

If the assumption is false (Hyprland does not expose special workspaces via `ext-workspace-v1`), the feature must be adapted or deferred.

**Acceptance Criteria:**
1. Live Hyprland test session completed (documented in verification report).
2. Special-workspace dots render and respond to state changes as specified.
3. No silent failures or protocol mismatches observed.

---

### REQ-C-003: Empirical Verification — Per-Monitor Active Workspace Distinction
**Type:** Empirical risk  
**Statement:** This specification relies on correctly distinguishing the active workspace **per monitor** in a multi-monitor Hyprland session. The implementation assumes that `wl_output*` → monitor-name resolution (REQ-C-001) correctly pairs active-workspace state with the right monitor and that per-monitor focus is accurately reported by the protocol.

**Required Verification:**
Before marking this feature complete, a developer must:
1. Launch holonight-shell on a live Hyprland session with ≥2 monitors (e.g., `DP-3` + `HDMI-1`).
2. Activate different workspaces on each monitor (e.g., workspace 2 on DP-3, workspace 5 on HDMI-1).
3. Observe that each monitor's topbar displays a different centered window (DP-3 centered on 2, HDMI-1 centered on 5).
4. Verify that clicking pills or edge arrows on monitor A does not affect monitor B's window.
5. Verify that when focus switches to monitor B (compositor keybind or mouse move), the window re-centers on monitor B's active workspace.
6. Document findings in a verification report (see REQ-C-004).

If per-monitor distinction does not work (e.g., both monitors always show the globally-focused workspace), the feature must be adapted.

**Acceptance Criteria:**
1. Live multi-monitor Hyprland test session completed (documented in verification report).
2. Each monitor's topbar independently centers its window on the active workspace for that monitor.
3. Cross-monitor window changes do not interfere with per-monitor navigation.
4. No silent protocol mismatches or blocking errors observed.

---

### REQ-C-004: Verification Report
**Type:** Documentation  
**Statement:** All findings from empirical verification (REQ-C-002, REQ-C-003) shall be documented in a verification report, co-located with this specification.

**Report Format:**
- File: `docs/sdd/workspace-indicator-redesign/VERIFICATION.md`
- Sections: Test environment (Hyprland version, monitor count/names, shell version), test scenarios, observations, any deviations from spec, sign-off or escalation.

**Acceptance Criteria:**
1. Verification report exists at the specified path.
2. Report covers special-workspace protocol support (REQ-C-002) and per-monitor active-workspace distinction (REQ-C-003).
3. Report is signed off by developer (name, date) or escalates blockers to the team.

---

### REQ-C-005: No Breaking Changes to Workspace Activation Mechanism
**Type:** Constraint  
**Statement:** The existing workspace activation dispatch mechanism (pill click → `WorkspaceModel::activateWorkspace()` → Hyprland `dispatch workspace N`) shall not be modified. All new features (edge arrow clicks, special-workspace dot clicks) shall reuse the same dispatch path.

**Rationale:** Minimizes risk of regression; leverages proven activation logic.

**Acceptance Criteria:**
1. `WorkspaceModel::activateWorkspace(int id)` is called unchanged for numbered-pill clicks.
2. Special-workspace dot clicks invoke `WorkspaceModel::activateWorkspace(QString name)` (or a parallel overload) using the same underlying dispatch mechanism.
3. Edge arrow clicks for urgent activation invoke the workspace activation path; pan-without-switch edge arrow clicks do NOT invoke activation (they only modify `window_start`).

---

## 5. Non-Goals (Out of Scope)

The following are explicitly **not** addressed by this specification:

1. **Per-monitor special-workspace ownership:** Special-workspace state (active/urgent) is global across all monitors. Scoping special workspaces to specific monitors is deferred.
2. **New configuration keys:** No new settings UI or config file entries beyond the existing `barWorkspaceCount`. Dynamic sizing, panel behavior, or animation durations are not user-configurable.
3. **Changes to workspace activation/dispatch:** The mechanism by which workspaces are switched (e.g., `hyprctl dispatch workspace N`) is unchanged.
4. **Occupied/Empty pill styling changes:** Except for the FocusedInactiveMonitor restyle (REQ-F-008) and urgent-color unification (REQ-F-007), pill styling for `Empty`, `Occupied`, and `FocusedActiveMonitor` states remains unchanged.
5. **Accessibility / announcements:** Screen-reader announcements for dot state changes are not mandated (though welcome as an enhancement).
6. **Animation tuning beyond spec ranges:** Easing curves and durations are specified as ranges (e.g., 150–250 ms); exact values are implementation discretion.

---

## 6. Glossary

| Term | Definition |
|------|-----------|
| **Window** | The visible range of workspace IDs, `[window_start, window_start + N - 1]`, displayed as N numbered pills on a monitor's topbar. |
| **Pan** | User-initiated scroll via edge arrow click that changes `window_start` without changing the active workspace (peek without switch). |
| **Pan-reset** | Automatic re-centering of the window on the active workspace, discarding any active pan. Triggered by active-workspace changes on this monitor. |
| **Special workspace** | Wayland `ext_workspace_handle_v1` with a non-numeric `name` (e.g., `"special:scratch"`). |
| **Indicator** | Visual element representing a workspace (pill for numbered, dot for special). |
| **Urgent state** | Workspace flagged as needing attention (e.g., unread notification in application on that workspace), set by the compositor via `ext_workspace_handle_v1` state bits. |
| **Global focus** | The compositor-wide currently-focused/active workspace (may differ per monitor). |
| **Per-monitor focus** | The currently-active workspace on a specific monitor. |

---

## 7. References & Related Documents

- **Current Implementation:** `apps/shell/qml/Topbar/WorkspaceSection.qml`, `WorkspacePill.qml`
- **Model & Backend:** `libs/holonight-core/src/WorkspaceModel.{h,cpp}`, `ExtWorkspaceManager.cpp`
- **Wayland Protocol:** `protocols/ext-workspace-unstable-v1.xml` (or similar)
- **Design System:** `assets/dont-commit/` (HoloNight palette reference)
- **Related Architecture:** `libs/holonight-surfaces/src/PerMonitorLayerManager.cpp` (per-monitor resolution precedent)
- **Theme Palette:** `HoloniightPalette` singleton from `holonight-qt` project

---

## 8. Version & Sign-Off

| Field | Value |
|-------|-------|
| **Spec Version** | 1.0 |
| **Date** | 2026-07-01 |
| **Author** | Specification (EARS format) |
| **Status** | Draft — Ready for architecture review and implementation planning |

**Approvals:**
- [ ] Architecture / Lead Designer
- [ ] QML Component Owner
- [ ] Core C++ Backend Owner
- [ ] QA / Verification Lead
