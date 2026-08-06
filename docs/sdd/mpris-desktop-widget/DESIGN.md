# MPRIS Desktop Widget — Design

Stage 2 (Architecture/Design) of the SDD cycle. Implements every requirement in `SPEC.md`
(REQ-F-001…062, REQ-C-001…003, REQ-U-001…003, REQ-NF-001…003). This document specifies the
implementation architecture; it deliberately does not contain code.

---

## 1. Overview

This feature adds a fourth wlr-layer-shell desktop widget — an ambient, read-only "now playing"
panel — alongside the existing Clock and TimeToEvent widgets managed by `WidgetManager`
(`libs/holonight-surfaces/src/WidgetManager.h`). It surfaces album art, track metadata, and a
locally-advanced playback-position readout sourced from the existing `MprisService` singleton
(`libs/holonight-services/src/mpris/`), which already powers the topbar "now playing" pill and is
reused here unmodified for player discovery/selection. Three genuinely new pieces of machinery are
required beyond what Clock/TimeToEvent needed: (1) five new data fields end-to-end from
`MprisDbus` → `MprisPlayer` → `MprisService` Q_PROPERTYs (REQ-F-011…020), (2) a local,
signal-driven, occupancy-suspendable position-tracking clock (REQ-F-021…024, REQ-F-061/062) that
is a fundamentally different control-flow shape than the tick-timer string computation
Clock/TimeToEvent use, and (3) a new, bounded, LRU-evicting on-disk artwork cache
(REQ-F-026…035, REQ-NF-003) with no precedent of its own shape in this codebase. Config plumbing
crosses into the sibling `holonight-settings` repository (REQ-C-001…003), which this design
specifies but cannot implement from here.

---

## 2. Components

### 2.1 Backend data model (extends existing files, `libs/holonight-services/src/mpris/`)

| File | Change | Requirements |
|---|---|---|
| `MprisPlayer.h` | Add `QString album`, `QString art_url`, `qint64 position{0}`, `qint64 length{0}`, `bool can_seek{false}` fields to the existing plain struct. | REQ-F-011…015 |
| `MprisMetadata.h`/`.cpp` | Extend `Fields` (returned by `extractFields`) with `album`, `art_url`, `length`; parse `xesam:album` (string), `mpris:artUrl` (string, stored verbatim, no scheme validation per REQ-F-012), `mpris:length` (int64, clamped to 0 if absent or ≤0 per REQ-F-014). No new unwrap helper needed — these are scalar variant reads, not the `a{sv}`/`as` shapes `unwrapDict`/`unwrapStringList` exist for. | REQ-F-011, REQ-F-012, REQ-F-014 |
| `MprisService.h`/`.cpp` | Add five Q_PROPERTYs: `activeAlbum`, `activeArtUrl`, `activePosition`, `activeLength`, `activeCanSeek`, each read-only with a NOTIFY signal, populated in `applyActiveSnapshot()` alongside the existing ten. Add `Position`/`CanSeek`/`Rate` handling to `applyPlayerProperties()` (mirroring the existing `CanGoNext` etc. pattern) and a `Seeked` signal path (see 2.2). | REQ-F-016…020 |
| `MprisDbus.h`/`.cpp` (`IMprisDBus`/`SystemMprisDBus`/`FakeMprisDBus`) | Add a `connectSeeked(service, receiver, slot)` / `disconnectSeeked(...)` pair mirroring the existing `connectPropertiesChanged` pair, subscribing to `org.mpris.MediaPlayer2.Player.Seeked(x)`. `FakeMprisDBus` gains `emitSeeked(service, positionUs)` for tests. No new blocking read method is needed for `Position` beyond the existing `getAllPlayerProperties` — REQ-F-021(a)'s "read once per track" and REQ-F-022's periodic reconciliation both reuse it. | REQ-F-013, REQ-F-021(c), REQ-F-022 |

`MprisPlayerSelector` and `MprisActivityTimestamp` are **reused as-is** — the spec's Context
section and the task brief both confirm the priority-selection algorithm needs no change; the new
fields ride along in the `MprisPlayer` struct the selector already treats as an opaque snapshot.

### 2.2 Position- and pause-duration-tracking (revised: owned by `MprisService`, not the widget
manager — see §5b, pivoted from the original design during review)

**No `MprisPositionTracker` class exists.** Position advancement, drift reconciliation, and
pause-elapsed-duration tracking are all private state/behavior inside `MprisService` itself,
because there is exactly one process-global active player (§5b) — these are single global facts,
not per-monitor facts, and duplicating them once per visible surface would let the copies drift
apart from each other for no benefit.

