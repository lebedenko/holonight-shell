#!/usr/bin/env python3
"""Actual build and relocated installation launches; invoke inside run-isolated-test --hide-host."""
import argparse
import contextlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
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


def verify(log, style, installed, prefix, build):
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
        assert str(build) not in log, 'installed process discovered the shell build directory'
        assert '/build-dependencies/' not in log, 'installed process discovered build dependencies'
        assert '/apps/shell/qml/' not in log, 'installed process discovered source QML'


def stage_runtime(runtime, stage):
    """Copy only the resolved QML provider and Config's library/symlink chain."""
    shutil.copytree(runtime['qml_source'], stage / runtime['qml_destination'])
    source = Path(runtime['config_source'])
    destination = stage / runtime['lib_destination']
    destination.mkdir(parents=True, exist_ok=True)
    resolved = source.resolve(strict=True)
    shutil.copy2(resolved, destination / resolved.name)
    # Rewrite absolute links and links outside the source directory to keep
    # every link inside the relocated stage.
    while source.is_symlink():
        target = source.readlink()
        target = target if target.is_absolute() else source.parent / target
        (destination / source.name).symlink_to(target.name)
        source = target
    soname = destination / runtime['config_soname']
    if not soname.exists():
        soname.symlink_to(resolved.name)


def provider_masks(runtime):
    """Mask configured providers and system fallback providers, never their prefix."""
    qml = {Path(runtime['qml_source']).resolve()}
    libraries = {Path(runtime['config_source']).resolve()}
    libdirs = {Path(runtime['config_source']).parent}
    for prefix in (Path('/usr'), Path('/usr/local')):
        libdirs.update((prefix / 'lib', prefix / 'lib64'))
        libdirs.update((prefix / 'lib').glob('*-linux-gnu'))
    for directory in libdirs:
        qml.add((directory / 'qt6/qml/Holonight').resolve())
        for pattern in ('*holonight*.so*', '*HoloNight*.so*'):
            libraries.update(path.resolve() for path in directory.glob(pattern) if path.is_file())
    command = []
    for path in sorted(qml):
        if path.is_dir():
            command += ['--tmpfs', str(path)]
    for path in sorted(libraries):
        if path.is_file():
            command += ['--ro-bind', '/dev/null', str(path)]
    return command


def installed_command(runtime, command):
    # UQC_ISOLATED identifies the fresh acceptance container: it has no host
    # providers and must not require nested user namespaces. Native service
    # isolation has a separate marker because its providers remain readable.
    if os.environ.get('UQC_ISOLATED') == '1':
        return command
    return ['bwrap', '--die-with-parent', '--bind', '/', '/', '--dev-bind', '/dev', '/dev',
            *provider_masks(runtime), '--', *command]


