# Phase 12 — Popup Resilience and Settings Defaults: Tasks

**Status**: Accepted

## Pre-flight

- [x] T-110: Revalidate the four Phase 7 findings against the accepted Phase 11
  baseline.
  - Check: confirm the three effects still follow their source shapes, the
    brightness timer still restarts, Weather has no bounded viewport, and the
    model still initializes `cyan`.
  - Result: all four findings remained present at the Phase 11 baseline.

## Implementation and Tests

- [x] T-111: Put tray-menu, tooltip, and calendar glows behind their source
  shapes.
  - REQs: REQ-F-01
  - Files: `TrayMenuPopup.qml`, `TooltipPopup.qml`,
    `SidebarOverviewCalendar.qml`, and only necessary QML coverage.
  - Check: components instantiate and the source remains the top visual layer.
  - Result: each `MultiEffect` now precedes its source shape, matching the
    established `StatusPopup.qml` declaration-order convention.

- [x] T-112: Implement a real brightness write throttle.
  - REQs: REQ-F-02
  - Files: `BrightnessSlider.qml`, focused QML test coverage.
  - Check: a continuous drag produces periodic writes; commit produces exactly
    one immediate final write and stops the timer.
  - Result: the timer now repeats at 100 ms after its first drag event and
    retains the latest pending value; release stops it before the final write.

- [x] T-113: Add a scrollable bounded viewport to Weather popup content.
  - REQs: REQ-F-03
  - Files: `WeatherPopupContent.qml`, focused QML test coverage.
  - Check: overflowing content is clipped to the popup viewport and can scroll
    to its final content.
  - Result: the existing stack is contained by a vertical `Flickable` with a
    bounded viewport and content-derived height.

- [x] T-114: Align settings-model accent initialization with the catalog.
  - REQs: REQ-F-04
  - Files: `SettingsEditModel.h`, `tests/test_settings_app.cpp`.
  - Check: a newly constructed model reports `ThemeConfigFile::defaultAccent()`
    and remains clean.
  - Result: both model accent initializers now call `ThemeConfigFile`, and a
    settings unit test verifies the fresh-model behavior.

## Validation and Handoff

- [x] T-115: Run focused and project validation.
  - Check: affected QML/settings tests, `task qml-lint`, `task qmltypes-check`,
    `task test`, and `git diff --check` pass.
  - Result: all checks passed. The QML harness covers periodic drag writes,
    immediate final brightness write, and Weather viewport scrolling.

- [x] T-116: Perform live compositor verification.
  - Steps:
    1. Open the tray menu and a tooltip; confirm their glows sit behind borders.
    2. Open the sidebar calendar; confirm the current-day glow is behind its
       frame and numeral.
    3. Drag brightness continuously, then release; confirm smooth feedback and
       final value application.
    4. Open Weather and scroll to the final details row at the popup's fixed
       size.
  - Result: user verified the glow, brightness, settings, and Weather overflow
    behavior in the live shell. The fixed-size Weather frame remains usable,
    but its unnecessary scroll on a roomy display exposed the broader popup
    sizing issue now tracked by Phase 13.

- [x] T-117: Record acceptance and update the Phase 7 handoff.
  - Result: Phase 12 is accepted as the bounded-overflow safeguard. Its
    Weather viewport remains intentionally in place until Phase 13 supplies
    adaptive, monitor-bounded popup geometry; logging follow-ups remain out of
    scope.
