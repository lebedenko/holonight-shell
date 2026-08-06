# SPEC — osd-overlay

## Overview

The `osd-overlay` feature provides a generic on-screen display (OSD) overlay that observes three system state channels — volume, brightness, and keyboard layout — and renders transient notifications to the user when those states change. Rather than polling or responding to keypresses, the OSD observes service-level change signals (`AudioService`, `BrightnessService`, `KeyboardLayoutService`), applies a prime-and-diff filter to suppress redundant displays, gates the display based on startup timing and surface visibility, and renders a shared composited surface with channel-specific renderers (level-bars for volume and brightness, selection text for keyboard layout). The surface is non-interactive, creates on demand per monitor, and fades away after a configurable timeout.

---

## Non-goals

- **Microphone mute, Caps Lock, Num Lock, touchpad state, airplane mode, keyboard backlight, power profile** — deferred; the generic event model makes these additions cheap in future phases.
- **Per-monitor brightness control or DDC-CI** — `BrightnessService` is single-value sysfs only; cannot be decomposed per display.
- **Over-100% volume amplification display** — `PulseAudioBackend` clamps percent to 0..100 on read, so amplified volumes are not observable by this shell.
- **Screen-capture exclusion** — wlr-layer-shell offers no per-surface capture opt-out; `grim` will always capture the OSD.
- **Queueing, history, notification-style stacking, and user interaction** — the OSD has no close button, no history, and no modal controls; a new event on any channel replaces the current display.
- **Configurable keyboard-layout short-label override map** — the existing `KeyboardLayoutService::layoutCode()` derivation (e.g. "EN", "DE") is accepted for v1 despite the known US/UK English collision.
- **Screen-reader announcement of OSD content** — deferred.

---

## Terminology

- **Channel** — one of three monitored state streams: `audio-volume` (level), `screen-brightness` (level), `keyboard-layout` (selection).
- **Event** — a notification that a channel has changed state, carrying normalized data (level value, muted flag, or selection label).
- **Level kind** — an event carrying a numeric value (0–100 percent), a muted flag, and an icon. Renders a progress bar, percentage text, and icon.
- **Selection kind** — an event carrying a short label (e.g. "EN") and a full label (e.g. "English (US)"). Renders large text, no progress bar.
- **Prime** — the initial value observed for a channel after its service becomes available. Priming is silent (no OSD displayed).
- **Grace period** — a 2000 ms window after shell startup during which no OSD is displayed, regardless of observed changes. Covers late-arriving service values and PulseAudio connection storms.
- **Suppression** — conditional hiding of the OSD when a shell surface displaying that channel's value is already open (e.g., audio popup suppresses volume OSD, sidebar+QuickSettings suppresses brightness OSD).
- **Update in place** — when an event arrives on the currently displayed channel, animate the value to the new state without replaying entrance animation or hide-then-show cycle; restart the hide timer.
- **Replace** — when an event arrives on a different channel than the one currently displayed, immediately swap the content (no hide-then-show).

---

## Requirements

### Functional

#### REQ-F-001: OsdController shall define normalized event types
**Requirement:** The system shall define two event types within the `OsdController` namespace: `OsdLevelEvent` (carries channel name, value 0–100, muted flag) and `OsdSelectionEvent` (carries channel name, short label, full label).

**Acceptance Criterion:**
- Both event types are copyable and assignable
- `OsdLevelEvent` compiles with members: `channel` (QString), `value` (int), `muted` (bool)
- `OsdSelectionEvent` compiles with members: `channel` (QString), `shortLabel` (QString), `fullLabel` (QString)
- A unit test instantiates both types and assigns values without error

---

#### REQ-F-002: OsdController shall expose abstract channel source interface
**Requirement:** The system shall provide an `OsdChannelSource` base class — a `QObject` declaring the signals `eventObserved(OsdEvent)` and `availableChanged(bool)`, plus a pure virtual `channel()` accessor — that concrete adapters implement to translate each service (`AudioService`, `BrightnessService`, `KeyboardLayoutService`) into the normalized event model. The controller knows only this base class.

**Acceptance Criterion:**
- `OsdChannelSource` declares `channel()` as pure virtual and both signals in a `Q_SIGNALS:` section (signals are emitted by the base, implemented by moc, not overridden)
- A test-only `FakeChannelSource` deriving from it compiles and can emit both signals on demand
- The controller accepts a list of `OsdChannelSource*` at construction and connects to the base-class signals only
- `grep` over `OsdController.cpp` finds no reference to `AudioService`, `BrightnessService`, or `KeyboardLayoutService`

---

#### REQ-F-003: OsdController shall implement prime-and-diff on each channel
**Requirement:** When a channel source emits its first value after becoming available, the controller shall record that value internally and emit no OSD event (silent prime). On subsequent values, if the new value differs from the cached value, emit an OSD event; if equal, emit nothing.

**Acceptance Criterion:**
- A unit test instantiates controller with a mock source, calls `onEventObserved(value=50)`, and verifies no signal emitted
- Calling `onEventObserved(value=51)` emits an OSD event with `value=51`
- Calling `onEventObserved(value=51)` again emits no signal
- Calling `onEventObserved(value=50)` emits a signal with `value=50`

