# Test Tooling — Architecture Design

**Session:** test-tooling  
**Status:** Design  
**Date:** 2026-05-21  

---

## 1. Component Overview

This session touches four distinct deliverables. None share code, but they compose into a single manual-testing workflow.

| Deliverable | File(s) | Status |
|---|---|---|
| Debug stdout in `WorkspaceModel` | `src/WorkspaceModel.cpp` | Modify existing |
| Test environment skill | `.claude/skills/test-env/SKILL.md` | New file |
| Pixel-check script | `scripts/check-pill.sh` | New file (new dir) |
| ydotool reference | Embedded in test-env skill | Part of skill |

No new C++ types, no new CMake targets, no new QML files. The `scripts/` directory must be created.

---

## 2. Debug Stdout Mode (REQ-F-001, REQ-NF-001, REQ-UB-001)

### 2.1 Call Site Location

The guard and output go into `WorkspaceModel::applyBatchUpdate()` in `src/WorkspaceModel.cpp`, immediately after the `++revision_` / `emit revisionChanged()` lines — the last two lines of the function. This placement ensures:

- `revision_` already holds the new value when the line is printed, so `rev=N` is consistent with what QML observes.
- The print happens after the model is updated; a tester reading stdout sees the state that QML will also read.
- It is outside all `if/else` branches, so it fires regardless of whether a reset or a `dataChanged` path was taken.

### 2.2 Guard Pattern

```cpp
if (!qgetenv("HOLONIGHT_DEBUG").isEmpty()) {
    // print loop here
}
```

`qgetenv` returns a `QByteArray`. Checking `.isEmpty()` (not `.isNull()`) correctly gates on both unset and empty-string cases. The check is placed at the call site, not in a constructor or a config singleton, so there is no state to carry and no initialization order dependency. When the env var is absent, the branch is not taken and no heap allocation occurs beyond the `qgetenv` call itself (which Qt optimises to a single `getenv(3)` call returning null).

### 2.3 State-to-String Helper

`WorkspaceState` is a scoped enum (`enum class WorkspaceState : uint8_t`) declared inside `WorkspaceModel`. It is registered with `Q_ENUM`, but `QMetaEnum::fromType` would require a round-trip through the meta-object system at debug time. A static constexpr lookup is cheaper and has no external dependencies:

```cpp
// local lambda inside the guard block — no linkage, zero overhead when disabled
auto stateStr = [](WorkspaceState s) -> const char* {
    switch (s) {
        case WorkspaceState::Empty:    return "Empty";
        case WorkspaceState::Occupied: return "Occupied";
        case WorkspaceState::Active:   return "Active";
        case WorkspaceState::Urgent:   return "Urgent";
    }
    return "Unknown";
};
```

The lambda is defined inside the `if (!qgetenv(...).isEmpty())` block so the compiler can eliminate it entirely in the unset case.

### 2.4 Output Mechanism

Output goes to stdout via `printf`. Alternatives considered:

- `qDebug()` — routes to Qt's message handler; may be suppressed, redirected, or decorated by the application's `qInstallMessageHandler`. Not appropriate for structured machine-readable output.
- `fprintf(stderr, ...)` — stderr is useful for errors, not live state inspection. A tester piping `HOLONIGHT_DEBUG=1 task run | grep '\[ws'` would need to redirect stderr. stdout is the right choice.
- `std::cout` — works, but mixing Qt and `std::cout` with Qt's stream buffers can cause interleaving. `printf` with `\n` flush is simpler and unambiguous.

Output format (one line per entry):

```
[ws rev=1] id=10 state=Active on_monitor=true
```

Produced by:
```cpp
for (const auto& entry : rows_) {
    printf("[ws rev=%d] id=%d state=%s on_monitor=%s\n",
           revision_,
           entry.id,
           stateStr(entry.state),
           entry.on_monitor ? "true" : "false");
}
```

### 2.5 Data Flow Diagram

