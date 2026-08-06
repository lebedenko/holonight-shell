# Weather Icon Compositor — Specification

**Feature Name:** Weather Icon Compositor  
**Component Type:** Reusable QML component + C++ utility layer  
**Scope Iteration:** Initial (static compositing, single 512x512 resolution)  
**Date:** 2026-06-16

---

## 1. Overview

The Weather Icon Compositor is a reusable component system that renders current-weather visuals by compositing layered 512x512 PNG assets. It determines which layers to display based on OpenWeatherMap condition code, day/night state, and (when applicable at night) moon phase. The system is stateless, resolution-agnostic, and designed to support multiple concurrent instances at different requested sizes without network calls or animation.

---

## 2. Asset Inventory & Reference Table

### Available Assets (21 files in `assets/weather-png/512x512/`)

**Day/Night Condition Overlays:**
- `sun.png`
- `star-field.png`
- `broken-clouds-day.png`, `broken-clouds-night.png`
- `few-clouds-overlay-day.png`, `few-clouds-overlay-night.png`
- `scattered-clouds-day.png`, `scattered-clouds-night.png`
- `heavy-drizzle-day.png`, `heavy-drizzle-night.png`
- `light-drizzle-day.png`, `light-drizzle-night.png`
- `rain-day.png`, `rain-night.png`

**Moon Phase Assets (8 phases):**
- `moon-new.png`
- `moon-waxing-crescent.png`
- `moon-first-quarter.png`
- `moon-waxing-gibbous.png`
- `moon-full.png`
- `moon-waning-gibbous.png`
- `moon-last-quarter.png`
- `moon-waning-crescent.png`

### Condition-to-Layer Mapping Table

| OWM Code(s) | Condition Name | Day Layers (bottom→top) | Night Layers (bottom→top) |
|---|---|---|---|
| 800 | Clear | `sun` | `star-field`, `moon-{phase}` |
| 801 | Few Clouds | `sun`, `few-clouds-overlay-day` | `star-field`, `moon-{phase}`, `few-clouds-overlay-night` |
| 802 | Scattered Clouds | `sun`, `scattered-clouds-day` | `star-field`, `moon-{phase}`, `scattered-clouds-night` |
| 803, 804 | Broken/Overcast Clouds | `sun`, `broken-clouds-day` | `star-field`, `moon-{phase}`, `broken-clouds-night` |
| 300, 301, 310, 313, 315 | Light Drizzle Family | `light-drizzle-day` | `star-field`, `moon-{phase}`, `light-drizzle-night` |
| 302, 311, 312, 314 | Heavy Drizzle Family | `heavy-drizzle-day` | `star-field`, `moon-{phase}`, `heavy-drizzle-night` |
| 500, 501, 502, 503, 504, 520, 521, 522, 531 | Rain Family (all intensities) | `rain-day` | `star-field`, `moon-{phase}`, `rain-night` |

### Unmapped Condition Codes (Yield Empty/Transparent Result)

**Explicitly unmapped, no fallback rendering:**
- 511 (Freezing Rain)
- 2xx (Thunderstorm: 200–232)
- 6xx (Snow: 600–622)
- 7xx (Atmosphere/other: 701, 711, 721, 731, 741, 751, 761, 762, 771, 781)

---

## 3. Requirements

### 3.1 Functional Requirements — Layer Mapping (C++)

#### REQ-F-LM-001: Ubiquitous Condition-to-Layer Mapping

**Sentence:**  
The system shall provide a pure function that accepts an OpenWeatherMap condition code (integer), day/night boolean, and moon phase enum, and returns an ordered list of asset resource paths (bottom-to-top render order) matching the condition-to-layer mapping table exactly.

**Acceptance Criteria:**
- GIVEN condition code 800 (clear), isDay = true, THEN returned list equals exactly `["sun"]`
- GIVEN condition code 800 (clear), isDay = false, moonPhase = FULL, THEN returned list equals exactly `["star-field", "moon-full"]`
- GIVEN condition code 802 (scattered clouds), isDay = true, THEN returned list equals exactly `["sun", "scattered-clouds-day"]`
- GIVEN condition code 802 (scattered clouds), isDay = false, moonPhase = NEW, THEN returned list equals exactly `["star-field", "moon-new", "scattered-clouds-night"]`
- GIVEN condition code 500 (rain), isDay = false, moonPhase = WAXING_CRESCENT, THEN returned list equals exactly `["star-field", "moon-waxing-crescent", "rain-night"]`
- GIVEN condition codes 803 and 804 (both broken/overcast), THEN both return identical layer sequences
- All paths in returned list shall be valid QRC paths with prefix `qrc:/HolonightShell/`

