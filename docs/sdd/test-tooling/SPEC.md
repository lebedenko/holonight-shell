# Test Tooling — Specification

**Session:** test-tooling  
**Status:** Specification Driven Development (EARS format)  
**Date:** 2026-05-21  

---

## Overview

Test tooling for `holonight-shell` provides debugging, environment validation, and pixel-level screenshot comparison capabilities to support manual and automated testing workflows during development. This spec defines four integrated tools: a debug stdout mode for WorkspaceModel, an executable test environment setup skill, a pixel-check script for visual regression detection, and ydotool integration documentation.

---

## Functional Requirements

### REQ-F-001: Debug Stdout Mode via HOLONIGHT_DEBUG

**GIVEN** the application is started with `HOLONIGHT_DEBUG=1` environment variable set,  
**WHEN** `WorkspaceModel::applyBatchUpdate()` is called during any workspace state transition,  
**THEN** the method prints to stdout exactly one line per workspace entry in the batch, using the format `[ws rev=N] id=X state=Y on_monitor=Z`, where:
- `N` is the workspace revision number (integer)
- `X` is the workspace ID (integer)
- `Y` is the `WorkspaceState` enum name: `Empty`, `Occupied`, `Active`, or `Urgent`
- `Z` is a bool: `true` or `false`

**Constraint:** The implementation uses `qgetenv("HOLONIGHT_DEBUG")` at the call site; no string formatting or output occurs when the env var is unset (zero-cost when disabled).

**Acceptance Criteria:**
- `qgetenv("HOLONIGHT_DEBUG")` check is placed before any printf/qDebug call
- Output format matches example exactly: `[ws rev=1] id=10 state=Active on_monitor=true`
- No output is produced when `HOLONIGHT_DEBUG` is unset or empty
- Output goes to stdout, not stderr
- A human tester can set `HOLONIGHT_DEBUG=1` and see live workspace updates in real time

---

### REQ-F-002: Test Environment Setup Skill (test-env)

**GIVEN** Claude invokes the `.claude/skills/test-env/SKILL.md` skill,  
**WHEN** the skill executes its embedded bash commands,  
**THEN** the skill performs the following steps in order:

1. **Kill waybar process** — if `waybar` is running (pid exists in `/proc`), kill it; if not running, report "waybar not running" without error
2. **Verify WAYLAND_DISPLAY** — check that `$WAYLAND_DISPLAY` is set; print its value or print "ERROR: WAYLAND_DISPLAY not set" and exit 1
3. **Print Hyprland cheat sheet** — output a reference list of basic `hyprctl` workspace manipulation commands:
   - `hyprctl dispatch focusmonitor <monitor-name>`
   - `hyprctl dispatch workspace <workspace-id>`
   - `hyprctl dispatch movetoworkspace <workspace-id>,<window-class>`
   - `hyprctl workspaces` (list all workspaces)
4. **Document urgency limitation** — print a warning that Hyprland v0.55.2 does not send the urgent bit to clients, so workspace urgency testing via the ExtWorkspaceManager protocol cannot verify visual urgency indicators

**Constraint:** The skill must be human-readable and executable by Claude; each step must print output to confirm it completed.

**Acceptance Criteria:**
- Skill is located at `.claude/skills/test-env/SKILL.md`
- Skill can be invoked via Claude's skill system (Skill tool)
- Waybar kill succeeds silently if waybar was running; reports status if not
- WAYLAND_DISPLAY check exits with error code 1 if unset; exits 0 if set
- Hyprland cheat sheet is printed to stdout in human-readable format
- Urgency limitation warning is clear and references Hyprland v0.55.2 specifically
- A developer can run this skill before manual testing to ensure environment is ready

---

### REQ-F-003: Pixel-Check Script

**GIVEN** the script `scripts/check-pill.sh` is invoked with arguments `--x X --y Y --color RRGGBB [--tolerance N]`,  
**WHEN** the script executes,  
**THEN** the script performs the following steps:

