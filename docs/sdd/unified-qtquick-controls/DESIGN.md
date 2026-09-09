# UQC-102 design

This design was reviewed against the assigned shell code and pinned provider on
2026-09-09. [SPEC.md](SPEC.md) defines acceptance; [TASKS.md](TASKS.md) orders work.
Paths below are shell-relative unless explicitly identified as provider paths.

## QML inventory and migration

| Files | Current boundary and planned action |
|---|---|
| `apps/shell/qml/Launcher/LauncherRightPanelBrowse.qml` | Unqualified QtQuick.Controls import; qualify standard instances, attached properties and enums |
| `qml/Authentication/AuthenticationDialog.qml` | Basic plus direct Holonight imports; migrate ApplicationWindow/Button/ScrollBar and other standard uses to Controls; qualify ScrollView and retain custom ActionButton painting |
| `qml/Authentication/AuthenticationPrompt.qml` | Basic TextField/Button; qualify while preserving input aliases, echoMode, reveal action, validation, focus and key handlers |
| `qml/Authentication/MessageList.qml` | Basic ScrollBar attached property and instance; qualify both |
| `qml/Authentication/IdentitySelector.qml` | Direct H.ComboBox/H.ItemDelegate, Basic Labels and style-only delegateHeight; use public HnIconComboBox, Controls.ItemDelegate and Controls.Label |
| `qml/Authentication/AuthenticationIcon.qml` | Core-only primitive; retain |
| `qml/HoloNight/Components/ExternalIcon.qml`, `qml/HoloNight/CMakeLists.txt` | Compatibility URI Holonight.Components, Core-based icon wrapper; no standard controls or style bypass, preserve exported API and registration |
| `tests/qml/tst_AuthenticationDialog.qml` | Remove Basic import; qualify runtime controls and enums, preserve fake prompt model |

Already-qualified standard-control surfaces must participate in dual-style acceptance:
`apps/shell/qml/Popups/Tooltip/TooltipPopup.qml`,
`apps/shell/qml/Popups/Network/WifiPasswordDialog.qml`,
`apps/shell/qml/RightSidebar/SidebarTabBar.qml`,
`apps/shell/qml/RightSidebar/SidebarContent.qml`,
`apps/shell/qml/RightSidebar/Tabs/Media/SidebarMedia.qml`,
`apps/shell/qml/RightSidebar/Tabs/QuickSettings/SidebarQuickSettings.qml`,
`apps/shell/qml/RightSidebar/Tabs/System/DefaultAppRow.qml`,
`apps/shell/qml/RightSidebar/Tabs/System/SidebarSystem.qml`,
`apps/shell/qml/RightSidebar/Tabs/Notifications/SidebarNotifications.qml`,
`apps/shell/qml/RightSidebar/Tabs/Calendar/SidebarCalendar.qml`,
`apps/shell/qml/RightSidebar/Tabs/Overview/SidebarOverviewUpcoming.qml`,
`apps/shell/qml/RightSidebar/Tabs/Overview/SidebarOverview.qml`, and
`apps/shell/qml/Topbar/{LogoSection,ClockSection,ActiveWindowSection,KeyboardLayoutWidget,AudioWidget}.qml`.
Unused Controls imports may be removed. Other shell QML uses QtQuick primitives,
Core, local components or Holonight.Controls composites; scan all files in policy.

Preserve LauncherSearchField/HnSearchField, audio sliders/lists, sidebar composites,
HudFrame/BarFrame, palette and metrics APIs. Do not mechanically rename custom
`size`, `variant` or icon properties. IdentitySelector's 82-unit `delegateHeight`
is the identified style coupling: the pinned provider's
`qml/controls/HnIconComboBox.qml` exposes delegateHeight, maximumVisibleItems,
standard delegate/contentItem/indicator hooks and popup geometry across styles.
Use `iconRole: ""`, retain textRole/valueRole, the avatar row delegate, selected
identity synchronization and Return/Tab semantics. Bind the custom delegate height
to the public hook; retain the composite popup, never copy private provider geometry.
Custom painting remains application-owned in evidence even when its base control
resolves to Fusion. Access no Holonight.impl types or internal enums.

Add `scripts/check-qml-import-policy.py` and
`tests/test_qml_import_policy.py`; inspect apps, qml and test fixtures. Reject direct
Holonight (aliased/versioned too), competing style/Basic/Templates/impl imports,
unqualified standard instances, attached properties and enums, and direct-style
aliases. Explicit compatibility fixtures get individual documented allowlist entries;
production and normal tests have no exemptions. Parse comments/strings correctly,
permit genuine QtQuick enums such as TextInput.Password, local ActionButton and
public composite enums. Run each negative fixture alone to prove its own failure.
Correct the direct `import Holonight` theming advice in `CLAUDE.md`; clarify the
Core/composite/runtime split in `AGENTS.md` and `README.md`, including the obsolete
settings-app ownership wording. Preserve unrelated historical SDD records.

