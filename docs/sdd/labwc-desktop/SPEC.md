# Labwc desktop integration

## Product contract

Extend the existing labwc plugin. Preserve named workspaces and active-window projections.
Every committed foreign toplevel is listed, including minimized windows and windows without output membership.
IDs are opaque, unique for a handle lifetime, and never reused across reconnections.
A request is accepted for dispatch, never a promise that compositor state changed.

## Boundaries

QML → presentation models → WindowCatalog application rules → integration contracts ← labwc adapter.
Protocol objects and seats remain in integrations/labwc. Icons come from LauncherService metadata.
WindowCatalog owns lookup, validation, grouping keys, search and observed activation history.
Other plugins advertise no window inventory until separately implemented.

## Capability matrix

| Feature | Labwc | Other backends |
| --- | --- | --- |
| Inventory, activate, minimize, maximize, close | foreign-toplevel v1 | unchanged |
| Fullscreen | foreign-toplevel v2+ | unchanged |
| Parent event | foreign-toplevel v3 | unchanged |
| Geometry, workspace membership, thumbnails | unavailable | not exposed |

## Ordered delivery gates

1. Contracts and isolation: neutral records and commands, application rules and models, no other-backend changes.
2. Adapter: version negotiation, done-only commit, immediate closure invalidation, independent workspace availability.
3. Taskbar: grouped by app by default, individual unnamed windows, all outputs, bounded overflow.
4. Window actions: negotiated operations, stale-target validation, transient dismissal.
5. Overview: search, frozen activation ordering, keyboard navigation, safe closure, optional Super+Tab.
6. Configuration: existing shell-config provider and Settings consumer, opt-in desktop ownership.

## Deferred

Workspace filtering/moves, geometry-dependent actions and thumbnails require authoritative APIs.
Native Alt+Tab and desktop menu remain defaults. Show-desktop stays compositor-owned.
Do not rewrite user labwc configuration. Publish provider revisions before changing consumer pins.

## Verification

Unit tests cover atomic state, flags, rejection, grouping/search/history and model visibility.
Isolated labwc smoke must verify inventory, commands, closure, restart and plugin isolation.
Shared-contract/surface changes require Sway and live Hyprland checks.
The supplied shell-impl.md and moc.png references are absent from this checkout.

## Local implementation and release status

The contract, catalog, window models, version-negotiating adapter, grouped taskbar,
window actions, overview control command, shell-config provider and Settings draft are implemented locally.
Taskbar modes and overview access are configurable under `[bar.taskbar]` using `enabled`,
`grouped`, `overview_access` and `desktop_menu`. Desktop menu defaults off. Its current working
entries are Applications, Settings and Lock screen; unavailable wallpaper/widget editors are omitted.
All taskbar rows cover all advertised outputs and workspaces. Overflow remains accessible as a
chooser when searchable overview access is disabled.

Private Shell/provider version is 0.2.0; the private plugin IID is 2.0. Rebuild all private providers
and consumers together. Settings requires the new package version for its local draft.
Provider publication, consumer revision pins and umbrella submodule commits remain pending until
release acceptance gates pass, including a live Hyprland session. Do not publish the consumer first.

The optional binding fragment is in `labwc-bindings.xml`. Show-desktop uses the compositor's
[ToggleShowDesktop action](https://labwc.github.io/labwc-actions.5.html), preserving its restoration rules.
No installed compositor configuration or bindings are rewritten.

Validation recorded during implementation:

- Shell `task test`: 1,212 tests passed; Settings `task test`: 55 tests passed.
- Architecture boundaries, formatting, QML lint and QML metadata checks passed.
- Isolated labwc 0.20.2 smoke: real workspace/window inventory, all supported commands,
  stale IDs, independent plugin loading, overview keyboard input, shell restart passed.
  Real clients negotiated v1, v2 and v3 and verified capability-gated commands.
  Two-output overview, output removal, reopening and activation on the remaining output passed.
- Headless Sway runtime smoke passed. Live Hyprland is unavailable in the current labwc session.
- QML behavior checks passed for search, empty results, keyboard navigation, grouping,
  bounded task area, selection closure and stale-menu dismissal.
- Live Hyprland and manual visual/menu acceptance remain release gates.
- The overview screenshot is generated under `build-tests/smoke-artifacts/labwc-overview.png`.

## Optional direct workspace shortcuts

The named workspace section hides when fewer than two actual workspaces are listed.
`[bar.workspaces].count` limits how many labels the shell displays; it does not create desktops.
Numbered providers retain their shortcut slots, including slots without an actual workspace.
Workspace occupancy and shell-driven window movement remain deferred for labwc.

For an optional five-desktop setup, add or update this element inside your existing
`<labwc_config>` in `~/.config/labwc/rc.xml`:

```xml
<desktops number="5" />
```

Add the following bindings inside the existing `<keyboard>` element, preserving your
other shortcuts (replace any conflicting bindings). `W` means Super and `S` means Shift.

```xml
<keybind key="W-1"><action name="GoToDesktop" to="1" /></keybind>
<keybind key="W-2"><action name="GoToDesktop" to="2" /></keybind>
<keybind key="W-3"><action name="GoToDesktop" to="3" /></keybind>
<keybind key="W-4"><action name="GoToDesktop" to="4" /></keybind>
<keybind key="W-5"><action name="GoToDesktop" to="5" /></keybind>
<keybind key="W-S-1"><action name="SendToDesktop" to="1" follow="no" /></keybind>
<keybind key="W-S-2"><action name="SendToDesktop" to="2" follow="no" /></keybind>
<keybind key="W-S-3"><action name="SendToDesktop" to="3" follow="no" /></keybind>
<keybind key="W-S-4"><action name="SendToDesktop" to="4" follow="no" /></keybind>
<keybind key="W-S-5"><action name="SendToDesktop" to="5" follow="no" /></keybind>
```

Apply changes with `labwc --reconfigure`. Super+5 jumps directly from workspace 1 to
workspace 5. Super+Shift+5 sends the focused window directly to workspace 5 while you
stay on the current workspace. Labwc switches workspaces across all outputs.
These are native compositor actions; no shell command, IPC extension or sequential
navigation workaround is needed. See the [labwc action reference](https://labwc.github.io/labwc-actions.5.html).
Five desktops are an example, not a shell default; the shell does not change personal configuration.

For live verification, first use a single desktop and confirm the workspace section
leaves no frame, gap or clickable surface. Then apply the optional five-desktop setup,
check names and active state, jump from 1 to 5 with Super+5, return to 1 and send a
focused window with Super+Shift+5. Confirm you remain on 1 and the window appears on 5.