1. **Startup checks** — verify that `grim`, `convert`, and `WAYLAND_DISPLAY` are present:
   - If `grim` is missing, print "ERROR: grim not installed. Install with: pacman -S grim" and exit 1
   - If `convert` is missing, print "ERROR: convert not installed. Install with: pacman -S imagemagick" and exit 1
   - If `WAYLAND_DISPLAY` is not set, print "ERROR: WAYLAND_DISPLAY not set" and exit 1
2. **Parse arguments** — accept `--x`, `--y`, `--color`, and optional `--tolerance` (default 5)
3. **Screenshot pixel** — run `grim -g "X,Y 1x1" /tmp/pill-sample.png` to capture the 1x1 region at coordinates (X, Y)
4. **Sample and compare** — extract the pixel color from the PNG using `convert` and compare to expected color with tolerance threshold
5. **Report result** — if color matches (within tolerance), exit 0 and print "PASS: pixel at (X,Y) is RRGGBB"; if mismatch, exit 1 and print "FAIL: pixel at (X,Y) is RRGGBB_actual (expected RRGGBB_expected, tolerance=N)"

**Constraint:** The script documents its coordinate assumptions explicitly (e.g., "coordinates are relative to monitor top-left corner in pixels").

**Acceptance Criteria:**
- Script location: `scripts/check-pill.sh`
- Script is executable (mode 755)
- All three startup checks (grim, convert, WAYLAND_DISPLAY) are performed before any screenshot attempt
- Error messages include pacman install commands for missing dependencies
- Tolerance defaults to 5 if not supplied; custom tolerance can be passed via `--tolerance`
- Output format matches spec: "PASS: ..." or "FAIL: ..." with actual color, expected color, and tolerance printed
- Script does NOT hardcode pixel coordinates; all coordinates are supplied by caller via `--x` and `--y`
- Coordinate assumptions are documented in a comment block at the top of the script
- A tester can verify workspace pill color at arbitrary on-screen coordinates without editing the script

---

### REQ-F-004: ydotool Integration

**GIVEN** the test environment requires keyboard input simulation (e.g., workspace switching via `ydotool key`),  
**WHEN** a tester needs to use ydotool,  
**THEN** the implementation provides:

1. **Installation check** — a task or skill command that checks if `ydotool` is installed; if missing, prints "Install ydotool with: pacman -S ydotool" and exits 1
2. **Usage documentation** — the test-env skill (REQ-F-002) includes a section documenting basic ydotool usage:
   - Example: `ydotool key 49:1 49:0` (press and release Super key to open application menu or switch workspaces)
   - Example: `ydotool type "text"` (simulate typing)
   - Note: ydotool requires `/dev/uinput` access, typically via `sudo` (do not automate the sudo grant; document the requirement)
3. **Workspace switching example** — document the key sequence to cycle workspaces on Hyprland (typically Super+Page_Down for next, Super+Page_Up for previous, or Super+1..9 for specific workspace)

**Constraint:** ydotool requires `sudo` for `/dev/uinput` access; this constraint must be documented but not automated.

**Acceptance Criteria:**
- Installation check is present (via task or in test-env skill)
- ydotool usage examples are clear and include at least two commands (key injection, typing)
- Documentation notes that `sudo` is required for `/dev/uinput` and instructs users to test with `sudo ydotool` if needed
- Example key sequences for Hyprland workspace switching are provided
- A tester can use ydotool to automate workspace navigation during manual testing

---

## Non-Functional Requirements

### REQ-NF-001: Zero-Cost Debug Mode

**GIVEN** the application is built with `HOLONIGHT_DEBUG` unset in the environment,  
**WHEN** `WorkspaceModel::applyBatchUpdate()` is called,  
**THEN** no performance penalty occurs: no string formatting, no branching, no allocation.

