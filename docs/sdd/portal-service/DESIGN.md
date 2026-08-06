# Portal Service — Architecture Design

## 1. Component Overview

### File Layout

```
src/services/portal/
    PortalService.h        # Public class declaration, Q_PROPERTYs, Q_INVOKABLEs
    PortalService.cpp      # All implementation; Q_LOGGING_CATEGORY; #include "PortalService.moc"
    NullPortalBackend.h    # Test seam interface — pure-virtual for D-Bus injection
    NullPortalBackend.cpp  # No-op implementation used by unit tests
```

No protocol XML is needed: `xdg-desktop-portal` exposes a standard D-Bus service; all wire-level
access goes through QtDBus types (`QDBusInterface`, `QDBusMessage`, `QDBusPendingCallWatcher`).

### Class List

| Class | Location | Purpose |
|---|---|---|
| `PortalService` | `PortalService.h/.cpp` | QML_SINGLETON; owns all D-Bus handles; exposes broker diagnostics, Settings values, and invokable portal calls to QML |
| `IPortalDBus` | `NullPortalBackend.h` | Abstract seam for D-Bus calls (`isNameRegistered`, `asyncCall`, `listNames`); injected via constructor so unit tests never hit the real bus |
| `SystemPortalDBus` | `NullPortalBackend.h/.cpp` | Production implementation of `IPortalDBus`; delegates to `QDBusConnection::sessionBus()` |
| `NullPortalDBus` | `NullPortalBackend.h/.cpp` | Test-only implementation: `isNameRegistered` returns a configurable bool; `listNames` returns a configurable list; all `asyncCall` returns immediately with a pre-set reply |

### Header vs. .cpp Split

`PortalService.h` contains only the class declaration, Q_PROPERTY macros, Q_SIGNAL declarations,
and Q_INVOKABLE signatures — nothing that touches QtDBus types directly. This keeps the header
lightweight and avoids exposing internal D-Bus handle types (`QDBusServiceWatcher`,
`QDBusPendingCallWatcher`) to translation units that only need the QML interface.

All implementation detail — D-Bus constants, async call chains, `QDBusPendingCallWatcher` creation,
`QDBusArgument` deserialization — lives in `PortalService.cpp`. Because `PortalService` uses
`Q_OBJECT` in the `.cpp` compilation unit (via the anonymous namespace pattern for the watcher
slots), `#include "PortalService.moc"` is placed at the end of `PortalService.cpp`.

---

## 2. PortalService Class Design

### Q_PROPERTY List

```cpp
// Broker diagnostics
Q_PROPERTY(bool available READ available NOTIFY availableChanged FINAL)
Q_PROPERTY(QStringList interfaces READ interfaces NOTIFY interfacesChanged FINAL)
Q_PROPERTY(QStringList backends READ backends NOTIFY backendsChanged FINAL)

// Per-interface availability (derived from interfaces)
Q_PROPERTY(bool settingsAvailable READ settingsAvailable NOTIFY settingsAvailableChanged FINAL)
Q_PROPERTY(bool fileChooserAvailable READ fileChooserAvailable NOTIFY fileChooserAvailableChanged FINAL)
Q_PROPERTY(bool openUriAvailable READ openUriAvailable NOTIFY openUriAvailableChanged FINAL)
Q_PROPERTY(bool inhibitAvailable READ inhibitAvailable NOTIFY inhibitAvailableChanged FINAL)
Q_PROPERTY(bool screenCastAvailable READ screenCastAvailable NOTIFY screenCastAvailableChanged FINAL)
Q_PROPERTY(bool globalShortcutsAvailable READ globalShortcutsAvailable NOTIFY globalShortcutsAvailableChanged FINAL)

// Settings portal values
Q_PROPERTY(int colorScheme READ colorScheme NOTIFY colorSchemeChanged FINAL)
Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged FINAL)
```

All properties are read-only from QML. There are no WRITE accessors — property mutations are driven
entirely by incoming D-Bus signals and async reply handlers.

### Q_INVOKABLE Signatures

