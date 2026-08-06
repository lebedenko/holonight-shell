# Exact HnIcon Adoption Tasks

1. Update dependency/build assumptions to require the `holonight-qt` build that provides `Holonight.HnIcon`.
2. Migrate shell `HnIcon` call sites to `Holonight.HnIcon` and remove the local component.
3. Remove the local `MultiEffect` shader tint path from `HnIcon`.
4. Update QML tests for wrapper compatibility, shell SVG adoption, and `image://icon` fallback behavior.
5. Run `task test`.
6. Run `task qml-lint`.
7. Run `task qmltypes-check`.
