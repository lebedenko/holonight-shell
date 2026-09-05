#!/usr/bin/env python3
"""Remove only manifest files beneath an explicitly selected installation prefix."""

import argparse
from pathlib import Path
import sys


def uninstall(manifest: Path, prefix: Path, delete: bool) -> None:
    if not prefix.is_absolute() or prefix == Path("/"):
        raise ValueError("installation prefix must be absolute and must not be /")
    prefix = prefix.resolve()
    if prefix == Path("/"):
        raise ValueError("installation prefix must not resolve to /")
    if not manifest.exists():
        print(f"No install manifest at {manifest}; nothing to uninstall.")
        return

    paths = []
    for line in manifest.read_text().splitlines():
        if not line:
            continue
        path = Path(line)
        # Resolve parents, but never follow the final symlink being uninstalled.
        if (not path.is_absolute() or ".." in path.parts or path == prefix
                or not path.parent.resolve().is_relative_to(prefix)):
            raise ValueError(f"refusing manifest entry outside {prefix}: {line}")
        if path.is_dir() and not path.is_symlink():
            raise ValueError(f"refusing to remove a directory: {line}")
        paths.append(path)

    # Validate the entire manifest before removing anything.
    for path in paths:
        print(f"{'Removing' if delete else 'Would remove'} {path}")
        if delete:
            path.unlink(missing_ok=True)
    if delete:
        manifest.unlink()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--delete", action="store_true", help="remove files; the default is a dry run")
    args = parser.parse_args()
    try:
        uninstall(args.manifest, args.prefix, args.delete)
    except (OSError, ValueError) as error:
        print(f"uninstall: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
