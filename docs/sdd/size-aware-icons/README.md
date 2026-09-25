# Explicit shared icon rendering in Shell

Baseline: `933606763c01b0cf253ad76749fd080f32eee7ec`.

Migrate Shell `HnIcon` users to `name` for theme icons and `source` for assets or provider URLs. Choose `Original` or `Semantic` explicitly; remove semantic-class detection as a routing decision. Preserve full-color app icons and original tray pixmaps, while palette-aware shell glyphs remain semantic. Update QML and qmllint fixtures and run focused shell tests against the accepted `holonight-qt` revision.

Implementation: `qml/HoloNight/Components/ExternalIcon.qml`, affected Shell QML callers, QML lint stubs, and QML tests. Local verification (2026-09-25): `task qml-lint`, `task qmltypes-check`, and `task test` passed (1170 tests); the focused QML harness passed all 5 checks against the local provider build. Existing AudioService qmllint warnings remain.
