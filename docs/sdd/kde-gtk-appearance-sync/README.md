# Login appearance reconciliation

After preparing and importing the session environment, `holonight-session` invokes one adapter apply against the canonical appearance file before starting a compositor or shell service. `timeout` bounds the call to 15 seconds. Failure produces a warning and session startup proceeds. The existing cursor query still supplies `XCURSOR_THEME` for session processes.

Verification: `tests/test_session_scripts.sh` covers both compositors, canonical path override and default, and adapter failure; run the repository test and required script checks as well.

The shell task dependency assertion uses umbrella pinned `holonight-qt` revision `61d0c16`, so local acceptance can build against the current provider.
