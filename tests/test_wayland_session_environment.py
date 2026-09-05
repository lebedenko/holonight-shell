#!/usr/bin/env python3
"""Exercise session discovery against real Unix socket peers, without a compositor."""
import importlib.machinery
import importlib.util
import os
import struct
from unittest.mock import patch, MagicMock
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SOURCE_ROOT = Path(__file__).resolve().parents[1]
HELPER = SOURCE_ROOT / 'scripts/holonight-wayland-session-environment'
sys.dont_write_bytecode = True
loader = importlib.machinery.SourceFileLoader('wayland_session_environment', str(HELPER))
spec = importlib.util.spec_from_loader(loader.name, loader)
helper = importlib.util.module_from_spec(spec)
loader.exec_module(helper)
SERVER = '''
import socket, sys
server = socket.socket(socket.AF_UNIX)
server.bind(sys.argv[1])
server.listen(8)
print("ready", flush=True)
while True:
    connection, _ = server.accept()
    connection.close()
'''


class SessionEnvironmentTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='holonight-peer-test-')
        self.addCleanup(self.directory.cleanup)
        self.runtime = Path(self.directory.name)

    def server(self, name, session, **overrides):
        environment = dict(os.environ, XDG_SESSION_ID=session,
                           XDG_RUNTIME_DIR=str(self.runtime), QT_QPA_PLATFORMTHEME='holonight',
                           QT_QUICK_CONTROLS_STYLE='Holonight', XCURSOR_THEME='default',
                           DISPLAY=':99', IGNORED_MARKER='never-export')
        environment.pop("WAYLAND_DISPLAY", None)
        environment.update(overrides)
        process = subprocess.Popen([sys.executable, '-u', '-c', SERVER, str(self.runtime / name)],
                                   env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        def stop():
            if process.poll() is None:
                process.terminate()
            process.communicate(timeout=5)
        self.addCleanup(stop)
        ready = process.stdout.readline()
        if ready != b'ready\n':
            self.fail(process.stderr.read().decode())
        return process

    def discover(self, session):
        return subprocess.run([sys.executable, str(HELPER), session, str(self.runtime)],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)

    def test_captures_peer_session_without_display_in_original_environment(self):
        self.server('wayland-1', 'session-a')
        result = self.discover('session-a')
        self.assertEqual(result.returncode, 0, result.stderr)
        values = dict(entry.split(b'=', 1) for entry in result.stdout.split(b'\0') if entry)
        self.assertEqual(values[b'XDG_SESSION_ID'], b'session-a')
        self.assertEqual(values[b'XDG_RUNTIME_DIR'], os.fsencode(self.runtime))
        self.assertEqual(values[b'WAYLAND_DISPLAY'], b'wayland-1')
        self.assertEqual(values[b'QT_QPA_PLATFORM'], b'wayland')
        self.assertEqual(values[b'QT_QPA_PLATFORMTHEME'], b'holonight')
        self.assertNotIn(b'DISPLAY', values)
        self.assertNotIn(b'IGNORED_MARKER', values)
        self.assertEqual(result.stderr, b'')

    def test_two_sessions_for_one_uid_keep_distinct_displays(self):
        self.server('wayland-1', 'session-a')
        self.server('wayland-2', 'session-b')
        for session, display in [('session-a', b'wayland-1'), ('session-b', b'wayland-2')]:
            result = self.discover(session)
            self.assertEqual(result.returncode, 0)
            self.assertIn(b'WAYLAND_DISPLAY=' + display + b'\0', result.stdout)
        self.assertEqual(self.discover('session-c').returncode, 1)

    def test_stale_socket_and_wrong_session_produce_no_environment(self):
        process = self.server('wayland-stale', 'session-a')
        process.terminate()
        process.wait(timeout=5)
        self.server('wayland-other', 'session-b')
        result = self.discover('session-a')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, b'')

    def test_ambiguous_same_session_displays_are_rejected(self):
        self.server('wayland-1', 'session-a')
        self.server('wayland-2', 'session-a')
        result = self.discover('session-a')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, b'')

    def test_peer_uid_must_match_the_session_owner(self):
        self.server('wayland-1', 'session-a')
        connection = MagicMock()
        connection.__enter__.return_value = connection
        connection.getsockopt.return_value = struct.pack('3i', os.getpid(), os.getuid() + 1, os.getgid())
        with patch.object(helper.socket, 'socket', return_value=connection):
            self.assertIsNone(helper.peer_environment(self.runtime / 'wayland-1', 'session-a',
                                                     self.runtime, os.getuid()))

    def test_peer_runtime_and_session_must_be_present_and_match(self):
        self.server('wayland-1', '', XDG_RUNTIME_DIR=str(self.runtime))
        self.server('wayland-2', 'session-a', XDG_RUNTIME_DIR='/different-runtime')
        result = self.discover('session-a')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, b'')

    def test_invalid_session_or_runtime_is_rejected(self):
        for session, runtime in [('../other', str(self.runtime)), ('session-a', 'relative')]:
            result = subprocess.run([sys.executable, str(HELPER), session, runtime],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(result.stdout, b'')


if __name__ == '__main__':
    unittest.main()
