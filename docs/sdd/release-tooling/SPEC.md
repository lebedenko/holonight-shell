# Release Tooling for holonight-shell 0.1.0

## Overview

This specification defines the release tooling and workflow for the holonight-shell v0.1.0 initial release. The release process is automated where appropriate and produces a source-only distribution via git archive, with a GitHub Release workflow gated by a successful build. The version is maintained in a single authoritative location (CMake's `project()` version directive) and plumbed into the binary and release artifacts via code generation and CI automation.

---

## Scope

**In Scope:**
- `--version` / `-V` flag in the `holonight-shell` binary (prints version and exits without starting GUI)
- Single-source-of-truth version in `CMakeLists.txt` (plumbed to a generated `version.h`)
- Release helper script (`scripts/release.sh`) for local version bumping and tag creation
- GitHub Actions CI release workflow triggered on tag push (`v*` pattern)
- Release documentation (`RELEASING.md`) with repeatable release process for maintainers
- Hand-written `CHANGELOG.md` in "Keep a Changelog" format with 0.1.0 entry
- Source tarball artifact (`holonight-shell-X.Y.Z.tar.gz`) attached to GitHub Release

**Out of Scope:**
- Package manager publishing (copr, Flathub, Homebrew, etc.)
- Prebuilt binary distributions (AppImage, Flatpak, or OS-specific packages)
- Automated changelog generation from git history (0.1.0 is hand-written; future releases document the Conventional Commits automation expectation)
- Windows or macOS release artifacts or platform-specific workflows

---

## Functional Requirements

### REQ-F-001: Version Flag Output

**Requirement:** The holonight-shell binary shall accept `--version` or `-V` as a command-line argument and shall print the version to stdout in the format `holonight-shell <VERSION>` followed by a newline.

**Acceptance Criteria:**
- Running `./build/holonight-shell --version` outputs exactly `holonight-shell 0.1.0\n` to stdout
- Running `./build/holonight-shell -V` outputs exactly `holonight-shell 0.1.0\n` to stdout
- The process exits with code 0
- No Qt GUI window or Wayland surface is created
- The exit occurs within 1 second of invocation

### REQ-F-002: Version Flag Early Exit

**Requirement:** When the `--version` or `-V` flag is detected, the main() function shall handle it in the existing manual argv loop before any Qt application initialization or GUI setup.

**Acceptance Criteria:**
- The flag is processed in `src/main.cpp` within the manual argv parsing loop (alongside `--toggle-launcher`)
- No `QApplication` constructor or `QQuickView` show() is executed when the flag is present
- Exit happens via `return 0` or `std::exit(0)` before reaching any GUI initialization code
- Code inspection confirms no Wayland surface creation attempt occurs

### REQ-F-003: Single Source of Truth for Version

**Requirement:** The version shall be maintained exclusively in the CMake `project(holonight-shell VERSION X.Y.Z)` directive in `CMakeLists.txt`, and no other source file or build artifact shall contain a hardcoded version string.

**Acceptance Criteria:**
- The version integer is set in CMakeLists.txt via `project(holonight-shell VERSION X.Y.Z, ...)`
- A CMake `configure_file()` call (following the pattern at `CMakeLists.txt:504`) generates a `version.h` header with a `HOLONIGHT_SHELL_VERSION` C-string macro
- The generated `version.h` is included in `src/main.cpp` (or appropriate source file) to populate the `--version` output
- A grep search for the literal version string (e.g., `"0.1.0"`) across `src/*.cpp`, `src/*.h`, and `CMakeLists.txt` (excluding generated files and `version.h.in`) returns no matches outside the `project()` line
- The source tarball filename is derived from the CMake version via git archive naming, not hardcoded in CI

### REQ-F-004: Release Helper Script Argument Validation

**Requirement:** The release helper script `scripts/release.sh` shall require exactly one argument (the target version) in `X.Y.Z` format and shall refuse execution with an error message if the argument is missing or malformed.

**Acceptance Criteria:**
- Running `scripts/release.sh` with no arguments exits with code 1 and prints an error message to stderr
- Running `scripts/release.sh 0.1.0` proceeds (valid semver)
- Running `scripts/release.sh 0.1` exits with code 1 and prints an error message
- Running `scripts/release.sh 0.1.0-rc1` exits with code 1 and prints an error message
- The script validates the format with a regex or explicit check; error message is clear and actionable

### REQ-F-005: Release Helper Script Dirty Tree Check

**Requirement:** When the release script is invoked, the system shall check that the working tree is clean (all files committed, no uncommitted changes or untracked files in source directories) and shall refuse to proceed if the tree is dirty.

**Acceptance Criteria:**
- `git status --porcelain` returns empty (or only expected ignored files) before the script proceeds
- If any uncommitted changes exist in `src/`, `CMakeLists.txt`, `CHANGELOG.md`, or `scripts/`, the script exits with code 1 and prints an error message instructing the user to commit or stash changes
- If the check passes, the script prints a confirmation message and continues
- Untracked files in `build/` or `.git/` do not trigger a failure (only source and config directories matter)

### REQ-F-006: Release Helper Script CMakeLists.txt Update

**Requirement:** The release script shall locate the `project(holonight-shell VERSION ...)` line in `CMakeLists.txt`, update the version to the target version, and verify the update was successful.

**Acceptance Criteria:**
- The script uses a tightly-anchored sed command (or equivalent text edit) that matches `project(holonight-shell VERSION` and replaces only the version number
- After the edit, a verification step re-reads the file and confirms the new version appears in the `project()` line
- If the verification fails, the script prints an error and exits with code 1 (without proceeding to commit or tag)
- The updated line reads `project(holonight-shell VERSION X.Y.Z ...)` with the new version
- No other lines in the file are modified by the edit

### REQ-F-007: Release Helper Script CHANGELOG.md Verification

**Requirement:** The release script shall verify that `CHANGELOG.md` exists and contains an entry (section header) for the target version, and shall prompt the user or refuse if the changelog is not up-to-date.

**Acceptance Criteria:**
- The script checks for the existence of `CHANGELOG.md`
- The script searches for a line matching the pattern `## [X.Y.Z]` or `# X.Y.Z` (as per "Keep a Changelog" format) in the file
- If the version entry exists, the script prints a confirmation and continues
- If the version entry is missing, the script prints an error message indicating that the CHANGELOG must be updated manually before running the release script, and exits with code 1
- The script does NOT auto-generate or append changelog entries; the user is responsible for adding the entry beforehand

### REQ-F-008: Release Helper Script Git Tag Creation

**Requirement:** The release script shall create an annotated git tag with the name `vX.Y.Z` (where X.Y.Z matches the target version) with a clear commit message.

**Acceptance Criteria:**
- After successfully updating CMakeLists.txt and verifying the changelog, the script creates a new annotated tag via `git tag -a vX.Y.Z -m "Release vX.Y.Z"`
- The tag command includes a message (not lightweight); `git cat-file -t vX.Y.Z` returns `tag` (not `commit`)
- The tag is created on the current HEAD (no implicit checkout or rebase)
- If the tag already exists, the script prints an error and exits with code 1 (does not force-overwrite)
- `git tag -l | grep "^vX.Y.Z$"` returns the new tag after the script completes

### REQ-F-009: Release Helper Script Publish Instructions

**Requirement:** The release script shall output next steps (push tag and create GitHub Release) as printed instructions, either behind an optional `--publish` flag or as manual follow-up commands, and shall not push to remote by default.

**Acceptance Criteria:**
- The script completes locally without invoking `git push` unless an explicit `--publish` flag is provided
- If `--publish` is NOT set, the script prints a message like: `To publish this release, run: git push origin vX.Y.Z && gh release create vX.Y.Z ...`
- If `--publish` IS set, the script runs `git push origin vX.Y.Z` and `gh release create vX.Y.Z --generate-notes` (or similar command to attach the tarball)
- The script does not silently push to remote in the default case
- The message is clear and can be copy-pasted by the user

### REQ-F-010: CI Release Workflow Trigger

**Requirement:** A GitHub Actions workflow shall be triggered automatically when a git tag matching the pattern `v*` is pushed to the repository.

**Acceptance Criteria:**
- The workflow is defined in `.github/workflows/release.yml` (or similar)
- The workflow is triggered by the `push` event with `tags: 'v*'` filter (or glob pattern equivalent)
- Pushing `v0.1.0` to the remote triggers the workflow
- Pushing a branch (non-tag) does not trigger the workflow
- The workflow name and trigger are documented in `RELEASING.md`

### REQ-F-011: CI Release Workflow Build Gate

**Requirement:** Before packaging and publishing a release, the CI workflow shall build the project using the same approach as the main CI pipeline, and shall abort the release if the build fails.

**Acceptance Criteria:**
- The release workflow reuses the build steps from `.github/workflows/ci.yml` (configure, build, test, lint) on `ubuntu-24.04`
- If any build step fails (compile error, test failure, lint warning), the workflow stops and does NOT proceed to packaging or GitHub Release creation
- The workflow logs and any error messages are visible in the GitHub Actions UI
- A failed release build prevents any artifacts from being published
- The workflow does not use `continue-on-error: true` for build/test steps (failures must halt the pipeline)

### REQ-F-012: CI Release Workflow Source Tarball Creation

**Requirement:** After a successful build, the CI workflow shall create a source tarball named `holonight-shell-X.Y.Z.tar.gz` (where X.Y.Z is extracted from the git tag) using `git archive`.

**Acceptance Criteria:**
- The workflow extracts the version from the tag name (e.g., `v0.1.0` → `0.1.0`) via bash string manipulation or git commands
- The workflow runs `git archive --format=tar.gz --prefix=holonight-shell-X.Y.Z/ -o holonight-shell-X.Y.Z.tar.gz vX.Y.Z` (or equivalent)
- The resulting tarball is placed in the workflow's artifact directory
- The tarball can be extracted and contains the full source tree with all files from the tag
- The filename matches the version extracted from the tag (verified by listing the artifact)

### REQ-F-013: CI Release Workflow GitHub Release Creation

**Requirement:** After successfully building and packaging, the CI workflow shall create a GitHub Release for the tag and attach the source tarball as a release asset.

**Acceptance Criteria:**
- The workflow uses `gh release create vX.Y.Z` (or `gh release create --repo owner/repo vX.Y.Z`) to create the release
- The tarball is attached as a release asset via the `gh release upload` command or the `--title` and file arguments to `gh release create`
- The GitHub Release is visible on the project's Releases page
- The tarball is downloadable from the release page
- The release is marked as "draft: false" (published)
- The release description includes the version and links to CHANGELOG.md (or is auto-generated from `--generate-notes`)

### REQ-F-014: RELEASING.md Documentation

**Requirement:** A `RELEASING.md` file shall be created at the repository root documenting the release process, target audience, and expectations for future releases.

**Acceptance Criteria:**
- The file exists at `/home/andrii/projects/holonight-shell/RELEASING.md`
- The document is written in Markdown and is approximately 1–2 pages
- The document covers:
  - **Target audience:** Project maintainers (who cut releases)
  - **Prerequisites:** Git, gh CLI, CMake, task runner (already installed)
  - **Step-by-step release process:** manual steps + the `scripts/release.sh` script + push/publish commands
  - **The version single-source-of-truth:** CMakeLists.txt as the authoritative version source
  - **CHANGELOG expectations:** Hand-written for 0.1.0, Conventional Commits-based for future releases
  - **Tag format:** `vX.Y.Z`
  - **CI automation:** How the GitHub Actions workflow builds and publishes on tag push
  - **Troubleshooting:** What to do if a tag build fails (delete the tag, fix the issue, try again)
- The document is readable and actionable (not just a summary of requirements)

### REQ-F-015: CHANGELOG.md for 0.1.0

**Requirement:** A `CHANGELOG.md` file shall exist at the repository root following the "Keep a Changelog" format, with an entry for version 0.1.0 clearly marked as "Initial release."

**Acceptance Criteria:**
- The file exists at `/home/andrii/projects/holonight-shell/CHANGELOG.md`
- The file follows the "Keep a Changelog" format (https://keepachangelog.com/)
- The topmost entry is `## [0.1.0] - 2026-06-03` (or similar date format)
- The 0.1.0 section contains a single high-level entry such as "Initial release of holonight-shell" or minimal feature summary
- The file does NOT contain auto-generated entries or tool artifacts; it is hand-written
- A comment or note explains that future changelog entries will follow Conventional Commits, but 0.1.0 is intentionally minimal
- The file is committed to the repository before the release tag is created

---

## Non-Functional Requirements

### REQ-NF-001: Build Gate Efficiency

**Requirement:** The CI release workflow's build gate shall complete within a reasonable time to avoid slow release cycles, reusing cached dependencies and build artifacts where possible.

**Acceptance Criteria:**
- The workflow runs on `ubuntu-24.04` with the same CMake, compiler, and dependencies as the main CI pipeline
- The workflow completes within 10 minutes (end-to-end, including build, test, lint, and package steps)
- If ccache or build caching is used, it is configured to accelerate builds on subsequent pushes
- Log output shows cache hits for dependencies (CMake configuration, Qt, etc.)

### REQ-NF-002: Error Messages Clarity

**Requirement:** All error messages from the release script and CI workflow shall be clear, actionable, and suitable for a maintainer with basic shell/git knowledge.

**Acceptance Criteria:**
- Error messages include the reason for failure (e.g., "Dirty working tree: Uncommitted changes in src/")
- Error messages suggest next steps (e.g., "Run 'git stash' and try again")
- Messages are not cryptic shell syntax errors or stack traces (if a step fails, the error is human-readable)
- Each error case in the release script is tested with output verified

### REQ-NF-003: Version Format Consistency

**Requirement:** The version format shall be consistent across all artifacts and locations: semantic versioning (X.Y.Z) in source, `vX.Y.Z` in git tags, and `X.Y.Z` in tarballs and binary output.

**Acceptance Criteria:**
- CMakeLists.txt uses semantic versioning: `VERSION 0.1.0`
- Git tags use the `v` prefix: `v0.1.0`
- Tarball filename uses no prefix: `holonight-shell-0.1.0.tar.gz`
- Binary `--version` output uses no prefix: `holonight-shell 0.1.0`
- CHANGELOG.md entry uses brackets: `## [0.1.0]`
- All of the above are consistent with a single CMake version value (no manual duplication)

### REQ-NF-004: Script Idempotency and Safety

**Requirement:** The release script shall be safe to run multiple times and shall not leave the repository in an inconsistent state if interrupted or if a step fails.

**Acceptance Criteria:**
- If the script is run twice with the same version (after cleanup), it fails gracefully (tag already exists, version unchanged in CMakeLists.txt)
- If the script is interrupted (Ctrl+C) after updating CMakeLists.txt but before tagging, the user can commit the version bump manually or revert and retry
- The script does not delete branches, force-push, or perform destructive git operations
- State is committed (via git commit or tag) after each successful step, so progress is not lost

---

## Constraints

### REQ-C-001: Version Generation Pattern

**Constraint:** The `version.h` generated file shall follow the same pattern as the existing `configure_file()` call in CMakeLists.txt at line 504, using a `version.h.in` template file with CMake variable substitution.

**Acceptance Criteria:**
- A `version.h.in` file exists in the repository (e.g., at `src/version.h.in`) with a placeholder like `#define HOLONIGHT_SHELL_VERSION "@HOLONIGHT_SHELL_VERSION@"`
- CMakeLists.txt includes a `configure_file(src/version.h.in src/version.h @ONLY)` call (or similar pattern from line 504)
- The generated `version.h` is in the build directory (e.g., `build/src/version.h`) and is included in the project's include path
- The generated file is added to `.gitignore` (not committed)

### REQ-C-002: Release Script Invocation Pattern

**Constraint:** The release script shall be invoked as `scripts/release.sh <version>` from the repository root, not from within the scripts directory.

**Acceptance Criteria:**
- The script uses `$(git rev-parse --show-toplevel)` or similar to determine the repository root, ensuring it works regardless of CWD
- The script is stored at `scripts/release.sh` (not `tools/release.sh` or elsewhere)
- Running `cd /tmp && /path/to/repo/scripts/release.sh 0.1.0` succeeds and modifies files in the correct repository

### REQ-C-003: GitHub Actions Environment

**Constraint:** The CI release workflow shall execute on `ubuntu-24.04` (matching the main CI pipeline) and shall use the `gh` CLI for GitHub Release operations.

**Acceptance Criteria:**
- The workflow's `runs-on` is set to `ubuntu-24.04`
- The workflow uses `gh` commands (e.g., `gh release create`, `gh release upload`) to interact with GitHub
- The GitHub token (GITHUB_TOKEN) is available in the workflow environment (default for Actions)
- The workflow does not use deprecated or custom release action plugins

### REQ-C-004: Tag Format Enforcement

**Constraint:** All release tags shall use the format `vX.Y.Z` (lowercase `v` prefix, semantic versioning), and the CI workflow shall only trigger on tags matching this pattern.

**Acceptance Criteria:**
- The workflow's tag filter is set to `tags: 'v*'` (or `refs/tags/v*`)
- Tags like `v0.1.0`, `v1.2.3` trigger the workflow
- Tags like `release-0.1.0`, `0.1.0`, `V0.1.0` do not trigger the workflow
- The release script enforces the `vX.Y.Z` format when creating tags (rejects user input like `0.1.0`)

### REQ-C-005: Source-Only Distribution

**Constraint:** The release artifact shall be a source tarball only; no prebuilt binaries, Docker images, or OS-specific packages shall be generated or published as part of the release workflow.

**Acceptance Criteria:**
- The GitHub Release asset is a single `.tar.gz` file named `holonight-shell-X.Y.Z.tar.gz`
- The tarball contains the full source code from the tag
- The tarball does not include pre-compiled binaries, `.o` files, or build artifacts
- No secondary artifacts (Docker images, `.deb`, `.rpm`, `.apk`, Flatpak, AppImage) are published

### REQ-C-006: Changelog Immutability Before Tag

**Constraint:** The CHANGELOG.md entry for a release version shall be committed to the repository before the release tag is created; the changelog shall not be modified by the release script.

**Acceptance Criteria:**
- The release script verifies but does NOT edit CHANGELOG.md
- The version entry must exist in CHANGELOG.md before `scripts/release.sh` is run
- The CHANGELOG is committed in a separate (or prior) commit; the release script only tags the current state
- This ensures the tag points to a state where both the version and changelog are aligned

---

## Summary

The release tooling for holonight-shell 0.1.0 establishes a clear, repeatable, and safe process for cutting releases. The single source of truth (CMake version), combined with the release helper script and CI automation, ensures that versions are kept in sync, builds are gated, and artifacts are published consistently. Hand-written changelog entries for 0.1.0 and clear documentation in RELEASING.md provide a foundation for future releases.
