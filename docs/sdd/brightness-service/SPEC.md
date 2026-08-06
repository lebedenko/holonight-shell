# SPEC — brightness-service

## Overview

The `brightness-service` feature provides sysfs-based display backlight control via logind D-Bus. It auto-selects the highest-capacity backlight device from `/sys/class/backlight/`, exposes current brightness to QML as a 0–100 ratio, and enables real-time slider control in the sidebar quick-settings panel. External brightness changes (Fn keys, other apps) are tracked via inotify polling and reflected back to the UI. On devices without a backlight, the service initializes without error and the slider is hidden entirely.

---

## Non-goals

- **DDC/CI external monitor brightness** — only sysfs backlight (internal display) is supported
- **Gamma/colour temperature / night-light** — out of scope
- **Per-monitor brightness** — single selected device only
- **Keyboard backlight** — separate `/sys/class/leds/` namespace, not included
- **Direct sysfs writes** — logind D-Bus `SetBrightness` is the only write mechanism

---

## Requirements

### Functional

#### REQ-F-001: BrightnessBackend shall define abstract interface
**Requirement:** The system shall provide an abstract `BrightnessBackend` base class that defines the interface for brightness read/write operations. Subclasses shall implement `maxBrightness()`, `currentBrightness()`, `setBrightness(value)`, and provide a signal for external brightness changes.

**Acceptance Criterion:**
- A `BrightnessBackend::brightnessChanged(int newValue)` signal exists and is emitted when external changes occur
- The abstract class compiles and serves as a template for all backend implementations

---

#### REQ-F-002: SysfsBackend shall enumerate backlight devices
**Requirement:** The system shall provide a `SysfsBackend` subclass that scans `/sys/class/backlight/` on construction and selects the device with the highest `max_brightness` value.

**Acceptance Criterion:**
- On a system with `/sys/class/backlight/intel_backlight` (max 4437) and `/sys/class/backlight/amdgpu_bl0` (max 255), intel_backlight is selected
- On a system with only one device, that device is selected
- The device name and max brightness are stored and accessible as read-only properties

---

#### REQ-F-003: NullBrightnessBackend shall handle missing devices gracefully
**Requirement:** When `/sys/class/backlight/` contains no devices, the system shall instantiate a `NullBrightnessBackend` that returns 0 for all operations without error.

**Acceptance Criterion:**
- `NullBrightnessBackend::maxBrightness()` returns 0
- `NullBrightnessBackend::currentBrightness()` returns 0
- `NullBrightnessBackend::setBrightness(value)` is a no-op
- The service initializes without logging errors or warnings

---

#### REQ-F-004: BrightnessService shall expose maxBrightness to QML
**Requirement:** The `BrightnessService` C++ singleton shall expose a read-only `maxBrightness` Q_PROPERTY (integer) derived from the active backend's maximum brightness value.

**Acceptance Criterion:**
- On a system with intel_backlight max 4437, `maxBrightness` = 4437
- On a system with no backlight, `maxBrightness` = 0
- The property is read-only and does not change after initialization

---

#### REQ-F-005: BrightnessService shall expose normalised brightness percentage
**Requirement:** The `BrightnessService` shall expose an integer `brightnessPercent` Q_PROPERTY (0–100) calculated as `(currentBrightness / maxBrightness) * 100`, rounded to the nearest integer.

**Acceptance Criterion:**
- When currentBrightness = 2218, maxBrightness = 4437, `brightnessPercent` = 50
- When currentBrightness = 4437, `brightnessPercent` = 100
- When currentBrightness = 0, `brightnessPercent` = 0
- When maxBrightness = 0 (no device), `brightnessPercent` = 0 without division-by-zero error

---

#### REQ-F-006: BrightnessService shall expose device presence
**Requirement:** The `BrightnessService` shall expose a read-only boolean `hasBacklight` Q_PROPERTY that is true when `maxBrightness` > 0, and false otherwise.

