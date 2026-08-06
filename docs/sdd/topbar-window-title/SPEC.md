# Topbar Window Title — SPEC

**Feature:** Display the active window title in the topbar center section, sourced from the Hyprland IPC event socket.

**Status:** Specification  
**Version:** 1.0  
**Date:** 2026-05-21

---

## Context

The topbar center section shall display the title of the currently-active window in the Hyprland compositor. The title is obtained via the Hyprland IPC event socket (`ext-workspace` protocol pattern: subscribe to async events from a Unix socket, parse line-based protocol). When no window is active or the title is empty, the entire active-window section is hidden.

### Non-Goals (This Session)

- Window icon or appClass display in QML
- Click-to-focus interaction
- Animation on title change
- Per-output / multi-monitor tracking
- Socket reconnection logic

---

## Requirements

### Functional Requirements

#### REQ-F-001: Socket connection initialization
**Template (Event-driven):** When the `ActiveWindowService` singleton is instantiated, the system shall connect to the Hyprland IPC event socket at the path derived from the `HYPRLAND_INSTANCE_SIGNATURE` environment variable.

**Acceptance criteria:**
- The service reads the `HYPRLAND_INSTANCE_SIGNATURE` environment variable
- The socket path is constructed as `/tmp/hypr/{HYPRLAND_INSTANCE_SIGNATURE}/.socket2.sock`
- `QLocalSocket` is opened in read-only mode with no-timeout on the socket connection
- If the socket file does not exist, the service logs a diagnostic message and gracefully remains inactive
- If `HYPRLAND_INSTANCE_SIGNATURE` is unset or empty, the service logs a diagnostic and does not attempt connection

#### REQ-F-002: Socket line-based event reading
**Template (State-driven):** While the socket is connected, the system shall maintain an internal `QByteArray` buffer to accumulate partial reads and emit the `activewindow` event only when a complete newline-terminated line is available.

**Acceptance criteria:**
- Each `QSocketNotifier` readyRead signal invokes a handler that reads up to 4096 bytes from the socket
- Partial lines (without trailing `\n`) are buffered internally and not processed
- When a complete line (ending with `\n`) is assembled, that line is extracted, the buffer is compacted, and event parsing proceeds
- If a single socket read contains multiple complete lines, all are processed in sequence in the same handler invocation
- Malformed or unrecognized event types do not crash the parser; they are logged and skipped

#### REQ-F-003: Active window event parsing
**Template (Event-driven):** When a complete line is received from the socket matching the pattern `activewindow>>appClass,title`, the system shall parse the event and update the `title` and `appClass` properties.

**Acceptance criteria:**
- The parser extracts the substring after `activewindow>>` as the event payload
- The payload is split on the first comma only (titles may contain commas)
- The substring before the first comma is stored in the `appClass` property
- The substring after the first comma is stored in the `title` property
- If the payload contains no comma, both `appClass` and `title` are set to empty strings
- If the event payload is empty (e.g., `activewindow>>`), both properties are set to empty strings
- Qt signals `titleChanged()` and `appClassChanged()` are emitted when properties change

#### REQ-F-004: Title property
**Template (Ubiquitous):** The system shall expose a `title` property (Q_PROPERTY) on the `ActiveWindowService` singleton, initialized to an empty string, that is updated by incoming `activewindow>>` events.

**Acceptance criteria:**
- `title` is a readable QString Q_PROPERTY with a `titleChanged()` signal
- The property is initially empty string `""`
- The property is updated synchronously when an event is parsed
- The signal is emitted only when the value actually changes (not on no-op updates to the same value)
- The property is accessible from QML via `ActiveWindowService.title`

#### REQ-F-005: AppClass property
**Template (Ubiquitous):** The system shall expose an `appClass` property (Q_PROPERTY) on the `ActiveWindowService` singleton, initialized to an empty string, that stores the window application class from incoming events.

**Acceptance criteria:**
- `appClass` is a readable QString Q_PROPERTY with an `appClassChanged()` signal
- The property is initially empty string `""`
- The property is updated synchronously when an event is parsed
- The signal is emitted only when the value actually changes (not on no-op updates to the same value)
- The property is accessible from QML via `ActiveWindowService.appClass`

#### REQ-F-006: Singleton instantiation
**Template (Ubiquitous):** The system shall register `ActiveWindowService` as a QML singleton via `qmlRegisterSingletonInstance()` at module initialization, making it accessible in QML without explicit imports.

