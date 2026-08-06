# Desktop Integration — Master SDD Roadmap

Master pipeline that decomposes the desktop-integration gaps (from
`assets/dont-commit/09-desktop-integrations/conversation-summary.md`) into
separate, independently-runnable SDD pipelines. Each sub-pipeline gets its own
`docs/sdd/{slug}/` with `SPEC.md` / `DESIGN.md` / `TASKS.md`. We run them one by
one; this file tracks order, dependencies, and status.

## Guiding principle (applies to every sub-pipeline)

HoloNight is the **UX/control plane over existing system daemons**, never the
security primitive and never a reimplementation. Every system-facing feature is
a **backend-adapter interface + compositor autodetect + status/config UI**.
Hyprland is the first adapter; Sway/Niri/wlroots/logind come later.

## Sub-pipelines

| # | Slug | Scope | Depends on | Status |
|---|------|-------|-----------|--------|
| 1 | `session-lock-backend` | Refactor `SessionService` → `SessionBackend` adapters (compositor autodetect); implement real `lockScreen()` via `loginctl lock-session` + hyprlock. Establishes the shared backend-adapter + detection pattern. | — | ✅ Done — `docs/sdd/session-lock-backend/` |
| 2 | `idle-management` | `IdleBackend` abstraction; detect/report active·idle·locked state (`ext-idle-notify-v1` / hypridle); keep-awake / inhibit-idle action; hypridle status. | 1 (detection pattern) | ✅ Done — `docs/sdd/idle-management/` (T-035–T-040 deferred: require extended live session or hypridle ≥ 0.2.0) |
| 3 | `brightness-service` | `BrightnessService` (backlight; DDC where available) + sidebar slider UI. | — | ✅ Done — `docs/sdd/brightness-service/` |
| 4 | `portal-service` | `PortalService` diagnostics (broker/backends/interfaces) + FileChooser/OpenURI/Settings consumers. Portal Inhibit remains diagnostic-only; #2 owns keep-awake via logind. | (soft) 2 | 🟡 Implemented; live verification pending — `docs/sdd/portal-service/` |
| 5 | `mime-desktop-integration` | `MimeService` / `DesktopEntryService` / `KdeCompatService`; default-app + per-MIME UI; "rebuild caches" + env diagnostics (the `kbuildsycoca6`/`XDG_MENU_PREFIX` problem). | — | ✅ Done — `docs/sdd/mime-desktop-integration/` |
| 6 | `notification-hardening` | DND mode, per-app rules, critical-notification handling, daemon-already-running detection. Extends existing notification server. | — | ✅ Done — `docs/sdd/notification-hardening/` |
| 7 | `launcher-fs-watch` | `QFileSystemWatcher` on XDG app dirs → cache invalidation when apps installed/removed. Pairs with #5's `DesktopEntryService`. | (soft) 5 | ✅ Done — `docs/sdd/launcher-fs-watch/` |
| 8 | `power-extensions` | Lid events, low-battery policy, suspend inhibitors, battery charge-limit/health. Extends `PowerProfilesService`/`BatteryService`. | — | ✅ Done — `docs/sdd/power-extensions/` |
| 9 | `desktop-session-bootstrap` | Installable Hyprland/HoloNight session bootstrap, D-Bus/systemd environment import, XDG menu/KDE sycoca diagnostics, safe application-cache rebuild, and live smoke tooling. | 4, 5, 7 | ⬜ Not started — `docs/sdd/desktop-session-bootstrap/` |

## Recommended execution order

1. **`session-lock-backend`** — small, foundational, delivers the real lock
   screen (part of the #1 gap) and establishes the backend-adapter + autodetect
   pattern every later pipeline reuses.
2. **`idle-management`** — pairs with lock; the original Teams-idle bug.
3. **`brightness-service`** — independent, high daily-UX value, self-contained.
4. **`portal-service`** — diagnostics and portal consumers; live-session acceptance remains pending.
5. **`mime-desktop-integration`**
6. **`launcher-fs-watch`** (after #5's DesktopEntryService lands)
7. **`notification-hardening`**
8. **`power-extensions`**
9. **`desktop-session-bootstrap`** — closes the remaining gap between a running
   shell process and a complete desktop session for D-Bus activation, portals,
   KDE frameworks, MIME/application caches, and apps such as Dolphin.

## Status legend

⬜ Not started · 🟡 In progress · ✅ Done

> When a sub-pipeline completes, flip its row to ✅ and link its `docs/sdd/{slug}/`.