```
ExtWorkspaceManager::onWorkspaceEvent()
        │
        ▼
WorkspaceModel::applyBatchUpdate(entries)
        │
        ├── beginResetModel() / dataChanged()   ← model update
        ├── rows_ = entries
        ├── endResetModel() / emit dataChanged
        ├── ++revision_
        ├── emit revisionChanged()
        │
        └── if (!qgetenv("HOLONIGHT_DEBUG").isEmpty())
                │
                └── for each entry in rows_:
                        printf("[ws rev=%d] id=%d state=%s on_monitor=%s\n")
                                │
                                ▼
                            stdout
```

---

## 3. Test Environment Skill (REQ-F-002, REQ-F-004, REQ-NF-003)

### 3.1 File Location and Format

`.claude/skills/test-env/SKILL.md`

Follows the same format as `.claude/skills/build/SKILL.md`: a YAML front-matter block with `name` and `description`, then `## Goal` and `## Steps` sections containing numbered steps with inline bash fenced code blocks that Claude executes directly.

### 3.2 Skill Steps

**Step 1 — Kill waybar**

```bash
pkill waybar && echo "waybar killed" || echo "waybar not running"
```

`pkill` exits 1 if no process matches; the `||` branch converts that to a non-error message. This is preferable to `kill $(pgrep waybar)` because `pgrep` failing and returning empty would cause `kill` to emit an error.

**Step 2 — Verify WAYLAND_DISPLAY**

```bash
if [ -z "$WAYLAND_DISPLAY" ]; then
    echo "ERROR: WAYLAND_DISPLAY not set"
    exit 1
fi
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY"
```

**Step 3 — Hyprland workspace cheat sheet**

Printed inline in the skill output. Includes:
- `hyprctl dispatch workspace N` — switch to workspace N
- `hyprctl dispatch movetoworkspace N` — move focused window to N
- `hyprctl workspaces` — list all workspaces with IDs and state
- `hyprctl activewindow` — print focused window info

**Step 4 — Urgency limitation warning**

```
WARNING: Hyprland v0.55.2 does not send the ext-workspace urgent bit to clients.
WorkspaceState::Urgent cannot be triggered via Hyprland workspace events alone.
To test urgent state, call WorkspaceModel directly from a test harness or stub the ExtWorkspaceManager.
```

**Step 5 — ydotool reference (REQ-F-004)**

```bash
# Check if ydotool is installed
which ydotool || echo "Install with: sudo pacman -S ydotool"
```

Followed by inline documentation:

```
ydotool usage notes:
- Requires ydotoold daemon: sudo ydotoold &
- Or run directly with sudo: sudo ydotool key ...
- Requires /dev/uinput access (group 'input' or sudo)

Hyprland workspace switching (Super+1 through Super+9):
  sudo ydotool key 125:1 2:1 2:0 125:0     # Super+1 (workspace 1)
  sudo ydotool key 125:1 3:1 3:0 125:0     # Super+2 (workspace 2)
  ...
  (key codes: 125=Super_L, 2=1, 3=2, ... 10=9)
```

### 3.3 Skill Invocation

Invoked via `/test-env` in a Claude Code session. Claude reads the SKILL.md and executes each bash block using its Bash tool. The skill is not a script run by the user directly — it is a Claude-executable recipe.

---

## 4. Pixel-Check Script (REQ-F-003, REQ-NF-002, REQ-UB-002, REQ-UB-003)

### 4.1 File Location

`scripts/check-pill.sh` (executable, `chmod +x`). The `scripts/` directory is new.

### 4.2 Interface Specification

**Invocation:**

```
scripts/check-pill.sh --x X --y Y --color RRGGBB [--tolerance N]
```

**Arguments:**

| Argument | Required | Description |
|---|---|---|
| `--x X` | Yes | Pixel X coordinate on the Wayland output |
| `--y Y` | Yes | Pixel Y coordinate on the Wayland output |
| `--color RRGGBB` | Yes | Expected color as 6-hex-digit string (no `#`) |
| `--tolerance N` | No | Max per-channel delta, default 5 |

**Exit codes:**

| Code | Meaning |
|---|---|
| 0 | PASS — actual color within tolerance of expected |
| 1 | FAIL — color mismatch, or missing dependency, or missing required argument |

