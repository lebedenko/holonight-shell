#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Analyze the standalone package consumer in its own compiler context."""
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[2]
build = root / 'build/package-consumer'
entries = json.loads((build / 'compile_commands.json').read_text())
for entry in entries:
    args = entry.get('arguments') or shlex.split(entry.pop('command'))
    entry['arguments'] = [arg for arg in args if arg not in {
        '-mno-direct-extern-access', '-fno-keep-inline-dllexport', '-Wno-template-id-cdtor'}]
clang = build / 'clang'
clang.mkdir(exist_ok=True)
(clang / 'compile_commands.json').write_text(json.dumps(entries))
subprocess.run(['clang-tidy', str(root / 'tests/shell-config-package-consumer/main.cpp'),
                '-p', str(clang), f'--config-file={root / "tests/.clang-tidy"}',
                rf'--header-filter=^{root}/(apps|libs|integrations|tests)/.*\.(h|hpp)$'], check=True)
