# Background/Wallpaper Component — Design Document

## 1. Overview

The background component renders a full-screen wallpaper surface on every monitor at application startup. It uses the `wlr-layer-shell` `layer_background` layer so the surface sits below all bars, popups, and windows. Each monitor gets its own `QQuickView` hosted by a new `BackgroundManager` class that mirrors the `LayerShellManager`/`createBar` pattern already used for the top bar.

Configuration is driven by a new `[background]` TOML section parsed by `ConfigService`. The section exposes an `images` key (a TOML array of strings) whose entries are mapped positionally to monitors (image[0] → screen[0], …). Live reload is hooked into `ConfigService`'s existing debounced-watcher mechanism: a `backgroundChanged()` signal propagates new paths to running views, which execute a 250 ms crossfade in QML.

When a path is absent, empty, or points to an unreadable file, the monitor falls back to `HoloniightPalette.surfaceVariant` (solid fill), consistent with the HoloNight design system.

---

## 2. Component Inventory

### 2.1 New files

| Path | Description |
|---|---|
| `src/surfaces/BackgroundManager.h` | Class declaration — owns background `LayerSurface` + `QQuickView` pairs |
| `src/surfaces/BackgroundManager.cpp` | Startup surface creation, `initializeBackgrounds()`, `createBackground()`, live-reload slot |
| `src/qml/Background/Background.qml` | Full-screen background QML: two `Image` layers + crossfade `NumberAnimation`, solid-color fallback |

### 2.2 Modified files

| Path | Change |
|---|---|
| `src/core/ConfigService.h` | Add `BackgroundConfig` struct; add `background_` member; add `background()` accessor; add `backgroundChanged()` signal; extend `MissingDefaults` |
| `src/core/ConfigService.cpp` | Add `parseBackground()` free function; call it in `parseFile()`; emit `backgroundChanged()` on diff; add `[background]` to `writeMissingDefaults()` and `writeConfig()` |
| `src/app/ShellApplication.h` | Add `BackgroundManager* background_manager_` raw pointer (or `std::unique_ptr`) |
| `src/app/ShellApplication.cpp` | Construct `BackgroundManager` in `startShell()`, after `layer_shell_manager_` construction, passing `config_service_` |
| `CMakeLists.txt` | Add `BackgroundManager.h` and `BackgroundManager.cpp` to `holonight_surfaces`; add `src/qml/Background/Background.qml` to `HOLONIGHT_QML_FILES` |

---

## 3. Data Flow

### 3.1 Startup path

```
main()
  ShellApplication::startShell()
    BackgroundManager(config_service_, this)
      reads config_service_->background()    // already parsed by ConfigService ctor
      iterates QGuiApplication::screens()
      for each screen[i]:
        resolvedPath = resolveImagePath(background.images, i)  // C++ helper
        createBackground(screen, resolvedPath)
          QQuickView::setScreen(screen)
          QQuickView::create()           // materialises wl_surface
          get wl_surface via QNativeInterface::Private::QWaylandWindow
          shell_.get_layer_surface(..., layer_background, "background")
          LayerSurface: anchor all four edges, size 0×0, exclusive_zone -1 (full output, under the bar)
          wl_surface_set_input_region(wlSurface, NULL)   // empty region — see §5.4
          view->setInitialProperties({{"imagePath", resolvedPath}})
          view->setSource("qrc:/HolonightShell/Background/Background.qml")
          wl_surface_commit(wlSurface)
          backgrounds_.emplace_back(layerSurface, view)
```

### 3.2 Live-reload path

```
inotify event (config.toml changed)
  ConfigService::onFileChanged()  →  debounce_timer_.start(200 ms)
  ConfigService::parseFile()
    parseBackground(table, missing)
    if new_background != background_:
      background_ = new_background
      emit backgroundChanged()
  BackgroundManager::onBackgroundChanged()   // connected in BackgroundManager ctor
    for i in 0..backgrounds_.size()-1:
      newPath = resolveImagePath(config_service_->background().images, i)
      view = backgrounds_[i].second
      view->rootObject()->setProperty("imagePath", newPath)
        // QML Connections on imagePath triggers crossfade animation
```

---

## 4. Interfaces and Contracts

### 4.1 `BackgroundConfig` struct (ConfigService.h)

