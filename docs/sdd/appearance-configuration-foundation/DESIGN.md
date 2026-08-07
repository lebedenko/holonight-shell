# DESIGN: Appearance Configuration and Shell Config Ownership

## Dependency correction

The current direction is inverted: Shell links a product-schema package physically owned and published by Settings.
The target direction is:

```text
holonight-config                 holonight-shell
HoloNight::Config               HoloNightShellConfig::Config
        |                                   |
        +-----------+-----------------------+
                    |
             holonight-settings

HoloNight::Config -> Shell AppearanceService -> QML + portal projection
HoloNightShellConfig::Config -> Shell ConfigService -> Shell product behavior
```

The Shell product package is a leaf library within this repository. A standalone CMake entry point under its
directory lets Settings build/install only that contract without discovering Wayland and compositor dependencies.
The root Shell build adds the same sources as a subdirectory, avoiding a second implementation.

## Product schema handoff

The initial Shell-owned library is derived from the currently published Settings package, then cleaned before it is
accepted as a new authority. Public names change deliberately to prevent an old and new package from looking
interchangeable. Appearance/theme structs and fields are deleted rather than deprecated.

During the cross-repository handoff, the published Shell package is the provider baseline. Settings removes its copy
only after that revision is available. The umbrella must never pin a state where Shell expects the deleted Settings
package but Settings has already removed it.

`ConfigService` remains the QObject/watch adapter for product settings. Its creation-on-missing and partial/default
policy remains a product concern and does not influence canonical appearance's strict whole-document policy.

## Appearance state

`AppearanceService` owns a provider value, resolved Qt/catalog projection, watcher, debounce/coalescing, and revision.
It uses the same two-phase candidate publication as the Qt design: parse/validate in isolation, resolve catalog
semantics, then replace the complete active state. The service exposes narrow change signals for QML plus a revision
for bridges that need a single invalidation point.

Shell already depends on `HolonightQt`; it reuses the published Qt reader/projection contract where exported rather
than implementing scheme/accent rules again. If the Qt package exposes only lower-level catalog functions, Shell's
adapter stays thin and tested against the same exact provider revision.

`ThemeService` has no independent state after adoption. Its D-Bus lifecycle may be folded into AppearanceService or
kept as a portal host injected with AppearanceService, but configuration loading and watching cannot remain there.
`SettingsPortalBackend` becomes a pure state-to-D-Bus adapter with an injectable value source for deterministic tests.

## Save-domain separation

The Shell product writer remains useful to Settings, but it cannot be called merely because the global appearance
edit model is dirty. Settings maintains independent snapshots and invokes each writer only for its changed domain.
There is no attempt to claim a cross-file atomic transaction. Each result is reported separately, and each successful
domain advances only its own saved snapshot.

This boundary removes appearance from the credential-bearing transaction without prematurely designing secret
migration. The planned [Shell Credential Storage](../shell-credential-storage/README.md) initiative will define
secret-store ownership, migration, failure recovery, and service access.

## Trade-offs

- Moving the package changes downstream includes and CMake names, but preserves domain ownership and prevents a UI
  repository from becoming Shell's schema authority.
- Keeping Qt types in the Shell product package is less neutral than the global appearance package, but matches its
  two Qt consumers and avoids expanding ACF into a second generic configuration framework.
- Separate save results can produce a successful appearance save and failed Shell-settings save. That is honest and
  recoverable; presenting two independent documents as one atomic transaction would be misleading.