**Acceptance Criterion:**
- On a system with a backlight, `hasBacklight` = true
- On a system without a backlight, `hasBacklight` = false
- QML can bind visibility of the brightness slider to this property

---

#### REQ-F-007: BrightnessService shall provide setBrightnessPercent invokable
**Requirement:** The `BrightnessService` shall expose a `setBrightnessPercent(int percent)` Q_INVOKABLE method that converts the 0–100 percentage to the underlying device's brightness value and calls the backend's `setBrightness()`.

**Acceptance Criterion:**
- `setBrightnessPercent(50)` on a 4437-max device sets underlying brightness to ~2218
- `setBrightnessPercent(100)` on a 4437-max device sets underlying brightness to 4437
- `setBrightnessPercent(0)` sets underlying brightness to 0
- When `maxBrightness` = 0, the invokable is a no-op

---

#### REQ-F-008: SysfsBackend shall use logind D-Bus for writes
**Requirement:** The `SysfsBackend::setBrightness(value)` method shall invoke `org.freedesktop.login1.Session.SetBrightness("backlight", device_name, value)` via D-Bus instead of writing directly to `/sys/class/backlight/<device>/brightness`.

**Acceptance Criterion:**
- A D-Bus trace shows `SetBrightness` being called with the correct device name and value
- Writing to sysfs files is never attempted
- Permission errors are logged if SetBrightness is denied

---

#### REQ-F-009: SysfsBackend shall track external brightness changes
**Requirement:** The `SysfsBackend` shall monitor `/sys/class/backlight/<device>/brightness` for changes caused by external agents (Fn keys, other apps) and emit `brightnessChanged()` signal within 500 ms of the change.

**Acceptance Criterion:**
- When a user presses Fn+F5 to decrease brightness, the slider updates within 500 ms
- When a user presses Fn+F6 to increase brightness, the slider updates within 500 ms
- Multiple rapid key presses are coalesced (signal emitted once per monitored read, not per keystroke)

---

#### REQ-F-010: SysfsBackend shall use inotify for change detection
**Requirement:** The `SysfsBackend` shall use inotify to monitor `/sys/class/backlight/<device>/brightness` for write events (`IN_MODIFY`), avoiding inefficient polling.

**Acceptance Criterion:**
- The implementation uses `inotify_init1()` and `inotify_add_watch()` with `IN_MODIFY` flag
- No timer-based polling loop exists in the code
- A QSocketNotifier is registered on the inotify file descriptor to dispatch events to the Qt event loop

---

#### REQ-F-011: BrightnessService shall expose hasBacklight to QML
**Requirement:** The `BrightnessService::hasBacklight` property shall notify QML via `Q_PROPERTY(...NOTIFY brightnessChanged)` when the backend is initialized, so QML can bind widget visibility.

**Acceptance Criterion:**
- QML code `visible: BrightnessService.hasBacklight` correctly hides/shows the slider on app startup
- The slider remains hidden throughout the session on systems without a backlight

---

#### REQ-F-012: SidebarQuickSettings shall include brightness slider
**Requirement:** The `SidebarQuickSettings.qml` component shall include a brightness slider (QML `Slider`) that:
- Is visible only when `BrightnessService.hasBacklight` is true
- Displays a label "Brightness"
- Has a horizontal layout spanning the full panel width

**Acceptance Criterion:**
- The slider is rendered in the quick-settings panel when a backlight exists
- The slider is hidden entirely (not taking up space) when no backlight is present
- The slider label reads "Brightness"

---

#### REQ-F-013: Brightness slider shall reflect current state on open
**Requirement:** When the sidebar is opened, the brightness slider position shall be synchronized to `BrightnessService.brightnessPercent` without user interaction.

**Acceptance Criterion:**
- Opening the sidebar with brightness at 50% shows the slider at the 50% mark
- Opening the sidebar at different brightness levels always reflects the current system state
- No pending writes or intermediate values are shown

---

