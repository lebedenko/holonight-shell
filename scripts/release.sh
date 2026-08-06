#!/usr/bin/env bash
#
# release.sh — cut a holonight-shell release.
#
# Usage: scripts/release.sh <X.Y.Z> [--publish]
#
# Validates preconditions, bumps the single-source-of-truth version in
# CMakeLists.txt, commits the bump, and creates an annotated `vX.Y.Z` tag.
# By default it stops there and prints the push/publish commands; pass
# --publish to push the tag and create the GitHub Release via `gh`.
#
# The CHANGELOG entry for the target version must already exist and be
# committed before running this script — the script verifies but never edits it.

set -euo pipefail

usage() {
  echo "Usage: scripts/release.sh <X.Y.Z> [--publish]" >&2
}

# --- Parse arguments --------------------------------------------------------
VERSION=""
PUBLISH=0
for arg in "$@"; do
  case "$arg" in
    --publish)
      PUBLISH=1
      ;;
    -*)
      echo "Error: unknown option '$arg'." >&2
      usage
      exit 1
      ;;
    *)
      if [[ -n "$VERSION" ]]; then
        echo "Error: unexpected extra argument '$arg'." >&2
        usage
        exit 1
      fi
      VERSION="$arg"
      ;;
  esac
done

if [[ -z "$VERSION" ]]; then
  echo "Error: missing version argument." >&2
  usage
  exit 1
fi

if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Error: version must be X.Y.Z semver (e.g. 1.2.3), got: '$VERSION'." >&2
  exit 1
fi

TAG="v${VERSION}"

# --- Resolve repository root ------------------------------------------------
REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

# --- Clean working tree check -----------------------------------------------
if [[ -n "$(git status --porcelain -- src/ CMakeLists.txt CHANGELOG.md scripts/)" ]]; then
  echo "Error: dirty working tree. Commit or stash changes in src/, CMakeLists.txt," >&2
  echo "       CHANGELOG.md, or scripts/ before running the release script." >&2
  exit 1
fi
echo "Working tree clean."

# --- CHANGELOG verification (read-only) -------------------------------------
if [[ ! -f CHANGELOG.md ]]; then
  echo "Error: CHANGELOG.md not found. Create it before releasing." >&2
  exit 1
fi
if ! grep -qE "^## \[${VERSION}\]" CHANGELOG.md; then
  echo "Error: no '## [${VERSION}]' entry found in CHANGELOG.md." >&2
  echo "       Add and commit the changelog entry before running this script." >&2
  exit 1
fi
echo "Changelog entry for ${VERSION} found."

# --- Tag existence check ----------------------------------------------------
if git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null; then
  echo "Error: tag '${TAG}' already exists. Delete it first to re-release." >&2
  exit 1
fi

# --- Bump version in CMakeLists.txt -----------------------------------------
sed -i "s/^\(project(holonight-shell VERSION \)[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*/\1${VERSION}/" CMakeLists.txt

if ! grep -q "project(holonight-shell VERSION ${VERSION}" CMakeLists.txt; then
  echo "Error: version bump verification failed; CMakeLists.txt was not updated." >&2
  echo "       Inspect the project() line and retry; no commit or tag was created." >&2
  git checkout -- CMakeLists.txt
  exit 1
fi
echo "CMakeLists.txt updated to VERSION ${VERSION}."

# --- Commit the bump --------------------------------------------------------
git add CMakeLists.txt
if git diff --cached --quiet; then
  echo "Version already at ${VERSION}; nothing to commit for the bump."
else
  git commit -m "chore: bump version to ${VERSION}"
  echo "Committed version bump."
fi

# --- Create annotated tag ---------------------------------------------------
git tag -a "${TAG}" -m "Release ${TAG}"
echo "Annotated tag '${TAG}' created on $(git rev-parse --short HEAD)."

# --- Publish or print next steps --------------------------------------------
if [[ "$PUBLISH" -eq 1 ]]; then
  echo "Pushing ${TAG} and creating the GitHub Release..."
  git push origin "${TAG}"
  gh release create "${TAG}" --generate-notes
  echo "Published ${TAG}. CI will build the gate and attach the source tarball."
else
  echo ""
  echo "Release ${TAG} is ready locally. To publish:"
  echo "  git push origin HEAD       # if the bump commit is not yet on the remote branch"
  echo "  git push origin ${TAG}     # pushing the tag triggers the release workflow"
  echo ""
  echo "The CI release workflow builds the tag, packages the source tarball,"
  echo "and creates the GitHub Release automatically. Re-run with --publish to"
  echo "push the tag and create the release from here instead."
fi
