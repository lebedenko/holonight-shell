# Development

`task build` and `task test` call CMake and CTest directly. Install dependencies first;
CMake reports missing packages and never downloads them. Use `cmake --preset dev
-DCMAKE_PREFIX_PATH=/path/to/prefix` for an additional installed provider prefix.
The development preset enables tests; `release` stages optimized builds.
clangd uses `build/dev/compile_commands.json`.

Native tests support installed providers in `/usr` or an explicit development
prefix. They require system Python 3 (`/usr/bin` or `/bin`), Bubblewrap, D-Bus and
Sway. CTest uses system Python independently of the interpreter selected for build
tools, including in existing CMake caches. Tests use disposable HOME/XDG directories,
a private D-Bus without host activation, and hidden host sockets. Installed providers
remain readable; sibling repositories and prefixes remain read-only, while the shell
and an explicitly supplied external build directory remain writable.

The launch test stages only the resolved Holonight QML tree and HoloNight Config
shared library, installs the shell there, then relocates the stage. Native installed
launches mask the original and system fallback providers in a separate namespace;
build launches retain their configured providers. The fresh acceptance container
uses its existing isolation and does not require nested Bubblewrap. Logs remain in
`build/dev/uqc-launch-logs/`. For an external build, use
`/usr/bin/python3 scripts/run-isolated-test.py --hide-host --build-dir /path/to/build
ctest --test-dir /path/to/build --output-on-failure`.

`task format` uses the pinned image. `task check` is the acceptance gate: fresh provider
builds, build, tests, format, static analysis, applicable QML checks and REUSE.
`task check:integration` adds Release staging and project runtime checks. GitHub invokes
the same launcher with `integration`; no additional validation is hidden in the workflow.
The only host launcher prerequisites are Python 3, Git, tar and Docker, plus Task for aliases.
Builds and clang-tidy use all available CPUs by default (`nproc` inside the container).
Set `JOBS=4 task check` to override the job count. Native component builds use Ninja
defaults; use `CMAKE_BUILD_PARALLEL_LEVEL=4 task build` to limit those.
Image, tool versions and the selected job count appear in the acceptance log. Logs and fresh builds stay under
ignored `build/acceptance/`; downloaded sources are reused under `build/sources/`.

Standalone checks automatically acquire the exact sources in `scripts/ci/providers.json`.
For simultaneous provider/consumer development, explicitly run
`task check -- --siblings /path/to/umbrella`. This uses current sibling files, including
edits. Final umbrella integration uses its gitlinks; never substitute sibling mode for it.
Provider sources are built with ordinary CMake into a disposable prefix. QMarkdown is
an independently installed package in the shared image and is not owned by our installer.

Routine fixes need tests and a commit explanation. Substantial designs use one SDD;
cross-project work uses one umbrella contract note. Keep execution logs out of Git.