#### REQ-F-014: Brightness slider shall throttle writes during drag
**Requirement:** While the user is dragging the slider, `setBrightnessPercent()` shall be called at most once per 100 ms, regardless of how many slider value changes occur.

**Acceptance Criterion:**
- Dragging the slider rapidly from 0% to 100% results in exactly 10±1 D-Bus `SetBrightness` calls (100 ms intervals over ~1 second drag)
- No more than one write occurs within any 100 ms window during a drag
- The slider does not become sluggish or unresponsive

---

#### REQ-F-015: Brightness slider shall write on release
**Requirement:** When the user releases the slider after a drag, `setBrightnessPercent()` shall be called immediately with the final position, regardless of throttle timing.

**Acceptance Criterion:**
- Dragging to 75% and releasing results in an immediate `SetBrightness` call to 75%
- The final value is always applied, even if the throttle window is not yet expired
- The brightness matches the slider position after release

---

#### REQ-F-016: Brightness slider shall update on external changes
**Requirement:** When `BrightnessService.brightnessChanged()` signal is emitted (due to external brightness change), the slider position shall update to reflect the new `brightnessPercent` without user interaction.

**Acceptance Criterion:**
- User presses Fn+F5 to decrease brightness; the slider moves left within 500 ms
- User presses Fn+F6 to increase brightness; the slider moves right within 500 ms
- The slider position always matches the system brightness, even if the sidebar was not focused

---

#### REQ-F-017: BrightnessService shall initialize before QML loads
**Requirement:** The `BrightnessService` singleton shall be instantiated and registered with `qmlRegisterSingletonInstance<BrightnessService>()` in the C++ main function before any QML component is loaded.

**Acceptance Criterion:**
- QML components can reference `BrightnessService.brightnessPercent` without null-pointer exceptions
- The service is available to all QML components that import `HolonightShell`

---

#### REQ-F-018: BrightnessService shall emit signals on changes
**Requirement:** The `BrightnessService` shall define and emit a `brightnessPercentChanged(int)` signal whenever `brightnessPercent` changes, and a `hasBacklightChanged(bool)` signal if the backend changes.

**Acceptance Criterion:**
- QML connections to `BrightnessService.onBrightnessPercentChanged` are triggered within 100 ms of a system brightness change
- A QML binding to `BrightnessService.brightnessPercent` automatically updates without polling

---

#### REQ-F-019: SysfsBackend shall read brightness from sysfs
**Requirement:** When the `BrightnessService` needs to read the current brightness (e.g., on startup or after a slider release), it shall read the device's `/sys/class/backlight/<device>/brightness` file directly into memory.

**Acceptance Criterion:**
- Reading `/sys/class/backlight/intel_backlight/brightness` yields an integer matching the system state
- The read is non-blocking and completes within 1 ms
- File I/O errors are logged without crashing

---

#### REQ-F-020: SidebarQuickSettings slider shall not persist across sidebar close/open
**Requirement:** The brightness slider position shall be a live binding to `BrightnessService.brightnessPercent` and shall not store slider state between sidebar open/close cycles.

**Acceptance Criterion:**
- Close sidebar, use Fn key to change brightness externally, open sidebar — the slider immediately reflects the new brightness
- No "stale" slider position persists from a previous session
- Slider state is computed fresh each time the sidebar is opened

---

### Non-Functional

#### REQ-NF-001: BrightnessService shall initialize asynchronously
**Requirement:** The `BrightnessService` D-Bus initialization and inotify setup shall not block the main Qt event loop or delay application startup.

**Acceptance Criterion:**
- Application launches and displays the sidebar in < 500 ms even if D-Bus is slow
- inotify setup is non-blocking
- Initial brightness enumeration does not stall the UI

---

#### REQ-NF-002: BrightnessService shall handle D-Bus connection loss gracefully
**Requirement:** If the D-Bus connection to logind is lost or `SetBrightness` fails, the `BrightnessService` shall log the error, revert the slider to the last-known state, and remain functional.