```cpp
// Opens the portal file chooser dialog. windowHandle is an XDG window identifier
// (e.g., "wayland:<handle>"); pass empty string when unavailable.
Q_INVOKABLE void openFile(const QString& window_handle,
                           const QString& title,
                           const QStringList& filters);

// Opens a URI via the portal OpenURI interface (handles http://, file://, etc.).
Q_INVOKABLE void openUri(const QString& uri);
```

Both invokables are no-ops with a `qCWarning` when the respective portal interface is unavailable
(`fileChooserAvailable == false` / `openUriAvailable == false`). Neither blocks the main thread.

### Private Members

```cpp
private:
    // D-Bus abstraction seam (production vs. test injection)
    std::unique_ptr<IPortalDBus> dbus_;

    // Service watcher for the broker (exact name match)
    std::unique_ptr<QDBusServiceWatcher> broker_watcher_;

    // Broker diagnostics state
    bool available_{false};
    QStringList interfaces_;
    QStringList backends_;

    // Derived availability booleans (computed from interfaces_)
    bool settings_available_{false};
    bool file_chooser_available_{false};
    bool open_uri_available_{false};
    bool inhibit_available_{false};
    bool screen_cast_available_{false};
    bool global_shortcuts_available_{false};

    // Settings portal values
    int color_scheme_{0};
    QColor accent_color_;

    // Re-entrancy guard: true while an async Introspect probe is in flight
    bool probe_in_flight_{false};

    // Startup timestamp for REQ-NF-001 measurement
    std::chrono::steady_clock::time_point startup_time_;

    // Private helpers
    void startProbe();
    void onIntrospectReply(QDBusPendingCallWatcher* watcher);
    void onListNamesReply(QDBusPendingCallWatcher* watcher);
    void onSettingsReadReply(const QString& key, QDBusPendingCallWatcher* watcher);
    void onSettingChanged(const QString& ns, const QString& key, const QDBusVariant& value);
    void applyInterfaceList(const QStringList& ifaces);
    void applyBackendList(const QStringList& names);
    bool setAvailable(bool val);          // returns true if changed
    bool setColorScheme(int val);         // returns true if changed
    bool setAccentColor(const QColor& c); // returns true if changed
    QColor decodeAccentColor(const QDBusArgument& arg) const;
```

All state-setter helpers return `bool` so callers can emit NOTIFY signals conditionally (no
spurious emissions when the value is unchanged, satisfying REQ-F-025).

### Constructor Injection Seam

Mirroring the `BrightnessService` / `IdleService` pattern:

```cpp
// Production: connects to the real session bus.
explicit PortalService(QObject* parent = nullptr);

// Test seam: inject a fake D-Bus abstraction; no real bus contact.
explicit PortalService(std::unique_ptr<IPortalDBus> dbus, QObject* parent = nullptr);
```

The test seam allows unit tests to exercise Settings deserialization, interface-list parsing,
graceful degradation, and watcher logic without a running compositor or portal broker.

---

## 3. Startup Probe Sequence

### Overview

The constructor calls a private `init()` helper that fires the probe asynchronously via
`QTimer::singleShot(0, ...)`. This defers all D-Bus I/O past the constructor return, so
`ShellApplication` can finish wiring signal connections before any callbacks fire.

### Step-by-Step