---

#### REQ-F-004: OsdController shall gate display for 2000 ms after startup
**Requirement:** The controller shall maintain an internal startup grace-period timer initialized to 2000 ms at construction. If an event would be displayed during this window, then the controller shall **discard** it — updating the per-channel cached value but emitting no signal. Grace-period events are never queued and never flushed later.

**Acceptance Criterion:**
- A unit test constructs the controller, immediately emits three events, and verifies no signals emitted
- Advancing the injected clock past 2000 ms emits nothing retroactively (zero signals total)
- After the window elapses, an event whose value differs from the discarded one emits normally
- After the window elapses, an event whose value equals the last discarded value emits nothing (proving the cache was updated during grace, not bypassed)

---

#### REQ-F-005: OsdController shall support per-channel suppression
**Requirement:** The controller shall expose a public `setSuppressed(const QString &channel, bool suppressed)` method. When `suppressed` is true for a channel, events on that channel do not emit signals; when false, suppression is cleared and the next distinct event emits normally.

**Acceptance Criterion:**
- A unit test calls `setSuppressed("audio-volume", true)`, emits a volume event, and verifies no signal
- Calling `setSuppressed("audio-volume", false)` then emitting a different volume value emits a signal
- Suppressing one channel does not affect others
- A `task test` unit test covers suppression for all three channels

---

#### REQ-F-006: OsdController shall route suppression API to the app layer
**Requirement:** Because `OsdController` lives in `holonight-services` (forbidden to query surface visibility), the app layer (`ShellApplication`, in `apps/shell/`) shall wire surface visibility signals into `OsdController::setSuppressed()`. Specifically, when the audio status popup opens, call `setSuppressed("audio-volume", true)`; when it closes, call `setSuppressed("audio-volume", false)`. Similarly for brightness + sidebar QuickSettings and keyboard layout (not suppressed, as no surface displays it).

**Acceptance Criterion:**
- `ShellApplication` connects the audio status popup's open/close notification to `setSuppressed("audio-volume", …)`; the exact signal is chosen at design time from what `StatusPopupSurface` already exposes, adding one if none exists
- `ShellApplication` connects right-sidebar open state combined with QuickSettings tab selection to `setSuppressed("screen-brightness", …)`
- `setSuppressed` is never called for `"keyboard-layout"`
- `task architecture-check` passes, and `grep` over `libs/holonight-services/src/osd/` finds no include of any header under `libs/holonight-surfaces/`

---

#### REQ-F-007: OsdController shall support per-channel enable/disable
**Requirement:** The controller shall expose a public `setChannelEnabled(const QString &channel, bool enabled)` method. When `enabled` is false for a channel, events on that channel are ignored and no signal is emitted; when enabled is true, events are processed normally.

**Acceptance Criterion:**
- A unit test disables the brightness channel, emits a brightness event, and verifies no signal
- Calling `setChannelEnabled("brightness", true)` then emitting a brightness event emits a signal
- Enabling/disabling does not affect other channels
- Config loading (REQ-F-041) calls this method for each channel according to config values

---

#### REQ-F-008: OsdController shall emit events with channel-specific renderers
**Requirement:** The controller shall expose two signals: `displayLevelEvent(OsdLevelEvent)` and `displaySelectionEvent(OsdSelectionEvent)`. The view connects to both and dispatches to the appropriate QML renderer based on signal.

**Acceptance Criterion:**
- A unit test emits a level-kind event (volume) and verifies `displayLevelEvent()` signal fires with correct values
- Emitting a selection-kind event (keyboard layout) fires `displaySelectionEvent()` signal
- Both signals propagate through the C++/QML binding without crashes
- Integration test wires both signals to QML `Connections` and verifies both fire

---

#### REQ-F-009: AudioChannelSource shall adapt AudioService to OsdLevelEvent
**Requirement:** The `AudioChannelSource` class shall connect to `AudioService::volumeChanged()` and `AudioService::mutedChanged()` signals, store the current values, and emit `onEventObserved()` with an `OsdLevelEvent` carrying `channel="audio-volume"`, the percent value, and the muted flag.

**Acceptance Criterion:**
- A unit test with mock `AudioService` sets volume to 50% (unmuted) and verifies `onEventObserved()` emits correctly
- Setting muted flag emits `onEventObserved()` with `muted=true`
- Volume changes from 50% to 75% emit a new event
- `AudioChannelSource::onSourceAvailableChanged()` fires when `AudioService::available` changes

---

#### REQ-F-010: BrightnessChannelSource shall adapt BrightnessService to OsdLevelEvent
**Requirement:** The `BrightnessChannelSource` class shall connect to `BrightnessService::brightnessPercentChanged()` signal, store the current value, and emit `onEventObserved()` with an `OsdLevelEvent` carrying `channel="screen-brightness"`, the percent value, and `muted=false` (brightness has no mute state).

**Acceptance Criterion:**
- A unit test with mock `BrightnessService` sets brightness to 75% and verifies `onEventObserved()` emits with `value=75, muted=false`
- Changing brightness from 75% to 80% emits a new event
- `BrightnessChannelSource::onSourceAvailableChanged()` fires when `BrightnessService::hasBacklight` changes

