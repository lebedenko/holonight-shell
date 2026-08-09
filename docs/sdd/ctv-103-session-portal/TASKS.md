# CTV-103 tasks

- [x] Resolve and query the canonical cursor before session environment import.
- [x] Preserve inherited/native cursor behavior on every adapter fallback.
- [x] Propagate cursor and canonical-path variables through D-Bus, systemd, and the service wrapper.
- [x] Compare active and canonical cursor state with session-restart guidance.
- [x] Report explicit HoloNight Settings portal ownership and disconnected-bus fallback.
- [x] Queue diagnostic refresh after canonical cursor changes during active work.
- [x] Extend deterministic script, service, portal-signal, and smoke coverage.

## Verification

2026-08-09:

- `task test`: 1133/1133 passed; one monitor-dependent test skipped by its existing runtime guard.
- `task format-check`, `task tidy`, `task qml-lint`, `task qmltypes-check`, and `task architecture-check`: passed.
- Focused session/portal suite: 25/25 passed after the final test-helper refinement.
- Install smoke to `/tmp/holonight-shell-ctv103-install`: passed and included both session scripts, the user unit,
  session entry, and portal routing files.
- `task compositor-smoke-check`: detected the live Wayland/Hyprland session; its `hyprctl monitors` query was
  unavailable in the sandbox, while the remaining required smoke tools were present.
- `scripts/check-desktop-integration.sh`: completed and correctly reported the current non-HoloNight bootstrap as
  missing cursor/systemd/portal state, with native fallback and session-restart guidance.
