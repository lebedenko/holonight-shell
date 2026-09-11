"""Run only the cancellation challenge; a timeout never counts as completion."""
import subprocess
import time


def run_challenge(run, timeout=60, command=None):
    command = command or ['pkexec', '--disable-internal-agent', '/usr/bin/true']
    started = time.monotonic()
    outcome = 'normal-exit'
    try:
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=timeout)
        status, output = result.returncode, result.stdout
        if status < 0:
            outcome = 'signal-exit'
    except subprocess.TimeoutExpired as error:
        # subprocess.run kills and reaps its child before raising. That cleanup
        # is explicitly distinguished from a returned cancellation result.
        outcome, status = 'timeout', 124
        output = error.stdout or b''
        if isinstance(output, bytes):
            output = output.decode(errors='replace')
    except KeyboardInterrupt:
        outcome, status, output = 'interrupted', 130, ''
    elapsed = time.monotonic() - started
    (run / 'challenge.txt').write_text(
        f'{output}\noutcome={outcome}\nexit_status={status}\nelapsed_seconds={elapsed:.3f}\n')
    print(f'Challenge: {outcome}; exit: {status}; evidence: {run / "challenge.txt"}', flush=True)
    # pkexec cancellation is 126; refusal is 127. Prompt observation and live
    # registration are independent prerequisites, checked by the manual helper.
    return 0 if outcome == 'normal-exit' and status in (126, 127) else 1
