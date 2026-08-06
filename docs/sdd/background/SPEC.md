# Background/Wallpaper Component Specification

## Overview

The holonight-shell background component is a full-screen desktop background renderer that displays user-configured wallpaper images on each monitor via the Wayland wlr-layer-shell BACKGROUND layer. The component integrates with the existing ConfigService to support live reload of wallpaper configurations from `config.toml` and falls back to solid theme colors when images are unavailable. Input events pass through the background surface to windows and desktop elements below.

## Scope

This specification defines the requirements for:
- Full-screen background surface creation on each connected monitor
- Image configuration via TOML with path expansion and overflow/underflow rules
- Image rendering with crop-to-fill semantics and live reload support
- Fallback rendering to solid theme colors for missing or invalid images
- Crossfade transitions when wallpapers change at runtime

Out of scope (non-goals):
- Desktop widgets (future feature; infrastructure only)
- Per-monitor named mapping (positional indexing only this iteration)
- Configurable fill modes (crop-to-fill is hardcoded)
- Monitor hot-plug runtime handling (startup-only surface creation)

## Functional Requirements

### REQ-F-001: Full-Screen Background Layer Surface Creation
**Template:** Ubiquitous

The system shall create a full-screen background surface on each connected monitor using the Wayland wlr-layer-shell protocol, positioned on the `layer_background` layer (BACKGROUND), anchored to all four screen edges (top, bottom, left, right), with exclusive zone of -1.

> **Implementation note (divergence from initial spec):** the exclusive zone is `-1`, not `0`. `0` causes the compositor to displace the surface out of the top bar's exclusive zone, so the wallpaper would not extend under the bar. `-1` makes the surface span the full output and ignore other surfaces' exclusive zones, which is the required full-screen wallpaper behavior (the bar still renders above it because it is on a higher layer).

**Acceptance Criteria:**
- Each monitor reported by `QGuiApplication::screens()` at application startup has a corresponding background surface.
- The surface is anchored to all four edges, verified by inspecting the created wlr-layer-shell surface attributes.
- The exclusive zone attribute is set to -1, so the surface fills the entire output and the wallpaper extends under the top bar.
- The background layer is confirmed to render below all other layer-shell layers.

### REQ-F-002: Empty Input Region
**Template:** Ubiquitous

The system shall set an empty input region on the background surface so that all pointer and keyboard input events pass through to windows and desktop elements below.

**Acceptance Criteria:**
- Hovering the mouse over the background surface does not highlight or interact with the background itself.
- Clicking on the background surface does not prevent mouse/touch events from reaching windows underneath.
- Keyboard input while the background is in focus is not consumed and passes to windows below.

### REQ-F-003: TOML Configuration Section
**Template:** Ubiquitous

The system shall recognize a new `[background]` section in the holonight-shell TOML configuration file (`$XDG_CONFIG_HOME/holonight/config.toml`), with a required key `images` containing a list of image file paths.

**Acceptance Criteria:**
- The configuration parser accepts `[background]` as a valid top-level section without error.
- The `images` key is parsed as an array of strings.
- Example config `images = ["~/Pictures/a.png", "/abs/b.jpg"]` parses without error.
- ConfigService exposes a `BackgroundConfig` struct with an `images` field (QStringList or equivalent).

### REQ-F-004: Path Expansion (Absolute and Tilde)
**Template:** Ubiquitous

The system shall expand image paths in the `images` list, supporting absolute paths (e.g., `/abs/path/to/img.png`) and home directory tilde expansion (e.g., `~/Pictures/img.png`).

**Acceptance Criteria:**
- A path `~/Pictures/wallpaper.png` is expanded to the user's home directory (via `QDir::homePath()` or `expandUser()` equivalent).
- An absolute path `/etc/wallpaper.png` is used verbatim without expansion.
- Relative paths (e.g., `Pictures/img.png`) are rejected or treated as invalid (implementation choice).
- The expanded path is used when attempting to load the image file.
- The expanded filesystem path is converted to a `file://` URL (via `QUrl::fromLocalFile`) before being handed to the QML `Image.source`, since a scheme-less absolute path would be resolved against the component's `qrc:` base URL and fail to load.

### REQ-F-005: Supported Image Formats
**Template:** Ubiquitous

The system shall accept any image file format that Qt's `QImageReader` can decode, including but not limited to PNG, JPEG, WebP, and BMP.

**Acceptance Criteria:**
- Loading a valid PNG file succeeds and displays on the background.
- Loading a valid JPEG file succeeds and displays on the background.
- Loading a valid WebP file succeeds and displays on the background.
- Attempting to load an unsupported format (e.g., `.xyz`) fails gracefully and triggers the missing-image fallback.