**Rationale:** Ensures the mapping is exact, unambiguous, and testable without QML or rendering.

---

#### REQ-F-LM-002: Unmapped Condition Handling

**Sentence:**  
The system shall return an empty ordered list (not a fallback glyph or placeholder) when given an unmapped condition code (511, 2xx, 6xx, 7xx).

**Acceptance Criteria:**
- GIVEN any thunderstorm code (200–232), WHEN layers are requested, THEN returned list is empty
- GIVEN snow code 600, WHEN layers are requested, THEN returned list is empty
- GIVEN atmosphere code 701 (mist), WHEN layers are requested, THEN returned list is empty
- GIVEN freezing rain code 511, WHEN layers are requested, THEN returned list is empty
- Empty list shall not trigger error logging, warning, or user-facing notification

**Rationale:** Unmapped codes are out of scope; empty result is a deliberate design choice, not a bug.

---

#### REQ-F-LM-003: Layer Mapping Testability

**Sentence:**  
The condition-to-layer mapping function shall be a pure, stateless function with no dependencies on QML, rendering, file-system access, or system state, and shall be testable via GTest with mocked inputs.

**Acceptance Criteria:**
- Function signature accepts only primitive types (int condition code, bool isDay, moon-phase enum) and returns a standard container (e.g., `QList<QString>`)
- No `#include <QQuick*>`, `#include <QQml*>`, or file-system access in the mapping implementation
- Function can be invoked and verified in a pure C++ unit test with no QML engine, no window, no Wayland session
- All 16 day/night/phase combinations for condition 800 (clear) produce distinct, correct outputs under unit test

**Rationale:** Enables fast, deterministic testing without environment setup; simplifies debugging.

---

### 3.2 Functional Requirements — Moon Phase Calculation (C++)

#### REQ-F-MP-001: Synodic-Month Moon Phase Calculation

**Sentence:**  
The system shall calculate lunar phase from a given date/time by computing days since a fixed reference new-moon timestamp, taking modulo the synodic month (~29.530589 days), and bucketing into one of eight equal-width phase windows, yielding one of eight enum values representing the standard lunar phases.

**Acceptance Criteria:**

The synodic month (29.530589 days) is divided into 8 equal-width windows of ~3.6913 days each,
indexed 0-7 in enum declaration order (window k = `[k * 29.530589/8, (k+1) * 29.530589/8)`).
A timestamp's phase is the index of the window its elapsed-days-since-reference (modulo the
synodic month) falls into. The mid-point of each window is used below as a boundary-safe example
value (per the floating-point boundary-jitter risk noted in DESIGN.md §6.2):

- GIVEN a reference new-moon timestamp (e.g., 2000-01-06 18:14 UTC, a known new moon), WHEN the same timestamp is provided, THEN phase is NEW
- GIVEN a timestamp 1.85 days after the reference new moon (mid-window 0), THEN phase is NEW
- GIVEN a timestamp 5.54 days after the reference new moon (mid-window 1), THEN phase is WAXING_CRESCENT
- GIVEN a timestamp 9.23 days after the reference new moon (mid-window 2), THEN phase is FIRST_QUARTER
- GIVEN a timestamp 12.92 days after the reference new moon (mid-window 3), THEN phase is WAXING_GIBBOUS
- GIVEN a timestamp 16.61 days after the reference new moon (mid-window 4), THEN phase is FULL
- GIVEN a timestamp 20.30 days after the reference new moon (mid-window 5), THEN phase is WANING_GIBBOUS
- GIVEN a timestamp 24.00 days after the reference new moon (mid-window 6), THEN phase is LAST_QUARTER
- GIVEN a timestamp 27.69 days after the reference new moon (mid-window 7), THEN phase is WANING_CRESCENT
- GIVEN a timestamp 29.530589 days after the reference new moon, THEN phase wraps to NEW (modulo synodic month)
- Enum values shall be: NEW, WAXING_CRESCENT, FIRST_QUARTER, WAXING_GIBBOUS, FULL, WANING_GIBBOUS, LAST_QUARTER, WANING_CRESCENT
- Phase calculation shall accept a `std::chrono::system_clock::time_point` or equivalent timestamp (not string)

