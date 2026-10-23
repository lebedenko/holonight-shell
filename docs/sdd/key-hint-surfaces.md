# Key hints on host surfaces

## Intent and contract

Use transparent HnKeyHint badges throughout Shell. Neutral launcher and audio
footer hints inherit provider defaults. The launcher selected-row Return hint
binds both borderColor and foregroundColor to the shared delegate outlineColor.
Authentication ActionButton hints bind both colors to the actual button background
border, including primary, focus and disabled states. Explicit colors remain
authoritative when disabled. Shortcut behavior and badge geometry do not change.

## Dependency and verification

Requires the provider API described in
[key-hint surfaces](../../../holonight-qt/docs/sdd/key-hint-surfaces.md).
Build against an installed development provider; publish that provider before
consumer commits reference it. Verify LauncherSharedControlsQmlTests and
AuthenticationDialog under Holonight and Fusion, plus QML lint/import policy.
Capture launcher and authentication surfaces to verify transparent interiors and
matching container/hint colors. See the
[umbrella contract](../../../docs/initiatives/key-hint-surfaces.md).
