# Agent Window Activation — Design

**Initiative:** `agent-notification-window-activation`
**Work package:** `ANWA-101`
**Baseline:** `b48574d39a83943cb7c6fa012ed0066bb8120399`

## Component boundary

`WindowActivationServer` lives in `holonight-services` as a thin `QDBusAbstractAdaptor`-backed session-bus adapter. It
validates D-Bus-shaped input and delegates to `CompositorService`; it does not parse compositor data, launch commands,
or retain target inventories. `ShellApplication` owns and starts it beside the existing shell D-Bus servers.

```text
D-Bus caller
  -> WindowActivationServer
  -> CompositorService::requestWindowActivation(lineage, titleHint)
  -> CompositorBackend::requestWindowActivation(...)
  -> HyprlandBackend | SwayBackend | GenericBackend
```

The public compositor snapshot capabilities gain a distinct `window_activation` bit. The service returns a small
accepted/rejected result suitable for the D-Bus boolean while backends emit bounded diagnostics through the existing
snapshot/diagnostic path.

## Shared candidate resolver

A compositor-private inventory is converted to internal `WindowActivationCandidate` values containing PID, exact
title, and an opaque backend target. A pure resolver:

1. validates and de-duplicates the lineage while preserving order;
2. groups candidates by their earliest matching lineage index;
3. takes only the first non-empty group;
4. selects its sole entry, or applies exact-title equality to that group and selects only a sole survivor.

The opaque target is never exposed through D-Bus or QML. The resolver returns missing and ambiguous outcomes
separately for diagnostics, but both map to `false` publicly.

## Hyprland backend

The existing serialized `HyprlandIpcTransport` command path remains the only IPC writer.

- Extend `j/clients` parsing with the positive `pid`, existing `title`, and existing client `address`.
- Keep a private current inventory of `{pid, title, address}` after a valid client refresh.
- Once a unique candidate is resolved, enqueue `dispatch focuswindow address:<address>` on the command socket and
  fall back to `dispatch hl.dsp.focus({ window = "address:<address>" })` when current Lua-based Hyprland rejects the
  legacy dispatcher syntax.
- Treat an unavailable transport, an in-flight command plus an occupied one-entry activation queue, a rejected
  `runCommand`, or an `error:` response as failure/diagnostic state.
- Refresh after completion, but do not re-resolve the accepted request against a later inventory.

Workspace and window commands share the existing serialized command state machine. Add explicit command phases and
separate bounded pending slots so refresh traffic cannot interleave frames and an accepted activation is never
silently overwritten.

## Sway backend

Extend the recursive `GET_TREE` traversal to build a private inventory for window containers with positive `pid`, full
`name` title, and numeric container `id`. Nested `nodes` and `floating_nodes` are both traversed.

After resolving a unique candidate, send a Sway `COMMAND` payload of `[con_id=<decimal-id>] focus` through the existing
request socket. The request is accepted only when connected and either sent immediately or placed in the one-entry
activation queue. The asynchronous command reply must be a non-empty success array; otherwise publish a diagnostic.
Disconnect clears inventory and pending requests so stale container IDs cannot be used after reconnect.

## Generic backend

`GenericBackend` reports `window_activation = false` and rejects every request without probing protocols or launching
helpers. Generic Wayland has no activation implementation in this work package.

## D-Bus lifecycle and diagnostics

`WindowActivationServer::start()` owns `org.holonight.Shell` and exports `/org/holonight/Shell` with interface
`org.holonight.Shell.WindowActivation1` on the session bus. Registration failure is logged and leaves the provider
unavailable. The method performs only
bounded validation and in-memory resolution before queuing existing non-blocking compositor IPC; it does not enter a
nested event loop.

Diagnostics identify invalid input, unsupported/disconnected backend, missing target, ambiguous target, queue full,
send failure, or compositor rejection without logging titles or full process lineages at normal severity.

## Test seams

- Pure resolver tests use synthetic candidates and lineages.
- Hyprland tests use the existing fake `HyprlandIpcTransport` to assert parsing, serialized queue behavior, and the
  exact focus command.
- Sway decoder/socket tests assert recursive inventory parsing, container selection, exact command payload, replies,
  and disconnect cleanup.
- Compositor service tests use a fake backend for capability and result forwarding.
- D-Bus adapter tests invoke the exported method without a real compositor and verify validation/result mapping.
- Application composition and QML metadata tests confirm the new server and capability do not break startup or
  generated singleton metadata.

Live compositor focus policy and observation of the actual focused window remain manual verification boundaries.
