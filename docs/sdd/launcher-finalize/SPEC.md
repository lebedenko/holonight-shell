# Launcher Finalize Feature Specification

## Overview

The Launcher Finalize feature completes the holonight-shell application launcher UI/UX. The launcher backend (DesktopEntryScanner, LauncherService, LauncherModel) and core QML UI (Launcher.qml, LauncherResultRow.qml, LauncherSearchField.qml) are already working; this feature implements the remaining presentation polish, surface lifecycle management, and interaction refinements. The launcher is a full-screen modal overlay (layer-shell surface, id "launcher") triggered via the `toggle-launcher` control socket command, which searches desktop entries by name/keywords and launches applications on Enter or click.

**Design context**: Single-column layout (no categories panel, pinned/recent sections, or bottom action bar). Surface stays mapped and reuses its layer-shell surface across toggle cycles (keep-alive pattern). Animation: scale+fade 95%→100% on open, reversed on close.

---

## Functional Requirements

### Surface Lifecycle & Visibility

**REQ-F-001: Keep-Alive Surface Lifecycle**  
The <launcher surface> shall remain mapped after the first show and persist across hide/show toggle cycles. The QML root's `visible` property shall toggle independently of the Wayland surface lifecycle.  
Acceptance: Call `toggle-launcher` twice (open, close, open); the wl_surface is created once on the first show; re-opening does not recreate the surface or layer-shell role (verify via Wayland protocol trace or that surface state persists).

**REQ-F-002: Query Reset on Open**  
When <the launcher surface transitions from hidden to visible>, the <system> shall programmatically reset the search query to an empty string before revealing the surface to the user.  
Acceptance: Search for "firefox", close launcher, reopen launcher; the search field is empty and shows the placeholder "Type to launch an application".

**REQ-F-003: Search Focus on Open**  
When <the launcher surface becomes visible>, the <system> shall automatically focus the search input field (`LauncherSearchField`) so that keyboard input is immediately captured without a click.  
Acceptance: Close launcher, press Ctrl+Space (or the launcher hotkey) to open; start typing without clicking in the search field; text appears in the input.

**REQ-F-004: Visibility Toggle Without Surface Destruction**  
The <launcher surface> shall never call `QQuickView::hide()`, `QQuickView::show()`, `QQuickView::deleteLater()`, or `LayerSurface::deleteLater()` during a hide operation. The only mechanism for hiding shall be toggling the root QML item's `visible` property to `false`.  
Acceptance: Inspect `LauncherSurface::hide()` and the QML close callback path; neither path calls `view_->hide()`, `view_->deleteLater()`, `surface_->deleteLater()`, or `view_->show()`. `destroySurface()` may retain teardown calls, but it is only used by the destructor/error cleanup path.

**REQ-F-005: Close Animation Sequencing**  
When <the user triggers a close action (Esc key, click outside, or app launch)>, the <system> shall animate the panel out (scale+fade to 95%/opacity 0) and only set the root QML `visible` to `false` after the animation completes (via `onCompleted` signal).  
Acceptance: Open launcher, press Esc; the panel fades and scales down smoothly over ~150ms; the surface becomes not-interactive only after the animation finishes (no flickering or immediate dismissal).

### Open Animation

**REQ-F-006: Scale+Fade Open Animation**  
When <the launcher surface becomes visible>, the <root panel> shall animate from scale 95% and opacity 0 to scale 100% and opacity 1 over approximately 150 milliseconds using an easing curve appropriate for ease-out motion.  
Acceptance: Open launcher (via toggle-launcher command); the panel smoothly grows from smaller size and fades in from transparent; animation duration is 140–160ms (measure via frame capture or log timestamps).

**REQ-F-007: Animation Easing Curve**  
The <open and close animations> shall use `Easing.OutCubic` or similar ease-out easing to give a polished, responsive feel.  
Acceptance: Visually inspect the animation; motion feels fast-out, not linear or ease-in (no sluggish start).

### Close Animation

**REQ-F-008: Scale+Fade Close Animation**  
When <the launcher closes>, the <root panel> shall animate from scale 100% and opacity 1 to scale 95% and opacity 0 over approximately 150 milliseconds using an ease-in curve, before the surface becomes hidden.  
Acceptance: Open launcher, press Esc; panel shrinks and fades over ~150ms; after animation, the surface is not visible (no lingering artifacts).