**Acceptance Criterion:**
- Restarting logind does not crash holonight-shell
- If a `SetBrightness` call fails, the slider remains at the pre-change position
- An error is logged with the failure reason (permission denied, service unavailable, etc.)

---

#### REQ-NF-003: All QML colors and styling shall use HoloniightPalette
**Requirement:** The brightness slider component (label, track, handle, background) shall not contain hardcoded hex color values; all styling shall be sourced from `HoloniightPalette` tokens.

**Acceptance Criterion:**
- A code review of the brightness slider QML shows zero hardcoded `#` hex values
- All colors import via `import Holonight` and access `HoloniightPalette.<token>`
- The slider theme updates when the system HoloNight palette changes

---

#### REQ-NF-004: Code shall pass all linting and formatting checks
**Requirement:** All C++ and QML code for the brightness feature shall pass `task format-check`, `task qml-lint`, and `task tidy` without errors or warnings.

**Acceptance Criterion:**
- `task build` completes without compiler warnings
- `task qml-lint` reports zero warnings for all brightness-related QML files
- `task format-check` reports the code is correctly formatted
- `task tidy` reports zero issues in C++ brightness code

---

#### REQ-NF-005: BrightnessService shall use standard Qt logging
**Requirement:** Diagnostic output (device enumeration, D-Bus errors, inotify events) shall use `qCInfo` and `qCWarning` categories, visible via `QT_LOGGING_RULES` environment variable without code modification.

**Acceptance Criterion:**
- Running with `QT_LOGGING_RULES="*brightness*.info=true"` shows device selection and inotify setup logs
- Errors are logged at warning level and include sufficient context (D-Bus error codes, sysfs path, etc.)
- No `qDebug` or `std::cout` is used in the implementation

---

#### REQ-NF-006: inotify watch shall not leak file descriptors
**Requirement:** The `SysfsBackend` inotify watch shall be properly cleaned up on destruction via `inotify_rm_watch()` and the inotify fd via `close()`.

**Acceptance Criterion:**
- Running `lsof -p $(pgrep -x holonight-shell)` shows no leaked inotify fds after multiple brightness reads
- The service destructor closes all file descriptors it opened
- valgrind reports no "still reachable" inotify resources

---

#### REQ-NF-007: SysfsBackend shall validate device paths
**Requirement:** Before reading or watching `/sys/class/backlight/<device>/brightness`, the `SysfsBackend` shall validate that the path exists and is readable, and shall not allow path traversal (e.g., device name containing `../`).

**Acceptance Criterion:**
- Device names are sanitized to alphanumeric and underscore characters only
- Attempting to set an invalid device name logs an error and fails gracefully
- No security warnings from static analysis

---

### Constraints

#### REQ-C-001: BrightnessService shall use sysfs backlight only
**Requirement:** Brightness data shall be sourced exclusively from `/sys/class/backlight/`; no alternative APIs (ACPI, DDC/CI, nvidia-settings, amdgpu direct access) shall be used.

**Acceptance Criterion:**
- The implementation scans only `/sys/class/backlight/` on initialization
- No other brightness sources are queried or accessed

---

#### REQ-C-002: BrightnessService shall register as a QML singleton
**Requirement:** The `BrightnessService` shall be exposed to QML via `qmlRegisterSingletonInstance<BrightnessService>()` in the C++ main function.

**Acceptance Criterion:**
- QML components can access the service via `import HolonightShell` and reference the singleton directly
- Only one instance of BrightnessService exists throughout the application lifetime

---

#### REQ-C-003: Backend writes shall use logind D-Bus only
**Requirement:** All brightness writes shall use the `org.freedesktop.login1.Session.SetBrightness("backlight", device_name, value)` method; direct sysfs file writes are forbidden.

**Acceptance Criterion:**
- A D-Bus trace (`busctl monitor org.freedesktop.login1`) shows SetBrightness calls for every brightness change
- No writes to `/sys/class/backlight/<device>/brightness` are performed by the application