**Rationale:** Provides deterministic moon phase without external API; testable with fixed reference points.

---

#### REQ-F-MP-002: No External Moon Phase Data

**Sentence:**  
Moon phase calculation shall be a pure mathematical function based on the synodic month reference and elapsed time; it shall not fetch data from external services, databases, or APIs.

**Acceptance Criteria:**
- No HTTP/HTTPS requests in moon-phase calculation code path
- No database queries, file reads, or external state lookups
- Calculation uses only arithmetic on the input timestamp and constant parameters (reference new moon, synodic month duration)
- Function can be invoked offline with no network connectivity

**Rationale:** Ensures reliability, low latency, and no external dependencies.

---

#### REQ-F-MP-003: Moon Phase Calculation Testability

**Sentence:**  
Moon phase calculation shall be a pure, deterministic function testable via GTest with fixed timestamps and expected phase outputs, requiring no environment setup.

**Acceptance Criteria:**
- Function signature accepts a timestamp and returns a moon-phase enum
- Given identical input timestamp, function always returns identical phase (deterministic)
- Unit tests verify phase values for at least 8 known timestamps spanning the full lunar cycle
- Test execution requires no system clock, no Wayland, no file access, no network

**Rationale:** Enables regression testing and offline verification.

---

### 3.3 Functional Requirements — QML Rendering Component

#### REQ-F-QML-001: Ubiquitous Composited Icon Rendering

**Sentence:**  
A QML component shall render the weather icon by stacking ordered `Image` elements (one per layer) from bottom to top, with each image sourced from a C++-provided ordered layer list, smoothed and mipmapped for quality downscaling.

**Acceptance Criteria:**
- Component accepts input properties: `conditionCode` (int), `isDay` (bool), `iconSize` (int, in logical pixels), `date` (QDateTime or similar, used for moon-phase calculation internally — see DESIGN.md §2.2; no separate public `moonPhase` property is exposed, to avoid a dual-source-of-truth between `date` and a redundant phase value)
- Component delegates to C++ to obtain ordered layer list given these inputs
- Component iterates the returned layer list via `Repeater`, instantiating one `Image` per layer
- Each `Image` has `sourceSize: Qt.size(root.iconSize, root.iconSize)`, `smooth: true`, `mipmap: true`, `fillMode: Image.PreserveAspectFit`
- Images are stacked in returned order (first element bottom, last element top) via explicit `z` or implicit stacking order
- Rendered output visually matches the reference design assets when all layers are present

**Rationale:** Relies on Qt's pixmap cache for deduplication; no custom painting required.

---

#### REQ-F-QML-002: Multi-Size Concurrent Instantiation

**Sentence:**  
The component shall support multiple concurrent instances on-screen at different requested `iconSize` values, with each instance independently scaling its layers from the shared 512x512 source assets, without blocking or race conditions.

**Acceptance Criteria:**
- GIVEN one instance with `iconSize: 512` and another with `iconSize: 128` on the same screen, both displaying condition 800 (clear), THEN both render correctly with no visual corruption
- GIVEN five instances with `iconSize` values {64, 96, 128, 256, 512}, each loading condition 802 (scattered clouds), THEN all five load and render within 500ms on first display (subject to compositor frame pacing), and subsequent redraws within 16ms
- Qt's pixmap cache shall cache each size independently; decoding the same 512x512 asset at different sizes shall not block other instances
- No mutex, lock, or global state shall serialize rendering across concurrent instances

**Rationale:** Supports use cases like hero icon + forecast strip on the same screen; ensures UI responsiveness.

---

#### REQ-F-QML-003: Layer Visibility for Unmapped Codes

**Sentence:**  
When the C++ mapping function returns an empty layer list (unmapped condition), the QML component shall render no visible content (transparent, zero height/width, or an explicit empty state) without error dialogs or warnings.

