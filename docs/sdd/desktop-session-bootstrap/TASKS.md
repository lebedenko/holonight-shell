# SDD Tasks - desktop-session-bootstrap

## Phase 1: Documentation and Bootstrap Assets

- [x] T-001: Add `scripts/holonight-hyprland-session`
  - REQs: REQ-F-017, REQ-F-019, REQ-C-001, REQ-C-004
  - Check: Script exports Hyprland desktop variables, imports them with
    `dbus-update-activation-environment --systemd` and
    `systemctl --user import-environment`, then starts Hyprland through the
    selected UWSM/direct path.

- [x] T-002: Add `data/wayland-sessions/holonight-hyprland.desktop`
  - REQs: REQ-F-018
  - Check: Desktop file has `Name=HoloNight (Hyprland)`, `Exec=holonight-hyprland-session`,
    `TryExec=Hyprland`, and `DesktopNames=Hyprland`.

- [x] T-003: Install bootstrap assets from CMake
  - REQs: REQ-F-017, REQ-F-018
  - Check: `cmake --install build --prefix ~/.local` installs the script under
    `~/.local/bin` and the session file under
    `~/.local/share/wayland-sessions`.

- [x] T-004: Document UWSM and non-UWSM startup modes in README or docs
  - REQs: REQ-F-019, REQ-NF-003
  - Check: Documentation explains which login entry to select and how to verify
    the environment with the smoke script.

## Phase 2: Smoke Check Script

- [x] T-005: Add `scripts/check-desktop-integration.sh`
  - REQs: REQ-F-020, REQ-NF-003, REQ-NF-004
  - Check: Running the script prints Environment, Portals, D-Bus desktop
    services, MIME/default apps, XDG menus, and KDE caches sections without
    changing system state.

- [x] T-006: Add XDG menu prefix validation to the smoke script
  - REQs: REQ-F-005, REQ-F-007
  - Check: With `XDG_MENU_PREFIX=hyprland-` and no
    `hyprland-applications.menu`, the script prints a warning and lists
    available `*applications.menu` files.

- [x] T-007: Add KDE sycoca freshness validation to the smoke script
  - REQs: REQ-F-008, REQ-F-009
  - Check: The script prints newest `.desktop`, newest `mimeinfo.cache`, newest
    `ksycoca6*`, and warns when sycoca is missing or older.

## Phase 3: SessionIntegrationService Scaffolding

- [x] T-008: Add session integration service source files
  - REQs: REQ-F-001
  - Check: `src/services/session-integration/SessionIntegrationService.h/.cpp`
    exist and are listed in `holonight_services`.

- [x] T-009: Register `SessionIntegrationService` as a QML singleton
  - REQs: REQ-F-001
  - Check: `ShellApplication` constructs the service and registers it as
    `SessionIntegrationService` in the `HolonightShell` module.

- [x] T-010: Add service properties and invokables
  - REQs: REQ-F-001, REQ-F-012
  - Check: The service exposes `overallStatus`, `diagnostics`,
    `refreshInProgress`, `rebuildInProgress`, `refresh()`, and
    `rebuildApplicationCaches()`.

## Phase 4: Diagnostic Logic

- [x] T-011: Implement process environment diagnostics
  - REQs: REQ-F-002
  - Check: Unit test verifies all required desktop variables produce diagnostic
    rows with observed values.

- [x] T-012: Implement systemd user environment comparison
  - REQs: REQ-F-003
  - Check: Unit test feeds fake `systemctl --user show-environment` output and
    verifies missing `XDG_CURRENT_DESKTOP` is reported.

- [x] T-013: Implement D-Bus activation environment diagnostics
  - REQs: REQ-F-004
  - Check: Missing `DBUS_SESSION_BUS_ADDRESS` is classified as error; missing
    desktop variables are classified as warnings.

- [x] T-014: Implement XDG menu path discovery
  - REQs: REQ-F-005, REQ-F-007
  - Check: Unit test covers `$XDG_CONFIG_HOME/menus`, `XDG_CONFIG_DIRS`, and
    `/etc/xdg/menus` fallback ordering.