**Stdout on success:**

```
PASS: pixel at (X,Y) = #AABBCC (expected #RRGGBB, tolerance 5)
```

**Stdout on failure:**

```
FAIL: pixel at (X,Y) = #DDEEFF (expected #RRGGBB, tolerance 5)
```

Or for missing argument:
```
ERROR: --x is required
Usage: check-pill.sh --x X --y Y --color RRGGBB [--tolerance N]
```

### 4.3 Implementation Flow

```
check-pill.sh
    │
    ├── [Startup checks]
    │       ├── command -v grim   || { echo "ERROR: grim not found"; exit 1; }
    │       ├── command -v convert || { echo "ERROR: imagemagick (convert) not found"; exit 1; }
    │       └── [ -n "$WAYLAND_DISPLAY" ] || { echo "ERROR: WAYLAND_DISPLAY not set"; exit 1; }
    │
    ├── [Argument parsing]
    │       └── while loop consuming --x --y --color --tolerance
    │           error-exits on missing --x, --y, or --color after loop
    │
    ├── [Screenshot]
    │       └── grim -g "${x},${y} 1x1" /tmp/pill-sample.png
    │
    ├── [Color extraction]
    │       └── actual=$(convert /tmp/pill-sample.png -format "%[hex:u]" info:)
    │           # strips alpha: actual=${actual:0:6}
    │
    ├── [Tolerance comparison]
    │       └── per-channel arithmetic in bash:
    │           r_diff = abs(actual_r - expected_r)
    │           g_diff, b_diff same
    │           pass if all diffs <= tolerance
    │
    └── [Report + exit]
            ├── PASS → exit 0
            └── FAIL → exit 1
```

### 4.4 Temp File

`/tmp/pill-sample.png` is used as the scratch file. It is overwritten on every invocation. It is not cleaned up (intentional: leaves the sample available for manual inspection after a FAIL). The path is hardcoded in the script but is not an argument because it is an implementation detail, not a user-facing coordinate.

### 4.5 Color Extraction Detail

`convert /tmp/pill-sample.png -format "%[hex:u]" info:` returns an 8-character hex string (RRGGBBAA) from ImageMagick. The script slices `${actual:0:6}` to discard the alpha channel before comparison. This is required because `grim` may write a PNG with an alpha channel even when capturing an opaque surface.

### 4.6 Tolerance Comparison

Pure bash arithmetic (no `bc`, no Python):

```bash
r_actual=$((16#${actual:0:2}))
g_actual=$((16#${actual:2:2}))
b_actual=$((16#${actual:4:2}))
r_expected=$((16#${color:0:2}))
g_expected=$((16#${color:2:2}))
b_expected=$((16#${color:4:2}))

abs_diff() { local d=$(( $1 - $2 )); echo $(( d < 0 ? -d : d )); }

r_diff=$(abs_diff $r_actual $r_expected)
g_diff=$(abs_diff $g_actual $g_expected)
b_diff=$(abs_diff $b_actual $b_expected)

if (( r_diff <= tolerance && g_diff <= tolerance && b_diff <= tolerance )); then
    echo "PASS: ..."
    exit 0
else
    echo "FAIL: ..."
    exit 1
fi
```

---

## 5. Key Decisions and Rationale

### 5.1 Environment Variable, Not Compile Flag

The spec considered `#ifdef HOLONIGHT_DEBUG` as an alternative. Rejected because:

- A compile flag requires reconfiguring and rebuilding to toggle debug output. The env-var approach lets a tester flip the output on the running binary without touching the build.
- CI can run the binary with and without the flag without a second build artifact.
- The check cost is one `getenv(3)` call per `applyBatchUpdate` invocation. Workspace updates happen at human interaction speed (keypress frequency), not in hot loops, so the overhead is immeasurable.

### 5.2 Caller-Supplied Coordinates, No Hardcoded Constants

