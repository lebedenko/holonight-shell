# MPRIS Desktop Widget Specification

## Overview

This specification defines a new desktop widget that displays ambient, read-only MPRIS (Media Player Remote Interfacing Specification) media information. The widget appears as a bottom-layer wlr-layer-shell surface (similar to the existing Clock and TimeToEvent widgets), showing current playback status, album artwork, track metadata, and playback position when a media player is active. The widget is hidden when no media player is active, when playback is stopped, or when the target monitor's visible workspace contains windows.

This is a new addition to the existing MPRIS backend (which powers the topbar "now playing" pill) and the existing desktop-widget system (which manages Clock and TimeToEvent widgets).

## Context: Existing Systems

**MprisService**: A QML singleton (in `libs/holonight-services/src/mpris/`) that selects the "active" player from multiple possible MPRIS players using a priority system (playing > paused > manually selected > stopped). Currently exposes Q_PROPERTYs: `hasActivePlayer`, `activeTitle`, `activeArtist`, `activeIdentity`, `activeDesktopEntry`, `activePlaybackStatus`, `canGoNext`, `canGoPrevious`, `canPlay`, `canPause`, `canControl`. This selection logic is reused by the new widget as-is.

**MprisPlayer struct**: The data container in `libs/holonight-services/src/mpris/MprisPlayer.h` that holds per-player information. Currently includes: service_name, identity, desktop_entry, playback_status, title, artists (QStringList), track_id, can_go_next/previous/play/pause/control, last_activity_timestamp_ms. This feature adds new fields to this struct (see REQ-F-011 through REQ-F-015).

**WidgetManager & WidgetDefinition**: An existing system (`libs/holonight-surfaces/src/WidgetManager.h`) that manages wlr layer-shell surfaces on the bottom layer, one per (monitor, widget-definition) pair. Already supports Clock and TimeToEvent widgets. Surfaces stay mapped and cached across visibility toggles; only QML root visibility changes to show/hide. `WidgetDefinition` (in the separate `holonight-config` package) carries shared fields: `monitors` (QStringList, empty = all), `position` (one of 9 anchor values), and `enabled` (bool). A shared `WidgetsConfig` carries a `margin` (logical pixels, applied to all widget edges).

**MonitorOccupancyService**: An existing system that determines whether a monitor's currently-visible workspace has windows. All existing desktop widgets use this to gate visibility: hidden while workspace is occupied, shown when empty.

**Topbar MPRIS pill**: An existing read-only indicator in the topbar (`apps/shell/qml/Topbar/Mpris*.qml`) that shows only the app icon via `image://icon/<desktop_entry>`. This feature does not modify it.

---

## Requirements

### Timing acceptance semantics

Unless a requirement defines a controlled benchmark explicitly, phrases such as "within one
animation frame" mean that the state mutation is queued on the next GUI event-loop turn and is
visible on the next compositor frame in an idle-shell manual test; they are not hard real-time
guarantees under arbitrary scheduler, D-Bus, GPU, or compositor load. Millisecond UI-latency values
are service objectives measured with `QElapsedTimer` over at least 20 samples and reported at the
95th percentile. Deterministic unit tests verify signal ordering and event-loop turns rather than
wall-clock deadlines.

### Display and Visual Composition (REQ-F-001 to REQ-F-010)

**REQ-F-001: Ubiquitous Display Element**
The system SHALL display a desktop widget on the bottom layer of the wlr-layer-shell compositor.

*Acceptance Criterion*: Verify that when MprisService.hasActivePlayer is true and playback status is "Playing", a wlr-layer-shell surface with role `zwlr_layer_surface_v1` appears on the monitor's bottom layer, and its stacking order allows it to remain visible behind all non-desktop windows.

---

**REQ-F-002: Album Artwork Display**
The widget SHALL prominently display album artwork as a large, subdued image.

*Acceptance Criterion*: Verify that the artwork (resolved from `MprisService.activeArtUrl`) is rendered at no smaller than 256 pixels (logical) on a side, with opacity reduced to 50% or lower relative to the full-opacity state, and that it remains within the widget's bounded layout region (not overflowing into adjacent areas).

---

**REQ-F-003: Metadata Display**
The widget SHALL display artist name and track title in a human-readable format.

*Acceptance Criterion*: Verify that both `MprisService.activeArtist` and `MprisService.activeTitle` are visible as text elements within the widget's bounds at a legible font size (≥ 11pt); both update within one animation frame when the properties change.

---

**REQ-F-004: Album Name Display**
The widget SHALL display the album name when available.

*Acceptance Criterion*: Verify that `MprisService.activeAlbum` (if non-empty) is rendered as a distinct text element; when the album is empty or missing, no album line is shown (no placeholder text, no extra blank space reserved).

---

**REQ-F-005: Playback Progress Indicator**
WHEN the active player's track has a defined length (mpris:length is present and > 0) THEN the widget SHALL display a thin horizontal progress line showing elapsed vs. total time.

*Acceptance Criterion*: Verify that a progress bar is rendered proportional to `activePosition / activeLength`; the bar is exactly 2–4 pixels tall and spans the widget's width within layout margins; bar appears within 500 ms of a track-length becoming known.

---

**REQ-F-006: Elapsed Time Display**
The widget SHALL display elapsed playback time in MM:SS format.