**Acceptance Criteria:**
- GIVEN condition code 511 (freezing rain), WHEN the component is instantiated, THEN `Repeater.count` is 0 and no `Image` elements are created
- No error or warning messages appear in Qt logs (`qCDebug`, `qCWarning`, `qCCritical`)
- The component occupies zero visual space by default (or parent layout contracts it)
- Changing `conditionCode` from 802 to 511 smoothly removes all visible layers without flashing or glitches

**Rationale:** Matches specification that unmapped codes produce empty results with no fallback.

---

#### REQ-F-QML-004: Dynamic Input Reactivity

**Sentence:**  
The component shall immediately re-render when any input property (`conditionCode`, `isDay`, `iconSize`, `date`) changes, fetching a new layer list from C++ and updating all `Image` sources accordingly.

**Acceptance Criteria:**
- GIVEN an instance displaying condition 800, `isDay: true`, WHEN `isDay` is set to false and `date` is set to a known full-moon timestamp, THEN within one frame the rendered output updates to show `star-field + moon-full` instead of `sun`
- GIVEN an instance with `iconSize: 128`, WHEN `iconSize` is changed to 256, THEN all `Image.sourceSize` values update to 256x256 and Qt's pixmap cache fetches the appropriately-scaled versions
- Changes to `date` trigger moon-phase recalculation and re-render if on the night side
- No memory leaks or dangling pixmaps when properties change rapidly

**Rationale:** Ensures component integrates seamlessly with reactive QML bindings.

---

### 3.4 Non-Functional Requirements

#### REQ-NF-PERF-001: Pixmap Caching & Deduplication

**Sentence:**  
The component shall leverage Qt's built-in pixmap cache to deduplicate decoded and scaled image data across multiple instances requesting the same source asset at the same target size.

**Acceptance Criteria:**
- Five concurrent instances, all displaying `rain-day.png` scaled to 128x128, shall decode and cache that asset once, with all instances sharing the cached pixmap
- Measured memory footprint for five identical instances at the same size shall be approximately equal to one instance plus metadata overhead, not five independent copies
- Pixmap cache key shall include source path + target size (e.g., `qrc:/.../rain-day.png@128x128`)
- No `QQuickImageProvider` or custom `QQuickPaintedItem` implementation required; rely on `Image` element's native caching

**Rationale:** Reduces memory and CPU across multiple on-screen weather icons.

---

#### REQ-NF-PERF-002: Render-Time Latency

**Sentence:**  
First render (asset decode + scale + composite) of a weather icon shall complete within 200ms on entry-level hardware; subsequent renders and property changes shall update within 16ms (one frame at 60 Hz).

**Acceptance Criteria:**
- Measured from component instantiation (C++ layer lookup, C++ moon-phase calc) through first frame visible on screen: ≤ 200ms for a single 512x512 icon
- Measured from property change (e.g., `moonPhase` or `iconSize`) through next frame rendered: ≤ 16ms
- No synchronous file I/O, decoding, or blocking calls in the render path (Qt handles async pixmap loading)
- Benchmark on a machine representative of the deployment environment (e.g., typical Linux Wayland desktop, not high-end workstation)

**Rationale:** Ensures responsive UI for interactive property changes (e.g., day/night toggle, size adjustment in a UI editor).

---

#### REQ-NF-TEST-001: C++ Unit Test Coverage

**Sentence:**  
The C++ mapping and moon-phase functions shall have unit test coverage of at least 95%, verifying all 16 day/night/phase combinations for representative conditions (800, 801, 802, 300, 500), all 8 moon phases, and all unmapped code families.

**Acceptance Criteria:**
- GTest suite includes at least 40 test cases covering mapping function and moon-phase function
- Tests verify correct return values for inputs on boundary conditions (e.g., new moon, full moon)
- Tests verify that unmapped codes (511, 2xx, 6xx, 7xx) all return empty lists
- `gcov` or equivalent reports ≥ 95% line coverage for mapping and moon-phase implementation files
- All tests pass with `task test` (CTest)

**Rationale:** Ensures correctness and maintainability without manual testing.

---

#### REQ-NF-ARCH-001: No Modification to Existing Weather Path

