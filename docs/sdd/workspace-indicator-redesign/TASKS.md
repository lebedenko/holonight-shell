# SDD Tasks — workspace-indicator-redesign

- [x] T-001: Extend ExtWorkspaceGroup to track wl_output* pointers
  - REQs: REQ-C-001
  - Check: ExtWorkspaceGroup::outputs_ list is populated on output_enter/output_leave and monitorNames() returns correct monitor names via QScreen matching

- [x] T-002: Update ExtWorkspaceHandle to distinguish special vs. numeric workspaces
  - REQs: REQ-F-005
  - Check: ExtWorkspaceHandle sets is_special=true for non-numeric names and is_special=false for numeric names after toInt() parsing

- [x] T-003: Add SpecialWorkspaceEntry struct and applySpecialWorkspaces to WorkspaceModel
  - REQs: REQ-F-005, REQ-F-006
  - Check: WorkspaceModel::specialWorkspaceList() returns QVariantList of SpecialWorkspaceEntry{name, active, urgent} matching Wayland protocol state

- [x] T-004: Implement activeWorkspaceForMonitor per-monitor query in WorkspaceModel
  - REQs: REQ-C-001, REQ-F-003
  - Check: WorkspaceModel::activeWorkspaceForMonitor(QString monitorName) returns the correct active workspace ID for that monitor and emits signal on changes

- [x] T-005: Add maxWorkspaceId helper to WorkspaceModel
  - REQs: REQ-F-003
  - Check: WorkspaceModel::maxWorkspaceId() returns highest entry.id from rows_ (excluding specials) or 0 if empty

- [x] T-006: Add edge-arrow helper methods to WorkspaceModel
  - REQs: REQ-F-004
  - Check: All five new methods (hasOccupiedOrUrgentBeyond, hasUrgentBeyond, firstUrgentIdBeyond, hasUrgentBefore, lastUrgentIdBefore) return correct boolean/int values for their edge-query predicates

- [x] T-007: Remove six overflow* methods from WorkspaceModel
  - REQs: REQ-F-001
  - Check: grep for 'overflowWorkspaceId\|overflowWorkspaceState\|overflowUrgentWorkspaceId\|overflowUrgentWorkspaceState\|overflowUrgentWorkspaceLabel\|hiddenUrgentWorkspaceCount' in WorkspaceModel returns zero matches

- [x] T-008: Update ExtWorkspaceManager to route special workspaces via applySpecialWorkspaces
  - REQs: REQ-F-005, REQ-F-006
  - Check: ext_workspace_manager_v1_done() correctly separates numeric entries into applyBatchUpdate() and special entries into applySpecialWorkspaces()

- [x] T-009: Unify urgent color to accentViolet in WorkspacePill.qml
  - REQs: REQ-F-007, REQ-NF-003
  - Check: grep for 'borderUrgent\|error' in WorkspacePill.qml returns zero matches; urgent state uses accentViolet for both border and text color

- [x] T-010: Add FocusedInactiveMonitor styling and top-center dot to WorkspacePill.qml
  - REQs: REQ-F-008, REQ-NF-002
  - Check: Pill in FocusedInactiveMonitor state renders with border.width=0, fill=workspaceOccupied, textColor=textSecondary, and animated 3–4px cyan dot at top-center

- [x] T-011: Create WorkspacePillStrip.qml clipped viewport component
  - REQs: REQ-F-002, REQ-F-003, REQ-NF-002
  - Check: WorkspacePillStrip renders exactly displayCount numbered pills, clips to visible window, and animates strip.x position smoothly (150–250ms) on windowStart changes

- [x] T-012: Create WorkspaceEdgeArrow.qml component
  - REQs: REQ-F-004, REQ-NF-002, REQ-NF-003
  - Check: WorkspaceEdgeArrow renders chevron glyph, shows accentViolet with pulsing glow (0.5–1.5s cycle) when urgent, workspaceOccupied without glow otherwise, and emits activated() on click

