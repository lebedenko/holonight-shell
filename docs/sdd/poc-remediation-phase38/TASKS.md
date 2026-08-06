# Phase 38 — Desktop Entry Field Mapping Consistency: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-257: Revalidate U-06 I-09 against current `main`.
  - Check: confirm the scalar `DesktopEntry` fields are still independently
    enumerated by scanner parsing and both serializer directions; identify the
    existing direct serializer, scanner, and cache-reopen test seams.
  - Result: confirmed at `8e1d735`. Eight desktop-file scalar keys remained
    explicit in `applyDesktopEntryField()`, while nine scalar members remained
    explicit in each JSON direction. Existing deterministic coverage includes
    direct serializer, scanner, and cache-reopen tests.

## Implementation and Tests

- [x] T-258: Add a compile-time scalar `DesktopEntry` field descriptor.
  - REQs: REQ-F-01
  - Files: `libs/holonight-services/src/launcher/DesktopEntryScanner.h` (or a
    focused launcher header if review shows that is clearer).
  - Check: the descriptor maps JSON keys, optional desktop-file keys, and
    `QString DesktopEntry::*` members for exactly the nine scoped scalar
    fields; `desktop_file` has no desktop-file key.
  - Result: `kDesktopEntryTextFields` is a compile-time array mapping the nine
    JSON keys, the eight applicable desktop-file keys, and typed member
    pointers. The `desktop_file` descriptor has an empty desktop key.

- [x] T-259: Route desktop-file scalar parsing through the descriptor.
  - REQs: REQ-F-01, REQ-F-02
  - Files: `libs/holonight-services/src/launcher/DesktopEntryScanner.cpp`.
  - Check: existing XDG scalar keys assign the same members; path-derived
    `desktop_file`, boolean fields, actions, MIME types, and launchability
    filtering retain their present behavior.
  - Result: scalar assignment now resolves through the shared descriptor;
    special parsing and path-derived `desktop_file` assignment remain
    unchanged.

- [x] T-260: Route JSON scalar serialization through the descriptor.
  - REQs: REQ-F-01, REQ-F-02
  - Files: `libs/holonight-services/src/launcher/DesktopEntrySerializer.h`.
  - Check: every existing JSON key/value remains stable; mandatory-field
    validation and action/MIME collection handling remain explicit; no cache
    schema version or SQL change is introduced.
  - Result: `toJson()` and `fromJson()` iterate the descriptor for scalar text
    values. Required fields, booleans, actions, MIME types, cache version, and
    SQL are unchanged.

- [x] T-261: Add focused deterministic regression coverage.
  - REQs: REQ-F-01, REQ-F-02
  - Files: `tests/test_launcher_service.cpp`.
  - Check: one representative desktop file exercises every mapped scanner
    field, JSON round-trip retains every scalar field, and a cache upsert/
    reopen preserves those values without a live desktop or compositor.
  - Result: scanner coverage now asserts all mapped input fields plus the
    derived desktop-file path; serializer coverage includes booleans, actions,
    and MIME types; cache-reopen coverage asserts all mapped scalar values.

## Validation and Handoff

- [x] T-262: Run focused and project validation.
  - Check: focused `DesktopEntryScanner.*`, `DesktopEntrySerializer.*`, and
    `DesktopEntryCache.*` coverage; `task test`; `task architecture-check`;
    `task format-check`; changed-file `clang-format --dry-run --Werror`; and
    `git diff --check`. Record pre-existing repository-wide failures
    separately.
  - Result: focused `DesktopEntrySerializer.*`, `DesktopEntryCache.*`, and
    `DesktopEntryScanner.*` coverage passed 13/13. `task architecture-check`,
    changed-file `clang-format --dry-run --Werror`, and `git diff --check`
    passed. `task test` ran 953 tests and failed only the pre-existing
    `KeyboardLayoutService.EventSocketConnectedTriggersDevicesQuery` test,
    already recorded in Phase 34. `task format-check` reports only the four
    pre-existing `HyprlandWorkspaceService.cpp` violations at lines 56, 232,
    257, and 295.

- [x] T-263: Obtain user verification and close the phase.
  - Check: user confirms launcher results remain available after a restart;
    record the implementation commit, validation outcome, and reduce the
    queued Low-severity backlog from 27 to 26 only after acceptance evidence.
  - Result: user verified launcher results remain available after restart.
    `a4ed9b9` (`refactor: consolidate desktop entry fields`) implements U-06
    I-09; 26 Low-severity candidates remain queued.