*Acceptance Criterion*: Verify that the elapsed time updates at least every 500 ms (2 Hz) while playback is active and the widget is visible; the displayed value matches `activePosition` (in microseconds, divided by 1,000,000 and formatted as MM:SS); format is correct for both short (<1 min) and long (>1 hour) durations.

---

**REQ-F-007: Duration Display**
WHEN the active player reports a valid track length (mpris:length > 0) THEN the widget SHALL display total duration in MM:SS format adjacent to elapsed time (formatted as "MM:SS / MM:SS").

*Acceptance Criterion*: Verify that duration is rendered (e.g., "01:23 / 03:45") when `activeLength` is > 0; update latency is ≤ 500 ms after a new track starts.

---

**REQ-F-008: Player Identity Icon**
The widget SHALL display the active player's application icon.

*Acceptance Criterion*: Verify that `image://icon/<activeDesktopEntry>` is resolved and rendered at a size of 32–48 logical pixels; icon updates within one frame when the active player changes.

---

**REQ-F-009: Pause Indicator**
WHEN the active player's playback status is "Paused" THEN the widget SHALL display a subtle pause glyph or visual indicator (distinct from the active/playing appearance).

*Acceptance Criterion*: Verify that a pause symbol (e.g., two vertical bars) is overlaid on or adjacent to the player icon when `activePlaybackStatus == "Paused"`; the glyph is smaller and lower-opacity than the icon (e.g., 16 logical pixels, 60% opacity); glyph appears/disappears within one animation frame of the status change.

---

**REQ-F-010: Seekability Indicator**
WHEN the active player does not support seeking (canSeek is false) THEN the widget's progress line (if displayed) SHALL be visually distinct (e.g., lighter, dimmed, or marked as non-interactive).

*Acceptance Criterion*: Verify that the progress bar's opacity or color changes to indicate non-seekability when `activeCanSeek == false`; the change is visible and distinguishable from a seekable-player state; update latency is ≤ 500 ms when canSeek property changes.

---

### Backend Data and MPRIS Service Additions (REQ-F-011 to REQ-F-025)

**REQ-F-011: Album Metadata in Player Struct**
The `MprisPlayer` struct SHALL include an `album` field (QString) to hold the track's album name.

*Acceptance Criterion*: Verify that `MprisPlayer::album` is populated from the MPRIS `org.mpris.MediaPlayer2.Player.Metadata` dictionary's `xesam:album` property on every metadata change; field is empty QString if property is absent.

---

**REQ-F-012: Artwork URL in Player Struct**
The `MprisPlayer` struct SHALL include an `art_url` field (QString) to hold the raw artwork URL/URI.

*Acceptance Criterion*: Verify that `MprisPlayer::art_url` is populated from the MPRIS metadata's `mpris:artUrl` property; field is empty QString if absent; both `file://` and `http(s)://` URIs are stored verbatim without transformation or validation.

---

**REQ-F-013: Position in Player Struct**
The `MprisPlayer` struct SHALL include a `position` field (qint64) to hold playback position in microseconds.

*Acceptance Criterion*: Verify that `MprisPlayer::position` is updated whenever the MPRIS `Position` property changes or immediately after a `Seeked` signal is received; value is in microseconds (matching MPRIS spec); field is 0 when no player is active.

---

**REQ-F-014: Track Length in Player Struct**
The `MprisPlayer` struct SHALL include a `length` field (qint64) to hold track duration in microseconds.

*Acceptance Criterion*: Verify that `MprisPlayer::length` is populated from the MPRIS metadata's `mpris:length` property; field is 0 (not negative or uninitialized) if the property is absent or the value is ≤ 0.

---

**REQ-F-015: Seek Capability in Player Struct**
The `MprisPlayer` struct SHALL include a `can_seek` field (bool) to indicate whether the player supports track seeking.

*Acceptance Criterion*: Verify that `MprisPlayer::can_seek` is populated from the MPRIS `CanSeek` property; defaults to false if the property is absent; value updates within 100 ms of an MPRIS `PropertiesChanged` signal for CanSeek.

---

**REQ-F-016: MprisService Album Property**
MprisService SHALL expose an `activeAlbum` Q_PROPERTY (const QString&, read-only, NOTIFY signal).

*Acceptance Criterion*: Verify that `activeAlbum` returns the active player's `MprisPlayer::album` field; property emits its NOTIFY signal when the active player changes or when that player's metadata is updated; value is empty QString when no player is active.

---

**REQ-F-017: MprisService Artwork URL Property**
MprisService SHALL expose an `activeArtUrl` Q_PROPERTY (const QString&, read-only, NOTIFY signal) providing the artwork URL for the active player's current track.

*Acceptance Criterion*: Verify that `activeArtUrl` returns the active player's `MprisPlayer::art_url` field; property emits its NOTIFY signal when the URL changes or the active player changes; value is empty QString when no artwork URL is present.

---

**REQ-F-018: MprisService Position Property**
MprisService SHALL expose an `activePosition` Q_PROPERTY (qint64, read-only, NOTIFY signal) for the active player's current playback position in microseconds.

*Acceptance Criterion*: Verify that `activePosition` returns the active player's `MprisPlayer::position` in microseconds; a `Seeked` signal is applied on the next GUI event-loop turn and emits the NOTIFY signal once; value is 0 when no player is active. In an idle-shell integration test, the correction SHOULD be visible within 100 ms at the 95th percentile over at least 20 samples.