---

#### REQ-C-004: Device selection shall be automatic and unambiguous
**Requirement:** The system shall select exactly one device from `/sys/class/backlight/` at initialization time based on maximum brightness (highest wins); no user configuration or manual selection is supported.

**Acceptance Criterion:**
- On a system with multiple devices, the highest-capacity device is always selected
- Device selection is logged at info level for troubleshooting
- The selected device does not change during the application lifetime

---

#### REQ-C-005: SidebarQuickSettings integration follows existing patterns
**Requirement:** The brightness slider shall be integrated into `SidebarQuickSettings.qml` following the same pattern as existing quick-settings controls (e.g., audio volume, network toggle).

**Acceptance Criterion:**
- The slider is a child of the quick-settings `ColumnLayout`
- Visibility is bound to a service property (`hasBacklight`)
- Label and control layout matches the visual style of other quick-settings items

---

#### REQ-C-006: No multi-device or curve support
**Requirement:** The feature shall control brightness on a single selected device only; no interface shall support multiple devices, curves, profiles, or per-zone brightness.

**Acceptance Criterion:**
- `SysfsBackend` maintains exactly one active device
- There is no API to switch between devices
- No curve or profile selection exists in the UI

---

#### REQ-C-007: Slider shall be display-only when no backlight
**Requirement:** When `hasBacklight` is false, the brightness slider control shall be completely hidden (not displayed, not occupying space in the layout).

**Acceptance Criterion:**
- On a system without a backlight, the entire brightness section is invisible in quick-settings
- Opening the sidebar on a desktop PC shows no brightness control or stub
- The sidebar layout is unchanged by the presence or absence of a backlight

---

#### REQ-C-008: BrightnessBackend and subclasses are internal
**Requirement:** The `BrightnessBackend` abstract class and its implementations (`SysfsBackend`, `NullBrightnessBackend`) shall be internal to the `BrightnessService` and shall not be exposed to QML or external C++ code.

**Acceptance Criterion:**
- QML code only references `BrightnessService`, never `SysfsBackend` or `NullBrightnessBackend`
- Backend classes are declared in a private header (not installed)
- The public API surface is `BrightnessService` only

---

## Acceptance Criteria Summary

**Visual Acceptance (manual testing on a laptop with backlight):**
- [ ] Brightness slider appears in quick-settings panel when sidebar is open
- [ ] Slider position reflects current system brightness on open (e.g., 50% = middle)
- [ ] Dragging the slider changes brightness in real time
- [ ] Releasing slider applies the final value
- [ ] Pressing Fn+F5/F6 to adjust brightness externally updates slider within 500 ms
- [ ] Slider label reads "Brightness"
- [ ] No visual glitches or layout breaks in sidebar

**Visual Acceptance (manual testing on a desktop with no backlight):**
- [ ] Brightness slider does not appear in quick-settings
- [ ] No empty space or stub control is visible
- [ ] Sidebar layout is unchanged

**Functional Acceptance (automated testing):**
- [ ] `task build` succeeds without warnings
- [ ] `task qml-lint` reports zero errors in brightness-related QML files
- [ ] `task format-check` passes
- [ ] `task tidy` reports zero issues in C++ brightness code
- [ ] BrightnessService initializes in < 500 ms on all devices
- [ ] `setBrightnessPercent(50)` results in D-Bus SetBrightness call with correct value
- [ ] External brightness changes trigger `brightnessPercentChanged()` signal within 500 ms
- [ ] No D-Bus connection loss crashes the application

**Integration Acceptance:**
- [ ] BrightnessService is a singleton accessible to all QML via `import HolonightShell`
- [ ] SidebarQuickSettings includes the brightness slider component
- [ ] Visibility is bound to `BrightnessService.hasBacklight`
- [ ] Slider value is bound to `BrightnessService.brightnessPercent`
- [ ] Application starts and displays the sidebar in < 500 ms on all devices