### Best-Match Row Visual Distinction

**REQ-F-009: Best-Match Row Index 0 Highlight**  
If <the search query is non-empty and at least one result exists>, then the <result at index 0 (first result)> shall receive distinct visual treatment compared to subsequent result rows. The distinction shall include one or more of: increased height, increased text size, brighter or accent-colored text, enhanced shadow, or background color change.  
Acceptance: Open launcher, search for "firefox"; the first result row is visibly taller or brighter than the second result row (if one exists); on an empty query or zero results, no row has this treatment.

**REQ-F-010: Best-Match Row Height Increase**  
The <best-match row (index 0)> shall have a height of 72 pixels (compared to standard rows at 64 pixels), creating a 8-pixel visual prominence.  
Acceptance: Render result 0 and result 1; measure or visually confirm result 0 is taller by approximately 8 pixels.

**REQ-F-011: Best-Match Label Color Accent**  
The <app name text in the best-match row> shall use `HoloniightPalette.accentViolet` or `HoloniightPalette.accentCyan` instead of the standard `HoloniightPalette.onSurface` color.  
Acceptance: Open launcher and search; the first result's app name is visibly tinted violet or cyan, while subsequent results use the default neutral text color.

### Clear Button Styling

**REQ-F-012: Clear Button Styled Glyph**  
The <clear button in LauncherSearchField> shall display a styled glyph (e.g., multiplication sign "×", or a circle-with-slash symbol) instead of the plain text "x". The glyph shall be styled with a HoloniightPalette color (e.g., `accentViolet` or `accentCyan`).  
Acceptance: Open launcher with any text in the search field; the clear button displays a proper symbol (not plain "x"); the symbol is colored with a theme accent color, not the default text color.

**REQ-F-013: Clear Button Hover State**  
When <the user hovers over the clear button>, the <system> shall highlight the glyph (e.g., increase opacity, change color to a brighter variant, or add a small scale animation) to indicate interactivity.  
Acceptance: Move the mouse over the clear button; the glyph visually responds (brighter, larger, or highlighted) to signal it is clickable.

**REQ-F-014: Clear Button Click Behavior**  
When <the user clicks the clear button>, the <system> shall immediately clear the search input text and reset the results list to zero matches (if any query was present).  
Acceptance: Type "firefox" in the search field (results appear), click the clear button; the input becomes empty and results disappear (placeholder text reappears).

### Empty State

**REQ-F-015: Placeholder on Empty Query**  
When <the search query is empty>, the <system> shall display a placeholder message centered in the results area that reads "Type to launch an application" (or similar) in subtle text color.  
Acceptance: Open launcher with an empty query; the placeholder text is visible and centered; it uses `HoloniightPalette.textSubtle` or equivalent subtle color.

**REQ-F-016: Placeholder Visibility Control**  
The <placeholder message> shall be visible only when the query length is exactly zero. It shall be hidden the moment any character is typed, even if no results match.  
Acceptance: Type "zzzzzzzzzzz" (nonsense query with zero matches); the placeholder is hidden; the "No matches" message appears instead.

### No-Results State

**REQ-F-017: No-Matches Message**  
When <the search query is non-empty AND zero results are returned>, the <system> shall display a message reading "No matches" centered in the results area in subtle text color.  
Acceptance: Type "zzzzzzzzzzzzzzzzzzz" in the search field; results list is empty; the "No matches" message appears centered.

**REQ-F-018: No-Matches Visibility Control**  
The <"No matches" message> shall appear only when the query is non-empty AND `LauncherService.resultCount === 0`. It shall be hidden when results exist or when the query is empty.  
Acceptance: Type a valid app name (e.g., "firefox") and see results; the "No matches" message is hidden. Clear the query; the "No matches" message remains hidden (placeholder appears instead).

### Best-Match Label

**REQ-F-019: Best-Match Label Display**  
When <the search query is non-empty AND at least one result exists>, the <system> shall display a label reading "BEST MATCH" above the first result row in a distinct color (e.g., `HoloniightPalette.accentViolet`) and smaller font (12pt, medium weight).  
Acceptance: Open launcher and search for "fire"; above the first result (firefox), the text "BEST MATCH" appears in violet, styled smaller and bolder than result text.

