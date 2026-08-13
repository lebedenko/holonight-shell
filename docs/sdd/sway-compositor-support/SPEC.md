# Sway compositor support specification

Status: Accepted

## Scope

The Shell supports Hyprland 0.56.2, Sway 1.12, and compositors exposing
`ext-workspace-v1`. Compositor identity is selected once during application
construction. The selected backend owns compositor IPC and publishes complete
snapshots through `CompositorService`; QML never consumes backend objects.

## Public contract

`CompositorService` exposes `backendKind`, `backendName`, `connected`,
`diagnostic`, `revision`, `focusedOutput`, `workspaces`, capability flags,
output-scoped active-window fields, `isOutputEmpty(output)`, and
`activateWorkspace(id)`. Workspace IDs are opaque strings. Model roles are:
`workspaceId`, `numericSlot`, `displayName`, `stableOrder`, `workspaceKind`,
`outputs`, `active`, `focused`, `urgent`, `occupied`, and `visualState`.

Each refresh replaces workspace, output, focus, urgency, occupancy, and active
window state in one model transaction before `revisionChanged` is emitted.
Consumers must gate optional state and actions on capabilities.

## Selection

Desktop tokens are the colon-separated, case-insensitive values in
`XDG_CURRENT_DESKTOP`. Exactly one known token selects that backend. If the
desktop declaration contains both known tokens it is ambiguous and selects
generic. Only when no known desktop token exists are runtime markers examined:
exactly one of non-empty `HYPRLAND_INSTANCE_SIGNATURE` and `SWAYSOCK` selects
its backend; both or neither select generic. Socket availability never changes
the selected identity.

## Capability truth table

| Capability | Hyprland | Sway | Generic ext-workspace |
|---|---:|---:|---:|
| list/activate workspaces | yes | yes | protocol present |
| create numeric slots | yes | no | no |
| special workspaces | yes | no | no |
| active-window data | yes | yes | no |
| focused output | yes | yes | protocol present |
| urgency | yes | yes | protocol present |
| occupancy | yes | yes | no |

Configured empty numeric slots are synthesized only when numeric creation is
supported. Unknown occupancy keeps desktop widgets unmapped.

## IPC constraints

Hyprland refreshes monitors, workspaces, and clients as one snapshot while
retaining event-socket reconnect and safe dispatch behavior. Sway uses two Unix
sockets and native-endian i3 IPC frames (`i3-ipc`, 32-bit length, 32-bit type),
rejects payloads over 8 MiB and mismatched response types, subscribes to
workspace/window/output/shutdown events, and coalesces them into full workspace,
output, and tree refreshes. Reconnect delay is bounded. Sway command names are
escaped for quoted command arguments and `__i3_scratch` is excluded.

## Surface and session behavior

All layer surfaces are described by `Holonight::Wayland::LayerSurfaceSpec` and
owned by `LayerSurfaceHost`. Persistent surfaces stay mapped and change only
their QML root visibility. Transient surfaces close the host and construct a
fresh host on reopen. Host failure, compositor close, output removal, and
global loss converge on manager teardown. No consumer hides a live-role
`QQuickView` or commits a raw `wl_surface`.

Installed descriptors invoke `holonight-session hyprland` and
`holonight-session sway`. `HOLONIGHT_SESSION_MODE` accepts `auto`, `uwsm`, and
`direct`. The launcher clears variables belonging to the other compositor,
imports shared appearance/session variables, and starts the matching direct or
UWSM desktop. The systemd wrapper waits for `WAYLAND_DISPLAY` and the selected
runtime marker. Logout uses `uwsm stop` for UWSM, otherwise `hyprctl dispatch
exit` or `swaymsg exit`; generic logout is unsupported.

Portal routing installs Hyprland, Sway (`wlr;gtk`), and generic HoloNight
configurations. The settings portal advertises both desktops.

