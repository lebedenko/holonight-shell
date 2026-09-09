# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
"""Each forbidden application form must fail independently."""
import subprocess
import sys
import tempfile
from pathlib import Path

checker = Path(__file__).resolve().parents[1] / 'scripts/check-qml-import-policy.py'
fixtures = {
    'comments': ('// Button {}\n/* import Holonight */\nItem { property string example: \"Button {}\" }', True),
    'templates': ('import QtQuick.Templates as T\nT.Button {}', False),
    'private': ('import Holonight.impl as Impl\nItem {}', False),
    'versioned-style': ('import Holonight 1.0 as H\nH.Button {}', False),
    'positive': ('import QtQuick.Controls as Controls\nimport Holonight.Core\n'
                 'import Holonight.Controls\nControls.Button { Controls.ButtonGroup.group: group; '
                 'property int transition: Controls.StackView.Immediate }', True),
    'core-only': ('import Holonight.Core\nHnLabel {}', True),
    'missing-core': ('HnLabel {}', False),
    'direct-style': ('import Holonight as HnStyle\nHnStyle.Button {}', False),
    'basic': ('import QtQuick.Controls.Basic as Controls\nControls.Button {}', False),
    'fusion': ('import QtQuick.Controls.Fusion as Controls\nControls.Button {}', False),
    'unaliased': ('import QtQuick.Controls\nButton {}', False),
    'wrong-alias': ('import QtQuick.Controls as C\nC.Button {}', False),
    'missing-import': ('Controls.Button {}', False),
    'instance': ('import QtQuick.Controls as Controls\nButton {}', False),
    'fallback': ('import QtQuick.Controls as Controls\nDialog {}', False),
    'enum': ('import QtQuick.Controls as Controls\nItem { property int x: StackView.Immediate }', False),
    'attached': ('import QtQuick.Controls as Controls\nItem { ButtonGroup.group: group }', False),
}
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    for name, (source, valid) in fixtures.items():
        (root / 'Fixture.qml').write_text(source)
        result = subprocess.run([sys.executable, str(checker), str(root)], capture_output=True, text=True)
        assert (result.returncode == 0) == valid, (name, result.stderr)
        print(f'{name}: passed')