---

#### REQ-F-011: KeyboardLayoutChannelSource shall adapt KeyboardLayoutService to OsdSelectionEvent
**Requirement:** The `KeyboardLayoutChannelSource` class shall observe `KeyboardLayoutService`'s layout change signal and emit an `OsdSelectionEvent` carrying `channel="keyboard-layout"`, `shortLabel` = the service's `layoutCode` (e.g. "EN"), and `fullLabel` = the service's `layoutName` (e.g. "English (US)") added by REQ-C-014.

**Acceptance Criterion:**
- A test double reporting code "EN" / name "English (US)" produces an event with exactly those two labels
- Changing to code "DE" / name "German" emits a new event with the updated pair
- If `layoutName` is empty, then `fullLabel` falls back to `layoutCode` and the renderer shows the short label alone rather than an empty second line
- The diff is on the layout **code**, so a name-only change with an unchanged code emits nothing

---

#### REQ-F-012: OsdSurface shall create on demand per monitor
**Requirement:** The `OsdSurface` class (in `libs/holonight-surfaces/src/`) shall inherit the layer-shell surface pattern from `NotificationToastSurface`, creating a new `QQuickView` and layer-shell role only when `show()` is called with a monitor name, and reusing the same surface if `show()` is called again before the surface is destroyed.

**Acceptance Criterion:**
- A unit test calls `osdSurface->ensureSurface("DP-1")` and verifies a `QQuickView` is created
- Calling `ensureSurface("DP-1")` again reuses the existing surface (no new view created)
- Calling `ensureSurface("DP-2")` while the DP-1 surface is alive destroys the DP-1 surface and rebuilds on DP-2 — at most one OSD surface exists at any time, matching `NotificationToastSurface::ensureSurface()`
- If the layer-shell global is not yet active, then creation is deferred and replayed once `LayerShell::activeChanged` reports active
- Calling `hide()` schedules surface destruction after the fade-out completes

---

#### REQ-F-013: OsdSurface shall use transient layer-shell role
**Requirement:** The `OsdSurface` shall set the layer-shell role to `zwlr_layer_shell_v1_layer_overlay`, allowing the surface to float above normal windows and popups.

**Acceptance Criterion:**
- A compositor trace (via `wlr-layer-shell` debug) shows the OSD surface created in the overlay layer
- The OSD appears above taskbars, notifications, and regular windows in live testing
- The surface is not clipped by panel geometry

---

#### REQ-F-014: OsdView shall render level-kind events
**Requirement:** The QML component `OsdLevelRenderer.qml` (in `apps/shell/qml/Osd/`) shall accept properties `channel` (QString), `value` (int 0–100), `muted` (bool), and render: an icon (mapped by channel and muted state), a label (channel name), a progress bar showing the value, and percentage text.

**Acceptance Criterion:**
- A QML-test instantiates the renderer with `channel="audio-volume", value=50, muted=false` and verifies the icon is `"audio-volume-medium"`
- Setting `value=75` updates the bar and text without replaying entrance animation
- Setting `muted=true` renders the bar at its real position but dimmed, and replaces percentage text with "Muted"
- For `channel="screen-brightness"`, the icon is `"brightness"`
- Colors are sourced from `HoloniightPalette` tokens only

---

#### REQ-F-015: OsdView shall render selection-kind events
**Requirement:** The QML component `OsdSelectionRenderer.qml` (in `apps/shell/qml/Osd/`) shall accept properties `shortLabel` (QString) and `fullLabel` (QString) and render: a large font short label and a smaller full label below it, no progress bar.

**Acceptance Criterion:**
- A QML-test instantiates the renderer with `shortLabel="EN", fullLabel="English (US)"` and verifies both labels are rendered
- Short label font size is >= 32pt, full label is smaller
- Updating `shortLabel` to "DE" smoothly transitions the text
- Colors and fonts are sourced from `HoloniightPalette` tokens

---

#### REQ-F-016: OsdView shall animate entrance and exit
**Requirement:** When an OSD event is received, the root QML item shall play an entrance animation (fade-in + scale, or similar, 200 ms) and position itself according to the configured widget position. When hide is triggered, exit animation (fade-out + scale, 150 ms) plays, then the surface is destroyed.

**Acceptance Criterion:**
- A manual test in live Hyprland shows the OSD fading and scaling in smoothly over ~200 ms
- Exiting plays a 150 ms fade-out; the surface is destroyed after fade completes
- No visual pop-in or stutter observed
- Colors and timing match the HoloNight design system (consult `assets/dont-commit/`)

---

#### REQ-F-017: OsdView shall update in place on same-channel events
**Requirement:** When a new event arrives on the same channel as the currently displayed OSD, update the content (progress bar value, text labels, icon if applicable) with smooth animation (100 ms), do NOT replay entrance animation, and restart the hide timer.

**Acceptance Criterion:**
- A manual test volume-up while OSD is visible; the bar slides to the new value without fade-in replay
- Timer is reset; the OSD persists for a full 1500 ms after the last change
- Rapid volume adjustments show smooth real-time bar animation
- Selection-kind events (keyboard layout) smoothly fade text to new labels and restart timer