---

**REQ-F-019: MprisService Track Length Property**
MprisService SHALL expose an `activeLength` Q_PROPERTY (qint64, read-only, NOTIFY signal) for the active player's current track duration in microseconds.

*Acceptance Criterion*: Verify that `activeLength` returns the active player's `MprisPlayer::length` in microseconds; property updates within 500 ms of a new track being selected; emits NOTIFY signal on track change; value is 0 when no length is known or no player is active.

---

**REQ-F-020: MprisService Seek Capability Property**
MprisService SHALL expose an `activeCanSeek` Q_PROPERTY (bool, read-only, NOTIFY signal).

*Acceptance Criterion*: Verify that `activeCanSeek` returns the active player's `MprisPlayer::can_seek` boolean; property emits NOTIFY signal when the capability changes; value is false when no player is active.

---

**REQ-F-021: Position Tracking Using MPRIS Specification Approach**
WHILE the active player's playback status is "Playing" THEN the system SHALL maintain an accurate displayed position by:
(a) reading the initial MPRIS `Position` property once per track;
(b) advancing the displayed position locally using a monotonic clock and the player's `Rate` property;
(c) immediately applying corrections when the player emits a `Seeked` signal;
(d) freezing the displayed position when playback status changes to "Paused" or "Stopped";
(e) invalidating the previous track's position when the track ID changes and re-anchoring from a fresh `Position` read; 0 is used only until that read succeeds.

*Acceptance Criterion*: Verify that displayed position advances smoothly at 1x rate when `activePlaybackStatus == "Playing"` and Rate == 1.0; Paused freezes and `Seeked` re-anchors on the next GUI event-loop turn; a track-ID change invalidates the old position before the new track is rendered and requests a fresh Position value; displayed position never drifts more than 500 ms from ground truth over 60 seconds. Idle-shell integration latency SHOULD be ≤100 ms at the 95th percentile over at least 20 samples.

---

**REQ-F-022: Periodic Position Reconciliation**
WHILE playback is active (status is "Playing") THEN the system SHALL periodically reconcile the locally-advanced position with the player's ground-truth Position property to correct any accumulated drift.

*Acceptance Criterion*: Verify that a fresh Position property read is requested from the MPRIS player at an interval between 10 and 30 seconds (exact interval left to Design); reconciliation occurs at least once per 60 seconds of continuous playback; reconciliation corrects a simulated 1-second drift to within ±100 ms within one reconciliation cycle.

---

**REQ-F-023: Position Update Rate**
WHILE the widget is visible (not occupancy-hidden, per REQ-F-046/REQ-F-061) AND the active player's status is "Playing" THEN the widget's displayed position SHALL update at a frequency of at least 2 times per second (≤500 ms between updates).

*Acceptance Criterion*: Verify that the widget's local elapsed-position display re-renders at least 2 times per second (every ≤500 ms) while visible and `activePlaybackStatus == "Playing"`; `MprisService::activePosition`'s own NOTIFY cadence and the widget's on-screen tick rate may be decoupled (the widget may sample the property on its own 2 Hz timer rather than requiring the property itself to emit at 2 Hz); a Seeked correction (REQ-F-018) is still reflected immediately, independent of this tick rate; CPU use at 2 Hz is measurably lower than a naive 10 Hz implementation and does not cause visible frame-rate impact elsewhere in the shell.

---

**REQ-F-024: Rate Property Handling**
WHEN the active player's playback Rate property is not 1.0 (e.g., fast-forward or slow-motion) THEN the displayed position SHALL advance at the rate multiplied by the normal playback speed.

*Acceptance Criterion*: Verify that with Rate == 2.0, displayed position advances at 2x real-time; with Rate == 0.5, advances at 0.5x; position remains accurate to within ±100 ms of the ground-truth Position when reconciled. Pausing is determined by `PlaybackStatus`, as required by MPRIS; a non-conforming Rate value of 0 is sanitized to 1.0 but never advances position while `PlaybackStatus != "Playing"`.

---

**REQ-F-025: Player Metadata Change Detection**
WHEN the active player emits a metadata change event (MPRIS `PropertiesChanged` signal for `Metadata`) THEN all affected widget properties (title, artist, album, artwork, length, position) SHALL update to reflect the new metadata within 500 ms.

*Acceptance Criterion*: Verify that a track change (new Metadata with different track ID) triggers updates to all visible fields; displayed changes are consistent (no torn reads, e.g., old title with new artwork); update latency is ≤ 500 ms from the D-Bus signal emission time.

---

### Artwork Caching and Resolution (REQ-F-026 to REQ-F-035)

**REQ-F-026: Artwork Cache Initialization**
The system SHALL establish a persistent local cache for downloaded artwork.

*Acceptance Criterion*: Verify that the cache directory exists (e.g., `~/.cache/holonight/mpris-artwork/` or similar) after the first widget instantiation; directory is created with appropriate permissions; cache persists across shell restarts.

---

**REQ-F-027: Artwork URL Protocol Support**
The artwork cache SHALL handle `file://`, `http(s)://`, and base64-encoded `data:` artwork URLs.

