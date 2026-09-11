"""Check the manual runner's completion/timeout distinction without Polkit."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

source = Path(__file__).resolve().parents[1] / 'docs/sdd/unified-qtquick-controls/fixtures/bounded_challenge.py'
spec = importlib.util.spec_from_file_location('bounded_challenge', source)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class BoundedChallenge(unittest.TestCase):
    def test_denial_is_normal_but_success_and_timeout_are_not_cancellation(self):
        for program, timeout, expected, outcome in (
            ('raise SystemExit(126)', 3, 0, 'normal-exit'),
            ('raise SystemExit(127)', 3, 0, 'normal-exit'),
            ('raise SystemExit(0)', 3, 1, 'normal-exit'),
            ('import time; time.sleep(30)', 0.1, 1, 'timeout'),
        ):
            with self.subTest(outcome=outcome, program=program), tempfile.TemporaryDirectory() as directory:
                run = Path(directory)
                self.assertEqual(module.run_challenge(run, timeout, [sys.executable, '-c', program]), expected)
                evidence = (run / 'challenge.txt').read_text()
                self.assertIn('outcome=' + outcome, evidence)
                if outcome == 'timeout':
                    self.assertIn('exit_status=124', evidence)


if __name__ == '__main__':
    unittest.main()
