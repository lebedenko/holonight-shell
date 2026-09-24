#!/usr/bin/env python3
"""Check generated shell singleton exports against runtime registration."""

import re
import sys
from pathlib import Path


def main() -> int:
    metadata = Path(sys.argv[1]).read_text()
    source = Path(sys.argv[2]).read_text()
    registered = set(re.findall(r'reg\([^,]+, "([^"]+)"\)', source))
    registered.update(("WeatherIconBridge", "SidebarManager"))

    components = {}
    for block in re.findall(r"(?ms)^    Component \{\n(.*?)(?=^    Component \{|^\})", metadata):
        match = re.search(r'^        name: "([^"]+)"$', block, re.MULTILINE)
        if match:
            components[match.group(1)] = block

    problems = []
    for name in sorted(registered):
        block = components.get(name)
        if block is None:
            problems.append(f"{name}: missing Component")
            continue
        if f'HolonightShell/{name} 1.0' not in block:
            problems.append(f"{name}: missing HolonightShell 1.0 export")
        if "isSingleton: true" not in block or "isCreatable: false" not in block:
            problems.append(f"{name}: missing singleton semantics")

    navigation = components.get("SettingsNavigationService", "")
    method = re.search(r'Method \{(?:(?!Method \{|^        \}).)*name: "openPage"(?:(?!Method \{|^        \}).)*\}',
                       navigation, re.DOTALL | re.MULTILINE)
    if not method or not re.search(r'Parameter \{ name: "page_key"; type: "QString" \}', method.group()):
        problems.append("SettingsNavigationService: missing openPage(QString) method")

    if problems:
        print("QML interface metadata errors:\n  " + "\n  ".join(problems), file=sys.stderr)
        return 1
    print(f"QML interface metadata check passed ({len(registered)} runtime singletons).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
