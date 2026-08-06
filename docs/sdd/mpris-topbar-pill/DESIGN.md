# Design — mpris-topbar-pill

## Overview

This feature adds an MPRIS (Media Player Remote Interfacing Specification) client backend to
`holonight-shell` and a "now playing" pill on the topbar. The backend watches the D-Bus session
bus for `org.mpris.MediaPlayer2.*` names, tracks each discovered player's playback state and
metadata, deterministically selects one "active" player, and exposes the result to QML through a
single `MprisService` singleton. `MprisSection.qml` renders the pill (icon, artist—title, three
control buttons) between `ActiveWindowSection` and `WeatherSection` on the topbar, visible only
when an active player exists.

The design follows the project's established backend-adapter pattern (`IPortalDBus` /
`SystemPortalDBus` / `NullPortalDBus`, `BrightnessBackend` / `SysfsBackend` /
`NullBrightnessBackend`): an injectable `IMprisDBus` seam, a real `SystemMprisDBus` implementation,
and a `FakeMprisDBus` test double. The active-player-selection algorithm and metadata extraction
are both pure, D-Bus-free, unit-testable functions operating on a plain `MprisPlayer` struct with
no `QObject`/D-Bus dependency — mirroring how `BrightnessService`'s percent computation and
`PortalService`'s accent-color decoding are kept out of the D-Bus-facing class.

This is a read-only D-Bus **client**. The shell never exports an MPRIS or
`org.freedesktop.DBus.Properties` interface of its own, so REQ-NF-005's `Q_CLASSINFO("D-Bus
Interface", ...)` requirement does not apply anywhere in this feature — see "Known risks" for the
explicit call-out.

---

## Architecture Diagram (ASCII)

```
┌────────────────────────────────────────────────────────────────────────────────────┐
│  QML layer (HolonightShell module) — one instance per monitor's TopBar.qml          │
│                                                                                      │
│   TopBar.qml                                                                        │
│     ├─ ActiveWindowSection                                                          │
│     ├─ MprisSection.qml   ◄── visible: MprisService.hasActivePlayer                 │
│     │    └─ MprisWidget.qml (BarSection)                                            │
│     │         ├─ image://icon/<activeDesktopEntry>                                  │
│     │         ├─ artist — title  Text (fade-truncated, fixed width)                 │
│     │         └─ Row [ MprisControlButton×3 ]  (fixed total width, REQ-F-023)       │
│     └─ WeatherSection                                                               │
└───────────────────────────────────┬──────────────────────────────────────────────── ┘
       Q_PROPERTY reads (all monitors bind the SAME singleton instance — REQ-F-026)    │
       Q_INVOKABLE playPause()/next()/previous()                                       │
                                     ▼
┌────────────────────────────────────────────────────────────────────────────────────┐
│  MprisService  (QML_SINGLETON, holonight_services, libs/holonight-services/src/mpris)│
│                                                                                      │
│  QList<MprisPlayer> players_          (owned, insertion-ordered registry)           │
│  QHash<QString,QObject*> propWatchers_ (per-player PropertiesChanged routers)        │
│  QString activeService_                                                             │
│                                                                                      │
│  onNameOwnerChanged(name,old,new) ──filter "org.mpris.MediaPlayer2."──► acquire/release
│  onPlayerPropertiesChanged(service, iface, changed, invalidated) ──► refresh + select │
│                                             └──► MprisPlayerSelector::selectActiveIndex(players_)
│                                             └──► applyActiveSnapshot() → NOTIFY diffs │
│                                                                                      │
│  playPause()/next()/previous() ──► capability gate (REQ-F-015) ──► dbus_->asyncX()   │
│                                     (no watcher, no state mutation — REQ-F-013/014)  │
└───────────────────────────┬──────────────────────────────────────────────────────── ┘
                            │  std::unique_ptr<IMprisDBus>
          ┌─────────────────┴─────────────────┐
          ▼                                    ▼
┌─────────────────────────┐        ┌───────────────────────────────┐
│  SystemMprisDBus         │        │  FakeMprisDBus (tests)         │
│  (session bus, blocking  │        │  in-memory player table,       │
│  QDBusReply<T> reads —   │        │  synchronous signal injection, │
│  REQ-C-003)              │        │  no dbus-daemon required        │
│  async fire-and-forget   │        │  (REQ-NF-001)                   │
│  commands (REQ-F-013)    │        └───────────────────────────────┘
└──────────┬────────────────┘
           │ session D-Bus
           ▼
┌──────────────────────────────────────────┐
│  org.mpris.MediaPlayer2.<Player>          │
│  /org/mpris/MediaPlayer2                  │
│  iface org.mpris.MediaPlayer2.Player      │
│  iface org.freedesktop.DBus.Properties    │
└──────────────────────────────────────────┘

  Pure, D-Bus-free, unit-tested in isolation (REQ-NF-002):
  ┌────────────────────────────┐   ┌──────────────────────────────┐
  │ MprisPlayerSelector          │   │ MprisMetadata                 │
  │ selectActiveIndex(           │   │ unwrapDict/unwrapStringList/  │
  │   const QList<MprisPlayer>&) │   │ unwrapTrackId/extractFields   │
  │ → int index or -1            │   │ (handles the a{sv} / as /     │
  └────────────────────────────┘   │  QDBusObjectPath traps)        │
                                     └──────────────────────────────┘
```

---

## Components

### `MprisPlayer` (plain struct)

**File:** `libs/holonight-services/src/mpris/MprisPlayer.h`

Deliberately **not** a `QObject`. Holds one player's tracked state as plain data so it can be
constructed, copied, and compared in unit tests with zero D-Bus/Qt-meta-object machinery — this is
what makes `MprisPlayerSelector` and the activity-timestamp logic testable per REQ-NF-001/002.

```cpp
struct MprisPlayer {
  QString serviceName;                                    // "org.mpris.MediaPlayer2.vlc"
  QString identity;                                        // REQ-F-003
  QString desktopEntry;                                    // REQ-F-003, REQ-F-018
  QString playbackStatus{QStringLiteral("Stopped")};        // REQ-F-003
  QString title;                                            // xesam:title, REQ-F-003/011
  QStringList artists;                                      // xesam:artist, REQ-F-003/012
  QString trackId;                                          // mpris:trackid, REQ-F-003/006b
  bool canGoNext{false};
  bool canGoPrevious{false};
  bool canPlay{false};
  bool canPause{false};
  bool canControl{false};                                   // REQ-F-003, REQ-F-015
  qint64 lastActivityTimestampMs{-1};                       // REQ-F-005; -1 = unset
};
```

Satisfies: REQ-F-003, REQ-F-005, REQ-NF-001, REQ-NF-002 (as the vocabulary type the pure functions
operate on).

### `MprisMetadata` (free functions)

**Files:** `libs/holonight-services/src/mpris/MprisMetadata.h` / `.cpp`

Pure functions, no `QObject`, no D-Bus connection — just `QVariant`/`QVariantMap` in, typed data
out. Isolates the `QDBusArgument` extraction traps (see "Metadata extraction design" below) in one
place so `MprisService` never touches raw D-Bus marshalling directly.