*Acceptance Criterion*: Verify that a `file:///path/to/album.png` URL is resolved to the local file path and verified readable; an `https://example.com/art.jpg` URL is fetched via HTTPS; a `data:image/<type>;base64,<payload>` URL is base64-decoded and decoded/cached the same as a fetched file (this is the common form reported by local-file players such as Haruna and VLC, which embed cover art rather than exposing a `file://` path); a `data:` payload that is not base64-encoded, is not a decodable image, or exceeds the per-file size limit (REQ-F-029) resolves the same as missing artwork (fallback applies); other unsupported schemes (e.g. non-existent custom schemes) are silently skipped (treated as missing artwork, fallback applies).

---

**REQ-F-028: Artwork Cache Keying**
The artwork cache SHALL use the exact raw artwork URL as its cache identity, hashed to obtain a safe filename. Track ID SHALL NOT be part of the persistent cache key.

*Acceptance Criterion*: Verify that the same artwork URL used by different tracks resolves to the same cache entry without re-fetching; different URLs produce different SHA-256 filenames; stale asynchronous completions are rejected by comparing the completed request URL/generation with the manager's current request, independently of cache identity.

---

**REQ-F-029: Per-File Size Limit**
The artwork cache SHALL reject or skip artwork files that exceed a maximum size threshold (e.g., 5 MB).

*Acceptance Criterion*: Verify that an artwork file larger than the maximum is not downloaded/cached; the download is terminated if it exceeds the limit in-progress; `qrc:/HolonightShell/assets/media/media-placeholder.svg` is displayed (REQ-F-051); no error is logged that would alarm the user.

---

**REQ-F-030: Total Cache Budget and LRU Eviction**
The artwork cache SHALL enforce a maximum total cache size (e.g., 50 MB) with least-recently-used (LRU) eviction.

*Acceptance Criterion*: Verify that when total cache size exceeds the budget, the oldest-accessed entry is deleted; subsequent cache operations succeed; total cache size remains ≤ budget after eviction; no data corruption occurs during eviction.

---

**REQ-F-031: Artwork Decoding at Display Size**
The artwork cache SHALL decode and serve images at approximately the size the widget will display them (not full original resolution).

*Acceptance Criterion*: Verify that artwork is decoded/stored at no larger than 512x512 logical pixels; cached file size is proportional to this resolution (e.g., 512x512 PNG occupies ~100–200 KB); displayed artwork is sharp and not pixelated at the widget's final render size.

---

**REQ-F-032: Artwork Cache Scoped to Desktop Widget**
The artwork cache is used ONLY by this new desktop widget and does NOT affect or replace artwork resolution for the existing topbar MPRIS pill.

*Acceptance Criterion*: Verify that the topbar pill continues to use its existing artwork logic (or none, if it currently displays only the app icon); changing the desktop widget's artwork cache settings or clearing the cache does not affect topbar rendering; the two widgets are independent code paths.

---

**REQ-F-033: Remote Artwork Fetching (No Config Option in v1)**
The system SHALL fetch remote (http/https) artwork by default; there is deliberately NO configuration option to disable remote artwork fetching in this version.

*Acceptance Criterion*: Verify that an `https://example.com/art.jpg` URL is fetched automatically whenever a track with that URL is encountered; no `fetch_remote_artwork` or equivalent config key exists in the widget's config struct; if a user wishes to disable this, it requires code changes (deferred to a future version).

---

**REQ-F-034: Artwork Fetch Timeout and Error Handling**
WHEN a remote artwork URL is fetched THEN if the fetch times out or returns an error THEN the system SHALL stop waiting and fall back to displaying the app icon.

*Acceptance Criterion*: Verify that a fetch timeout (e.g., 10 seconds) does not hang the widget; an HTTP 404 or network error is caught and does not corrupt the cache; the bundled media placeholder is selected when the failure is delivered; no error popups or logged warnings disrupt the user (errors are logged at debug level).

---

**REQ-F-035: Artwork Cache Persistence Across Metadata Changes**
WHEN the same artwork URL is encountered again, including for a different player or track, THEN the artwork cache SHALL reuse the cached file without re-fetching.

*Acceptance Criterion*: Verify that a cache hit (exact URL already present) results in artwork appearing within 50 ms (file read only, no network); network traffic is not initiated for a cache hit; cache lookup is O(1) or O(log n) in the number of cached entries.

---

### Widget Configuration (REQ-C-001 to REQ-C-003)

**REQ-C-001: Cross-Repository Config Package Change**
The `holonight-config` package (located in the separate `holonight-settings` repository at `libs/holonight-config/`) SHALL be updated to define a new `WidgetType::Mpris` enum value and an associated config struct containing the MPRIS-specific settings.

*Acceptance Criterion*: Verify that after updating `holonight-config` source and rebuilding/installing it, `holonight-shell`'s CMake configure step can `find_package(HolonightConfig CONFIG REQUIRED)` and successfully link/reference the new `WidgetType::Mpris` enum and associated config struct; a build without the updated package fails with a clear "Mpris not found in WidgetType" or similar message.

---

**REQ-C-002: Pause-Hide Duration Configuration**
The MPRIS widget's config struct SHALL include a `pause_hide_minutes` field (int, default ~10 minutes) specifying how long the widget remains visible in the Paused state before auto-hiding.

*Acceptance Criterion*: Verify that a user can set `pause_hide_minutes = 15` in a `[[widget]]` TOML block and the widget hides 15 minutes after entering Paused state (not before); default value (if omitted) is ~10 minutes; invalid values (negative, zero) are rejected or clamped to a safe range (e.g., 1–60) at config parse time.

