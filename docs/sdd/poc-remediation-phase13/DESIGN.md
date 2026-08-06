# Phase 13 — Adaptive Status Popup Sizing: Design

**Input**: `poc-remediation-phase13/SPEC.md`
**Baseline**: Phase 12 accepted.

## 1. Problem Boundary

`StatusPopupGeometry.cpp` currently maps a popup ID directly to one fixed
content size. `StatusPopupSurface` applies that size before loading
`StatusPopup.qml`, whose Loader then gives the rich content a fixed area. This
keeps layer-shell surfaces stable, but makes Weather scroll on displays where a
modestly taller frame would fit naturally.

The phase keeps the stable-surface property and replaces only the fixed-size
lookup with a monitor-aware policy.

## 2. Sizing Policy

Introduce a C++ value type owned by `StatusPopupGeometry`:

| Field | Meaning |
|---|---|
| `minimum_content_size` | smallest usable content area |
| `preferred_content_size` | comfortable normal frame on a roomy monitor |
| `maximum_content_size` | design cap before monitor constraints |
| `overflow_mode` | fixed informational content or internally scrolling collection |

Weather uses a preferred height high enough for its existing composition,
bounded by a deliberate design maximum. Audio and Network retain their current
designed preferred frames and internal list scrolling. Battery and fallback
popups retain compact bounds.

The first implementation intentionally uses declared policies, not arbitrary
runtime measurement of `Loader.item`. That avoids an asynchronous resize cycle,
post-show geometry jump, and repeated resizes driven by dynamic content.

## 3. Geometry Resolution

1. `StatusPopupSurface` selects the target `QScreen` as it does today.
2. It passes the target screen's geometry and available geometry to
   `statusPopupGeometry`.
3. Geometry resolves the policy's preferred size, limits it by its design
   maximum and the monitor-safe available content area, and never goes below the
   policy minimum unless the monitor itself is too small to satisfy that
   minimum.
4. The returned surface size includes `kTopPadding`, the notch band, and glow
   padding. Horizontal placement and pointer clamping continue to derive from
   the resolved content width.
5. The resolved size is committed once before `StatusPopup.qml` is shown.

The vertical cap must reserve the layer-shell top gap and a small screen-edge
margin. It uses the selected screen's available geometry rather than a global
desktop coordinate, so both monitors receive correct bounds.

## 4. QML Overflow Behavior

`WeatherPopupContent` keeps the Phase 12 `Flickable`. Its viewport already
becomes non-interactive when `contentHeight <= height`; therefore the enlarged
preferred Weather frame removes the scroll interaction on roomy displays
without a parallel QML policy. When geometry caps the frame, the same viewport
clips and scrolls only the real overflow.

No outer resizing is triggered from QML. Audio and Network continue to own
scrolling at their list controls, preventing Wi-Fi scans and audio stream
changes from moving the popup during use.

## 5. Test Strategy

| Layer | Scenario | Assertion |
|---|---|---|
| C++ | policy lookup | known IDs have intended preferred/max bounds and fallback is safe |
| C++ | roomy target screen | Weather resolves to preferred size and valid pointer placement |
| C++ | short target screen | Weather height is capped and surface remains inside safe area |
| C++ | edge anchor | content remains on-screen and pointer stays within notch limits |
| QML | preferred Weather viewport | content fits and Flickable is non-interactive |
| QML | capped Weather viewport | content overflows, clips, and reaches its final row by scrolling |
| Live shell | both monitor profiles | Weather is naturally sized where possible and scrollable only where constrained |

## 6. Risks and Decisions

- `QScreen::availableGeometry()` can be equal to full geometry on some Wayland
  configurations. The policy must still apply its own edge margin and must not
  assume the top bar has been subtracted.
- A surface may be configured smaller than its policy minimum on an unusually
  small display. The resulting overflow behavior is preferable to an off-screen
  frame.
- The phase deliberately does not generalize tray menus or tooltips. They use
  different surfaces and should be designed separately if needed.
