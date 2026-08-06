# POC Remediation Phase 2 — EARS Requirements Specification

| Field | Value |
|---|---|
| Document ID | poc-remediation-phase2/SPEC.md |
| Cycle | Remediation Phase 2 of the POC Readiness Review |
| Reference | Review REPORT.md §4 (U-01 Unit), UNITS.md/U-01-foundational-core.md ([F-01], [F-04], [F-05]) |
| Scope | 3 foundational-layer bug fixes: 1 High, 2 Medium severity |
| Status | In development |

---

## Overview

This specification addresses 3 discrete, self-contained bug fixes from the POC Readiness Review Unit U-01 (Foundational Core: `holonight-config` and `holonight-platform`). All items are detection/validation/logging gaps with trivial reproduction and no architectural prerequisites. Requirements are written in EARS (Easy Approach to Requirements Syntax) format, organized into 3 requirement groups (one per item), with functional (REQ-F) and constraint (REQ-C) IDs continuing the letter sequence (D, E, F) established in Phase 0–1.

---

## Item D: Disabled `[[widget]]` Config Entries Silently Lose Field Data on Every Parse

**Defect**: When a `[[widget]]` TOML entry has `enabled = false`, the `parseWidgetEntry()` function skips parsing the optional fields (title, deadline, position, monitors, clock fields) entirely. Since `ConfigService::parseFile()` runs this on every shell startup and the Settings app (`apps/settings/src/ConfigFileService.cpp`) round-trips the same parsed struct through `ConfigWriter::write()` on any save, disabling a widget and then saving any unrelated settings change (e.g. an appearance tweak) permanently erases that widget's title, deadline, position, and monitor scoping — a real silent data-loss path a user can hit today.

**Root location**: `libs/holonight-config/src/ConfigParsers.cpp`, function `parseWidgetEntry` (currently lines ~410-460).

### REQ-F-D.1 (Functional — Unwanted Behaviour)

**The system shall not lose widget configuration data when a `[[widget]]` entry has `enabled = false`.**

When parsing a `[[widget]]` TOML entry, the parser must extract and preserve all field data (title, deadline, position, monitors, clock-specific fields) regardless of the `enabled` flag. The `enabled` flag must only control *validation strictness* for invalid individual fields, not whether those fields are parsed at all.

**Acceptance Criterion (Automated):**
- Write a GTest in `tests/unit_widget_disabled_field_preservation.cpp`:
  1. Create a test TOML entry with `enabled = false`, `type = "time-to-event"`, a valid title "Weekly Review", and a valid deadline "2026-01-15T10:00:00"
  2. Call `parseWidgetEntry()` on this entry
  3. Assert that the returned `WidgetDefinition` (or `std::optional<WidgetDefinition>`, if still using that type) contains:
     - `.enabled == false`
     - `.type == "time-to-event"`
     - `.time_to_event.title == "Weekly Review"` (not empty)
     - `.time_to_event.deadline` correctly populated (not default-constructed)
  4. Repeat the test with a `type = "clock"` entry, asserting `.clock` fields are populated
  5. Repeat with a `type = "time-to-event"` entry that includes explicit `position` and `monitors` fields, asserting those are present in the returned struct
- Run `task configure-tests` explicitly, then `task test`, and confirm `test_widget_disabled_field_preservation` passes

### REQ-F-D.2 (Functional — Conditional)

**If an individual field (title, deadline, position, monitor, clock property) is invalid or missing within a disabled widget entry, the system shall apply a safe default for that field instead of rejecting the entire entry.**

When `enabled == false` and an individual field is invalid (e.g. empty title, unparseable deadline, invalid position string):
1. Log a `qCWarning` identifying the invalid field and the reason (same warning the field parser already emits)
2. Apply a safe default for *only that field*:
   - Invalid/missing title → empty `QString{}`
   - Invalid/missing/unparseable deadline → the field's default-constructed value (e.g. null `QDateTime{}`)
   - Invalid position string → `WidgetPosition::CenterCenter` (center default)
   - A `monitors` array entry that is not a string → skip that entry (existing per-entry-skip behavior, unchanged)
3. Return a fully-populated `WidgetDefinition` with the defaulted fields, not `std::nullopt`