**Acceptance Criteria:**
- `qgetenv("HOLONIGHT_DEBUG")` is checked once per call site, not inside loops
- All debug output code (printf, qDebug) is guarded by the check
- Release build disassembly shows no debug-related code paths
- A profiler run with HOLONIGHT_DEBUG unset shows no measurable overhead in applyBatchUpdate()

---

### REQ-NF-002: Script Portability

**GIVEN** the pixel-check script runs on Arch Linux with Wayland,  
**WHEN** dependencies are installed via pacman,  
**THEN** the script requires only Bash (>= 4.0) and the packages: `grim`, `imagemagick`.

**Acceptance Criteria:**
- Script is pure Bash with no Python or Perl dependencies
- Script runs on Arch Linux with Wayland compositor (Hyprland or similar)
- Error messages provide pacman install commands, not generic instructions

---

### REQ-NF-003: Skill Discoverability

**GIVEN** a developer opens Claude Code with this repository,  
**WHEN** they run the slash command `/test-env`,  
**THEN** the skill is immediately available without manual registration.

**Acceptance Criteria:**
- Skill YAML/MD metadata is present at `.claude/skills/test-env/SKILL.md`
- Skill name is `test-env` and is registered per Claude Code skill conventions
- Skill can be invoked via `/test-env` from the Claude Code CLI

---

## Conditional Requirements

### REQ-C-001: Tolerance Handling in Pixel Check

**GIVEN** the pixel-check script is invoked with `--color RRGGBB` and no `--tolerance` argument,  
**WHEN** the pixel is sampled,  
**THEN** a default tolerance of 5 (per-channel, e.g., ±5 in sRGB range 0–255) is used.

**GIVEN** the script is invoked with `--tolerance N`,  
**WHEN** the pixel is sampled,  
**THEN** the supplied tolerance N is used instead of the default.

**Acceptance Criteria:**
- Default tolerance of 5 is applied when `--tolerance` is omitted
- Supplied tolerance overrides default
- Tolerance is applied per-channel (e.g., R within ±5, G within ±5, B within ±5)
- Output message clearly shows the tolerance value used

---

### REQ-C-002: WAYLAND_DISPLAY Dependency Chain

**GIVEN** the test-env skill is invoked,  
**WHEN** WAYLAND_DISPLAY is not set,  
**THEN** the skill exits with error code 1 and prints "ERROR: WAYLAND_DISPLAY not set".

**GIVEN** the pixel-check script is invoked,  
**WHEN** WAYLAND_DISPLAY is not set,  
**THEN** the script exits with error code 1 and prints "ERROR: WAYLAND_DISPLAY not set".

**Acceptance Criteria:**
- Both tools check WAYLAND_DISPLAY before attempting any Wayland operations
- Error exit code is 1 in both cases
- Error message is identical or similarly clear in both tools

---

## Unwanted Behaviour Requirements

### REQ-UB-001: Debug Output Leakage

**UNWANTED:** A build is released to users with `HOLONIGHT_DEBUG=1` baked into the environment or code, causing workspace debug spam on stdout.

**TO PREVENT:** 
- Debug mode MUST be opt-in via environment variable only (no CMake compile-time default).
- Code review enforces that `HOLONIGHT_DEBUG` is never hardcoded or set in systemd unit files.
- Integration tests with `HOLONIGHT_DEBUG` unset confirm no debug output is produced.

**Acceptance Criteria:**
- `qgetenv("HOLONIGHT_DEBUG")` is checked at runtime; there is no `#ifdef DEBUG_ENABLED` hardcoding
- Build system does not set `HOLONIGHT_DEBUG` in environment or CMakeLists.txt
- Unset environment produces zero debug output (verified by piping stdout to /dev/null and confirming empty)

---

### REQ-UB-002: Silent Coordinate Hardcoding

**UNWANTED:** The pixel-check script hardcodes pill coordinates, making it useless for different bar layouts or screen resolutions.

