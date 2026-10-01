# Shared application icon resolution

## Status and scheduling

Deferred local SDD. Address this after the current labwc desktop integration initiative
in `../labwc-desktop/SPEC.md`; it is not an acceptance gate for that initiative.
Baseline shell commit: `50143ee`. This SDD proposes future work and adds no resolver implementation.

## Problem and evidence

Window icons currently depend on the launcher's visible application inventory.
`LauncherService::iconForAppId()` matches a desktop filename or `StartupWMClass` against
`LauncherModel` entries. `DesktopEntryScanner::scanWithDirs()` excludes `NoDisplay=true`
entries, so applications deliberately absent from the launcher also lose their window icons.

The installed Satty entry demonstrates the problem: `/usr/share/applications/satty.desktop`
has `NoDisplay=true`, `Icon=satty`, and `StartupWMClass=com.gabm.satty`.
The icon exists at `/usr/share/icons/hicolor/scalable/apps/satty.svg`, yet the taskbar uses
its generic window fallback. Keeping Satty out of the launcher is correct.

Notifications follow another path: `NotificationServer` retains a supplied `app_icon`, or
uses image-path or desktop-entry hints, and `ToastItem` passes the result to `ExternalIcon`.
This allows an icon to appear in a notification despite failed window metadata lookup.
The exact payload from Satty's observed notification has not been captured; a supplied icon
is an explanation supported by the code, not a verified trace of that notification.

## Scope

Review application identity lookup, source selection, loading, rendering and invalidation
across the launcher, taskbar, active-window section, window chooser and overview,
notifications and history, MPRIS, tray, and other application metadata consumers.
Retain the shared Qt icon primitives and the existing explicit rendering contract from
`../size-aware-icons/README.md`. Application artwork must keep its original colors and aspect ratio.
Palette-aware shell glyphs remain semantic icons.

This initiative does not change compositor protocols, window commands, grouping, launcher
visibility policy, Settings APIs, or user configuration. It must not add a Satty-specific
alias, broaden launcher results, or embed ad hoc resolution logic in taskbar QML.

## Requirements

1. Application metadata lookup must operate independently of launcher visibility.
   Include valid `NoDisplay=true` records in the metadata catalog while preserving
   launcher filtering. Respect XDG directory precedence and `Hidden=true` masking.
2. Resolve desktop IDs, optional `.desktop` suffixes and `StartupWMClass` deterministically.
   Define case normalization, nested desktop IDs and collisions in one place.
   Do not silently pick an ambiguous application or match by its translated display name.
3. Keep source types distinct: application identity, theme icon name, local file or resource,
   and protocol-supplied pixmap. An arbitrary desktop ID is not automatically an icon name.
4. Preserve caller-specific explicit artwork, notification images and tray overrides.
   Shared metadata supplies application identity fallback; it must not overwrite valid
   notification content, attention icons, embedded pixmaps, or caller overrides.
5. Keep loading failures observable. A strict lookup reports unresolved metadata or artwork;
   each surface chooses its established visual fallback without a hidden generic substitution.
6. Refresh successful and failed lookups after desktop metadata or theme changes.
   Clear stale images and failed-image state when the application or selected source changes.
   Avoid repeated filesystem scanning and linear inventory searches for every delegate.
7. Application icon selection for the same identity must agree across shell surfaces when
   no explicit caller artwork is supplied. Surface-specific fallback glyphs may differ.

## Acceptance and verification

- Satty remains absent from launcher results and displays its real icon in window surfaces.
- Fixtures cover NoDisplay, Hidden masks, user overrides, nested desktop IDs, suffixes,
  WM-class matches, case handling, ambiguous matches, empty IDs and missing artwork.
- Supplied notification icons and image data, MPRIS identity, tray pixmaps, attention state
  and configured tray overrides retain their precedence and behavior.
- Installation, removal, metadata edits and theme changes invalidate positive and negative
  cache entries; QML switches sources without retaining the previous application's image.
- Offscreen rendering checks verify original colors, preserved aspect ratio and explicit
  fallback behavior. Typography uses pointSize only.
- Run shell tests, architecture boundaries, formatting, QML lint and QML metadata checks.
  Capture normal and narrow taskbars in isolated labwc, rerun isolated Sway, and perform
  live Hyprland checks when available. Record unavailable checks explicitly.