- **New private state on `MprisService`** (`libs/holonight-services/src/mpris/MprisService.h`/`.cpp`):
  - `qint64 position_mark_us_{0}` — the last ground-truth `Position` read (initial per-track read,
    a `Seeked` correction, or a reconciliation-timer read), REQ-F-021(a)/(c).
  - `QElapsedTimer position_mark_elapsed_` — (re)started every time `position_mark_us_` is set from
    a fresh ground-truth read; while `PlaybackStatus == "Playing"` its `elapsed()` is the basis for
    local extrapolation.
  - `double active_rate_{1.0}` — last-known `Rate`, sanitized to 1.0 before storage (REQ-F-059) so
    nothing downstream has to re-sanitize (mirrors the old tracker's contract, now upheld by
    `MprisService` itself since it owns the raw D-Bus property).
  - `QElapsedTimer paused_since_` — (re)started the instant `PlaybackStatus` transitions *into*
    `"Paused"`; invalidated (`invalidate()`) the instant it leaves `"Paused"` for any reason (status
    change to Playing/Stopped, or the active player changing entirely). This mark is set inside the
    same `applyPlayerProperties()`/`reselectActivePlayer()` dispatch that already runs
    unconditionally for every `PropertiesChanged` signal regardless of any widget's existence or
    visibility — so maintaining it costs nothing extra and is deliberately **not** gated by
    `position_tracking_refcount_` below (see the merged §5b rationale for why this one field is the
    exception).
  - `int position_tracking_refcount_{0}` — the count of configured `MprisWidgetManager` consumers,
    independent of whether their surfaces are currently occupancy-hidden.
  - `QTimer* reconcile_timer_` — REQ-F-022's periodic drift-correction timer (interval 20 s, the
    mid-point of REQ-F-022's 10–30 s band); runs only while `position_tracking_refcount_ > 0 &&
    active_playback_status_ == "Playing"` (paused/stopped positions do not drift, so reconciling
    them is pure waste).

- **Interest-counting API** (public):
  - `[[nodiscard]] PositionTrackingHandle acquirePositionTracking();` — see below for the
    `PositionTrackingHandle` shape and the RAII-vs-plain-methods decision (§4.1, §5b). Increments
    `position_tracking_refcount_`; on a 0→1 transition, immediately issues one fresh `Position`
    read to re-anchor `position_mark_us_`/`position_mark_elapsed_` (so a caller that just started
    watching never extrapolates from a stale mark) and starts `reconcile_timer_` if the player is
    Playing.
  - `releasePositionTracking()` is **private**, reachable only via `PositionTrackingHandle`'s
    destructor (a `friend`) — see §5b for why this is deliberately not a symmetric public method.
    Decrements the refcount; on a →0 transition, stops `reconcile_timer_` (position extrapolation
    itself simply stops being read by anyone once refcount is 0 — there is nothing else to tear
    down).

- **Read accessors**:
  - `activePosition()` (existing `Q_PROPERTY`, REQ-F-018 — unchanged signature) is now
    computed-on-read rather than a stored+diffed value: while `position_tracking_refcount_ > 0 &&
    active_playback_status_ == "Playing"`, it returns `position_mark_us_ +
    static_cast<qint64>(position_mark_elapsed_.elapsed() * active_rate_ * 1000)`, clamped to
    `[0, active_length_]` when length is known (REQ-F-021(b)). Otherwise (refcount is 0, or status
    is not Playing) it returns `position_mark_us_` unextrapolated — i.e. the last-known raw
    ground-truth value, exactly the cheap, no-interpolation behavior `MprisService` already had for
    every other property before this feature existed. A `Seeked` signal (§2.1) always updates
    `position_mark_us_`/resets `position_mark_elapsed_` and emits `activePositionChanged`
    on the next GUI event-loop turn, **regardless of refcount** — REQ-F-018's event-ordering and
    wording is about ground-truth correctness, not about the presentation-only local-extrapolation
    machinery REQ-F-061 explicitly permits suspending.
  - `[[nodiscard]] qint64 activePauseElapsedMs() const;` — a **plain public method, not a
    `Q_PROPERTY`** (see §4.1 for the reasoning: only `MprisWidgetManager` C++ code polls this, on
    its own 2 Hz tick, and it would gain nothing from `Q_PROPERTY`/NOTIFY machinery no QML binding
    would ever use). Returns `paused_since_.isValid() ? paused_since_.elapsed() : 0`. Reset
    semantics, stated exactly per REQ-F-039/REQ-F-044's revised acceptance criteria: MUST reset
    (`invalidate()`) the instant `PlaybackStatus` leaves `"Paused"`; MUST reset the instant the
    active player changes (REQ-F-042, even if the newly-active player is itself already Paused —
    that starts a **fresh** mark, never an inherited one); MUST NOT persist across a shell restart —
    trivially true, since `paused_since_` is transient in-memory `QElapsedTimer` state,
    default-invalid at construction, with no serialization path anywhere in this design.

  This state is pure private bookkeeping inside an existing class, not a new pure/unit-testable
  standalone type the way the original `MprisPositionTracker` was designed to be — its correctness
  is now exercised through `MprisService`'s existing `FakeMprisDBus`-backed test seam (`emitSeeked`,
  property-change injection) rather than a dedicated tracker-only test target.

### 2.3 Widget surface manager (new sibling class — see §5a for the fork decision)

- **`MprisWidgetManager`** (new, `libs/holonight-surfaces/src/MprisWidgetManager.h`/`.cpp`),
  subclasses `PerMonitorLayerManager` directly (not `WidgetManager`). One instance per
  `WidgetDefinition` of `type == WidgetType::Mpris`, covering all monitors that definition targets
  — exactly the existing one-manager-per-definition contract `ShellApplication::rebuildWidgets()`
  already uses for Clock/TimeToEvent.
  - Constructor signature mirrors `WidgetManager`'s: `(LayerShell&, WidgetDefinition, int margin,
    int index, QList<QStringList> position_blockers, MonitorOccupancyService*, MprisService*,
    QObject* parent)` — one extra `MprisService*` dependency injected by `ShellApplication`
    (already constructed and available as the `mpris_` member).
  - `layerConfig()`: same `layer_bottom`, `namespace_name = "widget"`,
    `extra_flags = Qt::WindowTransparentForInput` as `WidgetManager::layerConfig()` — this is the
    mechanism that already satisfies REQ-U-001 for Clock/TimeToEvent (confirmed by reading
    `WidgetManager::layerConfig()`; the empty input region is a Qt window-flag effect at surface
    creation, not something `WidgetManager` sets and unsets per hide/show). Reused verbatim.
  - `configureSurface()`: same anchor/size/margin/exclusive-zone-(-1)/`configured()`-guarded
    first-reveal pattern as `WidgetManager::configureSurface()`. Uses a **new, MPRIS-specific**
    placement — see §4.3 on `WidgetSurfacePolicy` — because the fixed 460×200 `kWidgetWidth`/
    `kWidgetHeight` used by Clock/TimeToEvent is too short to hold ≥256px artwork (REQ-F-002) plus
    two text rows and a progress bar without clipping.
  - `qmlSource()`: loads a new `qrc:/HolonightShell/Widgets/MprisWidgetSurface.qml` root (kept
    **out of** the existing `WidgetSurface.qml` Loader-dispatch — seeded fields differ so
    drastically, per-frame, from Clock/TimeToEvent's string-push model that folding it into the
    same file would mean the required/plain-property split documented in
    `WidgetSurface.qml`'s comment grows a third, signal-driven branch; a dedicated root keeps that
    file's existing contract intact). Its `initial_properties` seed `barMonitorName` only —
    everything else arrives via live property pushes (see §3) rather than `setInitialProperties`,
    because MPRIS content changes continuously while the surface is mapped, unlike Clock/TTE's
    "seed once, then tick-push" model.
  - `shouldCreateSurface()`: identical monitor/position-collision logic, reusing
    `widgetTargetsMonitor`/`widgetBlockedOnMonitor` from `WidgetSurfacePolicy.h` — no changes
    needed there.
  - Occupancy handling: connects to `MonitorOccupancyService::occupancyChanged` exactly like
    `WidgetManager::onOccupancyChanged`/`applyVisibility` — pushes `contentVisible` and leaves QML
    as the sole owner of its `visible` binding; it never hides the `QQuickView` (REQ-F-047). On reveal, calls `resyncSurface(monitor)` (§2.4) instead of
    replaying a cached string (REQ-F-062).
  - Per-surface local tick: a single **shared** `QTimer` at 500 ms (2 Hz, REQ-F-023) drives
    `onPositionTick()`, which — for every monitor whose surface is currently visible — reads
    `MprisService::activePosition()` directly (a plain property/method read on the shared
    singleton — no local tracker object of any kind) and pushes it into that surface's QML root. On
    the same tick it also reads `MprisService::activePauseElapsedMs()` and compares it against that
    monitor's own `mpris.pause_hide_minutes * 60'000`; crossing the threshold drives the
    Paused-and-hidden-by-timeout presentation state (§5b, merged with the former §5d). The timer
    runs only while `anySurfaceVisible()` (REQ-F-061), mirroring `WidgetManager::
    updateTimerState()`'s freeze/resume pattern, but the *payload* differs (position sampling +
    pause-threshold comparison + metadata push instead of string recomputation).
  - **No reconciliation timer of its own.** REQ-F-022's periodic drift-correction timer now lives
    entirely inside `MprisService` (§2.2), gated by `MprisService`'s own interest-count, not
    anything `MprisWidgetManager` schedules — with tracking centralized process-wide there is
    exactly one reconciliation timer in the whole process regardless of how many monitors or
    `[[widget]] type = "mpris"` definitions exist, so the old "one call per definition" multi-manager
    concern (formerly Known Risk #4) does not arise at all (see §7).
  - **No pause-hide timer of its own.** There is no `QTimer` (single-shot or otherwise) per monitor
    for REQ-F-039…044 — "the timer" is just the comparison the 2 Hz tick already performs above,
    against `MprisService`'s centrally-tracked, never-reset-by-occupancy pause-elapsed duration; see
    the merged §5b for the full reasoning, including the on-reveal semantics (REQ-F-062).
  - **Interest-counting the shared tracking**: each configured `MprisWidgetManager` holds one
    `MprisService::PositionTrackingHandle` for its entire lifetime. Occupancy only starts/stops that
    manager's 2 Hz presentation timer; it never releases shared tracking interest. This preserves
    one process-global 20-second reconciliation timer while at least one configured MPRIS widget
    exists, avoids one timer per monitor, and guarantees a hidden surface can reveal from maintained
    state even if its immediate re-anchor read fails. Ordinary manager destruction, including config
    reload teardown, releases the handle automatically.

### 2.4 Artwork cache (new, no existing precedent of this exact shape — see §5c)

- **`MprisArtworkCache`** (new, `libs/holonight-services/src/mpris/MprisArtworkCache.h`/`.cpp`).
  A plain (non-QObject, non-QML-exposed) class owned directly by `MprisWidgetManager` — **not** a
  new QML singleton, so the `ShellApplication` two-file registration gotcha does not apply to it
  (flagged explicitly regardless, in §7, in case a future change exposes it to QML). One instance
  is shared across all `MprisWidgetManager`s in the process (there is normally only one MPRIS
  widget definition, but nothing prevents a user from configuring two with different `position`
  values targeting different monitors — sharing one cache instance avoids duplicate downloads of
  the same artwork for that case). Constructed once in `ShellApplication`, passed by reference/
  pointer into each `MprisWidgetManager`.
  - Public interface:
    - `void resolve(const QString& art_url, std::function<void(QString)> on_ready)`
      — asynchronous and keyed solely by the exact raw URL; `on_ready` is invoked on the GUI thread
      (queued) exactly once with a local file path or an empty path on error.
      A synchronous cache-hit still completes via the same queued callback path for a uniform
      caller contract (never a reentrant same-stack-frame callback).
      Shared work is never cancelled by an individual manager. Managers suppress stale presentation
      updates using captured URL plus a monotonically increasing local generation.
  - Internals:
    - Cache root: `QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) +
      "/holonight-shell/mpris-artwork/"` — following the **launcher cache's** precedent
      (`DesktopEntryCache`'s db path helper in `LauncherService.cpp`,
      `.../holonight-shell/launcher.db`) rather than the weather cache's manual
      `XDG_CACHE_HOME`-read-plus-`/holonight/weather.json` path. Rationale in §5c.
    - Cache key: `QCryptographicHash::hash(url.toUtf8(), Sha256)` hex
      string, used as the on-disk filename (`<hash>.png`, always re-encoded to PNG at decode time
      regardless of source format, so file extension is trivially uniform) — satisfies REQ-F-028's
      URL-only key without a lookup table: the filename **is** the key, so "does this exist" is a
      single `QFileInfo::exists()` call, no index structure to keep in sync.
    - Eviction bookkeeping: **no SQLite index, no JSON sidecar** — a directory scan at
      `MprisArtworkCache` construction builds an in-memory `QList<Entry{path, size, QDateTime
      last_touch}>` from each file's `QFileInfo::lastModified()`. "Touch" (LRU freshness update)
      is an explicit `QFile::setFileTime(QDateTime::currentDateTime(),
      QFileDevice::FileModificationTime)` call on every cache hit (mtime, not atime — many systems
      mount `noatime`, mtime is portable and the file's *content* is genuinely being "used" by the
      read regardless of the mount option). After every write, total size is recomputed from the
      in-memory list (kept current incrementally, not rescanned) and oldest-mtime entries are
      deleted until `total ≤ 50 MB` (REQ-F-030's suggested budget, used as the concrete default —
      not user-configurable per REQ-F-033's "no config option" precedent for this whole feature
      area). Per-file cap: 5 MB, enforced by aborting the `QNetworkReply` once
      `bytesReceived() > kMaxFileBytes` (REQ-F-029's "terminated if it exceeds the limit
      in-progress").
    - Decode pipeline: `QImageReader` with `setScaledSize(QSize(512, 512))` set **before** `read()`
      (not full-decode-then-scale) — REQ-F-031's "decode... at approximately the size" is exactly
      what `QImageReader::setScaledSize` is for, and it avoids allocating a full-resolution
      `QImage` for a typical 1000×1000+ source album-art JPEG only to immediately downscale it.
      Aspect ratio is preserved by computing the scaled size from the reader's `size()` first
      (`QImageReader::size()` reads just the header) and fitting it inside 512×512, not by forcing
      a square. The decoded `QImage` is saved to the cache file as PNG (`QImage::save(path,
      "PNG")`), then the same in-memory `QImage` (or a freshly re-read pixmap from the just-written
      file) is what's handed back — no second decode round-trip for the resolve that triggered the
      fetch.
    - `file://` URLs: read directly via `QFile`/`QImageReader` on a background thread (see below),
      still passed through the same size-cap/decode/cache-write pipeline (REQ-F-027) — a `file://`
      source is still worth caching at display resolution since the source file could be
      arbitrarily large (e.g. an embedded high-res cover art extracted by the player to a temp
      path).
    - `http(s)://` URLs: fetched via `QNetworkAccessManager` (one instance owned by
      `MprisArtworkCache`) with `QNetworkRequest::setTransferTimeout(10000)` (REQ-F-034's 10 s
      timeout) — this keeps the fetch itself asynchronous/event-loop-driven with no dedicated
      thread required for the network I/O portion (REQ-NF-003's "non-blocking to the QML/rendering
      thread" is satisfied because `QNetworkAccessManager` is inherently async; it never blocks the
      calling thread regardless of which thread it runs on).
    - `data:` URIs (revised during implementation, T-039 live testing — see §7's changelog entry):
      several real-world players report artwork this way rather than as `file://`, most notably
      Haruna and VLC when playing a local file with embedded (rather than sidecar) cover art. The
      original design silently skipped this scheme; it is now parsed directly from the raw
      `art_url` string (not via `QUrl`, since the base64 payload is not itself a hierarchical URL
      and `QUrl` would attempt to percent-decode characters that are meaningful base64, not URL
      escapes) — everything after the scheme's `,` separator is base64-decoded
      (`QByteArray::fromBase64`) if the header before it contains `;base64` (the only sub-form any
      observed MPRIS player emits), then handed to the same `decodeAndCache()` pipeline as any
      other source, still off the GUI thread via `QtConcurrent::run` (the base64 decode of a
      multi-megabyte inline payload is itself non-trivial CPU work, same rationale as the image
      decode step below). A payload that isn't base64-labelled, doesn't decode to a valid image,
      or exceeds the REQ-F-029 size cap resolves to `on_ready("")` — same graceful-failure contract
      as every other source.
    - Decode (the CPU-bound step, for `file://` reads, `data:` payloads, and completed HTTP
      downloads) runs via `QtConcurrent::run` (per this project's documented include-path gotcha:
      `#include <QtConcurrent/QtConcurrentRun>`) onto the global thread pool, with the completion
      `QFuture` bridged back to the GUI thread via `QFutureWatcher` — matching the async pattern
      already used elsewhere in `holonight-services` (e.g. the launcher's `ScanResult`/
      `QFutureWatcher<ScanResult>` pattern per project memory) rather than inventing a new
      thread-management primitive.
    - Other unsupported schemes (anything not `file`/`http`/`https`/`data`): `resolve()` detects
      the scheme synchronously via `QUrl::scheme()` and invokes `on_ready("")` immediately (still
      via the queued path) — REQ-F-027's "silently skipped."

### 2.5 QML (new files, `apps/shell/qml/Widgets/`)

| File | Responsibility | Requirements |
|---|---|---|
| `MprisWidgetSurface.qml` | New root item (analogous to `WidgetSurface.qml` but not sharing it — see §2.3). Declares plain properties for every pushed field, using QML `real` for microsecond position/duration. No interactive handlers; `focusPolicy: Qt.NoFocus`. Explicit Playing, Paused, Stopped, occupancy-hidden, and pause-timeout states own opacity and visibility transitions. | REQ-F-001…010, REQ-F-036…038, REQ-U-001…003 |
| `MprisArtwork.qml` | Artwork `Image` (source: `artworkPath` if non-empty, else `qrc:/HolonightShell/assets/media/media-placeholder.svg`) at ≥256px, `opacity: 0.5`, `fillMode: Image.PreserveAspectFit`, clipped to its layout bounds. The 32–48px player identity icon remains a separate `image://icon/<desktopEntry>` element. | REQ-F-002, REQ-F-008, REQ-F-051 |
| `MprisProgressBar.qml` | Thin (2–4px) `Rectangle`-based bar, `visible: lengthUs > 0` (REQ-F-053), width fraction `positionUs / lengthUs`, dimmed/lightened variant bound to `canSeek` (REQ-F-010, REQ-F-060). | REQ-F-005, REQ-F-010, REQ-F-053 |

All three files are read-only presentational QML; no new C++↔QML invokable surface beyond the
plain-property pushes `MprisWidgetManager` performs via `QQuickItem::setProperty` (same mechanism
`WidgetManager::pushClockStrings`/`root->setProperty("remainingText", ...)` already uses).

### 2.6 `WidgetSurfacePolicy` (modified, `libs/holonight-surfaces/src/WidgetSurfacePolicy.h`/`.cpp`)

`widgetSurfacePlacement(WidgetPosition, int margin)` currently hardcodes `kWidgetWidth = 460`,
`kWidgetHeight = 200` for every widget type. The MPRIS widget's minimum content (≥256px artwork +
title/artist row + album row + progress row + time row) does not fit in a 200px-tall surface.
Design: add an overload `widgetSurfacePlacement(WidgetPosition, int margin, int width, int
height)` that the two-argument version now delegates to with the existing constants — a
source-compatible, additive change. `MprisWidgetManager::configureSurface()` calls the
four-argument overload with new constants `kMprisWidgetWidth = 320`, `kMprisWidgetHeight = 420`
(320×420 comfortably fits 256×256 artwork inset with margins, four text/bar rows below, at the
existing widget font sizes — exact pixel tuning is an implementation-time visual pass, not a
Design-stage commitment).

### 2.7 `ShellApplication` (modified, `apps/shell/app/ShellApplication.h`/`.cpp`)

- `rebuildWidgets()`'s per-definition loop currently does
  `std::make_unique<WidgetManager>(...)` unconditionally. It changes to a `switch (def.type)`
  dispatch: `WidgetType::Clock`/`WidgetType::TimeToEvent` continue constructing `WidgetManager`;
  `WidgetType::Mpris` constructs `MprisWidgetManager`, passing `mpris_` (already a member,
  constructed before `rebuildWidgets()` ever runs) and `&mpris_artwork_cache_` (new member, see
  below).
- The `widget_managers_` container's element type changes from
  `std::vector<std::unique_ptr<WidgetManager>>` to `std::vector<std::unique_ptr<PerMonitorLayerManager>>`
  — both `WidgetManager` and `MprisWidgetManager` are `PerMonitorLayerManager` subclasses, so this
  is a supertype widening, not a redesign; nothing outside `rebuildWidgets()` iterates this vector
  by concrete type today (confirmed: `widget_managers_` has no other reader in `ShellApplication.h`
  beyond the destruction-order comment and the container declaration itself).
- New member: `MprisArtworkCache mpris_artwork_cache_;` — constructed inline (no D-Bus/async
  dependency at construction time; the directory scan it does at startup is a local filesystem
  operation, consistent with how other lightweight services in this constructor list are built).
  **Not** registered as a QML singleton (`reg(...)` is not called for it) — it has no QML-facing
  API, only `MprisWidgetManager` touches it from C++. This sidesteps the `QML_SINGLETON`
  registration gotcha entirely by design; see §7 for why it's still called out as a risk in case a
  future change adds a QML-facing debug/inspection surface for it.
- No new `reg(...)` call is needed for anything else in this feature — `MprisService` is already
  registered; nothing else introduced here is QML-exposed.
- **Construction-order note (new with this pivot):** `MprisService` gains
  `position_tracking_refcount_{0}` and a stopped `reconcile_timer_`, both inert at construction —
  it does not need to know at construction time whether any MPRIS widgets exist, so no reordering
  relative to `rebuildWidgets()` is required. The existing order (`mpris_` constructed once, early,
  before `rebuildWidgets()` ever runs; `rebuildWidgets()` may run many times afterward on config
  reload) is preserved unchanged. Each `MprisWidgetManager::PositionTrackingHandle` acquisition
  happens during manager initialization and lasts until manager destruction, so there is no
  ordering hazard between `MprisService`'s lifetime (spans the
  whole process) and any individual `MprisWidgetManager`'s lifetime (spans one `rebuildWidgets()`
  generation).

