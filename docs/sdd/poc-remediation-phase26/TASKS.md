# Phase 26 — Weather Coordinate Validation: Tasks

**Status**: Complete — implementation, automated validation, and user verification passed.

## Pre-flight

- [x] T-191: Revalidate U-01 I-10 against current HEAD.
  - Result: `parseWeather()` accepts every TOML `double`, while
    `WeatherService` copies any present latitude/longitude pair directly into
    a weather request without a downstream range check.

## Implementation and Tests

- [x] T-192: Validate optional configured weather coordinates.
  - REQs: REQ-F-01
  - Files: `libs/holonight-config/src/ConfigParsers.cpp`.
  - Check: accept finite latitude `[-90, 90]` and longitude `[-180, 180]`;
    log and ignore values outside those ranges.

- [x] T-193: Cover coordinate bounds through the public parser.
  - REQs: REQ-F-01
  - Files: `tests/test_config_parsers.cpp`.
  - Check: retain valid endpoints and reject values just beyond each range.

## Validation and Handoff

- [x] T-194: Run focused and project validation.
  - Check: focused config-parser tests, `task test`, `task format-check`, and
    `git diff --check`.
  - Result: the focused coordinate-bounds regression passed through CTest, and
    `task test` passed all 941 tests. The changed C++ files pass direct
    `clang-format --dry-run --Werror`, and `git diff --check` passes.
    `task format-check` reports only four pre-existing violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp` (lines 56, 232,
    257, and 295).

- [x] T-195: Record user verification and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the unscheduled Low
    backlog from 45 to 44 only after acceptance evidence is recorded.
  - Result: user verification passed. `3d2c9fd` (`fix: validate configured
    weather coordinates`) implements U-01 I-10; the other 44 Low-severity
    candidates remain queued.
