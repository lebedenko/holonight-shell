# Portal Service Specification

## Overview

The `PortalService` is a QML_SINGLETON C++ service that bridges the HoloNight shell to the XDG Desktop Portal broker and portal backends on the system. It provides:

1. **Broker diagnostics**: detect portal broker availability, enumerate available interfaces and backends
2. **Settings portal consumer**: subscribe to and reflect system appearance settings (color scheme, accent color)
3. **Portal invokables**: application-level plumbing for file chooser and URI opening (no active callers yet)
4. **Live daemon monitoring**: re-probe availability when D-Bus services appear/disappear

The service does not implement portal backends, use the portal `Inhibit` interface (logind inhibitor in idle-management is sufficient), or add new QML UI — it is a data and control bridge only.

---

## Functional Requirements

### Startup Diagnostics

**REQ-F-001** (Ubiquitous)  
The service shall probe for the XDG Desktop Portal broker at startup by checking for the `org.freedesktop.portal.Desktop` D-Bus service name.

- **Acceptance criterion**: The `available` Q_PROPERTY is `true` if and only if the name is registered on the session bus at probe time.

**REQ-F-002** (Ubiquitous)  
The service shall enumerate all portal interfaces exported by the broker at startup.

- **Acceptance criterion**: Query `org.freedesktop.portal.Desktop` via D-Bus Introspection; store the interface names (e.g., `org.freedesktop.portal.Settings`, `org.freedesktop.portal.FileChooser`) in the `interfaces` Q_PROPERTY as a QStringList.

**REQ-F-003** (Ubiquitous)  
The service shall detect all portal backends available on the system at startup.

- **Acceptance criterion**: List all D-Bus service names matching the pattern `org.freedesktop.impl.portal.*` (e.g., `org.freedesktop.impl.portal.Hyprland`, `org.freedesktop.impl.portal.gtk`); store them in the `backends` Q_PROPERTY as a QStringList.

**REQ-F-004** (Ubiquitous)  
The service shall expose six named boolean Q_PROPERTYs indicating availability of specific portal interfaces.

- **Acceptance criterion**: Each of `settingsAvailable`, `fileChooserAvailable`, `openUriAvailable`, `inhibitAvailable`, `screenCastAvailable`, and `globalShortcutsAvailable` shall be `true` if the corresponding interface name appears in the `interfaces` QStringList, `false` otherwise.

### Live Updates

**REQ-F-005** (Event-driven)  
When a D-Bus service matching `org.freedesktop.impl.portal.*` appears on the session bus, the service shall re-run backend detection and update the `backends` property.

- **Acceptance criterion**: Monitor the session bus via `QDBusServiceWatcher` with `watchMode: QDBusServiceWatcher::WatchForOwnerChange`; emit `backendsChanged()` after a new backend is detected.

**REQ-F-006** (Event-driven)  
When the portal broker service `org.freedesktop.portal.Desktop` appears on the session bus, the service shall re-probe the broker and re-enumerate its interfaces.

- **Acceptance criterion**: Update `available` and `interfaces` properties; emit both `availableChanged()` and `interfacesChanged()` signals.

**REQ-F-007** (Event-driven)  
When the portal broker service `org.freedesktop.portal.Desktop` disappears from the session bus, the service shall immediately set `available` to `false`.

- **Acceptance criterion**: `availableChanged(false)` is emitted synchronously.

**REQ-F-008** (Event-driven)  
When an interface that was previously available is no longer enumerated in the broker's Introspection, the corresponding availability boolean shall be set to `false`.

- **Acceptance criterion**: Re-run Introspection on broker reload; if an interface name is missing, update its boolean and emit the corresponding `<interface>AvailableChanged(false)` signal.

### Settings Portal Consumer

**REQ-F-009** (Ubiquitous)  
The service shall read the `org.freedesktop.appearance` interface's `ColorScheme` setting at startup if the Settings portal is available.

- **Acceptance criterion**: Call `org.freedesktop.portal.Settings.Read("org.freedesktop.appearance", "color-scheme")` asynchronously; decode the returned D-Bus variant into a C++ integer (0 = no preference, 1 = dark, 2 = light) and store in the `colorScheme` Q_PROPERTY.

