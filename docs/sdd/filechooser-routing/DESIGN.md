# FileChooser routing design

Only configuration ownership changes: each desktop preference selects the
standalone HoloNight FileChooser backend first and GTK as fallback. Shell's
Settings service remains independent. Existing integration install rules stage
these configuration files, so no new runtime service or dependency is added to
Shell. Tests parse installed INI data and assert preserved unrelated preferences.

Files provider and backend publication/pinning precede implementation. Keep
Shell implementation and verification in this repository, with the umbrella
ledger linking this SDD and recording the accepted commit afterward.