---

**REQ-C-003: Widget Config Opt-In and No Auto-Backfill**
The MPRIS widget is enabled ONLY when a user explicitly adds a `[[widget]]` block with `type = "mpris"` to `config.toml`; there is NO auto-backfill, no default widget instance created, no settings-GUI page for authoring config in this version.

*Acceptance Criterion*: Verify that a fresh `config.toml` without a `[[widget]]` block of type "mpris" does not instantiate or display the widget; adding a `[[widget]] type = "mpris" position = "center-center"` (and installing the updated `holonight-config` package) causes the widget to appear on next shell restart; removing the `[[widget]]` block disables the widget without requiring a settings GUI or code changes.

---

### Visual and Behavioral States (REQ-F-036 to REQ-F-060)

**REQ-F-036: Playing State Appearance**
WHEN the active player's playback status is "Playing" THEN the widget SHALL be fully visible at normal opacity and all metadata/progress elements SHALL be displayed at full brightness.

*Acceptance Criterion*: Verify that `root.opacity == 1.0` (or 0.95–1.0, implementation choice) when `activePlaybackStatus == "Playing"`; all text and imagery is at full color/brightness; transition to this state occurs within one animation frame (~16 ms at 60 Hz).

---

**REQ-F-037: Paused State Appearance**
WHEN the active player's playback status is "Paused" THEN the widget SHALL display at a visibly reduced opacity (e.g., 50–60%) with a pause indicator visible on the player icon.

*Acceptance Criterion*: Verify that `root.opacity` is set to 0.5–0.6 when `activePlaybackStatus == "Paused"`; a pause glyph (two vertical bars or equivalent) is visible and distinct; the transition is animated over ~300 ms; the widget remains interactive-free (pointer-transparent, as per REQ-U-001).

---

**REQ-F-038: Stopped State Appearance**
WHEN there is no active player, or the active player's status is "Stopped" THEN the widget SHALL be fully hidden (opacity 0, not accepting pointer input, not occupying the workspace for accessibility/focus purposes).

*Acceptance Criterion*: Verify that `root.opacity == 0` and `root.visible == false` when `hasActivePlayer == false` or `activePlaybackStatus == "Stopped"`; QML root is hidden within one animation frame; hidden state persists until a new player becomes active or status changes to Playing/Paused.

---

**REQ-F-039: Auto-Hide Timer on Paused State**
WHEN the playback status changes to "Paused" THEN the system SHALL start an internal timer set to the configured `pause_hide_minutes` value.

*Acceptance Criterion*: Verify that after entering Paused, pause-duration tracking begins at 0 and increases in real time; the tracked duration is observable (e.g. via a test seam, for verification); the widget does not hide before `pause_hide_minutes` has elapsed. Per REQ-F-044, this tracking does not persist across a shell restart — a restart always begins tracking fresh, even if the player was already paused before the restart.

---

**REQ-F-040: Pause-Hide Timer Expiration**
WHEN the pause-hide timer expires (widget has been Paused for `pause_hide_minutes`) THEN the widget SHALL fade to fully hidden (opacity 0, visible false), the same as the Stopped state.

*Acceptance Criterion*: Verify that after exactly pause_hide_minutes in Paused state, the widget begins fading (animated transition); opacity reaches 0 within 1 second; widget is fully hidden (visible false) after animation completes; widget does not reappear until playback resumes.

---

**REQ-F-041: Pause Timer Reset on Playback Resume**
WHEN the active player's status changes from "Paused" to "Playing" THEN the pause-hide timer SHALL be cancelled and the widget SHALL fade to fully visible.

*Acceptance Criterion*: Verify that if the timer would expire in 30 seconds but playback resumes in 10 seconds, the widget does not hide at the 30-second mark; instead, widget fades to visible (opacity 1.0) within 300 ms; timer is cleared from memory so no spurious hide occurs later.

---

**REQ-F-042: Pause Timer Reset on Active-Player Change**
WHEN `MprisService`'s active player changes, for any reason — automatic priority-based reselection, a different player starting or stopping, or a manual selection performed by some other shell surface (e.g. a future popup player-switcher) or an external tool (e.g. `playerctl`) — THEN the pause-hide timer SHALL be cancelled and, if the newly active player is itself Paused, a fresh timer SHALL be started for it.

*Acceptance Criterion*: Verify that switching from a Paused player to a different player (even if the new player is also Paused) cancels the old timer and starts a fresh one at the full `pause_hide_minutes` duration (not an accumulated/carried-over value); no timer collision or double-hide occurs; this holds regardless of *why* the active player changed — the widget has no visibility into or dependency on the cause.

---

**REQ-F-043: Widget Provides No Player-Selection Mechanism**
The widget SHALL NOT provide any interactive means to select or switch the active MPRIS player — consistent with its strictly read-only, pointer-transparent nature (REQ-U-001 to REQ-U-003). It only ever reflects whichever player `MprisService` currently reports as active.

*Acceptance Criterion*: Verify a grep of the widget's QML source finds no player-selection UI element (button, list, switcher, or similar); verify that programmatically changing `MprisService`'s active player (e.g. via the existing `MprisTestSeed` test seam, or by stopping the currently active player so priority-based selection picks another) updates the widget's displayed title/artist/artwork within the REQ-F-025 latency bound (≤500 ms), with zero interaction on the widget itself.

