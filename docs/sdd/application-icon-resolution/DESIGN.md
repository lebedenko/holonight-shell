# Application icon resolver design

## Proposed boundaries

Keep desktop metadata parsing and the application catalog in `holonight-services`.
Expose a shell-owned shared application icon resolver to presentation consumers through a
small typed interface. Compositor adapters continue supplying identities only; they must
not read desktop files or load artwork. Keep raster loading in the surfaces image provider
and shared Qt rendering primitives. Confirm final target placement with architecture checks.

Separate the catalog from launcher search results. Reuse parsing and directory discovery,
but do not reuse a launchability-filtered model as the source of metadata truth.
`scanForDefaultApps(true)` is useful prior art, not a complete catalog contract: the current
scanner still requires launchable fields, deduplicates by basename, and records an ID only
when parsing succeeds. Review those rules before reusing them for nested IDs or Hidden masks.

## Identity and source selection

Build indexed maps for canonical XDG desktop IDs and WM classes. Apply directory precedence
before visibility filtering so a higher-priority Hidden record masks a lower-priority entry.
Compute nested desktop IDs relative to their applications directory. Match exact desktop ID
first, then exact WM class; permit documented case-insensitive fallback only when unambiguous.
Evaluate executable aliases during the audit and add them only with a clear collision policy.
Do not infer identities from window titles or translated application names.

A proposed result carries source kind, source value, matched desktop ID, match origin,
metadata revision and unresolved reason. Theme names, local paths, resources and pixmaps
must remain distinguishable through QML and image-provider boundaries. Share metadata
selection without forcing every protocol's payload into a single string or precedence rule.

Window surfaces request metadata artwork, then retain their existing drawn fallback.
Launcher rows use the same catalog metadata after applying their own visibility policy.
Notification presentation preserves explicit icon and image payload semantics, and resolves
an application identity fallback only when those sources are absent or unusable under the
existing notification contract. MPRIS desktop identities go through the resolver rather
than being assumed to be theme names. Tray overrides, attention icons, theme paths and
protocol pixmaps keep their protocol-specific precedence; reuse common loading where suitable.

Review both `IconImageProvider` strict and substituting modes and `ExternalIcon` source routing.
Use explicit Original rendering for application artwork and Semantic rendering for shell glyphs.
Preserve the notification distinction between app identity and notification content images.

## Updates and diagnostics

Scan and index metadata once per catalog generation outside the render path. Reuse existing
filesystem watcher and debounce machinery where it satisfies the catalog contract, including
atomic replacements and newly created application directories. Publish a revision after a
complete update. Cache misses as well as matches, and invalidate both on catalog changes.
Use the accepted shared Qt theme revision mechanism for image invalidation; audit its behavior
before introducing any additional watcher or revision API.

Provide bounded debug diagnostics for unmatched or ambiguous identities and missing artwork.
Include the identity, selected match origin and fallback reason, but never window titles or
notification bodies. Theme and metadata revisions must cause visible consumers to re-resolve.

## Migration and open decisions

First inventory consumers and their source precedence. Add the independent catalog and resolver
with fixtures, then migrate window surfaces, launcher metadata, notifications, MPRIS and suitable
tray loading paths in reviewed steps. Retain `LauncherService::iconForAppId()` temporarily as a
compatibility wrapper if needed, then remove it when no consumer depends on launcher-owned lookup.

Decide the typed public result and exact QML facade, metadata rules for entries without Exec,
executable alias support, and theme revision integration during design review. These choices
must not weaken Hidden masking, ambiguity handling or launcher visibility guarantees.
