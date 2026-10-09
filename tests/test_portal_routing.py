#!/usr/bin/env python3
"""Verify FileChooser routing independently of Settings and compositor preferences."""

import configparser
import os
from pathlib import Path
import unittest

SOURCE = Path(__file__).resolve().parents[1]
STAGE = os.environ.get("HOLONIGHT_PORTAL_STAGE")


class PortalRouting(unittest.TestCase):
    def test_desktop_preferences(self):
        for desktop, default, settings in (("holonight", "*", "holonight;*"),
                                           ("hyprland", "hyprland;gtk", "gtk;holonight"),
                                           ("sway", "wlr;gtk", "gtk;holonight"),
                                           ("labwc", "wlr;gtk", "gtk;holonight")):
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
                    "org.freedesktop.impl.portal.settings": settings,
                    "org.freedesktop.impl.portal.filechooser": "holonight-filechooser;gtk",
                })

    def test_settings_descriptor_keeps_its_identity(self):
        path = (Path(STAGE) / "share/xdg-desktop-portal/portals/holonight.portal" if STAGE
                else SOURCE / "data/xdg-desktop-portal/portals/holonight.portal")
        config = configparser.ConfigParser(interpolation=None)
        self.assertEqual(config.read(path), [str(path)])
        self.assertEqual(config["portal"]["DBusName"], "org.freedesktop.impl.portal.desktop.holonight")
        self.assertEqual(config["portal"]["Interfaces"], "org.freedesktop.impl.portal.Settings;")


if __name__ == "__main__":
    unittest.main()