### 2.8 Cross-repo: `holonight-config` (`holonight-settings` repository, NOT this repo)

See §4.2 for the exact struct/TOML shape and §7 for the build-order risk. Files affected there
(named for completeness, not edited from this repo): `libs/holonight-config/include/
holonight_config/config_structs.h` (add `WidgetType::Mpris`, `struct MprisWidgetConfig`, embed
`MprisWidgetConfig mpris;` in `WidgetDefinition`), `libs/holonight-config/src/ConfigParsers.cpp`
(`parseWidgetEntry()` gains an `if (type == QLatin1String("mpris"))` branch mirroring the existing
`"clock"` branch — no required sub-fields the way `time-to-event`'s `title`/`deadline` are
required, so it degrades to `"clock"`'s simpler shape: position parse only, then defaults), `libs/
holonight-config/src/ConfigWriter.cpp` (`writeWidgetDefinition()` gains a `WidgetType::Mpris` case
in the existing `switch`, writing `pause_hide_minutes`).

---

## 3. Data flow

### 3.1 Metadata/position update path (D-Bus signal → rendered pixel)

```
MPRIS player (e.g. VLC) emits PropertiesChanged(Player, {Metadata: {...}}) or Seeked(x)
        │
        ▼
MprisPlayerPropWatcher::onPropertiesChanged / a new SeekedWatcher slot   (MprisService.cpp)
        │  routes to owner_->onPlayerPropertiesChanged(...) — UNCHANGED dispatch,
        │  now also populates album/art_url/position/length/can_seek via MprisMetadata
        ▼
MprisService::applyPlayerProperties() → player.album/art_url/position/length/can_seek updated
        │  ALSO, internally (new with this pivot, unconditional — runs regardless of any widget's
        │  existence or visibility, same as every other field this method already updates):
        │    - track_id changed → position_mark_us_/position_mark_elapsed_ re-anchored from the
        │      fresh Position value (REQ-F-021(e))
        │    - playback_status transitions into "Paused" → paused_since_.start()
        │    - playback_status leaves "Paused" (any direction) → paused_since_.invalidate()
        ▼
MprisService::reselectActivePlayer() → applyActiveSnapshot()
        │  emits activeAlbumChanged / activeArtUrlChanged / activePositionChanged /
        │  activeLengthChanged / activeCanSeekChanged (only the ones that actually changed —
        │  same "if (new != old) { store; emit; }" diffing MprisService already does)
        │  active-player CHANGE (not just a property update) also invalidates paused_since_
        │  unconditionally (REQ-F-042) before applying the new player's own status
        ▼
MprisWidgetManager (Q_SLOTs connected to each of the above five NOTIFY signals, ONE connection
per manager instance, not per monitor — the active player is process-global)
        │
        ├─ metadata (title/artist/album/desktopEntry) changed:
        │      → resyncSurface(monitor) for every currently-VISIBLE monitor:
        │          push title/artist/album/desktopEntry to that surface's QML root
        │          MprisArtworkCache::resolve(new_art_url, callback)
        │             → callback pushes "artworkPath" only if generation + URL are still current
        │      hidden monitors: no push now: REQ-F-062 defers the push to reveal time
        │
        ├─ artwork URL changed:
        │      → increment the manager generation; shared URL-keyed work is not cancelled;
        │        callback validation, not cancellation timing, prevents stale artwork
        │      (no per-monitor tracker to reset — MprisService already re-anchored its own
        │      position_mark_ above, unconditionally, before this signal even reached the manager)
        │
        └─ playback_status changed:
               → opacity-state push (Playing/Paused/Stopped, REQ-F-036…038)
               → NO pause-hide timer start/cancel here — there is no per-monitor timer; the next
                 2 Hz tick (§3.2) simply starts comparing MprisService::activePauseElapsedMs()
                 against this monitor's threshold once status is Paused, and stops comparing (i.e.
                 stays fully visible) once it isn't

