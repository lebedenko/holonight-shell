# Release Tooling — Design

Satisfies: SPEC.md in this directory.

---

## 1. Overview

This feature adds four deliverables that together produce a repeatable, gated release process:

1. **`--version` / `-V` flag** — the `holonight-shell` binary prints its version to stdout and exits immediately, before any Qt or Wayland initialisation. Satisfies REQ-F-001, REQ-F-002.

2. **`RELEASING.md`** — a human-readable guide at the repository root explaining the full release workflow for maintainers. Satisfies REQ-F-014.

3. **`scripts/release.sh`** — a Bash helper that validates preconditions, bumps the CMake version, commits the bump, creates an annotated tag, and prints (or optionally executes) the push + publish commands. Satisfies REQ-F-004 through REQ-F-009.

4. **`.github/workflows/release.yml`** — a GitHub Actions workflow triggered on `v*` tag pushes that builds and tests the project as a gate, then creates a source tarball with `git archive` and publishes a GitHub Release. Satisfies REQ-F-010 through REQ-F-013, REQ-C-003, REQ-C-004.

Tying them together is the single-source-of-truth principle: the version lives exclusively in `CMakeLists.txt` line 2 (`project(holonight-shell VERSION 0.1.0 ...)`). A `configure_file()` call generates a `version.h` header that the binary reads at compile time; CI extracts the version from the tag name at runtime. No version string is duplicated or hardcoded anywhere else (REQ-F-003, REQ-NF-003, REQ-C-001).

`CHANGELOG.md` (REQ-F-015) is written before any release is cut; the release script verifies its presence but never edits it (REQ-C-006).

---

## 2. Components & Responsibilities

### 2.1 Version Plumbing: `src/version.h.in` + CMake `configure_file`

**Job:** Expose the CMake project version as a C preprocessor macro so `src/main.cpp` can print it without any hardcoded string.

**Pattern:** Follows the existing `configure_file` call at `CMakeLists.txt` lines 504–508:

```cmake
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/GeneratedQmlFiles.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/GeneratedQmlFiles.h
  @ONLY
)
```

**New template — `src/version.h.in`:**

```cpp
#pragma once
#define HOLONIGHT_SHELL_VERSION "@PROJECT_VERSION@"
```

`@PROJECT_VERSION@` is a standard CMake variable automatically set to the version string from the `project()` directive (line 2 of `CMakeLists.txt`). No custom variable is required.

**New `configure_file` call** placed immediately after the existing one (after line 508):

```cmake
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/src/version.h
  @ONLY
)
```

**Output path:** `${CMAKE_CURRENT_BINARY_DIR}/src/version.h` — i.e. `build/src/version.h`.

**Include path:** The `holonight-shell` executable target is defined at line 410 as `qt6_add_executable(holonight-shell src/main.cpp)`. It does not currently carry an explicit `target_include_directories` call of its own; it links against `holonight_app` (line 539–541), which transitively pulls in all library includes. The build-tree root (`${CMAKE_CURRENT_BINARY_DIR}`) is already on `holonight_platform`'s include path via `target_include_directories(holonight_platform PUBLIC ${CMAKE_CURRENT_BINARY_DIR})` at line 211. However, that public include propagates to downstream targets through the link chain, so `src/main.cpp` can already reach files under `${CMAKE_CURRENT_BINARY_DIR}`. To be explicit and avoid relying on transitive propagation for a file owned by main.cpp, add a direct include to the executable:

```cmake
target_include_directories(holonight-shell PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/src
)
```

This is placed immediately after the `configure_file` call so it's obvious why the directory is needed.

**How `src/main.cpp` uses it:**

```cpp
#include "version.h"
```

This resolves to `build/src/version.h` via the `PRIVATE` include directory added above.

**`.gitignore` entry:** `build/` is already covered by `.gitignore` (line 55). No additional entry is required; the generated `build/src/version.h` is never committed (REQ-C-001).

**Requirements satisfied:** REQ-F-003, REQ-C-001.

---

### 2.2 `--version` Handling in `src/main.cpp`