- [x] T-013: Create SpecialWorkspaceDot.qml component
  - REQs: REQ-F-005, REQ-F-006, REQ-NF-002, REQ-NF-003
  - Check: SpecialWorkspaceDot renders 4px dot (hidden state), 8px dot (active/urgent), animates color/size (100–150ms), pulses glow when urgent, and calls activateSpecialWorkspace(name) on click

- [x] T-014: Rewrite WorkspaceSection.qml with sliding-window mechanism
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-006, REQ-C-005, REQ-NF-001
  - Check: WorkspaceSection computes window_start from activeWorkspaceForMonitor(), renders edge arrows conditionally, separates pills from dots only when specialWorkspaceList().length > 0, and wires all arrow/dot clicks correctly

- [x] T-015: Update tests/test_workspace_model.cpp — replace overflow tests with new helper tests
  - REQs: REQ-F-004
  - Check: All old OverflowZero/OverflowReturned test variants are deleted; new tests for hasOccupiedOrUrgentBeyond, hasUrgentBeyond, firstUrgentIdBeyond, hasUrgentBefore, lastUrgentIdBefore all pass

- [x] T-016: Update tests/test_integration_workspace_config.cpp to migrate from overflow methods
  - REQs: REQ-F-004
  - Check: Test reference to overflowWorkspaceId() at lines 87–106 is migrated to hasOccupiedOrUrgentBeyond() and passes

- [x] T-017: Add C++ unit tests for per-monitor active workspace and special workspaces
  - REQs: REQ-C-001, REQ-F-005, REQ-F-003
  - Check: GTest suite for activeWorkspaceForMonitor(), maxWorkspaceId(), specialWorkspaceList(), applySpecialWorkspaces(), and activateSpecialWorkspace() all pass with correct state transitions and signal emissions

- [x] T-018: Create QML test tst_WorkspaceSection.qml for window centering and lifecycle
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-006
  - Check: Test cases for window-centering algorithm (worked examples), manual pan-offset survival/reset, edge-arrow visibility conditions, and separator/dot instantiation (zero-specials = not in DOM) all pass

- [x] T-019: Create QML test tst_WorkspacePillStrip.qml for sliding-viewport behavior
  - REQs: REQ-F-002, REQ-F-003, REQ-NF-002
  - Check: Test cases for pill count constancy, position animation on windowStart changes, and clip boundaries all pass

- [x] T-020: Create QML test tst_WorkspacePill.qml for urgent color and FocusedInactiveMonitor styling
  - REQs: REQ-F-007, REQ-F-008, REQ-NF-003
  - Check: Tests verify urgent pills render accentViolet, FocusedInactiveMonitor pills have zero border and cyan dot, and palette inspection confirms no borderUrgent/error usage

- [x] T-021: Create QML test tst_WorkspaceEdgeArrow.qml for arrow styling and dispatch
  - REQs: REQ-F-004, REQ-NF-002
  - Check: Tests verify color/glow state transitions, pulsing animation (0.5–1.5s cycle, ≥2 intensity levels), and activated() signal dispatch for both urgent-jump and pan behaviors

- [x] T-022: Create QML test tst_SpecialWorkspaceDot.qml for dot lifecycle and interaction
  - REQs: REQ-F-005, REQ-F-006, REQ-NF-002
  - Check: Tests verify radius/color/glow per state (Hidden=4px/disabled, Active=8px/cyan, Urgent=8px/violet+glow), state transitions animate smoothly, and clicking invokes activateSpecialWorkspace()

- [ ] T-023: Live verification of special workspace protocol support (manual)
  - REQs: REQ-C-002, REQ-C-004
  - Check: Developer in live Hyprland session confirms special-workspace dots appear, state changes (Hidden/Active/Urgent) reflect Wayland protocol, and clicking activates; findings documented in VERIFICATION.md

- [ ] T-024: Live verification of per-monitor active workspace distinction (manual)
  - REQs: REQ-C-003, REQ-C-004
  - Check: Developer in live ≥2-monitor Hyprland session confirms each topbar centers independently on its monitor's active workspace and window changes are isolated per-monitor; findings documented in VERIFICATION.md