### REQ-F-006: Positional Monitor Mapping
**Template:** Ubiquitous

The system shall map images to monitors by position: the i-th image in the `images` list is displayed on the i-th monitor in `QGuiApplication::screens()` order (image[0] → screen[0], image[1] → screen[1], etc.).

**Acceptance Criteria:**
- With a 2-monitor setup and config `images = ["/path/a.png", "/path/b.png"]`, monitor 0 displays image a and monitor 1 displays image b.
- Rearranging the order in the config changes which image appears on which monitor according to the positional rule.
- The mapping persists across the running session and only updates when the config is reloaded.

### REQ-F-007: Overflow Handling (More Images Than Monitors)
**Template:** Conditional/Optional

Where the `images` list contains more entries than the number of connected monitors, the system shall silently ignore all images beyond the monitor count without logging or warning.

**Acceptance Criteria:**
- With a single monitor and config `images = ["/path/a.png", "/path/b.png", "/path/c.png"]`, only image a is used.
- Images b and c are not loaded, decoded, or logged as unused.
- No warning or info log entry appears for the overflow condition.

### REQ-F-008: Underflow Handling (Fewer Images Than Monitors)
**Template:** Conditional/Optional

Where the `images` list contains fewer entries than the number of connected monitors, the system shall display the LAST image in the list on all remaining monitors that have no dedicated image.

**Acceptance Criteria:**
- With 3 monitors and config `images = ["/path/a.png", "/path/b.png"]`, monitor 0 displays a, monitors 1 and 2 both display b (the last image).
- If a single monitor uses the last image, the image is displayed and decoded only once (not duplicated in memory for each monitor).
- Changing the last image in the config causes all monitors using it to update immediately on reload.

### REQ-F-009: Empty or Absent Configuration
**Template:** State-driven

While the `[background]` section is absent from the configuration file OR the `images` list is empty, the system shall fill every monitor with a solid color derived from the system theme—specifically the `surfaceVariant` token from the HoloNight palette.

**Acceptance Criteria:**
- Starting the application with no `[background]` section in `config.toml` displays the `surfaceVariant` color on all monitors.
- A configuration with `[background]` and `images = []` displays the `surfaceVariant` color on all monitors.
- Removing all entries from the `images` list and reloading the config causes all monitors to switch to the solid color within 250ms (respecting the crossfade transition).
- The solid color is obtained via `HoloniightPalette.surfaceVariant` (using the HoloNight theme module).

### REQ-F-010: Image Rendering—Crop-to-Fill Semantics
**Template:** Ubiquitous

The system shall render wallpaper images to fill the monitor using crop-to-fill semantics (Qt Image fillMode `PreserveAspectCrop`): images are scaled to cover the entire monitor while preserving aspect ratio, with center-cropping applied to any dimension that exceeds the monitor bounds. No letterbox bars or image distortion shall occur.

**Acceptance Criteria:**
- A portrait image (taller than monitor) is scaled to fill the width and center-cropped vertically.
- A landscape image (wider than monitor) is scaled to fill the height and center-cropped horizontally.
- A square image on a rectangular monitor is scaled to fill the longer dimension and center-cropped on the shorter dimension.
- No black bars or empty space appears around the image.
- The image is not stretched or distorted.

### REQ-F-011: Image Rendering—Fixed Crop-to-Fill (Non-Configurable)
**Template:** Ubiquitous

The system shall use crop-to-fill as the exclusive fill mode; no alternative fill modes (stretch, tile, fit, etc.) are configurable in this iteration.

**Acceptance Criteria:**
- The fillMode is hardcoded to `PreserveAspectCrop` with no TOML option to change it.
- Attempting to configure an alternative fill mode in TOML produces no effect or a parse error (implementation choice).

### REQ-F-012: Missing or Invalid Image Fallback
**Template:** If/Then (Unwanted Behaviour)

If a configured image path does not exist OR the file fails to decode, then the system shall display the `surfaceVariant` solid color on that monitor AND log a warning via `qCWarning`.

**Acceptance Criteria:**
- Configuring a path to a non-existent file (e.g., `/tmp/nonexistent.png`) results in a `qCWarning` log entry and the monitor displays `surfaceVariant`.
- A corrupted image file (e.g., a text file with `.png` extension) fails to decode, triggers a `qCWarning`, and the monitor displays `surfaceVariant`.
- The warning message includes the file path and reason for failure (not found or decode error).
- Other monitors with valid images continue to display their images; only the failing monitor falls back to solid color.

### REQ-F-013: Live Reload—Configuration Change Integration
**Template:** Event-driven

When `config.toml` is edited at runtime, the system shall reload the background configuration and update wallpapers on affected monitors without requiring a restart, hooking into ConfigService's existing debounced reload mechanism.