**Placement:** Inside the existing manual argv loop at lines 28–33, immediately after (or as an additional `if` branch alongside) the `--toggle-launcher` check, and before the `QGuiApplication app(argc, argv)` call at line 35.

**Exact addition:**

```cpp
for (int index = 1; index < argc; ++index) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const QString arg = QString::fromLocal8Bit(argv[index]);
    if (arg == QStringLiteral("--toggle-launcher")) {
        return sendControlCommand(argc, argv, QByteArrayLiteral("toggle-launcher"));
    }
    if (arg == QStringLiteral("--version") || arg == QStringLiteral("-V")) {
        std::println(stdout, "holonight-shell {}", HOLONIGHT_SHELL_VERSION);
        return 0;
    }
}
```

`std::println` is already available in this translation unit — the project uses C++23 (`CMakeLists.txt` line 4, `set(CMAKE_CXX_STANDARD 23)`). It requires `#include <print>`.

**Output format:** `holonight-shell 0.1.0\n` — the string `"holonight-shell "` followed by the macro value, followed by the newline that `std::println` appends. Goes to `stdout` (first argument). Matches the required format exactly (REQ-F-001).

**Exit:** `return 0` before `QGuiApplication app(argc, argv)` at line 35. No Qt application object is constructed, no Wayland surface is created (REQ-F-002).

**Required `#include` additions to `src/main.cpp`:**

```cpp
#include "version.h"
#include <print>
```

**Requirements satisfied:** REQ-F-001, REQ-F-002.

---

### 2.3 `scripts/release.sh`

**Invocation:** `scripts/release.sh <VERSION> [--publish]` from the repository root or any working directory. `<VERSION>` is the bare semver string (e.g., `0.1.0`), not the tag (REQ-C-002 clarifies the tag is constructed by the script).

**Preamble (Bash strictness):**

```bash
#!/usr/bin/env bash
set -euo pipefail
```

**Design tension — dirty tree vs. version bump:** The dirty-tree check (REQ-F-005) and the CMakeLists.txt edit (REQ-F-006) are in apparent conflict: if the tree is clean before the script runs, the script itself makes it dirty by editing `CMakeLists.txt`. The resolution is that the CHANGELOG entry is the only thing the user must commit manually before running the script; the version bump is a scripted commit that happens inside the script after the clean-tree check. The script is therefore the only agent that touches the working tree after the initial check.

**Full command flow (numbered sequence):**

1. **Parse arguments.** Validate that exactly one positional argument was given and matches the regex `^[0-9]+\.[0-9]+\.[0-9]+$`. On failure, print an actionable error to stderr and `exit 1` (REQ-F-004).

   ```bash
   VERSION="${1:-}"
   if [[ -z "$VERSION" ]]; then
       echo "Usage: scripts/release.sh <X.Y.Z> [--publish]" >&2
       exit 1
   fi
   if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
       echo "Error: version must be X.Y.Z semver (e.g. 1.2.3), got: '$VERSION'" >&2
       exit 1
   fi
   TAG="v${VERSION}"
   ```

2. **Resolve repository root.** Ensures the script works regardless of `$CWD` (REQ-C-002).

   ```bash
   REPO_ROOT="$(git rev-parse --show-toplevel)"
   cd "$REPO_ROOT"
   ```

3. **Clean tree check.** Uses `git status --porcelain` with a pathspec limited to source/config directories. If any output is returned, abort (REQ-F-005).

   ```bash
   if [[ -n "$(git status --porcelain -- src/ CMakeLists.txt CHANGELOG.md scripts/)" ]]; then
       echo "Error: dirty working tree. Commit or stash changes in src/, CMakeLists.txt," >&2
       echo "       CHANGELOG.md, or scripts/ before running the release script." >&2
       exit 1
   fi
   ```

   Untracked files under `build/` are not in the pathspec and do not trigger a failure.

