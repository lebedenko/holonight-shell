# Sway compositor support design

## Architecture

`holonight_integration` defines compositor snapshots, workspace/window operations,
the optional numbered-workspace and keyboard providers, and session contracts.
`holonight_compositor` contains only shared adapters, presentation, and discovery.
Concrete code lives in three runtime Qt plugins under `integrations/`. Each owns
its transports, parsers, session adapter, and private resources.

`IntegrationLoader` reads metadata without loading code. Desktop declarations take
precedence over environment markers; ambiguous declarations or markers select the
metadata-designated fallback. Sidecar catalogs retain selection information if a
library is missing. Missing/incompatible binaries produce diagnostics and try the
fallback. Without it, compositor features stay unavailable and independent shell
services still start. Selection happens once before QML engine creation. IPC loss
clears state and reconnects inside the selected integration, without switching it.

Plugins are installed in `${CMAKE_INSTALL_LIBDIR}/holonight/backends`. Discovery
uses executable-relative paths for builds and relocated installations. Plugin
metadata and the shared library SONAME use the shell version; this is a private
ABI, with no hot swapping or separately versioned third-party compatibility.

Sway framing is separated from socket orchestration. The incremental decoder
retains partial headers and payloads, validates magic and the 8 MiB bound before
allocation, and returns typed frames. Request sequencing validates each reply;
the subscription connection accepts only event frames after a successful
subscribe response. One zero-delay refresh timer coalesces event bursts.

`CompositorService` publishes only actual workspace snapshots atomically. IDs are
opaque and resolve inside the integration. Existing-workspace activation checks
both snapshot and per-workspace capabilities. No special-workspace or numbered
presentation state belongs to the common snapshot.

`NumberedWorkspaceProvider` exposes eligibility and ID-to-slot mappings separately.
Sway checks canonical positive names and rejects duplicate numbers in its parser;
Hyprland retains numbered presentation. `WorkspacePresentation` combines this data
with actual snapshots, configured count, and viewport offset. Its display entries
can be empty; they never enter the actual model. Numbered activation goes directly
to the optional provider, which owns creation/reuse semantics and command escaping.

`TopbarContributionHost` loads only the selected integration's component with an
instance-owned model and monitor context. Hyprland's private QML module supplies
special-workspace dots immediately before the common strip. It imports neutral
presentation primitives and Holonight controls, with no shell singleton dependency.
Other integrations supply no contribution. The keyboard service similarly adapts
an optional provider; only Hyprland supplies one, so unsupported sessions produce
neither layout UI nor layout OSD events. Common locking, power, logind, and command
execution remain shared; compositor logout comes from the selected plugin.

The generic ext-workspace backend projects each workspace's output membership,
using protocol IDs or handle-lifetime fallback identities, filters hidden workspaces,
and respects changing per-workspace activation capabilities. Duplicate or mutable
names are display labels only. It never derives keyboard focus from the protocol's per-output active state.
Consequently it leaves `focusedOutput` empty and `hasFocusedOutput` false, and
screen-targeted consumers use the primary-screen fallback.

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
configuration, starts Shell, exercises numeric and named workspace activation over
IPC, checks repeated activation with auto-back-and-forth enabled, reuses renamed
numbered workspaces, and verifies empty workspace removal. It requests a clean
exit while preserving logs on failure. Set `SWAY_SMOKE_SCREENSHOT_DIR` to capture
startup, switching, empty-workspace removal, and named-mode screenshots with `grim`.

Pointer/focus-dependent visual checks are deferred to SWS-201. The user must
verify bars, backgrounds, widgets, launcher, sidebar, popups, tray, toasts and
OSD on Hyprland and Sway; named workspace activation on Sway; generic hiding of
unsupported data; both installed greeter entries; and close/reopen plus output
hotplug lifecycle behavior.
