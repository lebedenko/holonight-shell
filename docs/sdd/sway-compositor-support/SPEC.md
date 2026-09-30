# Sway compositor support specification

Status: Accepted

## Scope

The Shell supports Hyprland 0.56.2, Sway 1.12, and compositors exposing
`ext-workspace-v1`. Compositor identity is selected once during application
construction. The selected backend owns compositor IPC and publishes complete
snapshots through `CompositorService`. Only a plugin’s private contribution receives
its instance-owned backend model. Common QML consumes neutral contracts.

## Public contract

`CompositorService` exposes `connected`,
`diagnostic`, `revision`, `focusedOutput`, `workspaces`, capability flags,
output-scoped active-window fields, `isOutputEmpty(output)`, and
`activateWorkspace(id)`. Workspace IDs are opaque strings. Model roles are:
`workspaceId`, `displayName`, `stableOrder`, `groups`, `canActivate`,
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
the selected identity. Desktop tokens and markers are declared in plugin metadata,
not shared service branches. A missing or incompatible plugin logs a diagnostic
and uses the metadata-designated fallback when available. If none can load,
independent shell services continue with compositor features unavailable.

## Capability truth table

| Capability | Hyprland | Sway | Generic ext-workspace |
|---|---:|---:|---:|
| list workspaces | yes | yes | protocol present |
| activate existing workspace | yes | yes | per-workspace capability |
| optional numbered provider | yes | yes | absent |
| private topbar contribution | special-workspace dots | absent | absent |
| active-window data | yes | yes | no |
| focused output | yes | yes | no |
| urgency | yes | yes | protocol present |
| occupancy | yes | yes | no |

`WorkspacePresentation.useNumericWorkspacePresentation` comes from the optional
`NumberedWorkspaceProvider`, independently of common snapshot capabilities.
Hyprland retains its numeric strip. Sway automatically uses the same moving numeric
window and arrows when every reported workspace has a canonical positive integer
name matching its numeric slot, with no duplicate slots. An empty connected
snapshot also qualifies. Named workspaces, number-prefixed names such as `2:web`,
zero, and ambiguous numbering use the existing-workspace list, where the configured
count remains a maximum visible list size. Generic compositors always use the list.

Numeric presentation shows the configured number of display slots, including empty
slots; displaying a slot does not create a compositor workspace. Clicking an empty
slot creates it. Sway activates canonical positive numeric targets with
`workspace --no-auto-back-and-forth number N`, reusing an existing numbered workspace
rather than creating a duplicate if it has since been renamed. Other targets retain
escaped exact-name activation. Changing the count updates display slots immediately;
actual snapshots never receive synthetic entries. Special workspaces are private
Hyprland state and are excluded from all common workspace snapshots. The generic protocol reports workspace output membership, but does
not identify the keyboard-focused output; placement therefore falls back to the
primary screen. Unknown occupancy keeps desktop widgets unmapped.

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
