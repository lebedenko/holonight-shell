# Phase 40 — Top-Bar QML Contract Cleanup: Design

## 1. Current State

`BarSection` provides `implicitWidth: container.implicitWidth + 16`, but every
current concrete subclass overrides `implicitWidth`. The fallback is therefore
unobservable and misleading: content assigned through the default property is
anchored to the container rather than contributing a useful implicit size.

`BarTooltipArea` defaults `barMonitorName` to an empty string even though
`showTooltipNow()` always forwards it to `TooltipSurface.show()`. All 13 current
instances already bind the property from their owning section or tray item.

`WorkspaceEdgeArrow` enables `MouseArea.hoverEnabled`, but neither the area nor
the component reads `containsMouse`, `hovered`, or any other hover-derived
state. Its pointer contract is click-only.

## 2. Design

### 2.1 Base section sizing

Remove the base `implicitWidth` binding and leave concrete sizing untouched.
This makes each section's existing explicit policy authoritative without adding
a new abstraction or changing layout inputs.

### 2.2 Tooltip construction contract

Declare `barMonitorName` as `required property string`. Existing call sites do
not need edits because each already supplies the value. The QML compiler then
guards future instances from silently routing a tooltip with an empty monitor
identity.

### 2.3 Arrow pointer handling

Remove `hoverEnabled: true` from the arrow's `MouseArea`, restoring Qt's default
click-only tracking. Preserve the area, enabled binding, and click signal path.

## 3. Tests

Extend the existing `WorkspaceEdgeArrow` focused test to observe that the
pointer area does not enable hover tracking. Its existing activation test
continues to cover click dispatch. QML lint/build checks validate the required
tooltip property across every statically instantiated call site, while the
existing top-bar harness covers component construction and sizing behavior.

## 4. Risk

The changes remove unused fallback behavior rather than add runtime branches.
The principal regression risk is a missed tooltip or section call site; full
QML compilation and the existing component-instantiation suite detect those
contract violations deterministically.
