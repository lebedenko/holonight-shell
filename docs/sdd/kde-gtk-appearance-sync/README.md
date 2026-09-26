# Login appearance reconciliation

After preparing and importing the session environment, `holonight-session` invokes one adapter apply against the canonical appearance file before starting a compositor or shell service. `timeout` bounds the call to 15 seconds. Failure produces a warning and session startup proceeds. The existing cursor query still supplies `XCURSOR_THEME` for session processes.

The shell task dependency assertion uses umbrella pinned `holonight-qt` revision `61d0c16`, so local acceptance can build against the current provider.

## Implementation tasks and files

- [x] `scripts/holonight-session`: run one bounded apply after session environment import and before compositor launch; warn and continue on failure.
- [x] `tests/test_session_scripts.sh`: cover both compositor paths, path selection, ordering, and nonfatal failure.
- [x] `Taskfile.yml`: align the local provider assertion with the umbrella pinned Qt revision.

Verification on 2026-09-26: focused session script test; `task test` 1170/1170; `bash -n` and REUSE lint pass.
