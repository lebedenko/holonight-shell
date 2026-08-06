# Phase 10 — Bounded Workspace Pill Delegates: Design

**Input**: `poc-remediation-phase10/SPEC.md`
**Baseline**: `fdc7a23` with Phase 9 complete. Implementation shall revalidate
the current strip and test harness before editing.

## 1. Change Map

| Requirement | Primary implementation | Tests |
|---|---|---|
| F-01, F-02, F-03 | `apps/shell/qml/Topbar/WorkspacePillStrip.qml` | `tests/qml/tst_WorkspacePillStrip.qml` |

No C++ model, QML module registration, or test-fake change is expected.

## 2. Design Decisions

### 2.1 Window-local absolute-ID range

The strip will expose these derived properties:

```
firstStripId = max(1, windowStart - stripPad)
lastStripId = windowStart + displayCount - 1 + stripPad
stripCount = lastStripId - firstStripId + 1
```

Delegates retain stable absolute IDs within the currently rendered range via
`absoluteId = firstStripId + index`. The resulting count is bounded by
`displayCount + 2 * stripPad`, independent of compositor state and absolute
workspace position.

### 2.2 Absolute positioning without a global strip

The inner strip keeps a local coordinate system: a delegate uses
`x = index * pillStep`. Its container is placed relative to the viewport by:

```
x = glowMargin + (firstStripId - windowStart) * pillStep
```

For a one-step move, the rendered range initially remains unchanged while the
strip shifts one pill step. After the existing 200 ms behavior completes, a
zero-interval rebase replaces only the off-screen edge delegate and restores a
local coordinate origin. The overlapping visible pills retain exactly the same
screen positions at that handoff. If a large jump falls outside the current
rendered range, it rebases immediately to avoid exposing a gap. The strip width
therefore stays bounded and does not need to begin at absolute workspace ID 1.

### 2.3 One neighbor on each valid side

`stripPad` remains 1. The right pad continues to make a real pill available
before a one-step right pan. The new left pad gives the corresponding behavior
for a left pan whenever `windowStart > 1`. At workspace 1, there is no ID 0,
so only the right pad is created.

### 2.4 No raw maximum dependency

`WorkspaceModel.maxWorkspaceId()` is deliberately removed from the QML strip
calculation. Navigation availability remains owned by `WorkspaceSection` and
the model's `hasVisibleOrNavigableBeyond()` / urgency queries; these determine
whether a user may pan or jump, not how many visual delegates are allocated.

## 3. Test Strategy

| Scenario | Observable assertion |
|---|---|
| High sparse ID with a high window start | `stripCount` and repeater delegates remain at the fixed maximum. |
| Visible high-ID window | every visible absolute ID has a delegate and correct `wsId`. |
| Window at 1 | no ID-0 delegate; right pad remains present. |
| One-step right/left shift | final x and the visible/pad IDs match the window-local range. |

Tests inspect public QML properties and instantiated delegates through existing
test object names. They use `tryCompare` only for the real x animation and no
fixed sleeps.

## 4. Risks

- Changing the Repeater model destroys/recreates delegates at the range edge.
  Rebase occurs only after the x transition (or immediately for a jump that
  would otherwise leave a gap), and tests cover the final range.
- A direct large compositor jump creates only the local window. This is the
  intended bounded behavior; the focused active workspace is already in the
  visible range selected by `WorkspaceSection`.
