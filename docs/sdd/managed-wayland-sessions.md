# Managed Wayland sessions

HoloNight login entries select native compositor configurations and a predictable
application environment across Hyprland, Sway and labwc. `auto` and `uwsm` require
UWSM; `direct` remains explicit manual integration. One graphical session per user
manager is supported. UWSM 0.27.1 is the initial environment-preparation baseline.

The launcher validates the compositor, helpers, user manager, logind session and
UWSM start conditions before starting services. Optional portal components produce
warnings. It composes private runtime configurations, then executes UWSM with the
actual compositor executable, preserving UWSM plugin selection. Hyprland uses
`start-hyprland -- --config`; Sway avoids distribution environment-reset snippets;
labwc uses a persistent configuration directory and explicit startup command.

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
Sway selects the first existing personal entrypoint in native order: `~/.sway/config`,
`$XDG_CONFIG_HOME/sway/config`, `~/.i3/config`, `$XDG_CONFIG_HOME/i3/config`. A small
runtime wrapper includes that file by reference, preserving relative includes and
reloads, and appends an `exec` readiness command (never `exec_always`). Shipped
bindings apply only when no personal entrypoint exists.

labwc uses `$XDG_CONFIG_HOME/labwc` directly when `rc.xml` exists; otherwise it uses
the installed `holonight/session/labwc` fallback directory. It does not copy or merge
XML, menus, or theme overrides. Native `labwc --reconfigure` sees atomic replacements
and menu/theme changes. Entrypoint selection changes take effect at the next login.
`HOLONIGHT_LABWC_CONFIG_DIR` identifies the selected persistent directory to Appearance;
finalization locks remain in a separate private runtime directory. HoloNight theme
selection remains opt-in. A read-only fallback cannot be edited by Appearance: create
a personal configuration and log in again before synchronizing its title fonts.

Before startup, existing files in `$XDG_CONFIG_HOME/holonight/session` require explicit
migration. Merge desired settings into native configuration and move legacy overrides
out of that directory. The launcher never edits, deletes, or migrates personal files.
Read-only preflight reports file/line diagnostics for duplicate Shell/Polkit startup,
competing UWSM startup/finalization, activation environment imports/resets, and managed
environment assignments. It follows literal Lua imports, Sway include globs, and
literal startup script references. Dynamic Lua and shell programs remain unchecked;
this is a known-conflict check, not a sandbox or exhaustive program analysis.
Ordinary application startup is allowed; use `uwsm app -- APPLICATION` for session
lifetime management and `uwsm stop` in user-defined logout bindings.

The readiness callback validates WAYLAND_DISPLAY plus HYPRLAND_INSTANCE_SIGNATURE,
SWAYSOCK or LABWC_PID. It serializes finalization per login, exports
HOLONIGHT_SESSION_READY through `uwsm finalize`, then queues Shell and session-specific
Polkit units. UWSM owns startup timeout, activation, and teardown. Applications remain
in UWSM application units, independent of Shell restarts.

The private `holonight-session-state` protocol uses `busctl --json=short`. Its Python
module decodes typed properties and emits only allowlisted NUL-separated environment
records. Wrappers never evaluate text; empty values and absent values stay distinct.
Logout discovers executable-based and legacy desktop-entry UWSM instances. All three
backends use `uwsm stop` for verified managed sessions, native logout for verified
unmanaged sessions, and an actionable error for unverifiable state or missing UWSM.

Managed Polkit discovery validates the active logind owner/session, a unique active
UWSM compositor unit, and the live PID named by `wayland-session-bindpid@PID.service`
inside that session's scope. UWSM intentionally excludes XDG_SESSION_ID from manager
environment; the login association comes from logind's GetSessionByPID. Any present
conflicting manager identity is rejected. The helper connects to the manager-selected
Wayland socket, checks SO_PEERCRED and unit membership, and rechecks session/process
identity before returning the allowlisted manager environment. It never reads the
managed compositor's `/proc/environ`, including capability-bearing Sway. Direct peer
discovery and the existing agent conflict handling remain available.

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
spaces, direct startup, managed preflight, marker validation, migration rejection, native selection,
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

### Historical VM verification, 2026-10-18 (before native-configuration revision)

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

### Native-configuration revision verification

Host dependency builds passed for Shell and Appearance. The focused Shell CTest
selection passed 6/6 suites; logout/lock cases passed 27/27. Appearance labwc tests
passed 2/2. Umbrella installer tests passed 87 cases (one skipped). The pinned Shell
formatter completed successfully. Focused script, real Unix
socket, structured environment, migration, configuration selection, and installed
prefix tests passed. Appearance apply/status/revert tests use the managed persistent
configuration variable, retaining menu edits, font/theme ownership, rollback and
recovery checks. C++ logout tests cover direct, managed, unavailable and unverifiable
states. These automated checks do not establish real authentication or video delivery.

### Native-configuration VM acceptance, 2026-10-18

The supplied hn-arch VM (192.168.122.29) completed this revision's acceptance with
UWSM 0.26.7, Hyprland 0.56.2, Sway 1.12 and labwc 0.20.2. Dependencies and binaries
were built natively against that VM's Qt 6.11.2/system libraries in a private prefix;
no container binary was deployed. A separate `hnmanaged` account used real PAM/logind
sessions on VT3, preserving the existing user's configurations and the host desktop.

All three compositors passed:

- Actual Polkit password authentication through HoloNight, creating root-owned test
  markers. Installed Sway retained `cap_sys_nice=ep`; managed discovery succeeded
  while its compositor environment was unreadable.
- Managed application PID survival across Shell restart, followed by Shell UI logout.
  Journal ordering showed applications, Shell and Polkit stopping before the compositor.
- Native configuration reload, repeat login, and installed fallback startup after
  temporarily moving the test account's native entrypoint out of the discovery path.
- HoloNight FileChooser request/close and Settings color-scheme routing.
- Firefox `getDisplayMedia` permission acceptance, real interactive monitor picker
  selection (wlr for Sway/labwc, Hyprland's Qt picker), and delivered video frames at
  1280×800. Observed minima were 60 frames on Sway, 45 on labwc and 60 on Hyprland.
  Picker acceptance used virtual keyboard/mouse input in the VM; it was not bypassed
  or replaced with a deterministic portal response.

Sway's relative include gained a working F9 binding after native reload without
restarting Shell. Hyprland's tracked Lua import changed `gaps_in` from 5 to 7 on
reload. The executable UWSM instance `wayland-wm@start\x2dhyprland.service` was
verified successfully. Its native Lua exit dispatcher was also exercised after the
final rebuilt fallback login; direct logout uses `hl.dsp.exit()`.

labwc Appearance apply/status/revert targeted the selected persistent directory.
Live checks covered title fonts, theme/accent changes, conflicting ownership in
another directory, atomic replacement of `rc.xml`, a working native keybinding,
and a menu edit visible after ordinary reconfigure and retained after revert.
The original title font was restored. The existing user's Sway configuration
containing a competing `uwsm finalize` was rejected read-only with file/line guidance.

The private-prefix FileChooser fixture needed a user D-Bus activation-file link
because its user bus predated the added data search path. VM Appearance builds
omitted the unrelated optional KDE adapter because KF6Config development files
were absent; host builds and focused labwc tests remained valid.

Logs, capture counters, screenshots, native build logs and disposable harnesses are
retained under the umbrella's ignored `build/managed-session-acceptance/` directory.
The temporary account's sessions and lingering manager were stopped, its credential
removed and account locked, and the VM returned to the greeter VT. No publication,
umbrella pin update or installation into the active host desktop was performed.