**Sentence:**  
The new Weather Icon Compositor shall not modify, extend, or depend on the existing SVG-based weather icon path (`WeatherService::iconPath()` → `wsymbol_*.svg` consumed by `WeatherCurrentSection.qml`); the two paths shall remain independent and orthogonal.

**Acceptance Criteria:**
- No changes to `WeatherService` class definition, signatures, or return types
- No changes to `WeatherCurrentSection.qml` or any existing weather-related QML
- The new component does not `import` or call any existing weather service methods except to obtain raw condition code and day/night state
- Existing weather icon rendering in the topbar continues to function identically after the new component is added
- Code review confirms no coupling between SVG path and PNG compositor

**Rationale:** Preserves existing stability; isolates new feature as a self-contained, reusable layer.

---

#### REQ-NF-ARCH-002: Reusable Component, Not Integrated

**Sentence:**  
The Weather Icon Compositor shall be implemented as a standalone, self-contained QML component and C++ utility layer, not wired into any existing screen or widget in this iteration; it shall be available for future consumers to `import` and instantiate as needed.

**Acceptance Criteria:**
- Component URI is `HolonightShell.WeatherIconCompositor` or similar (defined in `qmldir`)
- Component can be instantiated in any QML file via `import HolonightShell; WeatherIconCompositor { ... }`
- No automatic instantiation or binding in existing screens (TopBar, SideBar, etc.)
- C++ layer (mapping + moon-phase) exported as public API (e.g., `WeatherIconMapper` class, `MoonPhaseCalculator` class) with stable, documented signatures
- Future iterations can adopt the component without modifying the present implementation

**Rationale:** Maximizes reusability; prevents accidental coupling.

---

#### REQ-NF-ARCH-003: No Animation

**Sentence:**  
The component shall render a static composite image based on input state; no animation (twinkling, drifting, flashing, or morphing between states) shall be implemented in this iteration.

**Acceptance Criteria:**
- No `SequentialAnimationGroup`, `ParallelAnimation`, `NumberAnimation`, `ColorAnimation`, or equivalent in the component QML
- No custom `onPropertyChanged` handlers that trigger time-based state changes
- Changing `iconSize` instantly updates all layers; no smooth transition animation
- Changing `moonPhase` instantly re-renders; no fade/morph between phase images

**Rationale:** Simplifies scope; animation can be added in a future iteration.

---

#### REQ-NF-ARCH-004: Single Resolution, Extensible Design

**Sentence:**  
Assets in this iteration are sourced from `assets/weather-png/512x512/` exclusively; the component design shall not hardcode resolution paths or assume a single resolution exists forever, but shall not implement multi-resolution support or selection logic in this iteration.

**Acceptance Criteria:**
- Asset paths reference `512x512` folder explicitly in asset bundle configuration
- C++ and QML make no assumptions that future resolutions (e.g., `assets/weather-png/256x256/`) must exist or be auto-selected
- If resolution support is added later, the mapping function signature and layer-list return type require no breaking changes; only the asset bundle paths change
- Asset path prefix (e.g., `qrc:/HolonightShell/`) and layer naming (e.g., `sun.png`) remain identical across hypothetical future resolutions

**Rationale:** Allows future extensibility without redesign.

---

### 3.5 Constraint Requirements

#### REQ-C-NONET: No Network or External Services

**Sentence:**  
The component and its C++ layer shall not make HTTP requests, DNS lookups, or any calls to external services, including weather data providers, astronomical services, or time-sync services.

**Acceptance Criteria:**
- No `QNetworkAccessManager`, `QNetworkRequest`, or HTTP operations in mapping or moon-phase code
- No DNS lookups or socket operations
- Component operates entirely offline; can be tested with network disabled
- All input data (condition code, timestamp, day/night flag) supplied by caller; no data fetching by the component

**Rationale:** Ensures reliability and performance; moon phase is deterministic, not fetched.

---

#### REQ-C-NOQML-C++: C++ Layer QML-Independence

**Sentence:**  
The C++ mapping function and moon-phase calculator shall have no dependencies on Qt QML, Quick, or GUI libraries; they shall compile and link against only Qt Core.

