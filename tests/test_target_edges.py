#!/usr/bin/env python3
"""Negative fixtures for the shell target dependency policy."""

import importlib.util
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/check-target-edges.py"
SPEC = importlib.util.spec_from_file_location("target_edges", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class TargetEdgesTest(unittest.TestCase):
    def check(self, source: str) -> list[str]:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "CMakeLists.txt"
            path.write_text(source)
            return MODULE.check_file(path)

    def test_rejects_upward_dependency(self):
        errors = self.check("target_link_libraries(holonight_platform PUBLIC holonight_services)")
        self.assertEqual(len(errors), 1)
        self.assertIn("holonight_platform must not link holonight_services", errors[0])

    def test_allows_reviewed_surface_service_edge(self):
        self.assertEqual(self.check("target_link_libraries(holonight_surfaces PRIVATE holonight_services)"), [])

    def test_rejects_authentication_to_shell_dependency(self):
        errors = self.check("target_link_libraries(holonight_authentication_core PRIVATE holonight_core)")
        self.assertEqual(len(errors), 1)


if __name__ == "__main__":
    unittest.main()