**Acceptance Criteria:**
- Editing `config.toml` with a different `images` list and saving triggers a ConfigService reload.
- Within the debounce window (existing ConfigService behavior, typically < 1 second), the background wallpapers update to reflect the new configuration.
- The application continues running without interruption or restart.
- The `BackgroundConfig` change signal is emitted only when the parsed `images` value differs from the previous value.

### REQ-F-014: Wallpaper Swap Transition—Crossfade Animation
**Template:** Event-driven

When a wallpaper image changes at runtime (due to config reload), the system shall animate the transition from the old image to the new image using a crossfade over approximately 250ms (opacity animation), rather than an instantaneous flash.

**Acceptance Criteria:**
- Changing a single wallpaper in the config and reloading produces a smooth fade-out of the old image and fade-in of the new image over ~250ms.
- The 250ms duration is verifiable by visual inspection or by measuring the animation with precision timing.
- During the crossfade, neither the old nor new image is visible in full isolation (both are partially opaque simultaneously).
- If multiple monitors update simultaneously, each performs its own independent crossfade.

### REQ-F-015: Source Size Optimization for Large Images
**Template:** Ubiquitous

The system shall decode wallpaper images with a `sourceSize` cap to prevent full-resolution decoding of large images (e.g., 4K wallpapers) when they will be downscaled to the monitor resolution.

**Acceptance Criteria:**
- Loading a 4K image (3840×2160) on a 1920×1080 monitor does not hold the full 4K pixel data in memory.
- The `sourceSize` is set to the monitor resolution (or slightly above for anti-aliasing headroom) before decoding.
- The decoded image size reported by the image loader is at most 1.2× the monitor dimension (allowing for minor headroom).
- A 4K wallpaper on a 1920×1080 monitor uses approximately 1/4 the memory compared to full-resolution decoding.

### REQ-F-016: Monitor Hot-Plug Not Supported
**Template:** State-driven

While the shell is running, the system shall NOT handle monitor hot-plug events; background surfaces are created once at application startup and are not added or removed in response to monitor connection/disconnection.

**Acceptance Criteria:**
- Plugging in an additional monitor at runtime does not create a background surface on the new monitor.
- Unplugging a monitor at runtime does not remove or update the background surfaces on remaining monitors.
- To display a background on a newly connected monitor, the user must restart the shell.

## Non-Functional Requirements

### REQ-NF-001: Theme Integration
**Template:** Ubiquitous

The system shall use colors and styling from the HoloNight system theme exclusively; no hardcoded color values (hex, RGB, or named colors) shall appear in QML or C++ code for the background component.

**Acceptance Criteria:**
- The `surfaceVariant` color is retrieved from `HoloniightPalette` via the `import Holonight` module.
- No `#RRGGBB`, `rgb()`, `Color()`, or named color literals appear in background-related QML.
- A theme change at runtime causes the background solid color (if displayed) to update immediately.

### REQ-NF-002: ConfigService Integration Pattern
**Template:** Ubiquitous

The system shall follow the existing ConfigService pattern for background configuration: struct fields use snake_case, defaulted `operator==`, a change signal is emitted only when the parsed value differs from the previous value, and missing configuration keys are written back with defaults.

**Acceptance Criteria:**
- The `BackgroundConfig` struct declares fields in snake_case (e.g., `images` not `Images`).
- A `backgroundConfigChanged()` signal is defined and emitted by ConfigService only when `BackgroundConfig operator==()` indicates a difference.
- If the user deletes the `[background]` section, saving the config causes ConfigService to write back a default `[background]` section with `images = []`.
- The `operator==()` compares `images` lists element-by-element, ignoring whitespace or ordering differences in the TOML file itself.

### REQ-NF-003: Architecture Alignment
**Template:** Ubiquitous

The system shall follow the existing holonight-shell architecture: the background component shall use the same LayerShellManager pattern as the bar components (where applicable), inherit from C++ base classes (e.g., `QQuickView`), and interface with Wayland via established wlr-layer-shell bindings.

**Acceptance Criteria:**
- The background surface is created using the same `LayerShellManager::createBar()` pattern or an equivalent surface-creation mechanism.
- If separate from `LayerShellManager`, the background surface setup is documented in the code explaining the architectural choice.
- The layer-shell surface role and layer are correctly set to `wlr_layer_shell_v1_layer_background`.

### REQ-NF-004: Performance—Image Decoding
**Template:** Ubiquitous

The system shall decode and cache wallpaper images efficiently, avoiding repeated decoding on redraws and minimizing memory usage for large images via sourceSize capping.

**Acceptance Criteria:**
- A 4K wallpaper image is decoded only once at config load time, not on every frame.
- The decoded image is cached and reused for rendering across all frames until a config reload occurs.
- sourceSize capping is confirmed by profiling or code inspection to be applied before decoding.

