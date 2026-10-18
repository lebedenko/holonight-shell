#!/usr/bin/env python3
"""Verify unified portal routing and the shell payload boundary."""

import configparser
import os
from pathlib import Path
import unittest

SOURCE = Path(__file__).resolve().parents[1]
STAGE = os.environ.get("HOLONIGHT_PORTAL_STAGE")


class PortalRouting(unittest.TestCase):
    def test_desktop_preferences(self):
        for desktop, default in (("holonight", "*"), ("hyprland", "hyprland;gtk"),
                                 ("sway", "wlr;gtk"), ("labwc", "wlr;gtk")):
            with self.subTest(desktop=desktop):
                if STAGE:
                    relative = "share/xdg-desktop-portal" if desktop == "holonight" else "share/holonight/xdg/xdg-desktop-portal"
                    path = Path(STAGE) / relative / f"{desktop}-portals.conf"
                else:
                    relative = "data/xdg-desktop-portal" if desktop == "holonight" else "data/xdg/xdg-desktop-portal"
                    path = SOURCE / relative / f"{desktop}-portals.conf"
                config = configparser.ConfigParser(interpolation=None)
                self.assertEqual(config.read(path), [str(path)])
                preferred = config["preferred"]
                self.assertEqual(dict(preferred), {
                    "default": default,
                    "org.freedesktop.impl.portal.settings": "holonight;gtk",
                    "org.freedesktop.impl.portal.filechooser": "holonight;gtk",
                })

    def test_managed_preferences(self):
        for compositor, default in (("hyprland", "hyprland;gtk"), ("sway", "wlr;gtk"), ("labwc", "wlr;gtk")):
            with self.subTest(compositor=compositor):
                directory = Path(STAGE) / "share/xdg-desktop-portal" if STAGE else SOURCE / "data/xdg-desktop-portal"
                path = directory / f"holonight-{compositor}-portals.conf"
                config = configparser.ConfigParser(interpolation=None)
                self.assertEqual(config.read(path), [str(path)])
                self.assertEqual(dict(config["preferred"]), {
                    "default": default,
                    "org.freedesktop.impl.portal.settings": "holonight;gtk",
                    "org.freedesktop.impl.portal.filechooser": "holonight;gtk",
                })

    def test_shell_does_not_install_backend_descriptor(self):
        path = (Path(STAGE) / "share/xdg-desktop-portal/portals/holonight.portal" if STAGE
                else SOURCE / "data/xdg-desktop-portal/portals/holonight.portal")
        self.assertFalse(path.exists())


if __name__ == "__main__":
    unittest.main()
