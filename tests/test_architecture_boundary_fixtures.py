#!/usr/bin/env python3
"""Exercise source boundary rules on disposable project trees."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


CHECKER = Path(__file__).resolve().parents[1] / "scripts/check-architecture-boundaries.sh"


class ArchitectureBoundaryFixturesTest(unittest.TestCase):
    def check(self, relative_path: str, source: str) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ("apps/authentication", "libs/holonight-authentication",
                         "libs/holonight-services/src", "libs/holonight-surfaces/src",
                         "qml/Authentication", "tests", "cmake", "scripts"):
                (root / name).mkdir(parents=True)
            (root / "libs/holonight-services/src/ForbiddenService.h").write_text("#pragma once\n")
            (root / "libs/holonight-services/CMakeLists.txt").write_text("add_library(holonight_services STATIC)\n")
            target = root / relative_path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(source)
            environment = dict(os.environ, ARCHITECTURE_CHECK_ROOT=str(root))
            return subprocess.run(["bash", str(CHECKER)], env=environment, text=True,
                                  capture_output=True, check=False)

    def test_rejects_unauthorized_surface_service_include(self):
        result = self.check("libs/holonight-surfaces/src/Widget.cpp", '#include "ForbiddenService.h"\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unexpected service header", result.stderr)

    def test_rejects_authentication_to_shell_include(self):
        result = self.check("libs/holonight-authentication/Entry.cpp", '#include "HolonightShell.h"\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Authentication targets", result.stderr)

    def test_allows_reviewed_surface_service_include(self):
        result = self.check("libs/holonight-surfaces/src/Widget.cpp", '#include "NotificationService.h"\n')
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