**TO PREVENT:**
- Script rejects invocation without `--x` and `--y` arguments (prints usage and exits 1).
- Script documents coordinate assumptions in a comment block (e.g., "coordinates are relative to monitor top-left; adjust for bar position or DPI scaling").
- A tester must explicitly measure and pass pixel coordinates; no guessing.

**Acceptance Criteria:**
- Script exits with error if `--x` or `--y` is missing
- Usage message is clear: "Usage: check-pill.sh --x X --y Y --color RRGGBB [--tolerance N]"
- Script header contains a comment documenting coordinate assumptions
- Test documentation includes instructions to measure coordinates with `grim` or a color picker

---

### REQ-UB-003: Missing Dependency Silent Failure

**UNWANTED:** Pixel-check script or test-env skill fails cryptically due to missing `grim`, `convert`, or `WAYLAND_DISPLAY`.

**TO PREVENT:**
- All three prerequisites (grim, convert, WAYLAND_DISPLAY) are checked at startup, before any operation.
- Missing tool errors include pacman install command.
- Missing WAYLAND_DISPLAY error is clear and distinguishes it from missing binaries.

**Acceptance Criteria:**
- Script starts with three sequential checks: `command -v grim || exit 1`, `command -v convert || exit 1`, `test -n "$WAYLAND_DISPLAY" || exit 1`
- Each check prints a specific error message and exits 1 if it fails
- A user running the script without dependencies sees immediate, actionable error messages

---

### REQ-UB-004: Bar Layout Geometry Breakage Detection

**UNWANTED:** Topbar layout changes (e.g., bar height, pill position) break existing pixel-check tests silently, producing false negatives or false positives without alerting the tester.

**TO PREVENT:**
- Test-env skill documents current bar geometry (height, pill position relative to screen top-left) in a comment or output section.
- Script header includes a "Known coordinates" section or a pointer to current bar geometry documentation.
- When geometry changes, developer must update documentation and notify test suite maintainers.

**Acceptance Criteria:**
- Test-env skill output or documentation includes current bar geometry (e.g., "topbar height: 32px, workspace pill starts at pixel Y=8 on monitor primary")
- Script header includes a "Coordinate notes" or "Bar geometry" section pointing to current layout
- Documentation is version-controlled and visible to code reviewers
- A geometry change (e.g., bar height +5px) forces an explicit documentation update before CI passes

---

### REQ-UB-005: Ydotool Sudo Pitfall

**UNWANTED:** User runs ydotool commands without `sudo` and gets a cryptic `/dev/uinput: Permission denied` error, then wastes time debugging.

**TO PREVENT:**
- Test-env skill documentation explicitly states: "ydotool requires `sudo` to access `/dev/uinput`. Use: `sudo ydotool key ...`"
- Example commands in documentation include `sudo ydotool ...` prefix.
- No attempt is made to automate `sudo` grant (e.g., passwordless sudo config); that is user responsibility.

**Acceptance Criteria:**
- Documentation includes the phrase "requires `sudo` to access `/dev/uinput`"
- At least two example commands show `sudo ydotool ...` (with and without sudo documented separately)
- A new user following examples sees the `sudo` requirement up front

---

## Reference

**Related memories:**
- `mem:arch-layer-shell` — QWaylandClientExtensionTemplate pattern, surface role pitfalls
- `mem:arch-workspace-model` — WorkspaceModel internal architecture
- `mem:arch-ext-workspace-manager` — ExtWorkspaceManager protocol integration
- `mem:design-workspace-pill` — workspace pill visual spec and layout

**External references:**
- Hyprland v0.55.2 urgency limitation documented in `mem:arch-ext-workspace-manager`
- `grim` Wayland screenshot tool: https://github.com/emersion/grim
- `imagemagick convert`: https://imagemagick.org/script/convert.php
- `ydotool` keyboard input: https://github.com/ReimuNotMoe/ydotool

