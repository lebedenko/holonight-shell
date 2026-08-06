# SDD Tasks — poc-remediation-phase2

- [x] T-001: Refactor parseTimeToEventFields and parseWidgetPositionField signatures
  - REQs: REQ-F-D.2, REQ-C-D.1
  - Check: libs/holonight-config/src/ConfigParsers.cpp contains `parseTimeToEventFields(const toml::table& entry, bool strict)` and `parseWidgetPositionField(const toml::table& entry, const QString& label, bool strict)` with `if (strict)` branches wrapping rejection paths, leaving `parseClockFields` and `parseWidgetMonitors` unchanged.

- [x] T-002: Restructure parseWidgetEntry to unconditionally call all field-parser helpers
  - REQs: REQ-F-D.1, REQ-F-D.2, REQ-F-D.3, REQ-C-D.1
  - Check: libs/holonight-config/src/ConfigParsers.cpp parseWidgetEntry calls parseTimeToEventFields, parseWidgetPositionField, parseClockFields, and parseWidgetMonitors unconditionally for both "time-to-event" and "clock" types with `strict = enabled`, and the missing-deadline branch in parseTimeToEventFields uses `if (deadline_opt) {...} else {...}` instead of early return.

- [x] T-003: Add disabled-widget field-preservation test cases to test_config_parsers.cpp
  - REQs: REQ-F-D.1, REQ-F-D.2, REQ-F-D.3, REQ-C-3
  - Check: tests/test_config_parsers.cpp contains TEST cases DisabledTimeToEventPreservesValidFields, DisabledClockPreservesFields, DisabledEntryPreservesPositionAndMonitors, DisabledEntryWithMissingTitleDefaultsToEmptyString, DisabledEntryWithInvalidDeadlineDefaultsAndWarns, DisabledEntryWithInvalidPositionDefaultsToCenterCenter, DisabledEntryMonitorsSkipsNonStringEntriesOnly, EnabledEntryWithInvalidFieldStillRejectsWholeEntry, and DisabledEntryWithUnknownTypeStaysUnchanged.

- [x] T-004: Add qCWarning lines to three silent Hyprland IPC parsers
  - REQs: REQ-F-E.1, REQ-C-E.1, REQ-NF-1.1
  - Check: libs/holonight-platform/src/HyprlandIpc.cpp contains `qCWarning(lcHyprlandIpc) << "parseHyprlandActiveWindowJson: expected JSON object";` before line ~37 return, identical line for parseHyprlandKeyboardLayoutDevicesJson before ~68 return, and identical line for parseHyprlandActiveWorkspaceJson before ~190 return.

- [x] T-005: Add Hyprland IPC logging test cases to test_hyprland_ipc.cpp
  - REQs: REQ-F-E.1, REQ-F-E.2, REQ-C-3
  - Check: tests/test_hyprland_ipc.cpp contains TEST cases WarnsOnMalformedActiveWindowJson, WarnsOnMalformedKeyboardLayoutDevicesJson, WarnsOnMalformedActiveWorkspaceJson each using QTest::ignoreMessage to capture the exact warning message, plus regression tests confirming the four sibling parsers (parseHyprlandMonitorsJson, parseHyprlandFocusedMonitorNameJson, parseHyprlandClientsJson, workspaceIdForHyprlandClientAddressJson) still log on malformed input.

- [x] T-006: Add qCWarning to HyprlandIpcClient::runCommand on unresolved socket path
  - REQs: REQ-F-F.1, REQ-C-F.1, REQ-NF-1.1
  - Check: libs/holonight-platform/src/HyprlandIpcClient.cpp runCommand() contains `qCWarning(lcHyprlandIpcClient) << service_name_ << "HYPRLAND_INSTANCE_SIGNATURE not set; dropping command" << command;` before the `return false;` on the `resolvedCommandSocketPath().isEmpty()` branch.

- [x] T-007: Add runCommand logging test cases to test_hyprland_ipc_client.cpp
  - REQs: REQ-F-F.1, REQ-F-F.2, REQ-C-3
  - Check: tests/test_hyprland_ipc_client.cpp contains TEST case RunCommandWarnsAndDropsWhenSignatureUnset constructing HyprlandIpcClient with unset HYPRLAND_INSTANCE_SIGNATURE, calling runCommand with QTest::ignoreMessage capturing the warning, and asserting return false; plus regression test RunCommandSucceedsWithoutWarningWhenSignatureSet using ScopedHyprlandSocketEnv and confirming no warning when socket is resolvable.

- [x] T-008: Run full test suite and confirm all new test cases pass
  - REQs: REQ-C-3
  - Check: task configure-tests explicitly re-run (to refresh stale configure deps), followed by task test in build/ directory, returns exit code 0 with all test_config_parsers, test_hyprland_ipc, and test_hyprland_ipc_client tests passing, including pre-existing regression cases.

- [x] T-009: Manually verify disabled widget round-trip through Settings app
  - REQs: REQ-F-D.4, REQ-C-1
  - Check: Manually edit ~/.config/holonight/config.toml with `enabled = false`, `type = "time-to-event"`, `title = "My Meeting"`, `deadline = "2026-12-25T14:00:00"`, start shell, open Settings app, toggle a non-widget setting (e.g. theme), click Apply, close app, and verify the config file still contains the disabled widget's title and deadline fields unchanged with no new warnings in ~/.local/share/holonight/holonight-shell/holonight.log (requires live Wayland session and human verification).
