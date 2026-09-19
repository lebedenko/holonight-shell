# DESIGN: Consistent key hints

Adopt HnKeyHint.keyGroups using explicit Qt key constants after the provider is published and pinned. Remove local keycap painting and symbol sizing. Use shared wrapping for constrained help. Keep existing action handlers and validation messages; update size measurements and accessibility composition to match the shared badge.

## Adoption sites

- `apps/shell/qml/Launcher/Launcher.qml`: replace terminal-launch, navigation and
  close keycap rectangles with semantic HnKeyHint groups; keep action labels.
- `apps/shell/qml/Launcher/LauncherResultRow.qml`: selected-result Return hint.
- `qml/Authentication/AuthenticationDialog.qml`: Escape/Return hints in buttons;
  retain focus traversal, cancellation and submit handlers.
- `apps/shell/qml/Popups/Audio/KeyboardHintFooter.qml`: replace the local styled
  subclass with shared hints; remove navigation-specific font and padding changes.
  Preserve focus-context visibility and action labels for device rows/sliders.

Verification: focused launcher/authentication/audio QML tests, then `task test`,
`task qml-lint`; native ecosystem review after publication. No registration changes
are expected; run `task qmltypes-check` if registration changes become necessary.
