#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location("uninstall", Path(__file__).resolve().parents[1] / "scripts/uninstall.py")
uninstaller = importlib.util.module_from_spec(spec)
spec.loader.exec_module(uninstaller)


class UninstallTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.prefix = self.root / "prefix with spaces"
        self.prefix.mkdir()
        self.binary = self.prefix / "binary"
        self.binary.write_text("installed")
        self.unrelated = self.prefix / "unrelated"
        self.unrelated.write_text("keep")
        self.manifest = self.root / "install_manifest.txt"
        self.manifest.write_text(f"{self.binary}\n")

    def test_dry_run_is_default_and_preserves_manifest(self):
        uninstaller.uninstall(self.manifest, self.prefix, False)
        self.assertTrue(self.binary.exists())
        self.assertTrue(self.manifest.exists())

    def test_deletes_only_recorded_files_and_symlinks_and_tolerates_missing_files(self):
        outside = self.root / "outside"
        outside.write_text("keep")
        link = self.prefix / "alias"
        link.symlink_to(outside)
        self.manifest.write_text(f"{self.binary}\n{link}\n{self.prefix / 'missing'}\n")
        uninstaller.uninstall(self.manifest, self.prefix, True)
        self.assertFalse(self.binary.exists())
        self.assertFalse(link.is_symlink())
        self.assertFalse(self.manifest.exists())
        self.assertTrue(outside.exists())
        self.assertTrue(self.unrelated.exists())

    def test_validates_all_entries_before_deleting_anything(self):
        for invalid in (self.root / "outside", Path("relative"), self.prefix / ".." / "outside", self.prefix):
            with self.subTest(invalid=invalid):
                self.manifest.write_text(f"{self.binary}\n{invalid}\n")
                with self.assertRaises(ValueError):
                    uninstaller.uninstall(self.manifest, self.prefix, True)
                self.assertTrue(self.binary.exists())
                self.assertTrue(self.manifest.exists())

    def test_rejects_parent_symlink_escape_and_directories(self):
        (self.prefix / "escape").symlink_to(self.root, target_is_directory=True)
        for invalid in (self.prefix / "escape" / "outside", self.prefix / "directory"):
            if invalid.name == "directory":
                invalid.mkdir()
            self.manifest.write_text(f"{invalid}\n")
            with self.assertRaises(ValueError):
                uninstaller.uninstall(self.manifest, self.prefix, True)

    def test_rejects_root_prefix_including_symlink(self):
        alias = self.root / "root-alias"
        alias.symlink_to("/", target_is_directory=True)
        for prefix in (Path("/"), alias, Path("relative")):
            with self.assertRaises(ValueError):
                uninstaller.uninstall(self.manifest, prefix, True)

    def test_missing_manifest_is_a_noop(self):
        uninstaller.uninstall(self.root / "missing", self.prefix, True)
        self.assertTrue(self.binary.exists())


if __name__ == "__main__":
    unittest.main()