```
PortalService ctor
    └─ init()
        └─ QTimer::singleShot(0, this, &PortalService::startProbe)

startProbe()
    1. Guard: if (probe_in_flight_) return;          // REQ-F-025
       probe_in_flight_ = true;
       startup_time_ = steady_clock::now();

    2. Check name registration (async via IPortalDBus::isNameRegistered):
       → asyncCall("org.freedesktop.DBus", "/org/freedesktop/DBus",
                   "org.freedesktop.DBus", "NameHasOwner",
                   "org.freedesktop.portal.Desktop")
       → QDBusPendingCallWatcher → onNameHasOwnerReply()

onNameHasOwnerReply()
    3. Parse reply → bool name_owned
       if (!name_owned):
           setAvailable(false)
           probe_in_flight_ = false
           // leave interfaces_/backends_ empty (REQ-F-022)
           return

    4. setAvailable(true)

    5. Fire Introspect (async):
       asyncCall("org.freedesktop.portal.Desktop",
                 "/org/freedesktop/portal/desktop",
                 "org.freedesktop.DBus.Introspectable",
                 "Introspect")
       → QDBusPendingCallWatcher → onIntrospectReply()

    6. Fire ListNames (async, parallel with step 5):
       asyncCall("org.freedesktop.DBus", "/org/freedesktop/DBus",
                 "org.freedesktop.DBus", "ListNames")
       → QDBusPendingCallWatcher → onListNamesReply()

onIntrospectReply()
    7. Parse XML string → extract <interface name="..."/> elements
       Filter to names beginning with "org.freedesktop.portal."
       applyInterfaceList(ifaces)
           → derive six booleans from the list
           → emit interfacesChanged() + per-boolean Changed() for each that flipped

    8. If settingsAvailable: readSettingsAsync()
       probe_in_flight_ = false
       log elapsed time (INFO) → REQ-NF-001

onListNamesReply()
    9. Filter names starting with "org.freedesktop.impl.portal."
       applyBackendList(names) → emit backendsChanged()
```

Steps 5 and 6 are fired concurrently (two independent `QDBusPendingCallWatcher` objects) so the
Introspect round-trip and the `ListNames` round-trip overlap. Each watcher clears
`probe_in_flight_` independently only when _both_ are done (use an in-flight counter initialized
to 2, decremented in each reply handler; clear the flag when it reaches 0).

### Re-entrancy Guard (REQ-F-025)

`probe_in_flight_` is set to `true` at the top of `startProbe()` and is checked before entering.
The broker watcher's `serviceRegistered` handler calls `startProbe()`, so a second
`NameOwnerChanged` while a probe is running is silently dropped. This prevents the double-probe
that would happen if the broker restarts quickly.

---

## 4. Settings Portal: Read and Subscribe

### Asynchronous Read at Startup

Called from `startProbe()` after `settingsAvailable` becomes `true`:

```cpp
void PortalService::readSettingsAsync() {
    // Read color-scheme
    auto* watcher_cs = dbus_->asyncCall(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings",
        "Read",
        QStringLiteral("org.freedesktop.appearance"),
        QStringLiteral("color-scheme"));
    connect(watcher_cs, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher* w) {
                onSettingsReadReply(QStringLiteral("color-scheme"), w);
            });

    // Read accent-color (parallel, independent watcher)
    auto* watcher_ac = dbus_->asyncCall(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings",
        "Read",
        QStringLiteral("org.freedesktop.appearance"),
        QStringLiteral("accent-color"));
    connect(watcher_ac, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher* w) {
                onSettingsReadReply(QStringLiteral("accent-color"), w);
            });
}
```

Both reads are issued in parallel. Each `QDBusPendingCallWatcher` is parented to `this` so it is
deleted automatically when `PortalService` is destroyed.

### `accent-color` (ddd) Struct Deserialization

The `org.freedesktop.portal.Settings.Read` reply for `accent-color` has D-Bus signature `v`
(variant). Inside that variant lives another variant (`v`), and inside that lives a struct `(ddd)`.

The CLAUDE.md D-Bus trap applies here: **never call `QVariant::canConvert<T>()`** on a QVariant
that may hold a `QDBusArgument` — Qt may attempt a write-mode conversion and log
`QDBusArgument: write from a read-only object`. Check `userType()` explicitly first.

Step-by-step deserialization:

