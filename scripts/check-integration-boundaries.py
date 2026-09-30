#!/usr/bin/env python3
"""Concrete integrations must never leak into shared services or shell QML."""
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
private_headers = {path.name for path in (root / "integrations").rglob("*.h")}
errors = []
for base in (root / "libs", root / "apps/shell"):
    for path in base.rglob("*"):
        if path.suffix not in {".cpp", ".h", ".qml", ".txt"} or not path.is_file():
            continue
        source = path.read_text()
        includes = re.findall(r'#include\s*[<"]([^">]+)', source)
        imports_private_header = any(Path(header).name in private_headers for header in includes)
        # Keep these explicit names for negative fixtures with no integration tree.
        concrete_dependency = re.search(
            r'#include\s*[<"][^">]*(Hyprland|Sway|GenericBackend)'
            r'|import\s+[^\n]*(Hyprland|Sway)|SpecialWorkspaceDot'
            r'|holonight_(?:backend_\w+|\w+_implementation)', source)
        if imports_private_header or concrete_dependency:
            errors.append(str(path))
shared_headers = {
    path.name
    for library in ("holonight-compositor", "holonight-core", "holonight-services", "holonight-surfaces")
    for path in (root / "libs" / library).rglob("*.h")
}
for path in (root / "integrations").rglob("*"):
    if not path.is_file():
        continue
    source = path.read_text() if path.suffix in {".cpp", ".h", ".qml", ".txt"} else ""
    includes = re.findall(r'#include\s*[<"]([^">]+)', source)
    shared_dependency = any(Path(header).name in shared_headers for header in includes)
    if shared_dependency or re.search(r'holonight_(?:app|core|services|surfaces|compositor)\b', source):
        errors.append(str(path))
for base in (root / "integrations", root / "qml/Presentation"):
    for path in base.rglob("*.qml"):
        if re.search(r'import\s+HolonightShell(?!\.Presentation)|CompositorService|ConfigService|TooltipService',
                     path.read_text()):
            errors.append(str(path))
if errors:
    print("Integration boundary violations:\n" + "\n".join(errors), file=sys.stderr)
    sys.exit(1)
print("Integration boundaries passed.")
