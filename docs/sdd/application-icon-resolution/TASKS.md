# Application icon resolution tasks

Status: deferred. Begin only after the current labwc desktop integration initiative.
All tasks below are pending; this document is a backlog, not authorization to start implementation now.

- [ ] Audit every shell icon consumer and document identity input, source type, precedence,
  fallback, rendering mode and invalidation. Capture Satty window identity and notification payload.
- [ ] Review scanner reuse and define canonical desktop IDs, directory precedence, Hidden masks,
  NoDisplay metadata inclusion, WM-class matching and ambiguity handling.
- [ ] Agree the shared catalog and typed resolver interface, target boundaries, QML facade,
  and theme revision mechanism. Review against SPEC acceptance criteria.
- [ ] Implement the catalog and resolver with indexed lookups, positive and negative caching,
  complete update generations and bounded diagnostics.
- [ ] Add fixture tests for identity matching, visibility separation, masking, collisions,
  missing metadata and artwork, installation/removal and theme invalidation.
- [ ] Migrate taskbar, active-window, chooser and overview; verify Satty and generic fallbacks.
  Migrate launcher metadata without changing launcher eligibility or search behavior.
- [ ] Migrate notification identity fallback and MPRIS; preserve notification payload semantics.
  Reuse shared tray loading where appropriate while retaining overrides, attention icons and pixmaps.
- [ ] Remove obsolete launcher-owned resolution and duplicate source routing after consumers migrate.
- [ ] Verify Original application artwork, Semantic shell glyphs, aspect ratio, stale-source resets,
  pointSize typography, badges, overflow and cross-surface consistency.
- [ ] Run the shell suite and required static checks; capture isolated labwc screenshots,
  rerun Sway smoke, and record live Hyprland acceptance or its availability limitation.
