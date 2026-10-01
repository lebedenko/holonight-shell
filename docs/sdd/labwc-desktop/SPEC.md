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
