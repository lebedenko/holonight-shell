# SDD Tasks — test-tooling

- [x] T-001: Implement debug stdout guard in WorkspaceModel::applyBatchUpdate()
  - REQs: REQ-F-001
  - Check: HOLONIGHT_DEBUG=1 env var presence triggers debug output after revision bump; no output when unset.

- [x] T-002: Add debug output formatter for workspace batch updates
  - REQs: REQ-F-001
  - Check: Debug output prints exactly one line per entry with format `[ws rev=N] id=X state=Y on_monitor=Z` to stdout.

- [x] T-003: Create test-env skill with environment checks
  - REQs: REQ-F-002, REQ-F-004
  - Check: Skill file at `.claude/skills/test-env/SKILL.md` exists with YAML front-matter, goal, and steps sections.

- [x] T-004: Implement test-env skill startup checks
  - REQs: REQ-F-002, REQ-F-004
  - Check: Skill steps execute in order: kill waybar, check WAYLAND_DISPLAY, print hyprctl cheat sheet, document urgency limitation, check ydotool availability.

- [x] T-005: Create pixel-check shell script with argument parsing
  - REQs: REQ-F-003
  - Check: Script at `scripts/check-pill.sh` accepts `--x`, `--y`, `--color`, `--tolerance` args and validates required flags before execution.

- [x] T-006: Implement pixel sampling and color comparison logic
  - REQs: REQ-F-003
  - Check: Script uses grim to sample pixel, extracts 6-char hex color (alpha stripped), compares with tolerance via bash arithmetic, exits 0 on match or 1 on mismatch.

- [x] T-007: Add startup validation to pixel-check script
  - REQs: REQ-F-003
  - Check: Script checks for grim, convert, WAYLAND_DISPLAY presence and exits with error if missing; documents grim coordinate system as compositor-space.

- [x] T-008: Make pixel-check script executable and portable
  - REQs: REQ-F-003
  - Check: Script has executable permissions (755), shebang is #!/bin/bash, works from any working directory.

- [x] T-009: Verify test-env skill is discoverable via Claude Code
  - REQs: REQ-F-002, REQ-NF-003
  - Check: Invoking `/test-env` skill from Claude Code loads and displays the skill without errors.
