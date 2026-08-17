# Agent Window Activation — Tasks

**Initiative work package:** `ANWA-101`
**Implementation baseline:** `b48574d39a83943cb7c6fa012ed0066bb8120399`

All tasks are intentionally unchecked. Complete and commit this work package only in `holonight-shell`.

- [x] **SH-ACT-T01 — Public compositor capability and API:** add the window-activation capability, candidate/result
  types, and compositor-neutral service/backend request API without exposing backend handles.
- [x] **SH-ACT-T02 — Hyprland inventory:** parse bounded positive client PIDs with exact titles and addresses from
  `j/clients`, retaining inventory only after a valid refresh.
- [x] **SH-ACT-T03 — Hyprland activation:** serialize `dispatch focuswindow address:<address>` with refresh/workspace
  commands, enforce the one-entry queue, and report send/compositor failures.
- [x] **SH-ACT-T04 — Sway inventory:** recursively parse PID, name, and container ID from `GET_TREE` nodes and floating
  nodes, and clear inventory on disconnect.
- [x] **SH-ACT-T05 — Sway activation:** serialize `[con_id=<id>] focus`, validate its reply, enforce the one-entry queue,
  and preserve reconnect behavior.
- [x] **SH-ACT-T06 — Candidate resolution:** implement lineage ordering, duplicate removal, exact-title
  disambiguation, missing/ambiguous results, and all input bounds as a pure testable unit.
- [ ] **SH-ACT-T07 — D-Bus provider:** add `WindowActivationServer`, export the exact
  `RequestWindowActivation(au,s)->b` interface, and map validation and backend results to accepted semantics.
- [ ] **SH-ACT-T08 — Application composition:** own/start the provider in `ShellApplication`, handle registration
  failure, and add only the required service/CMake wiring.
- [ ] **SH-ACT-T09 — Unit and integration tests:** cover resolver edges, capabilities, exact Hyprland/Sway commands,
  queue/disconnect behavior, D-Bus introspection, and application composition.
- [ ] **SH-ACT-T10 — Architecture and metadata checks:** run `task architecture-check` and `task qmltypes-check`; update
  QML metadata expectations only if the public capability requires it.
- [ ] **SH-ACT-T11 — Automated verification:** run `task format-check`, `task tidy`, and `task test`, recording exact
  commands and results in the implementation handoff.
- [ ] **SH-ACT-T12 — Manual compositor smoke checks:** on Hyprland and Sway, activate a uniquely resolved terminal,
  verify exact-title disambiguation, then close the terminal and confirm a clean failure with no process launch.