**REQ-F-020: Best-Match Label Visibility Control**  
The <"BEST MATCH" label> shall be visible only when the query is non-empty AND `LauncherService.resultCount > 0`. It shall be hidden on empty query or zero results.  
Acceptance: Type a search (label appears), clear search (label disappears), type a nonsense query (label disappears because resultCount is 0).

### Keyboard Navigation

**REQ-F-021: Arrow Key Selection**  
When <the user presses the Up or Down arrow key>, the <system> shall move the selection (highlighted row) one result up or down, respectively, clamping at the list boundaries (no wrap-around).  
Acceptance: Open launcher, type "text" (assume 5+ matches); press Down arrow; selectedIndex increases to 1 (second result highlights). Press Down repeatedly; selection reaches the last result and does not wrap to index 0.

**REQ-F-022: Selection Boundary Clamp**  
When <the selection index reaches the first result (index 0) and the user presses Up>, the <system> shall not move the selection (it remains at index 0). Similarly, when <selection is at the last result and the user presses Down>, the selection shall not move.  
Acceptance: Navigate to the first result and press Up; no change in visual selection. Navigate to the last result and press Down; no change.

**REQ-F-023: Launch on Enter**  
When <the user presses Enter (or Return)>, the <system> shall launch the currently-selected application (via `LauncherService.launchSelected()`).  
Acceptance: Open launcher, search for an app, press Enter; the app launches (verify via running process list), and the launcher closes.

**REQ-F-024: Close on Escape**  
When <the user presses Escape>, the <system> shall initiate the close animation and hide the launcher surface.  
Acceptance: Open launcher, press Esc; launcher fades out and closes (root `visible` becomes `false`).

### Mouse Interaction

**REQ-F-025: Click Outside to Close**  
When <the user clicks anywhere outside the launcher panel (in the transparent overlay area)>, the <system> shall close the launcher with the close animation.  
Acceptance: Open launcher, click in a darkened area away from the panel; launcher animates closed and becomes hidden.

**REQ-F-026: Row Hover Selection**  
When <the user hovers the mouse over a result row>, the <system> shall update the selection to that row's index (without requiring a click).  
Acceptance: Open launcher with results; move mouse over the third result row; the third row becomes highlighted as if selected via Up/Down arrows.

**REQ-F-027: Row Click Launch**  
When <the user clicks a result row>, the <system> shall launch the corresponding application (via `LauncherService.launch(index)`).  
Acceptance: Open launcher, click any result row; the app launches and the launcher closes.

### Launch & Close Sequencing

**REQ-F-028: Launched Signal Closes Surface**  
When <`LauncherService.launched()` signal fires>, the <system> shall trigger the close animation and set the root QML `visible` to `false` after animation completes.  
Acceptance: Open launcher, search for and launch an app; the launcher closes with animation (does not instantly vanish).

**REQ-F-029: Launch Success Verification**  
When <an app is launched from the launcher>, the <system> shall verify that `QProcess::startDetached()` succeeds (returns `true`) and logs a diagnostic message if the launch fails.  
Acceptance: Attempt to launch a valid desktop entry; the application starts. Attempt to launch a malformed entry (if possible in test); a warning is logged and launcher closes gracefully (no crash).

### Layout & Sizing

**REQ-F-030: Panel Centering**  
The <launcher panel> shall be centered both horizontally and vertically within the full-screen layer-shell overlay.  
Acceptance: Open launcher; the panel appears in the center of the screen, equidistant from left and right edges, and from top and bottom edges.

**REQ-F-031: Responsive Panel Width**  
The <launcher panel width> shall be `Math.min(900, rootWidth - 32)`, ensuring the panel is no wider than 900 pixels and has at least 16 pixels of margin on each side.  
Acceptance: On a 1920px-wide display, panel is 900px wide. On a 800px-wide display, panel is 768px wide (800 - 32). Panel never exceeds screen width.

**REQ-F-032: Responsive Panel Height**  
The <launcher panel height> shall be `Math.min(520, rootHeight - 96)`, ensuring the panel is no taller than 520 pixels and has at least 48 pixels of margin on top and bottom.  
Acceptance: On a 1080px-tall display, panel is 520px tall. On a 600px-tall display, panel is 504px tall (600 - 96). Panel never exceeds screen height.

