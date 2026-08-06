# Phase 30 — PulseAudio Channel-Count Remediation: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-211: Revalidate the next unresolved Phase 7 candidate against current HEAD.
  - Result: U-03 I-03 was completed in Phase 17. U-04 I-04 remains applicable:
    the three PulseAudio volume setters still construct a two-channel
    `pa_cvolume` regardless of the target object.

## Implementation and Tests

- [x] T-212: Retrieve target metadata before applying sink, source, and
  playback-stream volume updates.
  - REQs: REQ-F-01, REQ-F-02, REQ-NF-01
  - Files: `libs/holonight-services/src/audio/PulseAudioBackend.cpp`.

- [x] T-213: Cover channel-count propagation through the PulseAudio test seam.
  - REQs: REQ-F-01, REQ-F-02
  - Files: `tests/test_pulse_audio_backend.cpp`.
  - Check: surround sink, mono source, and multichannel playback stream
    requests preserve the reported count and requested percentage.
  - Result: the fake PulseAudio seam verifies six-channel sink, mono source,
    eight-channel stream, malformed zero-channel fallback requests, and the
    terminal callback that follows each metadata response.

## Validation and Handoff

- [x] T-214: Run focused and project validation.
  - Check: focused PulseAudio backend tests, `task test`, `task format-check`,
    and `git diff --check`.
  - Result: the focused PulseAudioBackend CTest selection passed all 14 tests
    and `task test` passed all 942 tests. Changed C++ files pass direct
    `clang-format --dry-run --Werror`, and `git diff --check` passes.
    `task format-check` reports only four pre-existing violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp` (lines 56, 232,
    257, and 295).

- [x] T-215: Record user verification and update the Phase 7 handoff.
  - Result: user verified topbar audio-widget wheel volume changes without a
    crash after the terminal-callback ownership fix. `54133c3` (`fix:
    preserve PulseAudio channel counts`) implements U-04 I-04; the other 39
    Low-severity candidates remain queued.