---

#### REQ-F-018: OsdView shall replace on different-channel events
**Requirement:** When a new event arrives on a different channel than the one currently displayed, the OSD content is replaced immediately (no hide-then-show cycle). The exit animation does not play; the new content enters with a fresh entrance animation.

**Acceptance Criterion:**
- A manual test changes volume while OSD is visible, then quickly changes keyboard layout; the OSD immediately swaps to the layout renderer
- No fade-to-black or delay observed
- The new renderer's entrance animation plays
- Hide timer is reset for the new channel

---

#### REQ-F-019: OsdView shall hide after configurable timeout
**Requirement:** The OSD shall remain visible for a duration set by the `osd.timeout` config value (default 1500 ms). After the timeout, the exit animation plays and the surface is destroyed. The timer is restarted whenever an event updates the display.

**Acceptance Criterion:**
- A config-driven test with `timeout=500` shows OSD visible for ~500 ms then fading
- Manually adjusting volume triggers an event at 400 ms; OSD persists for a full 500 ms more (until 900 ms total)
- A zero-event OSD disappears exactly 1500 ms after first display (with 100 ms tolerance)

---

#### REQ-F-020: Volume icon shall map to muted and level tiers
**Requirement:** The audio-volume channel shall select icons according to: `audio-volume-muted` when muted, otherwise `audio-volume-high` (value >= 67), `audio-volume-medium` (value >= 34), `audio-volume-low` (value < 34). Exactly these four icon names, mapped via `BarIcon` dispatcher.

**Acceptance Criterion:**
- A QML-test with level value 75% (unmuted) renders `audio-volume-high`
- Value 50% renders `audio-volume-medium`
- Value 20% renders `audio-volume-low`
- Muted flag overrides all: renders `audio-volume-muted` regardless of value
- No intermediate icons (e.g., `audio-volume-medium-high`) are used

---

#### REQ-F-021: Brightness icon shall use UtilityIcon
**Requirement:** The brightness channel shall render the icon `"brightness"` via `UtilityIcon`, which must be added to the icon set. The icon shall use palette stroke colors like other utility icons.

**Acceptance Criterion:**
- A code review verifies `UtilityIcon.qml` includes a case for `name == "brightness"` drawing a brightness icon symbol
- A QML-test renders the brightness OSD and verifies the icon appears
- The icon color matches `HoloniightPalette` tokens (not hardcoded)

---

#### REQ-F-022: Keyboard-layout icon shall use UtilityIcon
**Requirement:** The keyboard-layout channel shall render the icon `"keyboard"` via `UtilityIcon`, which already exists in the utility icon set.

**Acceptance Criterion:**
- A QML-test renders a keyboard-layout OSD and verifies the icon is rendered
- The icon color matches `HoloniightPalette` tokens

---

#### REQ-F-023: Muted rendering shall preserve bar position
**Requirement:** When rendering a muted audio level OSD, the progress bar shall display at the actual underlying volume position (e.g., 75%) but with reduced opacity or a disabled color from the palette. The percentage text is replaced with the string "Muted" in the same position. The bar does NOT collapse to 0%.

**Acceptance Criterion:**
- A unit test renders muted audio at 75% and verifies bar is drawn at 75% mark with dimmed color
- Text displays "Muted", not "75%"
- Unmuting (muted=false) re-shows the percentage text at the same bar position
- Manual test confirms visual appearance matches design spec

---

#### REQ-F-024: OsdView shall not take input focus
**Requirement:** The OSD shall never accept pointer or keyboard input. `OsdSurface` shall request `keyboard_interactivity = none` on the layer surface and commit an empty `wl_region` as the surface's input region. The QML view shall contain no `MouseArea`, `TapHandler`, `HoverHandler`, or other input handler, and no item shall set `activeFocusOnTab: true`.

**Acceptance Criterion:**
- `grep` over `apps/shell/qml/Osd/` finds zero input handlers and zero `focus: true` assignments
- Code review confirms `wl_surface_set_input_region()` is called with an empty region after each configure
- Manual check: clicking where the OSD is drawn activates the window behind it, and typing while an OSD is visible reaches the focused application unchanged

---

