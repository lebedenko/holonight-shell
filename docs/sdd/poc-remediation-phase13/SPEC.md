# Phase 13 — Adaptive Status Popup Sizing

**Status**: Accepted

## Objective

Replace the fixed, per-popup status-surface dimensions with an adaptive but
bounded sizing policy. Informational popup content should use a comfortable
natural size when its monitor has room; scrolling should appear only when the
screen-safe size cap prevents the content from fitting.

## Functional Requirements

### REQ-F-01 — Popup size policies are explicit

Each status popup shall have an explicit sizing policy: minimum, preferred, and
maximum content dimensions, plus whether variable-length content belongs in an
internal scroll region. The policy must be owned centrally rather than spread
through ad-hoc QML constants.

**Acceptance**: unit coverage verifies the policy selected for Weather, Audio,
Network, Battery, and the fallback popup; no duplicate fixed-size lookup
remains.

### REQ-F-02 — Geometry respects the selected monitor's safe area

Status-popup geometry shall constrain its surface to the target monitor's
available vertical and horizontal space, including the panel notch and glow
padding. The pointer remains aligned to the triggering widget as far as the
edge constraints allow.

**Acceptance**: geometry tests cover a roomy monitor, a vertically constrained
monitor, and an edge anchor; every returned surface fits the permitted monitor
area and has a valid pointer position.

### REQ-F-03 — Fixed informational content uses its preferred height

Weather and comparable fixed informational content shall receive their
preferred height when the monitor can accommodate it. On a roomy monitor, the
Weather details and graphs shall be visible without an unnecessary vertical
scrollbar or flick gesture.

**Acceptance**: live verification on the primary display shows the full Weather
composition at its preferred height without scrolling.

### REQ-F-04 — Scrolling is a constrained-space fallback

When a monitor cannot accommodate a popup's preferred content height, the
central geometry shall cap the surface and the affected content shall retain a
bounded internal overflow path. Content below the cap must remain reachable.

**Acceptance**: deterministic QML coverage creates a capped Weather viewport,
confirms overflow is clipped, and scrolls to the final row; live verification
on a constrained monitor or equivalent display setup confirms the same.

### REQ-F-05 — Dynamic collections retain stable outer frames

Audio and Network popups shall keep stable designed outer dimensions during a
visible session. Their device, stream, and network lists may scroll internally;
arriving or disappearing rows must not repeatedly resize or reposition the
outer layer-shell surface.

**Acceptance**: QML/component coverage and live inspection show list overflow
is reachable without surface churn.

## Constraints and Verification

- Keep layer-shell placement, dismiss overlay behavior, keyboard handling, and
  popup notch/glow visuals intact.
- Compute the selected monitor's bounds from its actual geometry/available
  geometry; do not assume a primary-monitor origin or fixed desktop height.
- Apply an outer-popup size once for a visible session. Do not continuously
  resize a layer-shell surface in response to list or service updates.
- Preserve the Phase 12 Weather `Flickable` as the constrained-space overflow
  mechanism, but it must be non-interactive when the preferred frame fits.
- Keep the change scoped to status popups; tooltip and tray-menu sizing are not
  part of this phase.
- Add focused geometry and QML tests, then run `task test`, `task qml-lint`,
  `task qmltypes-check`, `task architecture-check`, and `git diff --check`.

## Out of Scope

- A generic arbitrary-content measurement and asynchronous surface-resize
  protocol.
- Redesigning Audio, Network, Weather, Battery, tooltip, or tray-menu visual
  content.
- Logging changes and the separate logging redesign.