```cpp
QColor PortalService::decodeAccentColor(const QDBusVariant& outer) const {
    // outer.variant() is the inner v(ddd)
    const QVariant inner = outer.variant();

    // REQ-F-024: guard against missing/malformed struct
    if (inner.userType() != qMetaTypeId<QDBusArgument>()) {
        // Some portals return a plain QVariant<double> tuple; attempt direct conversion.
        // If that also fails, log and return a null QColor.
        qCWarning(lcPortalService) << "accent-color: unexpected inner variant type"
                                   << inner.typeName();
        return {};
    }

    QDBusArgument arg = inner.value<QDBusArgument>();
    if (arg.currentType() != QDBusArgument::StructureType) {
        qCWarning(lcPortalService) << "accent-color: QDBusArgument is not a struct";
        return {};
    }

    double red{};
    double green{};
    double blue{};

    arg.beginStructure();
    arg >> red >> green >> blue;
    arg.endStructure();

    // Clamp: portal spec guarantees [0,1] but guard against out-of-range data.
    auto clamp01 = [](double val) -> double { return std::clamp(val, 0.0, 1.0); };
    return QColor::fromRgbF(clamp01(red), clamp01(green), clamp01(blue));
}
```

When the outer reply arrives in `onSettingsReadReply`:

```cpp
void PortalService::onSettingsReadReply(const QString& key, QDBusPendingCallWatcher* watcher) {
    QDBusPendingReply<QDBusVariant> reply = *watcher;
    watcher->deleteLater();

    if (reply.isError()) {
        qCWarning(lcPortalService) << "Settings.Read(" << key << ") error:"
                                   << reply.error().message();
        return;
    }

    const QDBusVariant result = reply.value();

    if (key == QStringLiteral("color-scheme")) {
        const int scheme = result.variant().toInt();
        if (setColorScheme(scheme)) {
            emit colorSchemeChanged();
        }
    } else if (key == QStringLiteral("accent-color")) {
        const QColor color = decodeAccentColor(result);
        if (color.isValid() && setAccentColor(color)) {
            emit accentColorChanged();
        }
    }
}
```

### Signal Subscription: SettingChanged

`SettingChanged` has signature `(sss)` — `(namespace, key, value)` where `value` is a `QDBusVariant`.
Connect at startup unconditionally (not gated on `settingsAvailable`), because the signal arrives
on the well-known path regardless of whether the initial read succeeded:

```cpp
QDBusConnection::sessionBus().connect(
    QStringLiteral("org.freedesktop.portal.Desktop"),
    QStringLiteral("/org/freedesktop/portal/desktop"),
    QStringLiteral("org.freedesktop.portal.Settings"),
    QStringLiteral("SettingChanged"),
    this,
    SLOT(onSettingChanged(QString, QString, QDBusVariant)));
```

Handler:

```cpp
void PortalService::onSettingChanged(const QString& ns,
                                      const QString& key,
                                      const QDBusVariant& value) {
    if (ns != QStringLiteral("org.freedesktop.appearance")) {
        return;
    }
    if (key == QStringLiteral("color-scheme")) {
        const int scheme = value.variant().toInt();
        if (setColorScheme(scheme)) {
            emit colorSchemeChanged();
        }
    } else if (key == QStringLiteral("accent-color")) {
        // value arrives as QDBusVariant wrapping QVariant wrapping QDBusArgument (ddd)
        const QColor color = decodeAccentColor(value);
        if (color.isValid() && setAccentColor(color)) {
            emit accentColorChanged();
        }
    }
}
```

The `SLOT(onSettingChanged(...))` macro requires the `QDBusVariant` include in the header, but
does not expose any other D-Bus types to callers.

### Async Race: SettingChanged Before Read Reply (REQ-NF-003 / REQ-F-025)

If a `SettingChanged` signal arrives while the initial `Read` is still in flight, the handler
updates the cached value and emits the NOTIFY signal. When the `Read` reply later arrives in
`onSettingsReadReply`, the `setColorScheme`/`setAccentColor` helpers check for equality — no
duplicate emission. This is safe because both paths write the same field and the equality check
is the deduplication gate.

---

## 5. Backend Detection

### Initial List via `ListNames`

`QDBusServiceWatcher` fires only for _changes_ after it is registered. It does not enumerate names
that were already registered before the watcher was created. To get the initial backend list,
`ListNames` on `org.freedesktop.DBus` is called asynchronously in `startProbe()` (step 6 above).