**Acceptance criteria:**
- A single global instance of `ActiveWindowService` exists for the lifetime of the application
- The instance is available in QML as `ActiveWindowService` after any QML module URI import
- The instance is created on first application start, before any QML component accesses it
- The instance outlives all QML components (lives in global scope, not tied to any window lifecycle)

#### REQ-F-007: ActiveWindowSection visibility
**Template (State-driven):** While the `title` property is an empty string, the `ActiveWindowSection` QML component shall set `visible: false` to completely hide the section from the layout.

**Acceptance criteria:**
- The section is hidden (takes no space, not drawn) when `ActiveWindowService.title === ""`
- The section becomes visible as soon as a non-empty title arrives
- The section remains visible for the duration that the title is non-empty
- The section is immediately hidden again when the title reverts to empty string

#### REQ-F-008: ActiveWindowSection layout behavior
**Template (Ubiquitous):** The `ActiveWindowSection` component shall extend `BarSection` and shall use `Layout.fillWidth: true` to stretch and fill available center space in the topbar.

**Acceptance criteria:**
- The component inherits from `BarSection` (same base as other topbar sections)
- The component sets `Layout.fillWidth: true` on itself
- The component expands to fill all available width in the topbar center row when visible
- When not visible (title is empty), the center row layout does not reserve space for it

#### REQ-F-009: Comment label in section
**Template (Ubiquitous):** The `ActiveWindowSection` component shall contain a static label with the text "// ACTIVE WINDOW" as a comment above the title.

**Acceptance criteria:**
- The label is present in the QML as a child of the section
- The text is exactly "// ACTIVE WINDOW"
- The label uses 14px Inter font
- The label color is sourced from `HoloniightPalette` (specific token TBD by design session)

#### REQ-F-010: Title text rendering
**Template (Ubiquitous):** The `ActiveWindowSection` component shall display the active window title as a Text element with elision, bound to `ActiveWindowService.title`.

**Acceptance criteria:**
- A `Text` element displays the value of `ActiveWindowService.title`
- The font size is 20px Inter
- The text color is sourced from `HoloniightPalette.textPrimary` (or equivalent token per design)
- The `elide` property is set to `Text.ElideRight` (titles that exceed available width are truncated with "…")
- The `width` is set to `parent.width` (or equivalent to fill the section width)
- No hardcoded color hex values appear in the QML file

#### REQ-F-011: HoloniightPalette color sourcing
**Template (Ubiquitous):** All color values in the `ActiveWindowSection` QML component shall be sourced from `HoloniightPalette` via the `Holonight` import; no hardcoded hex color values shall appear.

**Acceptance criteria:**
- The QML file contains `import Holonight` at the top
- All color properties (comment label, title text, background if any) use `HoloniightPalette.<token>` syntax
- No colors are hardcoded as `#xxxxxx`, `rgb()`, or named colors in the source
- `grep -E '#[0-9a-fA-F]{6}|rgb\(' ActiveWindowSection.qml` returns no matches in color contexts

### Non-Functional Requirements

#### REQ-NF-001: Socket read responsiveness
**Template (Ubiquitous):** The system shall process socket events on the main Qt event loop with no blocking I/O; title updates shall appear on screen within 100ms of the Hyprland IPC event emission.

**Acceptance criteria:**
- Socket reads are non-blocking via `QSocketNotifier`
- No worker threads or blocking reads are used
- Timestamp a test window title change in Hyprland, measure roundtrip to visible on-screen update
- The latency is consistently < 100ms on a typical 2026-era desktop

#### REQ-NF-002: Memory footprint
**Template (Ubiquitous):** The `ActiveWindowService` singleton shall not retain more than the current title, app class, and socket buffer; no title history or event log shall be kept.

**Acceptance criteria:**
- The only retained state is the current `title`, `appClass`, and socket line buffer
- Memory usage is constant regardless of session duration or number of window switches
- No vector/list of historical titles is maintained
- Valgrind or similar tool reports no memory leaks over a 1-hour manual test session

#### REQ-NF-003: Encoding robustness
**Template (Ubiquitous):** The system shall handle window titles in UTF-8 encoding and preserve all printable characters, including Unicode symbols and non-ASCII scripts.

**Acceptance criteria:**
- A window with a title containing emoji, Cyrillic, CJK, or other Unicode is displayed correctly
- Multi-byte UTF-8 sequences are not truncated or mangled by the line buffer
- Hyprland's native UTF-8 IPC format is preserved end-to-end

### Constraint Requirements

#### REQ-C-001: HolonightShell QML module URI
**Template (Ubiquitous):** The `ActiveWindowSection` component shall be registered in the QML module with URI `HolonightShell` and shall be importable via `import HolonightShell`.

