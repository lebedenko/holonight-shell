#!/usr/bin/env python3
"""Session contracts with real environment preparation and isolated command boundaries."""
import importlib.machinery
import importlib.util
import os
import contextlib
import io
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[1]
SCRIPTS = SOURCE / 'scripts'
loader = importlib.machinery.SourceFileLoader('doctor', str(SCRIPTS / 'holonight-session-doctor'))
spec = importlib.util.spec_from_loader(loader.name, loader)
DOCTOR = importlib.util.module_from_spec(spec)
loader.exec_module(DOCTOR)


class Sessions(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='holonight session ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.log = self.root / 'calls'
        self.env = dict(HOME=str(self.root), PATH=f'{self.bin}:{SCRIPTS}:/usr/bin:/bin',
                        XDG_RUNTIME_DIR=str(self.root), XDG_CONFIG_HOME=str(self.root / 'config'),
                        TEST_LOG=str(self.log), XDG_SESSION_ID='7', HOLONIGHT_SESSION_RUN_DIR=str(self.root))
        for name in ('Hyprland', 'sway', 'labwc', 'start-hyprland', 'holonight-shell',
                     'systemctl', 'uwsm'):
            self.fake(name, 'printf "%s\\n" "$0 $*" >> "$TEST_LOG"')
        self.fake('holonight-appearance-adapter', 'echo TestCursor')

    def fake(self, name, body):
        path = self.bin / name
        path.write_text('#!/bin/sh\n' + body + '\n')
        path.chmod(0o755)

    def run_script(self, name, *arguments, **environment):
        return subprocess.run([str(SCRIPTS / name), *arguments], env={**self.env, **environment},
                              capture_output=True, text=True)

    def test_path_is_idempotent_with_absent_and_present_local_bin(self):
        local = str(self.root / '.local/bin')
        for exists in (False, True):
            if exists:
                Path(local).mkdir(parents=True)
            for original, expected in [('/custom:/usr/bin', local + ':/custom:/usr/bin'),
                                       (f'/custom:{local}:/usr/bin', f'/custom:{local}:/usr/bin'),
                                       ('', local)]:
                script = f'. {shlex.quote(str(SCRIPTS / "holonight-session-defaults"))}; '
                script += script + 'printf "%s" "$PATH"'
                result = subprocess.run(['/bin/sh', '-c', script], capture_output=True, text=True,
                                        env={**self.env, 'PATH': original, 'script_directory': str(SCRIPTS)})
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, expected)

    def test_managed_start_uses_full_identity_and_never_imports_or_starts_shell(self):
        for compositor in ('hyprland', 'sway', 'labwc'):
            for mode in ('auto', 'uwsm'):
                self.log.write_text('')
                result = self.run_script('holonight-session', compositor, HOLONIGHT_SESSION_MODE=mode)
                self.assertEqual(result.returncode, 0, result.stderr)
                log = self.log.read_text()
                title = {'hyprland': 'Hyprland', 'sway': 'Sway', 'labwc': 'Labwc'}[compositor]
                native = 'Hyprland' if compositor == 'hyprland' else compositor
                self.assertIn(f'start -e -D HoloNight-{title}:HoloNight:{native}', log)
                self.assertNotIn('import-environment', log)
                self.assertNotIn('start holonight-shell', log)
                self.assertIn('--config', log)

    def test_auto_without_uwsm_fails_without_starting_compositor(self):
        (self.bin / 'uwsm').unlink()
        self.fake('holonight-appearance-adapter', 'exit 1')
        result = subprocess.run(['/bin/bash', str(SCRIPTS / 'holonight-session'), 'sway'],
                                env={**self.env, 'PATH': str(self.bin)}, capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn('managed sessions require UWSM', result.stderr)
        self.assertFalse(self.log.exists())

    def test_manager_or_existing_session_failure_starts_no_services(self):
        for command in ('systemctl', 'uwsm'):
            with self.subTest(command=command):
                self.fake(command, 'exit 1')
                self.log.write_text('')
                result = self.run_script('holonight-session', 'sway')
                self.assertEqual(result.returncode, 1)
                self.assertNotIn('start ', self.log.read_text())
                self.fake(command, 'printf "%s\\n" "$0 $*" >> "$TEST_LOG"')

    def test_finalization_requires_display_marker_and_session_before_services(self):
        for compositor, marker, value in [('hyprland', 'HYPRLAND_INSTANCE_SIGNATURE', 'sig'),
                                           ('sway', 'SWAYSOCK', '/run/sway.sock'), ('labwc', 'LABWC_PID', '123')]:
            (self.root / 'finalized').unlink(missing_ok=True)
            for environment in ({}, {'WAYLAND_DISPLAY': 'wayland-test'},
                                {'WAYLAND_DISPLAY': 'wayland-test', marker: value}):
                self.log.write_text('')
                result = self.run_script('holonight-session-finalize', compositor, **environment)
                if marker in environment:
                    self.assertEqual(result.returncode, 0, result.stderr)
                    log = self.log.read_text()
                    self.assertLess(log.index('finalize WAYLAND_DISPLAY'), log.index('start --no-block'))
                    self.assertIn('HOLONIGHT_SESSION_READY', log)
                else:
                    self.assertEqual(result.returncode, 1)
                    self.assertEqual(self.log.read_text(), '')

    def test_services_wait_for_successful_finalization(self):
        self.fake('uwsm', 'printf "waiting\\n"; read -r result; exit "$result"')
        environment = {**self.env, 'WAYLAND_DISPLAY': 'wayland-test', 'SWAYSOCK': '/run/sway.sock'}
        process = subprocess.Popen([str(SCRIPTS / 'holonight-session-finalize'), 'sway'],
                                   env=environment, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True)
        try:
            self.assertEqual(process.stdout.readline(), 'waiting\n')
            self.assertFalse(self.log.exists(), 'services started before finalization completed')
            _, error = process.communicate('0\n', timeout=5)
            self.assertEqual(process.returncode, 0, error)
            self.assertIn('start --no-block', self.log.read_text())
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()

    def test_finalization_is_once_per_login_and_failure_never_starts_services(self):
        environment = {'WAYLAND_DISPLAY': 'wayland-test', 'SWAYSOCK': '/run/sway.sock'}
        self.fake('uwsm', 'exit 1')
        result = self.run_script('holonight-session-finalize', 'sway', **environment)
        self.assertEqual(result.returncode, 1)
        self.assertFalse(self.log.exists())
        self.fake('uwsm', 'printf "%s\\n" "$*" >> "$TEST_LOG"')
        for _ in range(2):
            self.assertEqual(self.run_script('holonight-session-finalize', 'sway', **environment).returncode, 0)
        self.assertEqual(self.log.read_text().count('finalize WAYLAND_DISPLAY'), 1)

    def test_migration_rejected_without_modifying_personal_files(self):
        override = Path(self.env['XDG_CONFIG_HOME']) / 'holonight/session'
        override.mkdir(parents=True)
        legacy = override / 'sway.conf'
        legacy.write_text('focus_follows_mouse no\n')
        for compositor in ('sway', 'hyprland', 'labwc'):
            result = self.run_script('holonight-session-config', compositor)
            self.assertEqual(result.returncode, 1)
            self.assertIn(str(legacy), result.stderr)
            self.assertIn('merge desired settings', result.stderr)
        self.assertEqual(legacy.read_text(), 'focus_follows_mouse no\n')

    def test_native_configuration_and_relative_conflicting_include(self):
        config = Path(self.env['XDG_CONFIG_HOME'])
        (config / 'sway/parts').mkdir(parents=True)
        native = config / 'sway/config'
        native.write_text('include parts/*.conf\ninput * xkb_layout ua\n')
        part = config / 'sway/parts/start.conf'
        part.write_text('exec uwsm app -- foot\n')
        result = self.run_script('holonight-session-config', 'sway')
        self.assertEqual(result.returncode, 0, result.stderr)
        text = Path(result.stdout.strip()).read_text()
        self.assertIn(str(native), text)
        self.assertNotIn('bindsym', text)
        self.assertNotIn('exec_always', text)
        part.write_text('exec holonight-shell\n')
        result = self.run_script('holonight-session-config', 'sway')
        self.assertEqual(result.returncode, 1)
        self.assertIn(str(part) + ':1', result.stderr)
        (config / 'labwc').mkdir()
        (config / 'labwc/rc.xml').write_text('<labwc_config/>')
        result = self.run_script('holonight-session-config', 'labwc')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), str(config / 'labwc'))

    def test_startup_script_conflicts_and_protection_assignments(self):
        config = Path(self.env['XDG_CONFIG_HOME']) / 'labwc'
        config.mkdir(parents=True)
        (config / 'rc.xml').write_text('<labwc_config/>')
        startup = config / 'startup'
        (config / 'autostart').write_text('./startup\n')
        startup.write_text('uwsm app -- foot\n')
        environment = config / 'environment'
        environment.write_text('LABWC_UPDATE_ACTIVATION_ENV="0"\n')
        result = self.run_script('holonight-session-config', '--check', 'labwc')
        self.assertEqual(result.returncode, 0, result.stderr)
        for command in ['holonight-polkit-agent', 'uwsm finalize', 'dbus-update-activation-environment --systemd WAYLAND_DISPLAY']:
            startup.write_text(command + '\n')
            result = self.run_script('holonight-session-config', '--check', 'labwc')
            self.assertEqual(result.returncode, 1)
            self.assertIn(str(startup) + ':1', result.stderr)
        startup.write_text('uwsm app -- foot\n')
        environment.write_text('LABWC_UPDATE_ACTIVATION_ENV=1\n')
        result = self.run_script('holonight-session-config', '--check', 'labwc')
        self.assertEqual(result.returncode, 1)
        self.assertIn(str(environment) + ':1', result.stderr)

    @unittest.skipUnless(shutil.which('lua'), 'Lua interpreter unavailable')
    def test_hyprland_native_settings_imports_and_override_order(self):
        config_home = self.root / 'config with "quotes" and apostrophe\'s'
        native = config_home / 'hypr'
        (native / 'nested').mkdir(parents=True)
        (native / 'monitors.lua').write_text('''
hl.monitor({ output = "eDP-1", scale = 1.25 })
hl.monitor({ output = "DP-3", scale = 1.5 })
''')
        (native / 'nested/init.lua').write_text('hl.bind("SUPER + T", "personal")\n')
        (native / 'hyprland.lua').write_text('''
assert(settings.misc.disable_xdg_env_checks)
require("monitors")
require("nested")
hl.on("hyprland.start", function() hl.exec_cmd("native startup") end)
''')
        result = self.run_script('holonight-session-config', 'hyprland', XDG_CONFIG_HOME=str(config_home))
        self.assertEqual(result.returncode, 0, result.stderr)
        config = Path(result.stdout.strip())
        self.assertNotIn('uwsm app -- hn-files', config.read_text())
        harness = self.root / 'check.lua'
        harness.write_text('''
settings, monitors, bindings, callbacks, commands = {}, {}, {}, {}, {}
hl = {
    config = function(value) settings = value end,
    monitor = function(value) table.insert(monitors, value) end,
    bind = function(key, action) table.insert(bindings, key) end,
    on = function(event, callback)
        assert(event == "hyprland.start")
        table.insert(callbacks, callback)
    end,
    exec_cmd = function(command) table.insert(commands, command) end,
}
dofile(arg[1])
assert(monitors[1].scale == 1.25 and monitors[2].scale == 1.5)
assert(#bindings == 1 and bindings[1] == "SUPER + T")
assert(settings.misc.disable_xdg_env_checks)
assert(#callbacks == 2)
for _, callback in ipairs(callbacks) do callback() end
assert(commands[1] == "native startup")
assert(commands[2] == "holonight-session-finalize hyprland")
''')
        checked = subprocess.run(['lua', str(harness), str(config)], capture_output=True, text=True)
        self.assertEqual(checked.returncode, 0, checked.stderr)

    def test_hyprland_uses_home_config_when_xdg_config_home_is_unset(self):
        native = self.root / '.config/hypr'
        native.mkdir(parents=True)
        (native / 'hyprland.lua').write_text('error("broken personal config")\n')
        environment = dict(self.env)
        environment.pop('XDG_CONFIG_HOME')
        result = subprocess.run([str(SCRIPTS / 'holonight-session-config'), 'hyprland'],
                                env=environment, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        text = Path(result.stdout.strip()).read_text()
        self.assertIn(str(native), text)
        self.assertNotIn('hl.bind(', text)
        if shutil.which('Hyprland'):
            checked = subprocess.run([shutil.which('Hyprland'), '--verify-config', '-c', result.stdout.strip()],
                                     env=environment, capture_output=True, text=True)
            self.assertNotEqual(checked.returncode, 0)
            self.assertIn('broken personal config', checked.stdout + checked.stderr)

    @unittest.skipUnless(shutil.which('Hyprland'), 'Hyprland configuration verifier unavailable')
    def test_hyprland_verifier_accepts_fallback_and_native_imports(self):
        config_home = self.root / 'config with "quotes" and apostrophe\'s'
        for use_native in (False, True):
            with self.subTest(native=use_native):
                if use_native:
                    native = config_home / 'hypr'
                    (native / 'nested').mkdir(parents=True)
                    (native / 'hyprland.lua').write_text('require("monitors")\nrequire("nested")\n')
                    (native / 'monitors.lua').write_text('''
hl.monitor({ output = "eDP-1", mode = "preferred", position = "auto", scale = 1.25 })
hl.monitor({ output = "DP-3", mode = "preferred", position = "auto", scale = 1.5 })
''')
                    (native / 'nested/init.lua').write_text('hl.bind("SUPER + T", hl.dsp.exec_cmd("true"))\n')
                result = self.run_script('holonight-session-config', 'hyprland', XDG_CONFIG_HOME=str(config_home))
                self.assertEqual(result.returncode, 0, result.stderr)
                checked = subprocess.run([shutil.which('Hyprland'), '--verify-config', '-c', result.stdout.strip()],
                                         env=self.env, capture_output=True, text=True)
                self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)
                self.assertIn('config ok', checked.stdout + checked.stderr)

    @unittest.skipUnless(Path('/usr/lib/uwsm/prepare-env.sh').is_file(), 'real UWSM environment preloader unavailable')
    def test_actual_uwsm_preparation_preserves_identity_and_user_precedence(self):
        data = self.root / 'share'
        (data / 'uwsm').mkdir(parents=True)
        defaults = (SCRIPTS / 'holonight-session-defaults').read_text()
        defaults = defaults.replace('@CMAKE_INSTALL_FULL_DATADIR@', str(data))
        defaults = defaults.replace('@CMAKE_INSTALL_FULL_BINDIR@', str(self.bin))
        (data / 'defaults').write_text(defaults)
        hook = (SOURCE / 'data/uwsm/env-holonight').read_text()
        hook = hook.replace('@CMAKE_INSTALL_FULL_DATADIR@/holonight/session/holonight-session-defaults', str(data / 'defaults'))
        (data / 'uwsm/env-holonight').write_text(hook)
        config = Path(self.env['XDG_CONFIG_HOME']) / 'uwsm'
        config.mkdir(parents=True)
        (config / 'env').write_text('export QT_QUICK_CONTROLS_STYLE=UserStyle\n')
        self.fake('xdg-user-dirs-update', ':')
        for comp, title, native in [('hyprland', 'Hyprland', 'Hyprland'), ('sway', 'Sway', 'sway'), ('labwc', 'Labwc', 'labwc')]:
            identity = f'HoloNight-{title}:HoloNight:{native}'
            auxiliary = self.root / 'aux'
            variables = {'__OIFS__': ' \t\n', '__SELF_NAME__': 'uwsm', '__RANDOM_MARK__': 'ENV_START',
                         '__LOAD_PROFILE__': 'false', '__WM_DESKTOP_NAMES__': identity,
                         '__WM_FIRST_DESKTOP_NAME__': f'HoloNight-{title}',
                         '__WM_BIN_ID__': 'start_hyprland' if comp == 'hyprland' else comp,
                         '__WM_DESKTOP_NAMES_EXCLUSIVE__': 'true', '__WM_ID_UNIT_STRING__': comp, '__WM_ID__': comp}
            auxiliary.write_text('\n'.join(k + '=' + shlex.quote(v) for k, v in variables.items()))
            result = subprocess.run(['/bin/sh', '/usr/lib/uwsm/prepare-env.sh', str(auxiliary),
                                     f'/usr/share/uwsm/plugins/{comp}.sh'], env={**self.env, 'XDG_DATA_DIRS': str(data)}, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            env = dict(item.split(b'=', 1) for item in result.stdout.split(b'ENV_START', 1)[1].split(b'\0') if b'=' in item)
            self.assertEqual(env[b'XDG_CURRENT_DESKTOP'].decode(), identity)
            self.assertEqual(env[b'QT_QUICK_CONTROLS_STYLE'], b'UserStyle')
            self.assertIn(b'HOLONIGHT_SESSION_READY', env[b'UWSM_WAIT_VARNAMES'])
            self.assertNotIn(b'HOLONIGHT_SESSION_READY', env)
            if comp == 'hyprland':
                for flag in (b'HYPRLAND_NO_SD_VARS', b'HYPRLAND_NO_SD_NOTIFY', b'HYPRLAND_NO_SD_TARGET'):
                    self.assertEqual(env[flag], b'1')
            elif comp == 'labwc':
                self.assertEqual(env[b'LABWC_UPDATE_ACTIVATION_ENV'], b'0')

    def test_portal_user_and_admin_overrides_precede_installed_defaults(self):
        for compositor in ('hyprland', 'sway', 'labwc'):
            data = SOURCE / 'data'
            env = {**self.env, 'XDG_CURRENT_DESKTOP': f'HoloNight-{compositor}:HoloNight:{compositor}',
                   'XDG_CONFIG_DIRS': str(self.root / 'etc'), 'XDG_DATA_DIRS': str(data)}
            path = DOCTOR.portal_config(env)
            self.assertEqual(path.name, f'holonight-{compositor}-portals.conf')
            self.assertIn('Settings=holonight;gtk', path.read_text())
            for directory in (self.root / 'etc', Path(self.env['XDG_CONFIG_HOME'])):
                portal = directory / 'xdg-desktop-portal/portals.conf'
                portal.parent.mkdir(parents=True, exist_ok=True)
                portal.write_text('[preferred]\ndefault=gtk\n')
                self.assertEqual(DOCTOR.portal_config(env), portal)
            for directory in (self.root / 'etc', Path(self.env['XDG_CONFIG_HOME'])):
                (directory / 'xdg-desktop-portal/portals.conf').unlink()

    def test_doctor_detects_old_process_environment_without_executing_program(self):
        self.fake('local-program', 'exit 99')
        stable = {**self.env, 'PATH': str(self.root / '.local/bin') + ':' + str(self.bin),
                  'HOLONIGHT_SESSION_READY': '1', 'XDG_CURRENT_DESKTOP': 'HoloNight-Sway:HoloNight:sway',
                  'XDG_DATA_DIRS': str(SOURCE / 'data')}
        def command(*arguments):
            if 'show-environment' in arguments:
                return '\n'.join(k + '=' + shlex.quote(v) for k, v in stable.items())
            if 'list-units' in arguments:
                return 'wayland-wm@sway.service loaded active running\n'
            if 'show' in arguments:
                return '123\n'
            return ''
        old = {**stable, 'PATH': '/no-longer-installed'}
        with patch.dict(os.environ, stable, clear=True), patch.object(DOCTOR, 'command', side_effect=command), patch.object(DOCTOR, 'manager_environment', return_value=stable), patch.object(DOCTOR, 'managed_environment', return_value=stable), \
                patch.object(DOCTOR, 'process_environment', return_value=old), \
                patch('sys.argv', ['doctor', '--check-executable', 'local-program']), \
                contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(DOCTOR.main(), 1)
        self.assertIn('may retain an older environment', output.getvalue())
        self.assertIn('executable resolution differs', output.getvalue())

    def test_doctor_distinguishes_compositor_startup_from_stale_client_display(self):
        portal = self.root / 'portals.conf'
        portal.write_text('[preferred]\ndefault=none\n')
        stable = {**self.env, 'PATH': str(self.root / '.local/bin'),
                  'HOLONIGHT_SESSION_READY': '1', 'WAYLAND_DISPLAY': 'wayland-1',
                  'SWAYSOCK': '/run/user/test/sway.sock'}
        startup = {key: value for key, value in stable.items()
                   if key not in ('WAYLAND_DISPLAY', 'SWAYSOCK', 'HOLONIGHT_SESSION_READY')}

        def command(*arguments):
            if 'show-environment' in arguments:
                return '\n'.join(k + '=' + shlex.quote(v) for k, v in stable.items())
            if 'list-units' in arguments:
                return 'wayland-wm@sway.service loaded active running\n'
            if 'show' in arguments:
                return '2\n' if arguments[-1] == 'wayland-wm@sway.service' else '1\n'
            return ''

        for stale_client in (False, True):
            client = {**stable, 'WAYLAND_DISPLAY': 'wayland-old'} if stale_client else stable
            with self.subTest(stale_client=stale_client), \
                    patch.dict(os.environ, stable, clear=True), \
                    patch.object(DOCTOR, 'command', side_effect=command), patch.object(DOCTOR, 'manager_environment', return_value=stable), patch.object(DOCTOR, 'managed_environment', return_value=stable), \
                    patch.object(DOCTOR, 'process_environment', side_effect=lambda pid: startup if pid == 2 else client), \
                    patch.object(DOCTOR, 'portal_config', return_value=portal), \
                    patch('sys.argv', ['doctor']), contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(DOCTOR.main(), int(stale_client))
            self.assertNotIn('compositor: differs', output.getvalue())
            if stale_client:
                self.assertIn('shell: differs from manager (WAYLAND_DISPLAY)', output.getvalue())

    def test_diagnostics_resolution_and_inaccessible_processes(self):
        self.fake('local-program', 'exit 99')
        self.assertEqual(DOCTOR.resolve('local-program', str(self.bin)), str(self.bin / 'local-program'))
        self.assertIsNone(DOCTOR.resolve('local-program', '/nonexistent'))
        with self.assertRaises(OSError):
            DOCTOR.process_environment(99, self.root)
        with patch.object(Path, 'read_bytes', side_effect=PermissionError('restricted')):
            with self.assertRaises(PermissionError):
                DOCTOR.process_environment(1, self.root)
        self.fake('systemctl', 'exit 1')
        self.fake('busctl', 'exit 1')
        result = self.run_script('holonight-session-doctor', '--check-executable', 'local-program')
        self.assertEqual(result.returncode, 1)
        self.assertIn('inaccessible', result.stdout)
        self.assertEqual(self.run_script('holonight-session-doctor', '--invalid').returncode, 2)


if __name__ == '__main__':
    unittest.main()