```cpp
struct BackgroundConfig {
  QStringList images;  // expanded tilde paths; empty list = solid-color fallback

  bool operator==(const BackgroundConfig& other) const {
    return images == other.images;  // QStringList::operator== is element-wise and ORDER-SENSITIVE
  }
};
```

`operator==` uses `QStringList::operator==`, which compares element-by-element in order. Order drives positional mapping, so reordering the list in TOML must trigger a reload and must register as a change even when the set of paths is identical. The defaulted `operator==` cannot be used here because `QStringList`'s own `operator==` is already order-sensitive — there is no risk of accidentally making it order-insensitive; the explicit implementation documents the intent.

### 4.2 ConfigService API additions

```cpp
// ConfigService.h

struct BackgroundConfig { /* as above */ };

class ConfigService : public QObject {
  // …existing members…
  [[nodiscard]] const BackgroundConfig& background() const { return background_; }

 Q_SIGNALS:
  void backgroundChanged();   // added alongside existing appearanceChanged() etc.

 private:
  BackgroundConfig background_;  // default-constructed: images is empty
};
```

The `parseBackground()` free function (in the anonymous namespace of `ConfigService.cpp`) reads `tbl["background"]["images"]` as a TOML array of strings, performs tilde expansion on each entry (see §5.5), and returns a `BackgroundConfig`. The `parseFile()` method calls it, compares with `background_`, swaps on success, and emits `backgroundChanged()` on difference — matching the pattern used for `AppearanceConfig`, `BarWorkspacesConfig`, and `BarSystemTrayConfig`.

Missing `[background]` section or missing `images` key: sets `missing.background_images = true`, causing `writeMissingDefaults()` to write back:

```toml
[background]
images = []
```

This matches the existing write-back pattern in `defaultLinesForSection()` for the `appearance`, `bar.workspaces`, and `bar.systemtray` sections.

### 4.3 `BackgroundManager` C++ class

```cpp
class BackgroundManager : public QObject {
  Q_OBJECT
 public:
  explicit BackgroundManager(ConfigService* config_service, QObject* parent = nullptr);
  ~BackgroundManager() override;
  // no copy, no move

 private Q_SLOTS:
  void onBackgroundChanged();

 private:
  void initializeBackgrounds();
  void createBackground(QScreen* screen, const QString& image_path);

  ConfigService* config_service_;
  LayerShell shell_;   // same pattern as LayerShellManager — owns the zwlr_layer_shell_v1 global
  std::vector<std::pair<LayerSurface*, QQuickView*>> backgrounds_;  // parallel to LayerShellManager::bars_
  bool initialized_ = false;
};
```

`BackgroundManager` holds its own `LayerShell shell_` member (type `LayerShell` from `src/platform/LayerShell.h`) because the `zwlr_layer_shell_v1` global is stateless and cheap to hold twice; sharing it with `LayerShellManager` would couple the two classes unnecessarily (see §5.2).

Destruction order: `LayerSurface*` is deleted before `QQuickView*` in the destructor, matching the comment in `LayerShellManager.h` ("must be destroyed before QQuickView to preserve Wayland protocol order").

### 4.4 Background.qml root-item contract

The root item of `Background.qml` must expose this property:

```qml
// src/qml/Background/Background.qml
Item {
    id: root
    required property string imagePath   // "" means solid-color fallback
    // …
}
```

`BackgroundManager::createBackground()` injects it via `view->setInitialProperties({{"imagePath", resolvedPath}})`, mirroring how `LayerShellManager::createBar()` injects `barMonitorName`.

Live reload is delivered by `view->rootObject()->setProperty("imagePath", newPath)` from `BackgroundManager::onBackgroundChanged()`. The QML watches `imagePath` via `onImagePathChanged` to trigger the crossfade.

`imagePath` is a plain writable `property string` — not `required` — on the root item after initial construction. Declaring it `required` prevents `setInitialProperties` from being omitted at startup; `setProperty` works on required properties after the component is live.

### 4.5 Background.qml internal structure and crossfade

