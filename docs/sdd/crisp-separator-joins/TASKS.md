# Tasks

- [x] Audit current consumers and preserve unrelated edits.
- [x] Apply the accepted separator contract where needed.
- [x] Build and run relevant regressions against the explicitly staged modified provider.
- [x] Record local verification and remaining integration boundary.

Verified 2026-09-21 against the uncommitted provider working tree, staged from
`holonight-files/build/deps/holonight-qt` into this repository's dependency prefix.

Audio/Network popup separators now use implicit physical-pixel occupancy without caller DPR division;
Network fades and Audio tests use inherited opacity. Keyboard hints use the one-pixel default.

1175 CTest cases passed across the full run and targeted retries. Sandbox-denied socket/D-Bus tests
were rerun with access; conflicting bus-name tests passed serially under a private dbus-run-session.
Final QML smoke/harness selection: 5/5. Full build, QML lint and qmltypes checks pass; existing unrelated
QML advisory warnings remain.

Native connected-window acceptance belongs to Files and has passed. The user subsequently authorized
publication and pin updates; the umbrella ledger records published revisions and the CI snapshot.
