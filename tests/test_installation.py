#!/usr/bin/env python3
"""Exercise the real integration installer with disposable prefixes and DESTDIR."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1]


class InstallationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="holonight-install-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.fixture = self.root / "source"
        self.fixture.mkdir()
        for name in ("holonight-shell", "holonight-askpass", "holonight-polkit-agent"):
            (self.fixture / name).write_text('#!/bin/sh\nprintf "%s\\n" "${0##*/}"\n')
        (self.fixture / "CMakeLists.txt").write_text('''
cmake_minimum_required(VERSION 3.25)
project(InstallationFixture NONE)
set(CMAKE_INSTALL_LIBDIR lib)
include(GNUInstallDirs)
set(HOLONIGHT_INSTALL_SOURCE_DIR [=[@SOURCE@]=])
set(HOLONIGHT_POLKIT_CONFLICT_EXIT_STATUS 78)
configure_file("${HOLONIGHT_INSTALL_SOURCE_DIR}/cmake/InstallIntegration.cmake.in"
  "${PROJECT_BINARY_DIR}/InstallIntegration.cmake" @ONLY)
install(PROGRAMS holonight-shell DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(PROGRAMS holonight-askpass holonight-polkit-agent DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
install(SCRIPT "${PROJECT_BINARY_DIR}/InstallIntegration.cmake")
'''.replace("@SOURCE@", str(SOURCE)))
        self.build = self.root / "build"
        self.stage = self.root / "stage"

    def run_command(self, *args, **kwargs):
        result = subprocess.run(args, capture_output=True, text=True, **kwargs)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def configure(self, **directories):
        self.run_command("cmake", "-S", str(self.fixture), "-B", str(self.build),
                         "-DCMAKE_INSTALL_PREFIX=/original-prefix",
                         *(f"-DCMAKE_INSTALL_{name}={value}" for name, value in directories.items()))

    def install(self, prefix, component=None):
        args = ["cmake", "--install", str(self.build), "--prefix", str(prefix)]
        if component:
            args += ["--component", component]
        self.run_command(*args, env={**os.environ, "DESTDIR": str(self.stage)})

    def staged(self, path):
        return self.stage / str(path).lstrip("/")

    def check_layout(self, prefix, bindir="bin", libexecdir="libexec", datadir="share"):
        prefix = Path(prefix)
        bindir, libexecdir, datadir = (prefix / name for name in (bindir, libexecdir, datadir))
        self.install(prefix)
        unit = self.staged(datadir / "systemd/user/holonight-polkit-agent@.service").read_text()
        self.assertIn(f'ExecStart="{libexecdir}/holonight-polkit-agent-session" %i', unit)
        self.assertIn("RestartPreventExitStatus=78", unit)
        shell_unit = self.staged(datadir / "systemd/user/holonight-shell.service").read_text()
        self.assertIn(f'ExecStart="{bindir}/holonight-shell-systemd"', shell_unit)
        self.assertNotIn("/bin/sh -lc", shell_unit)
        self.assertIn(f'Environment="QML_IMPORT_PATH={prefix}/lib/qt6/qml"', shell_unit)
        for compositor in ("hyprland", "sway"):
            desktop = self.staged(datadir / f"wayland-sessions/holonight-{compositor}.desktop").read_text()
            self.assertIn(f'Exec="{bindir}/holonight-session" {compositor}', desktop)
        launcher = self.staged(bindir / "holonight-session").read_text()
        self.assertIn(f'installation_datadir="{datadir}"', launcher)
        self.assertNotIn(str(self.stage), launcher + unit + shell_unit)
        self.assertNotIn("/original-prefix", launcher + unit + shell_unit)
        for name in ("holonight-askpass", "holonight-sudo-askpass", "holonight-ssh-askpass"):
            alias = self.staged(bindir / name)
            self.assertTrue(alias.is_symlink())
            self.assertEqual(alias.resolve(), self.staged(libexecdir / "holonight-askpass"))
            self.assertEqual(self.run_command(str(alias)).strip(), name)
        manifest = (self.build / "install_manifest.txt").read_text().splitlines()
        for entry in (bindir / "holonight-session", bindir / "holonight-sudo-askpass",
                      libexecdir / "holonight-polkit-agent-session",
                      datadir / "systemd/user/holonight-polkit-agent@.service"):
            self.assertIn(str(entry), manifest)
        self.assertTrue(all(not entry.startswith(str(self.stage)) for entry in manifest))
        return bindir, datadir

    def test_usr_usr_local_and_local_prefix_with_spaces(self):
        self.configure()
        for prefix in ("/usr", "/usr/local", "/home/test user/.local"):
            with self.subTest(prefix=prefix):
                self.check_layout(prefix)

    def test_custom_gnu_install_directories(self):
        directories = {"BINDIR": "tools/bin", "LIBEXECDIR": "lib64/holonight", "DATADIR": "resources"}
        self.configure(**directories)
        self.check_layout("/opt/holonight", *directories.values())

    def test_absolute_gnu_install_directory(self):
        self.configure(LIBEXECDIR="/opt/holonight-helpers")
        self.check_layout("/usr", libexecdir="/opt/holonight-helpers")

    def test_reinstall_replaces_old_binary_alias_and_component_preserves_manifest(self):
        self.configure()
        self.check_layout("/usr")
        alias = self.staged("/usr/bin/holonight-sudo-askpass")
        alias.unlink()
        alias.write_text("old executable copy")
        self.check_layout("/usr")
        manifest = (self.build / "install_manifest.txt").read_bytes()
        self.install("/other-prefix", "Unspecified")
        self.assertEqual((self.build / "install_manifest.txt").read_bytes(), manifest)

    def test_installed_wrappers_use_their_own_binaries_and_portal_paths(self):
        self.configure()
        bindir, datadir = self.check_layout("/home/test/.local")
        fake_bin = self.root / "fake-bin"
        fake_bin.mkdir()
        commands = {
            "systemctl": "printf 'WAYLAND_DISPLAY=wayland-test\\nXDG_CURRENT_DESKTOP=HoloNight:Hyprland\\nHYPRLAND_INSTANCE_SIGNATURE=test\\n'",
            "dbus-update-activation-environment": ":",
            "holonight-appearance-adapter": "printf 'TestCursor\\n'",
            "holonight-shell": "exit 77",  # An older PATH entry must not win.
            "Hyprland": 'printf "%s\\n" "${SUDO_ASKPASS}" "${SSH_ASKPASS}" "${XDG_CONFIG_DIRS}" "${XDG_DATA_DIRS}" "${QML_IMPORT_PATH}" "${QT_PLUGIN_PATH}"',
        }
        for name, body in commands.items():
            command = fake_bin / name
            command.write_text("#!/bin/sh\n" + body + "\n")
            command.chmod(0o755)
        environment = {"PATH": f"{fake_bin}:/usr/bin:/bin", "HOME": str(self.root),
                       "HOLONIGHT_SESSION_MODE": "direct", "HOLONIGHT_SHELL_SYSTEMD_ENV_TIMEOUT": "0"}
        output = self.run_command(str(self.staged(bindir / "holonight-shell-systemd")), env=environment)
        self.assertEqual(output.strip(), "holonight-shell")
        output = self.run_command(str(self.staged(bindir / "holonight-session")), "hyprland", env=environment)
        self.assertIn(str(self.staged(bindir / "holonight-sudo-askpass")), output)
        self.assertIn(str(self.staged(bindir / "holonight-ssh-askpass")), output)
        self.assertIn(f"{datadir}/holonight/xdg:/etc/xdg", output)
        self.assertIn(f"{datadir}:/usr/local/share:/usr/share", output)
        self.assertIn("/home/test/.local/lib/qt6/qml", output)
        self.assertIn("/home/test/.local/lib/qt6/plugins", output)

    @unittest.skipUnless(shutil.which("task"), "Task is not installed")
    def test_install_tasks_use_separate_production_builds_and_matching_prefixes(self):
        for task, directory, prefix in (("install:system", "build-install-system", "/usr"),
                                        ("install:local", "build-install-local", str(Path.home() / ".local"))):
            result = subprocess.run(["task", "--dry", task], cwd=SOURCE, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            commands = result.stdout + result.stderr
            self.assertIn(f'-B "{directory}"', commands)
            self.assertIn(f'-DCMAKE_INSTALL_PREFIX="{prefix}"', commands)
            self.assertIn("-DBUILD_TESTS=OFF", commands)
            self.assertIn("-DENABLE_COVERAGE=OFF", commands)
            self.assertNotIn("/tmp/holonight", commands)
            self.assertNotIn("--prefix", commands)
            self.assertIn("daemon-reload", commands)


if __name__ == "__main__":
    unittest.main()