---

**REQ-F-044: No Timer Persistence Across Restart**
If the shell restarts or the widget is destroyed and recreated THEN the pause-hide timer state SHALL not persist; the timer SHALL restart on the next instantiation.

*Acceptance Criterion*: Verify that killing and restarting the shell while a player is Paused (and timer is running) results in a fresh timer starting; the widget does not hide prematurely or unexpectedly if the shell is restarted 2 minutes into a 10-minute pause-hide timer.

---

**REQ-F-045: Occupancy Gating Overrides Visual State**
WHEN a monitor's visible workspace transitions from empty to occupied (windows appear) THEN the widget SHALL be hidden regardless of playback status, even if the player is "Playing".

*Acceptance Criterion*: Verify that with a Playing player, the widget is visible; if a window is opened on the monitor, the widget fades to hidden (opacity 0, visible false) within one animation frame; when the window is closed and the workspace becomes empty again, the widget fades back to its previous state (Playing = visible, Paused = dimmed).

---

### Occupancy Gating and Monitor Visibility (REQ-F-046 to REQ-F-050, REQ-F-061 to REQ-F-062)

**REQ-F-046: Visibility Bound to Monitor Workspace Occupancy**
The widget SHALL be visible only when its target monitor's currently-visible workspace has no windows (occupancy gate).

*Acceptance Criterion*: Verify that `MonitorOccupancyService` reports zero windows on the visible workspace → widget visibility follows playback state (Playing = visible, Paused = dimmed, Stopped = hidden); when MonitorOccupancyService reports ≥1 windows, widget is hidden (opacity 0, visible false) overriding playback state.

---

**REQ-F-047: Surface Persistence Across Occupancy Toggle**
WHEN the occupancy gate hides the widget THEN the wlr-layer-shell surface SHALL remain mapped and cached (not destroyed); only the QML root's visibility SHALL toggle.

*Acceptance Criterion*: Verify that hiding via occupancy gate does not trigger `QQuickView::hide()` or `wl_surface` destruction; instead, `root.visible = false` is applied; when occupancy clears, the surface is still mapped (no 1×1 fallback surface or reconstruction); widget reappears without re-layout or flicker within one animation frame.

*Note*: this requirement concerns the Wayland surface object only. It does not imply the widget's own background processing keeps running while hidden — see REQ-F-061.

---

**REQ-F-048: Occupancy Service Integration**
The widget SHALL receive occupancy updates from the existing `MonitorOccupancyService` for the monitor(s) on which it is configured to appear.

*Acceptance Criterion*: Verify that the widget connects to `MonitorOccupancyService::occupancyChanged(monitorName)` signals for the target monitor; updates are received asynchronously within 100 ms of a workspace change; occupancy state is correctly mapped to visibility (empty = apply playback state, occupied = hide).

---

**REQ-F-049: Multi-Monitor Support**
The widget SHALL respect its configured `monitors` list (from WidgetDefinition), appearing on all listed monitors independently.

*Acceptance Criterion*: Verify that a widget with `monitors = ["DP-1", "DP-2"]` creates two independent surface instances, one per monitor; each instance has independent occupancy gating and playback state tracking; if DP-1's workspace is occupied but DP-2's is empty, the DP-2 instance is visible and DP-1's is hidden.

---

**REQ-F-050: Position Anchoring**
The widget SHALL be positioned according to its configured `position` field from WidgetDefinition (one of 9 anchor values: LeftTop, CenterTop, RightTop, LeftCenter, CenterCenter, RightCenter, LeftBottom, CenterBottom, RightBottom).

*Acceptance Criterion*: Verify that a widget with `position = "center-bottom"` is centered horizontally and anchored to the bottom edge of the monitor; `position = "right-top"` anchors to top-right; configured margin (from WidgetsConfig.margin) is applied uniformly to all edges.

---

**REQ-F-061: Widget Presentation Work Suspends While Occupancy-Hidden**
WHILE a given monitor's instance of the widget is hidden by the occupancy gate (workspace occupied) THEN the system SHALL suspend all presentation work owned by that widget instance for that surface — sampling/redrawing its displayed position, sampling/comparing pause-elapsed-duration against its configured threshold, and any in-flight/pending artwork-fetch retry scheduling it owns directly — until that monitor's workspace becomes empty again. This mirrors the existing behavior of the Clock/TimeToEvent widgets, whose shared tick timer freezes while every surface is hidden.

This requirement is about observable effect, not implementation ownership: it does not mandate that position-advancement or pause-duration tracking be computed locally per widget instance. Any underlying state-tracking that is shared across consumers (e.g. centralized in `MprisService`, alongside its existing D-Bus player discovery, metadata monitoring, and active-player selection) is exempt and MUST keep running regardless of any individual desktop-widget instance's visibility, the same as it already must for the topbar pill. What suspends is each widget instance's own act of sampling, comparing against its own threshold, and redrawing from that shared state — never the shared state itself.

