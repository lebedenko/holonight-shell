# Sway compositor support design

## Architecture

`holonight_compositor` contains the public service/model/snapshot types and
private Hyprland, Sway, and ext-workspace backends. A backend emits only a
complete `CompositorSnapshot`. `CompositorService::publishSnapshot` validates
and normalizes it, replaces the model, swaps output maps, then emits one
revision notification. This makes a revision the consistency boundary.

Backend selection is a pure function tested independently from sockets. A
known declaration deliberately survives missing IPC so diagnostics describe
the declared compositor instead of silently exposing weaker generic behavior.

Sway framing is separated from socket orchestration. The incremental decoder
retains partial headers and payloads, validates magic and the 8 MiB bound before
allocation, and returns typed frames. Request sequencing validates each reply;
the subscription connection accepts only event frames after a successful
subscribe response. One zero-delay refresh timer coalesces event bursts.

Workspace presentation is derived from snapshot facts. Opaque ID is the only
activation key. Numeric slot is optional and never reconstructed from an
arbitrary Sway name. Visual state precedence is focused, urgent, active,
occupied, empty; unknown optional facts remain unknown rather than false.

Shell surface managers translate their existing policies into specs. The
provider owns Qt private/native Wayland access and protocol objects. Image
providers are installed through `before_load`; properties are supplied in
`initial_properties`; configure-driven mutations use host setters.

## Failure handling

Malformed IPC disconnects the affected Sway connection and records a bounded
diagnostic. A lost subscription socket schedules reconnect; an in-flight
request failure discards the incomplete refresh. No partial snapshot is
published. Backend/global loss invalidates capability-dependent information.

Closing a transient surface from QML is queued by its manager. Provider access
is always guarded because `view()`, `rootObject()`, and `engine()` return null
after terminal state.

## Verification strategy

Pure tests cover selection, snapshot projection, framing, JSON/tree parsing,
escaping, and session command selection. Fake sockets cover partial reads,
wrong types, disconnects, reconnect backoff, and event coalescing. Surface
policy tests compare complete generated specs and lifecycle behavior. Shell
checks reject local layer-shell protocol ownership.

The automated compositor smoke test starts Sway with a temporary runtime and
configuration, starts Shell, exercises named workspace activation over IPC,
and requests a clean exit while preserving logs on failure.

Pointer/focus-dependent visual checks are deferred to SWS-201. The user must
verify bars, backgrounds, widgets, launcher, sidebar, popups, tray, toasts and
OSD on Hyprland and Sway; named workspace activation on Sway; generic hiding of
unsupported data; both installed greeter entries; and close/reopen plus output
hotplug lifecycle behavior.

