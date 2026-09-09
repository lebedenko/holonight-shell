#!/usr/bin/env python3
"""Actual build and relocated installation launches; invoke inside run-isolated-test --hide-host."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time


def stop(process):
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        raise AssertionError('child did not terminate promptly')


def verify(log, style, installed, prefix):
    assert 'phase=ui classification=loaded' in log, 'no created root evidence'
    implementation = re.search(rf'phase=implementation origin=.*?/{style}/(?:Button|Label|ComboBox|ScrollView)\.qml', log)
    # Fusion Label has no implementation child with a separate QML context.
    # Correlate an actual QQuickLabel instance with that declaration's resolved URL.
    for context in re.findall(r'phase=implementation instance=QQuickLabel context= (\S+)', log):
        implementation = implementation or re.search(
            rf'resolveType: {re.escape(context)} "Controls.Label".*?/{style}/Label\.qml', log)
    assert implementation, 'no created control origin'
    for library in ('libholonight_core_qml.so', 'libholonight_controls_qml.so'):
        assert re.search(rf'{re.escape(str(prefix))}.*{library}.*loaded library', log), f'missing provider plugin load: {library}'
    if style == 'Fusion':
        assert 'libqtquickcontrols2fusionstyleplugin' in log, 'Fusion plugin not loaded'
    else:
        assert re.search(r'libholonight_qml.so.*loaded library', log), 'Holonight style plugin not loaded'
    errors = re.findall(r'^.*(?:ReferenceError|TypeError|Cannot assign|Unable to assign|Binding loop|is not a type|is not installed|Required property).*$', log, re.M)
    assert not errors, '\n'.join(errors)
    if installed:
        assert '/build-dependencies/' not in log, 'installed process discovered build dependencies'
        assert '/apps/shell/qml/' not in log, 'installed process discovered source QML'


def main():
    assert os.environ.get("UQC_ISOLATED") == "1", "Use scripts/run-isolated-test.py --hide-host"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--logs', type=Path, required=True)
    args = parser.parse_args()
    build, prefix = args.build.resolve(), args.prefix.resolve()
    args.logs.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='uqc102-launch-') as directory:
        root = Path(directory)
        stage = root / 'installed'
        shutil.copytree(prefix, stage)
        subprocess.run(['cmake', '--install', str(build), '--prefix', str(stage)], check=True, stdout=subprocess.DEVNULL)
        # A second location proves that executable-relative discovery survives relocation.
        relocated = root / 'relocated'
        stage.rename(relocated)
        for installed in (False, True):
            location = 'installed' if installed else 'build'
            dependency = relocated if installed else prefix
            binaries = {'shell': relocated / 'bin/holonight-shell' if installed else build / 'holonight-shell',
                        'askpass': relocated / 'libexec/holonight-askpass' if installed else build / 'apps/authentication/holonight-askpass',
                        'polkit': relocated / 'libexec/holonight-polkit-agent' if installed else build / 'apps/authentication/holonight-polkit-agent'}
            if installed:
                for alias in ('holonight-askpass', 'holonight-sudo-askpass', 'holonight-ssh-askpass'):
                    binaries[alias] = relocated / 'bin' / alias
            for frontend, executable in binaries.items():
                for mode in ('default', 'environment', 'cli', 'precedence', 'external-config'):
                    env = dict(os.environ)
                    for name in ('QT_QUICK_CONTROLS_STYLE', 'QT_QUICK_CONTROLS_CONF', 'QML_IMPORT_PATH',
                                 'QML2_IMPORT_PATH', 'LD_LIBRARY_PATH', 'WAYLAND_DISPLAY', 'SWAYSOCK',
                                 'HYPRLAND_INSTANCE_SIGNATURE', 'UQC_STYLE_CLI'):
                        env.pop(name, None)
                    env.update(QML_IMPORT_TRACE='1', QT_DEBUG_PLUGINS='1', QT_QPA_PLATFORMTHEME='',
                               QT_LOGGING_RULES='holonight.controls.runtime.debug=true;qt.qml.import.debug=true;qt.core.plugin.loader.debug=true')
                    expected = 'Holonight' if mode == 'default' else 'Fusion'
                    if mode in ('environment', 'precedence'):
                        env['QT_QUICK_CONTROLS_STYLE'] = 'Fusion' if mode == 'environment' else 'Holonight'
                    cli = ['-style', 'Fusion'] if mode in ('cli', 'precedence') else []
                    if mode == 'external-config':
                        config = root / 'controls.conf'
                        config.write_text('[Controls]\nStyle=Fusion\n')
                        env['QT_QUICK_CONTROLS_CONF'] = str(config)
                    name = f'{location}-{frontend}-{mode}'
                    logfile = args.logs / f'{name}.log'
                    if frontend == 'polkit':
                        env['UQC_POLKIT_EXECUTABLE'] = str(executable)
                        env['UQC_POLKIT_LOG'] = str(logfile.resolve())
                        if cli:
                            env['UQC_STYLE_CLI'] = '1'
                        result = subprocess.run([str(build / 'tests/test_holonight_authentication'),
                                                 '--gtest_filter=PolkitAgentProcess.SigtermExitsPersistentDialogAndUnregisters'],
                                                env=env, capture_output=True, timeout=20)
                        assert result.returncode == 0, result.stdout.decode() + result.stderr.decode()
                        verify(logfile.read_text(), expected, installed, dependency)
                        print(f'PASS {name}', flush=True)
                        continue
                    compositor = None
                    compositor_log = None
                    try:
                        if frontend == 'shell':
                            runtime = root / name
                            runtime.mkdir(mode=0o700)
                            config = root / 'sway.conf'
                            config.write_text('output HEADLESS-1 mode 1280x720\n')
                            compositor_log = (args.logs / f'{name}-sway.log').open('w')
                            sway_env = dict(env, XDG_RUNTIME_DIR=str(runtime), WLR_BACKENDS='headless',
                                            WLR_RENDERER='pixman', WLR_LIBINPUT_NO_DEVICES='1')
                            compositor = subprocess.Popen(['sway', '--unsupported-gpu', '--config', str(config)],
                                                          env=sway_env, stdout=compositor_log, stderr=subprocess.STDOUT)
                            deadline = time.monotonic() + 10
                            while not list(runtime.glob('wayland-*')) and time.monotonic() < deadline:
                                assert compositor.poll() is None, 'headless Sway exited'
                                time.sleep(0.05)
                            sockets = [p for p in runtime.glob('wayland-*') if p.is_socket()]
                            assert len(sockets) == 1, 'no private Wayland socket'
                            env.update(XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY=sockets[0].name,
                                       SWAYSOCK=str(next(runtime.glob('sway-ipc.*.sock'))),
                                       XDG_CURRENT_DESKTOP='sway', QT_QPA_PLATFORM='wayland')
                        command = [str(executable)] + cli
                        if frontend == 'shell':
                            command = ['/usr/bin/stdbuf', '-oL'] + command + ['--debug', '--no-log-file']
                        else:
                            command += ['Synthetic acceptance prompt']
                        with logfile.open('w') as log, tempfile.TemporaryFile() as output:
                            child = subprocess.Popen(command, env=env, stdout=log if frontend == 'shell' else output, stderr=log)
                            try:
                                deadline = time.monotonic() + 3
                                while time.monotonic() < deadline:
                                    assert child.poll() is None, f'{name}: premature exit {child.returncode}; see {logfile}'
                                    time.sleep(0.05)
                            finally:
                                stop(child)
                            if frontend != 'shell':
                                output.seek(0)
                                assert output.read() == b'', 'askpass polluted its protocol'
                        verify(logfile.read_text(), expected, installed, dependency)
                        print(f'PASS {name}', flush=True)
                    finally:
                        if compositor is not None:
                            stop(compositor)
                        if compositor_log is not None:
                            compositor_log.close()

        for module, style in (('qmldir', 'Holonight'), ('Core', 'Fusion'), ('Controls', 'Fusion')):
            target = relocated / 'lib/qt6/qml/Holonight' / module
            hidden = target.with_name(target.name + '.hidden')
            target.rename(hidden)
            try:
                env = dict(os.environ, QT_QUICK_CONTROLS_STYLE=style)
                for variable in ('QML_IMPORT_PATH', 'QML2_IMPORT_PATH', 'LD_LIBRARY_PATH', 'QT_QUICK_CONTROLS_CONF'):
                    env.pop(variable, None)
                result = subprocess.run([str(relocated / 'bin/holonight-sudo-askpass'), 'Synthetic prompt'],
                                        env=env, capture_output=True, timeout=10)
                assert result.returncode != 0, 'missing required module unexpectedly succeeded'
                assert result.stdout == b'', 'missing-module failure polluted askpass stdout'
                assert b'classification=missing-module' in result.stderr, result.stderr.decode()
                assert b'correct its QML prefix' in result.stderr, result.stderr.decode()
                (args.logs / f'missing-{module}.log').write_bytes(result.stderr)
                print(f'PASS missing {module}: actionable diagnostic and empty protocol', flush=True)
            finally:
                hidden.rename(target)


if __name__ == '__main__':
    main()