```qml
import QtQuick
import QtQuick.Window
import Holonight

Item {
    id: root

    required property string imagePath

    // Solid-color base — always visible, covers the screen when no image loads
    Rectangle {
        anchors.fill: parent
        color: HoloniightPalette.surfaceVariant
    }

    // "current" layer — the image that is actively displayed
    Image {
        id: currentImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(Screen.width, Screen.height)
        source: imagePath !== "" ? imagePath : ""
        visible: status === Image.Ready
        asynchronous: true

        onStatusChanged: {
            if (status === Image.Error) {
                console.warn("Background: failed to load image:", source)
            }
        }
    }

    // "incoming" layer — fades in on top when imagePath changes
    Image {
        id: incomingImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(Screen.width, Screen.height)
        opacity: 0
        asynchronous: true

        onStatusChanged: {
            if (status === Image.Ready) {
                fadeIn.start()
            }
            if (status === Image.Error) {
                console.warn("Background: failed to load image:", source)
                source = ""
            }
        }
    }

    NumberAnimation {
        id: fadeIn
        target: incomingImage
        property: "opacity"
        from: 0; to: 1
        duration: 250
        easing.type: Easing.InOutQuad
        onFinished: {
            currentImage.source = incomingImage.source
            currentImage.opacity = 1
            incomingImage.opacity = 0
            incomingImage.source = ""
        }
    }

    onImagePathChanged: {
        if (imagePath === "") {
            fadeIn.stop()
            currentImage.source = ""
            incomingImage.source = ""
            return
        }
        incomingImage.source = imagePath
        // fadeIn fires in incomingImage.onStatusChanged when Image.Ready
    }
}
```

Key points:

- `Screen.width` / `Screen.height` (requires `import QtQuick.Window`) give the logical pixel size of the screen the view is on. Qt's image decoder honours `sourceSize` and decodes only to that resolution, avoiding holding full 4 K bitmaps in memory for a 1080 p monitor (REQ-F-015, REQ-NF-004).
- Two `Image` items act as a flip-flop: `currentImage` holds the stable displayed image; `incomingImage` loads the new path off-thread (`asynchronous: true`) and triggers `fadeIn` only when `Image.Ready`.
- The `Rectangle` behind both images fills with `HoloniightPalette.surfaceVariant`. It is always present. When `imagePath` is empty or both images have no source, the solid color shows through — no special state machine is needed (REQ-F-009).
- `console.warn` is used inside QML for the image-load-failure diagnostic (REQ-F-012). This is consistent with the logging note in CLAUDE.md: `qCWarning` applies to C++ code; in QML, `console.warn` feeds the same Qt logging output and is the idiomatic equivalent. The C++ side cannot receive `Image.Error` without additional plumbing (a `QQmlProperty` watcher or a C++ `QObject` exposed to QML), which would add complexity for no benefit — `console.warn` is sufficient and auditable via `QT_LOGGING_RULES`.
- `fillMode: Image.PreserveAspectCrop` satisfies REQ-F-010 and REQ-F-011 (hardcoded, not configurable).
- The `Rectangle` is declared first in the Item so `Image` layers render on top of it; no `z` value manipulation is required (consistent with the MultiEffect z-order gotcha in CLAUDE.md).

---

## 5. Key Decisions with Rationale

### 5.1 Where path resolution lives (C++ vs QML)

**Decision:** Resolve the image path fully in C++ inside `BackgroundManager` and pass the final `file://` URL string to `Background.qml` via the `imagePath` property. The anonymous-namespace helper `imageUrlForMonitor()` calls `BackgroundConfig::imageForMonitor()` and wraps a non-empty result in `QUrl::fromLocalFile(path).toString()`; an empty path stays empty (the "no wallpaper" sentinel). A bare filesystem path must not be handed to QML `Image.source` directly — QML resolves a scheme-less absolute path against the component's `qrc:` base URL and the load fails.

**Exclusive zone:** set to `-1`, not `0`. `0` lets the compositor displace the surface out of the top bar's exclusive zone (it would render below the bar); `-1` makes the surface span the full output and ignore other exclusive zones, so the wallpaper extends under the bar (which still draws above it on a higher layer).

**Rationale:**
- The positional mapping (`images[i] → screen[i]`, underflow repeats last, overflow ignored) and tilde expansion are business logic that belongs in a testable C++ layer, not in QML.
- `Background.qml` receives one opaque `string`; it does not know about monitor indices or the full image list. This keeps the QML component simple and reusable.
- Live reload: `BackgroundManager::onBackgroundChanged()` re-resolves paths for all views and pushes them via `setProperty`. QML fires `onImagePathChanged` and initiates crossfade. There is no need for QML to access `ConfigService` directly.
- The alternative — passing a screen index to QML and calling `ConfigService` from QML — would require exposing `BackgroundConfig` to QML as a singleton or property, adding QML/C++ coupling and making the path-resolution logic untestable without a running QML engine.

