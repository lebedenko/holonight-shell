# Exact HnIcon Adoption Design

## Ownership

`holonight-qt` owns exact symbolic SVG rendering, the `image://hnicons` provider, and the shared `Holonight.HnIcon` API. `holonight-shell` owns local module compatibility and shell-specific validation.

## Adoption Strategy

Delete the shell-local `qml/HoloNight/Components/HnIcon.qml` component and migrate the small set of call sites to the shared `Holonight.HnIcon` type. The affected files already import `Holonight`, so this avoids a duplicate wrapper and keeps `Holonight.Components` focused on shell-owned components.

Consumers keep using `HnIcon.Normal`, `HnIcon.Active`, and the current color properties through the shared type.

## Provider Boundaries

- `image://hnicons/...`: exact symbolic SVG rendering owned by `Holonight.HnIcon`.
- `image://icon/...`: general shell image provider for app, tray, notification, and themed pixmap icons.

When `tinted` is true and a source is not already `image://icon`, the shared component generates an `image://hnicons` URL. When `tinted` is false, or when the source is `image://icon`, the original source is used directly.

## CMake Cleanup

No shell-side C++ provider changes are needed. The `Holonight.Components` module stops registering `HnIcon.qml`.

## Fallback Rules

- Empty source renders nothing.
- Invalid source renders as an image load failure without crashing.
- `image://icon` sources bypass exact SVG recoloring because they may represent pixmaps, app icons, or non-symbolic assets.