## Constraints

### REQ-C-001: C++23 and Qt6 Requirements
**Template:** Ubiquitous

The background component shall be implemented in C++23 and Qt6, following the holonight-shell stack and build system (`Taskfile.yml`, CMakeLists.txt, clang-tidy, clang-format).

**Acceptance Criteria:**
- The component compiles with C++23 standard (`-std=c++23`).
- No C++20 or earlier syntax is used.
- The build succeeds via `task build` without warnings or errors (subject to existing project clang-tidy rules).
- clang-format verification passes: `task format-check` reports no violations.

### REQ-C-002: Layer-Shell Protocol Compliance
**Template:** Ubiquitous

The background surface shall comply with the Wayland wlr-layer-shell protocol (`zwlr_layer_shell_v1`), using the BACKGROUND layer, an exclusive zone of -1, all-edge anchors, and an empty input region as specified in the protocol and existing holonight-shell implementations.

**Acceptance Criteria:**
- The surface declares `zwlr_layer_surface_v1_set_layer(surface, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND)`.
- The surface declares `zwlr_layer_surface_v1_set_exclusive_zone(surface, -1)` so it spans the full output under the bar.
- The surface is anchored to ZWLR_LAYER_SHELL_V1_ANCHOR_TOP | BOTTOM | LEFT | RIGHT.
- The input region is explicitly set to empty via `wl_surface_set_input_region(surface, NULL)` or equivalent.

### REQ-C-003: TOML Configuration Location and Format
**Template:** Ubiquitous

The background configuration shall be stored in the holonight-shell TOML configuration file at `$XDG_CONFIG_HOME/holonight/config.toml`, following the existing ConfigService TOML parser.

**Acceptance Criteria:**
- The `[background]` section is parsed from the same `config.toml` file as other sections (e.g., `[theme]`, `[workspace]`).
- No separate configuration file is created.
- The TOML syntax adheres to the TOML 1.0.0 specification as parsed by the existing ConfigService library.

### REQ-C-004: HoloNight Theme Palette Dependency
**Template:** Ubiquitous

The system shall source the `surfaceVariant` color token from the HoloNight theme palette (via `import Holonight` and `HoloniightPalette.surfaceVariant`), and shall not define or hardcode this color value.

**Acceptance Criteria:**
- The QML module `Holonight` is imported where the solid color is applied.
- The color is accessed as `HoloniightPalette.surfaceVariant` (note: double-i spelling per existing codebase).
- The color value is not duplicated or hardcoded elsewhere in the background component.
- If the theme changes, the color updates reflect the new theme (no caching of the original value).

## Glossary

**Background Layer:** The wlr-layer-shell protocol layer positioned below all UI elements (bars, panels, tooltips) and above the desktop/window manager. Layer-shell surfaces on the BACKGROUND layer render behind all other window types.

**Crop-to-Fill:** An image sizing algorithm that scales an image to cover a target rectangle entirely while preserving aspect ratio, with any overflow center-cropped to fit exactly within the target bounds. Also known as `PreserveAspectCrop` in Qt.

**Crossfade:** A visual transition that simultaneously reduces the opacity of one element to zero while increasing the opacity of another from zero to full, creating a smooth blend effect.

**Exclusive Zone:** A wlr-layer-shell attribute indicating how much screen space a surface reserves for its own use. A value of 0 indicates the surface does not reserve exclusive space and other surfaces may overlap it.

**HoloNight Palette:** The system theme color palette defined in the holonight-shell theming system, providing named color tokens (e.g., `surfaceVariant`, `primary`, `onSurface`) used throughout the UI.

**Live Reload:** The ability to update application state (in this case, wallpapers and configuration) in response to external file changes (config.toml) without restarting the application.

**Monitor:** A connected display device (physical monitor or virtual output) enumerated by the Wayland compositor and reported by `QGuiApplication::screens()`.

**Source Size:** A Qt image optimization parameter that specifies the maximum decoded image size. When set, images are decoded only up to this resolution, reducing memory usage when the decoded size will be smaller than the source file's native resolution.

**Surface Variant:** A HoloNight theme color token used for secondary background surfaces and fallback elements. Typically a neutral color that complements the primary surface color.

**Tilde Expansion:** Path expansion in which a leading `~` character is replaced with the user's home directory path (e.g., `~/Pictures` → `/home/username/Pictures`).

**wlr-layer-shell:** A Wayland protocol extension (`zwlr_layer_shell_v1`) that allows clients to create surfaces on specific layers (background, bottom, top, overlay) with anchoring and exclusive zone properties, commonly used for desktop shells, panels, and bars.
