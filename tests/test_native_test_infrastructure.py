#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Regressions for system Python, selective staging and provider isolation."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('launch', ROOT / 'tests/test_quick_controls_launch.py')
launch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launch)


class TestPython(unittest.TestCase):
    def test_user_python_on_path_and_in_existing_cache(self):
        with tempfile.TemporaryDirectory(prefix='test python ') as directory:
            root = Path(directory)
            userbin = root / 'user bin'
            userbin.mkdir()
            userpython = userbin / 'python3'
            userpython.symlink_to('/usr/bin/python3')
            (root / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.25)\nproject(probe NONE)\n'
                'find_package(Python3 REQUIRED COMPONENTS Interpreter)\n'
                f'include("{ROOT}/cmake/TestPython.cmake")\n'
                'enable_testing()\n'
                'add_test(NAME python COMMAND "${HOLONIGHT_TEST_PYTHON}" -c "print(123)")\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/build-python" "${Python3_EXECUTABLE}")\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/test-python" "${HOLONIGHT_TEST_PYTHON}")\n'
            )
            env = dict(os.environ, PATH=f'{userbin}:{os.defpath}')
            for cached in (False, True):
                command = ['cmake', '-S', str(root), '-B', str(root / 'build')]
                if cached:
                    command += [f'-DPython3_EXECUTABLE={userpython}',
                                f'-DHOLONIGHT_TEST_PYTHON={userpython}']
                subprocess.run(command, env=env, check=True, capture_output=True)
                self.assertEqual((root / 'build/build-python').read_text(), str(userpython))
                self.assertEqual((root / 'build/test-python').read_text(), '/usr/bin/python3')
                subprocess.run(['ctest', '--test-dir', str(root / 'build'), '--output-on-failure'],
                               env=env, check=True, capture_output=True)


class RuntimeStaging(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='provider staging ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.prefix = self.root / 'provider prefix'
        self.qml = self.prefix / 'lib64/qt6/qml/Holonight'
        self.qml.mkdir(parents=True)
        for module in ('', 'Core', 'Controls'):
            directory = self.qml / module
            directory.mkdir(exist_ok=True)
            (directory / 'qmldir').write_text(f'module Holonight.{module}\n')
        self.lib = self.prefix / 'lib64/libholonight_config.so.1.2'
        self.lib.write_bytes(b'library fixture')
        self.soname = self.lib.with_name('libholonight_config.so.1')
        self.soname.symlink_to(self.lib.name)
        self.link = self.lib.with_name('libholonight_config.so')
        self.link.symlink_to(self.soname)  # Absolute source link must be relocated.
        (self.prefix / 'unrelated-secret').write_text('do not copy')
        self.runtime = dict(qml_source=str(self.qml), qml_destination='lib/qt6/qml/Holonight',
                            config_source=str(self.link), config_soname=self.soname.name,
                            lib_destination='lib')

    def test_selective_relocatable_copy_preserves_originals(self):
        stage = self.root / 'stage'
        launch.stage_runtime(self.runtime, stage)
        relocated = self.root / 'relocated'
        stage.rename(relocated)
        self.assertFalse((relocated / 'unrelated-secret').exists())
        self.assertEqual((relocated / 'lib' / self.link.name).read_bytes(), b'library fixture')
        self.assertFalse((relocated / 'lib' / self.link.name).readlink().is_absolute())
        for module in ('', 'Core', 'Controls'):
            staged = relocated / self.runtime['qml_destination'] / module / 'qmldir'
            staged.write_text('changed')
            self.assertNotEqual(staged.read_text(), (self.qml / module / 'qmldir').read_text())
        self.assertEqual(self.link.readlink(), self.soname)
        self.assertEqual(self.lib.read_bytes(), b'library fixture')

    def test_imported_real_library_gets_soname_alias(self):
        self.runtime['config_source'] = str(self.lib)
        stage = self.root / 'stage'
        launch.stage_runtime(self.runtime, stage)
        self.assertEqual((stage / 'lib' / self.soname.name).read_bytes(), b'library fixture')

    def test_container_does_not_require_nested_bubblewrap(self):
        with patch.dict(os.environ, UQC_ISOLATED='1'):
            self.assertEqual(launch.installed_command(self.runtime, ['probe']), ['probe'])

    @unittest.skipIf(os.environ.get('UQC_ISOLATED') == '1', 'fresh acceptance container')
    @unittest.skipUnless(shutil.which('bwrap'), 'bubblewrap unavailable')
    def test_native_masks_originals_but_preserves_staged_providers(self):
        stage = self.root / 'stage'
        launch.stage_runtime(self.runtime, stage)
        code = (f'from pathlib import Path; import stat; '
                f'assert Path("/dev/null").read_bytes() == b""; '
                f'assert not list(Path({str(self.qml)!r}).iterdir()); '
                f'assert stat.S_ISCHR(Path({str(self.link)!r}).stat().st_mode); '
                f'assert Path({str(stage / "lib" / self.link.name)!r}).read_bytes() == b"library fixture"; '
                f'assert Path({str(self.prefix / "unrelated-secret")!r}).read_text() == "do not copy"')
        subprocess.run(launch.installed_command(self.runtime, ['/usr/bin/python3', '-c', code]), check=True)
        self.assertEqual(self.lib.read_bytes(), b'library fixture')
        self.assertTrue((self.qml / 'qmldir').is_file())

    @unittest.skipIf(os.environ.get('UQC_ISOLATED') == '1', 'fresh acceptance container')
    @unittest.skipUnless(shutil.which('bwrap'), 'bubblewrap unavailable')
    def test_service_isolation_retains_installed_providers(self):
        # Include real system providers when installed: the previous wrapper
        # hid these even though ordinary native binaries were linked to them.
        providers = [self.lib, self.qml / 'qmldir']
        providers += [path for path in (Path('/usr/lib/libholonight_config.so'),
                                        Path('/usr/lib/qt6/qml/Holonight/qmldir')) if path.is_file()]
        expected = {str(path): path.read_bytes() for path in providers}
        probe = self.root / 'probe.py'
        probe.write_text('from pathlib import Path\n'
                         f'for name, content in {expected!r}.items():\n'
                         '    assert Path(name).read_bytes() == content\n')
        # External build binding exposes this fixture under /tmp while keeping
        # the original service/socket hiding and sibling read-only mounts.
        subprocess.run(['/usr/bin/python3', str(ROOT / 'scripts/run-isolated-test.py'),
                        '--hide-host', '--build-dir', str(self.root),
                        '/usr/bin/python3', str(probe)], check=True)


if __name__ == '__main__':
    unittest.main()