4. **CHANGELOG verification.** Checks that `CHANGELOG.md` exists and contains a section header for the target version (REQ-F-007, REQ-C-006). The script does NOT edit the file.

   ```bash
   if [[ ! -f CHANGELOG.md ]]; then
       echo "Error: CHANGELOG.md not found. Create it before releasing." >&2
       exit 1
   fi
   if ! grep -qE "^## \[${VERSION}\]" CHANGELOG.md; then
       echo "Error: no '## [${VERSION}]' entry found in CHANGELOG.md." >&2
       echo "       Add the changelog entry and commit it before running this script." >&2
       exit 1
   fi
   echo "Changelog entry for ${VERSION} found."
   ```

5. **Tag existence check.** Abort early if the tag already exists to prevent accidental re-tagging (REQ-F-008, REQ-NF-004).

   ```bash
   if git tag -l | grep -q "^${TAG}$"; then
       echo "Error: tag '${TAG}' already exists. Delete it first if you want to re-release." >&2
       exit 1
   fi
   ```

6. **CMakeLists.txt version bump.** Uses `sed` with a tightly-anchored pattern that matches only the `project(holonight-shell VERSION ...)` line (REQ-F-006). The anchor is precise enough to avoid matching other lines:

   ```bash
   sed -i "s/^\(project(holonight-shell VERSION \)[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*/\1${VERSION}/" CMakeLists.txt
   ```

   **Verification step** immediately after (REQ-F-006 acceptance criteria):

   ```bash
   if ! grep -q "project(holonight-shell VERSION ${VERSION}" CMakeLists.txt; then
       echo "Error: version bump verification failed. CMakeLists.txt was not updated correctly." >&2
       exit 1
   fi
   echo "CMakeLists.txt updated to VERSION ${VERSION}."
   ```

7. **Commit the version bump.** Creates a dedicated commit so the tag points to a state where CMakeLists.txt, CHANGELOG.md, and all source are aligned. The CHANGELOG was committed by the user in a prior commit; this commit adds only the version bump.

   ```bash
   git add CMakeLists.txt
   git commit -m "chore: bump version to ${VERSION}"
   ```

   Committing before tagging ensures the annotated tag points to the release commit, not the pre-bump commit. If the commit fails for any reason, `set -e` halts the script before the tag is created, leaving a clean recovery path (revert the CMakeLists.txt edit manually and retry).

8. **Create annotated tag** (REQ-F-008).

   ```bash
   git tag -a "${TAG}" -m "Release ${TAG}"
   echo "Annotated tag '${TAG}' created on $(git rev-parse HEAD)."
   ```

9. **Print next steps or publish** (REQ-F-009).

   If `--publish` was not passed:
   ```bash
   echo ""
   echo "Release ${TAG} is ready locally. To publish:"
   echo "  git push origin ${TAG}"
   echo "  gh release create ${TAG} --generate-notes"
   ```

   If `--publish` was passed:
   ```bash
   git push origin "${TAG}"
   gh release create "${TAG}" --generate-notes
   ```

**`set -euo pipefail`** ensures any unexpected failure (bad `git add`, commit rejection, `gh` not found) aborts the script rather than silently continuing.

**Requirements satisfied:** REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-C-002, REQ-C-004, REQ-NF-002, REQ-NF-003, REQ-NF-004.

---

### 2.4 CI Release Workflow: `.github/workflows/release.yml`

**Trigger:** `push: tags: ['v*']` (REQ-F-010, REQ-C-004). Branch pushes do not trigger it.

**Runner and container:** `ubuntu-24.04` with `container: debian:trixie` — identical to ci.yml's `build-test` job (lines 15–16) (REQ-C-003, REQ-NF-001).

**One job vs. two jobs:** The workflow uses a single job with sequential steps rather than two jobs with `needs:`. Rationale: the release workflow is inherently sequential (build → package → publish); splitting into two jobs adds scheduling overhead and complicates artefact passing between jobs (a tarball created in job-1 must be uploaded/downloaded via `actions/upload-artifact`). With a single job, the tarball is a filesystem file and the publish step reads it directly. Failure at any step halts the job and prevents publishing (REQ-F-011 requires no `continue-on-error: true`).

**Version extraction:**

```yaml
- name: Extract version
  run: echo "VERSION=${GITHUB_REF_NAME#v}" >> "$GITHUB_ENV"
```

