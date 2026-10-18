# Standalone portal migration (UPB-002)

The shell stops publishing Settings and installing holonight.portal. Remove the
publication-only ThemeService; preserve AppearanceService and broker consumers.
The standalone backend owns both Settings and FileChooser, using installed
appearance infrastructure independently of shell lifetime. All shipped routing
preferences select holonight;gtk for both interfaces, preserving other defaults.

Diagnostics retain their identifiers and explain independent D-Bus activation.
Shell appearance regressions replace the obsolete in-process portal ordering
contract. Routing checks cover source and stage, including descriptor absence.
Backend protocol and watcher checks live in the backend repository.