*Acceptance Criterion*: Verify that when a monitor's workspace becomes occupied, that widget instance stops sampling/redrawing (no per-tick property push or repaint occurs for that instance) within one animation frame; meanwhile any shared tracking this feature relies on (position, pause-elapsed-duration) continues to be maintained correctly in the background — verifiable via the topbar pill, which remains live and unaffected on any monitor, and via the widget instance itself showing an up-to-date (not stale) value immediately on the next reveal (REQ-F-062); when the workspace becomes empty again, the widget instance's sampling/redrawing resumes.

---

**REQ-F-062: Position and Metadata Resync on Reveal**
WHEN a widget instance transitions from occupancy-hidden to visible THEN the system SHALL immediately resynchronize its displayed elapsed position, artist/title/album, artwork, and pause-hide timer state (if the active player is Paused) from `MprisService`'s current values, rather than resuming from whatever was last displayed before hiding.

*Acceptance Criterion*: Verify that if the active player finishes one track and starts another entirely while a widget instance was occupancy-hidden, on reveal that instance displays the CURRENT track's metadata and position — never the stale track/position from before it was hidden, not even for a single rendered frame; if the active player changed from Paused to Stopped while hidden, the instance reveals already in the hidden-for-Stopped state (REQ-F-038) rather than briefly flashing the old Paused appearance.

---

### Input Handling and Pointer Transparency (REQ-U-001 to REQ-U-003)

**REQ-U-001: No Pointer Input Acceptance**
IF a pointer event (mouse click, touch, or stylus) is directed at the widget's surface region THEN the system SHALL NOT accept or intercept that input; the event SHALL pass through to any windows or surfaces below.

*Acceptance Criterion*: Verify that `QQuickView` does NOT consume pointer events (no implicit `wl_surface` input region set); Wayland input events directed at the widget's coordinates are not absorbed by the shell and reach applications below; clicking on the widget does not steal focus or prevent interaction with windows behind it.

---

**REQ-U-002: No Input Handler Components in QML**
The widget's QML component tree SHALL NOT contain any `MouseArea`, `TapHandler`, `WheelHandler`, or scroll handlers.

*Acceptance Criterion*: Verify that a grep of the widget QML source finds zero instances of `MouseArea {`, `TapHandler {`, `WheelHandler {`, or wheel-handling code; any hand-coded pointer interception is absent; QML linting (`qml-lint`) reports no interactive handler warnings for the widget tree.

---

**REQ-U-003: Keyboard Focus Not Accepted**
The widget's QML root and all descendants SHALL NOT accept keyboard focus (focusPolicy set to `Qt.NoFocus` or equivalent).

*Acceptance Criterion*: Verify that `root.focusPolicy == Qt.NoFocus`; no child element has `focus: true` or `focusPolicy != NoFocus`; the widget does not receive keyboard input even if Wayland keyboard focus is directed at its surface (wl_keyboard::enter is not triggered for the widget).

---

### Graceful Degradation (REQ-F-051 to REQ-F-060)

**REQ-F-051: Fallback on Missing Artwork**
IF artwork cannot be resolved (`art_url` is empty, malformed, unsupported, missing, oversized, corrupt, or a download fails) THEN the widget SHALL display the bundled `qrc:/HolonightShell/assets/media/media-placeholder.svg` image in place of artwork. The separate player-identity icon required by REQ-F-008 remains `image://icon/<activeDesktopEntry>`.

*Acceptance Criterion*: Verify that with `activeArtUrl` empty or unresolvable, `assets/media/media-placeholder.svg` appears in the artwork region at ≥256×256 logical pixels; no blank space or broken-image glyph is shown. Synchronous validation failures select it on the next GUI event-loop turn; asynchronous failures select it when the failed operation completes.

---

**REQ-F-052: Fallback on Missing Title**
IF the track title is empty or missing THEN the widget SHALL display the player's `identity` (friendly name, e.g., "VLC media player") in the title position.

*Acceptance Criterion*: Verify that with `activeTitle` empty, `activeIdentity` is displayed where the title would normally appear; identity text is styled consistently with title (same font size, positioning); fallback appears within one animation frame of title becoming empty.

---

**REQ-F-053: Graceful Missing Length**
IF the active player does not report a track length (mpris:length is absent or ≤ 0) THEN the widget SHALL NOT display a progress line or duration suffix (e.g., not "01:23 / --:--").

*Acceptance Criterion*: Verify that with `activeLength == 0`, the progress bar is not rendered; elapsed time is displayed as a bare "MM:SS" (not "MM:SS / 00:00" or "MM:SS / --:--"); layout does not reserve space for a progress bar, so remaining content shifts up to fill the space gracefully.

---

**REQ-F-054: Graceful Missing Album**
IF the track album is empty or missing THEN the widget SHALL NOT display an album row.

*Acceptance Criterion*: Verify that with `activeAlbum` empty (QString()), no album text element is rendered; layout does not reserve blank space for a missing album; next layout element (e.g., progress line) shifts up to fill the gap.

---

**REQ-F-055: Failed D-Bus Property Read Handling**
WHEN a D-Bus call to retrieve a newly-added property (position, length, art_url, album, can_seek) fails (timeout, introspection error, property not available) THEN the displayed value for that property SHALL remain unchanged from its last successful read; no optimistic/stale value updates until the next successful read.

*Acceptance Criterion*: Verify that if a Position property read times out, the displayed position continues to advance from its last known good value (if Playing) or remains frozen (if Paused); if an art_url fetch fails, the previous successfully resolved artwork remains displayed until the request conclusively fails, then the bundled media placeholder is selected; no visual flicker or transient blank state is introduced.

