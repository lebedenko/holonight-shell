# Repository Guidelines

Use Conventional Commits for every new commit: `type(scope): imperative summary`, or `type: imperative summary` when a scope adds no clarity.

## Project Structure & Module Organization

`apps/` contains executable-owned code. The shell entry point, app wiring, and QML module live under `apps/shell/`; authentication frontends live under `apps/authentication/`. Settings is owned by the separate holonight-settings repository. Reusable C++ targets live under `libs/`, such as `libs/holonight-core/`, `libs/holonight-services/`, and `libs/holonight-surfaces/`. Wayland protocol XML files are in `protocols/`, bundled SVG assets are in `assets/`, and design/spec notes are in `docs/sdd/`. Unit tests live in `tests/`.

## Build, Test, and Development Commands

Use `task` as the primary workflow:

- `task configure` configures a Debug CMake/Ninja build and writes `compile_commands.json`.
- `task build` builds `build/dev/holonight-shell`.
- `task run` builds and launches the shell; it requires a live Wayland/Hyprland session.
- `task test` configures tests, builds them, and runs `ctest --output-on-failure`.
- `task coverage` generates an HTML coverage report in `build/coverage/index.html`.
- `task format`, `task format-check`, `task tidy`, and `task qml-lint` run clang-format, clang-tidy, and qmllint checks.
- `task architecture-check` verifies lightweight target/include boundary rules.
- `task qmltypes-check` builds the executable and verifies generated `HolonightShell` qmltypes contain key C++ singletons.
- `task compositor-smoke-check` prints the live Hyprland smoke checklist for popup, sidebar, tray, notification, launcher, and widget checks.
- `task clean-artifacts` removes ignored local build directories and root log files after confirmation.
- `task clean` removes `build/` after confirmation.

## Coding Style & Naming Conventions

Follow the checked-in `.clang-format` and `.clang-tidy` rules: Google-based formatting, 2-space indentation, 120-column limit, and C++23. Include order is local headers first, then Qt headers, then system headers. Naming conventions are `CamelCase` for classes/types, `camelBack` for functions/methods, `lower_case` for members, and `lower_case_` for private members. QML files should be feature-scoped and registered in `CMakeLists.txt` with a `QT_RESOURCE_ALIAS`.

## Testing Guidelines

Tests use GTest for C++ and QtQuickTest for QML, driven through CTest.

- For C++, add new tests under `tests/` with descriptive `TEST` or `TEST_F` names, following patterns in `tests/test_workspace_model.cpp`.
- For QML, add new `TestCase` components under `tests/qml/tst_*.qml`. QML tests are executed offscreen (`QT_QPA_PLATFORM=offscreen`) by the `test_holonight_qml_harness` executable, leveraging unified mock services from `FakeQmlServices.h`.

Run `task test` before submitting behavior changes. For QML changes, also run `task qml-lint`; for compositor-facing shell behavior, run `task compositor-smoke-check` in a live Hyprland session and use `scripts/check-pill.sh` for targeted pixel checks when useful.

## Commit & Pull Request Guidelines

Recent history uses Conventional Commit-style prefixes such as `feat:`, `docs:`, and `chore:`. Keep subjects imperative and scoped to the change, for example `feat: add battery warning state`. Pull requests should describe the user-visible change, list validation commands run, link relevant issues or SDD tasks, and include screenshots or screencasts for UI changes.

## Agent-Specific Notes

Do not edit generated Wayland files; update XML in `protocols/` or CMake scanner configuration instead. Keep QML resource paths under `qrc:/HolonightShell/`. Follow project-specific gotchas in `CLAUDE.md` when touching layer shell, D-Bus, QML theming, or popup behavior.

When touching QML/CMake registration, run `task qmltypes-check` in addition to `task qml-lint`; an almost-empty `holonight-shell.qmltypes` can still build but break QML tooling. When touching target boundaries or service includes from `libs/holonight-surfaces/src/`, run `task architecture-check`.

Qt Quick standard controls use `import QtQuick.Controls as Controls`, including enums and attached properties.
Use `Holonight.Core` for palette/primitives and `Holonight.Controls` for composites. Graphical executables
embed an overridable Holonight default; do not import a concrete style or select one imperatively.



## Development and documentation

Use `task build` and `task test` for incremental native CMake/CTest work. Native builds
consume installed packages; use `CMAKE_PREFIX_PATH` for an explicit development prefix.
Use `task format` for the pinned formatter and `task check` for container acceptance.
Run `task check:integration` for installation/runtime changes. Container builds use
fresh build/prefix directories, separate from `build/dev` and `build/release`.
clangd reads `build/dev/compile_commands.json` directly.

Routine changes need tests and a commit explanation; update user documentation when
behavior changes. Substantial designs need one local SDD describing intent, decisions,
interfaces and acceptance. Cross-project changes need one umbrella note linking local
designs and stating contracts, dependency order and integration checks. Historical SDDs
remain historical. No separate planning, readiness, handoff or closure commits are required.
Keep routine logs and generated evidence in ignored build directories or CI artifacts.
Provider and consumer implementation may proceed together; publish providers before
consumer commits that reference them. Update umbrella pins per coherent milestone.