#### REQ-F-025: OsdController shall be unit-testable with injection seam
**Requirement:** The controller shall receive its channel sources at construction as a list of `OsdChannelSource*` (Qt parent-ownership, consistent with REQ-F-002 and this codebase's `QObject` conventions — not `shared_ptr`), and shall never construct or query a concrete service itself. Timing that the tests must control shall likewise be injectable rather than read from a real clock.

**Acceptance Criterion:**
- A unit test constructs the controller with fake sources and a fake time source, with no real service instantiated
- Grace-period behaviour is verified by advancing the injected time source, with no `sleep` and no wall-clock wait in the test
- All of prime-and-diff, suppression, per-channel enable/disable, and cross-channel replacement are driven purely by emitting signals on the fakes
- `task test` runs the OSD controller tests with no PulseAudio, sysfs, D-Bus, or Wayland connection available

---

### Non-Functional

#### REQ-NF-001: OSD shall not block service initialization
**Requirement:** The OSD controller initialization (connecting to service signals, constructing channel sources) shall perform no blocking I/O, and shall create no layer-shell surface until the first displayable event.

**Acceptance Criterion:**
- Code review of `OsdController`'s constructor and the three adapters finds no file, socket, D-Bus, or `QProcess` calls
- A unit test constructs the controller and asserts zero surface-show signals were emitted
- Launching the shell with all three channels enabled creates no OSD surface until a value actually changes (verified in a live session by the absence of an OSD at login)

---

#### REQ-NF-002: OSD shall respond to events within 100 ms
**Requirement:** From the moment a service emits a change signal (e.g., `AudioService::volumeChanged()`) to the moment the OSD surface becomes visible with updated content, latency shall be <= 100 ms.

**Acceptance Criterion:**
- The controller adds no deliberate delay: code review confirms the observe→emit path contains no timer, no `QTimer::singleShot`, and no debounce
- A unit test asserts the display signal is emitted synchronously within the slot handling a source event
- Manual check in a live session: holding volume-up produces continuous bar motion with no perceptible lag between keypress and OSD update

---

#### REQ-NF-003: OSD animations shall be declarative and interruptible
**Requirement:** All OSD animations (entrance, exit, value transitions) shall be expressed as QML `Behavior`/`NumberAnimation` on animated properties, so that a value change arriving mid-animation retargets the running animation rather than queueing or restarting it.

**Acceptance Criterion:**
- Code review of the renderers finds every animated property driven by a `Behavior`, with no imperative `start()`/`stop()` sequencing of value transitions
- A QML test sets `value` twice within one animation duration and asserts the final settled value equals the second assignment
- Manual check: rapid volume adjustment shows continuous bar motion with no visible restart or backtrack

---

#### REQ-NF-004: All QML colors and styling shall use HoloniightPalette
**Requirement:** The OSD renderers and surface shall not contain hardcoded hex color values; all styling shall be sourced from `HoloniightPalette` tokens.

**Acceptance Criterion:**
- Code review of `OsdLevelRenderer.qml` and `OsdSelectionRenderer.qml` finds zero hardcoded `#` hex values
- All colors access `HoloniightPalette.<token>` after `import Holonight`
- OSD appearance changes when the system palette is updated

---

#### REQ-NF-005: Code shall pass all linting and formatting checks
**Requirement:** All C++ and QML code for the OSD feature shall pass `task format-check`, `task qml-lint`, and `task tidy` without errors or warnings.

**Acceptance Criterion:**
- `task build` completes without compiler warnings
- `task qml-lint` reports zero warnings for all OSD QML files
- `task format-check` reports the code is correctly formatted
- `task tidy` reports zero issues in C++ OSD code

---

#### REQ-NF-006: OSD shall use standard Qt logging
**Requirement:** Diagnostic output (event emissions, suppression state, grace-period progress) shall use `qCInfo` and `qCWarning` categories, visible via `QT_LOGGING_RULES` environment variable without code modification.

**Acceptance Criterion:**
- Running with `QT_LOGGING_RULES="*osd*.debug=true"` shows event emissions and state changes
- Errors are logged at warning level with sufficient context
- No `qDebug` or `std::cout` is used in the implementation

---

#### REQ-NF-007: OsdSurface shall not leak memory
**Requirement:** When the OSD surface is destroyed, `OsdSurface` shall delete its `QQuickView` and `LayerSurface`, release the `wl_surface`, and reset every owning pointer member to `nullptr`, matching `NotificationToastSurface::destroySurface()`.

**Acceptance Criterion:**
- `isActive()` returns false after `destroySurface()`, and a subsequent `ensureSurface()` constructs a fresh view rather than reusing a dangling one
- The destructor calls `destroySurface()`, so shutdown with a visible OSD does not warn or crash
- A live-session check: triggering 20 consecutive OSDs leaves exactly zero OSD surfaces mapped afterwards

---

#### REQ-NF-008: OsdController shall emit signals safely across thread boundaries
**Requirement:** If any channel source is created on a non-GUI thread (e.g., a worker thread that queries services asynchronously), the controller shall use `Qt::QueuedConnection` for signal/slot connections to ensure Qt thread safety.

**Acceptance Criterion:**
- All internal signal emissions use queued connections where appropriate
- Unit tests with threading do not race or deadlock
- No `Q_ASSERT_X(qApp->thread() == QThread::currentThread(), ...)` violations

---

#### REQ-NF-009: OSD shall not render before the compositor has configured its surface
**Requirement:** The QML root shall stay invisible until the layer surface reports its first real configure, so the entrance animation never plays against the initial 1×1 configure geometry.

**Acceptance Criterion:**
- The root's visibility is gated on `LayerSurface::configured()`, not on construction
- Any single-shot handler attached to `configured()` is guarded by a current-state check, so a late-arriving configure event cannot re-hide an OSD that has since been shown (the documented `SingleShotConnection` race)
- Manual check: the OSD never appears as a sliver or mis-sized box on first trigger after shell start

---

#### REQ-NF-010: OSD shall not interfere with compositor performance
**Requirement:** While no OSD is displayed, the OSD subsystem shall hold no running timer, no mapped surface, and no scene graph.

**Acceptance Criterion:**
- A unit test asserts the hide timer is inactive both before the first event and after the hide completes
- `isActive()` on the surface is false whenever no OSD is displayed
- Code review confirms no polling timer exists anywhere in the controller, adapters, or surface — all inputs are signal-driven

---

### Constraints

#### REQ-C-001: OsdController shall reside in holonight-services library
**Requirement:** The `OsdController` and all channel-source adapter classes shall be defined in `libs/holonight-services/src/osd/` so they can access service instances (which are also in holonight-services).

**Acceptance Criterion:**
- CMake structure places `OsdController.cpp/h` and `*ChannelSource.cpp/h` in `libs/holonight-services/src/osd/`
- The library compiles with Qt6::Core, Qt6::Dbus, and no GUI dependencies
- `task architecture-check` confirms no forbidden `holonight-services` → `holonight-surfaces` includes

---

#### REQ-C-002: OsdSurface shall reside in holonight-surfaces library
**Requirement:** The `OsdSurface` class and related layer-shell logic shall be defined in `libs/holonight-surfaces/src/OsdSurface.*` following the `NotificationToastSurface` pattern.

**Acceptance Criterion:**
- `libs/holonight-surfaces/CMakeLists.txt` adds `OsdSurface.cpp` and `OsdSurface.h` to the target
- The surface is linkable by the shell app
- `task architecture-check` confirms proper boundaries

---

#### REQ-C-003: OsdView shall reside in apps/shell/qml/Osd/
**Requirement:** The QML components `OsdView.qml` (the view root), `OsdLevelRenderer.qml`, and `OsdSelectionRenderer.qml` shall be placed in `apps/shell/qml/Osd/`, reachable as `qrc:/HolonightShell/Osd/*.qml` and via `import HolonightShell`.

**Acceptance Criterion:**
- Directory `apps/shell/qml/Osd/` contains the three .qml files
- No hand-written registration is added to `apps/shell/CMakeLists.txt` for them — shell QML is collected by the existing `file(GLOB_RECURSE HOLONIGHT_QML_FILES ... qml/*.qml CONFIGURE_DEPENDS)`, which derives `QT_RESOURCE_ALIAS` automatically; adding a parallel `qt6_add_resources()` step would double-register them
- `task build` followed by loading the surface resolves `qrc:/HolonightShell/Osd/OsdView.qml` without a "no such file" warning
- `task qml-lint` reports zero warnings for all three files

---

#### REQ-C-004: OsdSurface shall use NotificationToastSurface pattern
**Requirement:** The `OsdSurface` implementation shall follow the same create-on-demand, layer-shell-role binding, and destruction patterns as `NotificationToastSurface`. Do not duplicate pattern code; refactor if needed.

**Acceptance Criterion:**
- `OsdSurface` owns a `LayerShell` member and a `LayerSurface*`, binding the role directly as `NotificationToastSurface` does — it does **not** use `PerMonitorLayerManager` (which serves the persistent per-monitor bar/sidebar surfaces, a different lifecycle)
- Surface creation is deferred behind `pending_show_`/`pending_screen_` while `LayerShell::isActive()` is false, and replayed on `activeChanged`
- `destroySurface()` is idempotent and is called from the destructor

---

#### REQ-C-005: WidgetPosition enum shall route OSD position
**Requirement:** The OSD `osd.position` config value shall be parsed via `widgetPositionFromString()` (which already exists in `holonight_config`) and interpreted using `WidgetPosition` enum values (CenterTop, BottomCenter, etc.).

**Acceptance Criterion:**
- Config value `"center-bottom"` parses to `WidgetPosition::CenterBottom`
- Unknown strings fall back to `WidgetPosition::CenterBottom` and log a warning
- All nine position strings are accepted without error

---

#### REQ-C-006: OSD positioning shall reuse WidgetSurfacePolicy logic
**Requirement:** The OSD shall derive its layer-shell anchor flags from the same position→anchor mapping that `WidgetSurfacePolicy` uses, via a shared helper. It shall **not** call `widgetSurfacePlacement()` directly, because that function hardcodes desktop-widget dimensions the OSD does not share.

**Acceptance Criterion:**
- `anchorFlagsForPosition()` (currently file-static in `WidgetSurfacePolicy.cpp`) is exposed and consumed by both the widget path and the OSD path — the nine-way switch exists exactly once in the codebase
- A unit test asserts `CenterBottom` yields the bottom anchor alone, and `RightTop` yields right|top
- Top-anchored OSD positions clear the bar by the same `kBarHeight + margin` rule as widgets
- Existing widget placement tests still pass unchanged after the extraction

---

#### REQ-C-007: Suppression API is architecture boundary point
**Requirement:** `OsdController::setSuppressed()` is the sole injection point for surface visibility state. `ShellApplication` (in `apps/shell/`) is the ONLY caller of this method. No other code path shall attempt to suppress the OSD.

**Acceptance Criterion:**
- `grep -r "setSuppressed" apps/ libs/` yields only `ShellApplication` constructor/member functions
- Code review confirms `holonight-services` has no awareness of which surfaces are open
- All surface-visibility logic is in `ShellApplication`, not the controller

---

#### REQ-C-008: Keyboard-layout channel shall not use suppression
**Requirement:** `ShellApplication` shall never call `setSuppressed("keyboard-layout", ...)` because no shell surface displays keyboard-layout state. Keyboard-layout OSD is always shown (if enabled) regardless of sidebar or popup visibility.

**Acceptance Criterion:**
- Code search for `setSuppressed("keyboard-layout"` yields no results
- Keyboard-layout OSD appears even when sidebar or audio popup are open
- Manual test confirms keyboard-layout events are not suppressed

---

#### REQ-C-009: Config shall use TOML [osd] section
**Requirement:** OSD configuration shall live in the shell's existing config file — `$XDG_CONFIG_HOME/holonight/config.toml` (resolved by `ConfigService::resolveConfigPath()`) — under an `[osd]` section with keys `enabled`, `timeout`, `position`, and the per-channel `volume.enabled`, `brightness.enabled`, `keyboard_layout.enabled`. Parsing shall use the existing free functions in `ConfigParsers.cpp` against an `OsdConfig` struct in `config_structs.h`.

**Acceptance Criterion:**
- A unit test parses a TOML fragment containing all six keys and asserts each lands in the expected `OsdConfig` field
- A unit test parses an empty `[osd]` section and asserts every default (enabled=true, timeout=1500, position=CenterBottom, all three channels enabled)
- A unit test parses a file with **no** `[osd]` section at all and gets the same defaults
- `OsdConfig` defines `operator==` (defaulted) consistent with the other config structs
- A disabled-widget-style round trip through `ConfigWriter` preserves every `[osd]` key rather than dropping unknown fields

---

#### REQ-C-010: OSD shall be disabled globally via osd.enabled
**Requirement:** When `osd.enabled = false` in config, no OSD surface is ever created and no events are emitted by the controller, even if services emit signals. The `ShellApplication` shall not wire surface visibility into `setSuppressed()`.

**Acceptance Criterion:**
- A unit test constructs controller with `enabled=false`, emits events, and verifies no signals
- A manual test with `osd.enabled = false` shows no OSD regardless of volume/brightness/layout changes
- Disabling OSD does not affect sidebar or audio popup visibility

---

#### REQ-C-011: OsdController shall clamp and validate timeout config
**Requirement:** The `osd.timeout` value shall be clamped to [300, 10000] milliseconds. If the config value is outside this range, it is clamped and a warning is logged.

**Acceptance Criterion:**
- Config with `timeout=100` results in OSD persistence of 300 ms (minimum clamped) with a warning logged
- Config with `timeout=20000` results in 10000 ms (maximum clamped) with a warning
- Config with `timeout=1500` is accepted as-is with no warning

---

#### REQ-C-012: Monitor routing shall use focused monitor or primary fallback
**Requirement:** When an OSD event is emitted, the surface is displayed on `ActiveWindowService::focusedMonitorName`. If no monitor is focused or the name is empty, fall back to the primary monitor (via `QGuiApplication::primaryScreen()->name()`).

**Acceptance Criterion:**
- A unit test mocks `ActiveWindowService::focusedMonitorName()` to return "HDMI-1"; OSD is created on that monitor
- When focused monitor returns empty string, OSD falls back to primary
- Manual test with multiple monitors shows OSD on the focused monitor

---

#### REQ-C-013: OSD surface shall be non-interactive
**Requirement:** The OSD surface shall be sized to its content rather than to the output, so that even a failure of the empty-input-region rule cannot make a full-screen area unclickable.

**Acceptance Criterion:**
- The surface width and height track the QML root's `implicitWidth`/`implicitHeight` (plus a fixed glow margin), as `NotificationToastSurface` tracks `contentHeight`
- The surface is never anchored to opposing edges in a way that stretches it to the full output dimension
- Manual check: the OSD occupies a compact box, and the rest of the screen remains interactive while it is visible

*(This requirement complements REQ-F-024, which forbids input handling outright; this one bounds the blast radius if that guarantee regresses.)*

---

#### REQ-C-014: Keyboard-layout name exposure is a required service change
**Requirement:** This feature shall add a `layoutName` property (QString, with a change notify signal) to `KeyboardLayoutService`, holding the full layout name. The value already flows through `setLayoutName()`, which currently converts it to a code and discards the original; the change is to retain and expose it.

**Acceptance Criterion:**
- `KeyboardLayoutService` declares `Q_PROPERTY(QString layoutName READ layoutName NOTIFY layoutNameChanged)`
- Feeding the service a Hyprland layout event with name "English (US)" leaves `layoutCode() == "EN"` and `layoutName() == "English (US)"`
- `layoutNameChanged` fires only when the name actually changes, matching the existing `setLayoutCode` guard style
- The existing `test_keyboard_layout_service.cpp` cases still pass unmodified

---

#### REQ-C-015: Test coverage shall include GTest unit tests with no live services
**Requirement:** All OSD controller logic (prime-and-diff, grace period, suppression, enable/disable, config parsing) shall be verifiable via GTest unit tests using mock `OsdChannelSource` instances, without instantiating real `AudioService`, `BrightnessService`, or `KeyboardLayoutService`.

**Acceptance Criterion:**
- `task test` runs at least 8 OSD-specific unit tests
- All tests pass without PulseAudio, sysfs, or D-Bus access
- Code coverage for `OsdController` and config parsing is > 85%
- A single test covers the end-to-end prime-and-diff → display suppression → update-in-place flow

---

## Accepted Consequences

- **Pressing volume-up at 100% or brightness-up at max produces no OSD.** This is inherent to the observation trigger model. An event is only shown if the service emits a change signal; if the value is already at maximum, the service may not emit, and no OSD is displayed.

- **A gradual software brightness fade writes sysfs repeatedly, so the OSD stays visible for the whole fade.** This is expected: each write triggers a `brightnessChanged()` signal, restarting the hide timer. The OSD serves as real-time feedback during the fade.

- **If Hyprland is configured with per-window keyboard layouts, the keyboard-layout OSD fires on every window focus change.** The service observes layout changes; if Hyprland emits a layout change on each window focus, the OSD will fire each time. This is correct behavior for the observation model.

- **OSD is captured by `grim` and other screenshot tools.** wlr-layer-shell offers no per-surface capture opt-out; this is accepted.

---

## Acceptance Criteria Summary

**Functional Acceptance (automated testing):**
- [ ] `task build` succeeds without warnings in C++ and QML
- [ ] `task qml-lint` reports zero errors in OSD QML files
- [ ] `task format-check` passes for all OSD code
- [ ] `task tidy` reports zero issues in C++ OSD code
- [ ] `task test` runs >= 8 OSD controller unit tests, all passing
- [ ] Unit tests cover: prime-and-diff, grace period, suppression, enable/disable, config parsing, all three channels
- [ ] Config parsing tests verify clamping, fallback on unknown position, and per-channel disable
- [ ] OsdController emits correct signals for level and selection events
- [ ] GTest tests run without live services, PulseAudio, sysfs, or D-Bus

**Integration Acceptance (automated + manual):**
- [ ] `ShellApplication` constructor wires audio popup visibility → `setSuppressed("audio-volume", ...)`
- [ ] `ShellApplication` wires sidebar QuickSettings visibility → brightness suppression
- [ ] OSD surface is created and destroyed via `PerMonitorLayerManager` following the `NotificationToastSurface` pattern
- [ ] QML renderers dispatch level-kind vs. selection-kind events correctly
- [ ] Colors are sourced from `HoloniightPalette` tokens; no hardcoded hex
- [ ] Configuration is parsed from `[osd]` section in `shell.toml`
- [ ] Disabling OSD globally via `osd.enabled=false` prevents all surface creation

**Presentation Acceptance (manual testing in live Hyprland session):**
- [ ] Volume OSD appears when volume changes, displays progress bar + icon + percentage
- [ ] Brightness OSD appears when brightness changes, displays progress bar + icon + percentage
- [ ] Keyboard-layout OSD appears when layout changes, displays short label + full label (no bar)
- [ ] Muted audio OSD displays bar at real position (not 0%), bar is dimmed, text reads "Muted"
- [ ] Icon mapping: `audio-volume-muted` when muted, `audio-volume-high/medium/low` by tier, `brightness` for brightness, `keyboard` for layout
- [ ] New events on the same channel update in place (no entrance replay); hide timer restarts
- [ ] New events on different channels replace immediately (no hide-then-show); entrance animation plays
- [ ] OSD fades in smoothly over ~200 ms, fades out over ~150 ms
- [ ] OSD disappears 1500 ms after the last change (by default)
- [ ] OSD appears on the focused monitor; falls back to primary if no focus
- [ ] OSD position follows config value (center-bottom by default, can move to other positions)
- [ ] Clicking the OSD does not consume the click; underlying window is focused
- [ ] Opening audio popup while OSD is visible suppresses subsequent volume OSD
- [ ] Opening sidebar + QuickSettings while OSD is visible suppresses subsequent brightness OSD
- [ ] Closing the popup/sidebar re-enables suppression lifting (next event shows OSD)
- [ ] Keyboard-layout OSD is never suppressed
- [ ] Audio status popup and sidebar QuickSettings continue to function normally alongside OSD
- [ ] Rapid volume/brightness changes show smooth real-time feedback
- [ ] Switching between monitors with OSD visible routes OSD to the focused monitor
- [ ] Manual compositor test with `task compositor-smoke-check` yields no critical issues

**Non-Functional Acceptance:**
- [ ] No OSD surface is created until the first displayable event; no OSD appears at login
- [ ] The observe→emit path contains no timer, debounce, or artificial delay
- [ ] All value transitions are `Behavior`-driven and retarget correctly when interrupted
- [ ] `destroySurface()` is idempotent, resets all pointers, and runs from the destructor
- [ ] No polling timer exists in the controller, adapters, or surface
- [ ] Logging is available via `QT_LOGGING_RULES="*osd*.debug=true"` without code changes