**Acceptance Criterion (Automated):**
- Extend `tests/unit_widget_disabled_field_preservation.cpp` with additional test cases:
  1. Test case: `enabled = false`, `type = "time-to-event"`, **no** title field (missing)
     - Assert returned `WidgetDefinition.time_to_event.title == ""` (empty string)
  2. Test case: `enabled = false`, `type = "time-to-event"`, `title = ""` (explicitly empty)
     - Assert returned `WidgetDefinition.time_to_event.title == ""` (preserved as empty)
  3. Test case: `enabled = false`, `type = "time-to-event"`, invalid deadline syntax (e.g. `deadline = "not-a-date"`)
     - Assert a `qCWarning` is logged (existing behavior from field parser)
     - Assert returned `WidgetDefinition.time_to_event.deadline` is default-constructed
  4. Test case: `enabled = false`, `type = "time-to-event"`, `position = "invalid-position-name"`
     - Assert returned `WidgetDefinition.position == WidgetPosition::CenterCenter` (safe default, not causing rejection)
  5. Test case: `enabled = false`, `type = "time-to-event"`, `monitors = ["HDMI-1", 123]` (mixed valid string and invalid int)
     - Assert returned `WidgetDefinition.monitors` contains only `"HDMI-1"` (non-string entries skipped)
     - Assert no rejection (`std::nullopt` is NOT returned)
- Run `task configure-tests`, then `task test`, and confirm all cases pass

### REQ-F-D.3 (Functional — Conditional)

**If an individual field within an enabled widget entry is invalid, the system shall continue to reject the entire entry (existing behavior, unchanged).**

For entries with `enabled == true`, the existing validation behavior is preserved: if any single field is invalid, the entire entry is rejected and returned as `std::nullopt`, causing the widget to be dropped from the configuration. This ensures that the user's *active* widgets remain in a consistent state.

**Acceptance Criterion (Code Review):**
- After implementation, read `parseWidgetEntry()` in `libs/holonight-config/src/ConfigParsers.cpp`
- Verify that the code structure contains a branch checking `enabled` **after** parsing all fields (not before)
- Confirm that validation/rejection logic only executes when `enabled == true`
- Confirm that when `enabled == false` and individual fields are invalid, safe defaults are applied instead of returning `std::nullopt`

### REQ-F-D.4 (Functional — State-Driven)

**While a disabled widget's configuration is persisted, saving any other settings change must preserve that widget's field data intact.**

After a user disables a widget in the configuration, that widget's entry must survive a Settings-app save (where the app reloads, re-parses, and re-writes the config) with all field data (title, deadline, position, monitors) intact.

**Acceptance Criterion (Manual):**
1. Manually edit `~/.config/holonight/config.toml` and add a `[[widget]]` entry with `enabled = false`, `type = "time-to-event"`, a title "My Meeting", and a deadline `"2026-12-25T14:00:00"`
2. Start the shell (or verify the config is readable by opening Settings app without saving)
3. Open the Settings app
4. Navigate to the Appearance or any non-widget-related settings
5. Make a trivial change (e.g. toggle the theme between Light and Dark)
6. Click "Apply" to save
7. Close the Settings app
8. Manually verify the config file (`~/.config/holonight/config.toml`):
   - The disabled widget entry is still present
   - The `title` field is still "My Meeting" (not deleted, not commented-out, not empty)
   - The `deadline` field is still `"2026-12-25T14:00:00"` (not deleted, not commented-out)
   - No new warnings or errors in `~/.local/share/holonight/holonight-shell/holonight.log` related to the disabled widget

---

## Item E: Inconsistent Error Logging Across `HyprlandIpc.cpp` JSON Response Parsers

**Defect**: Three parser functions return `std::nullopt`/defaults on a malformed/unexpected-shape Hyprland IPC JSON response with ZERO logging: `parseHyprlandActiveWindowJson` (currently ~line 33), `parseHyprlandKeyboardLayoutDevicesJson` (currently ~line 64), and `parseHyprlandActiveWorkspaceJson` (currently ~line 186). Meanwhile, four sibling parsers in the same file (`parseHyprlandMonitorsJson`, `parseHyprlandFocusedMonitorNameJson`, `parseHyprlandClientsJson`, `workspaceIdForHyprlandClientAddressJson`) all log on the identical failure shape with `qCWarning`. A malformed/truncated hyprctl reply for active-window or active-workspace queries is currently diagnostically invisible, turning a compositor-side regression into an unexplained "workspace indicator stopped updating" bug report with no log trail.

**Root location**: `libs/holonight-platform/src/HyprlandIpc.cpp`, lines ~33, ~64, ~186 (silent) vs. existing logged patterns in sibling functions.

### REQ-F-E.1 (Functional — Event-Driven)