**Acceptance Criteria:**
- Mapping function implementation includes only `#include <QtCore/...>` and standard library headers
- No `#include <QQuick*>`, `#include <QQml*>`, or GUI-library headers
- C++ layer can be tested in a standalone unit test binary linked against `Qt6::Core` only
- CMake target for C++ utilities does not depend on Qt6::Gui, Qt6::Quick, or Qt6::Qml

**Rationale:** Simplifies testing; enables embedded/headless use cases.

---

#### REQ-C-NOUI: Component Has No UI Affordances for Configuration

**Sentence:**  
The component shall not provide UI controls (buttons, sliders, dropdowns) for user configuration; all inputs are supplied programmatically by the caller.

**Acceptance Criteria:**
- Component has zero interactive UI elements
- All configuration via QML properties (read-only or bound from parent)
- No pop-up menus, dialogs, or setting screens in the component
- User-facing configuration, if needed, is the responsibility of the consumer (e.g., a weather app that instantiates the compositor)

**Rationale:** Keeps component focused and reusable across different UIs.

---

#### REQ-C-SINGLECOND: Single-Condition Per Instance

**Sentence:**  
Each component instance shall render exactly one condition code at a time; the component shall not multiplex or display multiple conditions in a single visual element.

**Acceptance Criteria:**
- Component accepts a single `conditionCode` property, not a list
- Rendering output corresponds to one condition + day/night state
- To display multiple conditions on screen (e.g., forecast), the caller instantiates multiple component instances

**Rationale:** Simplifies component contract; allows horizontal scaling via multiple instances.

---

### 3.6 Event-Driven Requirements

#### REQ-E-PROPCHANGE: Property Change Triggers Re-render

**Sentence:**  
When any input property changes, the component shall re-request the layer list from C++ and update all rendered `Image` elements to reflect the new state.

**Acceptance Criteria:**
- Property bindings on `conditionCode`, `isDay`, `iconSize`, or `date` automatically trigger C++ lookup and QML update
- Changes propagate within one frame (< 16ms)
- No explicit refresh/invalidate calls required by the caller

**Rationale:** Integrates seamlessly with QML reactive data flow.

---

#### REQ-E-INITIAL: Component Initializes on First Load

**Sentence:**  
On first instantiation, the component shall immediately fetch the layer list for the provided input properties and render all layers.

**Acceptance Criteria:**
- Component does not require explicit `load()`, `refresh()`, or `init()` method calls
- Rendering occurs in `Component.onCompleted` or equivalent, with all layers visible after the first frame
- If any required property (e.g., `conditionCode`) is not set at instantiation, the component shall emit a clear error or render empty (not crash)

**Rationale:** Follows QML conventions for automatic initialization.

---

### 3.7 State-Driven Requirements

#### REQ-S-LAYERORDER: Layer Stack Order Reflects Mapping Table

**Sentence:**  
The vertical Z-order of rendered `Image` elements shall exactly match the bottom-to-top layer order in the condition-to-layer mapping table.

**Acceptance Criteria:**
- For condition 801 (few clouds), day: rendered stack is [sun (z=0), few-clouds-overlay-day (z=1)], with overlay visibly on top
- For condition 800 (clear), night with moon=FULL: rendered stack is [star-field (z=0), moon-full (z=1)], with moon visibly on top
- Reversing Z-order (e.g., putting overlay on bottom) produces visually incorrect result (overlay hidden by opaque layer below)
- Z-order is implicit (via Repeater stacking) or explicit (via `z` properties), but the final rendering order is correct

**Rationale:** Ensures visual correctness; overlays must appear on top of base layers.

---

#### REQ-S-UNMAPPEDEMPTY: Unmapped Code Yields No Visible Output

**Sentence:**  
When an unmapped condition code is provided, the component shall render no visible imagery; the component shall be invisible or occupy zero space in the layout.

**Acceptance Criteria:**
- Condition code 511, 2xx, 6xx, 7xx all render no `Image` elements
- Component height and width are effectively 0 (or explicit `width: 0; height: 0` if not managed by layout)
- Parent layout (e.g., ColumnLayout) contracts the space where the component would appear
- If the component is set to an unmapped condition at runtime, previous visible layers disappear