**REQ-F-033: Result List Scrolling**  
The <results ListView> shall be scrollable if results exceed the available vertical space in the panel.  
Acceptance: Search for a very common term (e.g., "application") that returns 20+ results; the list scrolls smoothly with the mouse wheel or drag.

### Theme Integration

**REQ-F-034: Panel Frame Styling**  
The <launcher panel> shall use `HudFrame` with variant `Popup`, `frameStroke: HoloniightPalette.borderActive`, `innerGlowOpacity: 0.06`, and corner cuts (14px `leftCornerCut` and `rightCornerCut`) for a consistent holonight aesthetic.  
Acceptance: Open launcher; the panel has a glowing cyan border, rounded corners with subtle cuts, and a faint inner glow; it matches other popup surfaces in the shell.

**REQ-F-035: Shadow Effect**  
The <launcher panel> shall be wrapped in a `MultiEffect` with shadow enabled (color `accentCyan`, blur 0.62, opacity 0.18, scale 1.015) to create depth and visual hierarchy.  
Acceptance: Open launcher; the panel casts a subtle cyan shadow, giving it a floating appearance above the background.

**REQ-F-036: Search Field Frame**  
The <search field (`LauncherSearchField`)> shall use a `HudFrame` widget that changes border color from `borderPassive` to `borderActive` when the input has focus.  
Acceptance: Open launcher; the search field has a subtle passive border; click in the search field; the border brightens to active color.

**REQ-F-037: Color Palette Compliance**  
All <text colors and accent colors> used in the launcher (e.g., "BEST MATCH" label, clear button glyph, selected row highlight) shall come from `HoloniightPalette` properties (e.g., `accentViolet`, `accentCyan`, `textSubtle`, `onSurface`). No hardcoded hex color values shall appear in QML source.  
Acceptance: Inspect Launcher.qml and related files; all color references use `HoloniightPalette.*`; no `color: "#..."` literals.

---

## Non-Functional Requirements

**REQ-NF-001: Animation Performance**  
The <open and close animations> shall run at 60 FPS without frame drops on a mid-range GPU (e.g., Intel iGPU).  
Acceptance: Run the launcher with a framerate monitor (e.g., `QSG_RENDERER=software` off, running on native hardware); animations maintain 60 FPS (zero dropped frames during scale+fade).

**REQ-NF-002: Input Latency**  
Keyboard input (arrow keys, Enter, Escape) and mouse interaction (hover, click) shall respond within 16ms (one frame at 60 FPS) of user action.  
Acceptance: Type rapidly in the search field; text appears immediately without lag. Click a result row; the app launches without perceptible delay.

**REQ-NF-003: Search Responsiveness**  
When <the user types a character>, the <search results> shall update within 100ms.  
Acceptance: Type "f" in the search field; results filtering happens nearly instantly (< 100ms latency measured between keystroke and visible results update).

**REQ-NF-004: Surface Persistence**  
The <launcher surface> shall not be recreated between toggle cycles, ensuring zero delay or visual artifacts (e.g., black flash) when reopening.  
Acceptance: Rapidly toggle launcher open/close 5 times; no black flashes, no delay, no wl_surface recreation (verified via protocol trace if available).

---

## Constraints & Out-of-Scope

**REQ-C-001: No Categories Panel**  
The launcher shall not display a category filter sidebar or column. All search results are displayed in a single column.

**REQ-C-002: No Pinned or Recent Section**  
The launcher shall not include pinned applications, recently-launched apps, or any persistent history/cache section.

**REQ-C-003: No Bottom Action Bar**  
The launcher shall not display an action bar or footer with settings, help, or other meta controls.

**REQ-C-004: No Ctrl+Enter Terminal Launch**  
The launcher shall not support special key combinations (e.g., Ctrl+Enter) to launch commands in a terminal or execute arbitrary shell commands.

**REQ-C-005: Single Monitor Scope**  
The launcher operates on the focused monitor (where the `toggle-launcher` command originated) and does not support multi-monitor app distribution.

---

## Acceptance Criteria Summary

