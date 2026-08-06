# Exact HnIcon Adoption Specification

## Goal

Adopt the shared `Holonight.HnIcon` exact-color SVG renderer from `holonight-qt` while preserving existing shell QML behavior.

## Requirements

- Existing shell `HnIcon` call sites migrate to `Holonight.HnIcon`.
- Shell-local icon recoloring must no longer use `QtQuick.Effects.MultiEffect` for `HnIcon`.
- `qrc:/HolonightShell/bar-icons/*.svg` sources render through the shared exact SVG path when tinted.
- `image://icon/...` remains the app, tray, notification, and themed pixmap provider.
- App/tray icons continue to work through the existing shell `IconImageProvider`.
- `HnIcon` keeps the shell-facing API: `source`, `size`, `iconState`, `tinted`, state colors, and `resolvedColor`.
- Invalid or empty sources must not crash.

## Verification

- QML tests cover API compatibility, a shell SVG source, and an existing `image://icon` source.
- `task test` validates C++ and QML behavior.
- `task qml-lint` validates QML syntax and imports.
- `task qmltypes-check` validates generated shell QML type metadata.
