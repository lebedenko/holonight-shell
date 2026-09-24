#!/usr/bin/env python3
"""Reject shell library dependencies that point up the target graph."""

import re
import sys
from pathlib import Path


ALLOWED = {
    "holonight_platform": {"holonight_qt_wayland_client"},
    "holonight_shell_config": set(),
    "holonight_core": {"holonight_platform", "holonight_shell_config"},
    "holonight_compositor": {"holonight_platform"},
    "holonight_services": {"holonight_core", "holonight_compositor", "holonight_platform"},
    "holonight_surfaces": {"holonight_core", "holonight_compositor", "holonight_platform",
                           "holonight_services"},
    "holonight_authentication_core": set(),
}


def check_file(path: Path) -> list[str]:
    source = re.sub(r"#[^\n]*", "", path.read_text())
    problems = []
    for match in re.finditer(r"target_link_libraries\s*\(\s*(holonight_\w+)\s+(.*?)\)",
                             source, re.DOTALL):
        owner, arguments = match.groups()
        if owner not in ALLOWED:
            continue
        edges = set(re.findall(r"\bholonight_\w+\b", arguments))
        if "HoloNightShellConfig::Config" in arguments:
            edges.add("holonight_shell_config")
        for edge in sorted(edges - ALLOWED[owner] - {owner}):
            problems.append(f"{path}: {owner} must not link {edge}")
    return problems


def main() -> int:
    paths = [Path(argument) for argument in sys.argv[1:]]
    problems = [problem for path in paths for problem in check_file(path)]
    if problems:
        print("\n".join(problems), file=sys.stderr)
        return 1
    print("Target dependency edges passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
