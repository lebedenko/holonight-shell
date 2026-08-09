# CTV-103 design

Settings writes canonical appearance and orchestrates the production adapter. Shell only queries the adapter during
session bootstrap, then publishes the already-existing Settings portal projection from `AppearanceService`.

`AppearanceService::cursorThemeChanged` updates `SessionIntegrationService` through
`setExpectedCursorTheme(QString)`. Diagnostics compare that expected value with the process environment, query
systemd and desktop-service ownership concurrently, and identify the HoloNight portal as owned, missing, or
unavailable. If the canonical cursor changes while a refresh is active, one follow-up refresh is queued; a mutex
protects the expected value read by diagnostic workers.

The smoke script resolves the identical canonical path, checks process and systemd activation inputs, reads live
portal color/accent values when possible, and labels live, application-relaunch, and session-restart effects.
