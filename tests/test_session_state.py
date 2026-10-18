#!/usr/bin/env python3
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import holonight_session_state as state


class SessionState(unittest.TestCase):
    def test_structured_environment_is_lossless_and_allowlisted(self):
        values = ['spaces and "double" and \'single\'', r'back\slash', 'Україна', '', '$(touch /tmp/not-executed); `false`', 'line\nbreak']
        for value in values:
            response = subprocess.CompletedProcess([], 0, json.dumps({'type': 'as', 'data': ['XCURSOR_THEME=' + value, 'SECRET=hidden']}), '')
            with patch.object(state.subprocess, 'run', return_value=response) as run:
                self.assertEqual(state.manager_environment(), {'XCURSOR_THEME': value})
                self.assertIn('--json=short', run.call_args.args[0])
                self.assertNotIn('show-environment', run.call_args.args[0])

    def test_executable_and_legacy_units_are_discovered(self):
        for name in ['sway', 'sway.desktop', 'start-hyprland', 'labwc.desktop']:
            row = ['wayland-wm@' + name + '.service', '', 'loaded', 'active', 'running', '', '/unit', 0, '', '/']
            with patch.object(state, 'bus', return_value=[[row]]):
                self.assertEqual(state.active_units(), [(row[0], '/unit')])

    def test_direct_ambiguous_and_transitioning_states(self):
        with patch.object(state, 'active_units', return_value=[]):
            self.assertIsNone(state.snapshot('7', '/run/user/1000'))
        for units in [[('wayland-session-bindpid@2.service', '/bind')],
                      [('wayland-wm@sway.service', '/a'), ('wayland-wm@labwc.service', '/b')]]:
            with patch.object(state, 'active_units', return_value=units), self.assertRaises(ValueError):
                state.snapshot('7', '/run/user/1000')

    def test_bus_failures_and_duplicate_environment_fail_closed(self):
        with patch.object(state.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1, '', 'unavailable')):
            with self.assertRaises(ValueError):
                state.manager_environment()
        with patch.object(state, 'property_value', return_value=['PATH=a', 'PATH=b']):
            with self.assertRaises(ValueError):
                state.manager_environment()

    def test_managed_socket_uses_cgroup_identity_not_proc_environment(self):
        import socket
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'wayland-test'
            with socket.socket(socket.AF_UNIX) as server:
                server.bind(str(path)); server.listen(1)
                environment = {'WAYLAND_DISPLAY': path.name, 'XDG_SESSION_ID': '7', 'XDG_RUNTIME_DIR': directory}
                snapshot = ([], (), '/scope', 10, 'start', '/unit', os.getpid(), 'start', environment)
                with patch.object(state, 'snapshot', return_value=snapshot), patch.object(state, 'process_identity', return_value='start') as process:
                    self.assertEqual(state.managed_environment('7', directory), environment)
                    self.assertEqual(process.call_args.args[2], '/unit')
                with patch.object(state, 'snapshot', side_effect=[snapshot, None]), patch.object(state, 'process_identity', return_value='start'):
                    with self.assertRaises(ValueError):
                        state.managed_environment('7', directory)
                server.accept()[0].close(); server.accept()[0].close()
                with patch.object(state, 'snapshot', return_value=snapshot), patch.object(state, 'process_identity', side_effect=ValueError('wrong unit')):
                    with self.assertRaises(ValueError):
                        state.managed_environment('7', directory)

    def test_login_binding_supplies_id_omitted_by_uwsm(self):
        uid = os.getuid()
        units = [('wayland-session-bindpid@123.service', '/bind'), ('wayland-wm@sway.service', '/wm')]
        environment = {'XDG_RUNTIME_DIR': '/run/user/test', 'WAYLAND_DISPLAY': 'wayland-1'}
        def prop(user, path, interface, key, *args):
            return {'ActiveState': 'active', 'Id': '7', 'ControlGroup': '/wm-group', 'MainPID': 456}[key]
        with patch.object(state, 'active_units', return_value=units), \
             patch.object(state, 'property_value', side_effect=prop), \
             patch.object(state, 'bus', return_value=['/login']), \
             patch.object(state, 'login', return_value=((uid, True), '/session-7.scope')), \
             patch.object(state, 'process_identity', return_value='start') as process, \
             patch.object(state, 'manager_environment', side_effect=lambda: dict(environment)):
            result = state.snapshot('', '/run/user/test')
            self.assertEqual(result[-1]['XDG_SESSION_ID'], '7')
            self.assertEqual(process.call_args_list[0].args, (123, uid, '/session-7.scope'))
            with self.assertRaisesRegex(ValueError, 'another logind session'):
                state.snapshot('8', '/run/user/test')
            environment['XDG_SESSION_ID'] = '8'
            with self.assertRaisesRegex(ValueError, 'conflicts'):
                state.snapshot('7', '/run/user/test')
            environment.pop('XDG_SESSION_ID')
            process.side_effect = ValueError('stale process')
            with self.assertRaisesRegex(ValueError, 'stale'):
                state.snapshot('7', '/run/user/test')

    def test_shell_wrapper_transports_json_values_without_evaluation(self):
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            values = {'WAYLAND_DISPLAY': 'wayland-1', 'SWAYSOCK': '/tmp/sway socket',
                      'XDG_CURRENT_DESKTOP': 'HoloNight:Sway', 'QT_QPA_PLATFORMTHEME': '',
                      'XCURSOR_THEME': 'Україна \\ " \' $(touch ' + directory + '/executed)\nnext line'}
            bus = root / 'busctl'
            bus.write_text('#!/usr/bin/env python3\nimport json\nprint(' + repr(json.dumps({'type': 'as', 'data': [k+'='+v for k,v in values.items()]})) + ')\n')
            bus.chmod(0o755)
            shell = root / 'shell'
            shell.write_text('#!/usr/bin/env python3\nimport os,json\nprint(json.dumps(dict(os.environ)))\n')
            shell.chmod(0o755)
            wrapper = Path(state.__file__).with_name('holonight-shell-systemd')
            result = subprocess.run([str(wrapper)], env={**os.environ, 'PATH': str(root) + ':' + os.environ['PATH'],
                                    'HOLONIGHT_SHELL_EXECUTABLE': str(shell), 'HOLONIGHT_SHELL_SYSTEMD_ENV_TIMEOUT': '0',
                                    'QT_QUICK_CONTROLS_STYLE': 'stale'}, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            actual = json.loads(result.stdout)
            for key, value in values.items():
                self.assertEqual(actual[key], value)
            self.assertNotIn('QT_QUICK_CONTROLS_STYLE', actual)
            self.assertFalse((root / 'executed').exists())

if __name__ == '__main__':
    unittest.main()
