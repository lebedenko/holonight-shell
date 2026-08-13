# Sway compositor support tasks

| ID | Task | State |
|---|---|---|
| SWS-102-01 | Accept interfaces, truth table, selection, snapshot, IPC, surface, session, portal, and verification design | Done |
| SWS-102-02 | Implement compositor service and Hyprland/Sway/generic backends with deterministic tests | In Progress |
| SWS-102-03 | Adopt `HolonightQt::Wayland` for every Shell layer surface and remove local protocol ownership | Planned |
| SWS-102-04 | Install descriptor-driven sessions and compositor-aware portal routing | In Progress |
| SWS-102-05 | Run focused and full verification, record results, and publish the Shell series | Planned |

Iteration 3 baseline: `holonight-shell@34580eaad4da8a5440c561c0fe2e82c5f3189727`

Provider: `holonight-qt@a45f7552054abbc6cbd66609e802b43b9b8ee894`

## Iteration 3 verification — 2026-08-13

- Focused compositor tests: passed (14 tests).
- Headless Sway runtime smoke: passed, including named workspace activation and full Shell startup.
- `task test`: passed (1111 tests, 3 skipped).
- `task format-check`: passed.
- `task qml-lint`: passed with the existing unresolved `AudioService` warnings.
- `task qmltypes-check`: passed.
- `task architecture-check`: passed.
- `task tidy`: pending; the first run reported new-code diagnostics that are being resolved.

`SWS-102-02` remains In Progress until the static-analysis gate passes and the superseded
compositor-state implementations are removed.