- [x] T-015: Implement selected menu file validation
  - REQs: REQ-F-005, REQ-F-006
  - Check: Unit test verifies non-empty `XDG_MENU_PREFIX` still warns when the
    matching menu file does not exist.

- [x] T-016: Implement KDE sycoca missing/stale checks
  - REQs: REQ-F-008, REQ-F-009
  - Check: Unit test creates temporary `.desktop`, `mimeinfo.cache`, and
    `ksycoca6` files with controlled mtimes and verifies status classification.

- [x] T-017: Implement MIME/default app diagnostic rows
  - REQs: REQ-F-010, REQ-F-011
  - Check: Unit test uses a fake command executor for `xdg-mime` and
    `xdg-settings` and verifies role diagnostics include desktop filenames.

- [x] T-018: Implement portal and desktop D-Bus owner diagnostics
  - REQs: REQ-F-014, REQ-F-015, REQ-F-016
  - Check: Unit test uses a fake D-Bus name provider and verifies portal,
    ScreenSaver, Notifications, and StatusNotifier rows.

## Phase 5: Cache Rebuild Action

- [x] T-019: Implement `ApplicationCacheRebuilder`
  - REQs: REQ-F-012, REQ-NF-002
  - Check: Unit test verifies only user-writable application dirs are passed to
    `update-desktop-database`.

- [x] T-020: Wire `rebuildApplicationCaches()` to the rebuilder
  - REQs: REQ-F-012
  - Check: Fake command executor records sequential
    `update-desktop-database <dir>` calls followed by
    `kbuildsycoca6 --noincremental`.

- [x] T-021: Refresh shell models after rebuild
  - REQs: REQ-F-013
  - Check: After fake successful rebuild, service calls its own `refresh()`,
    `MimeService::refreshAllRoles()`, and the selected launcher refresh path.

## Phase 6: Sidebar UI

- [x] T-022: Add a compact Session Integration section to `SidebarSystem.qml`
  - REQs: REQ-F-001, REQ-NF-003
  - Check: The System tab shows overall status and failing diagnostics without
    listing every successful check.

- [x] T-023: Replace or subsume the old KDE cache warning row
  - REQs: REQ-F-005, REQ-F-006, REQ-F-012
  - Check: A missing or invalid XDG menu prefix and stale sycoca both show the
    rebuild/cache guidance; the old warning condition remains covered.

- [x] T-024: Add Refresh and Rebuild buttons
  - REQs: REQ-F-012, REQ-NF-001
  - Check: Buttons call `SessionIntegrationService.refresh()` and
    `rebuildApplicationCaches()`; busy state disables repeated rebuild clicks.

## Phase 7: Verification

- [x] T-025: Add focused unit tests for parsers and classifiers
  - REQs: REQ-F-002 through REQ-F-016
  - Check: New tests pass with `ctest -R SessionIntegration --output-on-failure`.

- [x] T-026: Run build and narrow tests
  - REQs: all implementation requirements
  - Check: `task build` and relevant `ctest -R ... --output-on-failure` pass.

- [x] T-027: Run QML checks after sidebar changes
  - REQs: REQ-F-001
  - Check: `task qml-lint` and `task qmltypes-check` pass.

- [x] T-028: Live verify Dolphin "Open with..." repair
  - REQs: REQ-F-005, REQ-F-008, REQ-F-012
  - Check: In a live Hyprland session, reproduce Dolphin showing manual
    executable entry, run the HoloNight rebuild action, restart Dolphin if
    needed, and verify "Open with..." lists applications.

- [x] T-029: Live verify environment propagation from login
  - REQs: REQ-F-017, REQ-F-018, REQ-F-019
  - Check: Start a new session through `HoloNight (Hyprland)`, run
    `scripts/check-desktop-integration.sh`, and verify process, D-Bus, and
    systemd user environments agree on desktop variables.