def main():
    assert any(os.environ.get(name) == "1" for name in ("UQC_ISOLATED", "UQC_NATIVE_ISOLATED")), "Use scripts/run-isolated-test.py --hide-host"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--installed-root', type=Path, help=argparse.SUPPRESS)
    parser.add_argument('--logs', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    runtime = json.loads(args.runtime.read_text())
    args.logs.mkdir(parents=True, exist_ok=True)
    with (contextlib.nullcontext(args.installed_root) if args.installed_root else
          tempfile.TemporaryDirectory(prefix='uqc102-launch-')) as directory:
        root = Path(directory)
        sway = root / 'sway'
        relocated = root / 'relocated'
        installed = args.installed_root is not None
        if not installed:
            # Copy only bytes: Arch's cap_sys_nice attribute prevents exec in
            # an ordinary acceptance container.
            shutil.copyfile(shutil.which('sway'), sway)
            sway.chmod(0o755)
            stage = root / 'installed'
            stage_runtime(runtime, stage)
            subprocess.run(['cmake', '--install', str(build), '--prefix', str(stage)],
                           check=True, stdout=subprocess.DEVNULL)
            stage.rename(relocated)
        location = 'installed' if installed else 'build'
        dependency = relocated if installed else Path(runtime['qml_source']).parent
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
                    if installed:
                        env['LD_LIBRARY_PATH'] = str(relocated / runtime['lib_destination'])
                        env['QML_IMPORT_PATH'] = str((relocated / runtime['qml_destination']).parent)
                    if cli:
                        env['UQC_STYLE_CLI'] = '1'
                    result = subprocess.run([str(build / 'tests/test_holonight_authentication'),
                                             '--gtest_filter=PolkitAgentProcess.SigtermExitsPersistentDialogAndUnregisters'],
                                            env=env, capture_output=True, timeout=20)
                    assert result.returncode == 0, result.stdout.decode() + result.stderr.decode()
                    verify(logfile.read_text(), expected, installed, dependency, build)
                    print(f'PASS {name}', flush=True)
                    continue
                compositor = None
                compositor_log = None
                try:
                    if frontend == 'shell':
                        wayland_runtime = root / name
                        wayland_runtime.mkdir(mode=0o700)
                        config = root / 'sway.conf'
                        config.write_text('output HEADLESS-1 mode 1280x720\n')
                        compositor_log = (args.logs / f'{name}-sway.log').open('w')
                        sway_env = dict(env, XDG_RUNTIME_DIR=str(wayland_runtime), WLR_BACKENDS='headless',
                                        WLR_RENDERER='pixman', WLR_LIBINPUT_NO_DEVICES='1')
                        compositor = subprocess.Popen([str(sway), '--unsupported-gpu', '--config', str(config)],
                                                      env=sway_env, stdout=compositor_log, stderr=subprocess.STDOUT)
                        deadline = time.monotonic() + 10
                        while not list(wayland_runtime.glob('wayland-*')) and time.monotonic() < deadline:
                            assert compositor.poll() is None, 'headless Sway exited'
                            time.sleep(0.05)
                        sockets = [p for p in wayland_runtime.glob('wayland-*') if p.is_socket()]
                        assert len(sockets) == 1, 'no private Wayland socket'
                        env.update(XDG_RUNTIME_DIR=str(wayland_runtime), WAYLAND_DISPLAY=sockets[0].name,
                                   SWAYSOCK=str(next(wayland_runtime.glob('sway-ipc.*.sock'))),
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
                            if frontend == 'shell':
                                mappings = Path(f'/proc/{child.pid}/maps').read_text()
                                assert 'libholonight_backend_sway.so' in mappings, 'selected plugin was not loaded'
                                assert 'libholonight_backend_hyprland.so' not in mappings, 'inactive Hyprland plugin loaded'
                                assert 'libholonight_backend_wayland.so' not in mappings, 'inactive generic plugin loaded'
                                if installed:
                                    assert str(relocated / runtime['lib_destination'] / 'holonight/backends') in mappings
                        finally:
                            stop(child)
                        if frontend != 'shell':
                            output.seek(0)
                            assert output.read() == b'', 'askpass polluted its protocol'
                    verify(logfile.read_text(), expected, installed, dependency, build)
                    print(f'PASS {name}', flush=True)
                finally:
                    if compositor is not None:
                        stop(compositor)
                    if compositor_log is not None:
                        compositor_log.close()

        if not installed:
            command = [sys.executable, str(Path(__file__).resolve()), '--build', str(build),
                       '--runtime', str(args.runtime.resolve()), '--logs', str(args.logs.resolve()),
                       '--installed-root', str(root)]
            subprocess.run(installed_command(runtime, command), check=True)
            return

        for module, style in (('qmldir', 'Holonight'), ('Core', 'Fusion'), ('Controls', 'Fusion')):
            target = relocated / runtime['qml_destination'] / module
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

        # Prove the loader cannot rescue a broken stage from the native /usr
        # installation (or the configured development prefix).
        library = (relocated / runtime['lib_destination'] / runtime['config_soname']).resolve()
        hidden = library.with_name(library.name + '.hidden')
        library.rename(hidden)
        try:
            result = subprocess.run([str(relocated / 'bin/holonight-sudo-askpass'), 'Synthetic prompt'],
                                    env=env, capture_output=True, timeout=10)
            assert result.returncode != 0, 'missing Config library unexpectedly succeeded'
            assert result.stdout == b'', 'missing-library failure polluted askpass stdout'
            assert runtime['config_soname'].encode() in result.stderr, result.stderr.decode()
            (args.logs / 'missing-config-library.log').write_bytes(result.stderr)
            print('PASS missing Config library: no provider fallback and empty protocol', flush=True)
        finally:
            hidden.rename(library)


if __name__ == '__main__':
    main()
