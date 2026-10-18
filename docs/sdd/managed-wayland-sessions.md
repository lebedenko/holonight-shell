# Managed Wayland sessions

HoloNight login entries compose compositor configurations and a predictable
application environment across Hyprland, Sway and labwc. `auto` and `uwsm` require
UWSM; `direct` remains explicit manual integration. One graphical session per user
manager is supported. UWSM 0.27.1 is the initial environment-preparation baseline.

The launcher validates the compositor, helpers, user manager, logind session and
UWSM start conditions before starting services. Optional portal components produce
warnings. It composes private runtime configurations, then executes UWSM with the
actual compositor executable, preserving UWSM plugin selection. Hyprland uses
`start-hyprland -- --config`; Sway avoids distribution environment-reset snippets;
labwc uses a private directory and explicit startup command.

A POSIX-sourceable defaults file is shared by the launcher and `uwsm/env-holonight`.
It adds local-bin idempotently, sets overridable Qt/askpass defaults, resolves the
canonical cursor and extends installation search paths. Unset or empty
`XDG_DATA_DIRS` is initialized to `/usr/local/share:/usr/share` before checking for
the installation data directory.
Explicit nonempty values retain their ordering; the installation directory is
prepended only when absent, and the result is exported even when already present.
UWSM user environment files have final precedence. No interactive terminal files
are sourced. The managed hook disables Hyprland's
native systemd variable/notification/target management and labwc's native activation
imports, so those paths cannot race UWSM finalization or cleanup. The shell unit
has no fixed PATH. Managed mode leaves propagation and cleanup to UWSM; direct
mode imports PATH with the existing allowlist.

Each full desktop identity starts with `HoloNight-<compositor>`, followed by
`HoloNight` and the native desktop token. Desktop-specific portal preferences live
in the standard data directory. User/admin search precedence remains intact.
Settings selects `holonight;gtk`, FileChooser `holonight-filechooser;gtk`, and
compositor interfaces retain Hyprland/wlr. Legacy compositor-named files remain
installed for compatibility without being injected into managed environments.

Hyprland loads `$XDG_CONFIG_HOME/hypr/hyprland.lua` when present, otherwise shipped
monitor and keybinding defaults. `XDG_CONFIG_HOME` defaults to `~/.config`.
The native directory's `?.lua` and `?/init.lua` patterns precede the existing
`package.path`; tracked `require` loads the entrypoint and its modules for automatic
reload. Native startup callbacks remain registered and configuration errors remain
visible through Hyprland diagnostics without fallback. Before user settings,
`misc.disable_xdg_env_checks = true` acknowledges the managed desktop identity.
Sway and labwc retain their shipped bases. Compositor overrides follow the base
under `$XDG_CONFIG_HOME/holonight/session`.
The readiness callback follows overrides and validates WAYLAND_DISPLAY plus
HYPRLAND_INSTANCE_SIGNATURE, SWAYSOCK or LABWC_PID. It serializes finalization per
runtime configuration, exports HOLONIGHT_SESSION_READY through `uwsm finalize`,
then queues shell and session-specific Polkit units. The marker is a UWSM wait
requirement. UWSM owns bounded startup failure and session teardown. Applications
remain in UWSM application units, independent of shell restarts. Polkit discovery,
conflict handling and authentication routing are unchanged.

The read-only doctor compares an allowlist of process environments and resolves a
requested executable without running it. `/proc` exposes process startup values;
compositor-produced readiness/display values may only appear in the manager.
Missing or restricted information yields exit 1, invalid arguments exit 2. Portal
configuration is selected using the broker environment when available; provider
ownership is reported separately. The search assumes the standard portal build
locations `/etc` and `/usr/share`; custom-built brokers need manual verification.

All assets use prefix-aware CMake installation and manifest ownership. No personal
configuration is migrated and no running desktop is restarted. Apply on next login.

## Acceptance

Focused tests cover PATH ordering/idempotence, absent local-bin, prefixes with
spaces, direct startup, managed preflight, marker validation, runtime overrides,
portal precedence and diagnostic failures. The actual installed UWSM preloader is
exercised when available, including its compositor plugins and user env precedence.
Installation tests stage assets with DESTDIR and execute from a disposable real
prefix; uninstall retains the existing manifest and modified-file protections.
Installed defaults are tested under POSIX sh and Bash with unset-variable checks
for `/usr`, `/usr/local` and a prefix containing spaces. Unset, empty and explicit
data paths cover child-process exports, ordering and idempotence on repeated sourcing.

Run `task test` and shell/umbrella `task check:integration`. Containers are acceptance
only; deployed binaries must use host dependencies. In a disposable VM or dedicated
test user with systemd, for each compositor:

1. Log in cleanly with `XDG_DATA_DIRS` initially unset, confirm UWSM starts and the
   shell appears, check the full identity and doctor output, and resolve a local
   executable through launcher, shortcut and D-Bus activation.
2. Delay finalization and confirm graphical-session activation waits. Omit the
   compositor marker and confirm UWSM timeout cleans up without starting the shell.
3. Restart the shell and confirm applications survive. Log out and confirm managed
   applications stop; log in again and confirm no stale readiness/display state.
4. Exercise FileChooser, Settings and screen sharing with backends installed, then
   confirm missing backends warn and user/admin overrides win.

These live checks must never run against the developer's current user manager.

### VM verification, 2026-10-18

A disposable Arch VM exercised real PAM/logind sessions on VT3 with UWSM 0.26.7,
Hyprland 0.56.2, Sway 1.12 and labwc 0.20.2. All three preserved the full desktop
identity and local-bin PATH, passed launcher-service/shortcut/D-Bus child executable
resolution, kept managed applications alive across shell restarts, and stopped them
on logout. Subsequent logins worked. Five-second finalization delays held back the
graphical target; missing compositor markers triggered UWSM's 30-second timeout
without starting the shell. Cleanup removed the readiness marker.

Settings reads matched HoloNight's provider. Native-built FileChooser dependencies
in a private prefix supported dialog cancellation and successful save-URI selection
on all three desktops. ScreenCast returned streams and PipeWire remote connections
through wlr on Sway/labwc and the Hyprland backend on Hyprland. Capture tests used
deterministic source selection (wlr's disabled chooser and Hyprland's custom picker).
Hyprland's interactive picker opened, but automated selection did not complete;
manual picker/browser capture acceptance remains outstanding. These API checks do
not establish that a consumer received video frames.

The live doctor check exposed a false stale-environment warning: compositor `/proc`
environments need not contain display/readiness values created after startup. The
doctor now excludes those values only for the compositor and still detects stale
client display values. A regression test covers both cases. Sway restricts access
to its process environment in this VM; the doctor correctly reports that check as
incomplete instead of claiming success. Logs and disposable test harnesses remain
under the ignored umbrella `build/session-acceptance/` directory.