```cpp
void PortalService::onListNamesReply(QDBusPendingCallWatcher* watcher) {
    QDBusPendingReply<QStringList> reply = *watcher;
    watcher->deleteLater();

    if (reply.isError()) {
        qCWarning(lcPortalService) << "ListNames error:" << reply.error().message();
        // Leave backends_ empty; watcher will catch future registrations.
        return;
    }

    const QStringList all_names = reply.value();
    QStringList impl_names;
    impl_names.reserve(4);
    for (const QString& name : all_names) {
        if (name.startsWith(QStringLiteral("org.freedesktop.impl.portal."))) {
            impl_names.append(name);
        }
    }
    applyBackendList(impl_names);
}
```

The `for` loop over `all_names` does not use `std::ranges::copy_if` + a `back_inserter` because
the reserve-then-append pattern is more readable and avoids a temporary lambda (cognitive
complexity stays well below 25). `std::ranges::any_of` is used where a predicate query is needed
over a range.

### Ongoing Changes via NameOwnerChanged

The service subscribes to `org.freedesktop.DBus.NameOwnerChanged` and filters names beginning with
`org.freedesktop.impl.portal.`. Every matching owner change triggers `refreshBackends()`, which
re-runs `ListNames` asynchronously. The `backends` list is therefore rebuilt from a fresh bus
snapshot rather than maintained as an incremental delta.

---

## 6. QDBusServiceWatcher Setup

### Broker Watcher (Exact Name Match)

```cpp
broker_watcher_ = std::make_unique<QDBusServiceWatcher>(
    QStringLiteral("org.freedesktop.portal.Desktop"),
    QDBusConnection::sessionBus(),
    QDBusServiceWatcher::WatchForOwnerChange,
    this);

connect(broker_watcher_.get(), &QDBusServiceWatcher::serviceRegistered,
        this, &PortalService::onBrokerAppeared);
connect(broker_watcher_.get(), &QDBusServiceWatcher::serviceUnregistered,
        this, &PortalService::onBrokerDisappeared);
```

`onBrokerAppeared` calls `startProbe()` (which is guarded by `probe_in_flight_`).

`onBrokerDisappeared` sets `available_ = false`, clears `interfaces_`, resets all six availability
booleans to `false`, and emits the corresponding NOTIFY signals. It does **not** clear `backends_`
— backends may outlive the broker during a restart race.

### Backend Owner-Change Monitoring

Backend services are monitored through `org.freedesktop.DBus.NameOwnerChanged` instead of a
`QDBusServiceWatcher` wildcard. `QDBusServiceWatcher` only documents exact-name matching; the
service therefore subscribes to owner changes explicitly, filters names beginning with
`org.freedesktop.impl.portal.`, and re-issues an async `ListNames` call whenever a matching
backend appears or disappears.

```cpp
dbus_->connectNameOwnerChanged(this, SLOT(onNameOwnerChanged(QString, QString, QString)));
```

`onNameOwnerChanged()` ignores non-portal names and no-op owner changes. For matching backend
names, `refreshBackends()` fires async `ListNames`, waits for the reply, and calls
`applyBackendList()`.

### Signal Connections Summary

| Watcher | Signal | Handler | Effect |
|---|---|---|---|
| `broker_watcher_` | `serviceRegistered` | `onBrokerAppeared` | Re-probe broker, re-enumerate interfaces |
| `broker_watcher_` | `serviceUnregistered` | `onBrokerDisappeared` | Set `available=false`, clear interfaces and booleans |
| D-Bus `NameOwnerChanged` | backend owner changed | `onNameOwnerChanged` | Re-list `ListNames`, update `backends` |
| (session bus connect) | `SettingChanged` | `onSettingChanged` | Update `colorScheme` or `accentColor` |

---

## 7. QML Singleton Registration

### ShellApplication.cpp

`PortalService` is constructed in the `ShellApplication` member-initializer list alongside the
other services, then registered via the existing `reg()` lambda in `registerQmlTypes()`:

```cpp
// In ShellApplication private members (ShellApplication.h):
PortalService* portal_service_{nullptr};

// In ShellApplication ctor member initializer:
portal_service_(new PortalService(this)),

// In registerQmlTypes():
reg(portal_service_, "PortalService");
```

