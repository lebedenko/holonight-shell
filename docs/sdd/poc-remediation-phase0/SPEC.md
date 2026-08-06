# POC Remediation Phase 0 — EARS Requirements Specification

| Field | Value |
|---|---|
| Document ID | poc-remediation-phase0/SPEC.md |
| Cycle | Remediation Phase 0 of the POC Readiness Review |
| Reference | Review REPORT.md §4 (U-02, U-06, U-07, U-11), ARCHITECTURE-GAPS.md (gaps #2, #4) |
| Scope | 6 highest-priority bug fixes: 2 Critical, 4 High severity |
| Status | In development |

---

## Overview

This specification addresses the 6 highest-priority, self-contained bug fixes from the POC Readiness Review. All items are unauthenticated-attacker-triggerable or silent-data-loss bugs with trivial reproduction, requiring no architectural prerequisites. Requirements are written in EARS (Easy Approach to Requirements Syntax) format, organized into 6 requirement groups (one per item), with functional (REQ-F), non-functional (REQ-NF), and constraint (REQ-C) IDs.

---

## Item 1: Tray-Icon Pixmap Integer Overflow (U-02 F-01, Critical)

**Defect**: Any local process registering as a `StatusNotifierItem` on the session D-Bus can send icon pixmap dimensions that overflow the existing size guard, triggering a multi-gigabyte memory allocation and crashing the entire shell across all monitors.

**Root location**: `libs/holonight-surfaces/src/tray/` — tray-icon pixmap decoding logic.

### REQ-F-1.1 (Functional — Unwanted Behaviour)

**The system shall not allocate unbounded memory based on untrusted pixmap dimensions.**

When tray-icon pixmap data arrives via D-Bus, dimensions must be validated using 64-bit arithmetic before any allocation. The following checks must occur in sequence:

1. Width and height must both be strictly positive (> 0)
2. Width and height must each not exceed a concrete maximum (e.g. 512 pixels) — this maximum shall be state in code comments with justification (e.g. "icons are small UI elements; 512x512 covers all practical tray use cases")
3. Data length must match `width * height * 4` (RGBA format) exactly, computed with 64-bit integers to rule out overflow
4. If any check fails, the dimension pair must be rejected; the tray entry shall remain visible with a fallback/default glyph instead of the custom pixmap

**Acceptance Criterion (Automated):**
- Write a GTest in `tests/unit_tray_pixmap_validation.cpp` that instantiates the tray-icon validation function with representative test cases:
  - Valid small pixmap (e.g. 32x32)
  - Valid maximum-allowed pixmap (e.g. 512x512)
  - Width overflow (e.g. `UINT32_MAX`)
  - Height overflow (e.g. `UINT32_MAX`)
  - Data-length mismatch (e.g. data too short)
  - Zero or negative dimensions
- Assert that all invalid cases return rejection (bool false, or a result enum), and exactly zero allocation was attempted
- Assert that valid cases return acceptance and allocation proceeds
- Run with `task test` and confirm `test_tray_pixmap_validation` passes

### REQ-F-1.2 (Functional — Event-Driven)

**When a tray icon pixmap with rejected dimensions is received, the system shall emit a diagnostic log entry.**

The rejection must be logged at the `qCWarning` level with:
- The offending tray item's D-Bus service name (bus name of the StatusNotifierItem)
- The rejected dimensions (width, height, data length, reason for rejection)
- A human-readable message (e.g. "Rejecting tray icon from org.example.App: width 2147483647 exceeds maximum 512")

**Acceptance Criterion (Manual):**
- Register a fake `StatusNotifierItem` on the session D-Bus (use `gdbus-send` or a custom test utility) with oversized dimensions (e.g. `width=2147483647, height=1`)
- Capture shell logs with `QT_FORCE_STDERR_LOGGING=1 ./build/holonight-shell` (or read `~/.local/share/holonight/holonight-shell/holonight.log`)
- Verify the qCWarning appears with the sender's bus name and rejected dimensions
- Verify the tray entry remains visible (not blank, not crashed)

### REQ-F-1.3 (Functional — Ubiquitous)

**The system shall continue to render the affected tray entry even when pixmap validation fails.**

When a custom pixmap is rejected, the tray entry's icon shall fall back to a default/placeholder glyph (or remain empty if a previous valid pixmap was never set). The tray entry itself (application name, menu) shall remain interactive and functional.

**Acceptance Criterion (Manual):**
- Repeat the fake StatusNotifierItem registration from REQ-F-1.2
- Verify the entry is still present in the tray area with a visible placeholder (not missing from the list, not causing a crash/hang)
- Verify you can right-click the tray entry and access its context menu (if one exists)

### REQ-NF-1.1 (Non-Functional — Performance)

**The validation logic shall execute in constant time; no quadratic or unbounded loops.**

The dimension/data checks in REQ-F-1.1 shall consist only of arithmetic comparisons and one division (to check data-length match). No filesystem access, network calls, or model iterations shall occur during validation.

**Acceptance Criterion (Code Review):**
- Read the tray-icon validation function after implementation
- Verify the function contains only comparisons, arithmetic, and early returns — no loops or I/O

---

## Item 2: CalDAV Dead Timeout Guard + Duplicated HTTP-Sync Scaffolding (U-07 F-01 + F-10, Critical + Medium)

**Defect**: `CalDavProvider::sendSync()`'s documented "returns nullptr on timeout" contract is dead code. `QNetworkAccessManager::sendCustomRequest()` never returns nullptr, and the function never checks whether the reply actually finished before returning it. A hung CalDAV server silently produces a "successful" empty sync with zero diagnostic. This exists because `CalDavProvider` and `IcsProvider` independently hand-rolled the same synchronous-HTTP-with-timeout idiom, and one copy silently dropped the abort() call the other has.

**Root location**: `libs/holonight-services/src/calendar/CalDavProvider.cpp` and `IcsProvider.cpp`; duplicated HTTP-with-timeout logic.

### REQ-F-2.1 (Functional — Event-Driven)

**When a CalDAV server does not respond within a timeout window, the system shall detect the timeout and abort the request.**

CalDAV synchronization must:
1. Send an HTTP request to the CalDAV server
2. Start a timeout timer (specify the concrete timeout value, e.g. 30 seconds; must match `IcsProvider`'s timeout)
3. If the request completes before the timeout fires, accept the response
4. If the timeout fires before the request completes, call `QNetworkReply::abort()` and treat the result as a synchronization failure (not a successful empty sync)

The timeout must be a guard that **actively aborts** the reply, not a passive timeout that merely stops waiting. This correction must bring `CalDavProvider` into alignment with `IcsProvider`'s existing correct timeout implementation.

**Acceptance Criterion (Automated):**
- Write a GTest in `tests/unit_caldav_timeout.cpp` that uses a mock/fake `QNetworkAccessManager`:
  - Create a fake reply that never signals `finished()` (simulating a hung server)
  - Call the CalDAV sync function with a 1-second timeout
  - Assert that after 1 second, `QNetworkReply::abort()` was called on the reply
  - Assert the function returns a failure result (not nullptr, but a designated failure enum or error signal)
  - Verify no empty CalendarProvider::SyncResult with event count == 0 is returned
- Run `task test` and confirm `test_caldav_timeout` passes

### REQ-F-2.2 (Functional — Event-Driven)

**When CalDAV synchronization fails (timeout or network error), the system shall emit a synchronization-failure signal to the calendar service.**

The `CalDavProvider::sendSync()` return value must change from "optional reply pointer (dead code)" to "a SyncResult struct or signal that explicitly indicates success vs. failure." The failure case must propagate to `CalendarSyncManager` so that:

1. Consecutive sync failures are counted
2. A `syncError()` signal (or equivalent `lastError` property) is emitted to QML
3. The calendar UI can display a visual indicator that sync is broken (e.g. a warning badge on the calendar widget)

Currently, a hung server produces an empty sync (0 events) with `success=true`, making it indistinguishable from a server with no upcoming events. After this fix, a hung server must signal `success=false` or emit a `syncError()` signal.

**Acceptance Criterion (Manual):**
- Set up a dummy calendar server that accepts connections but never responds (e.g. `nc -l localhost 8000` in a terminal, or a custom blocking socket)
- Configure the shell to sync against this dummy server
- Wait for the sync timeout (30 seconds, or whatever value is chosen)
- Verify one of the following:
  - A `syncError()` signal fires in the shell's logs, OR
  - A `lastError` property on the calendar service changes to a non-empty string, OR
  - The calendar widget displays an error indicator (visual confirmation is secondary; the primary requirement is that *some* failure path activates)
- Verify the shell does not crash and continues to function normally after the timeout

### REQ-F-2.3 (Functional — Ubiquitous)

**The system shall extract HTTP-synchronization-with-timeout logic into a shared helper class.**

Instead of duplicating the timeout pattern in both `CalDavProvider` and `IcsProvider`, the logic must be extracted into a single helper class (e.g. `HttpSyncClient`). Both providers shall use this helper, eliminating the risk of future silent divergence.

The helper shall:
1. Accept a URL, HTTP method, request body, and a timeout duration
2. Return a result object (not an optional pointer) indicating success/failure
3. Implement the timeout guard correctly (await the reply, call abort() on timeout)
4. Be documented with a comment explaining the timeout semantics for future maintainers

**Acceptance Criterion (Code Review):**
- After implementation, confirm that `CalDavProvider` and `IcsProvider` both instantiate and call the same `HttpSyncClient` class
- Grep the codebase for "QNetworkAccessManager::sendCustomRequest" — exactly two call sites should remain (in CalDavProvider and IcsProvider, now delegating to HttpSyncClient), not three or more
- Confirm the extraction is tested in the GTest from REQ-F-2.1

---

## Item 3: ConfigWriter Silently Deletes Weather Coordinates on Every Save (U-11 D-C1, High)

**Defect**: `ConfigWriter` in the Settings app converts `weather.latitude`/`longitude`/`city` to a commented-out placeholder on every single save, even when the user touched nothing weather-related, silently destroying any pinned weather location.

**Root location**: `apps/settings/src/ConfigWriter.cpp` — TOML generation logic for the `[weather]` section.

### REQ-F-3.1 (Functional — Unwanted Behaviour)

**The system shall not delete or blank weather configuration fields that the user did not modify.**

When the user saves settings in the Settings app, any weather-related fields (`latitude`, `longitude`, `city`) that are not currently being edited must retain their on-disk values unchanged. If the user's edit model has no explicit value for these fields (indicating the user did not touch weather settings), the ConfigWriter must preserve the existing on-disk value during the write operation.

**Acceptance Criterion (Automated):**
- Write a GTest in `tests/unit_configwriter_weather_preservation.cpp`:
  1. Create a test TOML file with `[weather]` section containing `latitude = 42.3`, `longitude = -71.1`, `city = "Boston"`
  2. Load this file into a `SettingsEditModel` (without modifying the weather fields)
  3. Call `ConfigWriter::write()` to save the model back to a new file
  4. Parse the output file and assert that:
     - `latitude`, `longitude`, `city` are still present and have their original values
     - They are NOT commented-out
     - They are NOT replaced with placeholder strings
  5. Repeat the test with the model's weather fields explicitly cleared (set to empty string) — this variant should be a separate test case asserting that empty fields are preserved as empty (not deleted)
- Run `task test` and confirm both test cases pass

### REQ-F-3.2 (Functional — State-Driven)

**While a weather location is pinned in the configuration, the shell shall use that location for weather forecasting, even if the Settings app is opened and closed without changes.**

After a user pins a weather location via the Settings app, that location must persist across Settings-app sessions. Simply opening and closing Settings without editing the weather section must not alter the pinned location.

**Acceptance Criterion (Manual):**
- Open the Settings app and navigate to the Weather/Location section
- Pin a weather location (e.g. "San Francisco" or coordinates)
- Close the Settings app
- Verify the weather widget on the topbar reflects the pinned location (or check the config file: `~/.config/holonight/config.toml` should contain the weather fields)
- Re-open the Settings app, navigate to the same Weather section
- Do NOT modify the weather location (do not click any buttons, do not change any values)
- Click "Apply" to save settings
- Close the Settings app
- Verify the weather location is still the same (repeat the verification from step 3)

---

## Item 4: Unregistered HolonightTheme QML Singleton (U-11 D-C2, High)

**Defect**: The `HolonightTheme` QML singleton, referenced by the Settings app's own default landing page, is never registered anywhere in the codebase. Every user hits a silent `ReferenceError` on first launch; the Theme/Accent picker sections render permanently empty with no visible error.

**Root location**: `apps/settings/qml/` — singleton references; and `apps/settings/src/SettingsApp.cpp` (or equivalent main/registration file) — missing singleton registration.

### REQ-F-4.1 (Functional — Ubiquitous)

**The system shall register the HolonightTheme QML singleton at startup.**

The `HolonightTheme` singleton must be registered using the same pattern already used elsewhere in the Settings app (grep for `qmlRegisterSingletonType` or `QML_SINGLETON` in the Settings app's C++ code and repeat that pattern for HolonightTheme).

**Acceptance Criterion (Code Review):**
- After implementation, grep `apps/settings/src/SettingsApp.cpp` (or the main-app C++ file) for a line registering `HolonightTheme`
- Verify the registration uses the same convention as other registered singletons (e.g. `qmlRegisterSingletonType<HolonightTheme>(...)`), not a custom pattern
- Verify the registration occurs before any QML is loaded (typically in the composition root's constructor or a pre-QML-load setup function)

### REQ-F-4.2 (Functional — Event-Driven)

**When the Settings app launches, the Theme/Accent picker section shall render without ReferenceError.**

The default landing page of the Settings app must load without any `ReferenceError: HolonightTheme is not defined` messages in the QML console.

**Acceptance Criterion (Manual):**
- Open the Settings app in a fresh session (e.g. restart the shell)
- Navigate to the Settings app (or confirm it auto-opens to the default landing page)
- Verify the Theme/Accent picker section is visible and displays selectable options (not empty, not showing an error)
- Verify the QML console (with `QT_FORCE_STDERR_LOGGING=1 holonight-settings 2>&1 | grep ReferenceError`) shows zero ReferenceError messages related to HolonightTheme

### REQ-F-4.3 (Functional — Ubiquitous)

**The system shall include a regression check ensuring the HolonightTheme singleton registration cannot be silently dropped in a future refactor.**

Add a lightweight verification (either a QML smoke test or an extension to the existing `task qmltypes-check` verification) that:
1. Loads the Settings app's QML in a test harness
2. Attempts to access `HolonightTheme` at the QML level
3. Asserts that the access succeeds (does not raise ReferenceError)

Alternatively, extend the `qmltypes-check` task to verify that `HolonightTheme` appears in the generated QML metatypes file (if it is an exported singleton).

**Acceptance Criterion (Automated or Build-time):**
- If implemented as a QML smoke test: create `tests/qml/tst_holonight_theme_singleton.qml` with a `TestCase` that loads a minimal QML file attempting to import and access `HolonightTheme`, asserting no error occurs. Run `task test` and confirm it passes.
- If implemented as an extended qmltypes-check: run `task qmltypes-check` and confirm the output includes HolonightTheme in the registered singletons list.

---

## Item 5: Control-Socket Sidebar DoS via Unvalidated Monitor Name (U-02 F-02, High)

**Defect**: The control-socket command `sidebar:toggle:<monitor>` tears down the currently-open sidebar *before* validating that the named monitor actually exists. Any local process with control-socket access can blindly close a user's sidebar by sending a malformed/nonexistent monitor name.

**Root location**: `libs/holonight-app/src/ControlServer.cpp` — control-socket message parsing and sidebar toggle logic.

### REQ-F-5.1 (Functional — Event-Driven)

**When a control-socket `sidebar:toggle:<monitor>` command arrives with an invalid monitor name, the system shall not modify the sidebar state.**

The monitor name must be validated against the currently known monitor list **before** any sidebar state changes occur. If the named monitor does not exist:
1. The entire command is rejected
2. No sidebar is closed, opened, or toggled
3. A `qCWarning` is logged identifying the rejected monitor name and the source (if available from the socket connection context)

**Acceptance Criterion (Automated):**
- Write a GTest in `tests/unit_control_socket_monitor_validation.cpp`:
  1. Mock the monitor list (e.g. create a fake `MonitorManager` or inject a test monitor list)
  2. Simulate a control-socket message with an invalid monitor name (e.g. `sidebar:toggle:NONEXISTENT-MONITOR`)
  3. Verify that the sidebar state did not change (no toggle occurred)
  4. Verify that a qCWarning was logged with the rejected monitor name
  5. Repeat with a valid monitor name and verify the toggle proceeds normally
- Run `task test` and confirm `test_control_socket_monitor_validation` passes

### REQ-F-5.2 (Functional — Ubiquitous)

**The system shall reject monitor names that are not in the currently known monitor list.**

Maintain a list of known monitors (derived from `MonitorManager` or `Hyprland`'s monitor list). When a `sidebar:toggle:<monitor>` command is received:
1. Check if the monitor name exists in the current monitor list
2. If not, reject the command
3. If yes, proceed with the toggle

This ensures that malformed, misspelled, or outdated monitor names (from a previous session) cannot trigger sidebar changes.

**Acceptance Criterion (Code Review):**
- Read the `ControlServer::handleSidebarToggle()` function (or equivalent)
- Verify the first operation is a monitor-name lookup/validation, not a sidebar state change
- Confirm that sidebar state changes only occur after validation passes

---

## Item 6: Unbounded Notification Payload Logging (U-06 F-02, High)

**Defect**: `NotificationServer::Notify()` performs no length validation on untrusted D-Bus input (summary, body, hint values) and unconditionally logs the full unbounded payload to disk. Any session-bus process (no privilege required) can force arbitrarily large, repeated writes into the persistent log file — an unauthenticated local DoS against disk space/log integrity.

**Root location**: `libs/holonight-services/src/notifications/NotificationServer.cpp` — D-Bus Notify() method and logging logic.

### REQ-F-6.1 (Functional — Unwanted Behaviour)

**The system shall not log unbounded notification payloads to disk.**

Every notification field that originates from untrusted D-Bus input must be bounded before being logged, stored, or displayed. The bounded fields are:
- `summary` (notification title)
- `body` (notification text)
- Each hint value (key-value pairs in the hints dictionary)

Establish a concrete maximum length per field (e.g. 4 KB per field, or a value justified by typical notification text sizes) and enforce it at the point of logging and storage.

**Acceptance Criterion (Automated):**
- Write a GTest in `tests/unit_notification_payload_bounds.cpp`:
  1. Create a mock notification with `summary` = 10 MB of repeated 'A' characters
  2. Call `NotificationServer::Notify()` with this oversized notification
  3. Verify that:
     - The notification is still processed (not dropped entirely)
     - The logged/stored version of `summary` is truncated to the maximum length (e.g. 4 KB)
     - A visible truncation marker (e.g. "...[truncated]" or similar) appears at the end of the truncated field in the log/storage
  4. Repeat with `body` and hint values
- Run `task test` and confirm the test passes

### REQ-F-6.2 (Functional — Event-Driven)

**When a notification with an oversized field is received, the system shall truncate that field with a visible marker before logging or displaying.**

The truncation must:
1. Preserve the beginning of the field (for readability)
2. Append a visible marker indicating truncation (e.g. "...[truncated]" or "[field exceeded 4 KB]")
3. Occur at or before the length limit (no over-allocation beyond the max)

The notification itself must still be shown to the user, but with the truncated text. This ensures the notification remains functional while bounding disk/memory impact.

**Acceptance Criterion (Manual):**
- Send a notification via D-Bus with an oversized body (e.g. 10 MB of text) using a tool like `gdbus-send` or a custom client
- Verify the notification appears on screen with the body text visible but truncated (not blank, not missing)
- Verify the log file (`~/.local/share/holonight/holonight-shell/holonight.log`) contains the truncated version, not the full 10 MB
- Verify no crash or hang occurred during the truncation/logging

### REQ-F-6.3 (Functional — Ubiquitous)

**The system shall apply the same length bounds to all notification fields.**

Summary, body, and each hint value must respect the same maximum length. This prevents attackers from bypassing the limit by using a different field.

**Acceptance Criterion (Code Review):**
- After implementation, read the truncation logic in `NotificationServer::Notify()`
- Verify that `summary`, `body`, and all hint values pass through the same `truncateToMaxLength()` function (or equivalent) before being logged/stored
- Confirm the maximum length is defined once (as a named constant), not repeated in multiple places where it could drift

---

## Cross-Cutting Requirements

### REQ-C-1 (Constraint — Manual Verification Protocol)

**All manual-verification acceptance criteria shall be executable by a human without AI-driven automation.**

When a criterion is marked "(Manual)", it must be phrased as a checklist of actions a human can perform in a live shell session. Do not imply that an AI agent will drive the shell UI programmatically or execute shell commands in a headless automated context.

**Examples of acceptable manual criteria:**
- "Open the Settings app and navigate to X; verify that Y is visible"
- "Run `gdbus-send ... org.freedesktop.Notifications.Notify ...` and observe the result"
- "Check the log file at ~/.local/share/holonight/holonight-shell/holonight.log for the message 'X'"

**Examples of unacceptable criteria:**
- "The agent shall open the Settings app, click the weather button, and verify X" (implies AI automation)
- "Drive the control socket with a test harness to send commands" (acceptable only if "test harness" is a pre-existing tool; not AI-automated)

### REQ-C-2 (Constraint — No Backward-Compatibility Shims)

This is a pre-production POC. Aggressive refactoring (e.g., extracting `HttpSyncClient` in Item 2, consolidating timeout patterns) is acceptable without compatibility shims or deprecation periods. Breaking changes are permitted if they improve correctness.

### REQ-C-3 (Constraint — Tested Logic Must Be Testable in Isolation)

Where an acceptance criterion is marked "(Automated)", the tested logic must be exercisable in a GTest or QtQuickTest without requiring:
- A live Wayland compositor
- A running D-Bus session (though mock D-Bus connections may be used)
- A live network server or calendar provider
- Programmatic UI driving (no QML script execution to click buttons)

If a fix requires a live D-Bus service, a real Wayland compositor, or a live network server to observe the end-to-end behavior, the acceptance criterion must be marked "(Manual)" instead, phrased as a human checklist.

### REQ-C-4 (Constraint — Testing Discipline)

For each requirement with an "(Automated)" acceptance criterion:
- Add a new test file under `tests/` (GTest: `tests/unit_*.cpp`; QML: `tests/qml/tst_*.qml`)
- List the new test file in the appropriate `tests/CMakeLists.txt` section
- Run `task configure-tests` explicitly after adding a new file (the configure dep can be stale)
- Run `task test` to confirm the new test passes
- Document the test's purpose in a code comment at the top of the file

For each requirement with a "(Manual)" acceptance criterion:
- The criterion must be unambiguous and executable by a non-expert in a single shell session
- Include expected observable outcomes (e.g., "the log file contains...", "the widget displays...", "no crash occurs")

### REQ-NF-2.1 (Non-Functional — Logging Verbosity)

All rejection/failure diagnostics must be logged at `qCWarning` level or higher. Do not use `qCDebug` for information that should be visible by default (without `-v` flag or `QT_LOGGING_RULES`). Diagnostics must be recoverable from the persistent log file at `~/.local/share/holonight/holonight-shell/holonight.log`.

### REQ-NF-2.2 (Non-Functional — Localization & Accessibility)

Log messages may be English-only (no localization required at the log level). User-visible error messages (if any) must be human-readable and informative (e.g., avoid opaque error codes without explanation).

---

## Non-Goals

The following items are **explicitly out of scope** for Phase 0 and are deferred to later SDD cycles:

### Phase 1–6 Remediation Items

The review's full remediation roadmap (detailed in REPORT.md §7) includes architectural refactoring cycles (Phase 1) and cross-cutting gap fixes (Phases 2–6). This spec covers only Phase 0; later cycles will address:

- **Architecture Gap #1** (Silent failure paths) — establishing a project-wide error-propagation convention
- **Architecture Gap #3** (`roleNames()` caching) — the ~10-site mechanical sweep to cache role names
- **Architecture Gap #5** (Synchronous blocking I/O) — extracting and applying the async-with-timeout helper at all call sites
- **Architecture Gap #6** (Built-but-unwired APIs) — auditing and integrating/removing unused backend APIs
- **Architecture Gap #7** (Release-mode-silent assertions) — replacing composition-root Q_ASSERTs with loud qCritical checks
- All remaining Medium and Low severity findings from the 11 units (62+ additional items)

### Investigation-Target Items (60–79/100 Confidence)

The review identified 102 "investigation targets" — findings with 60–79/100 confidence that require human verification before any remediation cycle acts on them. These are explicitly deferred pending separate human review, not included in this spec.

### Test-Coverage Refresh

DESIGN.md §5 observation #7 notes that formal test-coverage auditing is ~5 weeks stale. This spec does not include a test-coverage refresh — that remains a separately-scoped concern.

---

## Summary

**Total Requirements:** 23 (16 functional, 3 non-functional, 4 constraint)

**Organized as:**
- **Item 1 (Tray pixmap overflow)**: REQ-F-1.1, REQ-F-1.2, REQ-F-1.3, REQ-NF-1.1
- **Item 2 (CalDAV timeout + HTTP helper)**: REQ-F-2.1, REQ-F-2.2, REQ-F-2.3
- **Item 3 (ConfigWriter weather)**: REQ-F-3.1, REQ-F-3.2
- **Item 4 (HolonightTheme singleton)**: REQ-F-4.1, REQ-F-4.2, REQ-F-4.3
- **Item 5 (Control-socket DoS)**: REQ-F-5.1, REQ-F-5.2
- **Item 6 (Notification logging)**: REQ-F-6.1, REQ-F-6.2, REQ-F-6.3
- **Cross-cutting**: REQ-C-1, REQ-C-2, REQ-C-3, REQ-C-4, REQ-NF-2.1, REQ-NF-2.2

Each requirement includes:
- An EARS-format statement (Ubiquitous, Event-driven, State-driven, Conditional, or Unwanted-behaviour)
- Independent acceptance criteria (automated GTest/QtQuickTest or manual checklist)
- Clear traceability to the original review finding

---

## Appendix: EARS Template Reference

This spec uses the following EARS templates:

| Template | Format | Example |
|---|---|---|
| **Ubiquitous** | The system shall [capability] | REQ-F-3.1: The system shall not delete weather fields the user did not modify |
| **Event-driven** | When [event], the system shall [response] | REQ-F-1.2: When a tray icon with rejected dimensions is received, the system shall emit a diagnostic log |
| **State-driven** | While [state], the system shall [capability] | REQ-F-3.2: While a weather location is pinned, the shell shall use that location for forecasting |
| **Conditional** | If [condition], the system shall [response] | REQ-F-2.1: If the request does not complete before timeout, call abort() and treat as failure |
| **Unwanted-behaviour** | The system shall not [unwanted behaviour] | REQ-F-1.1: The system shall not allocate unbounded memory based on untrusted dimensions |