```cpp
namespace MprisMetadata {

struct Fields {
  QString title;
  QStringList artists;
  QString trackId;
};

// Unwraps a QVariant that may hold a raw QDBusArgument encoding a nested a{sv} dict (the
// "Metadata" entry inside a GetAll/PropertiesChanged a{sv} reply) into a flat QVariantMap.
// Passes through unchanged if `raw` already holds a QVariantMap (lets FakeMprisDBus build
// Metadata dicts directly in tests without touching QDBusArgument at all).
[[nodiscard]] QVariantMap unwrapDict(const QVariant& raw);

// Unwraps a QVariant that may hold a raw QDBusArgument encoding an `as` array (xesam:artist)
// into a QStringList. Passes through if already a QStringList.
[[nodiscard]] QStringList unwrapStringList(const QVariant& raw);

// Normalizes mpris:trackid: QDBusObjectPath (spec-compliant `o`) or a plain string (`s`, seen on
// some non-compliant players) -> QString path/value, for stable trackid-changed comparisons.
[[nodiscard]] QString unwrapTrackId(const QVariant& raw);

// Runs unwrapDict/unwrapStringList/unwrapTrackId over the three fields REQ-F-003 cares about.
[[nodiscard]] Fields extractFields(const QVariantMap& metadataDict);

}  // namespace MprisMetadata
```

Satisfies: REQ-F-003, REQ-F-004 (by construction — no `Position`/`Rate`/`Seeked` fields exist in
`Fields`), REQ-F-011, REQ-F-012, REQ-C-002 (property names match the MPRIS spec verbatim).

### `MprisPlayerSelector` (free function)

**Files:** `libs/holonight-services/src/mpris/MprisPlayerSelector.h` / `.cpp`

```cpp
namespace MprisPlayerSelector {
// Returns the index into `players` selected by REQ-F-007's algorithm, or -1 if none qualifies.
[[nodiscard]] int selectActiveIndex(const QList<MprisPlayer>& players);
}
```

