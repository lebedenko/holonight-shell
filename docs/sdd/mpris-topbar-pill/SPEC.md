# MPRIS Backend + Topbar "Now Playing" Pill — Specification

## Overview

This specification defines a first-iteration MPRIS (Media Player Remote Interfacing Specification) backend for `holonight-shell` that discovers and tracks active media players on the D-Bus session bus, and exposes a "now playing" pill on the topbar showing the current track's title/artist and playback controls.

The backend discovers MPRIS-compliant players, selects an active player based on playback state and activity timestamp, and exposes read-only metadata and control invokables to QML. The topbar pill renders a minimal, fixed-layout display (player icon, artist—title, previous/play-pause/next buttons) with capability-gated control buttons. This phase implements steps 1–5 of `../../holonight-docs/13-mpris/idea.md`; media popups, the desktop widget, seek controls, artwork, and player-switching UI are deferred.

**Scope boundary**: Single topbar section; global singleton service state; no per-monitor arbitration of player selection; no position/duration tracking; no artwork caching.

---

## Non-Goals

The following aspects are explicitly out of scope for this feature cycle:

- Media player popup surface or detailed player view.
- Player switcher UI or "Open player"/Raise window functionality.
- Manual player selection APIs (`selectPlayer()`, `selectNextPlayer()`) or ignore/priority-list configuration.
- Position, duration, or progress display; seek bar; rate control; seeking of any kind.
- Album art, artwork URLs (`artUrl`), or artwork caching (local or remote). This deliberately differs from the reference topbar mockup, which uses artwork with an application-icon fallback; this phase always uses the application icon.
- Configurable bar layout modes (compact/full/status-only variations).
- Volume control or mixing with audio widgets.
- Shuffle, repeat, or playlist control buttons.
- Stopped-player fallback selection (no display when no player is actively Playing or Paused).
- TrackList or Playlists MPRIS interface support.
- Hardware media-key integration.
- Public D-Bus service exposing the selected player/policy to external consumers.

---

## Functional Requirements

### Backend: Player Discovery

**REQ-F-001** — D-Bus name watching for MPRIS players
*When* the shell starts and *whenever* a D-Bus name appears or disappears on the session bus, *the system shall* watch for names matching the pattern `org.mpris.MediaPlayer2.*` and maintain a collection of discovered players.

- **Acceptance criterion**: A new MPRIS player (`org.mpris.MediaPlayer2.VLC`, etc.) appearing on the bus triggers acquisition of its Root and Player interfaces. The player is added to an internal registry. Disappearance removes it. A direct non-empty-old-owner to non-empty-new-owner replacement releases the old snapshot and reacquires the replacement process.

---

### Backend: Player Property Tracking

**REQ-F-002** — Subscribe to player state changes
*For each* discovered MPRIS player, *the system shall* establish a D-Bus subscription to `org.freedesktop.DBus.Properties.PropertiesChanged` before reading its initial property snapshots. The handler shall accept only `org.mpris.MediaPlayer2` and `org.mpris.MediaPlayer2.Player` payloads, and shall refresh tracked properties named in the signal's invalidated-property list.

- **Acceptance criterion**: A `PlaybackStatus` change updates internal state without polling. Invalidating `PlaybackStatus` causes a bounded `GetAll(Player)` refresh. Unrelated interfaces cannot mutate player state. If subscription setup fails, acquisition is rolled back so a later appearance event can retry.

