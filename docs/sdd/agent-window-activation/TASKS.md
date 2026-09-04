# Agent Window Activation — Tasks

**Initiative work package:** `ANWA-101`
**Implementation baseline:** `b48574d39a83943cb7c6fa012ed0066bb8120399`

All tasks are intentionally unchecked. Complete and commit this work package only in `holonight-shell`.

- [x] **SH-ACT-T01 — Public compositor capability and API:** add the window-activation capability, candidate/result
  types, and compositor-neutral service/backend request API without exposing backend handles.
- [x] **SH-ACT-T02 — Hyprland inventory:** parse bounded positive client PIDs with exact titles and addresses from
  `j/clients`, retaining inventory only after a valid refresh.
- [x] **SH-ACT-T03 — Hyprland activation:** serialize legacy and current Lua window-focus commands with
  refresh/workspace commands, enforce the one-entry queue, and report send/compositor failures.
- [x] **SH-ACT-T04 — Sway inventory:** recursively parse PID, name, and container ID from `GET_TREE` nodes and floating
  nodes, and clear inventory on disconnect.
- [x] **SH-ACT-T05 — Sway activation:** serialize `[con_id=<id>] focus`, validate its reply, enforce the one-entry queue,
  and preserve reconnect behavior.
- [x] **SH-ACT-T06 — Candidate resolution:** implement lineage ordering, duplicate removal, exact-title
  disambiguation, missing/ambiguous results, and all input bounds as a pure testable unit.
- [x] **SH-ACT-T07 — D-Bus provider:** add `WindowActivationServer`, export the exact
  `RequestWindowActivation(au,s)->b` interface, and map validation and backend results to accepted semantics.
- [x] **SH-ACT-T08 — Application composition:** own/start the provider in `ShellApplication`, handle registration
  failure, and add only the required service/CMake wiring.
- [x] **SH-ACT-T09 — Unit and integration tests:** cover resolver edges, capabilities, exact Hyprland/Sway commands,
  queue/disconnect behavior, D-Bus introspection, and application composition.
- [x] **SH-ACT-T10 — Architecture and metadata checks:** run `task architecture-check` and `task qmltypes-check`; update
  QML metadata expectations only if the public capability requires it.
- [x] **SH-ACT-T11 — Automated verification:** run `task format-check`, `task tidy`, and `task test`, recording exact
  commands and results in the implementation handoff.
- [x] **SH-ACT-T12 — Manual compositor smoke checks:** on Hyprland and Sway, activate a uniquely resolved terminal,
  verify exact-title disambiguation, then close the terminal and confirm a clean failure with no process launch.

## Verification record

- 2026-09-03: `dbus-run-session -- env XDG_RUNTIME_DIR=/tmp QT_QPA_PLATFORM=offscreen task test` passed
  1,094/1,094 tests, including the D-Bus contract and Hyprland/Sway activation coverage.
- 2026-09-03: `task format-check`, `task tidy`, `task architecture-check`, and `task qmltypes-check` passed.
- 2026-09-04: Hyprland 0.56.2 rejected the legacy `focuswindow` dispatcher syntax; the Lua fallback
  `hl.dsp.focus({ window = "address:<address>" })` returned `ok` and focused the exact `htop` terminal on another
  workspace. Automated coverage now verifies this fallback command and bounded rejection behavior.
- 2026-09-04: user acceptance passed the remaining Hyprland and Sway positive and negative checks. Two Ghostty
  windows with the same compositor PID and exact `htop` title correctly returned `false`: their distinct child
  lineages converge at the single Ghostty process, so the compositor inventory cannot distinguish them and Shell
  preserves the fail-safe ambiguous-target result. Closing either candidate restored unique activation as expected.
