# Coverage Improvement Tasks

This is the actionable follow-up list for improving refactor safety after the 2026-06-19 coverage review.

## Current Baseline

- `task coverage`: 53.6% line, 31.6% branch after the second focused coverage pass.
- Highest-risk remaining areas are surface orchestration, external backend adapters, skipped socket/audio tests, and QML component behavior.

## Task List

### 1. Surface orchestration seams

- [x] Extract and test `StatusPopupSurface` sizing/positioning policy.
- [x] Extract and test session popup placement policy from `PopupSurface`.
- [x] Extract and test launcher surface lifecycle state transitions.
- [x] Extract and test widget visibility/routing policy from `WidgetManager`.
- [x] Extract and test sidebar show/hide/routing policy from `SidebarManager`.

Acceptance:

- Surface policy tests run without Wayland.
- Tests assert public behavior or value objects, not private implementation details.
- No production UI behavior changes except through explicitly verified helpers.

### 2. External backend adapters

- [x] Add fake-provider tests for `WeatherService` config/cache/fetch/stale/backoff behavior.
- [x] Extract deterministic response parsing from `WeatherProvider` and test malformed/success payloads.
- [x] Add fake D-Bus snapshot tests for `NetworkManagerBackend`.
- [x] Add PulseAudio adapter seams so callback/event handling can be tested without a real server.
- [x] Add focused tests for `DbusPropertyClient` error and type-conversion behavior where practical.

Acceptance:

- Backend tests avoid live NetworkManager, PulseAudio, and internet access.
- Error recovery and malformed payload paths are covered.

### 3. Skipped tests

- [x] Stabilize `HyprlandIpcClient` local-socket tests or replace live local-server dependency with an injectable socket seam.
- [x] Make `PulseAudioBackend.StartIsIdempotent` deterministic through an adapter seam or mark it as an explicit integration-only test.

Acceptance:

- Coverage runs have no unexpected skips.
- Any intentional integration-only skips are documented and excluded from coverage expectations.

### 4. QML component smoke coverage

- [x] Add launcher component smoke coverage.
- [x] Add right-sidebar component smoke coverage.
- [x] Add network popup smoke coverage.
- [x] Add audio popup smoke coverage.
- [x] Add battery popup smoke coverage.
- [x] Add notification toast stack smoke coverage.
- [x] Add widget surface smoke coverage.

Acceptance:

- Component smoke tests use fake singleton state.
- Tests catch missing properties, moved QML files, broken singleton APIs, and required-property regressions.

### 5. Coverage gates

- [x] Raise coverage gates to 45% line / 30% branch after surface and backend seams land.
- [x] Re-run `task coverage` and update the baseline in this file.
- [x] Keep raising thresholds incrementally only after high-risk files improve.

Acceptance:

- Coverage thresholds reflect meaningful runtime-risk coverage, not only trivial helper tests.