**REQ-F-003** — Read and track player metadata and capabilities
*The system shall* read and maintain these properties from each discovered player:
  - From `org.mpris.MediaPlayer2` (Root):
    - `Identity` (string — the player's application name)
    - `DesktopEntry` (string — desktop file base name for icon lookup)
  - From `org.mpris.MediaPlayer2.Player`:
    - `PlaybackStatus` (string: `Playing`, `Paused`, or `Stopped`)
    - `Metadata` (dict, specifically `xesam:title`, `xesam:artist` array, `mpris:trackid`)
    - Capability flags: `CanGoNext`, `CanGoPrevious`, `CanPlay`, `CanPause`, `CanControl`

- **Acceptance criterion**: Separate `GetAll("org.mpris.MediaPlayer2")` and `GetAll("org.mpris.MediaPlayer2.Player")` calls populate the corresponding fields. The fake backend stores the interfaces separately, and tests confirm that title fallback and icon lookup use Root values.

**REQ-F-004** — Ignore Position, Rate, and Seek-related signals
*The system shall NOT* read `Position`, `Rate`, or `MinimumRate` properties, nor subscribe to or handle the `Seeked` signal.

- **Acceptance criterion**: The implementation contains no code reading `Position`, consuming `Seeked`, or reacting to `Rate`-change notifications. Code review confirms the absence of these.

---

### Backend: Activity Timestamp Tracking

**REQ-F-005** — Track and update activity timestamp per player
*The system shall* maintain a `lastActivityTimestamp` for each known player, initialized to an undefined/null value on discovery.

- **Acceptance criterion**: Each player object has a `lastActivityTimestamp` field. Initial state is null/not-set. The field changes only under conditions specified in REQ-F-006.

**REQ-F-006** — Update activity timestamp only on specific state changes
*The system shall* update a player's `lastActivityTimestamp` to the current time *if and only if* one of the following occurs:
  a. The player's `PlaybackStatus` transitions from `Paused` or `Stopped` into `Playing`.
  b. While the player's `PlaybackStatus` is already `Playing`, the `mpris:trackid` property changes (indicating a genuine track change).
  c. The player is discovered (first D-Bus appearance) already in `Playing` state (timestamp = discovery time).

*The system shall NOT* update `lastActivityTimestamp` on:
  - Repeated `PropertiesChanged` notifications with an unchanged `mpris:trackid` (same track still playing).
  - Transitions from `Playing` to `Paused` or `Stopped`.
  - Changes to capability flags (e.g., `CanPlay` toggling).

This deliberately narrows the reference concept's broader activity policy. Metadata-only noise is excluded so browser players cannot steal selection repeatedly; command activity and `Seeked` are excluded because commands target only the already-active player and seeking is out of scope.

- **Acceptance criterion**: Unit test with a mock player shows that `lastActivityTimestamp` is updated only on the three specified conditions. Test cases verify that a `PropertiesChanged` event with unchanged `mpris:trackid` does NOT update the timestamp; a transition to `Paused` does NOT update it; a capability-flag-only change does NOT update it.

---

### Backend: Active Player Selection Algorithm

**REQ-F-007** — Deterministic active player selection by state and timestamp
*The system shall* select the active player using the following algorithm, applied whenever the player registry changes or a player's state is updated:
  1. Among all known players with `PlaybackStatus == "Playing"`, select the one with the highest (most recent) `lastActivityTimestamp`.
  2. If no `Playing` player exists, among all known players with `PlaybackStatus == "Paused"`, select the one with the highest `lastActivityTimestamp`.
  3. If neither a `Playing` nor a `Paused` player exists, or if all players are `Stopped`, there is no active player.

*The system shall NOT* select a `Stopped` player or use `Stopped` state as a fallback.

- **Acceptance criterion**: Unit test with three mock players (Player-A in Playing/ts=100, Player-B in Playing/ts=50, Player-C in Paused/ts=150) confirms that Player-A is selected (highest Playing timestamp). Another test with Player-A Paused and Player-C Paused (ts=150 > ts=100) confirms Player-C is selected. A third test with all players Stopped confirms no active player is selected.

**REQ-F-008** — Re-run active player selection on state changes
*When* a known player's `PlaybackStatus` changes, its `mpris:trackid` changes, a player disappears from the D-Bus bus, or a new player is discovered, *the system shall* immediately re-run the active player selection algorithm (REQ-F-007) and update the active player if the algorithm result changes.

- **Acceptance criterion**: Integration test shows selection is re-run when a previously Playing player transitions to Paused. With no other Playing player, it remains active as the most-recent Paused player, unless another Paused player has a newer activity timestamp.

---

### Backend: QML Service Exposure

**REQ-F-009** — Expose active player state as a QML singleton
*The system shall* register a `MprisService` QML singleton in the `HolonightShell` module that exposes the following read-only Q_PROPERTY values and NOTIFY signals:
  - `bool hasActivePlayer` — true if an active player is selected, false otherwise.
  - `QString activeTitle` — the active player's `xesam:title`, or `activeIdentity` if title is empty/absent.
  - `QString activeArtist` — the active player's primary artist from `xesam:artist` array (first element), or empty string if array is absent/empty.
  - `QString activeIdentity` — the active player's `Identity` property.
  - `QString activeDesktopEntry` — the active player's `DesktopEntry` property (for icon lookup).
  - `QString activePlaybackStatus` — the active player's `PlaybackStatus` (`Playing`, `Paused`, or empty if no active player).
  - `bool canGoNext` — the active player's `CanGoNext` capability, or false if no active player.
  - `bool canGoPrevious` — the active player's `CanGoPrevious` capability, or false if no active player.
  - `bool canPlay` — the active player's `CanPlay` capability, or false if no active player.
  - `bool canPause` — the active player's `CanPause` capability, or false if no active player.
  - `bool canControl` — the active player's `CanControl` capability, or false if no active player.

- **Acceptance criterion**: QML code imports `HolonightShell` and accesses `MprisService.activeTitle`, `MprisService.hasActivePlayer`, etc. without errors. Each property is bound correctly and updates reactively when the active player changes.

**REQ-F-010** — Expose playback control invokables
*The system shall* expose the following invokable methods on the `MprisService` QML singleton:
  - `void playPause()` — request play or pause on the active player.
  - `void next()` — request next track on the active player.
  - `void previous()` — request previous track on the active player.

- **Acceptance criterion**: QML code calls `MprisService.playPause()` without errors. The method is listed in the QML module's type information (qmltypes/typeinfo).

---

### Backend: Metadata Fallbacks

**REQ-F-011** — Use Identity as title fallback
*If* the active player's `xesam:title` metadata is empty, absent, or null, *the system shall* display the player's `Identity` property as the title instead.

- **Acceptance criterion**: Unit test with a mock player having empty `xesam:title` confirms `activeTitle` property returns the player's `Identity`. Test with a mock player having a non-empty title confirms `activeTitle` returns that title.

**REQ-F-012** — Display no artist text if artist array is empty
*If* the active player's `xesam:artist` array is absent, empty, or contains no elements, *the system shall* set `activeArtist` to an empty string (not a placeholder or fallback text).

- **Acceptance criterion**: Unit test with a mock player having an empty `xesam:artist` array confirms `activeArtist` returns `""`. Test with a single-element array confirms `activeArtist` returns that element.

---

### Backend: Control Commands

**REQ-F-013** — Fire-and-forget command invocation
*When* a control method (`playPause()`, `next()`, `previous()`) is invoked on `MprisService`, *the system shall* issue a D-Bus method call to the active player's `org.mpris.MediaPlayer2.Player` interface and *shall NOT* wait for a reply or optimistically update local state.

- **Acceptance criterion**: Code inspection of `MprisService::playPause()` shows a `QDBusInterface::asyncCall()` (or equivalent fire-and-forget pattern) and no subsequent update to any backend playback state. No signal is emitted immediately after the call; state changes only in response to a future `PropertiesChanged` signal.

**REQ-F-014** — No optimistic playback state updates
*The system shall NOT* update `activePlaybackStatus` or any other player state in response to a user clicking a control button. State changes *only* when a subsequent `PropertiesChanged` signal is received from the player, or the command fails silently and state does not change at all.

- **Acceptance criterion**: Unit test mocks `QDBusInterface::asyncCall()` to fail or hang. The test confirms that clicking a button and having the call fail does not change `activePlaybackStatus` or any displayed state. A separate integration test confirms that `activePlaybackStatus` updates only after a real `PropertiesChanged` signal arrives.

**REQ-F-015** — Gate commands by capability flags
*The system shall* apply the following capability gating to control commands:
  - `playPause()` is effective only if `canControl && (canPlay || canPause)` depending on the current `activePlaybackStatus` (Paused requires `canPlay`, Playing requires `canPause`).
  - `next()` is effective only if `canControl && canGoNext`.
  - `previous()` is effective only if `canControl && canGoPrevious`.
  - If `canControl == false`, all three commands have no effect regardless of other flags.

- **Acceptance criterion**: Unit test with a mock player having `canControl = true` and `canPlay = false` confirms that `playPause()` does NOT issue a D-Bus call when the player is Paused (canPlay would be required). A test with `canControl = false` confirms no D-Bus call is issued for any command.

---

### Topbar Pill: Visibility and Composition

**REQ-F-016** — Visibility tied to active player
*The system shall* render the MPRIS topbar pill (as a new `MprisSection.qml` component) if and only if `MprisService.hasActivePlayer == true`. When no active player exists, the section renders zero width and is not visible on the topbar.

- **Acceptance criterion**: Visual test shows the pill present and rendered when an active player is Playing or Paused. Visual test shows the pill absent (no space, no rendering) when all players are Stopped or no player is discovered.

**REQ-F-017** — Fixed layout: icon, artist—title, controls
*The system shall* lay out the MPRIS pill in a fixed sequence:
  1. Application icon (resolved from `activeDesktopEntry`).
  2. Single fixed-width text area displaying `activeArtist` (if non-empty) followed by an en-dash separator and `activeTitle`. Text uses ellipsis or fade truncation if it exceeds available width; the topbar layout does not dynamically resize to fit the text.
  3. Button row (previous, play-pause, next) in that order.

- **Acceptance criterion**: Visual inspection of the rendered pill shows the icon on the left, center text area with artist—title, and three control buttons on the right. Changing the active player's title to a very long string does not cause the topbar to widen; text is truncated or faded.

**REQ-F-018** — Icon resolution from desktop entry
*The system shall* resolve the active player's icon by looking up `activeDesktopEntry` using the existing shell icon-provider mechanism (e.g., `image://icon/...`), identical to the pattern used elsewhere in the shell (e.g., default app icons).

- **Acceptance criterion**: A player with `DesktopEntry = "vlc"` displays the VLC icon (as resolved by the icon provider). Swapping active players changes the displayed icon to match the new player's desktop entry. Code uses `image://icon/` URL pattern.

**REQ-F-019** — Disabled button styling
*The system shall* visually indicate when previous, play-pause, or next buttons are disabled (per REQ-F-015 capability gating) by reducing opacity or dimming them according to existing shell disabled-control conventions (using `HoloniightPalette` theme tokens for the dimmed color/opacity, no hardcoded values).

- **Acceptance criterion**: Unit test or visual inspection shows a button with `enabled: false` binding rendered with a distinct visual state (e.g., reduced opacity). Button is re-enabled and re-styled when the underlying capability flag changes.

**REQ-F-020** — Play/pause glyph reflects playback state
*The system shall* display a pause glyph when `activePlaybackStatus == "Playing"` and a play glyph when `activePlaybackStatus == "Paused"`. The glyph updates reactively whenever `activePlaybackStatus` changes.

- **Acceptance criterion**: Visual test shows a play glyph when the active player is Paused. Triggering a state change (via D-Bus `PropertiesChanged`) to Playing changes the glyph to pause. The glyph reflects only the backend state, not a button press.

---

### Topbar Pill: Interactivity

**REQ-F-021** — Control buttons only interactive elements
*The system shall* design the MPRIS pill such that the *only* interactive elements (responding to clicks/taps) are the three control buttons (previous, play-pause, next). The icon and the artist—title text area have no click handlers and produce no visible feedback on interaction.

- **Acceptance criterion**: Clicking the icon or text area produces no change in displayed state and no D-Bus method call. Clicking a control button issues the corresponding method call (per REQ-F-013). Code inspection confirms no `MouseArea` or `TapHandler` on the icon or text area.

**REQ-F-022** — No scroll or middle-click handling
*The system shall NOT* respond to scroll-wheel events, middle-click, or any button other than left-click on any part of the MPRIS pill.

- **Acceptance criterion**: Scrolling over the pill does not change any state or trigger any action. Middle-clicking a control button has no effect.

---

### Topbar Pill: Layout Stability

**REQ-F-023** — Reserve fixed button widths to prevent layout shifts
*The system shall* allocate fixed width for the button row such that enabling or disabling individual buttons (due to capability-flag changes) does not cause the topbar layout to reflow or shift. The button row width remains constant whether all three buttons are enabled, partially enabled, or all disabled.

- **Acceptance criterion**: Unit test or visual inspection shows no change in pill width or position when a button transitions from disabled to enabled or vice versa. Layout metrics (x, y, width) for the text area and icon remain constant.

---

### Topbar Pill: Visual Style

**REQ-F-024** — Use only theme palette for coloring
*The system shall* apply visual distinction between Playing and Paused states (e.g., accent emphasis on the button row or text when Playing, reduced emphasis when Paused) using only `HoloniightPalette` theme tokens (no hardcoded colors or hex values).

- **Acceptance criterion**: Code review of `MprisSection.qml` and any related components finds no hardcoded `#rrggbb` or `rgb()` color literals. All colors are assigned via `HoloniightPalette.<token>` imported from `import Holonight`.

---

### Topbar Integration

**REQ-F-025** — Place section in topbar between ActiveWindowSection and WeatherSection
*The system shall* add the MPRIS pill as a new section in `apps/shell/qml/Topbar/TopBar.qml` positioned between the `ActiveWindowSection` and the `WeatherSection` in the topbar composition.

- **Acceptance criterion**: Code inspection of `TopBar.qml` shows `MprisSection` instantiated in the correct position in the section list. Visual test confirms the pill appears between the active window title and the weather widget.

---

### Per-Monitor Consistency

**REQ-F-026** — Identical pill state across all monitors
*The system shall* use a single global `MprisService` singleton shared by all per-monitor topbar instances. All monitors display the same active player's metadata and controls; there is no per-monitor arbitration of player selection or per-monitor state variants.

- **Acceptance criterion**: A multi-monitor setup with two topbars shows the same active player, title, artist, and control states on both monitors. Changing the active player on the backend updates both topbars identically and synchronously.

---

## Non-Functional Requirements

### Testability and Seams

**REQ-NF-001** — Injectable D-Bus backend seam
*The system shall* structure the MPRIS backend (player discovery, property tracking, D-Bus interaction) with an injectable seam (following the project pattern of `IPortalDBus`, `NullBrightnessBackend`, or similar) to enable unit testing of active-player-selection logic, activity-timestamp updates, and capability gating without a live D-Bus session or real MPRIS players.

- **Acceptance criterion**: The backend defines an interface (e.g., `IMprisBackend` or `IMprisDBus`) that abstracts D-Bus operations. A test implementation (e.g., `MockMprisBackend` or `FakeMprisPlayer`) is provided that allows tests to instantiate player objects, set properties, emit signals, and verify that the active-player-selection algorithm and state-update logic respond correctly. Tests run without requiring `dbus-daemon` to be active.

**REQ-NF-002** — Unit-testable active player selection
*The system shall* expose the active-player-selection algorithm (REQ-F-007) in a form that is unit-testable independently of D-Bus initialization, D-Bus name watching, or QML property binding — e.g., as a static or free function that takes a vector/list of mock player objects and returns the selected player index or object.

- **Acceptance criterion**: A unit test file (e.g., `test_mpris_selection.cpp`) instantiates mock player objects, calls the selection function, and verifies the result. Test cases cover the three selection branches (Playing+highest, Paused+highest, none selected).

---

### Color and Theme Compliance

**REQ-NF-003** — No hardcoded color values
*The system shall* ensure that all colors used in the MPRIS section (`MprisSection.qml`) and related components come from the HoloNight theme system (`HoloniightPalette.*` tokens imported via `import Holonight`). Hardcoded hex values or `rgb()` literals shall not appear.

- **Acceptance criterion**: Code review of `MprisSection.qml` and any nested QML components uses `grep` or manual inspection to confirm zero `#[0-9a-fA-F]{3,6}` or `rgb(` patterns outside comments. All color assignments reference `HoloniightPalette.<token>`.

---

### Layout Stability and Performance

**REQ-NF-004** — Stable layout under state changes
*While the pill is visible, the system shall* keep its width stable when metadata, playback state, or enabled/disabled button states change. Appearance and disappearance intentionally animate the pill between zero and its fixed visible width and may reflow neighboring sections.

- **Acceptance criterion**: QML layout metrics confirm capability changes do not change pill width. A visual test confirms appearance/disappearance reflow is limited to the pill's 200 ms width animation and that its fully visible width is stable.

---

### D-Bus Interface Convention

**REQ-NF-005** — D-Bus interface declaration
*The system shall* ensure that any C++ class implementing a named D-Bus interface includes the macro `Q_CLASSINFO("D-Bus Interface", "org.example.InterfaceName")` so that the interface is correctly advertised in D-Bus introspection XML.

- **Acceptance criterion**: Code review confirms the presence of the `Q_CLASSINFO` macro if the backend exports any custom D-Bus service. If no custom service is exported (only client-side property reads), this requirement does not apply.

---

### QML Module Registration

**REQ-NF-006** — QML singleton registration
*The system shall* register `MprisService` as a `QML_SINGLETON` in the `HolonightShell` QML module via the standard Qt6/CMake registration mechanism (e.g., `set_source_files_properties()` with `QT_QML_SINGLETON_TYPE`).

- **Acceptance criterion**: Running `task qmltypes-check` confirms that the `HolonightShell` qmltypes file includes a `MprisService` singleton entry (type name, properties, and invokables listed). QML code imports `HolonightShell` and accesses `MprisService.<property>` without import errors or undefined-type warnings.

---

## Constraints

**REQ-C-001** — QML directory structure
The MPRIS section QML file shall be placed in `apps/shell/qml/Topbar/` (following the per-directory organization pattern used by other topbar sections, e.g., `apps/shell/qml/Topbar/LogoSection/`, `apps/shell/qml/Topbar/WeatherSection/`). The exact directory and file-naming convention (e.g., `MprisSection/MprisSection.qml` or `MprisSection.qml`) shall follow the existing shell conventions.

- **Acceptance criterion**: File path `apps/shell/qml/Topbar/MprisSection.qml` (or equivalent per-directory structure) exists and is referenced in `apps/shell/CMakeLists.txt` with appropriate QRC asset bundling.

**REQ-C-002** — MPRIS protocol specification compliance
The implementation shall follow the MPRIS 2.2 specification (or later) as standardized by freedesktop.org, specifically:
  - D-Bus service names must match the pattern `org.mpris.MediaPlayer2.*`.
  - `Identity` and `DesktopEntry` shall be read from `org.mpris.MediaPlayer2`; playback state, metadata, and transport capabilities shall be read from `org.mpris.MediaPlayer2.Player`.
  - No custom/non-standard MPRIS extensions are required or assumed.

- **Acceptance criterion**: Code review confirms that no hardcoded service names deviate from the `org.mpris.MediaPlayer2.*` pattern and that property names match the MPRIS spec (e.g., `xesam:title`, `mpris:trackid`, `CanGoNext`).

**REQ-C-003** — Synchronous D-Bus initialization during shell startup
*The system shall* initialize the MPRIS backend (D-Bus name watching) synchronously during shell startup so that players already on the bus before the shell launches are discovered immediately and available for selection without delay.

- **Acceptance criterion**: Integration test starts the shell with a known MPRIS player already running (e.g., `paplay` with MPRIS support, or a mock test player on the D-Bus bus). On shell launch, the player is discovered and available in `MprisService` within the startup window (no async delay in discovery).

---

## Acceptance Summary

Verification of all requirements above consists of:

1. **Code review**: Backend structure, injectable seams, no hardcoded colors, proper D-Bus interface declarations.
2. **Unit tests**: Active-player-selection algorithm, activity-timestamp updates, capability gating, metadata fallbacks, command fire-and-forget behavior.
3. **Integration tests**: D-Bus name watching, property subscription, player discovery lifecycle, active-player re-selection on state changes.
4. **Visual/manual tests**: Pill visibility, layout, icon resolution, glyph updates, button enable/disable, multi-monitor consistency, no layout shifts.
5. **QML smoke test**: Singleton registration, property bindings, invokable methods, theme compliance (via code review and live inspection in a Wayland session).

A passing test suite and code review against all requirements above constitutes readiness for the next development phase.
