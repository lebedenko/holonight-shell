# Releasing holonight-shell

This guide is for **project maintainers** cutting a release. A release produces
a git tag (`vX.Y.Z`) and a GitHub Release with a source tarball attached. There
are no prebuilt binaries — holonight-shell is distributed as source and built
against the host's Qt6.

## Prerequisites

- `git`
- [`gh`](https://cli.github.com/) (GitHub CLI), authenticated (`gh auth status`)
  — only needed if you publish from your machine with `--publish`; CI does not
  need it on your machine.
- A clean checkout of `main` with the release commit(s) already merged.

## Version: single source of truth

The version lives in exactly one place — the `project()` directive at the top of
`CMakeLists.txt`:

```cmake
project(holonight-shell VERSION 0.1.0 LANGUAGES C CXX)
```

Everything else derives from it:

- `CMakeLists.txt` runs `configure_file` to generate `build/src/version.h`
  (`holonight::shellVersion`), which feeds `holonight-shell --version`.
- The git tag is `vX.Y.Z`.
- The source tarball is `holonight-shell-X.Y.Z.tar.gz`.

Never hardcode the version anywhere else. `scripts/release.sh` is the tool that
bumps the `project()` line for you.

## Tag format

Release tags are `vX.Y.Z` (lowercase `v` + [semver](https://semver.org/)). The
CI release workflow only triggers on tags matching `v*`.

## Changelog

`CHANGELOG.md` follows [Keep a Changelog](https://keepachangelog.com/). **You
write the entry by hand and commit it before tagging** — `scripts/release.sh`
verifies the entry exists but never edits the file.

- For each release, move items from `## [Unreleased]` into a new
  `## [X.Y.Z] - YYYY-MM-DD` section and update the compare/link references at the
  bottom of the file.
- The project uses [Conventional Commits](https://www.conventionalcommits.org/)
  (`feat:`, `fix:`, `chore:`, `ci:`, …), so the commit history is the raw
  material for changelog entries. The `0.1.0` entry was hand-written as the
  initial release; future entries can be curated from the conventional-commit log.

## Cutting a release

1. **Update the changelog.** Add a `## [X.Y.Z] - YYYY-MM-DD` section to
   `CHANGELOG.md`, then commit it (e.g. `docs: add changelog entry for X.Y.Z`)
   and make sure it is on `main`.

2. **Run the release script** from anywhere in the repo:

   ```bash
   scripts/release.sh X.Y.Z
   ```

   It will:
   - validate the version is `X.Y.Z` semver,
   - refuse to run on a dirty working tree,
   - verify a `## [X.Y.Z]` entry exists in `CHANGELOG.md`,
   - refuse if the `vX.Y.Z` tag already exists,
   - bump the `project()` version in `CMakeLists.txt` and verify the edit,
   - commit the bump (`chore: bump version to X.Y.Z`),
   - create the annotated tag `vX.Y.Z`,
   - print the push/publish commands.

3. **Push the commit and tag.** Review the bump commit first, then:

   ```bash
   git push origin HEAD      # if the bump commit isn't on the remote branch yet
   git push origin vX.Y.Z    # pushing the tag triggers the release workflow
   ```

   Pushing the `vX.Y.Z` tag starts `.github/workflows/release.yml`.

   Alternatively, run `scripts/release.sh X.Y.Z --publish` to push the tag and
   create the GitHub Release directly from your machine using `gh`.

## What CI does on a tag push

`.github/workflows/release.yml` runs automatically when a `v*` tag is pushed:

1. **Build gate** (`debian:trixie` container, same steps as CI): configure,
   build, `ctest`, and `qml-lint`. If any step fails, **nothing is published** —
   a broken tag never ships.
2. **Publish** (only if the gate passes): creates
   `holonight-shell-X.Y.Z.tar.gz` via `git archive` and publishes a GitHub
   Release for the tag with that tarball attached and auto-generated notes.

The release appears on the repository's Releases page when the workflow finishes.

## Troubleshooting

- **The build gate failed.** No release was published. Delete the tag, fix the
  problem on `main`, and re-tag:

  ```bash
  git push origin :vX.Y.Z      # delete the remote tag
  git tag -d vX.Y.Z            # delete the local tag
  # fix, commit, then re-run scripts/release.sh X.Y.Z
  ```

- **`scripts/release.sh` says the tag already exists.** Delete the local tag
  (`git tag -d vX.Y.Z`) before re-running, or choose a new version.

- **Re-releasing the same version.** Delete the existing GitHub Release on the
  Releases page first, then delete and re-push the tag. The workflow fails if a
  Release for the tag already exists.

- **`gh release create` permission error in CI.** The workflow declares
  `permissions: contents: write` and uses the built-in `GITHUB_TOKEN`. If the
  repository's default workflow token permissions are set to read-only in
  Settings → Actions → General, grant write permission there.