**REQ-F-010** (Ubiquitous)  
The service shall read the `org.freedesktop.appearance` interface's `AccentColor` setting at startup if the Settings portal is available.

- **Acceptance criterion**: Call `org.freedesktop.portal.Settings.Read("org.freedesktop.appearance", "accent-color")` asynchronously; decode the returned D-Bus struct `(ddd)` (three doubles representing normalized R, G, B in [0, 1]) explicitly via `QDBusArgument` deserialization; convert to QColor with 8-bit channels; store in the `accentColor` Q_PROPERTY.

**REQ-F-011** (Event-driven)  
When the Settings portal emits a `SettingChanged` signal for `("org.freedesktop.appearance", "color-scheme")`, the service shall update the `colorScheme` property and emit `colorSchemeChanged()`.

- **Acceptance criterion**: Connect to `org.freedesktop.portal.Settings.SettingChanged` signal on the session bus; decode and update synchronously.

**REQ-F-012** (Event-driven)  
When the Settings portal emits a `SettingChanged` signal for `("org.freedesktop.appearance", "accent-color")`, the service shall update the `accentColor` property and emit `accentColorChanged()`.

- **Acceptance criterion**: Decode the D-Bus struct `(ddd)` explicitly and update `accentColor` QColor; emit signal.

### File Chooser Consumer

**REQ-F-013** (Ubiquitous)  
The service shall provide a Q_INVOKABLE `openFile(QVariant windowHandle, const QString &title, const QStringList &filters)` method.

- **Acceptance criterion**: Method signature is accessible from QML; method does not raise runtime errors when called with valid parameters.

**REQ-F-014** (Conditional)  
Where the file chooser portal is available, `openFile()` shall invoke `org.freedesktop.portal.FileChooser.OpenFile()` with the provided window handle, title, and filter list.

- **Acceptance criterion**: The D-Bus call is placed asynchronously; results are internally logged (no return value to QML yet).

**REQ-F-015** (Conditional)  
Where the file chooser portal is unavailable, `openFile()` shall log a warning and return gracefully without attempting the D-Bus call.

- **Acceptance criterion**: A `qCWarning` is written to the log; method does not raise an exception.

### OpenURI Consumer

**REQ-F-016** (Ubiquitous)  
The service shall provide a Q_INVOKABLE `openUri(const QString &uri)` method.

- **Acceptance criterion**: Method signature is accessible from QML; method does not raise runtime errors when called with a valid URI string.

**REQ-F-017** (Conditional)  
Where the OpenURI portal is available, `openUri()` shall invoke `org.freedesktop.portal.OpenURI.OpenURI()` with the provided URI and an empty window handle.

- **Acceptance criterion**: The D-Bus call is placed asynchronously; results are internally logged.

**REQ-F-018** (Conditional)  
Where the OpenURI portal is unavailable, `openUri()` shall log a warning and return gracefully.

- **Acceptance criterion**: A `qCWarning` is written; method does not raise an exception.

### QML Exposure

**REQ-F-019** (Ubiquitous)  
The service shall be registered as a QML_SINGLETON with URI `HolonightShell` and type name `PortalService`.

- **Acceptance criterion**: QML code can `import HolonightShell; PortalService { … }` or access it directly as `PortalService.<property>` without explicit instantiation.

**REQ-F-020** (Ubiquitous)  
The service shall expose all Q_PROPERTY values (`available`, `interfaces`, `backends`, `colorScheme`, `accentColor`, `settingsAvailable`, `fileChooserAvailable`, `openUriAvailable`, `inhibitAvailable`, `screenCastAvailable`, `globalShortcutsAvailable`) as readable from QML.

- **Acceptance criterion**: Each property can be read without compilation or runtime errors; property bindings from QML are valid.

**REQ-F-021** (Ubiquitous)  
The service shall emit all corresponding NOTIFY signals (`availableChanged`, `interfacesChanged`, `backendsChanged`, `colorSchemeChanged`, `accentColorChanged`, etc.) whenever property values change.

- **Acceptance criterion**: QML bindings depending on these properties update reactively when signals are emitted.

### Graceful Degradation

**REQ-F-022** (If the portal broker is unavailable)  
The service shall set `available` to `false`, leave `interfaces` and `backends` as empty QStringLists, and set all interface-availability booleans to `false`.

