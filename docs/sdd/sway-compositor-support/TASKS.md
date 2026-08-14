# Sway compositor support tasks

| ID | Task | State |
|---|---|---|
| SWS-102-01 | Accept interfaces, truth table, selection, snapshot, IPC, surface, session, portal, and verification design | Done |
| SWS-102-02 | Implement compositor service and Hyprland/Sway/generic backends with deterministic tests | Done |
| SWS-102-03 | Adopt `HolonightQt::Wayland` for every Shell layer surface and remove local protocol ownership | Done |
| SWS-102-04 | Install descriptor-driven sessions and compositor-aware portal routing | Done |
| SWS-102-05 | Run focused and full verification, record results, and publish the Shell series | Done |

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

## SWS-102-04 completion verification — 2026-08-14

- Session and routing commits:
  - `8377c34` — descriptor-driven Hyprland/Sway sessions;
  - `6e527f5` — pinned Wayland provider and compositor portal routing.
- Cleanup baseline: `holonight-shell@037ed1d` (`fix: finish compositor-aware session bootstrap`).
- Session script syntax and focused behavior tests: passed for Hyprland and Sway, direct/auto/UWSM modes, cursor
  fallbacks, environment imports, stale-marker removal, shell-service startup, invalid arguments, and missing
  executables.
- Focused `SessionService` tests: passed (17 tests).
- Temporary-prefix install: passed; both login descriptors, the canonical launcher, systemd service, portal
  descriptor, generic routing, and Hyprland/Sway routing were installed; the legacy launcher was absent.
- `task test`: passed (1059 tests, 2 skipped because no session D-Bus was available).
- `task build`, `task format-check`, `task tidy-src`, `task tidy-tests`, `task tidy`, `task qmltypes-check`, and
  `task architecture-check`: passed.
- `task qml-lint`: passed with the existing unresolved `AudioService` warnings.
- Manual Hyprland/Sway login, visual, hotplug, and close/reopen checks remain deferred to umbrella integration task
  `SWS-201`.

## SWS-102-05 completion verification — 2026-08-14

- Published implementation completion commit: `holonight-shell@248a61d9f947b56e6fddeb60d8e3c06a0228a13f`
  (`ci: install system services dependency`). This handoff record is the final Shell documentation commit for
  SWS-102; umbrella status and gitlink integration remain owned by `SWS-201`.
- Accepted dependency revisions:
  - `holonight-config@5cd36ec9986801c4b831527a38e7b7414b9a1312`;
  - `holonight-qt@a45f7552054abbc6cbd66609e802b43b9b8ee894`;
  - `holonight-system-services@4556dc9b22237823387110340347cd1301ed9245`.
- Focused `test_holonight_compositor`: passed (18 tests).
- Focused `test_holonight_surfaces`: passed (164 tests).
- Focused `SessionServiceTest`, `SessionIntegrationServiceTest`, and `LogindSessionResolverTest`: passed (44
  tests).
- `tests/test_session_scripts.sh`: passed for Hyprland and Sway session selection and routing; the expected
  cursor-query fallback warnings were unchanged.
- `task sway-runtime-smoke`: passed outside the filesystem sandbox, including named-workspace activation and Shell
  startup. The sandboxed attempt could not create Sway's Wayland socket and retained diagnostics at
  `/tmp/holonight-sway-smoke.wCVgDa`.
- Temporary-prefix install and inspection: passed; both session descriptors, `holonight-session`,
  `holonight-shell-systemd`, the user service, the HoloNight portal descriptor, generic portal routing, and
  Hyprland/Sway routing were installed, and the legacy launcher was absent.
- `task test`: passed (1059 tests, 2 skipped because no session D-Bus was available).
- `task build`, `task format-check`, `task tidy`, `task qmltypes-check`, and `task architecture-check`: passed.
- `task qml-lint`: passed with the existing unresolved `AudioService` warnings.
- The architecture check and source inspection confirmed that no Shell-owned layer-shell protocol generation,
  direct private protocol use, or private layer-surface lifecycle implementation remains.
- Candidate CI: [run 31757317259](https://github.com/lebedenko/holonight-shell/actions/runs/31757317259) passed both
  `Build and test` and `Static checks` at the exact implementation completion commit with the accepted dependency
  revisions.
- Live Hyprland/Sway visual, hotplug, login, and close/reopen checks remain deferred to umbrella integration task
  `SWS-201`.

`SWS-102-02`, `SWS-102-03`, `SWS-102-04`, and `SWS-102-05` are Done. The Shell handoff is ready for `SWS-201`.
