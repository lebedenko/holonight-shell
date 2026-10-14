#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline real-CMake discovery, provider isolation and cache regressions."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DependencyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='Shell dependencies with spaces ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'source with spaces'
        shutil.copytree(ROOT, self.source, symlinks=True, ignore=shutil.ignore_patterns(
            '.git', 'build', '.cache', '__pycache__', 'compile_commands.json'))
        self.prefix = ROOT / 'build/deps/prefix'
        self.empty = self.root / 'empty internal prefix'
        self.empty.mkdir()
        self.env = dict(os.environ)
        for name in ('HOLONIGHT_EXTERNAL_PREFIX', 'HOLONIGHT_DEPENDENCY_PREFIX', 'PKG_CONFIG_PATH',
                     'PKG_CONFIG_LIBDIR', 'PKG_CONFIG_SYSROOT_DIR', 'HOLONIGHT_BUILD_FEATURES'):
            self.env.pop(name, None)
        self.env['HOLONIGHT_DEPENDENCY_PREFIX'] = str(self.prefix)

    def command(self, args, env=None):
        return subprocess.run(args, cwd=self.source, env=env or self.env, text=True, capture_output=True)

    def configure(self, *options):
        return self.command(['cmake', '-S', '.', '-B', 'build/direct', '-G', 'Ninja',
                             f'-DHOLONIGHT_DEPENDENCY_PREFIX={self.prefix}',
                             f'-DQML_IMPORT_PATH={self.prefix}/lib/qt6/qml', *options])

    def assert_pass(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def assert_failure(self, result, message):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(message, result.stdout + result.stderr)

    def test_missing_qt_rejected(self):
        self.assert_failure(self.configure('-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE'), 'Qt6')

    def test_incompatible_qt_rejected(self):
        directory = self.root / 'old Qt'
        directory.mkdir()
        (directory / 'Qt6Config.cmake').write_text('')
        (directory / 'Qt6ConfigVersion.cmake').write_text(
            'set(PACKAGE_VERSION 6.10.0)\nset(PACKAGE_VERSION_COMPATIBLE FALSE)\n')
        hook = self.root / 'isolate Qt.cmake'
        hook.write_text('macro(find_package)\nif("${ARGV0}" STREQUAL "Qt6")\n'
                        f'set(Qt6_DIR "{directory}")\n_find_package(${{ARGV}} PATHS "{directory}" NO_DEFAULT_PATH)\n'
                        'else()\n_find_package(${ARGV})\nendif()\nendmacro()\n')
        self.assert_failure(self.configure(f'-DCMAKE_PROJECT_INCLUDE={hook}'), '6.11')

    def test_mismatched_private_qt_rejected(self):
        directory = self.root / 'wrong private Qt'
        directory.mkdir()
        (directory / 'Qt6Config.cmake').write_text('')
        (directory / 'Qt6ConfigVersion.cmake').write_text(
            'set(PACKAGE_VERSION 6.10.0)\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\nset(PACKAGE_VERSION_EXACT FALSE)\n')
        hook = self.root / 'private Qt hook.cmake'
        hook.write_text('macro(find_package)\nif("${ARGV0}" STREQUAL "Qt6" AND "${ARGV}" MATCHES "WaylandClientPrivate")\n'
                        f'set(Qt6_DIR "{directory}")\n_find_package(${{ARGV}} PATHS "{directory}" NO_DEFAULT_PATH)\n'
                        'else()\n_find_package(${ARGV})\nendif()\nendmacro()\n')
        self.assert_failure(self.configure(f'-DCMAKE_PROJECT_INCLUDE={hook}'), 'exactly matches')

    def test_testing_off_without_qt_test(self):
        hook = self.root / 'reject Qt Test.cmake'
        hook.write_text('macro(find_package)\nif("${ARGV0}" STREQUAL "Qt6" AND "${ARGV}" MATCHES "(;|^)Test(;|$)")\n'
                        'message(FATAL_ERROR "Qt Test unavailable")\nendif()\n_find_package(${ARGV})\nendmacro()\n')
        self.assert_pass(self.configure('-DBUILD_TESTS=OFF', f'-DCMAKE_PROJECT_INCLUDE={hook}'))

    def test_testing_on_requires_qt_test(self):
        hook = self.root / 'reject Qt Test.cmake'
        hook.write_text('macro(find_package)\nif("${ARGV0}" STREQUAL "Qt6" AND "${ARGV}" MATCHES "(;|^)Test(;|$)")\n'
                        'message(FATAL_ERROR "Qt Test unavailable")\nendif()\n_find_package(${ARGV})\nendmacro()\n')
        self.assert_failure(self.configure('-DBUILD_TESTS=ON', f'-DCMAKE_PROJECT_INCLUDE={hook}'), 'Qt Test unavailable')

    def test_missing_gtest_rejected_without_download(self):
        self.assert_failure(self.configure('-DBUILD_TESTS=ON', '-DCMAKE_DISABLE_FIND_PACKAGE_GTest=TRUE'), 'GTest')
        self.assertFalse((self.source / 'build/direct/_deps').exists())

    def test_old_gtest_rejected(self):
        directory = self.root / 'old GTest'
        directory.mkdir()
        (directory / 'GTestConfig.cmake').write_text('')
        (directory / 'GTestConfigVersion.cmake').write_text(
            'set(PACKAGE_VERSION 1.13.0)\nset(PACKAGE_VERSION_COMPATIBLE FALSE)\n')
        hook = self.root / 'isolate GTest.cmake'
        hook.write_text('macro(find_package)\nif("${ARGV0}" STREQUAL "GTest")\n'
                        f'set(GTest_DIR "{directory}")\n_find_package(${{ARGV}} PATHS "{directory}" NO_DEFAULT_PATH)\n'
                        'else()\n_find_package(${ARGV})\nendif()\nendmacro()\n')
        self.assert_failure(self.configure('-DBUILD_TESTS=ON', f'-DCMAKE_PROJECT_INCLUDE={hook}'), '1.14')

    def test_missing_gmock_rejected(self):
        directory = self.root / 'GTest without GMock'
        directory.mkdir()
        (directory / 'GTestConfig.cmake').write_text('add_library(GTest::gtest INTERFACE IMPORTED)\n')
        (directory / 'GTestConfigVersion.cmake').write_text(
            'set(PACKAGE_VERSION 1.14.0)\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\n')
        self.assert_failure(self.configure('-DBUILD_TESTS=ON', f'-DGTest_DIR={directory}'), 'GTest::gmock')

    def test_standalone_config_restricts_internal_provider(self):
        result = self.command(['cmake', '-S', 'libs/holonight-shell-config', '-B', 'build/config', '-G', 'Ninja',
                               f'-DHOLONIGHT_DEPENDENCY_PREFIX={self.empty}',
                               f'-DHOLONIGHT_EXTERNAL_PREFIX={self.prefix}'])
        self.assert_failure(result, 'Missing HoloNightConfig in approved internal')

    def test_internal_providers_cannot_leak(self):
        for location in ('external', 'system', 'cached'):
            with self.subTest(location=location):
                options = [f'-DHOLONIGHT_DEPENDENCY_PREFIX={self.empty}']
                if location == 'external':
                    options += [f'-DHOLONIGHT_EXTERNAL_PREFIX={self.prefix}', f'-DCMAKE_PREFIX_PATH={self.prefix}']
                elif location == 'system':
                    options += [f'-DCMAKE_SYSTEM_PREFIX_PATH={self.prefix}']
                else:
                    options += [f'-DHoloNightConfig_DIR={self.prefix}/lib/cmake/HoloNightConfig']
                self.assert_failure(self.configure(*options), 'Missing HoloNightConfig in approved internal')

    def test_missing_services_provider_rejected(self):
        internal = self.root / 'partial prefix'
        shutil.copytree(self.prefix, internal)
        shutil.rmtree(internal / 'lib/cmake/HoloNightSystemServices')
        self.assert_failure(self.configure(f'-DHOLONIGHT_DEPENDENCY_PREFIX={internal}'), 'Missing HoloNightSystemServices')

    def test_provider_prefix_spaces(self):
        internal = self.root / 'installed providers with spaces'
        shutil.copytree(self.prefix, internal)
        self.assert_pass(self.configure(f'-DHOLONIGHT_DEPENDENCY_PREFIX={internal}',
                                        f'-DQML_IMPORT_PATH={internal}/lib/qt6/qml'))

    def pkg_config_fixture(self, protocols=False):
        directory = self.root / 'pkg config with spaces'
        directory.mkdir()
        if protocols:
            (directory / 'wayland-protocols.pc').write_text(
                f'prefix={directory}\npkgdatadir=${{prefix}}/missing protocols\nName: wayland-protocols\n'
                'Description: fixture\nVersion: 1.0\n')
        else:
            (directory / 'wayland-client.pc').write_text(
                'Name: wayland-client\nDescription: fixture\nVersion: 1.0\nRequires: unavailable-wayland-fixture\n')
        self.env['PKG_CONFIG_PATH'] = str(directory)

    def test_missing_wayland_client_rejected(self):
        self.pkg_config_fixture()
        self.assert_failure(self.configure(), 'unavailable-wayland-fixture')

    def test_missing_protocol_xml_rejected(self):
        self.pkg_config_fixture(protocols=True)
        self.assert_failure(self.configure(), 'xdg-shell protocol XML not found')

    def test_ordinary_commands_validate_without_preparation(self):
        env = dict(self.env, HOLONIGHT_DEPENDENCY_PREFIX=str(self.empty))
        for command in ('configure', 'build', 'test'):
            self.assert_failure(self.command(['python3', 'tooling/workflow.py', command, '--preset', 'test'], env), 'HoloNightConfig')
            self.assertFalse((self.source / 'build/deps').exists())
            self.assertFalse((self.source / 'build/external').exists())
            self.assertFalse((self.source / 'build/ci-prepared').exists())

    def test_external_prefix_spaces_compatible_reuse_and_changed_cache_archival(self):
        directory = self.root / 'external prefix with spaces' / 'lib/cmake/Qt6'
        directory.mkdir(parents=True)
        qt = subprocess.check_output(['pkg-config', '--variable=libdir', 'Qt6Core'], text=True).strip()
        installed = Path(qt) / 'cmake/Qt6'
        (directory / 'Qt6Config.cmake').write_text(f'include("{installed}/Qt6Config.cmake")\n')
        shutil.copyfile(installed / 'Qt6ConfigVersion.cmake', directory / 'Qt6ConfigVersion.cmake')
        for extra in installed.glob('*VersionImpl.cmake'):
            shutil.copyfile(extra, directory / extra.name)
        env = dict(self.env, HOLONIGHT_EXTERNAL_PREFIX=str(directory.parents[2]))
        args = ['python3', 'tooling/workflow.py', 'configure', '--preset', 'test']
        self.assert_pass(self.command(args, env))
        marker = self.source / 'build/test/marker'
        marker.write_text('preserved')
        self.assert_pass(self.command(args, env))
        self.assertTrue(marker.exists())
        self.assertIn(str(directory), (self.source / 'build/test/CMakeCache.txt').read_text())
        second = self.root / 'second external prefix'
        shutil.copytree(directory.parents[2], second)
        env['HOLONIGHT_EXTERNAL_PREFIX'] = str(second)
        self.assert_pass(self.command(args, env))
        self.assertEqual((self.source / 'build/test.previous-1/marker').read_text(), 'preserved')
        self.assertFalse(marker.exists())
        self.assertFalse((self.source / 'build/deps').exists())


if __name__ == '__main__':
    unittest.main()