### 5.2 Separate BackgroundManager vs extending LayerShellManager

**Decision:** New `BackgroundManager` class; do not modify `LayerShellManager`.

**Rationale:**
- `LayerShellManager` is constructed after services and types are registered (`ShellApplication::startShell()`). `BackgroundManager` has the same lifecycle — this would not be a forcing factor.
- The two managers differ in layer (`layer_top` vs `layer_background`), anchor (top+left+right vs all four), exclusive zone (bar height vs 0), input region (default vs explicitly empty), and the `setInitialProperties` payload (`barMonitorName` vs `imagePath`). Folding both into one class would require flag parameters or inheritance, making each harder to read.
- `LayerShellManager` takes `TrayModel*` which `BackgroundManager` does not need. Introducing that dependency would violate the principle that `holonight_surfaces` must not depend on service-layer types.
- Precedent: `PopupSurface`, `TooltipSurface`, and `StatusPopupSurface` each manage their own `LayerShell shell_` member independently. The cost of two `LayerShell` instances is negligible.

### 5.3 Empty input region mechanism

**Decision:** Call `wl_surface_set_input_region(wlSurface, nullptr)` immediately after `view->create()` and before `shell_.get_layer_surface(...)`.

`wl_surface_set_input_region` with a `nullptr` region argument is specified by the Wayland protocol (`wl_surface.set_input_region` documentation) to reset the input region to "infinite" — but for layer-shell surfaces anchored to the full screen with no exclusive zone the compositor already routes input to the topmost surface, so passing `nullptr` alone would not help. The correct behaviour (empty — i.e. zero-area — input region) requires creating an empty `wl_region` and passing it:

```cpp
// After view->create() and before wl_surface_commit():
wl_compositor* compositor =
    QGuiApplication::instance()
        ->nativeInterface<QNativeInterface::QWaylandApplication>()
        ->compositor();
wl_region* emptyRegion = wl_compositor_create_region(compositor);
// Do NOT call wl_region_add() — leave it empty (zero area).
wl_surface_set_input_region(wlSurface, emptyRegion);
wl_region_destroy(emptyRegion);
```

`QNativeInterface::QWaylandApplication` is the public Qt API for accessing the Wayland display and compositor from the client side. It is declared in `<qguiapplication_platform.h>` and its `compositor()` method returns the `wl_compositor*` global (verified in `/usr/include/qt6/QtGui/qguiapplication_platform.h:75`). This header is available because `Qt6::Gui` is already linked in `holonight_platform`.

