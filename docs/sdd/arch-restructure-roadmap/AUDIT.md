# Test Coverage Audit — arch-restructure-roadmap M5

**Date:** 2026-05-28  
**State:** after M4 (T-018–T-023 complete), 205 tests passing

---

## 1. Test File Inventory

| # | Test file | Tests | Links to target | What it covers |
|---|-----------|-------|-----------------|----------------|
| 1 | `test_active_window_service.cpp` | 8 | `holonight_services` | Event/command parsing, snapshot, category mapping |
| 2 | `test_audio_device_model.cpp` | 10 | `holonight_services` | applyAdd/applyChange/applyRemove, roles, clear |
| 3 | `test_audio_service.cpp` | 5 | `holonight_services` | signal dedup, SkipInit lifecycle |
| 4 | `test_audio_state.cpp` | 10 | `holonight_core` | volume↔percent conversion, clamp, channel count |
| 5 | `test_audio_stream_model.cpp` | 10 | `holonight_services` | same as AudioDeviceModel, stream type routing |
| 6 | `test_battery_service.cpp` | 5 | `holonight_services` | state update, signal dedup, available flag |
| 7 | `test_battery_state.cpp` | 11 | `holonight_core` | batteryStateFromUpower, all UPower state values |
| 8 | `test_dbusmenu_client.cpp` | 13 | `holonight_surfaces` | makeItem parsing, DbusMenuModel roles, mnemonic stripping |
| 9 | `test_hyprland_ipc.cpp` | 22 | `holonight_platform` | All Hyprland IPC parser functions |
| 10 | `test_hyprland_ipc_client.cpp` | 8 | `holonight_platform` | Reconnect/backoff logic |
| 11 | `test_hyprland_workspace_service.cpp` | 9 | `holonight_core` | Event→model dispatch, command result parsing |
| 12 | `test_network_service.cpp` | 12 | `holonight_services` | Strength conversion, signal dedup, property mocking |
| 13 | `test_pulse_audio_backend.cpp` | 5 | `holonight_services` | Lifecycle, GMock signal wiring, idempotent start |
| 14 | `test_qml_smoke.cpp` | 1 | `holonight_app` | All QML components load without errors |
| 15 | `test_system_info.cpp` | 12 | `holonight_core` | parseOsRelease, unquoting, findSystemLogoPath |
| 16 | `test_tray_item.cpp` | 7 | `holonight_surfaces` | TrayItem snapshot construction and field access |
| 17 | `test_tray_item_properties.cpp` | 12 | `holonight_surfaces` | Accumulator pattern, icon/pixmap merge |
| 18 | `test_tray_model.cpp` | 10 | `holonight_surfaces` | Roles, slot algorithm, urgency transitions |
| 19 | `test_tray_watcher.cpp` | 7 | `holonight_surfaces` | Key parsing, registration tracking |
| 20 | `test_workspace_model.cpp` | 28 | `holonight_core` | Full WorkspaceModel state machine |

**Total:** 205 tests across 20 test files.

---

## 2. Production Files With Zero Test Coverage

These classes have no test file and contain logic not reachable from any existing test.

### 2a. Untestable without runtime (Wayland, D-Bus, PA, filesystem)

| File | Reason untestable |
|------|-------------------|
| `LayerShell.h`, `LayerSurface.{h,cpp}` | Requires live Wayland compositor |
| `LayerShellManager.{h,cpp}` | Creates `QQuickView` per screen; Wayland compositor needed |
| `PopupSurface.{h,cpp}`, `TooltipSurface.{h,cpp}`, `TrayMenuSurface.{h,cpp}` | Layer-shell overlay surfaces |
| `SessionService.{h,cpp}` | Commands: `loginctl`, `systemctl reboot/poweroff`; no injectable facade |
| `ThemeService.{h,cpp}` | All properties are `CONSTANT` string/int literals; trivial accessors |
| `SystemInfoService.{h,cpp}` | Thin wrapper over `SystemInfo`; constructor reads `/etc/os-release` |
| `ShellApplication.{h,cpp}` | Startup orchestration; wires all services together |
| `IconImageProvider.h` | Header-only Qt image provider; exercises Qt internals |

### 2b. Testable, zero coverage — **critical gaps**

| File | Untested methods | Lines/branches | Why testable |
|------|-----------------|----------------|--------------|
| `KeyboardLayoutService.{h,cpp}` | `processEventLine()`, `onCommandFinished()`, `setLayoutCode()`, idempotent `start()` | ~25 lines, 4 branches | Accepts injectable `HyprlandIpcTransportPtr` (same pattern as `HyprlandWorkspaceService`) |

`KeyboardLayoutService` is the only file with injectable transport that has zero test coverage. The parallel class `HyprlandWorkspaceService` is fully tested (9 tests) with a fake transport — the same infrastructure applies directly here.

---

## 3. Partially Covered Files With Untested Branches

| File | Tested | Not tested |
|------|--------|-----------|
| `TrayModel` | Add/remove rows, slot algorithm, urgency, image cache | `activate()`, `secondaryActivate()`, `scroll()` — dispatch to live D-Bus |
| `TrayWatcher` | Key parsing, registration | `processNameChange()` path for SNI owner changes |
| `HyprlandIpcClient` | Reconnect/backoff scheduling | Actual socket I/O (requires live Hyprland socket) |
| `ActiveWindowService` | Event parsing, snapshot, category mapping | `scanDesktopFiles()` (filesystem), `queryAllMonitorWindows()` (live IPC) |
| `keyboardLayoutCode()` | Known-code lookup, 2-letter extraction | Empty-string input path |

---

## 4. Priority Matrix for T-025

| Priority | Gap | Effort | Value |
|----------|-----|--------|-------|
| **HIGH** | `KeyboardLayoutService` — processEventLine, onCommandFinished, setLayoutCode | Low (fake transport already exists) | High (25 lines, 4 branches, zero prior coverage) |
| **MEDIUM** | `keyboardLayoutCode` empty-string edge case | Trivial (add 1 case to existing test) | Low (1 branch) |
| **LOW** | `TrayModel::activate/secondaryActivate/scroll` | High (needs D-Bus mock) | Medium |
| **LOW** | `TrayWatcher::processNameChange` | High (needs D-Bus mock) | Medium |

**T-025 focus:** `KeyboardLayoutService` tests — covers the single most valuable gap with the least infrastructure investment.
