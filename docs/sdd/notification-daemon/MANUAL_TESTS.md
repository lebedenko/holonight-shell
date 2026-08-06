# Notification Daemon — Manual Integration Test Plan (REQ-NF-042)

Live Wayland session required. Launch the shell first:

```bash
task run            # or: ./build/holonight-shell
```

In a second terminal, optionally watch D-Bus traffic for the whole session:

```bash
dbus-monitor "interface='org.freedesktop.Notifications'"
```

Record each case as **PASS** / **FAIL** in the result column.

> Note: if another notification daemon (mako, dunst, swaync, GNOME) is running it will
> own `org.freedesktop.Notifications` first and holonight-shell's server will fail to
> register. Stop it before testing. Verify ownership with:
> `busctl --user list | grep Notifications`.

---

## T-020 — Basic notification + introspection

| # | Command | Expected | Result |
|---|---------|----------|--------|
| 20.1 | `notify-send "Hello" "World"` | Cyan-accent toast appears top-right of focused monitor with summary "Hello", body "World" | |
| 20.2 | `busctl --user introspect org.freedesktop.Notifications /org/freedesktop/Notifications` | Interface `org.freedesktop.Notifications` lists 4 methods (Notify, CloseNotification, GetCapabilities, GetServerInformation) and 2 signals (NotificationClosed, ActionInvoked) | |
| 20.3 | `busctl --user call org.freedesktop.Notifications /org/freedesktop/Notifications org.freedesktop.Notifications GetServerInformation` | Returns 4 strings; spec_version = "1.2" | |
| 20.4 | `busctl --user call org.freedesktop.Notifications /org/freedesktop/Notifications org.freedesktop.Notifications GetCapabilities` | Returns exactly `["body","body-markup","actions","icon-static"]` | |
| 20.5 | Let toast time out (or dismiss). Watch `dbus-monitor`. | `NotificationClosed(id, 1)` on timeout, or `(id, 2)` on dismiss | |

## T-021 — Actions, default action, body click

| # | Command | Expected | Result |
|---|---------|----------|--------|
| 21.1 | `notify-send -t 0 --action "reply=Reply" --action "ignore=Ignore" "Msg" "Two actions"` | Toast shows two outlined HUD buttons "Reply" and "Ignore" | |
| 21.2 | Click the "Reply" button | `dbus-monitor` shows `ActionInvoked(id, "reply")`; toast closes with `NotificationClosed(id, 3)` | |
| 21.3 | `notify-send -t 0 --action "default=Open" "Default" "Click body"` then click body text | `ActionInvoked(id, "default")` emitted | |
| 21.4 | `notify-send -t 0 "Plain" "No default"` then click body | Toast dismissed with `NotificationClosed(id, 2)` (no ActionInvoked) | |
| 21.5 | `notify-send -t 0 --hint boolean:resident:true --action "snooze=Snooze" "Resident" "Stays"` then click "Snooze" | `ActionInvoked(id, "snooze")` emitted; toast stays visible; second click works again | |

> `notify-send` action syntax uses `=` (`key=Label`). Older libnotify accepts `key:Label` — use whichever your build supports.

## T-022 — Timeout behaviour

| # | Command | Expected | Result |
|---|---------|----------|--------|
| 22.1 | `notify-send -t 2000 "Short" "2 seconds"` | Closes after ~2s; `NotificationClosed(id, 1)` | |
| 22.2 | `notify-send -u normal "Default" "5s default"` | Closes after ~5s (config default_timeout_ms) | |
| 22.3 | `notify-send -t 0 "Sticky" "Never expires"` | Still visible after 60s; closes only on dismiss/CloseNotification | |
| 22.4 | `notify-send -u critical "Critical" "Never expires"` | Still visible after 60s (critical ignores default timeout) | |
| 22.5 | `notify-send -t 5000 "Hover" "Pause me"`, hover the toast at ~2s and hold 5s | Timer pauses while hovered; on leave the remaining ~3s elapses then it expires | |

## T-023 — Multi-monitor placement & affinity