A `nullptr` region arg to `wl_surface_set_input_region` means "reset to infinite" per the Wayland spec; only an explicitly empty `wl_region` (created but never `wl_region_add`'d) means "no input". The `wl_region_destroy` call must follow immediately — the compositor copies the region content into the surface state and the client-side object can be freed (Wayland double-buffering rule). The `wl_surface_set_input_region` must precede `wl_surface_commit` to take effect.

`Qt::WindowTransparentForInput` window flag is NOT used here. That flag affects Qt's own hit-testing, not the Wayland `wl_region`; it would prevent Qt from routing events to the QML item tree but would not stop the compositor from delivering events to the surface at the Wayland protocol level.

### 5.4 Tilde expansion location

**Decision:** Tilde expansion happens at **parse time** inside `parseBackground()` in `ConfigService.cpp`, not at use time in `BackgroundManager`.

**Rationale:**
- `ConfigService` already owns all path processing for other keys. Expanding tildes at parse time means `BackgroundConfig::images` always contains absolute paths. `BackgroundManager` and QML receive clean absolute paths and need no expansion logic of their own.
- A single expansion site is easier to test: unit tests for `parseBackground()` cover expansion without a live `QScreen` or layer surface.
- `QDir::homePath()` is pure and available at parse time. There is no benefit to deferring it.

Implementation: in `parseBackground()`, for each string in the TOML array, if the string starts with `~/`, replace the leading `~` with `QDir::homePath()`. Relative paths (no leading `/` or `~/`) are passed through unchanged; `Background.qml` will fail to load them and emit a `console.warn`, which is the same code path as any other bad path (REQ-F-004 implementation choice: treat relative paths as invalid).

### 5.5 `operator==` is order-sensitive

`QStringList::operator==` is element-wise, so `BackgroundConfig::operator==` using `images == other.images` is inherently order-sensitive. This is intentional: swapping `images[0]` and `images[1]` changes which wallpaper appears on which monitor, so it must trigger a reload. No additional sorting or normalisation is applied.

### 5.6 `sourceSize` strategy

`sourceSize: Qt.size(Screen.width, Screen.height)` is set on both `Image` items. `Screen.width` and `Screen.height` in QML refer to the logical size of the screen the window is placed on (requires `import QtQuick.Window`). Qt's image loader uses `sourceSize` to cap the decoded resolution: a 3840×2160 image loaded with `sourceSize: Qt.size(1920, 1080)` is decoded at approximately half resolution, reducing GPU-upload memory by ~4×. The cap is applied before decoding, not after, so the full raw file is never expanded in RAM.

---

## 6. Alternatives Considered

### 6.1 Extending LayerShellManager instead of a new class

Rejected — see §5.2. The two classes have different dependencies, configurations, and lifecycles. Merging them would make both harder to reason about.

### 6.2 Resolving image paths in QML

Rejected — see §5.1. Path resolution and positional mapping are unit-testable business logic; they do not belong in QML. QML also cannot access `QDir::homePath()` directly.

### 6.3 Using Qt.WindowTransparentForInput to pass input through

Rejected. `Qt::WindowTransparentForInput` is a Qt window flag that stops Qt's input dispatch but does not set an empty `wl_region` at the Wayland protocol level. The compositor still considers the surface to own input for its covered area. Setting an explicit empty `wl_region` via `wl_surface_set_input_region` is the correct Wayland mechanism.

### 6.4 Using `wl_surface_set_input_region(surface, nullptr)` for empty region

Rejected. Passing `nullptr` to `wl_surface_set_input_region` is documented as resetting the input region to cover the entire surface (infinite). An empty `wl_region` — created with `wl_compositor_create_region()` without any `wl_region_add()` calls — is the correct way to express zero input area.

### 6.5 Named monitor mapping in TOML

Deferred per SPEC.md (non-goal). The `images` array uses positional indexing only. Named mapping (e.g. `[background.monitors.DP-1]`) is reserved for a future iteration and would require ConfigService changes beyond scope.

### 6.6 Single Image item with instant swap

A single `Image` item that just updates `source` would cause a flash (frame of solid color between old and new image). Two items with a crossfade avoid the flash (REQ-F-014).

### 6.7 Tilde expansion at use time (in BackgroundManager)

Rejected — see §5.4. A single expansion site in `ConfigService` is simpler and unit-testable without Wayland infrastructure.

---

## 7. Known Risks and Open Issues

### 7.1 `QGuiApplication::screens()` order stability

The positional mapping assumes `screens()` returns monitors in a stable, deterministic order across sessions. Qt reflects the order reported by the compositor. In Hyprland this order is typically stable (monitors appear in configuration file order), but it is compositor-defined and not guaranteed by any Wayland protocol. If the compositor reorders monitors across restarts, image assignments shift. Mitigation: document this limitation in the `[background]` config file comment written by `writeConfig()`. Named mapping (future iteration) would resolve this.

### 7.2 Memory for large wallpapers on many monitors

With `sourceSize` capped to screen resolution, a 4K wallpaper on a 4K monitor uses ~31 MB (3840×2160×4 bytes). With four such monitors and two Image items per monitor (current + incoming during crossfade), peak memory is ~250 MB for wallpaper textures. This is reasonable for a desktop environment. No further mitigation is needed within scope.

### 7.3 Interaction with the compositor's own background

`layer_background` surfaces render behind everything the compositor draws at that layer. Some compositors (e.g. Hyprland) render their own wallpaper at the BACKGROUND layer using `hyprpaper` or built-in `wallpaper` config. If both are active, the compositor's wallpaper renders behind this component's surface and will be hidden. This is the expected and desired behaviour: holonight-shell takes full ownership of the background layer. If the user runs a separate wallpaper daemon, they should disable it.

### 7.4 Layer-shell surface not available at startup

`BackgroundManager` uses the same `QWaylandClientExtension::activeChanged` guard as `LayerShellManager`: if `shell_.isActive()` is false at construction, `initializeBackgrounds()` runs when the signal fires, and a 3-second fallback timer exits the application if the protocol never arrives. This guard is inherited unchanged from `LayerShellManager`.

### 7.5 `Screen.width`/`Screen.height` vs physical pixels for `sourceSize`

`Screen.width` and `Screen.height` are logical pixels; on a HiDPI display with `devicePixelRatio = 2`, the physical framebuffer is 2× larger. Qt's `Image` element and `sourceSize` operate in logical pixels by default. The GPU texture will be scaled up by the compositor. This means a 4K image on a 1920×1080 logical-pixel HiDPI display will be decoded at 1920×1080 (logical) but displayed at 3840×2160 physical pixels — the image may appear softer. A more accurate `sourceSize` would multiply by `Screen.devicePixelRatio`. This is a known trade-off between memory and sharpness; correcting it is a future improvement.

### 7.6 Hot-plug deliberately unsupported

Per REQ-F-016 and the SPEC non-goals, no `QGuiApplication::screenAdded` / `screenRemoved` signal handling is implemented. `initialized_` is set to `true` after the first pass, matching `LayerShellManager`'s guard. This is intentional.

---

## 8. Test Strategy

### 8.1 Unit tests (GTest via `task test`)

New tests belong in `tests/` and link against `holonight_core` (which contains `ConfigService`).

**`ConfigServiceBackgroundTest`**

| Test case | Verifies |
|---|---|
| `ParsesBackgroundSection` | `[background]\nimages = ["/a.png", "~/b.png"]` yields `BackgroundConfig{images={"/a.png", QDir::homePath()+"/b.png"}}` |
| `TildeExpansionApplied` | `~/foo.png` becomes an absolute path after parse |
| `EmptySectionGivesSolidFallback` | `[background]\nimages = []` yields `BackgroundConfig{images={}}` |
| `AbsentSectionGivesSolidFallback` | Config with no `[background]` section yields `BackgroundConfig{images={}}` |
| `OperatorEqualsIsOrderSensitive` | `{"a","b"} != {"b","a"}` |
| `WritesBackMissingSection` | After parse with absent section, `config.toml` gains `[background]\nimages = []` |
| `SignalEmittedOnChange` | Changing `images` list in file triggers `backgroundChanged()` exactly once |
| `SignalNotEmittedOnSameValue` | Re-parsing identical content does not emit `backgroundChanged()` |
| `OverflowIgnored` | 3 images, 1 screen: `resolveImagePath(images, 0)` returns `images[0]` |
| `UnderflowRepeatsLast` | 1 image, 3 screens: `resolveImagePath(images, 2)` returns `images[0]` |
| `EmptyListReturnsEmpty` | `resolveImagePath({}, 0)` returns `""` |

`resolveImagePath()` should be a `static` free function in `BackgroundManager.cpp`'s anonymous namespace; to unit-test it without Wayland infrastructure, extract it to a header-only helper or a testable free function in `src/core/` (alongside the config struct) if the test cannot link `holonight_surfaces`.

### 8.2 Manual / visual tests

All of the following require a live Wayland compositor (`task run` or the `test-env` skill).

| Scenario | What to verify |
|---|---|
| Single monitor, one image | Image fills screen with `PreserveAspectCrop`; no bars or distortion |
| Two monitors, two images | Each monitor shows its assigned image |
| Two monitors, one image | Both monitors show the same image (underflow: last entry repeated) |
| Three images, two monitors | Third image silently ignored; no warning logged |
| `images = []` | Both monitors show `surfaceVariant` solid color |
| `[background]` absent | Solid color fallback on all monitors |
| Bad path | `console.warn` in Qt logs; affected monitor shows `surfaceVariant`; other monitors unaffected |
| Live reload (change images) | Crossfade animates smoothly over ~250 ms; no flash |
| Live reload (clear images) | Crossfade to solid color |
| Portrait image on landscape monitor | Center-cropped vertically, fills width; no letterbox |
| 4K image on 1080 p monitor | `sourceSize` limits decode (inspect via `QSG_RENDERER_DEBUG` or profiler) |
| Input passthrough | Mouse click on background does not block clicks on windows behind it |
| Bar renders above background | Top bar is visible above the background surface |
