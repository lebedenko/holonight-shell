# CTV-104 tasks

- [x] Establish Qt platform-theme and Quick Controls defaults before session startup.
- [x] Preserve explicit non-empty user overrides.
- [x] Import and recover both supported Qt activation variables.
- [x] Stop importing or recovering `QT_STYLE_OVERRIDE` and diagnose it when present.
- [x] Extend deterministic direct, UWSM, service-wrapper, CLI, and in-Shell regression coverage.
- [x] Route HoloNight Settings without replacing the Hyprland compositor identity.
- [x] Preserve the Hyprland desktop identity across the exclusive UWSM handoff and display-manager metadata.
- [x] Export the Settings backend's scriptable invokables and cover them on an isolated real D-Bus session.

## Verification

2026-08-09:

- Session-script tests and 23/23 focused `SessionIntegrationServiceTest` cases passed.
- Full CTest passed 1127 tests with one existing monitor-dependent skip; eight NetworkManager fixture cases could not
  register the real system-bus service name on the active reference session and failed before exercising product code.
- Format check, clang-tidy, QML lint/type metadata, and architecture checks passed.
- Install smoke to `/tmp/holonight-shell-ctv104-install` included both session scripts, the user unit, session entry,
  and portal files.
- Desktop-integration diagnostics reported correct process Qt defaults and the expected missing systemd imports in
  the current non-HoloNight bootstrap session.
- Canonical GitHub CI run `31333697915` passed at implementation commit `788b96e`.
- Fresh-session verification found and repaired a missing source-install prerequisite (`holonight-config`), an
  inherited `XDG_CURRENT_DESKTOP=Hyprland` routing mismatch, and unexported Settings backend invokables. After the
  Config runtime was installed, the Shell remained active and the user confirmed background, widgets, top bar, and
  notification behavior were restored.
- Follow-up verification passed all 1,137 tests (two environment-dependent skips), the isolated session-bus portal
  export test, formatting, clang-tidy, QML lint/type metadata, and architecture checks.