| Feature | Acceptance Test |
|---------|-----------------|
| Keep-alive surface | Surface created once; hide/show toggles only QML `visible` |
| Query reset on open | Search cleared, placeholder shown on reopen |
| Focus on open | Keyboard input captured immediately (no click needed) |
| No surface destruction | `destroySurface()` never calls `hide()`, `show()`, or `deleteLater()` |
| Close animation | Panel fades+shrinks over ~150ms before `visible` becomes `false` |
| Open animation | Panel grows+fades-in from 95%/opacity-0 over ~150ms |
| Best-match row (height) | Index 0 is 72px tall (vs 64px standard) |
| Best-match row (color) | Index 0's app name text is accent-colored |
| Clear button | Displays styled glyph, not plain "x"; hover highlights it |
| Empty placeholder | Visible when query length is 0 |
| No-matches message | Visible when query length > 0 AND resultCount is 0 |
| Best-match label | "BEST MATCH" appears when query length > 0 AND resultCount > 0 |
| Arrow key navigation | Up/Down move selection; clamped at boundaries (no wrap) |
| Enter launches | Pressing Enter launches `selectedIndex` app |
| Esc closes | Pressing Esc closes launcher with animation |
| Click outside closes | Clicking overlay area closes launcher |
| Row hover selection | Moving mouse over row updates selection |
| Row click launches | Clicking row launches corresponding app |
| Launched signal closes | `LauncherService.launched()` triggers close animation |
| Panel centering | Panel centered horizontally and vertically |
| Responsive sizing | Panel width/height respects min/max bounds |
| Result list scrolling | ListView scrollable if results exceed space |
| Color palette | All colors from `HoloniightPalette`; no hardcoded hex |
| Animation performance | 60 FPS on mid-range GPU |
| Input latency | Keyboard/mouse response < 16ms |
| Search responsiveness | Results update within 100ms of keystroke |

---

## Implementation Notes

### LauncherSurface Redesign
- **Current issue**: `hide()` calls `view_->hide()`, destroying the wl_surface and layer-shell role.
- **Fix**: Replace `destroySurface()` implementation to NOT call `view_->hide()` or `view_->deleteLater()`. Instead, use a signal to the QML root to set `visible = false` for animation sequencing.
- **Keep-alive flow**: First `show()` creates surface and view; subsequent `hide()` only sets root `visible = false`; `show()` again just sets root `visible = true` without recreating.

### QML Animation Sequencing
- Add `Behavior on opacity` and `Behavior on scale` to the root panel, OR use explicit `SequentialAnimation` triggered by `visible` property change.
- On `visible: true`, animate from (scale: 0.95, opacity: 0) to (1.0, 1.0) over 150ms with `Easing.OutCubic`.
- On `visible: false`, animate out, then in QML or C++, ensure the surface does not reappear (keep root `visible: false` until next `show()` call).

### Best-Match Row Styling
- Wrap index 0 row in a component that receives a `isBestMatch` property.
- Increase height from 64 to 72 in the best-match variant.
- Apply accent color to the app name text when `isBestMatch` is true.
- Use `ListView.isCurrentItem` or compare `LauncherService.selectedIndex === 0` to trigger best-match styling (independent of hover selection).

### Clear Button Glyph
- Replace `text: "x"` with a Unicode glyph (e.g., `text: "×"` or `text: "✕"` for a multiplication sign or heavy ballot X).
- Style with `color: clearArea.containsMouse ? HoloniightPalette.accentViolet : HoloniightPalette.textSubtle`.
- Optional: Add a small scale or opacity animation on hover via `Behavior on scale` or explicit `NumberAnimation` in `onEntered`.

---

## Testing Strategy

1. **Manual UI smoke tests**: Open/close launcher, search, navigate, launch apps, verify animations.
2. **Keyboard interaction tests**: Arrow keys, Enter, Escape on various result counts.
3. **Mouse interaction tests**: Hover selection, click to launch, click outside to close.
4. **Animation frame-rate tests**: Monitor for dropped frames at 60 FPS during open/close.
5. **Surface persistence test**: Rapid toggle cycles; verify no wl_surface recreation via Wayland logs or process inspection.
6. **Edge cases**: Empty results, single result, many results, special characters in query, very long app names (elision).
7. **Theme compliance check**: Run with debug overlays and color-verification tools to ensure all colors come from palette.