**Acceptance criteria:**
- The component is compiled into the HolonightShell QML module (not a loose QML file)
- CMakeLists.txt registers the component with the module URI
- In any QML file that imports `HolonightShell`, the component is accessible by name (e.g., `ActiveWindowSection {}`)
- The component does not require a separate import statement

#### REQ-C-002: QRC resource prefix
**Template (Ubiquitous):** The `ActiveWindowSection.qml` file shall be served via Qt resource system with the QRC prefix `/HolonightShell/` and aliased to strip the `src/qml/` directory prefix.

**Acceptance criteria:**
- The source file is located at `src/qml/ActiveWindowSection/ActiveWindowSection.qml` or equivalent
- CMakeLists.txt sets `QT_RESOURCE_ALIAS` to strip the `src/qml/` prefix (resulting in `/HolonightShell/ActiveWindowSection/ActiveWindowSection.qml`)
- The resource is accessible at `qrc:/HolonightShell/ActiveWindowSection/ActiveWindowSection.qml` in QML
- The QML engine loads the file without 404 or path resolution errors

#### REQ-C-003: C++ naming conventions
**Template (Ubiquitous):** All C++ code in the `ActiveWindowService` and related classes shall follow the project's naming conventions: classes `CamelCase`, methods `camelBack`, private member variables `lower_case_` with trailing underscore.

**Acceptance criteria:**
- Class name is `ActiveWindowService` (CamelCase)
- Public methods are `connectSocket()`, `title()`, `appClass()`, etc. (camelBack)
- Private member variables are named `socket_`, `buffer_`, `title_`, etc. (lower_case_)
- clang-tidy with `.clang-tidy` config reports no naming violations in this module

#### REQ-C-004: C++ standard and include ordering
**Template (Ubiquitous):** The C++ implementation files shall compile under C++23 standard and shall follow include ordering: local `"..."` → Qt `<Q...>` → system `<...>`.

**Acceptance criteria:**
- CMakeLists.txt sets `CXX_STANDARD 23` (or the project default)
- Header includes in `.cpp` files follow the ordering pattern: local headers first, Qt headers second, system headers last
- The project builds without warnings related to C++ standard compliance
- clang-format with `.clang-format` config does not flag any include order violations

#### REQ-C-005: CMake integration
**Template (Ubiquitous):** The `ActiveWindowService` and `ActiveWindowSection` components shall be integrated into the project's CMake build system with no manual invocation required; `task build` shall compile them automatically.

**Acceptance criteria:**
- CMakeLists.txt includes the source files in a target (e.g., `holonight_shell_lib` or main executable target)
- Running `task build` from the project root successfully compiles the module
- Running `task build` produces a warning or error if the source files are missing or malformed
- No separate CMake invocation is required to include these components

#### REQ-C-006: QLocalSocket usage
**Template (Ubiquitous):** The system shall use `QLocalSocket` (Qt 6 standard Unix domain socket API) for IPC; no raw POSIX socket, libsystemd, or external socket library shall be used.

