# Application launching

Launcher entries and desktop actions, storage Files actions, and the network editor use ApplicationLaunchService. Requests complete asynchronously; launch acceptance does not imply startup succeeded. Errors keep the launcher open and do not update recent applications.

An active `wayland-wm@*.service` selects `uwsm app -t service --`. Otherwise the systemd user manager starts a uniquely named `app-holonight-*.service` using `Type=exec` in `app.slice`. Native services receive the current environment (excluding service control variables), working directory, and journal output. An active `graphical-session.target` supplies logout ordering and lifetime through `After` and `PartOf`. Without a user manager, ordinary sessions use detached processes. A shell service refuses detached launching when manager access fails. Managed launch errors never trigger a detached retry.

Startup has a 15-second timeout. Native units are stopped after timeout; UWSM timeout reports an uncertain outcome. Keep the shell unit's `KillMode=control-group`.

For compositor shortcuts in an UWSM session, use wrappers such as:

```ini
# Hyprland
bind = SUPER, E, exec, uwsm app -t service -- holonight-files
bind = SUPER, RETURN, exec, uwsm app -t service -- foot
```

```text
# Sway
bindsym $mod+e exec uwsm app -t service -- holonight-files
bindsym $mod+Return exec uwsm app -t service -- foot
```

These examples follow [UWSM's application launching interface](https://github.com/Vladimir-csp/uwsm#application-launching). No desktop files or personal compositor configuration are rewritten. One graphical session per user manager is supported.

Manual integration verification requires disposable sessions: launch through each caller, inspect application unit cgroups, restart only the disposable shell, verify applications survive, then verify managed logout stops them. Also check detached launches in a session without a user manager. Do not restart a live user shell for automated validation.