MPRIS player emits Seeked(x)
        ▼
MprisService's Seeked watcher → position_mark_us_/position_mark_elapsed_ re-anchored to the
corrected value, unconditionally (not gated by position_tracking_refcount_) → activePosition
NOTIFY fires (REQ-F-018/021(c)'s ≤10ms-latency correction path)
        ▼
MprisWidgetManager receives activePositionChanged separately from the coherent metadata snapshot
signal and immediately pushes the corrected position to every visible surface. Hidden surfaces
receive the corrected value on reveal through resyncSurface.
```

### 3.2 The 2 Hz local tick (visible-only)

```
MprisWidgetManager::position_tick_timer_ (500ms interval, running IFF anySurfaceVisible(); the
                                          tracking handle is held for manager lifetime)
        │  fires
        ▼
onPositionTick()
        │  for each monitor where content_visible_[monitor] == true:
        ▼
mpris_->activePosition()       (plain read on the shared MprisService singleton — pure arithmetic
                                internally, QElapsedTimer::elapsed() * rate, no D-Bus call, no
                                local tracker object of any kind on the MprisWidgetManager side)
mpris_->activePauseElapsedMs() (same singleton, same tick — compared against this monitor's own
                                mpris.pause_hide_minutes threshold to decide the Paused-hidden-by-
                                timeout presentation state, §5b)
        │
        ▼
root->setProperty("positionUs", value)   (per-monitor QQuickItem push, same mechanism as
                                          WidgetManager::pushClockStrings)
```

### 3.3 Occupancy suspend/resume (REQ-F-061/062)

```
MonitorOccupancyService::occupancyChanged("DP-1", is_empty=false)
        ▼
MprisWidgetManager::onOccupancyChanged("DP-1", false)
        ▼
applyVisibility("DP-1")
        │  content_visible_["DP-1"] = false
        │  root->setProperty("contentVisible", false)       [QML owns visible; surface stays mapped]
        │  stop presentation/retry scheduling for this surface; a shared in-flight resolve may
        │      finish and populate the cache, but no result is pushed to this hidden surface
        │  updateTimerState()  → position_tick_timer_ stops IFF no monitor is visible anymore
        │      (still running if e.g. DP-2 is a second target monitor and remains empty); at the
        │      manager-lifetime position_tracking_handle_ remains held, so the single shared
        │      reconciliation timer continues while any MPRIS widget is configured
        │  no per-monitor pause-hide timer to stop — there isn't one (§5b). MprisService's
        │      paused_since_ mark keeps accruing in real wall-clock time regardless of DP-1's
        │      occupancy, because it costs nothing to maintain and is not gated by any widget's
        │      visibility (§5b) — only the widget's own comparison against it (this tick) stops
        ▼
[time passes; MprisService keeps running in the background regardless — the topbar pill on ANY
monitor, including DP-1, keeps updating live throughout, because it reads MprisService directly
and was never wired to MonitorOccupancyService. paused_since_ (if the player is Paused) keeps
accruing throughout, unconditionally.]
        ▼
MonitorOccupancyService::occupancyChanged("DP-1", is_empty=true)
        ▼
applyVisibility("DP-1")
        │  content_visible_["DP-1"] = true
        │  resyncSurface("DP-1")   ← REQ-F-062: re-pull activeTitle/activeArtist/activeAlbum/
        │      activeArtUrl/activePlaybackStatus/activeLength/activeCanSeek from MprisService
        │      FRESH (not from any locally-cached last-pushed value — the whole point is that
        │      DP-1's last-pushed value may be for a track that finished minutes ago) and push
        │      all of it in one frame; positionUs and the pause-hide comparison are picked up by
        │      the very next 2 Hz tick once position_tick_timer_ restarts below, reading
        │      MprisService's current values directly — no local reset() call is needed since
        │      there is no local tracker holding stale state to begin with. If
        │      activePauseElapsedMs() is already past this monitor's pause_hide_minutes at reveal
        │      time, the surface reveals ALREADY in the Paused-hidden-by-timeout state rather than
        │      flashing visible-Paused first (§5b)
        │  root->setProperty("contentVisible", true)
        │  updateTimerState()  → restarts position_tick_timer_ if this is the first visible
        │      surface again; the manager-lifetime tracking handle was never released
```

---

## 4. Interfaces / APIs

### 4.1 `MprisService` — five new Q_PROPERTYs, plus the centralized tracking API

```cpp
Q_PROPERTY(QString activeAlbum READ activeAlbum NOTIFY activeAlbumChanged FINAL)
Q_PROPERTY(QString activeArtUrl READ activeArtUrl NOTIFY activeArtUrlChanged FINAL)
Q_PROPERTY(qint64 activePosition READ activePosition NOTIFY activePositionChanged FINAL)
Q_PROPERTY(qint64 activeLength READ activeLength NOTIFY activeLengthChanged FINAL)
Q_PROPERTY(bool activeCanSeek READ activeCanSeek NOTIFY activeCanSeekChanged FINAL)
```
Backed by five new private members (`active_album_`, `active_art_url_`, `active_position_`,
`active_length_`, `active_can_seek_`) and diffed/emitted in `applyActiveSnapshot()` exactly like
the existing ten properties (REQ-F-016…020). `activePosition` keeps its `Q_PROPERTY` shape
unchanged (REQ-F-018 requires it) but its `READ` accessor is now computed-on-read from the private
tracking state in §2.2 rather than returning a stored+diffed field directly; its `NOTIFY` cadence
is unchanged (fires on ground-truth events — metadata/status changes, `Seeked` — never on a
synthetic 2 Hz cadence, since REQ-F-023 already established the widget's on-screen tick rate and
`MprisService`'s own NOTIFY cadence are decoupled). No `Q_PROPERTY` was added for pause-elapsed
duration — see `activePauseElapsedMs()` below.

**Centralized tracking additions** (not `Q_PROPERTY`, not QML-facing — `MprisWidgetManager` is the
only consumer, via plain C++ calls on the injected `MprisService*`):

```cpp
class MprisService : public QObject {
  ...
 public:
  // RAII interest handle. Move-only; an engaged handle's destructor calls
  // releasePositionTracking() exactly once. A default-constructed (or moved-from) handle is a
  // no-op on destruction. See §5b for why this is a guard type rather than a plain
  // acquire()/release() method pair.
  class PositionTrackingHandle {
   public:
    PositionTrackingHandle() noexcept = default;
    ~PositionTrackingHandle();

    PositionTrackingHandle(const PositionTrackingHandle&) = delete;
    PositionTrackingHandle& operator=(const PositionTrackingHandle&) = delete;
    PositionTrackingHandle(PositionTrackingHandle&& other) noexcept;
    PositionTrackingHandle& operator=(PositionTrackingHandle&& other) noexcept;

   private:
    friend class MprisService;
    explicit PositionTrackingHandle(MprisService* owner) noexcept;
    MprisService* owner_{nullptr};
  };

  // Registers interest in position tracking (extrapolation + periodic reconciliation, §2.2).
  // On a 0→1 transition, immediately re-anchors from a fresh Position read. [[nodiscard]] because
  // a discarded handle releases on the same statement it was acquired on, silently defeating the
  // whole mechanism.
  [[nodiscard]] PositionTrackingHandle acquirePositionTracking();

  // Milliseconds since PlaybackStatus last transitioned into "Paused"; 0 if not currently Paused.
  // Not refcounted/gated — always accurate, always cheap (§2.2, §5b).
  [[nodiscard]] qint64 activePauseElapsedMs() const;

 private:
  // Reachable only via PositionTrackingHandle's destructor.
  void releasePositionTracking();
  ...
  int position_tracking_refcount_{0};
  QElapsedTimer position_mark_elapsed_;
  qint64 position_mark_us_{0};
  double active_rate_{1.0};
  QElapsedTimer paused_since_;
  QTimer* reconcile_timer_{nullptr};
};
```

### 4.2 `MprisPlayer` struct — five new fields

```cpp
QString album;          // xesam:album, REQ-F-011
QString art_url;        // mpris:artUrl, verbatim, REQ-F-012
qint64 position{0};     // microseconds, REQ-F-013
qint64 length{0};       // microseconds, clamped ≥0, REQ-F-014
bool can_seek{false};   // CanSeek, REQ-F-015
```

### 4.3 `holonight-config` — new `WidgetType::Mpris` + `MprisWidgetConfig`

```cpp
enum class WidgetType : std::uint8_t { TimeToEvent, Clock, Mpris };

struct MprisWidgetConfig {
  int pause_hide_minutes{10};
  static constexpr int kMinPauseHideMinutes{1};
  static constexpr int kMaxPauseHideMinutes{60};

  bool operator==(const MprisWidgetConfig&) const = default;
};

struct WidgetDefinition {
  WidgetType type{WidgetType::TimeToEvent};
  QStringList monitors;
  WidgetPosition position{WidgetPosition::CenterCenter};
  bool enabled{true};
  TimeToEventConfig time_to_event;
  ClockConfig clock;
  MprisWidgetConfig mpris;   // valid when type == WidgetType::Mpris
  bool operator==(const WidgetDefinition&) const = default;
};
```

`parseWidgetEntry()` reads `pause_hide_minutes` via
`entry["pause_hide_minutes"].value<int>().value_or(10)`, then clamps into `[1, 60]`
(REQ-C-002's "invalid values... rejected or clamped" — clamped, not rejected, matching this file's
established `.value_or(default)`-then-soft-fallback style rather than the "reject whole entry"
path reserved for structurally required fields like TimeToEvent's `title`/`deadline`).

TOML shape (REQ-C-003):
```toml
[[widget]]
type = "mpris"
position = "center-bottom"
monitors = []
enabled = true
pause_hide_minutes = 10
```
`monitors = []` (or the key omitted entirely) means "all monitors," identical to Clock/TimeToEvent.
No `[[widget]]` block of `type = "mpris"` in `config.toml` ⇒ zero `MprisWidgetManager` instances
created ⇒ the widget does not exist for this run (REQ-C-003, no auto-backfill — `WidgetsConfig`
parsing already skips missing-key backfill for the whole `[[widget]]` array-of-tables, per the
task brief's confirmation this holds identically for the new type).

### 4.4 `MprisArtworkCache` — public interface

```cpp
class MprisArtworkCache {
 public:
  explicit MprisArtworkCache(QString cache_root = defaultCacheRoot());
  void resolve(const QString& art_url, std::function<void(QString local_path)> on_ready);
  [[nodiscard]] static QString defaultCacheRoot();  // GenericCacheLocation + "/holonight-shell/mpris-artwork/"
};
```

### 4.5 `MprisWidgetSurface.qml` — properties the QML root binds

| Property | Type | Source |
|---|---|---|
| `barMonitorName` | `string` (required) | `setInitialProperties`, once |
| `title` / `artist` / `album` / `identity` / `desktopEntry` | `string` | pushed on metadata change / resync |
| `artworkPath` | `string` | pushed by `MprisArtworkCache::resolve` callback |
| `playbackStatus` | `string` | pushed on `PlaybackStatus` change |
| `positionUs` / `lengthUs` | `real` (preserves microsecond values beyond QML's 32-bit `int` range) | `positionUs`: pushed by the manager's 2 Hz tick and immediate seek notification; `lengthUs`: coherent snapshot change |
| `canSeek` | `bool` | pushed on `CanSeek` change |
| `contentVisible` | `bool` | pushed by `applyVisibility`; QML is the sole owner of the root `visible` binding |

---

## 5. Key decisions with rationale

### 5a. `MprisWidgetManager` as a sibling class, not a third `WidgetManager` branch

**Decision: sibling class subclassing `PerMonitorLayerManager` directly (option b in the task
brief), not a third parallel-struct branch inside `WidgetManager`.**

Rationale: `WidgetManager`'s entire internal shape — one shared `QTimer tick_timer_` whose
`onTick()` recomputes a *complete string* from scratch every firing, with `recomputeAndPropagate()`
type-dispatching on `definition_.type` to pick which string-formatting function to call — is built
around the invariant that content is a pure function of wall-clock time, recomputed fully on
every tick with no external event ever needing to interrupt or accelerate that cadence. MPRIS
content is the opposite: it is normally *quiescent* (no PropertiesChanged for minutes at a time)
and reacts to a Seeked signal with the tightest latency bound in the whole spec (10 ms, REQ-F-018)
— a latency bound the shared-tick-timer model cannot deliver, since folding Seeked-handling into
`onTick()` would mean the timer's own cadence becomes the correction latency. Retrofitting
`WidgetManager` to also carry an event-driven fast path alongside its tick-driven one would mean
every `WidgetManager` instance — including Clock and TimeToEvent, which need none of this — pays
for `MprisService` signal-connection bookkeeping, an artwork-cache reference, and pause-hide-timer
state it never uses, violating the same "parallel structs, not a variant — revisit if a third
diverging type lands" comment the task brief already flags in `config_structs.h`: that comment is
about *config* shape, but the C++ manager shape has the identical problem one layer up, and MPRIS
is the third diverging type on both axes at once. A sibling class costs one more subclass of an
already-designed-for-this extension point (`PerMonitorLayerManager`'s virtuals exist precisely so
subclasses can differ this much) and keeps `WidgetManager` exactly as simple as it is today for
its two existing, structurally-similar tenants.

### 5b. Position- and pause-duration-tracking state lives centrally in `MprisService`, not
per-surface in `MprisWidgetManager` (pivoted during design review; merges the former §5b and §5d,
which independently arrived at the same underlying decision and shared the same mechanism)

**Decision: `MprisService` is the single owner of both position-tracking and pause-duration
tracking. `MprisWidgetManager` holds no local tracker object of any kind — it only reads
`MprisService::activePosition()`/`activePauseElapsedMs()` on its own 2 Hz tick and compares the
latter against its own per-instance `pause_hide_minutes` threshold. Interest in the shared
tracking is managed via a refcount, acquired/released through a move-only
`MprisService::PositionTrackingHandle` RAII guard, not plain `acquire()`/`release()` methods.**

This section was originally two separate decisions in the reviewed design — where position
tracking lives, and how the pause-hide timer works — because the original design gave each its
own owner (a per-surface `MprisPositionTracker` for the former, a per-monitor `QTimer` for the
latter). Centralizing collapses them into one decision with one shared rationale, because both are
the same shape of fact: a single global attribute of "the one active player, process-wide" (an
elapsed-position value; a duration-since-entering-Paused value), not a per-monitor fact, even
though each monitor's *widget instance* still renders/reacts to it independently via its own
configured threshold or its own occupancy state.

**Why centralize (supersedes the original §5b, which reasoned the opposite way):**

1. **There is exactly one active player.** `MprisPlayerSelector` already establishes this
   process-wide invariant (§2.1's "reused as-is" note). "Current elapsed position" and "how long
   has this been continuously Paused" are therefore single global facts. The original design's
   concern — that REQ-F-061 frames suspension per-surface, so tracking must be per-surface too —
   conflated two different things: *what is suspended* (each widget instance's own act of
   sampling/comparing/redrawing, which absolutely is per-surface and independent per REQ-F-049) and
   *where the underlying fact lives* (which does not need to be duplicated just because it is
   consumed independently in multiple places). REQ-F-061 was reworded during this pivot specifically
   to make this distinction explicit: it is now about observable effect, not implementation
   ownership, and explicitly exempts centralized shared tracking from the suspension it mandates for
   each widget instance's own presentation work.
2. **`MprisService` already owns the exact event stream that would need to drive this tracking
   regardless of who holds the state** — every `PropertiesChanged` for `Metadata`/`PlaybackStatus`,
   and the new `Seeked` signal (§2.1), already flow through `applyPlayerProperties()`/
   `reselectActivePlayer()`/`applyActiveSnapshot()`. Putting the tracking state anywhere else means
   re-deriving "did the track change," "did status change," "did a Seeked correction arrive" a
   second time from the same signals `MprisService` already diffs — duplicated diffing logic, not
   duplicated state for its own sake.
3. **Centralizing removes duplicated timer/state-machine logic that would otherwise be
   re-implemented, and re-tested, once per widget-manager instance** — and removes the multi-manager
   D-Bus-reconciliation-storm concern the original design's Known Risk #4 had to reason about
   entirely, rather than merely mitigating it: with exactly one reconciliation timer in the whole
   process regardless of how many `[[widget]] type = "mpris"` definitions or monitors exist, there is
   no "one call per definition" to multiply in the first place (§7).
4. **This does not reintroduce expensive presentation work while hidden.** A manager acquires one
   shared-tracking interest for its configured lifetime and releases it on config teardown, not on
   occupancy changes. While at least one MPRIS widget is configured and playback is Playing, the
   only visibility-independent recurring work is one process-global Position reconciliation every
   20 seconds; extrapolation itself is computed on demand. Per-monitor 2 Hz sampling, property
   pushes, and redraws remain stopped while hidden. With zero configured MPRIS widgets, the refcount
   is zero and no new reconciliation work runs. This is the performance/lifecycle boundary required
   by REQ-F-061.
5. **Each widget instance's role shrinks to sampling and comparing, not owning.** On its own
   already-existing, already-occupancy-suspended 2 Hz tick (REQ-F-023), it reads
   `MprisService::activePosition()` (no local `QElapsedTimer`) and compares
   `MprisService::activePauseElapsedMs()` against its own `pause_hide_minutes` (which stays
   per-widget-instance/configurable — only the raw duration-*tracking* mechanism moved centrally,
   not the per-widget *threshold*). No `MprisPositionTracker` class exists. No per-monitor
   `QTimer::singleShot` pause-hide timer exists — "the timer" is just a comparison the existing tick
   already performs.

**Why one refcount and gate position-extrapolation + reconciliation together, but leave
`paused_since_` entirely ungated:** these are not symmetric, and the asymmetry is deliberate, not
an oversight. `paused_since_` is a single `QElapsedTimer` mark set inside dispatch that already runs
unconditionally for every `PropertiesChanged` signal — maintaining it costs nothing whether or not
any widget exists, so gating it would add a branch for zero benefit. Position extrapolation +
reconciliation are different: extrapolation without periodic reconciliation would silently violate
REQ-F-021's 500 ms/60 s drift bound (nothing would ever correct the drift), so they are gated
*together*, as one unit, keeping the refcount's meaning simple ("is anyone watching closely enough
to need extrapolation-with-correction, or is the cheap raw ground-truth value good enough").

**Why a `PositionTrackingHandle` RAII guard, not plain `acquirePositionTracking()`/
`releasePositionTracking()` methods (a real alternative, and this codebase does have a precedent for
the plain-methods shape — `IdleInhibitor::acquire()`/`release()`):** `IdleInhibitor` is owned 1:1 by
a single caller (`SuspendInhibitorService`) that calls both methods itself in matched pairs it fully
controls — there is no refcounting and no risk of a second, independently-lifetimed owner forgetting
to call `release()`. This feature's acquire/release is fundamentally different: potentially multiple
`MprisWidgetManager` instances (REQ-C-003 permits more than one `[[widget]] type = "mpris"`
definition) share one `MprisService`, and `MprisWidgetManager` instances are destroyed and recreated
on every config reload (`rebuildWidgets()`, §2.7) — exactly the scenario where a plain
`acquire()`/`release()` pair risks a leaked acquire if a manager is torn down while it still holds
outstanding interest (a genuine new risk, §7). Tying release to a move-only RAII guard's destructor
makes that class of bug structurally hard to hit: as long as `position_tracking_handle_` is an
ordinary (non-pointer, non-manually-managed) member of `MprisWidgetManager`, C++ guarantees it is
released whenever the manager is destroyed, config-reload teardown included, with no dedicated
cleanup code to remember. `[[nodiscard]]` on `acquirePositionTracking()` further guards against the
adjacent mistake of discarding the returned handle immediately (which would release on the same
statement it was acquired on). The plain-methods shape remains the right choice for `IdleInhibitor`
precisely because it lacks this multi-owner, repeated-construction/destruction pattern — this is a
"pick the right tool for this shape of ownership," not a blanket "RAII is always better."

**On-reveal semantics and the pause-hide "does elapsed-while-hidden count" question (the former
§5d's hardest question, now much simpler to answer):** `MprisService`'s `paused_since_` tracking is
centralized and **not** gated by any individual widget's occupancy — it keeps accruing in real
wall-clock time while a given monitor's surface is occupancy-hidden, because it is cheap to
maintain (see above) and there is no reason to freeze it just because nobody happens to be looking
at that one monitor right now. Concretely: a user pauses music, works with windows open (widget
occupancy-hidden) for 12 real minutes with `pause_hide_minutes = 10`, then clears their desktop —
under this design, `activePauseElapsedMs()` already reads ~12 minutes by the time the surface is
revealed, past the 10-minute threshold. Per REQ-F-062, the widget's tick resumes on reveal,
immediately samples the current (already-past-threshold) value, and the surface reveals **already**
in the Paused-hidden-by-timeout state rather than briefly flashing visible-Paused before hiding —
whether it shows hidden-by-occupancy or hidden-by-pause-timeout is moot while occupancy-hidden
anyway (it is hidden either way); what matters is only what happens at the moment of reveal.

The original (pre-pivot) design reasoned to the opposite conclusion for the identical scenario —
freeze the pause-hide countdown while occupancy-hidden, so the same 12-minutes-hidden user would see
their paused track revealed still fully visible, with the countdown resuming from wherever it had
paused. Re-examining that reasoning under the new architecture: the original rationale was that
REQ-F-061 lists "any... pending... processing owned by that widget instance" as suspended while
hidden, and read the pause-hide countdown as falling into that category by analogy to the
artwork-retry scheduler REQ-F-061 explicitly names. That reading is no longer available, because
REQ-F-061 was reworded during this same pivot to explicitly exempt centralized shared tracking — the
pause-hide duration is now categorically state `MprisService` must keep correct in the background
(§2.2), not "processing owned by the widget instance," the same way `activePosition` itself must.
Independent of the wording change, the **counts-while-hidden** reading now also reads as more
internally consistent on its own terms: `pause_hide_minutes` asks the system to "hide paused tracks
after N minutes," and 12 real minutes of Paused time did in fact pass, whether or not a widget
happened to be occupancy-hidden for some of it — freezing the countdown behind occupancy would mean
the *same* real-world pause duration produces different hide behavior purely as a function of how
many windows happened to be open on that monitor in the meantime, which is arguably the more
surprising behavior of the two, not the less surprising one. As before, no REQ-F-039…044 acceptance
criterion exercises this specific cross-cutting case, so nothing in SPEC.md is violated by either
reading — this design now picks the counts-while-hidden reading both because it is what the revised
REQ-F-061 wording requires structurally, and because it holds up better under direct scrutiny.

### 5c. Artwork cache: service shape, storage, eviction bookkeeping

**Decision: a plain (non-QObject) `MprisArtworkCache` class, constructed once in
`ShellApplication`, shared by reference across `MprisWidgetManager` instances; on-disk storage at
`GenericCacheLocation + "/holonight-shell/mpris-artwork/"`; content-addressed filenames (SHA-256
of the exact raw URL) with no separate index — a directory scan plus per-file mtime is the entire
LRU bookkeeping.**

Rationale, cache-root path: this codebase has two existing, *inconsistent* on-disk cache
precedents — `DesktopEntryCache`'s launcher database resolves its path via
`QStandardPaths::GenericCacheLocation + "/holonight-shell/launcher.db"`
(`LauncherService.cpp`), while `WeatherService::cachePath()` manually reads
`XDG_CACHE_HOME`/falls back to `~/.cache`, then appends `/holonight/weather.json`. This design
follows the **launcher** precedent (`.../holonight-shell/...`, via the `QStandardPaths` API) rather
than the weather one, for two reasons: it is the structurally closer precedent — a directory of
many small per-item cache files with an eviction concern, versus weather's single JSON blob with
no eviction logic at all — and `QStandardPaths::GenericCacheLocation` already handles the
`XDG_CACHE_HOME` environment-variable override correctly without hand-rolling it, which is
strictly less code than weather's approach for the same result.

Rationale, no SQLite index: `DesktopEntryCache` uses SQLite because it stores *structured,
queryable records* (mtime/size metadata joined against a JSON blob of parsed `.desktop` fields,
looked up by original source path) — the query shape justifies a real database. This cache stores
*opaque image blobs* keyed by a single composite hash with exactly one lookup pattern ("does
`<hash>.png` exist") and one eviction pattern ("delete oldest mtime until under budget") — both
are trivially served by the filesystem itself (`QFileInfo::exists`, `QFileInfo::lastModified`)
with zero risk of the index and the actual files drifting out of sync (a failure mode a
SQLite-index approach must actively guard against, e.g. a row referencing a file deleted by
something else). A JSON-sidecar LRU list was also considered and rejected for the same
sync-drift reason: it is strictly more moving parts than "ask the filesystem," for a cache small
enough (50 MB budget) that a full directory `stat()` scan at startup is unmeasurably fast.

*(The former §5d, "`pause_hide_minutes` timer ownership and interaction with occupancy
suspend/resume," is now folded entirely into §5b above — both decisions turned out to share the
same architectural mechanism and rationale once tracking was centralized, so keeping them as
separate sections would have meant stating the same reasoning twice.)*

---

## 6. Alternatives considered

**Per-surface tracking (the original design, tried first and rejected during review).**
Alternative: a standalone `MprisPositionTracker` class, one instance per occupancy-visible surface,
owned by `MprisWidgetManager` — the shape actually specified in the first reviewed draft of this
document (formerly §2.2/§2.3), plus an independent per-monitor `QTimer::singleShot` pause-hide
timer (formerly §5d). Rejected during design review, before implementation, once it became clear
both mechanisms were tracking the same underlying global fact (there is exactly one active player
process-wide, §5b) once per monitor instead of once — risking the per-monitor copies drifting apart
from each other, duplicating timer/state-machine logic that would need re-implementing and
re-testing once per widget-manager instance, and creating a genuine (if bounded) multi-manager
D-Bus-reconciliation-storm concern if a user configured more than one `[[widget]] type = "mpris"`
definition (the original design's Known Risk #4). Superseded by centralizing both position- and
pause-duration-tracking inside `MprisService` itself, gated by a refcounted interest mechanism so
the "always running" cost this alternative was originally chosen to avoid does not reappear (§5b).

**WidgetManager-fork decision.** Alternative: extend `WidgetManager` with a third
`WidgetType::Mpris` branch, adding `MprisService*`/`MprisArtworkCache*` members and an
`if (definition_.type == WidgetType::Mpris)` fork inside `onTick()`/`recomputeAndPropagate()`/
`qmlSource()` alongside the existing Clock/TimeToEvent forks. Rejected: this is the
"parallel-structs-not-a-variant" pattern from `config_structs.h` recreated one layer up in C++,
and it fails the Seeked-signal-latency requirement (REQ-F-018, 10 ms) outright under the
tick-timer-only event model without *also* adding a signal-driven fast path — at which point
`WidgetManager` would contain two entirely different control-flow paradigms (poll-on-tick for two
types, react-to-signal for one), which is a worse code-reading experience than two classes with a
shared base, and couples Clock/TimeToEvent's build/test surface to MPRIS's D-Bus/network/cache
dependencies for no benefit to either.

**Artwork-cache-service-shape decision.** Alternative: make `MprisArtworkCache` a `QObject` +
`QML_SINGLETON`, exposing `Q_INVOKABLE QString resolve(...)` directly to QML so
`MprisWidgetSurface.qml` could call it itself instead of receiving a C++-pushed `artworkPath`
property. Rejected on two grounds: (1) it is exactly the shape the task brief's stated risk warns
about — a new QML-exposed singleton needs the two-file `ShellApplication::registerQmlTypes()` +
constructor-member wiring that has concretely bitten this codebase once already (per project
memory on the MPRIS topbar pill's own `reg()` omission), and introducing a second QML-facing
surface purely for artwork resolution adds that failure mode for no functional gain over a plain
C++ class the widget manager already has a reference to; (2) a `Q_INVOKABLE` returning a bare
`QString` cannot express "resolution is asynchronous" the way the chosen `std::function` callback
can — QML would need a signal-based follow-up call pattern anyway (invoke, then listen for a
"resolved" signal keyed by request id), which is strictly more plumbing than C++ pushing the
already-resolved `artworkPath` property once ready, matching how every other piece of this
widget's content already reaches QML (as a pushed property, not a QML-initiated pull).

---

## 7. Known risks

1. **Cross-repo build-order dependency (REQ-C-001).** `holonight-shell`'s CMake configure step
   cannot see `WidgetType::Mpris` or `MprisWidgetConfig` until `holonight-config` is edited,
   rebuilt, and (re)installed from the sibling `holonight-settings` repository — this repo has
   zero visibility into that package's source, only whatever is currently sitting at
   `find_package(HolonightConfig CONFIG REQUIRED)`'s resolved prefix (observed at both
   `/usr/lib/cmake/HolonightConfig` and `~/.local/lib/cmake/HolonightConfig` on this dev machine;
   which one wins depends on `CMAKE_PREFIX_PATH` ordering, unverified here). Task breakdown (Stage
   3) must sequence the `holonight-config` struct/parser/writer change as a hard prerequisite step
   before any `holonight-shell` C++ referencing `WidgetType::Mpris` is written, and the
   install step must actually run (`cmake --install`) — a `holonight-settings` *source* edit alone
   does not update the installed CMake config `holonight-shell` consumes.

2. **`MprisArtworkCache` is not currently QML-exposed by this design — but if a future change
   makes it so** (e.g. a settings-app cache-inspector page, or exposing cache stats to a debug
   overlay), it must go through the full two-file `ShellApplication::registerQmlTypes()` /
   constructor-member wiring every other QML singleton in this codebase uses (`reg(member_,
   "TypeName")` plus the forward-declare/member/initializer-list triad) — omitting either half
   builds, lints, and tests clean and only fails at runtime as `ReferenceError: X is not defined`,
   per this exact class of bug already having hit the MPRIS topbar pill once (project memory).
   This design deliberately avoids the exposure in the first place (§5c/§6), but the risk is
   flagged for whoever touches this class next.

3. **Wayland input-region / pointer-transparency (REQ-U-001).** Confirmed by direct inspection of
   `WidgetManager::layerConfig()` that Clock/TimeToEvent already achieve click-through via
   `Qt::WindowTransparentForInput` in `LayerConfig::extra_flags` — a Qt-level window flag applied
   once at surface creation, not a manually-set-and-cleared `wl_surface` input region toggled
   per hide/show the way the project's documented "full-screen overlay input region" gotcha
   describes for a *different* surface type (a keep-alive full-screen popup that toggles
   visibility via `root.visible` and must therefore re-clear its input region on every hide).
   `MprisWidgetManager::layerConfig()` reuses the identical flag and inherits the identical
   guarantee — no additional manual input-region code is needed. The risk is narrower than it
   first appears, but worth re-verifying live per `task compositor-smoke-check` once implemented,
   since the project's own QA history (documented `WheelHandler`/`TapHandler` gotchas) shows
   pointer-input behavior specifically has passed every automated check before while still being
   wrong live at least twice.

4. **D-Bus reconciliation multi-manager storm — resolved by this pivot, not just mitigated.** The
   original design's version of this risk was that reconciliation issued one `getAllPlayerProperties`
   call per manager per interval, so a user configuring multiple `[[widget]] type = "mpris"`
   definitions (REQ-C-003 permits this) would multiply D-Bus round trips by the number of
   definitions. With position/reconciliation tracking now centralized inside `MprisService` and
   gated by a single process-wide refcount (§2.2/§5b), there is exactly **one** reconciliation timer
   in the whole process regardless of how many `[[widget]]` definitions or monitors exist — the
   "one call per definition" multiplication cannot occur structurally, not just in the common case.
   This entry is retained only to record that the original risk is now closed by construction, not
   because it still needs mitigation.

   **New risk introduced by this pivot: a leaked `acquirePositionTracking()` interest.** The
   interest-counting mechanism itself (§2.2/§4.1) is new machinery. If a caller acquired interest
   without it ever being released — e.g. a hypothetical future code path that stores a
   `PositionTrackingHandle` somewhere with a longer lifetime than the `MprisWidgetManager` that
   acquired it, or manually calls into a would-be plain `releasePositionTracking()` an odd number of
   times — `MprisService` would keep `reconcile_timer_` (and extrapolation) running forever, even
   after the last MPRIS widget is removed from config, silently reintroducing the always-on cost
   this design otherwise avoids (REQ-F-061). This is why §4.1/§5b deliberately chose a move-only RAII
   `PositionTrackingHandle` over a plain public `acquirePositionTracking()`/`releasePositionTracking()`
   method pair: `MprisWidgetManager` only ever holds the handle in an ordinary
   `std::optional<MprisService::PositionTrackingHandle>` member, so config-reload teardown
   (`rebuildWidgets()` destroying and recreating managers, §2.7) releases any outstanding interest
   automatically via normal C++ object destruction — there is no cleanup call for a future maintainer
   to forget. `releasePositionTracking()` itself is private, reachable only via the handle's
   destructor (a `friend`), so this class of leak is structurally hard to introduce rather than
   merely documented against; residual risk is limited to a future change that deliberately bypasses
   the handle (e.g. storing a raw `MprisService*` and reimplementing the acquire logic manually) —
   flagged here so any such bypass gets the same scrutiny this decision received.

5. **Artwork cache eviction bugs causing unbounded growth.** The chosen bookkeeping (§5c) keeps
   the authoritative total-size figure only in memory, recomputed from a directory scan at
   startup and maintained incrementally thereafter. A crash or unclean shutdown mid-write could in
   principle leave a partially-written file on disk that a later scan double-counts or
   miscounts; mitigated by writing to a temp file and atomically renaming into place
   (`QSaveFile` or an explicit temp-path + `QFile::rename`) so a crash never leaves a *partial*
   file at the final content-addressed path, only, at worst, an orphaned temp file the next
   startup scan does not see at all (it will not match the `<hash>.png` naming pattern the scan
   looks for) and which a periodic OS tmp-cleanup or a future explicit temp-sweep would need to
   reclaim — not addressed further in this design as it is a minor, bounded leak (one file per
   crashed-mid-write occurrence) rather than an unbounded-growth path.

6. **Thread-safety of the async decode path touching QML-exposed state.** `MprisArtworkCache`'s
   decode runs on `QtConcurrent`'s global thread pool; the `on_ready` callback crossing back to
   the caller must be marshaled onto the GUI thread (§2.4 specifies this as "queued," via
   `QFutureWatcher`'s GUI-thread-affine `finished` signal, or an explicit
   `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` if a bare `std::function` callback is
   used instead of a watcher) — `QQuickItem::setProperty` and any other QML-tree touch from
   `MprisWidgetManager` must never happen from the worker thread directly. This is a standard Qt
   threading rule already respected elsewhere in this codebase's async patterns (portal probes,
   launcher scan futures) but is called out explicitly here because it is the one new place this
   feature introduces a background thread at all — every other new component in this design (the
   centralized position/pause-duration tracking state inside `MprisService`, its `reconcile_timer_`,
   `MprisWidgetManager`'s own `position_tick_timer_`, the D-Bus signal handlers) runs entirely on
   the GUI thread, so this is the sole seam where the rule must be actively enforced rather than
   falling out for free.

7. **Fixed widget surface size (§2.6) is a Design-stage estimate, not a validated layout.** 320×420
   is sized from the requirement floor (256px artwork + four text/bar rows) but has not been
   rendered against this project's actual font metrics/theme spacing tokens
   (`HoloniightPalette`-driven sizing, per this project's theming rules) — Stage 4 implementation
   should treat these two constants as a starting point subject to a live visual pass, not a fixed
   contract, the same way `kWidgetWidth`/`kWidgetHeight` themselves were presumably tuned
   empirically for Clock/TimeToEvent.

8. **Two divergences found during T-039 live testing, both fixed post-implementation:**
   - **Missing `image://icon/` provider.** `MprisWidgetManager` subclasses `PerMonitorLayerManager`
     directly (§5a) rather than reusing `LayerShellManager`'s surface, so it never inherited that
     class's `decorateEngine()` override registering `IconImageProvider`. Every other surface that
     resolves `image://icon/` (topbar, launcher, tray menu, sidebar, notification toasts) has its
     own explicit override for exactly this reason — this design's §2.3/§4.5 discussion of the
     identity badge and artwork fallback did not carry that requirement through to an explicit
     `decorateEngine()` override, and none of the automated checks (build, qml-lint, qmltypes-check,
     unit tests) catch a missing image provider — it is a runtime-only failure (`QQuickImage:
     Invalid image provider`), discovered only by live rendering. Fixed by adding the same
     one-line override every sibling surface manager already has.
   - **`data:` artwork URLs are the common case, not the excluded one.** REQ-F-027 as originally
     specified treated `data:` as an unsupported scheme alongside arbitrary custom schemes,
     reasoning it as an edge case. Live testing against Haruna (a real, commonly-installed video
     player) showed it reports `mpris:artUrl` as an inline base64 `data:` URI rather than a
     `file://` path to the source video's embedded cover art — the same is true of VLC for local
     files with embedded (not sidecar) art. Both REQ-F-027 and `MprisArtworkCache::resolve()` were
     revised to parse and decode this form (§2.4) rather than leaving real-world artwork
     permanently falling back to the app icon for an entire class of commonly-used players.
