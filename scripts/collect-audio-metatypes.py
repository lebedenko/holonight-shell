#!/usr/bin/env python3
"""Collect installed Audio metadata with its C++ namespace lookup preserved."""
import json
from pathlib import Path
import sys


def main():
    entries = [json.loads(Path(name).read_text()) for name in sys.argv[2:]]
    names = {component['className']: component['qualifiedClassName']
             for entry in entries for component in entry['classes']}
    for entry in entries:
        for component in entry['classes']:
            for prop in component.get('properties', []):
                # moc leaves provider-local pointer names unqualified; QML tools
                # need canonical names to link anonymous inherited model types.
                name = prop['type'].removesuffix('*')
                if name in names:
                    prop['type'] = names[name] + ('*' if prop['type'].endswith('*') else '')
    Path(sys.argv[1]).write_text(json.dumps(entries, indent=2) + '\n')


if __name__ == '__main__':
    main()
