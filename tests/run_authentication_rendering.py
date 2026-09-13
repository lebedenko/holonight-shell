#!/usr/bin/env python3
"""Capture compiled synthetic authentication cases in a private headless Sway.

Run through scripts/run-isolated-test.py to supply a disposable home and bus.
This supplementary graphics check requires the existing Sway/Mesa installation.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--logs', type=Path, required=True)
    args = parser.parse_args()
    args.logs.mkdir(parents=True, exist_ok=False)
    if os.environ.get('QT_QPA_PLATFORM') != 'offscreen':
        parser.error('run through scripts/run-isolated-test.py')
    source = Path(__file__).resolve().parent / 'qml/tst_AuthenticationRealModel.qml'
    with tempfile.TemporaryDirectory(prefix='authentication-rendering-') as directory:
        runtime = Path(directory)
        env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), QT_QPA_PLATFORM='wayland',
                   QT_QUICK_BACKEND='rhi', QSG_RHI_BACKEND='opengl', QSG_RENDER_LOOP='basic',
                   LIBGL_ALWAYS_SOFTWARE='1')
        config = runtime / 'sway.conf'
        config.write_text('output HEADLESS-1 mode 1280x960\nfor_window [app_id=".*"] floating enable\n')
        with (args.logs / 'compositor.log').open('w') as log:
            compositor = subprocess.Popen(['sway', '--unsupported-gpu', '--config', str(config)],
                env=dict(env, WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_LIBINPUT_NO_DEVICES='1'),
                stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 10
                sockets = []
                while time.monotonic() < deadline:
                    assert compositor.poll() is None, 'private compositor exited'
                    sockets = [path for path in runtime.glob('wayland-*') if path.is_socket()]
                    if sockets:
                        break
                    time.sleep(0.05)
                assert len(sockets) == 1, 'missing private compositor socket'
                env['WAYLAND_DISPLAY'] = sockets[0].name
                results = []
                for style in ('Holonight', 'Fusion'):
                    for scale in ('1', '1.25'):
                        case_env = dict(env, QT_QUICK_CONTROLS_STYLE=style, QT_SCALE_FACTOR=scale,
                                        UQC_AUTH_EVIDENCE_DIR=str(args.logs.resolve()))
                        with (args.logs / f'{style}-{scale}.log').open('w') as output:
                            result = subprocess.run([str(args.binary.resolve()), '-input', str(source)],
                                env=case_env, stdout=output, stderr=subprocess.STDOUT, timeout=60)
                        results.append(dict(style=style, scale=scale, exit_status=result.returncode))
                (args.logs / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                return int(any(result['exit_status'] for result in results))
            finally:
                compositor.terminate()
                try:
                    compositor.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    compositor.kill()
                    compositor.wait()


if __name__ == '__main__':
    raise SystemExit(main())