---

**REQ-F-056: Missing Player Capabilities**
IF the active player does not support a capability reported in the MPRIS spec (e.g., Seek is not present, or CanSeek is absent) THEN the widget SHALL treat that capability as false/unavailable and degrade gracefully.

*Acceptance Criterion*: Verify that a player without Seek support does not break progress bar rendering (QML does not crash, no null-pointer deref); canSeek is treated as false; progress line visual indicates non-seekability (REQ-F-010); no error is logged that requires user action.

---

**REQ-F-057: Artwork Cache Corruption Recovery**
IF an artwork file in the cache is corrupted or unreadable THEN the system SHALL skip that entry and fall back to displaying the app icon.

*Acceptance Criterion*: Verify that a corrupted PNG or invalid image file in the cache does not crash the image loader; the bundled media placeholder appears on the next GUI event-loop turn after detection; the corrupt entry is logged at debug level and removed before a later resolve may treat it as a hit; subsequent artwork fetches work normally.

---

**REQ-F-058: No Hung State on Metadata Absence**
The widget SHALL always display at least some content (artist, title, icon, or progress) even if several metadata fields are missing; never show a completely blank widget.

*Acceptance Criterion*: Verify that in the worst case (only player identity available, no title/artist/artwork/length), the player icon and identity string are displayed; widget height/width are non-zero (visually present on the desktop); layout does not collapse to an invisible state due to all content being missing.

---

**REQ-F-059: Mismatched Rate Property Handling**
IF the MPRIS Rate property is unavailable or returns an invalid value (negative, non-numeric) THEN the widget SHALL treat Rate as 1.0 (normal playback speed).

*Acceptance Criterion*: Verify that position advances at 1x speed if Rate is missing or invalid; no division-by-zero or runtime crash; fallback rate is applied silently without logging an alarming error.

---

**REQ-F-060: Seek Capability Default**
IF the MPRIS CanSeek property is missing or unavailable THEN the widget SHALL assume seeking is not supported (can_seek defaults to false) and display the progress line as non-interactive.

*Acceptance Criterion*: Verify that a player without CanSeek property results in `activeCanSeek == false`; progress bar (if displayed) is visually marked as non-seekable; no interactive seek is attempted; no error is logged for the missing property.

---

### Non-Functional Requirements (REQ-NF-001 to REQ-NF-003)

**REQ-NF-001: Performance Under Sustained Update Rate**
The widget's position display SHALL update at ~2 Hz (per REQ-F-023) without causing excessive CPU or render-thread blocking, and without memory leaks over extended playback (>8 hours).

*Acceptance Criterion*: Verify that sustained position updates at 2 Hz for 8 hours do not increase RSS memory by more than 10 MB (excluding artwork cache); frame rate remains ≥30 Hz even with multiple monitors displaying the widget; no process hangs or CPU spikes; CPU usage attributable to the widget's tick timer is negligible (near-zero) while occupancy-hidden, per REQ-F-061.

---

**REQ-NF-002: Responsive Metadata Changes**
Metadata updates (title, artist, album, artwork, position, length) from MPRIS players SHALL be reflected in the widget UI within 500 ms.

*Acceptance Criterion*: Measure the time from MPRIS D-Bus PropertiesChanged signal emission to visible UI update (pixel change) on the rendered surface; verify ≤ 500 ms over 10 consecutive metadata changes from different players.

---

**REQ-NF-003: Artwork Cache I/O Efficiency**
Cache lookups and hit operations SHALL complete in ≤ 50 ms; artwork fetch and decode operations (remote URL, full download) SHALL be non-blocking to the QML/rendering thread.

*Acceptance Criterion*: Verify that a cache hit (exact URL already present) is served from disk in ≤ 50 ms; artwork fetch/download operations run on a background thread (or via QNetworkAccessManager async); no frame-rate drops when a new artwork fetch begins; QML thread remains responsive for occupancy/state updates even during large artwork download.

---

## Non-Goals

The following are explicitly OUT OF SCOPE for this version and SHALL NOT be implemented:

- **No dominant-color artwork glow effect**: No automatic glow or background color derived from artwork dominant color.
- **No config option to disable remote artwork fetching**: Remote artwork (http/https) is always fetched in v1; deferring is allowed in a future version.
- **No settings-GUI authoring**: The holonight-settings app does not gain a widget configuration page in this version. All widget config is via hand-edited TOML only.
- **No topbar pill changes**: The existing topbar MPRIS "now playing" pill (`apps/shell/qml/Topbar/Mpris*.qml`) is not modified. The two widgets (topbar pill and desktop widget) remain independent.
- **No seek bar interactivity**: The progress line is a read-only visual indicator only. No click/drag to seek, no mouse-over tooltip showing seek time.
- **No shuffle/repeat/playlist display**: No controls or indicators for shuffle mode, repeat mode, or track list / playlist management. Widget shows only the currently-playing track's basic info.

---

## Constraints

**REQ-C-003 (reiterated)**: Successful implementation requires the `holonight-config` package (in the separate `holonight-settings` repository) to be updated first to define `WidgetType::Mpris` and its config struct, rebuilt, and installed. Only after that can `holonight-shell`'s CMake build see and use the new config types. This cross-repo build dependency must be acknowledged in the task breakdown and CI/build documentation.
