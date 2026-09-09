#!/usr/bin/env python3
"""Run shell tests with disposable configuration and a private bus without activation."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hide-host', action='store_true')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command
    if command and command[0] == '--':
        command = command[1:]
    if not command:
        parser.error('a command is required')
    with tempfile.TemporaryDirectory(prefix='uqc102-tests-') as directory:
        root = Path(directory)
        env = dict(PATH=os.defpath, LANG='C.UTF-8', HOME=str(root),
                   QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software',
                   QT_QPA_PLATFORMTHEME='', QT_FORCE_STDERR_LOGGING='1',
                   DBUS_SYSTEM_BUS_ADDRESS=f'unix:path={root}/no-system-bus',
                   PULSE_SERVER=f'unix:{root}/no-pulse', PIPEWIRE_REMOTE='no-pipewire')
        for name in ('QT_QUICK_CONTROLS_STYLE', 'QT_SCALE_FACTOR', 'LD_LIBRARY_PATH',
                     'QT_LOGGING_RULES', 'QML_IMPORT_TRACE', 'QT_DEBUG_PLUGINS'):
            if name in os.environ:
                env[name] = os.environ[name]
        for name in ('XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_CACHE_HOME', 'XDG_RUNTIME_DIR',
                     'XDG_CONFIG_DIRS', 'XDG_DATA_DIRS', 'XDG_STATE_HOME'):
            path = root / name
            path.mkdir(mode=0o700)
            env[name] = str(path)
        config = root / 'bus.conf'
        config.write_text(f'''<busconfig><type>session</type><listen>unix:tmpdir={root}</listen>
<policy context="default"><allow send_destination="*"/><allow receive_sender="*"/>
<allow own="*"/></policy></busconfig>''')
        bus = subprocess.Popen(['dbus-daemon', '--nofork', '--print-address=1', f'--config-file={config}'],
                               stdout=subprocess.PIPE, text=True, env=env)
        try:
            env['DBUS_SESSION_BUS_ADDRESS'] = bus.stdout.readline().strip()
            assert env['DBUS_SESSION_BUS_ADDRESS'], 'private bus did not start'
            if args.hide_host and os.environ.get('UQC_ISOLATED') != '1':
                project = Path(__file__).resolve().parents[1]
                isolated = ['bwrap', '--die-with-parent', '--unshare-net', '--unshare-pid',
                            '--unshare-ipc', '--unshare-uts', '--ro-bind', '/', '/', '--dev', '/dev',
                            '--proc', '/proc', '--tmpfs', '/run', '--tmpfs', '/tmp', '--tmpfs', '/home',
                            '--bind', str(root), str(root), '--ro-bind', str(project.parent), str(project.parent),
                            '--bind', str(project), str(project),
                            '--chdir', str(project)]
                for path in (Path('/usr/lib/qt6/qml/Holonight'), Path('/usr/local/lib/qt6/qml/Holonight')):
                    if path.is_dir():
                        isolated += ['--tmpfs', str(path)]
                for pattern in ('*holonight*.so*', '*HoloNight*.so*'):
                    for path in Path('/usr/lib').glob(pattern):
                        if path.is_file() and not path.is_symlink():
                            isolated += ['--ro-bind', '/dev/null', str(path)]
                command = isolated + ['--'] + command
            if args.hide_host or os.environ.get('UQC_ISOLATED') == '1':
                env['UQC_ISOLATED'] = '1'
            return subprocess.run(command, env=env, timeout=1800).returncode
        finally:
            bus.terminate()
            try:
                bus.wait(timeout=5)
            except subprocess.TimeoutExpired:
                bus.kill()
                bus.wait()


if __name__ == '__main__':
    raise SystemExit(main())
