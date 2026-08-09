# CTV-104 tasks

- [x] Establish Qt platform-theme and Quick Controls defaults before session startup.
- [x] Preserve explicit non-empty user overrides.
- [x] Import and recover both supported Qt activation variables.
- [x] Stop importing or recovering `QT_STYLE_OVERRIDE` and diagnose it when present.
- [x] Extend deterministic direct, UWSM, service-wrapper, CLI, and in-Shell regression coverage.

## Verification

2026-08-09:

- Session-script tests and 23/23 focused `SessionIntegrationServiceTest` cases passed.
- Full CTest passed 1128 tests with one existing monitor-dependent skip; eight NetworkManager fixture cases could not
  register the real system-bus service name on the active reference session and failed before exercising product code.
- Format check, clang-tidy, QML lint/type metadata, and architecture checks passed.
- Install smoke to `/tmp/holonight-shell-ctv104-install` included both session scripts, the user unit, session entry,
  and portal files.
- Desktop-integration diagnostics reported correct process Qt defaults and the expected missing systemd imports in
  the current non-HoloNight bootstrap session.
