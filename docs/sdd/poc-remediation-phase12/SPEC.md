# Phase 12 — Popup Resilience and Settings Defaults

**Status**: Accepted

## Objective

Remediate four small, contained medium-severity items remaining from the Phase
7 triage:

1. render popup and calendar glows behind their source shapes;
2. make brightness writes a genuine 100 ms throttle during a continuous drag;
3. make Weather popup content reachable when its fixed-height composition
   exceeds the available popup height; and
4. initialize the settings edit model with the theme catalog's actual default
   accent.

## Functional Requirements

### REQ-F-01 — Glows render behind their source shapes

The `MultiEffect` glow for the tray menu, tooltip, and current-day calendar
frame shall be ordered behind its source shape. The source border, fill, labels,
and pointer interaction remain visually and functionally unchanged.

**Acceptance**: QML component checks still instantiate the affected surfaces;
live visual inspection shows the source border remains crisp rather than being
overpainted by its glow.

### REQ-F-02 — Brightness writes are throttled during dragging

While a user continuously drags `BrightnessSlider`, at most one brightness
write shall be submitted per 100 ms interval, using the most recently observed
slider value. Releasing the slider shall stop pending periodic writes and submit
the released value immediately.

**Acceptance**: deterministic QML coverage demonstrates periodic writes during
a continuous drag and one immediate final write on release.

### REQ-F-03 — Weather content has an overflow path

The Weather popup shall constrain its content to the popup's available height
and provide scrolling whenever its natural content height exceeds that space.
No forecast or detail content may become unreachable below the popup edge.

**Acceptance**: QML coverage verifies the viewport clips overflowing content and
exposes a content height larger than its viewport; live verification can scroll
to the final details row on the fixed-height popup.

### REQ-F-04 — Settings model defaults use the catalog default accent

Before configuration loading, `SettingsEditModel::themeAccent` and its snapshot
shall equal `ThemeConfigFile::defaultAccent()`. The initializer must not repeat
an accent ID literal that can drift from the theme catalog.

**Acceptance**: a settings-model unit test constructs the model without loading
configuration and observes the catalog default and a clean dirty state.

## Constraints and Verification

- Preserve existing popup sizes, colors, and brightness service API.
- Preserve the final-write-on-release semantics for brightness.
- Do not add logging or change the deferred logging design.
- Keep weather content loaded as it is today; only introduce a viewport for
  overflow.
- Add focused automated coverage before live verification.
- Run relevant QML and settings tests, `task qml-lint`, `task qmltypes-check`,
  `task test`, and `git diff --check`.

## Out of Scope

- The remaining Phase 7 candidates, including tray synchronization,
  QQuickView-status checks, NetworkManager teardown, and launcher rescan state.
- A general popup geometry redesign or adaptive weather layout; tracked as
  Phase 13 after the fixed-height overflow safeguard proved necessary.
- The upcoming logging rework.
