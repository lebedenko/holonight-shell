# Authentication dialog redesign verification — 2026-09-05

The shared dialog now follows `docs/mockups/askpass.png`, using the installed theme palette,
`HnSurfaceFrame`, and `HnKeyHint`. The Polkit account card uses local UID profiles with asynchronous
AccountsService enrichment (two calls, each limited to 1.5 seconds). Request-scoped callbacks
ignore completed, cancelled, or replaced requests. The authentication core still links only Qt Core.

Changes cover `qml/Authentication/`, authentication identity/prompt models and coordinator,
`apps/authentication/` frontend wiring and account resolver, and the related tests/CMake registration.
The existing authentication protocol and lifecycle tests remain passing.

| Check | Result |
| --- | --- |
| Focused authentication C++ executable | 40 tests passed, including profile enrichment, absent names, missing/unreadable avatars, stale callbacks and existing identity/protocol/process tests. |
| Focused AuthenticationDialog QML tests | 18 checks passed, including setup/cleanup, after the final sizing changes. Covers button/Return/keypad Enter submission, clearing before emission, reveal/reset, clipboard restrictions, Tab/Backtab, modes, identity selection, and overflowing content. |
| `task test` | First run: four existing window-activation tests conflicted with the live session bus. |
| `dbus-run-session -- task test` | All 1,136 tests passed in 180.37 seconds on an isolated bus. Final presentation-only refinements were followed by the focused QML tests. |
| `task qml-lint` | Exit 0; existing unresolved AudioService warnings in unrelated shell components. Authentication-only qmllint is clean, including the final layout. |
| `task qmltypes-check` | Shell and authentication metadata and packaging checks passed. |
| `task architecture-check` | Passed. |
| `task compositor-smoke-check` | Live Hyprland, control socket, monitors and tooling prerequisites passed; checklist printed. Unrelated shell popup/sidebar checks were not exercised. |
| Changed C++ files: clang-format; `git diff --check` | Passed. |

Screenshots use synthetic requests and an empty response field. They exercise the production QML
through the existing QtQuickTest harness. They do not represent a new real PAM authentication or
an agent replacement. AccountsService replies are covered by an injected asynchronous lookup;
the real D-Bus method/property names were checked against the installed AccountsService XML.

| Frontend | Normal, offscreen | Live Hyprland, DP-3 at 150% |
| --- | --- | --- |
| Polkit | [740 × 647](redesign-polkit.png) | [1110 × 971](redesign-polkit-150.png) |
| SSH askpass | [740 × 420](redesign-askpass.png) | [1110 × 630](redesign-askpass-150.png) |

Visual inspection confirmed the chamfered frame, header divider, text/card hierarchy, avatar
fallback, response outline, and fixed action area. The live capture exposed compositor-retained
excess height; explicit content-based maximum dimensions corrected it. At 150%, the final
Polkit capture retains its 740 × 647 logical-pixel layout, with physical-pixel rounding.