- **Acceptance criterion**: No D-Bus errors are logged; the service remains functional and ready to re-probe if the broker appears later.

**REQ-F-023** (If the Settings portal is unavailable)  
The service shall skip the settings read at startup and leave `colorScheme` at 0 (no preference) and `accentColor` unset (or default gray).

- **Acceptance criterion**: No D-Bus errors are logged; `settingsAvailable` is `false`.

**REQ-F-024** (If the `accent-color` D-Bus struct is missing or malformed)  
The service shall catch the deserialization error, log a warning, and leave `accentColor` at its previous value (or default).

- **Acceptance criterion**: A `qCWarning` is logged; no exception is raised.

**REQ-F-025** (If a D-Bus service watcher emits a spurious or duplicate signal)  
The service shall not perform redundant probes if the `available` state has not changed.

- **Acceptance criterion**: A second `NameOwnerChanged(org.freedesktop.portal.Desktop, ...)` signal with the same ownership status does not re-enumerate interfaces.

---

## Non-Functional Requirements

**REQ-NF-001** (Performance)  
Startup diagnostics (broker probe + interface enumeration + backend listing) shall complete within 500 milliseconds.

- **Acceptance criterion**: Measure elapsed time from ctor entry to all initial probes finishing; log result as `qCInfo`; verify time < 500ms in a live Hyprland session.

**REQ-NF-002** (Concurrency)  
All D-Bus I/O shall be asynchronous; the main thread shall never block on D-Bus service lookups, Introspection calls, or property reads.

- **Acceptance criterion**: All D-Bus calls use `QDBusInterface::call(QDBus::NoBlock)` or `QDBusAbstractInterface::asyncCall()`, and responses are processed via QDBusPendingReply callbacks or signals.

**REQ-NF-003** (Reactivity)  
Settings changes from the portal broker shall be reflected in QML bindings within 50 milliseconds after the SettingChanged signal is received.

- **Acceptance criterion**: Emit the corresponding `<property>Changed()` signal synchronously in the SettingChanged handler.

**REQ-NF-004** (Memory)  
The service shall not leak memory on repeated probe cycles or when backends appear/disappear.

- **Acceptance criterion**: Run `valgrind --leak-check=full` on the shell with portal backends toggled on/off; verify no "definitely lost" or "indirectly lost" bytes.

**REQ-NF-005** (Logging)  
Startup probe steps, SettingChanged events, and D-Bus errors shall be logged at INFO or WARNING level (not DEBUG).

- **Acceptance criterion**: Log output includes: "Probing portal broker", "Broker available: true/false", "Settings color-scheme changed", "Portal FileChooser unavailable", etc. All D-Bus errors go to qCWarning.

---

## Constraints

**REQ-C-001**  
The service shall not implement any XDG Desktop Portal backend code; all backend logic remains external.

- **Implication**: No `org.freedesktop.impl.portal.*` interfaces are implemented by PortalService; it is a consumer only.

**REQ-C-002**  
The service shall not add any new QML UI components or visual elements.

- **Implication**: The service is a data/control bridge; file chooser and URI dialogs are rendered by the portal backend (e.g., GTK, Hyprland implementation), not by HoloNight.

**REQ-C-003**  
The service shall not touch the portal Inhibit interface; idle-management's existing logind inhibitor is the sole lock/unlock mechanism.

- **Implication**: `inhibitAvailable` is a diagnostic boolean only; no calls are made to `org.freedesktop.portal.Inhibit`.

**REQ-C-004**  
The service shall use Qt D-Bus (QtDBus) only; no external portal libraries (e.g., libportal, xdg-desktop-portal-kde) are linked.

- **Implication**: All D-Bus marshalling and struct deserialization use `QDBusInterface`, `QDBusArgument`, and `QDBusPendingReply`.

**REQ-C-005**  
The service shall not alter, cache, or validate the content of `interfaces` and `backends` lists beyond the D-Bus data received; no heuristics or fallbacks apply.

- **Implication**: If the broker reports an interface, it is listed as-is; if a backend service name is registered, it is listed as-is.

**REQ-C-006**  
The service shall be a singleton instance per shell process; no per-window or per-monitor variants exist.

- **Implication**: One QML_SINGLETON PortalService, shared across all QML engines.