`GITHUB_REF_NAME` for a `v0.1.0` tag is `v0.1.0`; the `#v` strip produces `0.1.0`. The resulting `$VERSION` environment variable is used in all subsequent steps (REQ-NF-003).

**Steps — mirroring ci.yml:**

The following steps are copied directly from ci.yml's `build-test` job (lines 19–64):

- **Install dependencies** — identical `apt-get` block from ci.yml lines 20–46. This includes `gh` CLI (`gh` is pre-installed on `ubuntu-24.04` runners; if using a container it must be added — add `gh` to the `apt-get` list since it is available in Debian trixie).
- **`actions/checkout@v4`** — same as ci.yml line 48.
- **Configure** — `cmake -B build -G Ninja -S . -DCMAKE_BUILD_TYPE=${{ env.BUILD_TYPE }} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTS=ON` (ci.yml lines 51–55).
- **Build** — `cmake --build build --parallel` (ci.yml line 58).
- **Test** — `ctest --test-dir build --output-on-failure --no-tests=error` (ci.yml line 61).
- **QML lint** — `cmake --build build --target qml-lint` (ci.yml line 64).

None of these steps carry `continue-on-error: true` (REQ-F-011).

Static checks (`format-check`, `clang-tidy`) from ci.yml's `static-checks` job are omitted from the release workflow. Rationale: static checks already ran and passed on the commits that led to this tag via the normal `push`-to-main CI. Running them again in the release workflow would add ~5 minutes for no new information. The build/test/lint gate is sufficient to confirm the tagged commit is healthy.

**Source tarball step** (REQ-F-012, REQ-C-005):

```yaml
- name: Create source tarball
  run: |
    git archive \
      --format=tar.gz \
      --prefix="holonight-shell-${VERSION}/" \
      -o "holonight-shell-${VERSION}.tar.gz" \
      "${GITHUB_REF_NAME}"
```

`GITHUB_REF_NAME` is the full tag name (`v0.1.0`). `git archive` produces a clean source tarball from the tag — no build artefacts, no `.git/` directory, no `build/` directory.

**GitHub Release creation step** (REQ-F-013, REQ-C-003):

```yaml
- name: Create GitHub Release
  run: |
    gh release create "${GITHUB_REF_NAME}" \
      "holonight-shell-${VERSION}.tar.gz" \
      --generate-notes
  env:
    GH_TOKEN: ${{ github.token }}
```

`--generate-notes` auto-fills the release body from merged PRs and commits since the previous tag (REQ-F-013 acceptance criteria: "links to CHANGELOG or auto-generated"). The tarball is passed as a positional argument to `gh release create`, which attaches it as a release asset.

**Permissions:** The workflow requires `contents: write` to create a GitHub Release. This is declared at the job level:

```yaml
permissions:
  contents: write
```

**`BUILD_TYPE` env variable:** Declared at the top of the workflow file as `env: BUILD_TYPE: Debug` (same as ci.yml line 10).

**Requirements satisfied:** REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-C-003, REQ-C-004, REQ-C-005, REQ-NF-001.

---

### 2.5 `CHANGELOG.md` and `RELEASING.md`