**When a Hyprland IPC JSON response has an unexpected shape (not an object or array as expected), the system shall emit a diagnostic log entry at the qCWarning level.**

All three parser functions (`parseHyprlandActiveWindowJson`, `parseHyprlandKeyboardLayoutDevicesJson`, `parseHyprlandActiveWorkspaceJson`) must log on the malformed-response branch, matching the existing pattern in the four sibling parsers.

Each log message must:
1. Name the affected parser function (e.g. "parseHyprlandActiveWindowJson")
2. State the expected type (e.g. "expected JSON object")
3. Use the category `lcHyprlandIpc` (already defined and used by sibling functions)

**Acceptance Criterion (Code Review):**
- After implementation, read `libs/holonight-platform/src/HyprlandIpc.cpp` and verify:
  1. Line ~33 (inside `parseHyprlandActiveWindowJson`, on the `!doc.isObject()` branch) contains a `qCWarning(lcHyprlandIpc) << "parseHyprlandActiveWindowJson: expected JSON object";` (or semantically identical message)
  2. Line ~64 (inside `parseHyprlandKeyboardLayoutDevicesJson`, on the `!doc.isObject()` branch) contains a `qCWarning(lcHyprlandIpc) << "parseHyprlandKeyboardLayoutDevicesJson: expected JSON object";`
  3. Line ~186 (inside `parseHyprlandActiveWorkspaceJson`, on the `!doc.isObject()` branch) contains a `qCWarning(lcHyprlandIpc) << "parseHyprlandActiveWorkspaceJson: expected JSON object";`
- Verify that the log line appears **before** the existing `return` statement in each branch
- Verify that the message naming/format matches the existing pattern from the four sibling parsers (same function-name prefix, same expected-type phrasing)

### REQ-F-E.2 (Functional — Ubiquitous)

**The system shall maintain diagnostic consistency across all Hyprland IPC JSON parsers.**

Any malformed Hyprland response shall be loggable for all eight parser functions without the caller needing to disambiguate which parser is failing. A user reading the log should be able to determine:
1. That a parse failure occurred
2. Which specific parser function encountered the failure
3. What the failure was (e.g. "expected JSON object")

