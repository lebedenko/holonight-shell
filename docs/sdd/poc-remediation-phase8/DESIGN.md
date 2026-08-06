# Phase 8 — Correctness Remediation: Design

**Input**: `poc-remediation-phase8/SPEC.md`
**Baseline**: Phase 7 revalidated at `fe32997a7c2264d9b54efbf78bead03983c4f7f8`; implementation shall re-check current HEAD before editing.

## 1. Change Map

| Requirement | Primary implementation | Tests |
|---|---|---|
| F-01 | `apps/shell/app/ControlServer.{h,cpp}` | `tests/test_control_server.cpp` |
| F-02 | `libs/holonight-services/src/BatteryService.cpp` | `tests/test_battery_service.cpp`, `tests/test_low_battery_monitor.cpp` |
| F-03 | `libs/holonight-services/src/calendar/CalendarSyncManager.{h,cpp}` | `tests/test_calendar_integration.cpp` |
| F-04 | `libs/holonight-surfaces/src/TooltipSurface.cpp` plus a small pure geometry helper | `tests/test_status_popup_geometry.cpp` or a focused new surfaces test |
| F-05 | `apps/settings/src/SettingsEditModel.cpp` | `tests/test_settings_app.cpp` |
| F-06 | `apps/settings/qml/FooterBar.qml` | focused QML smoke/inspection test if needed |

No persistent schema or public D-Bus API changes are planned.

## 2. Design Decisions

### 2.1 Control socket: EOF-delimited single command

The command-line client writes one payload then disconnects. `readyRead` is a
transport notification, not a message boundary, so each accepted socket will
own an append-only receive buffer. The server will:

1. append bytes on `readyRead`;
2. reject and disconnect immediately once the buffer exceeds named
   `kMaxCommandBytes = 4096`;
3. decode and dispatch once on `disconnected`, if the buffer was not rejected.

Each server-owned `ControlCommandBuffer` is stored by socket identity in a
`QHash`; its entry is removed during finalization. This makes the ownership and
single-dispatch rule explicit, avoids arbitrary delimiters in monitor names,
and lets the framing/limit behavior be tested without a local-socket
environment.

### 2.2 Battery percentage: normalize at one boundary

`BatteryService::setPercent()` becomes the sole normalization boundary:
`std::clamp(value, 0, 100)` precedes the equality test and assignment. This
protects both UPower property updates and direct `applyStateUpdate()` callers.
`LowBatteryMonitor` needs no duplicate clamp; it consumes the now-valid service
property.

### 2.3 Calendar: typed composite transient key

Use a private `SyncKey` value containing `provider_type` and `account_name` for
both transient maps, with one helper that constructs it. The existing
`std::pair<QString, QString>` hash support in the header can provide this key
without string concatenation or delimiter ambiguity.

Every access in `runProviderSync()`, `onSyncFinished()`, and `removeAccount()`
must use the helper. Cache storage intentionally remains `(provider_type,
account)` arguments because it already has the required namespace.

### 2.4 Tooltip: isolate coordinate math

Extract a small pure helper that takes `screen_width`, `screen_origin_x`,
`anchor_x`, and `anchor_width` and returns the left margin. It subtracts
`screen_origin_x` before applying the existing center/clamp calculation.
`TooltipSurface::ensureSurface()` supplies `screen->geometry().x()`.

Keeping the helper free of `QScreen`/Wayland objects provides deterministic
tests without a compositor. It must reuse existing tooltip dimensions and
`kScreenEdgeMargin`; it must not alter layer-shell setup.

### 2.5 Settings: model bounds mirror UI bounds

Define named bounds in `SettingsEditModel.cpp` and clamp at each of the four
integer setter boundaries before comparison. The ranges intentionally mirror
the existing QML sliders, rather than parser-only bounds, so all entry paths
maintain the same invariant.

### 2.6 Footer: one intentional action, correct availability

The settings feature SDD explicitly specifies that Apply and Save & Apply share
`ConfigFileService::save()`. Retain the labels and callbacks; replace each
button's availability binding with the same expression:

```
editModel.isDirty && !fileService.isSaving
```

This prevents meaningless writes and makes the action state consistent with
Discard Changes.

## 3. Test Strategy

| Area | Deterministic coverage |
|---|---|
| Control server | Exercise `ControlCommandBuffer` with multiple chunks and an oversized chunk; decode the completed buffer to verify the existing command contract. |
| Battery | Drive `applyStateUpdate()` below/above bounds and assert normalized value/signals; extend low-battery tests for a negative input. |
| Calendar | Create same-named fake CalDAV/ICS providers. Make one fail or remove it and verify the other still fetches. |
| Tooltip | Test pure local-coordinate computation for origin 0 and an offset origin, including clamping. |
| Settings | Call every numeric setter at values below/above the defined bounds and inspect `toParsedConfig()`. |
| Footer | Use QML test or source-level smoke assertion to verify both controls retain `save()` and use the dirty/saving gate. |

The implementation must not claim compositor validation from automated tests.

## 4. Risks

- Control EOF is the established client completion signal. A future persistent,
  multi-command client would require an explicit versioned framing protocol and
  is out of scope.
- Calendar sync workers are asynchronous. The test must wait on observable
  fetch/error signals and avoid inspecting private maps.
- Tooltip geometry verifies the bug mechanically, but real multi-monitor
  Wayland placement still needs the manual check described in TASKS.md.