No `start()` method is needed — `PortalService` fires its startup probe via
`QTimer::singleShot(0, ...)` from the constructor, so by the time `startServices()` is called the
probe is already queued. No inter-service dependencies require `portal_service_` to be wired up in
`startServices()` at this time (it is a standalone data source with no dependency on
`IdleService`, `NotificationService`, etc.).

### CMakeLists.txt Additions

Add the following lines to the `add_library(holonight_services STATIC ...)` block, after the
`brightness/` block:

```cmake
src/services/portal/PortalService.h
src/services/portal/PortalService.cpp
src/services/portal/NullPortalBackend.h
src/services/portal/NullPortalBackend.cpp
```

Add the include directory to `target_include_directories(holonight_services PUBLIC ...)`:

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/src/services/portal
```

No new `target_link_libraries` entries are needed: `Qt6::DBus` is already in the link set for
`holonight_services` (used by `IdleService`, `PowerProfilesService`, etc.).

No new protocol XML or `wayland-scanner` invocations are needed.

---

## 8. Key Decisions with Rationale

### Why Async Introspect Rather Than a Hardcoded Interface List

The set of portal interfaces available on a given system depends on the installed
`xdg-desktop-portal` version and which backend packages are present. Hardcoding a list (e.g.,
`{"org.freedesktop.portal.Settings", "org.freedesktop.portal.FileChooser", ...}`) would report
interfaces as available even if the running broker does not expose them, and would miss new
interfaces added in future portal versions. Introspect gives an authoritative answer from the
broker itself at runtime. The XML parsing cost is negligible (<1ms) and stays well within the
500ms startup budget (REQ-NF-001).

### Why QtDBus Only (No libportal)

REQ-C-004. Beyond the constraint: `libportal` is a C library requiring additional pkg-config
wiring, increases binary size, and introduces a transitive dependency on GLib/GObject. The portal
D-Bus API surface needed here — `Introspect`, `Settings.Read`, `SettingChanged`, `FileChooser.OpenFile`,
`OpenURI.OpenURI` — is narrow enough that QtDBus handles it with less code than the libportal
bindings would require. `QDBusArgument` deserialization, while verbose, is well understood and
covered by existing CLAUDE.md guidance.

### Why No Backend Adapter Pattern

The xdg-desktop-portal broker is itself the abstraction layer. All backends (`org.freedesktop.impl.portal.*`)
are transparent to consumers: the broker routes calls and exposes a unified API surface. Introducing
a `PortalBackend` abstract class in the shell would duplicate the broker's routing job without
adding value. The only abstraction introduced is `IPortalDBus`, which is a test seam for the
session bus calls — not a backend strategy.

### Why `accent-color` Requires Explicit QDBusArgument Deserialization

`accent-color` has D-Bus type `(ddd)`: a struct of three doubles. When received via
`QDBusPendingReply<QDBusVariant>`, the outer `QDBusVariant::variant()` yields a `QVariant` whose
`userType()` is `qMetaTypeId<QDBusArgument>()`. Calling `QVariant::canConvert<SomeType>()` on
this QVariant causes Qt to attempt a write-mode conversion on the read-mode `QDBusArgument`,
logging `QDBusArgument: write from a read-only object` and producing garbage data. The only
correct path is to check `userType() == qMetaTypeId<QDBusArgument>()` first, extract the
`QDBusArgument`, then call `beginStructure()` / `>>` / `endStructure()` explicitly. This is the
same trap documented in CLAUDE.md for `PowerProfilesService`.

### Startup Probe Strategy: Parallel Async Calls

Introspect and `ListNames` are independent of each other: the Introspect reply tells us which
portal interfaces the broker exports; the `ListNames` reply tells us which backend services are
registered. Firing them in parallel (two `QDBusPendingCallWatcher` objects) rather than
sequentially cuts the worst-case probe time roughly in half. The in-flight counter (initialized
to 2, decremented in each reply) tracks when both are done before clearing `probe_in_flight_` and
logging the elapsed time.

The `NameHasOwner` call (step 2) must precede both, since the Introspect call would fail or
time out if the broker is absent, and early-exit on `available = false` avoids unnecessary work.

---

## 9. Alternatives Considered

### libportal vs. QtDBus Direct

`libportal` provides typed C bindings for portal calls and handles some D-Bus plumbing. Rejected
because: (1) it is a GLib-based library incompatible with the Qt-only link policy (REQ-C-004);
(2) it does not support the Introspect / backend-detection use cases directly; (3) the extra
abstraction adds no value over `QDBusInterface::asyncCall()` for the narrow API surface used here.

### Polling vs. NameOwnerChanged for Backend List

Periodic polling (e.g., every 5 seconds) was rejected because it adds noise to the D-Bus daemon,
consumes CPU during idle, and introduces latency (up to `interval` ms) before backend changes are
reflected. `QDBusServiceWatcher` with `WatchForOwnerChange` delivers changes within one
event-loop cycle and has zero idle cost.

Relying solely on `NameOwnerChanged` (without an initial `ListNames`) was also considered but
rejected: `NameOwnerChanged` only fires for transitions (name gained/lost owner), not for names
already registered when the watcher is created. The combination — initial `ListNames` snapshot +
ongoing `QDBusServiceWatcher` for incremental updates — is the only correct strategy.

### Caching Interface List to Disk vs. Re-Probing on Broker Restart

Caching the `interfaces` list to disk (e.g., in `~/.cache/`) would allow QML to bind to
properties before the async probe completes. Rejected because: (1) the cached list could be
stale if portal packages are upgraded between sessions; (2) the async probe completes within
~50ms on a local D-Bus socket — fast enough that QML sees the correct values before any user
interaction can depend on them; (3) cache invalidation logic adds complexity for negligible UX
gain. Re-probing on every broker restart is the simpler, always-correct strategy.

---

## 10. Known Risks

### `accent-color` Absent from Some Portal Implementations

`org.freedesktop.portal.Settings` `accent-color` was added in `xdg-desktop-portal` 1.15. Systems
running older portal versions (e.g., Ubuntu 22.04 ships 1.14) will receive a D-Bus error reply
to the `Read("org.freedesktop.appearance", "accent-color")` call. The `onSettingsReadReply`
handler already gates on `reply.isError()` and logs a `qCWarning` without throwing. `accentColor`
stays at its default (`QColor()` — invalid/transparent). QML bindings that depend on `accentColor`
should guard with `accentColor.valid` before use.

### Backend Owner-Change Filtering

Backend updates do not rely on undocumented `QDBusServiceWatcher` wildcard matching. The
`NameOwnerChanged` handler ignores non-portal names and no-op owner changes, then refreshes the
backend list from `ListNames` for every matching `org.freedesktop.impl.portal.*` service name.

### Async Race: SettingChanged Before Initial Read Reply

If the portal broker emits `SettingChanged` for `color-scheme` or `accent-color` in the ~50ms
window between the shell connecting the signal and the `Read` reply arriving, the handler will
update the cached value and emit NOTIFY. When the `Read` reply arrives later, `setColorScheme` /
`setAccentColor` compare the new value against the already-updated cache — if they match, no
duplicate signal is emitted. The QML binding therefore reflects the most recently known value at
all times, and the race produces at most one extra signal emission (the one from the early
`SettingChanged`), which is harmless.

### Re-entrancy During Broker Restart

If the broker crashes and restarts within the ~100ms probe window, `onBrokerDisappeared` will
fire while `probe_in_flight_` is `true`. The disappear handler does not touch `probe_in_flight_`
(it only sets `available_ = false` and clears interface booleans). When `onBrokerAppeared` fires
next, `startProbe()` will return early because `probe_in_flight_` is still `true`, so the
re-probe for the new broker instance is skipped. Mitigation: `probe_in_flight_` is also cleared
in `onBrokerDisappeared` (after resetting state), ensuring the appear handler can start a fresh
probe for the restarted broker.