**Acceptance Criterion (Automated):**
- Write a GTest in `tests/unit_hyprland_ipc_logging.cpp`:
  1. Mock the logging system (use Qt's `QTest::ignoreMessage()` or a custom log sink) to capture `qCWarning` messages
  2. Call each of the three previously-silent parsers (`parseHyprlandActiveWindowJson`, `parseHyprlandKeyboardLayoutDevicesJson`, `parseHyprlandActiveWorkspaceJson`) with a malformed input (e.g. a JSON string containing `{}` when an array is expected, or a string "invalid" instead of JSON)
  3. Assert that exactly one `qCWarning` was logged for each call
  4. Assert that each logged message contains the parser's name (e.g., "parseHyprlandActiveWindowJson")
  5. Repeat the same calls on the four sibling parsers and verify they also log (regression check — ensure existing logging was not accidentally removed)
- Run `task configure-tests`, then `task test`, and confirm `test_hyprland_ipc_logging` passes

---

## Item F: `HyprlandIpcClient::runCommand` Silently No-ops When Command Socket Path Can't Be Resolved

**Defect**: `HyprlandIpcClient::connectEventStream()` logs a warning and schedules a reconnect when `resolvedEventSocketPath()` is empty (i.e. `HYPRLAND_INSTANCE_SIGNATURE` env var unresolved). `runCommand()` hits the identical empty-path condition via `resolvedCommandSocketPath().isEmpty()` but just `return false;` with no log and no retry. Callers (`HyprlandWorkspaceService::startCommand`, `KeyboardLayoutService::queryCurrentLayout`) either discard the bool or silently reset pending-command state, so if a command fires before the env var is available there is no log entry anywhere explaining why workspace/keyboard-layout queries never complete.

**Root location**: `libs/holonight-platform/src/HyprlandIpcClient.cpp`, function `runCommand` (currently ~line 60-68), compared against `connectEventStream` (currently ~line 21-34).

### REQ-F-F.1 (Functional — Event-Driven)

**When `runCommand()` is invoked but the command socket path cannot be resolved (because `HYPRLAND_INSTANCE_SIGNATURE` is unset), the system shall emit a diagnostic log entry at the qCWarning level.**

The log message must:
1. Identify the service name (`service_name_` member, already logged by `connectEventStream()`)
2. State the reason (e.g., "HYPRLAND_INSTANCE_SIGNATURE not set")
3. Identify the command being dropped (the `command` parameter or its purpose, to help the caller diagnose why their command did not execute)
4. Use the category `lcHyprlandIpcClient` (already defined and used by `connectEventStream()`)

**Acceptance Criterion (Code Review):**
- After implementation, read `libs/holonight-platform/src/HyprlandIpcClient.cpp` and verify:
  1. Inside `runCommand()`, on the branch where `resolvedCommandSocketPath().isEmpty()` is true, a `qCWarning(lcHyprlandIpcClient) << service_name_ << "HYPRLAND_INSTANCE_SIGNATURE not set; dropping command" << command;` (or semantically equivalent message) appears
  2. The log line appears **before** the existing `return false;`
  3. The message style matches the existing pattern from `connectEventStream()` (same category, same env-var-name phrasing, same service-name inclusion)

### REQ-F-F.2 (Functional — Conditional)

**If the command socket path is unresolved, the system shall not attempt to connect or send the command; the log entry must clearly indicate the command was dropped.**

No retry logic, no command queueing — this requirement is logging-only. The `false` return value is preserved; callers continue to handle it as they do today. The addition is purely diagnostic visibility.

**Acceptance Criterion (Automated):**
- Write a GTest in `tests/unit_hyprland_ipc_client_logging.cpp`:
  1. Construct an `HyprlandIpcClient` with an empty `HYPRLAND_INSTANCE_SIGNATURE` environment (or mock the `resolvedCommandSocketPath()` method to return an empty string)
  2. Mock the logging system (use `QTest::ignoreMessage()` or a custom log sink) to capture `qCWarning` messages
  3. Call `runCommand("set_workspace 3")` (or another sample command)
  4. Assert that `runCommand()` returns `false`
  5. Assert that exactly one `qCWarning` was logged
  6. Assert that the logged message contains:
     - The service name (if available, or the class name)
     - "HYPRLAND_INSTANCE_SIGNATURE" or equivalent env-var identifier
     - The command string or its identifier (to link the log to the caller's intent)
  7. Repeat with a valid socket path and a successful command; assert `runCommand()` returns `true` and no warning is logged
- Run `task configure-tests`, then `task test`, and confirm `test_hyprland_ipc_client_logging` passes

---

## Cross-Cutting Requirements

### REQ-C-D.1 (Constraint — Widget Field Parsing Order)

**The `parseWidgetEntry()` function must parse all optional fields (title, deadline, position, monitors, clock fields) unconditionally, before checking the `enabled` flag for validation strictness.**

This ensures that regardless of implementation order, disabled entries always receive full field data.

**Verified by**: Code review of `parseWidgetEntry()` in `libs/holonight-config/src/ConfigParsers.cpp` confirms field parsing occurs before the `enabled` flag is checked for rejection.

### REQ-C-E.1 (Constraint — Parser Consistency)

**All Hyprland IPC JSON parsers in `HyprlandIpc.cpp` must follow the same logging convention on malformed input.**

New log messages in the three previously-silent parsers must use the same function-name prefix, error description phrasing, and log category (`lcHyprlandIpc`) as existing parsers.

**Verified by**: Code review and automated test assert that all eight parsers log on the same failure class with identical message style.

### REQ-C-F.1 (Constraint — Diagnostic Consistency)

**`HyprlandIpcClient::runCommand()` and `connectEventStream()` must use the same logging pattern when the socket path cannot be resolved.**

The log message in `runCommand()` must reference the same environment variable (`HYPRLAND_INSTANCE_SIGNATURE`) and include the same contextual information (service name, attempted action) as `connectEventStream()`.

**Verified by**: Code review confirms message style and category match between the two functions.

### REQ-C-1 (Constraint — Manual Verification Protocol)

**All manual-verification acceptance criteria shall be executable by a human without AI-driven automation.**

Manual criteria (e.g., REQ-F-D.4) are phrased as human checklists and do not require programmatic UI driving or headless automation.

### REQ-C-2 (Constraint — No Architectural Refactoring)

This is Phase 2 of a multi-phase remediation cycle addressing isolated bug fixes. No architectural refactoring (e.g., extracting new helper classes or reorganizing module boundaries) is in scope. Each fix is self-contained within its existing module.

### REQ-C-3 (Constraint — Testing Discipline)

For each requirement with an "(Automated)" acceptance criterion:
- Add a new test file under `tests/` (GTest: `tests/unit_*.cpp`)
- List the new test file in `tests/CMakeLists.txt`
- Run `task configure-tests` explicitly after adding a new file (configure dependency can be stale)
- Run `task test` to confirm all new tests pass
- Document the test's purpose in a code comment at the top of the file

For each requirement with a "(Code Review)" acceptance criterion:
- The criterion is verified by human inspection of the code after implementation
- No new test file is required if the acceptance criterion is purely structural

For each requirement with a "(Manual)" acceptance criterion:
- The criterion must be unambiguous and executable by a non-expert in a single shell session
- Include expected observable outcomes (e.g., "the config file contains...", "no logs appear for X", "no crash occurs")

### REQ-NF-1.1 (Non-Functional — Logging Verbosity)

All diagnostic log messages added in this phase must be logged at `qCWarning` level. Diagnostics must be recoverable from the persistent log file at `~/.local/share/holonight/holonight-shell/holonight.log`. Do not use `qCDebug` for information that should be visible by default.

### REQ-NF-1.2 (Non-Functional — Message Clarity)

All log messages must be human-readable and actionable. Log messages must identify:
1. The component/function that generated the message (function name or category)
2. The specific failure or condition
3. Relevant context (e.g. service name, command name, config entry name)

Examples of acceptable messages:
- "parseHyprlandActiveWindowJson: expected JSON object"
- "workspace-service: HYPRLAND_INSTANCE_SIGNATURE not set; dropping command \"set_workspace 3\""

Examples of unacceptable messages:
- "Error" (no context)
- "Invalid input" (no identification of what was invalid)

---

## Non-Goals

The following items are **explicitly out of scope** for Phase 2:

### F-01 — Validation for unknown/missing `type` field
The current behavior of `parseWidgetEntry()` when `type` is missing or unrecognized (neither `"time-to-event"` nor `"clock"`) is preserved unchanged in both enabled and disabled cases. This is a pre-existing, consistent behavior, not part of this finding's regression.

### F-01 — Retry/queue behavior for invalid fields in disabled entries
When a disabled widget has an invalid field, the default-value fallback is strictly field-level (title→empty string, deadline→null, position→center). No re-validation or re-parsing of the field occurs — the fallback is applied once.

### F-04 — Adding logging to new parsers
This phase adds logging to the three existing silent parsers only. No new parser functions are created or instrumented.

### F-05 — Retry/queue logic for `runCommand()`
The fix is logging-only. No retry loop, no command queueing, no behavioral change to the `false` return value — only adding a `qCWarning` before the existing `return false;`.

### Phase 3+ Remediation Items
The audit identified additional Medium/Low severity findings and investigation targets (I-01 through I-10) that are explicitly deferred to Phase 3 and later cycles. This spec covers only F-01, F-04, and F-05 from U-01.

---

## Summary

**Total Requirements:** 11 (8 functional, 3 non-functional, 5+ cross-cutting constraints)

**Organized as:**
- **Item D (Disabled widget field preservation)**: REQ-F-D.1, REQ-F-D.2, REQ-F-D.3, REQ-F-D.4, REQ-C-D.1
- **Item E (Inconsistent parser logging)**: REQ-F-E.1, REQ-F-E.2, REQ-C-E.1
- **Item F (Silent runCommand no-op)**: REQ-F-F.1, REQ-F-F.2, REQ-C-F.1
- **Cross-cutting**: REQ-C-1, REQ-C-2, REQ-C-3, REQ-NF-1.1, REQ-NF-1.2

Each requirement includes:
- An EARS-format statement (Ubiquitous, Event-driven, State-driven, or Conditional)
- Independent acceptance criteria (automated GTest, code-review, or manual checklist)
- Clear traceability to the original audit finding

All three items are **Severity High or Medium** and **Effort S (small)**, with acceptance criteria verifiable in a single development cycle.

---

## Appendix: EARS Template Reference

This spec uses the following EARS templates:

| Template | Format | Example |
|---|---|---|
| **Ubiquitous** | The system shall [capability] | REQ-F-D.1: The system shall not lose widget configuration data |
| **Event-driven** | When [event], the system shall [response] | REQ-F-E.1: When a JSON response has unexpected shape, the system shall emit a diagnostic log |
| **State-driven** | While [state], the system shall [capability] | REQ-F-D.4: While a disabled widget's config is persisted, saving must preserve field data |
| **Conditional** | If [condition], the system shall [response] | REQ-F-D.2: If an individual field is invalid within a disabled entry, the system shall apply a safe default |
| **Unwanted-behaviour** | The system shall not [unwanted behaviour] | REQ-F-D.1: The system shall not lose widget configuration data |