**Acceptance criteria:**
- The implementation includes only `<QLocalSocket>` and related Qt headers
- No `#include <sys/socket.h>`, `<unistd.h>`, or similar POSIX socket headers appear in the IPC code
- The code compiles and runs correctly on Qt 6.8+ (project's baseline)

#### REQ-C-007: Guard against missing HYPRLAND_INSTANCE_SIGNATURE
**Template (If ... then ...):** If the `HYPRLAND_INSTANCE_SIGNATURE` environment variable is not set or is empty, then the system shall not attempt socket connection and shall remain in a no-op idle state with empty `title` and `appClass` properties.

**Acceptance criteria:**
- Running holonight-shell in a non-Hyprland Wayland session (e.g., GNOME Wayland, KDE Wayland) does not crash
- The service detects missing/empty env var before attempting socket operations
- A diagnostic log message is emitted (e.g., "ActiveWindowService: HYPRLAND_INSTANCE_SIGNATURE not set, disabling")
- `title` and `appClass` remain empty strings throughout the session
- The topbar layout is unaffected (ActiveWindowSection is hidden due to empty title)

#### REQ-C-008: QSocketNotifier integration
**Template (Ubiquitous):** The system shall use `QSocketNotifier` to monitor the socket file descriptor and trigger read events on the main Qt event loop; no polling or worker threads shall be used.

**Acceptance criteria:**
- A `QSocketNotifier` instance is created with `QSocketNotifier::Read` mode on the socket's native file descriptor
- The notifier's `activated` signal is connected to a slot that performs the read
- The notifier is enabled/disabled in sync with the socket connection state
- CPU usage during idle (no window switch) is minimal (< 1% on a typical desktop)

#### REQ-C-009: No hardcoded socket timeout
**Template (Ubiquitous):** The `QLocalSocket` shall be configured with no read timeout; blocking on socket reads shall not occur (async via notifier).

**Acceptance criteria:**
- `QLocalSocket::setReadBufferSize()` or timeout methods are not used in a way that blocks the event loop
- `QSocketNotifier` is the only mechanism for detecting available data
- The socket read handler completes in < 1ms even if the read buffer is empty

#### REQ-C-010: Line buffer compaction
**Template (State-driven):** While the internal line buffer contains processed lines, the system shall remove those lines from the buffer after extraction to prevent unbounded memory growth.

**Acceptance criteria:**
- After a complete line is processed and removed from the buffer, the buffer size decreases
- A test that alternates 100 small title updates shows buffer memory usage returning to baseline between updates
- No accumulation of old lines is observed over a multi-hour session

---

## Design Decisions

### Protocol: Hyprland IPC Event Socket

The implementation uses Hyprland's IPC event socket (`.socket2.sock`) rather than alternatives:

- **Not D-Bus**: Hyprland's native IPC is Unix socket, not D-Bus. Direct socket subscription is lighter-weight.
- **Not shell script polling**: Hyprland's `hyprctl` CLI is useful for one-off queries, but polling from C++ would add latency and CPU overhead. Event subscription is reactive.
- **Not Hyprland X11/Wayland protocol extensions**: The `activewindow` event is available directly from IPC without protocol extensions.

### C++ Service Pattern: QObject Singleton

`ActiveWindowService` is a `QObject` singleton rather than a static utility class:

- Qt signals (`titleChanged()`, `appClassChanged()`) integrate naturally with QML binding
- `QSocketNotifier` is a `QObject` that emits signals; singleton inherits from `QObject` to own it
- State persists for the lifetime of the application (appropriate for a service)

### QML Visibility Binding vs. Opacity

When the title is empty, the section is completely hidden (`visible: false`) rather than just transparent. This:

- Removes it from the layout, so the topbar center space is not reserved
- Matches the design pattern used in other conditional topbar sections

### Line Buffer over QTextStream

The implementation maintains a manual `QByteArray` line buffer rather than using `QTextStream`:

- `QTextStream` adds codec overhead and buffering complexity unnecessary for single-event-per-line protocol
- Manual buffer gives explicit control over partial-read handling
- Standard IPC protocol: split on `\n`, buffer remainder

---

## Testing Strategy

### Unit Tests (if GTest enabled)

- Parse correct `activewindow>>appClass,title` formats
- Handle commas in title (split on first comma only)
- Handle empty appClass or title
- Guard against missing `HYPRLAND_INSTANCE_SIGNATURE`

### Manual Testing

- Verify title updates when window focus changes in Hyprland
- Verify section visibility toggles when title becomes non-empty/empty
- Verify no layout glitches when section appears/disappears
- Verify Unicode titles (emoji, CJK, Cyrillic) render correctly
- Verify no crashes on non-Hyprland sessions (GNOME Wayland, KDE Wayland, X11)

### Performance Validation

- Measure socket-to-on-screen latency for a title change (target: < 100ms)
- Monitor memory and CPU usage over 1-hour session
- Verify no memory leaks with Valgrind

---

## Out of Scope (Non-Goals)

The following features are explicitly out of scope for this session:

- **Window icon**: appClass is parsed and stored but not rendered in QML. Icon rendering can be a future session.
- **Click-to-focus**: No mouse interaction on the title or section.
- **Title animation**: Title changes appear immediately (no slide or fade).
- **Per-output tracking**: Only the globally active window is tracked; no per-monitor state.
- **Socket reconnection**: If the socket disconnects (e.g., Hyprland restart), the service does not auto-reconnect. Manual restart of holonight-shell is required.
- **Backfill on startup**: On service startup, the socket is not queried for the current active window. The first `activewindow>>` event after startup populates the title.

---

## Acceptance

This specification is accepted when:

1. All REQ-F requirements have verifiable QML/C++ implementations
2. All REQ-NF requirements pass performance and memory validation
3. All REQ-C requirements are satisfied in CMakeLists.txt, C++ code, and QML
4. Manual testing confirms title updates on window switch with < 100ms latency
5. Manual testing on non-Hyprland Wayland sessions shows no crashes and empty title
6. Code review confirms no hardcoded colors and proper use of HoloniightPalette