## Executable default and discovery

Add one `resources/qtquickcontrols2.conf`, registered with alias
`qtquickcontrols2.conf` at resource prefix `/`. Share registration through a small
`cmake/QuickControlsRuntime.cmake` helper. Apply it to `holonight-shell`,
`holonight-askpass-bin`, `holonight-askpass-test`, `holonight-polkit-agent`, graphical
GTest targets using `tests/main.cpp`, `test_holonight_qml_harness` and new compiled
acceptance targets. Link Qt6::QuickControls2 where used. Authentication aliases are
symlinks to the same libexec askpass binary, not separate resource owners.

Add private `libs/holonight-core/src/QuickControlsRuntime.h/.cpp`, wired by
`libs/holonight-core/CMakeLists.txt`, with no public provider API. Configure discovery
before any engine or standard control is created. Shell has multiple engines:
process-local QML discovery must reach all, including surface-owned engines.
Derive provider QML/native locations from configured package prefixes/targets,
validate the expected module directories, and remove fixed /tmp assumptions.
Only an executable at its exact configured build output path may add development
QML directories. Installed binaries compute the relative path from actual executable
location to configured libdir/qt6/qml using GNUInstallDirs; distinguish bin and
libexec, resolve symlinks for discovery while preserving argv[0] for askpass mode.
Support staged/relocated prefixes and configured libdir, not hardcoded ../lib.
Installed RPATH/native dependency discovery must likewise exclude build prefixes.
Preserve explicit caller QML paths; do not export build paths globally.

Wire `CMakeLists.txt`, `apps/shell/CMakeLists.txt`,
`apps/shell/main.cpp`, `apps/authentication/CMakeLists.txt`, both authentication
`main.cpp` files and `tests/CMakeLists.txt`, `tests/main.cpp`,
`tests/test_qml_harness.cpp`. Resources and discovery precede style resolution;
Qt retains selector precedence. Move askpass's extra-argument validation after Qt
consumes recognized options; explicitly regress prompt preservation, option-like
prompts and extra positional arguments. Keep core-dump/signal protections before UI
startup. Do not add an imperative fallback that hides a missing Holonight module.

Prepare pinned private dependency builds in `Taskfile.yml` and
`.github/workflows/ci.yml`; preserve existing system-services dependency and include
configuration before provider before shell. Inspect `.github/workflows/release.yml`
for matching dependency setup. Never use stale host headers or update lockfiles.

## Session routing and diagnostics

Keep `scripts/holonight-session` default-if-unset style/platform-theme behavior and
its D-Bus/systemd environment export list. Its installed prefix discovery serves
session-launched applications. Preserve SUDO_ASKPASS/SSH_ASKPASS alias paths from
`cmake/InstallIntegration.cmake.in`. The authentication binary independently derives
its discovery so standalone bin aliases and libexec launches work without a session.

Retain the verified cgroup and peer routes, ownership/socket/session validation,
allowlist and conflicting-global-environment rejection in
`scripts/holonight-polkit-agent-session` and
`scripts/holonight-wayland-session-environment`. Both already allow
QT_QUICK_CONTROLS_STYLE. Do not import arbitrary peer QML/plugin search paths or
weaken routing to solve discovery. A verified peer's explicit Fusion must survive;
missing style uses the executable default. The unit
`data/systemd/user/holonight-polkit-agent@.service.in` continues through the wrapper.
CLI selection is tested directly; the service wrapper remains a single-session-argument
interface. Isolated wrapper/activation tests capture exports instead of contacting
live systemd, logind or desktop launchers.

Private runtime diagnostics report frontend, phase and classification. Selection
reports effective QQuickStyle name and selector inputs without claiming loaded style.
Discovery reports searched roots/module availability without claiming rendering.
Loaded-control evidence comes from actual QML URL/type resolution and plugin paths
in isolated acceptance (QML_IMPORT_TRACE/QT_DEBUG_PLUGINS plus assertions on created
controls), with explicit Holonight/Fusion/Basic fallback/application-owned labels.
A plugin alone proves no individual control implementation. Missing module failure
names the module and checked paths with advice to install the matching provider or
correct its prefix. Fusion still requires Core/composite modules for owned visuals.
All askpass diagnostics go to stderr; stdout remains exclusively ProtocolWriter's
response channel. Shell uses its existing logger; authentication emits bounded,
redacted stderr classifications. No new public diagnostic protocol is required.
