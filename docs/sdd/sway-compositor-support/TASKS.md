# Sway compositor support tasks

| ID | Task | State |
|---|---|---|
| SWS-102-01 | Accept interfaces, truth table, selection, snapshot, IPC, surface, session, portal, and verification design | Done |
| SWS-102-02 | Implement compositor service and Hyprland/Sway/generic backends with deterministic tests | Done |
| SWS-102-03 | Adopt `HolonightQt::Wayland` for every Shell layer surface and remove local protocol ownership | Done |
| SWS-102-04 | Install descriptor-driven sessions and compositor-aware portal routing | In Progress |
| SWS-102-05 | Run focused and full verification, record results, and publish the Shell series | Planned |

Iteration 3 baseline: `holonight-shell@34580eaad4da8a5440c561c0fe2e82c5f3189727`

Provider: `holonight-qt@a45f7552054abbc6cbd66609e802b43b9b8ee894`

## Iteration 3 completion verification — 2026-08-13

- Completion baseline: `holonight-shell@2fefa4f`.
- Focused compositor tests: passed (18 tests), including Hyprland refresh, partial-failure, event coalescing,
  queued activation, and Lua fallback coverage.
- Headless Sway runtime smoke: passed, including named workspace activation and full Shell startup. The harness uses
  an isolated temporary runtime directory; failed sandbox attempts retained logs at
  `/tmp/holonight-sway-smoke.lritEt`, `/tmp/holonight-sway-smoke.yM1cje`, and
  `/tmp/holonight-sway-smoke.nY4yc0`.
- `task test`: passed (1034 tests, 3 skipped).
- `task format-check`: passed.
- `task tidy-src`: passed.
- `task tidy-tests`: passed.
- `task tidy`: passed.
- `task qml-lint`: passed with the existing unresolved `AudioService` warnings.
- `task qmltypes-check`: passed.
- `task architecture-check`: passed.
- Production and test trees contain no references to `WorkspaceModel`, `ExtWorkspaceManager`,
  `HyprlandWorkspaceService`, `ActiveWindowService`, or `MonitorOccupancyService`.

## SWS-102-03 completion verification — 2026-08-14

- Adoption commits:
  - `a66c08b` — persistent shell surfaces;
  - `10e0afb` — standalone transient surfaces;
  - `e464d17` — paired transient surfaces;
  - `9c5434e` — sidebar surfaces.
- Cleanup baseline: `holonight-shell@3bf079d`.
- `test_holonight_surfaces`: passed (164 tests), preserving policy and lifecycle coverage for persistent,
  standalone transient, paired transient, and sidebar surfaces.
- `task test`: passed (1059 tests, 2 skipped because no session D-Bus was available).
- `task build`: passed.
- `task format-check`: passed.
- `task tidy-src`: passed.
- `task tidy-tests`: passed.
- `task tidy`: passed.
- `task qml-lint`: passed with the existing unresolved `AudioService` warnings.
- `task qmltypes-check`: passed.
- `task architecture-check`: passed.
- Manual Hyprland/Sway visual, hotplug, and close/reopen checks remain deferred to umbrella integration task
  `SWS-201`.

`SWS-102-02` and `SWS-102-03` are Done. `SWS-102-04` remains In Progress; `SWS-102-05` remains Planned.