(Requires ≥2 monitors. Single-monitor: verify toast lands on the active output and note N/A.)

| # | Command | Expected | Result |
|---|---------|----------|--------|
| 23.1 | Focus a window on monitor A, `notify-send "On A" "x"` | Toast appears on monitor A | |
| 23.2 | Move focus to monitor B, `notify-send "On B" "y"` | New toast appears on monitor B | |
| 23.3 | With a long-lived toast on A (`-t 0`), switch focus to B | The A toast stays on A (affinity); it does not migrate | |
| 23.4 | (fallback) Before Hyprland focus is known / IPC down, send a notification | Toast lands on the primary monitor | |

## T-024 — Queue overflow, FIFO, critical jump

(Assumes `max_visible = 3`.)

| # | Command | Expected | Result |
|---|---------|----------|--------|
| 24.1 | `for i in 1 2 3 4 5; do notify-send -t 0 "N$i" "body"; done` | 3 toasts visible, 2 queued, **no "+N more" indicator** | |
| 24.2 | Close the oldest visible toast | Next queued (N4) appears; order preserved (FIFO) | |
| 24.3 | With 3 normal visible, `notify-send -t 0 -u critical "URGENT" "jump"` | Critical appears immediately; oldest normal moves to queue **front** | |
| 24.4 | Then close one visible | The bumped normal returns first (original FIFO position, not rear) | |

## T-025 — Replace semantics & timer reset

| # | Command | Expected | Result |
|---|---------|----------|--------|
| 25.1 | `id=$(notify-send -p -t 0 "First" "v1")` then `notify-send -r $id "Updated" "v2"` | Single toast updates in place to "Updated"/"v2"; no second toast | |
| 25.2 | `notify-send -r 99999 "Orphan" "unknown replace-id"` | New toast created (unknown replace-id → fresh notification) | |
| 25.3 | `id=$(notify-send -p -t 5000 "Timer" "v1")`, at ~4s `notify-send -r $id -t 5000 "Timer" "v2"` | Replaced toast gets a fresh 5s timer (expires ~9s after the original, not at 5s) | |

## T-026 — Markup rendering & accent routing

| # | Command | Expected | Result |
|---|---------|----------|--------|
| 26.1 | `notify-send "Markup" "Text with <b>bold</b>, <i>italic</i>, <u>underline</u>"` | Bold / italic / underline rendered | |
| 26.2 | `notify-send "Link" "See <a href=\"https://x\">link</a>"` | Link styled cyan, **not** clickable (does not open URL) | |
| 26.3 | `notify-send "Image" "before <img src=\"/tmp/x.png\"> after"` | `<img>` omitted, no pixmap, surrounding text intact | |
| 26.4 | `notify-send "Long" "$(seq 1 10 | tr '\n' ' ')...many lines..."` | Body elides to ~3 lines with "…" | |
| 26.5 | `notify-send -u critical "Crit" "x"` | Red accent | |
| 26.6 | `notify-send --hint string:category:im.received "IM" "x"` | Violet accent | |
| 26.7 | `notify-send --hint string:category:mail.arrived "Mail" "x"` | Cyan (default) accent | |

## T-027 — Config live-reload

| # | Command | Expected | Result |
|---|---------|----------|--------|
| 27.1 | Edit `$XDG_CONFIG_HOME/holonight/config.toml` (or `~/.config/holonight/config.toml`): set `[notifications]` `default_timeout_ms = 3000`, `max_visible = 5`. Save. | No restart needed | |
| 27.2 | `notify-send "After reload" "x"` (no `-t`) | Closes after ~3s (new default) | |
| 27.3 | `for i in $(seq 1 6); do notify-send -t 0 "M$i" "x"; done` | Up to 5 toasts visible (new max_visible) | |
| 27.4 | A toast already visible before the edit | Its original timer is unaffected by the reload | |

---

## Summary

Total documented manual cases: **30** across 8 task groups (exceeds the REQ-NF-042 minimum of 10).
Fill in PASS/FAIL per row during the live session.