---

## Acceptance Criteria Summary

| Requirement | Test Method |
|-------------|-------------|
| REQ-F-001 | Check `available` property after shell startup with/without broker running |
| REQ-F-002 | Verify `interfaces` QStringList contains expected portal interface names |
| REQ-F-003 | Verify `backends` QStringList lists all `org.freedesktop.impl.portal.*` services on the bus |
| REQ-F-004 | Verify each `*Available` boolean matches the presence of the corresponding interface in `interfaces` |
| REQ-F-005 | Start a new portal backend while shell is running; observe `backendsChanged()` signal |
| REQ-F-006 | Restart portal broker while shell is running; observe `availableChanged(true)` and `interfacesChanged()` |
| REQ-F-007 | Stop portal broker; observe `availableChanged(false)` within 1 second |
| REQ-F-008 | (Architectural; verified by REQ-F-006) |
| REQ-F-009 | Verify `colorScheme` property matches portal Settings on startup |
| REQ-F-010 | Verify `accentColor` QColor property matches portal accent-color on startup |
| REQ-F-011 | Change system color-scheme; observe `colorSchemeChanged()` signal within 50ms |
| REQ-F-012 | Change system accent-color; observe `accentColorChanged()` signal within 50ms |
| REQ-F-013 | Call `PortalService.openFile(windowHandle, title, filters)` from QML; verify no error |
| REQ-F-014 | Monitor D-Bus activity; verify `org.freedesktop.portal.FileChooser.OpenFile` is called when broker available |
| REQ-F-015 | Disable file chooser backend; call `openFile()`; verify warning logged, no exception |
| REQ-F-016 | Call `PortalService.openUri(uri)` from QML; verify no error |
| REQ-F-017 | Monitor D-Bus; verify `org.freedesktop.portal.OpenURI.OpenURI` is called when available |
| REQ-F-018 | Disable OpenURI backend; call `openUri()`; verify warning logged, no exception |
| REQ-F-019 | Load QML module HolonightShell; verify PortalService is accessible as singleton |
| REQ-F-020 | Bind QML properties to all PortalService properties; verify no compilation/runtime errors |
| REQ-F-021 | Modify PortalService property; verify corresponding `Changed()` signal emitted and QML bindings update |
| REQ-F-022 | Stop portal broker; verify all properties degrade gracefully |
| REQ-F-023 | Stop Settings portal; verify `colorScheme`, `accentColor` remain at defaults, no errors logged |
| REQ-F-024 | Inject malformed accent-color struct; verify warning logged, `accentColor` unchanged |
| REQ-F-025 | Send duplicate NameOwnerChanged signal; verify no redundant Introspection calls (check logs) |
| REQ-NF-001 | Measure startup time in release build; log and verify < 500ms |
| REQ-NF-002 | Inspect implementation; verify all D-Bus calls use `NoBlock` or `asyncCall()` |
| REQ-NF-003 | Measure time from SettingChanged signal to QML binding update; verify < 50ms |
| REQ-NF-004 | Run valgrind leak check; verify no leaks |
| REQ-NF-005 | Inspect shell log file; verify startup and event logs are present at INFO+ level |

---

## Out of Scope / Deferred

1. **Portal Inhibit interface**: The existing logind inhibitor in idle-management is the sole lock/unlock mechanism; no portal Inhibit calls are made.
2. **Backend adaptation layer**: No abstract `PortalBackend` interface or strategy pattern; the portal broker IS the abstraction.
3. **Return values from file chooser / OpenURI**: Currently internal logging only; no callback or return-to-QML mechanism (plumbing reserved for future callers).
4. **Portal ScreenCast / GlobalShortcuts**: Detected and reported as availability booleans only; no active consumer code.
5. **Configuration or override**: Portal broker and backend selection are entirely delegated to the system configuration (e.g., `~/.config/xdg-desktop-portal/portals.conf`); no HoloNight overrides apply.

---

## Related Architecture

See `docs/sdd/portal-service/` for:
- Architecture decision record (backend detection, startup probe strategy, Settings struct deserialization)
- Implementation notes (D-Bus service watching, QML singleton registration)
- Testing strategy (unit tests for Settings parsing, integration tests for live broker/backend changes)