**`CHANGELOG.md`** (REQ-F-015) follows [Keep a Changelog](https://keepachangelog.com/) format. The topmost entry is:

```
## [0.1.0] - 2026-06-03

### Added
- Initial release of holonight-shell.
```

A short comment notes that future entries will follow Conventional Commits. The file is committed to the repository before any release tag is created (REQ-C-006).

**`RELEASING.md`** (REQ-F-014) lives at the repository root and covers:
- Target audience (project maintainers)
- Prerequisites (git, gh CLI, cmake, task)
- Step-by-step process: update CHANGELOG → commit → run `scripts/release.sh <version>` → push tag → CI does the rest
- Single-source-of-truth explanation (CMakeLists.txt version)
- Tag format (`vX.Y.Z`)
- CI automation summary (what the release workflow does)
- Troubleshooting (delete the tag, fix the issue, re-run)

---

## 3. Data / Control Flow

End-to-end walkthrough for releasing `0.1.0`:

1. **Maintainer edits `CHANGELOG.md`** — adds a `## [0.1.0] - 2026-06-03` section. Commits: `git commit -m "docs: add changelog entry for 0.1.0"`. This commit must be on `main` and pushed before the release tag.

2. **Maintainer runs the release script** from the repository root:
   ```
   scripts/release.sh 0.1.0
   ```
   - Script validates `0.1.0` matches `^[0-9]+\.[0-9]+\.[0-9]+$`. OK.
   - Script `cd`s to repo root via `git rev-parse --show-toplevel`.
   - Script checks `git status --porcelain -- src/ CMakeLists.txt CHANGELOG.md scripts/`. Clean. OK.
   - Script greps `CHANGELOG.md` for `## [0.1.0]`. Found. OK.
   - Script checks `git tag -l` for `v0.1.0`. Not found. OK.
   - Script runs `sed -i ...` on `CMakeLists.txt`, updating `VERSION 0.1.0` (already at 0.1.0 in this case — no-op but safe; the verification grep still passes).
   - Script runs `git add CMakeLists.txt && git commit -m "chore: bump version to 0.1.0"`.
   - Script runs `git tag -a v0.1.0 -m "Release v0.1.0"`.
   - Script prints push/publish instructions.

3. **Maintainer reviews, then pushes the tag:**
   ```
   git push origin v0.1.0
   ```
   (If the version bump commit is not yet on `origin/main`, also push the branch first.)

4. **GitHub Actions picks up the `v0.1.0` tag push** and starts `.github/workflows/release.yml`.

5. **CI release workflow runs sequentially:**
   - Installs Debian trixie dependencies (same `apt-get` block as ci.yml).
   - Checks out the tag (`actions/checkout@v4`).
   - Extracts `VERSION=0.1.0` from `GITHUB_REF_NAME=v0.1.0`.
   - Configures CMake with `-DBUILD_TESTS=ON`.
   - Builds the project. The generated `build/src/version.h` contains `#define HOLONIGHT_SHELL_VERSION "0.1.0"`.
   - Runs `ctest`. All tests pass.
   - Runs `cmake --build build --target qml-lint`. Passes.
   - Runs `git archive ... -o holonight-shell-0.1.0.tar.gz v0.1.0`.
   - Runs `gh release create v0.1.0 holonight-shell-0.1.0.tar.gz --generate-notes`.

6. **GitHub Release `v0.1.0` appears** on the repository Releases page with `holonight-shell-0.1.0.tar.gz` as a downloadable asset and auto-generated release notes.

**Handoffs:**
- Maintainer → script: the CHANGELOG entry must be committed before the script runs.
- Script → CI: the annotated tag `v0.1.0` is the trigger; CI reads the version purely from the tag name.
- CI build → CI publish: all steps in a single job; failure at build/test/lint aborts before the tarball is created or the release is published.

---

## 4. Key Decisions & Rationale

**Single source of truth via CMake `project(VERSION)`.** All version consumers — the binary, the tarball name, the git tag — derive from one location. Adding a new consumer means adding a `configure_file` or a shell variable expansion, not updating a second hardcoded string. Changing the version is one edit in one file (REQ-F-003).

**Why `version.h` is generated, not committed.** The generated file contains the version string that is also in `CMakeLists.txt`. Committing it would mean two places to update on every release and a perpetual source of merge conflicts. Generating it at build time from the canonical source eliminates the duplication. The `.gitignore` already covers `build/` entirely (line 55), so no extra entry is needed.

**Why the script commits the version bump before tagging.** An annotated tag is a permanent pointer to a specific commit. If the bump were committed after the tag, the tag would point to the pre-bump commit and `git archive` from that tag would produce a tarball containing the old version in `CMakeLists.txt`. Committing first and then tagging the bump commit ensures the tagged snapshot is fully self-consistent: the CHANGELOG entry, the `project(VERSION)`, and the tag name all agree.

**One job vs. two jobs in the CI release workflow.** Splitting build-gate and publish into two jobs requires uploading the tarball as a workflow artefact after the build job and downloading it in the publish job. This adds latency and complexity. Since the steps are strictly sequential and all must succeed, a single job is simpler, faster, and equally correct. The job-level `permissions: contents: write` declaration is a single clear statement of what the workflow can do.

**Source-tarball-only distribution** (REQ-C-005). holonight-shell is a Wayland shell that depends on Hyprland/wlr-layer-shell; there is no universal target environment for a pre-built binary. A source tarball is the appropriate distribution unit. Users compile from source with the documented dependencies. Platform packages (Flatpak, COPR, etc.) are explicitly out of scope for 0.1.0.

---

## 5. Alternatives Considered

**Hardcoding the version in a separate `VERSION` file or `src/version.h`.** Common in projects that don't use CMake's `project(VERSION)`. Rejected because it creates a second authoritative location: the `project()` line must still be updated for CMake to report the correct version, so the two would always need to be kept in sync manually. The `configure_file` approach eliminates this.

**Lightweight vs. annotated tags.** Lightweight tags are not objects in Git; they don't carry a message or tagger identity. Annotated tags are first-class objects, returned by `git describe`, and are the conventional form for release tags. `git cat-file -t v0.1.0` returns `tag` for annotated, `commit` for lightweight. REQ-F-008 requires annotated tags; this is also the right default.

**Building binary artifacts (AppImage, Flatpak, static binary).** Would require a hermetic build environment, a Flatpak manifest, and ongoing maintenance of the packaging layer. The target audience compiles from source. Deferred entirely — the spec calls this out of scope explicitly.

**Using a third-party release GitHub Action (e.g., `softprops/action-gh-release`, `release-please`).** These actions add external dependencies that can be deprecated, change their interface, or require a specific token scope. The `gh` CLI is pre-installed on GitHub-hosted runners and is the official GitHub tool. REQ-C-003 explicitly requires `gh`. Using `gh release create` directly is more transparent and auditable.

**`git-cliff` for automated changelog generation.** `git-cliff` can generate a `CHANGELOG.md` from Conventional Commits history. For the 0.1.0 release the git history pre-dates any formal Conventional Commits discipline across the whole project (it is an initial release). The spec explicitly calls for a hand-written 0.1.0 entry and notes that future releases may adopt Conventional Commits automation. Deferring cliff to a future release is intentional.

**Using `sed` with a looser pattern.** A pattern like `s/VERSION [0-9.]*/VERSION ${VERSION}/` would be shorter but could accidentally match other CMake `VERSION` usages (e.g., `cmake_minimum_required(VERSION 3.25)`). The anchored pattern `^\(project(holonight-shell VERSION \)...` matches only the one line it is designed to touch.

---

## 6. Risks & Mitigations

**Brittle `sed` on the `project()` line.** If the `project()` invocation is reformatted (e.g., split across lines), the `sed` regex will not match and the verification step will catch it and exit. This turns a silent failure into an explicit error (REQ-F-006 acceptance criterion: verification must fail loudly). Mitigation: the `configure_file` call for `version.h` is immediately below the `project()` line, so accidental reformatting would also break the build — making it self-revealing before release.

**CI publishing on a re-pushed or duplicate tag.** If a tag is deleted on the remote and re-pushed (`git push --force origin v0.1.0`), the workflow triggers again and attempts `gh release create` on an already-existing release, which will fail with a non-zero exit code. Mitigation: the script checks for a pre-existing local tag (step 5 in Section 2.3); the CI workflow will fail visibly rather than silently overwriting. Document in `RELEASING.md` that re-releasing the same version requires manual release deletion on GitHub before re-pushing the tag.

**`gh` authentication / permissions in CI.** The workflow uses `GH_TOKEN: ${{ github.token }}` (the built-in Actions token). This token has `contents: write` when the job declares `permissions: contents: write`. If the repository settings restrict default token permissions to `read-only`, the workflow will fail at the `gh release create` step with a permission error. Mitigation: declare `permissions: contents: write` explicitly at the job level so the intent is unambiguous; document this in `RELEASING.md`.

**Generated `version.h` missing from include path causing a build break.** If the `target_include_directories` call for `${CMAKE_CURRENT_BINARY_DIR}/src` is omitted or placed in the wrong target, `#include "version.h"` in `src/main.cpp` will fail to find the file. The `configure_file` call already creates the file at configure time, so the file exists; only the include path is the risk. Mitigation: place the `target_include_directories` call directly below the `configure_file` call in CMakeLists.txt so they are visually coupled; the CI build step will fail immediately and loudly if the include path is wrong.

**Tag pushed but CHANGELOG not committed.** If a maintainer pushes a tag pointing to a commit that does not yet include the CHANGELOG entry, the release will be published without it. Mitigation: the release script enforces the changelog check (step 4) and requires the CHANGELOG to be committed before the bump commit is made (the clean-tree check in step 3 will fail if `CHANGELOG.md` has unstaged changes). The ordering guarantee is: CHANGELOG committed → clean tree → script runs → tag created. Documented in `RELEASING.md`.

**First-time `gh` CLI availability in the Debian trixie container.** The ci.yml `build-test` job uses a `container: debian:trixie` image that does not include the `gh` CLI by default. The release workflow's publish step requires it. Mitigation: add `gh` to the `apt-get install` list in the release workflow's "Install dependencies" step. The `gh` package is available in Debian trixie as `gh` (from GitHub's apt repository) or can be installed via their official deb package. Alternatively, GitHub-hosted `ubuntu-24.04` runners have `gh` pre-installed, but since the workflow uses a container, it must be installed explicitly.

---

## 7. Testing Strategy

**`--version` flag (REQ-F-001, REQ-F-002):** Verified manually after `task build` by running `./build/holonight-shell --version` and `./build/holonight-shell -V`. Expected output: `holonight-shell 0.1.0`. Expected exit code: 0. No Wayland session required (the flag exits before Qt init). This is a one-command smoke test.

**`scripts/release.sh` — static analysis:** Run `shellcheck scripts/release.sh` to catch common shell pitfalls (unquoted variables, missing `set -e`, unreachable code). This requires `shellcheck` to be installed locally. The CI does not currently run `shellcheck`; it can be added as a step to the `static-checks` job in ci.yml if desired, but it is not required for this feature.

**`scripts/release.sh` — dry run mode:** The script's default behavior (without `--publish`) does not push or create a GitHub Release. A "dry run" of all local steps can be performed by running `scripts/release.sh <NEW_VERSION>` on a branch, then inspecting the commit and tag with `git log --oneline -2` and `git cat-file -t v<NEW_VERSION>`, and resetting with `git tag -d v<NEW_VERSION> && git reset --hard HEAD~1`. This exercises every step up to the push without touching the remote.

**`scripts/release.sh` — error paths:** Each error exit can be triggered manually:
- No arguments: `scripts/release.sh` → usage error.
- Bad format: `scripts/release.sh 0.1` → format error.
- Dirty tree: make an uncommitted change in `src/`, then run → dirty-tree error.
- Missing CHANGELOG entry: run with a version not in CHANGELOG.md → missing-entry error.
- Duplicate tag: create `v9.9.9` manually, then `scripts/release.sh 9.9.9` → tag-exists error.

**`configure_file` / version.h:** The existing CI `Build` step (`cmake --build build --parallel`) will fail to compile `src/main.cpp` if `version.h` is missing from the include path. This makes the CMake plumbing self-testing: a broken `configure_file` or missing `target_include_directories` produces an immediate, obvious build failure in CI.

**CI release workflow:** The workflow is validated on the first real tag push (`v0.1.0`). GitHub Actions provides per-step logs. If the build gate fails, no release is published. If the `gh release create` fails, the build logs will show the `gh` error message. The workflow cannot be meaningfully unit-tested in isolation from GitHub Actions; the test is the first real release.

**Note on GTest/CTest:** The existing test suite uses GTest/CTest (`Taskfile.yml` `test` task, `CMakeLists.txt` lines 595–628). Version plumbing, the release script, and the CI workflow are shell/CMake/CI artefacts — they do not have meaningful unit tests in the GTest sense. The verification strategy is: shellcheck for static analysis, manual dry runs for the script, and CI build success as the integration test for the CMake plumbing.
