# UQC-102: Unified QtQuick Controls adoption

Status: Implemented and locally verified; canonical CI publication in progress, 2026-09-09.
See [implementation evidence](IMPLEMENTATION.md).

Repository assignment: `holonight-shell` at published upstream
`723763e09ff815d344a6cb01529dd8345a43b316`. Required published provider:
`00e6e208b6c9b30d89b66ef3aeb4ef8175050764`; configuration:
`fe69a59e6b73167fd5349223a4d265d75386c139`.
The umbrella gitlinks remain authoritative. The design was published as
`5324b47fb01501d4fa3e90e7865b23065efce970` and coordinated in umbrella
`e3b85b5a339207ee421cb70f53311b1120f6b2c6` before this implementation.

See [design and file inventory](DESIGN.md) and [ordered work and verification](TASKS.md).
The accepted upstream contract is in the provider's
[implementation record](../../../../holonight-qt/docs/sdd/unified-qtquick-controls/IMPLEMENTATION.md).

## Requirements

- Use `import QtQuick.Controls as Controls` for standard types, attached properties
  and enums throughout shell, authentication and compatibility QML. Remove direct
  style imports and Basic imports; preserve Core primitives, composites, public
  component APIs, custom painting, accessibility and existing interaction behavior.
- Embed `:/qtquickcontrols2.conf` containing `[Controls]` and `Style=Holonight` in
  shell, askpass and polkit executables and matching graphical tests. Preserve
  explicit environment and Qt command-line selection, including CLI precedence.
  Do not introduce imperative production `QQuickStyle::setStyle()` calls.
- Discover provider QML for exact build executables and installed bin/libexec
  layouts without relying on the host installation or global development paths.
- Preserve session ownership/routing, polkit registration and cancellation,
  askpass basename dispatch, secret masking and stdout protocol. Propagate verified
  session style overrides through current wrappers and applicable activation paths.
- Report configured selection, module discovery and observed implementation loading
  as separate facts. Failures identify missing modules and useful discovery paths;
  no prompt, response, identity details or secret may enter style diagnostics.
- No new public provider API, configuration schema, authentication interface or
  compositor contract is planned. A provider gap becomes a separately coordinated,
  published prerequisite before dependent shell work proceeds.

## Acceptance matrix

Results and limitations are recorded in IMPLEMENTATION.md. Use separate processes for every selector:
(1) environment/CLI/config overrides unset → Holonight; (2) environment Fusion;
(3) CLI `-style Fusion` with environment unset; (4) CLI Fusion over environment
Holonight. Also cover an explicit external Controls configuration and missing style.

| Area | Required evidence |
|---|---|
| Each graphical entry point | Four selectors in actual build and staged-install launches; resource default present; bounded observation reaches QML/control creation, then terminates and reaps the process |
| Discovery | Host HoloNight QML and native libraries hidden; trace resolved QML URLs, representative control implementations and loaded provider plugins; installed runs reject source/build/private dependency paths |
| Authentication | Existing fake model/session backends: masked/revealed input, editing, submit, identity selection, retry, cancellation, queue/shutdown and disabled states under both styles; production process harness tests registration/teardown against a private authority |
| Askpass | Sudo/SSH/generic aliases, confirmation/notification, one secret plus newline only on success, empty stdout for cancellation/failure/diagnostics, invalid responses and SIGTERM; CLI options do not consume or reinterpret the prompt |
| Shell | Launcher editing/search, Wi-Fi password editing, sidebar and launcher scrolling, menus/tab navigation, first/last-item reachability, disabled controls and preserved Core/composite visuals under Holonight and Fusion |
| Geometry | Authentication account popup and shell popup/menu layouts at DPR 1 and 1.25, constrained windows and scales 0.78/1/1.25; popup bounds, actual delegate height, selection visibility and overflow scrolling |
| Wrappers | Verified cgroup and Wayland-peer routes retain explicit style, reject conflicting global session values, and launch with process-local discovery; capture terminal/launcher and D-Bus/systemd exports using fakes |
| Policy | Positive qualified controls/Core/composites; independently failing fixtures for every forbidden import/type/attached-property/enum pattern; no blanket exemption for tests |
| Quality | Focused checks, then full task test, QML lint/types, formatting, applicable architecture checks and green canonical remote CI |

Use disposable XDG/config/runtime directories, private buses without live activation,
fake services and unavailable native service endpoints. Shell executable observation
must run inside an isolated headless compositor or equivalent private harness; never
start a second shell against the active compositor. A failure before QML loads is not
style evidence. Source-QML harness success alone does not prove compiled production QML.
Use production resources in separate compiled acceptance targets with fake services.
Test-local property/signal/keyboard delivery must not move the desktop pointer or focus.
Live authentication, human-operated Hyprland/Sway, ecosystem activation and final
visual acceptance remain UQC-201. No system installation or live service action.