Satisfies: REQ-F-007, REQ-F-008 (called on every registry mutation), REQ-NF-002 (this *is* the
"static or free function that takes a vector/list of mock player objects and returns the selected
player index" the spec asks for, verbatim).

### `IMprisDBus` (seam interface)

**File:** `libs/holonight-services/src/mpris/MprisDbus.h`

```cpp
class IMprisDBus {
 public:
  virtual ~IMprisDBus() = default;
  IMprisDBus() = default;
  IMprisDBus(const IMprisDBus&) = delete;
  IMprisDBus& operator=(const IMprisDBus&) = delete;
  IMprisDBus(IMprisDBus&&) = delete;
  IMprisDBus& operator=(IMprisDBus&&) = delete;

  // Discovery (REQ-F-001, REQ-C-003)
  [[nodiscard]] virtual QStringList listNames() = 0;              // blocking; startup snapshot
  virtual bool connectNameOwnerChanged(QObject* receiver, const char* slot) = 0;

  // Per-player property read + live subscription (REQ-F-002, REQ-F-003)
  [[nodiscard]] virtual QVariantMap getAllRootProperties(const QString& service) = 0;    // blocking
  [[nodiscard]] virtual QVariantMap getAllPlayerProperties(const QString& service) = 0;  // blocking
  virtual bool connectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) = 0;
  virtual void disconnectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) = 0;

  // Fire-and-forget commands (REQ-F-013). void return type — there is nothing to await by
  // construction, so a caller cannot accidentally attach a QDBusPendingCallWatcher to these.
  virtual void asyncPlayPause(const QString& service) = 0;
  virtual void asyncNext(const QString& service) = 0;
  virtual void asyncPrevious(const QString& service) = 0;
};
```

Satisfies: REQ-NF-001 (the injectable seam itself).

### `SystemMprisDBus` (real implementation)

**File:** `libs/holonight-services/src/mpris/MprisDbus.cpp`

- `listNames()` — `QDBusConnection::sessionBus().interface()->registeredServiceNames()`, a
  synchronous `QDBusReply<QStringList>` call against the daemon's already-cached bus name list
  (typically sub-millisecond on a local session bus). This is the literal mechanism behind
  REQ-C-003 ("no async delay in discovery") — see "Key decisions."
- `connectNameOwnerChanged` — `sessionBus().connect(QString(), "/org/freedesktop/DBus",
  "org.freedesktop.DBus", "NameOwnerChanged", receiver, slot)`, matching
  `PortalDbus::connectNameOwnerChanged`'s exact call shape. This subscribes to **every** bus name
  change system-wide — `MprisService::onNameOwnerChanged` must self-filter on the
  `org.mpris.MediaPlayer2.` prefix, exactly as `PortalService::onNameOwnerChanged` filters on its
  own portal prefix.
- `getAllRootProperties(service)` and `getAllPlayerProperties(service)` call `GetAll` for
  `org.mpris.MediaPlayer2` and `org.mpris.MediaPlayer2.Player`, respectively. Both use the fixed
  `/org/mpris/MediaPlayer2` path and a 500 ms timeout. Root supplies `Identity` and `DesktopEntry`;
  Player supplies playback state, metadata, and transport capabilities.
- `connectPropertiesChanged`/`disconnectPropertiesChanged` — same
  `org.freedesktop.DBus.Properties` / `PropertiesChanged` connect/disconnect shape used by
  `BatteryService`, `NetworkService`, `PowerProfilesService`, scoped to one `service` name at a
  time (path is always the fixed `/org/mpris/MediaPlayer2`).
- `asyncPlayPause`/`asyncNext`/`asyncPrevious` — `QDBusInterface(service, "/org/mpris/MediaPlayer2",
  "org.mpris.MediaPlayer2.Player").asyncCall("PlayPause"|"Next"|"Previous")`, return value
  discarded. Qt's `QDBusPendingCallPrivate` is internally refcounted and self-cleans when the
  reply arrives or times out even with no `QDBusPendingCallWatcher` attached — discarding is safe,
  not a leak.

Satisfies: REQ-F-001, REQ-F-002, REQ-F-013, REQ-C-002, REQ-C-003.

### `FakeMprisDBus` (test double)

**File:** `libs/holonight-services/src/mpris/MprisDbus.cpp` (test-only class, same translation
unit as `NullPortalDBus` sits alongside `SystemPortalDBus` in `PortalDbus.cpp`) or a dedicated
`tests/`-local header if `MprisService`'s unit tests are the only consumer — either is acceptable;
default to co-locating with `SystemMprisDBus` to match the `PortalDbus.h` precedent.

```cpp
class FakeMprisDBus final : public IMprisDBus {
 public:
  // Test setup: keep Root and Player snapshots separate, matching production D-Bus replies.
  void seedPlayer(const QString& service, const QVariantMap& rootProperties,
                  const QVariantMap& playerProperties);
  void removePlayer(const QString& service);

  // Test-driven signal injection — synchronous, no event loop spin required.
  void emitNameOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);
  void emitPropertiesChanged(const QString& service, const QString& iface,
                             const QVariantMap& changed, const QStringList& invalidated = {});

  // Spies for REQ-F-013/014/015 assertions.
  [[nodiscard]] QStringList playPauseCalls() const { return play_pause_calls_; }
  [[nodiscard]] QStringList nextCalls() const { return next_calls_; }
  [[nodiscard]] QStringList previousCalls() const { return previous_calls_; }

  // IMprisDBus
  [[nodiscard]] QStringList listNames() override;
  bool connectNameOwnerChanged(QObject* receiver, const char* slot) override;
  [[nodiscard]] QVariantMap getAllRootProperties(const QString& service) override;
  [[nodiscard]] QVariantMap getAllPlayerProperties(const QString& service) override;
  bool connectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) override;
  void disconnectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) override;
  void asyncPlayPause(const QString& service) override { play_pause_calls_.append(service); }
  void asyncNext(const QString& service) override { next_calls_.append(service); }
  void asyncPrevious(const QString& service) override { previous_calls_.append(service); }

 private:
  QHash<QString, QVariantMap> rootProperties_;
  QHash<QString, QVariantMap> playerProperties_;
  QObject* name_owner_receiver_{nullptr};
  const char* name_owner_slot_{nullptr};
  QHash<QString, QPair<QObject*, const char*>> prop_receivers_;
  QStringList play_pause_calls_, next_calls_, previous_calls_;
};
```

`connectNameOwnerChanged`/`connectPropertiesChanged` record the receiver/slot instead of touching
a real bus. Interface-qualified changes, invalidations, and configurable subscription failure let
tests drive realistic discovery, refresh, retry, selection, and command paths without
`dbus-daemon`.

Satisfies: REQ-NF-001.

### `MprisService` (QML singleton)

**Files:** `libs/holonight-services/src/mpris/MprisService.h` / `.cpp`

The orchestrator. Owns the player registry, drives discovery/subscription through `IMprisDBus`,
re-runs selection on every mutation, and exposes the read-only snapshot + commands to QML.

```cpp
class MprisService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(bool hasActivePlayer READ hasActivePlayer NOTIFY hasActivePlayerChanged FINAL)
  Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeTitleChanged FINAL)
  Q_PROPERTY(QString activeArtist READ activeArtist NOTIFY activeArtistChanged FINAL)
  Q_PROPERTY(QString activeIdentity READ activeIdentity NOTIFY activeIdentityChanged FINAL)
  Q_PROPERTY(QString activeDesktopEntry READ activeDesktopEntry NOTIFY activeDesktopEntryChanged FINAL)
  Q_PROPERTY(QString activePlaybackStatus READ activePlaybackStatus NOTIFY activePlaybackStatusChanged FINAL)
  Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY canGoNextChanged FINAL)
  Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY canGoPreviousChanged FINAL)
  Q_PROPERTY(bool canPlay READ canPlay NOTIFY canPlayChanged FINAL)
  Q_PROPERTY(bool canPause READ canPause NOTIFY canPauseChanged FINAL)
  Q_PROPERTY(bool canControl READ canControl NOTIFY canControlChanged FINAL)

 public:
  explicit MprisService(QObject* parent = nullptr);                                  // production
  explicit MprisService(std::unique_ptr<IMprisDBus> dbus, QObject* parent = nullptr); // test seam

  [[nodiscard]] bool hasActivePlayer() const { return has_active_player_; }
  [[nodiscard]] QString activeTitle() const { return active_title_; }
  [[nodiscard]] QString activeArtist() const { return active_artist_; }
  [[nodiscard]] QString activeIdentity() const { return active_identity_; }
  [[nodiscard]] QString activeDesktopEntry() const { return active_desktop_entry_; }
  [[nodiscard]] QString activePlaybackStatus() const { return active_playback_status_; }
  [[nodiscard]] bool canGoNext() const { return can_go_next_; }
  [[nodiscard]] bool canGoPrevious() const { return can_go_previous_; }
  [[nodiscard]] bool canPlay() const { return can_play_; }
  [[nodiscard]] bool canPause() const { return can_pause_; }
  [[nodiscard]] bool canControl() const { return can_control_; }

  Q_INVOKABLE void playPause();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();

 Q_SIGNALS:
  void hasActivePlayerChanged();
  void activeTitleChanged();
  void activeArtistChanged();
  void activeIdentityChanged();
  void activeDesktopEntryChanged();
  void activePlaybackStatusChanged();
  void canGoNextChanged();
  void canGoPreviousChanged();
  void canPlayChanged();
  void canPauseChanged();
  void canControlChanged();

 private Q_SLOTS:
  void onNameOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);
  void onPlayerPropertiesChanged(const QString& service, const QString& iface,
                                 const QVariantMap& changed, const QStringList& invalidated);

 private:
  void init();                                    // constructor common path
  void discoverExistingPlayers();                 // REQ-C-003: listNames() + acquire() per match
  void acquirePlayer(const QString& service);      // subscribe -> Root/Player GetAll -> reselect
  void releasePlayer(const QString& service);      // unsubscribe -> erase -> reselect
  static void applyProperties(MprisPlayer& player, const QVariantMap& changed, bool isInitialRead);
  static void updateActivityTimestamp(MprisPlayer& player, const QString& previousStatus,
                                      const QString& previousTrackId, bool isDiscovery, qint64 nowMs);
  void reselectActivePlayer();
  void applyActiveSnapshot(const MprisPlayer* player);  // nullptr => hasActivePlayer=false
  [[nodiscard]] int indexOfService(const QString& service) const;

  std::unique_ptr<IMprisDBus> dbus_;
  QList<MprisPlayer> players_;                     // insertion-ordered; REQ-F-001 registry
  QHash<QString, QObject*> propWatchers_;          // service -> MprisPlayerPropWatcher* (owned)
  QString activeService_;                          // "" if none

  bool has_active_player_{false};
  QString active_title_, active_artist_, active_identity_, active_desktop_entry_, active_playback_status_;
  bool can_go_next_{false}, can_go_previous_{false}, can_play_{false}, can_pause_{false}, can_control_{false};
};
```

`MprisPlayerPropWatcher` is a small private `QObject` (defined in `MprisService.cpp`, following
`TrayWatcher.cpp`'s `ItemPropWatcher` pattern exactly) that stores the owning `service` name and
forwards `PropertiesChanged(iface, changed, invalidated)` to
`MprisService::onPlayerPropertiesChanged(service, iface, changed, invalidated)`. Because it declares `Q_OBJECT` in a
`.cpp` file, `MprisService.cpp` ends with `#include "MprisService.moc"` (per the project's
Q_OBJECT-in-.cpp convention).

Satisfies: REQ-F-001 through REQ-F-015, REQ-NF-001, REQ-NF-002 (via delegation to the pure
helpers), REQ-NF-006, REQ-C-002, REQ-C-003.

### `MprisSection.qml` / `MprisWidget.qml` / `MprisControlButton.qml`

**Files:** `apps/shell/qml/Topbar/MprisSection.qml`, `apps/shell/qml/Topbar/MprisWidget.qml`,
`apps/shell/qml/Topbar/MprisControlButton.qml`

Follows the exact `WeatherSection.qml` → `WeatherWidget.qml` split already established on this
topbar: `MprisSection.qml` is a thin visibility wrapper, `MprisWidget.qml` is a `BarSection`
holding the actual icon/text/button layout. This satisfies REQ-C-001's "follow existing shell
conventions" instruction literally, and keeps `TopBar.qml`'s per-section `visible` binding
consistent with the `WeatherSection` precedent (`visible: implicitWidth > 0`).

```qml
// MprisSection.qml
import QtQuick
import HolonightShell

MprisWidget {
    id: root
    visible: implicitWidth > 0
}
```

```qml
// MprisWidget.qml (sketch — full layout in "Data flow" / REQ-F-017 discussion below)
import QtQuick
import QtQuick.Layouts
import HolonightShell
import Holonight.Core
import "../Controls"

BarSection {
    id: root
    required property string barMonitorName
    readonly property bool ready: MprisService.hasActivePlayer

    implicitWidth: root.ready ? (contentRow.implicitWidth + <margins>) : 0
    Behavior on implicitWidth { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    BarFrame { anchors.fill: parent; visible: root.ready }

    RowLayout {
        id: contentRow
        spacing: 8
        // 1. icon — image://icon/<activeDesktopEntry>, REQ-F-018
        // 2. artist—title Text, fixed width, fade-truncated (ActiveWindowSection's titleFade
        //    gradient pattern), REQ-F-017
        // 3. Row of 3 MprisControlButton, fixed total width regardless of enabled state, REQ-F-023
    }
}
```

`MprisControlButton.qml` draws its glyph with `QtQuick.Shapes`/`ShapePath` — the same technique
`WorkspaceEdgeArrow.qml` already uses for its chevron, stroked with `HoloniightPalette` tokens, no
`Canvas` (see "Key decisions" for why). It exposes `glyph: "previous"|"playPause"|"next"`,
`playing: bool` (selects play vs. pause bars, REQ-F-020), `buttonEnabled: bool` (REQ-F-015/019),
and a `clicked()` signal wired through a `TapHandler` with `gesturePolicy:
TapHandler.ReleaseWithinBounds` (per the project's documented `TapHandler` gotcha — matches
`MouseArea.onClicked`'s forgiving click semantics) and `acceptedButtons: Qt.LeftButton` only (REQ-F-022).

Satisfies: REQ-F-016 through REQ-F-025, REQ-NF-003, REQ-NF-004, REQ-C-001.

### `MprisTestSeed` (QML-harness-only test seam)

**File:** `tests/FakeQmlServices.h` (extends the existing file; not shipped in production)

Mirrors `FakeTopbarTestSeed`/`WorkspaceModelTestSeed`: a small `QObject` wrapper with
`Q_INVOKABLE` methods that `tst_*.qml` files call to drive `MprisService` into arbitrary states for
QML smoke/behavior tests, without loosening `MprisService`'s own production API (its D-Bus-facing
slots stay `private`). Constructed by wrapping a `FakeMprisDBus` instance:

```cpp
class MprisTestSeed : public QObject {
  Q_OBJECT
 public:
  explicit MprisTestSeed(FakeMprisDBus& dbus) : dbus_(dbus) {}
  Q_INVOKABLE void seedPlayer(const QString& service, const QVariantMap& properties) {
    dbus_.seedPlayer(service, properties);
    dbus_.emitNameOwnerChanged(service, QString(), QStringLiteral("owner"));
  }
  Q_INVOKABLE void setPlaybackStatus(const QString& service, const QString& status) {
    dbus_.emitPlayerPropertiesChanged(service, {{QStringLiteral("PlaybackStatus"), status}});
  }
 private:
  FakeMprisDBus& dbus_;
};
```

`FakeQmlServices` gains `FakeMprisDBus mpris_dbus_; MprisService mpris_{std::make_unique<...>(),
...};` — actually `MprisService` owns its `IMprisDBus` via `unique_ptr`, so `FakeQmlServices` must
keep a raw pointer to the `FakeMprisDBus` it hands over (captured before `std::move`) to construct
`MprisTestSeed`, the same ownership shape any test using the injectable-seam pattern needs.
`registerSingletons()` gains `qmlRegisterSingletonInstance("HolonightShell", 1, 0, "MprisService",
&mpris_) >= 0 && qmlRegisterSingletonInstance(..., "MprisTestSeed", &mpris_test_seed_) >= 0`.

Satisfies: REQ-NF-006 (closes the exact "QML smoke-test singleton-registration gap" class of bug
called out in this project's own remediation history for other singletons).

### CMake changes

- `libs/holonight-services/CMakeLists.txt` — add
  `${CMAKE_CURRENT_SOURCE_DIR}/src/mpris` to `target_include_directories`. No other change needed:
  `HOLONIGHT_SERVICE_SOURCES` is a `GLOB_RECURSE` over `src/*.h`/`src/*.cpp`, so the new `src/mpris/`
  files are picked up automatically, and `MprisService`'s `QML_ELEMENT`/`QML_SINGLETON` macros flow
  into the `HolonightShell` module automatically through the existing
  `qt6_extract_metatypes(holonight_services ...)` → `collect-moc-metatypes.cmake` →
  `_qt_internal_qml_type_registration(holonight-shell)` pipeline in `apps/shell/CMakeLists.txt`
  (same mechanism `PortalService`, `BrightnessService`, etc. already use — no manual per-class
  registration step exists in this codebase).
- `apps/shell/CMakeLists.txt` — none. `HOLONIGHT_QML_FILES` is `GLOB_RECURSE qml/*.qml`, so the new
  `MprisSection.qml`/`MprisWidget.qml`/`MprisControlButton.qml` files are picked up automatically.
- `tests/CMakeLists.txt` — add `test_mpris_selection.cpp`, `test_mpris_metadata.cpp`,
  `test_mpris_service.cpp` to the test source list (per this project's documented gotcha, run `task
  configure-tests` explicitly after adding — its configure dependency can be stale otherwise).

---

## Data flow

### 1. Startup: players already running before the shell launches (REQ-C-003)

```
MprisService::MprisService()  [production ctor]
  → dbus_ = make_unique<SystemMprisDBus>()
  → init()
      → dbus_->connectNameOwnerChanged(this, SLOT(onNameOwnerChanged(...)))   // arm live watch first
      → discoverExistingPlayers()
          → const QStringList names = dbus_->listNames();   // BLOCKING QDBusReply<QStringList>
          → for each name matching "org.mpris.MediaPlayer2." → acquirePlayer(name)
```

Arming the live `NameOwnerChanged` subscription *before* the blocking `listNames()` snapshot
avoids a race where a player appears in the gap between the two calls and is silently missed (it
would arrive as a `NameOwnerChanged` the still-being-armed watcher hasn't attached to yet). This
ordering matches `PortalService::init()`'s "watcher first, probe second" structure.

### 2. A player appears later (REQ-F-001, REQ-F-002)

```
[real MPRIS player process starts, claims org.mpris.MediaPlayer2.vlc]
  → session bus emits NameOwnerChanged("org.mpris.MediaPlayer2.vlc", "", ":1.234")
  → MprisService::onNameOwnerChanged(name, old="", new=":1.234")
      → name.startsWith("org.mpris.MediaPlayer2.") → true; old.isEmpty() && !new.isEmpty() → appear
      → acquirePlayer("org.mpris.MediaPlayer2.vlc")
          → propWatchers_[service] = new MprisPlayerPropWatcher(service, this, this);
          → dbus_->connectPropertiesChanged(...); failure deletes watcher and leaves no registry entry
          → root = dbus_->getAllRootProperties(service);       // BLOCKING Root GetAll
          → playerProps = dbus_->getAllPlayerProperties(service); // BLOCKING Player GetAll
          → MprisPlayer player; applyRootProperties(player, root); applyPlayerProperties(player, playerProps)
          → players_.append(player)
          → reselectActivePlayer();                                    // REQ-F-008
```

### 3. A player's playback state changes (REQ-F-002, REQ-F-006, REQ-F-008, REQ-F-020)

```
[player transitions Paused -> Playing internally, e.g. user pressed play in the player's own UI]
  → player emits PropertiesChanged("org.mpris.MediaPlayer2.Player",
        {"PlaybackStatus": "Playing"}, [])
  → MprisPlayerPropWatcher::onPropertiesChanged(iface, changed, invalidated)
      → owner_->onPlayerPropertiesChanged(key_ /*service*/, iface, changed, invalidated)
          → int idx = indexOfService(service); MprisPlayer& player = players_[idx];
          → const QString previousStatus = player.playbackStatus;
          → const QString previousTrackId = player.trackId;
          → reject unrelated interfaces; apply only Root or Player keys for iface
          → if invalidated is non-empty, refresh those keys using GetAll(iface)
          → updateActivityTimestamp(player, previousStatus, previousTrackId,
                                    /*isDiscovery=*/false, QDateTime::currentMSecsSinceEpoch());
          → reselectActivePlayer();
              → int selected = MprisPlayerSelector::selectActiveIndex(players_);
              → const QString newActiveService = selected >= 0 ? players_[selected].serviceName : QString();
              → if (newActiveService != activeService_) { activeService_ = newActiveService; }
              → applyActiveSnapshot(selected >= 0 ? &players_[selected] : nullptr);
                  → per-field setXxx(val) helpers, each emitting its own NOTIFY only if the value
                    actually changed (mirrors PortalService::setColorScheme/setAccentColor)
```

QML side: `MprisSection.qml`'s `visible: implicitWidth > 0` and `MprisWidget.qml`'s
`implicitWidth: root.ready ? ... : 0` binding both react to `MprisService.hasActivePlayerChanged`;
the glyph inside `MprisControlButton { glyph: "playPause" }` reacts to
`MprisService.activePlaybackStatusChanged` (REQ-F-020). No polling anywhere in this path.

### 4. User clicks the play/pause button (REQ-F-013, REQ-F-014, REQ-F-015)

```
MprisControlButton (glyph: "playPause") TapHandler.onTapped
  → root.clicked()
  → MprisWidget.qml: MprisService.playPause()
      → MprisService::playPause()
          → if (!can_control_) return;                                        // REQ-F-015
          → const bool needsPlay = active_playback_status_ == "Paused";
          → const bool needsPause = active_playback_status_ == "Playing";
          → if ((needsPlay && !can_play_) || (needsPause && !can_pause_)) return;
          → dbus_->asyncPlayPause(activeService_);                             // fire-and-forget
          → // NOTHING ELSE. No local state write. No NOTIFY emitted here.     // REQ-F-014
```

The glyph and any accent styling do **not** change at click time. They only change later, through
path 3 above, once the real player emits its own `PropertiesChanged("PlaybackStatus", ...)` — or
never change at all if the command silently failed or the player ignored it. This is the literal
mechanism behind REQ-F-013/014's "no optimistic update" requirement, and it is testable
end-to-end with `FakeMprisDBus`: call `playPause()`, assert `activePlaybackStatus()` is unchanged,
then call `emitPropertiesChanged(...)` and assert it updates only then.

### 5. A player disappears (REQ-F-001, REQ-F-008)

```
[player process exits, releases its bus name]
  → session bus emits NameOwnerChanged("org.mpris.MediaPlayer2.vlc", ":1.234", "")
  → MprisService::onNameOwnerChanged → new.isEmpty() && !old.isEmpty() → disappear
      → releasePlayer("org.mpris.MediaPlayer2.vlc")
          → dbus_->disconnectPropertiesChanged(service, propWatchers_[service], SLOT(...));
          → delete propWatchers_.take(service);
          → players_.removeIf([&](const MprisPlayer& p) { return p.serviceName == service; });
          → reselectActivePlayer();   // if this was the active player, algorithm now picks the
                                       // next-best Playing/Paused player, or hasActivePlayer=false
```

### 6. Per-monitor consistency (REQ-F-026)

There is exactly one `MprisService` instance (a QML singleton, constructed once by the QML engine
the first time any QML file imports `HolonightShell` and references `MprisService`). Every
monitor's `TopBar.qml` → `MprisSection.qml` binds to the *same* C++ object's Q_PROPERTYs. There is
no per-monitor state, no per-monitor arbitration, and no synchronization step to get "wrong" — two
topbars showing different active players is structurally impossible with this design, not just
tested-to-be-consistent.

---

## Interfaces / APIs

### QML-facing (`MprisService`, `import HolonightShell`)

| Member | Type | Notes |
|---|---|---|
| `hasActivePlayer` | `bool`, read-only | REQ-F-009, REQ-F-016 |
| `activeTitle` | `QString`, read-only | REQ-F-009, REQ-F-011 |
| `activeArtist` | `QString`, read-only | REQ-F-009, REQ-F-012 |
| `activeIdentity` | `QString`, read-only | REQ-F-009 |
| `activeDesktopEntry` | `QString`, read-only | REQ-F-009, REQ-F-018 |
| `activePlaybackStatus` | `QString`, read-only | REQ-F-009, REQ-F-020 |
| `canGoNext` / `canGoPrevious` / `canPlay` / `canPause` / `canControl` | `bool`, read-only | REQ-F-009, REQ-F-015, REQ-F-019 |
| `playPause()` | `Q_INVOKABLE void` | REQ-F-010, gated per REQ-F-015 |
| `next()` | `Q_INVOKABLE void` | REQ-F-010, gated per REQ-F-015 |
| `previous()` | `Q_INVOKABLE void` | REQ-F-010, gated per REQ-F-015 |

All properties are plain read-only `Q_PROPERTY`s with per-property `NOTIFY` signals — no
`selectPlayer()`, no priority/ignore-list configuration surface, matching the Non-Goals list
exactly.

### Internal C++ seam (`IMprisDBus`)

See the "Components" section above for the full interface. Summary of the contract:

- Three **blocking** read operations (`listNames`, Root `GetAll`, Player `GetAll`) — REQ-C-003.
- Two signal-subscription pairs (`connectNameOwnerChanged`; `connectPropertiesChanged` /
  `disconnectPropertiesChanged`, the latter scoped per service name) — REQ-F-001, REQ-F-002.
- Three **void**, fire-and-forget command methods — REQ-F-013.

No method on `IMprisDBus` returns a `QDBusPendingCall`/`QDBusPendingReply<T>`; there is nothing in
this design that touches `QDBusPendingCallWatcher::isError()` at all, because every read is
blocking and every write is void. This sidesteps the project's documented
`QDBusPendingReply<T>`-with-`fromCompletedCall` trap entirely rather than working around it.

---

## Active-player selection algorithm design

`MprisPlayerSelector::selectActiveIndex` is a free function, not a method on any `QObject`,
operating purely on `const QList<MprisPlayer>&` — no D-Bus handle, no `QObject` parent, no timers.
This is the exact shape REQ-NF-002 asks for.

```cpp
int MprisPlayerSelector::selectActiveIndex(const QList<MprisPlayer>& players) {
  int bestPlayingIndex = -1;
  qint64 bestPlayingTs = -1;
  int bestPausedIndex = -1;
  qint64 bestPausedTs = -1;

  for (int i = 0; i < players.size(); ++i) {
    const MprisPlayer& p = players.at(i);
    if (p.playbackStatus == QStringLiteral("Playing")) {
      if (p.lastActivityTimestampMs > bestPlayingTs) {
        bestPlayingTs = p.lastActivityTimestampMs;
        bestPlayingIndex = i;
      }
    } else if (p.playbackStatus == QStringLiteral("Paused")) {
      if (p.lastActivityTimestampMs > bestPausedTs) {
        bestPausedTs = p.lastActivityTimestampMs;
        bestPausedIndex = i;
      }
    }
    // "Stopped" (or any other status string) is never a candidate — REQ-F-007's explicit
    // "shall NOT select a Stopped player or use Stopped as a fallback".
  }

  return bestPlayingIndex >= 0 ? bestPlayingIndex : bestPausedIndex;  // -1 if neither exists
}
```

Two independent single passes (Playing-max and Paused-max) computed in one `O(n)` loop, then the
Playing result wins unconditionally if present — this directly encodes REQ-F-007's three-step
priority (Playing-by-recency, else Paused-by-recency, else none) without needing to sort or
partition the list.

**Tie-breaking**: if two players have the *exact same* `lastActivityTimestampMs` (both
millisecond-resolution wall-clock stamps), the strict `>` comparison means the **earlier-indexed**
player in `players_` wins (first-seen-in-registry-order, since `>` never lets a later equal-valued
entry displace the current best). `players_` is a `QList` in discovery order (not a `QHash`, which
would have unspecified iteration order) specifically so this tie-break is deterministic and
reproducible in tests, even though the spec does not define required tie-break behavior. See
"Known risks" for the real-world implication.

**Testing** (`tests/test_mpris_selection.cpp`, REQ-NF-002's acceptance criterion): construct
`QList<MprisPlayer>` literals directly (no `FakeMprisDBus`, no `MprisService`) for the three named
scenarios in REQ-F-007's acceptance criterion — Player-A Playing/ts=100 vs. Player-B Playing/ts=50
vs. Player-C Paused/ts=150 → expect index of Player-A; Player-A Paused/ts=100 vs. Player-C
Paused/ts=150 → expect index of Player-C; all three Stopped → expect `-1`.

### Activity-timestamp update logic (REQ-F-005, REQ-F-006)

Also a free/static function, `MprisService::updateActivityTimestamp`, taking the *previous*
`playbackStatus`/`trackId` explicitly as parameters (not re-derived from anything stateful) so it
is testable the same way:

```cpp
void MprisService::updateActivityTimestamp(MprisPlayer& player, const QString& previousStatus,
                                           const QString& previousTrackId, bool isDiscovery, qint64 nowMs) {
  const bool enteringPlaying = player.playbackStatus == QStringLiteral("Playing");
  if (isDiscovery) {
    if (enteringPlaying) {
      player.lastActivityTimestampMs = nowMs;   // REQ-F-006c
    }
    return;
  }
  const bool wasPlayingOrStopped = previousStatus != QStringLiteral("Playing");
  if (enteringPlaying && wasPlayingOrStopped) {
    player.lastActivityTimestampMs = nowMs;      // REQ-F-006a: Paused/Stopped -> Playing
    return;
  }
  if (enteringPlaying && !wasPlayingOrStopped && player.trackId != previousTrackId) {
    player.lastActivityTimestampMs = nowMs;      // REQ-F-006b: still Playing, trackid changed
  }
  // Playing -> Paused/Stopped, capability-only changes, and same-track repeats: no-op, matching
  // REQ-F-006's explicit "shall NOT update" list.
}
```

Called from `applyProperties`'s caller (`acquirePlayer` with `isDiscovery=true`,
`onPlayerPropertiesChanged` with `isDiscovery=false`) *after* `player.playbackStatus`/`trackId`
have already been overwritten with the new values, which is why `previousStatus`/`previousTrackId`
must be captured by the caller beforehand — the function only ever looks at "new" vs.
"passed-in old", never mutates anything except the timestamp field itself.

**Testing** (also `test_mpris_selection.cpp` or a sibling `test_mpris_activity_timestamp.cpp`):
directly exercises the three "shall update" branches and the three explicit "shall NOT update"
cases from REQ-F-006's acceptance criterion, with a fixed `nowMs` input for determinism (no real
`QDateTime::currentMSecsSinceEpoch()` calls inside the unit under test).

---

## Metadata extraction design

The MPRIS `Metadata` property's D-Bus signature is `a{sv}` — a dict of string keys to variants.
When this dict itself arrives as the *value* of another `a{sv}` (the outer `GetAll`/
`PropertiesChanged` reply, itself a dict of variants), Qt's D-Bus marshalling only auto-converts
the **outermost** `a{sv}` into a `QVariantMap`; each entry's *value*, if it is itself a container
(another dict, or an array like `xesam:artist`'s `as`), is left as a raw `QDBusArgument` wrapped in
a generic `QVariant` — Qt does not recursively guess the target C++ type for nested containers
reached through a `QVariant`. This is the exact class of trap already documented in this
codebase's `TrayItemProperties.cpp` (`SniIconPixmapList`/`SniToolTip` extraction) and called out
in `CLAUDE.md`'s D-Bus gotchas section — and per that same guidance, this design uses
`userType() == qMetaTypeId<QDBusArgument>()` checks, **not** `QVariant::canConvert<T>()` probes
(which can trigger a spurious `QDBusArgument: write from a read-only object` warning on this Qt
build when Qt attempts a write-side conversion).

Three levels are involved end-to-end:

```
GetAll(...) / PropertiesChanged(...)          →  QVariantMap  (outer a{sv}, auto-converted by Qt)
  ["Metadata"]                                →  QVariant, likely holding a raw QDBusArgument
    unwrapDict(...)                           →  QVariantMap  (the actual per-track metadata dict)
      ["xesam:title"]                         →  QVariant holding a plain QString — no trap, `s` is a
                                                   basic type and converts automatically
      ["xesam:artist"]                        →  QVariant, likely holding a raw QDBusArgument (`as`)
        unwrapStringList(...)                 →  QStringList
      ["mpris:trackid"]                       →  QVariant holding QDBusObjectPath (spec-compliant `o`)
                                                   OR a plain QString (seen on non-compliant players)
        unwrapTrackId(...)                    →  QString
```

```cpp
QVariantMap MprisMetadata::unwrapDict(const QVariant& raw) {
  if (raw.userType() == qMetaTypeId<QDBusArgument>()) {
    QVariantMap result;
    raw.value<QDBusArgument>() >> result;      // a{sv} -> QVariantMap has a registered operator>>
    return result;
  }
  if (raw.userType() == QMetaType::QVariantMap) {
    return raw.toMap();                        // test-seam path: FakeMprisDBus builds this directly
  }
  return {};
}

QStringList MprisMetadata::unwrapStringList(const QVariant& raw) {
  if (raw.userType() == qMetaTypeId<QDBusArgument>()) {
    QStringList result;
    raw.value<QDBusArgument>() >> result;      // as -> QStringList
    return result;
  }
  if (raw.userType() == QMetaType::QStringList) {
    return raw.toStringList();                 // test-seam path
  }
  return {};                                    // absent/empty array — REQ-F-012
}

QString MprisMetadata::unwrapTrackId(const QVariant& raw) {
  if (raw.userType() == qMetaTypeId<QDBusObjectPath>()) {
    return raw.value<QDBusObjectPath>().path(); // spec-compliant `o`
  }
  return raw.toString();                        // non-compliant `s`, or absent -> ""
}

MprisMetadata::Fields MprisMetadata::extractFields(const QVariantMap& metadataDict) {
  return Fields{
      .title = metadataDict.value(QStringLiteral("xesam:title")).toString(),
      .artists = unwrapStringList(metadataDict.value(QStringLiteral("xesam:artist"))),
      .trackId = unwrapTrackId(metadataDict.value(QStringLiteral("mpris:trackid"))),
  };
}
```

`activeTitle`/`activeArtist` fallback logic (REQ-F-011, REQ-F-012) lives in `MprisService`, not in
`MprisMetadata` — `Fields::title` and `Fields::artists` stay exactly what the player reported (even
if empty), and `MprisService::applyActiveSnapshot` is where "empty title → use Identity" and
"empty artist array → empty string, no placeholder" are applied, since the fallback needs
`player.identity`, which is outside `Fields`' scope.

**Testing** (`tests/test_mpris_metadata.cpp`): hand-built `QVariantMap`s using plain `QStringList`
and `QVariantMap` values (the test-seam path in each `unwrapX` function above) — no real
`QDBusArgument` object is constructible outside an active D-Bus call, so tests exercise the
"already demarshalled" branch, while `SystemMprisDBus`'s live path exercises the
`QDBusArgument`-extraction branch (covered by the manual/integration test per REQ-C-003's
acceptance criterion, run in a live Wayland session with a real MPRIS player).

---

## Key decisions with rationale

1. **Blocking `QDBusReply<T>` calls for player property reads, not `QDBusPendingCallWatcher`.**
   REQ-C-003 explicitly wants zero async delay for startup discovery ("no async delay in
   discovery" in its acceptance criterion). `NetworkManagerBackend` and `SystemInfoService`
   already use blocking `QDBusReply<T>`/`QDBusConnection::call(msg, QDBus::Block, ...)` extensively
   in this codebase for similar small, local-session-bus reads — this is not a novel pattern, and
   it eliminates an entire class of pending-call lifetime/ordering bugs (no `isError()` checks, no
   watcher parenting, no risk of a stale reply landing after a player has already been released).
   The trade-off is a GUI-thread stall bounded by the D-Bus call timeout if a player hangs — see
   "Known risks."

2. **`IMprisDBus` commands are `void`, not `QDBusPendingCall`-returning.** REQ-F-013 requires
   fire-and-forget with no reply handling. Making the seam's command methods `void` enforces this
   at the type level: there is no pending call object for a future maintainer to accidentally
   attach a watcher to and start reacting to replies, which would silently reintroduce an
   optimistic-update path REQ-F-014 forbids.

3. **`MprisPlayer` is a plain struct, not a `QObject`.** Every other backend in this codebase
   (`BrightnessService`, `PortalService`) keeps its *service* as the `QObject`/D-Bus-facing class
   and keeps small owned data (percent, color) as plain fields. MPRIS needs a *collection* of
   per-player records, so the natural extension is a collection of plain structs rather than a
   `QList<QObject*>` of per-player `QObject`s — this is what makes the selector and
   activity-timestamp logic trivially copyable, comparable, and constructible in tests (REQ-NF-002)
   with no `QObject` parent-ownership questions at all.

4. **Registry stored as `QList<MprisPlayer>`, not `QHash<QString, MprisPlayer>`.** A hash gives
   O(1) lookup by service name (and `indexOfService` is O(n) as a result, acceptable since a
   realistic player count is 1-4), but `QHash` iteration order is unspecified — feeding an
   unordered collection into the selector would make tie-breaking between equal-timestamp players
   nondeterministic across runs, unrelated to any actual application logic. An ordered `QList`
   keeps `MprisPlayerSelector::selectActiveIndex` deterministic and reproducible in both tests and
   production, at a lookup-cost trade-off deemed acceptable for the expected registry size.

5. **A seam (`IMprisDBus`) exists even though there is exactly one real backend.** REQ-NF-001
   requires it explicitly, but it is also the only way to satisfy REQ-NF-002's "no D-Bus
   initialization" constraint on the selection/timestamp tests — those tests need `MprisPlayer`
   instances to exist without any D-Bus involvement at all, which the plain-struct design already
   gives for free; the seam is what additionally makes `MprisService`'s *orchestration* logic
   (discovery, subscription, reselection wiring, command gating) testable end-to-end via
   `FakeMprisDBus`, matching this codebase's `IPortalDBus`/`BrightnessBackend` precedent rather
   than inventing a new testing shape for one more service.

6. **Per-monitor consistency (REQ-F-026) falls out of the QML singleton for free.** No
   per-monitor arbitration code is needed or written — every `TopBar.qml` instance imports the
   same module and binds to the same C++ object instance. This is identical to how
   `WeatherSection`/`ActiveWindowSection` already behave (`WeatherService`/`ActiveWindowService`
   are also global singletons; `ActiveWindowService` additionally tracks *per-monitor* state
   internally for its own different reasons, but `MprisService` explicitly does not need to, per
   the spec's "Scope boundary").

7. **Glyphs drawn with `QtQuick.Shapes`, not `Canvas`.** `WorkspaceEdgeArrow.qml` already
   establishes a `ShapePath`-based chevron pattern stroked with `HoloniightPalette` colors on this
   topbar. Reusing it avoids introducing `Canvas`'s `onPaint`-unqualified-property-access lint trap
   (documented in `CLAUDE.md`) for a component with no other reason to use `Canvas` over `Shapes`.

8. **Capability-gating logic lives in `MprisService::playPause()/next()/previous()`, not in
   QML.** REQ-F-015's rule ("Paused requires `canPlay`, Playing requires `canPause`") depends on
   `activePlaybackStatus`, which is exactly the kind of business rule this codebase keeps in C++
   (see `PowerProfilesService`'s echo-only state model) rather than duplicating in QML bindings —
   this also means `buttonEnabled` in `MprisControlButton.qml` (REQ-F-019, REQ-F-023) is a pure
   read of the already-gated `MprisService.canX` booleans, with no independent gating logic on the
   QML side that could drift out of sync with what a click would actually do.

---

## Alternatives considered

1. **Polling `GetAll()` on a timer vs. `PropertiesChanged` subscription.** Rejected. REQ-F-002
   explicitly requires signal-driven updates ("without requiring a manual poll"), and every
   existing D-Bus-property-watching service in this codebase (`BatteryService`, `NetworkService`,
   `PowerProfilesService`, `IdleService`) already uses the `PropertiesChanged` subscription pattern
   — polling would be both spec-non-compliant and stylistically inconsistent with the rest of the
   backend layer.

2. **A config-driven topbar widget registry vs. `MprisSection` as a fixed, directly-instantiated
   component.** Rejected. `TopBar.qml` composes its sections as fixed, directly-instantiated QML
   components with no dynamic registry anywhere in this codebase (per the architecture-context
   brief). Building a generic registry for one new section would be a large, unrequested
   architectural change with no other consumer, and REQ-C-001/REQ-F-025 both describe a fixed
   insertion point, not a configurable one.

3. **Single monolithic `MprisService` (owning raw property maps) vs. `MprisService` +
   `MprisPlayer` split.** Considered keeping everything as `QHash<QString, QVariantMap>` inside
   `MprisService` directly, without a dedicated `MprisPlayer` struct. Rejected because it would
   force `MprisPlayerSelector` and the activity-timestamp function to operate on raw
   `QVariantMap`s (re-parsing `"PlaybackStatus"`/`lastActivityTimestampMs` out of loosely-typed
   maps on every call), which is both slower and much harder to unit-test cleanly than a typed
   struct — REQ-NF-002 specifically asks for a typed, mockable player representation.

4. **Full player-object QML exposure (a `QAbstractListModel` of all known players) vs. only the
   selected player's flat properties.** Rejected as out of scope. The spec's Non-Goals explicitly
   exclude manual player selection and a player-switcher UI; exposing a full list model would
   invite building exactly that UI later without being asked for it now, and would need
   per-player `QObject` wrappers (contradicting decision #3 above) purely to satisfy a QML list
   model's rowChanged/dataChanged contract that nothing in this feature cycle consumes.

5. **`QDBusServiceWatcher` per-known-player vs. one wildcard `NameOwnerChanged` subscription
   filtered in code.** `QDBusServiceWatcher` only watches literal, pre-registered service names —
   it cannot watch a pattern like `org.mpris.MediaPlayer2.*`, and a *newly-appearing* player's exact
   name is unknown in advance, so a `QDBusServiceWatcher` cannot be used for *discovery* at all
   (only for watching an *already-known* name's continued presence, which this design does not
   need since `PropertiesChanged` disconnection plus the general `NameOwnerChanged` handler already
   covers disappearance). This mirrors `PortalService`'s own reason for combining a
   `QDBusServiceWatcher` (for its one fixed portal broker name) with a raw, prefix-filtered
   `NameOwnerChanged` connection (for the dynamic backend-list case) — MPRIS's names are all
   dynamic, so only the raw `NameOwnerChanged` half of that pattern applies here.

---

## Known risks

- **Players that lie about capability flags.** Some MPRIS implementations report `CanPause: true`
  and still no-op or error on `Pause`. Per REQ-F-013, commands are strictly fire-and-forget with no
  reply handling — this design has no way to detect or surface such a failure, by design. The
  worst case is a click that visibly does nothing (the glyph never flips because no
  `PropertiesChanged` ever arrives), not a crash or stuck state. Not mitigated; explicitly
  deferred, consistent with REQ-F-014's "or the command fails silently and state does not change
  at all" acceptance path.

- **`DesktopEntry` absent or wrong.** Some players (especially Electron-based ones, or players
  invoked directly without a `.desktop` file) omit `DesktopEntry` entirely or report a name that
  doesn't resolve via the icon provider. `MprisWidget.qml`'s `image://icon/` binding on an empty or
  unresolvable string is expected to fall back to whatever empty-string/placeholder behavior the
  shared icon provider already has for other consumers (`LauncherResultRow.qml`,
  `DefaultAppRow.qml`) — this design does not add a second, MPRIS-specific fallback icon, since
  REQ-F-018 only specifies the resolution mechanism, not a fallback glyph.

- **Multiple players transitioning to `Playing` simultaneously (same-millisecond
  `lastActivityTimestampMs`).** REQ-F-007 does not define tie-break behavior. This design's
  deterministic-but-arbitrary "first player in registry insertion order wins" (decision #4 above)
  means which of two simultaneously-started players "wins" the pill depends on D-Bus signal
  delivery order for their respective `NameOwnerChanged`/first-`Playing` events — a real
  nondeterminism at the OS/bus level that no in-process tie-break rule can fully eliminate. Not
  mitigated further; flagged here as expected, spec-silent behavior rather than a defect.

- **MPRIS service crashes or restarts mid-session without dropping its bus name cleanly, or drops
  the name and never sends the corresponding `PropertiesChanged`/`NameOwnerChanged` due to an
  abrupt disconnect.** The `NameOwnerChanged` signal is emitted by `dbus-daemon` itself on socket
  disconnect (not by the crashing process), so an ungraceful crash still reliably produces the
  disappearance signal and `releasePlayer()` still runs — this is a property of D-Bus itself, not
  something this design has to implement. The remaining risk is a player that hangs (process alive,
  socket open, but not responding) rather than crashing — covered by the 500 ms
  `QDBusInterface::setTimeout()` on `getAllPlayerProperties`/property reads described under
  `SystemMprisDBus` above, which turns "the GUI thread blocks forever" into "the GUI thread blocks
  for at most 500 ms and that one player fails to register" (an error path this design should log
  via `qCWarning` and otherwise skip, rather than crash or retry-loop).

- **REQ-NF-005 (`Q_CLASSINFO("D-Bus Interface", ...)`) does not apply.** Called out explicitly
  per this design brief's instruction: nothing in this feature implements or exports a D-Bus
  interface — `MprisService` and `SystemMprisDBus` are exclusively D-Bus **clients** reading other
  processes' already-exported `org.mpris.MediaPlayer2.Player` /
  `org.freedesktop.DBus.Properties` interfaces. REQ-NF-005's own acceptance criterion anticipates
  this ("If no custom service is exported... this requirement does not apply"). No code changes
  address this item; it is a documented non-issue, not a gap.

- **Room left for deferred features, without designing for them now.** The Non-Goals list defers
  a media popup, player switcher, seek/position, artwork, and a public D-Bus service. This design
  does not preclude any of them: `MprisPlayer`'s fields are a strict subset of what a future popup
  would need (adding `Position`/`artUrl` tracking later is additive, not a rework); the existing
  `apps/shell/qml/RightSidebar/Tabs/Media/SidebarMedia.qml` placeholder tab is untouched by this
  feature and remains the natural home for a future detailed player view; and `MprisPlayerSelector`
  operating on an explicit `QList<MprisPlayer>` rather than being baked into `MprisService`'s
  internals would let a future manual-selection feature reuse the same pure function with a
  different, user-overridden input list. None of this is built now — noted only so the next phase
  does not need to re-architect the backend to get it.