**Rationale:** Matches design decision; unmapped codes are out of scope visually.

---

## 4. Assumptions & Dependencies

1. **Day/Night Input:** The caller (consumer) is responsible for determining day/night state (e.g., by comparing sunrise/sunset timestamps to current time) and passing it as the `isDay` boolean. The component does not compute sunrise/sunset.

2. **Timestamp for Moon Phase:** If the component is instantiated with `isDay: false`, the caller shall provide a valid `date` property (`QDateTime` or equivalent) so that moon phase can be calculated. If `isDay: true`, the `date` property is ignored (no moon phase needed).

3. **Asset Availability:** All 21 PNG files in `assets/weather-png/512x512/` are available and correctly bundled via `qt6_add_resources` with QRC prefix `qrc:/HolonightShell/`.

4. **Qt Version & Modules:** Project uses Qt 6.x with modules `Qt6::Core`, `Qt6::Qml`, `Qt6::Quick`, and `Qt6::Gui`. Pixmap caching and `Image` element smoothing/mipmapping are standard Qt6 features.

5. **Wayland Session Not Required for Rendering:** The C++ layer (mapping + moon-phase) does not require a Wayland session and is testable offline. QML rendering (the `Image` elements) requires the QML engine and typically a window, but asset paths are resolved by Qt without explicit Wayland interaction.

6. **Future Consumers:** This iteration implements the component; future PRs will wire it into specific screens (e.g., a weather popup). No existing screens are modified in this iteration.

---

## 5. Test Plan Outline

### Unit Tests (C++)

- **Mapping Function Tests:**
  - All 16 combinations of (day/night) × (clear, few-clouds, scattered-clouds, broken-clouds, light-drizzle, heavy-drizzle, rain)
  - All 8 moon phases for night conditions
  - All unmapped code families (511, 2xx, 6xx, 7xx)
  - Edge cases (first/last condition code in a family, boundary transitions)

- **Moon Phase Calculation Tests:**
  - Known new-moon, full-moon, quarter-moon timestamps
  - Full lunar cycle progression (29.5+ days)
  - Phase wrapping at cycle boundaries
  - Determinism (same input → same output)

### Integration Tests (QML)

- Component instantiation with valid input properties
- Property change triggering re-render
- Multiple concurrent instances at different sizes
- Unmapped condition rendering (empty/transparent)
- Visual inspection of rendered layers (manual or screenshot-based)

### Performance Tests

- First render latency (single 512x512 icon)
- Multi-size concurrent render latency
- Pixmap cache deduplication (memory footprint)

---

## 6. Out of Scope (Explicitly)

- Multi-resolution asset support (future iteration)
- Animation of any kind (future iteration)
- Fallback rendering for unmapped codes (deliberate empty result)
- Wiring into existing screens (future iteration)
- Sunrise/sunset calculation (caller's responsibility)
- External moon-phase API or data source
- User-facing UI for configuration
- Thunderstorm, snow, or atmospheric conditions visualization

---

## 7. Success Criteria

The feature is complete when:

1. ✓ C++ mapping function is implemented, unit-tested (≥ 95% coverage), and achieves correct layer lists for all test cases.
2. ✓ Moon-phase calculator is implemented, unit-tested, and produces correct phase enum for any given timestamp.
3. ✓ QML component is implemented, renders correctly with `Image` stacking, and reacts to property changes.
4. ✓ Component supports multiple concurrent instances at different sizes without visual corruption or performance degradation.
5. ✓ Unmapped conditions render empty/transparent with no errors.
6. ✓ All requirements are marked as satisfied and test evidence (CTest output, screenshots, code review) is provided.
7. ✓ No changes to existing WeatherService, WeatherCurrentSection, or other weather-related components.
8. ✓ Component is documented in CLAUDE.md or equivalent and available for future consumers to import.

---

## Appendix A: Reference Materials

- **Design assets:** `/home/andrii/Projects/pet/holonight/holonight-shell/assets/weather-png/512x512/`
- **Project guidelines:** `CLAUDE.md` (QML structure, D-Bus patterns, theming, testing, build commands)
- **EARS format reference:** Five sentence templates (Ubiquitous, Event-driven, State-driven, Optional/Conditional, Unwanted Behaviour)