The pixel-check script deliberately refuses to know about topbar geometry. Hardcoded coordinates would break whenever bar height, padding, or monitor resolution changes. The caller (human tester or a future wrapper script) is responsible for knowing where a pill is. This keeps the script stable as a utility across multiple visual sessions.

### 5.3 Executable Skill, Not Reference Documentation

The test-env content could have been written as a plain Markdown file in `docs/`. A skill is chosen because:

- Claude can execute the bash steps directly inside a session, reducing context-switching for the tester.
- The waybar kill and WAYLAND_DISPLAY check are side-effectful — they need to run in the live environment, not just be read.
- The skill format makes the steps verifiable: Claude either successfully executes each block or reports an error, giving the tester immediate confirmation.

### 5.4 printf Over qDebug for Debug Output

`qDebug()` is routed through Qt's message handler infrastructure. In production builds with a custom message handler (or if `QT_LOGGING_RULES` suppresses categories), `qDebug()` output could silently disappear. `printf` to stdout is unconditional and does not interact with Qt's logging system, making it reliable for live observation.

### 5.5 Post-revision Placement of Debug Print

The debug output is placed after `++revision_` and `emit revisionChanged()`, not before. If placed before, the printed `rev=N` would be the value from the previous batch (revision has not yet incremented). Printing after increment means the rev number in stdout matches what QML's `WorkspaceModel.revision` property will report in the same event loop turn.

---

## 6. Alternatives Considered

| Alternative | Reason Rejected |
|---|---|
| `QLoggingCategory` for debug output | Adds a new category string, suppressible via `QT_LOGGING_RULES`, and outputs to stderr by default. Over-engineered for a one-field debug aid. |
| `#ifdef HOLONIGHT_DEBUG` compile flag | Requires rebuild to toggle; env var is strictly more flexible. |
| Python script for pixel check | Requires Python and Pillow; pure Bash + grim + ImageMagick is the already-established Wayland screenshot stack on Arch. |
| Separate `ydotool` skill file | ydotool docs are too short to justify a separate invocation point; embedding in test-env keeps the test setup workflow in one place. |
| Cleaning up `/tmp/pill-sample.png` | Left intentionally for post-failure inspection. A failed test is more useful when the actual screenshot survives. |
| Reading color via `wl-paste` or `wlrctl` | `grim + convert` is the canonical approach; consistent with what other Wayland screenshot tools use. |

---

## 7. Known Risks

### R-1: grim coordinate system vs. multi-monitor

`grim -g "X,Y 1x1"` uses compositor-space coordinates. On a multi-monitor setup, X and Y are relative to the global compositor coordinate space, not the monitor origin. A tester must supply compositor-space coordinates (e.g., obtained from `hyprctl monitors` or by noting the bar's position in the global layout). The script does not validate or translate coordinates — this is documented in the skill.

### R-2: Hyprland urgent bit (known limitation, pre-existing)

`WorkspaceState::Urgent` cannot be triggered via normal Hyprland workspace events in v0.55.2 because Hyprland does not emit the urgent bit. The test-env skill documents this explicitly. Testing the Urgent visual state requires either a future Hyprland fix or a test harness that calls `applyBatchUpdate` with a synthetic `Urgent` entry.

### R-3: ImageMagick `%[hex:u]` format portability

The `%[hex:u]` pixel format string is an ImageMagick 7 feature. ImageMagick 6 (legacy) uses `%[pixel:u]` which returns a different format. Arch Linux ships ImageMagick 7 by default; the script does not need a version guard, but a note in the skill is appropriate.

### R-4: ydotool keycodes are layout-dependent

The ydotool key sequences for `Super+N` use evdev keycodes that correspond to the standard US keyboard layout. Non-US layouts or custom keymaps may produce different results. Documented in the skill with a note to verify against `xev` or `evtest` if key sequences do not work.

### R-5: `/tmp/pill-sample.png` collision in concurrent runs

If two instances of `check-pill.sh` run simultaneously, they share the same temp file. This is not a concern for the current manual testing workflow, but would be a problem in parallel CI. If CI use is ever needed, the script should use `mktemp` instead. Deferred as out of scope for this session.
