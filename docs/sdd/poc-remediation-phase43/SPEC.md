# Phase 43 — Settings Application Cleanup

## Goal

Bundle the seven remaining U-11 settings-application findings from the Phase 7
backlog. Preserve settings values, save behavior, page navigation, and visual
appearance while tightening model notifications and QML contracts.

## Scope

| Source | Phase 43 item | Required outcome |
|---|---|---|
| U-11 I-C2 | Guard parsed-config notifications | Loading emits only signals for values that changed and clears dirty state through the model's normal recomputation path. |
| U-11 I-C3 | Remove dead save overload | `SettingsEditModel` exposes only the production save-snapshot API. |
| U-11 I-C5 | Cache font families | Fixed-pitch filtering reuses the font-family snapshot captured by the model. |
| U-11 I-Q1 | Bind settings delegates | Delegate access in `NavPanel` and `AppearancePage` is checked under bound component semantics. |
| U-11 I-Q2 | Unify page selection state | Navigation highlight and loaded content consume one authoritative page key. |
| U-11 I-Q5 | Cache navigation colors | Page selection does not reconstruct constant palette-derived colors per delegate. |
| U-11 I-Q6 | Declare plain settings text | Settings `Text` items explicitly use `Text.PlainText`. |

## Acceptance Criteria

1. Re-loading identical parsed configuration emits no property or dirty-state
   notifications.
2. Loading saved configuration over an unsaved edit restores the value and
   clears dirty state with exactly the relevant notifications.
3. The unused `markSaved(ParsedConfig, QString)` overload has no declaration,
   definition, or call site.
4. Toggling `fixedPitchOnly` filters and restores the model without querying a
   new family list.
5. Settings navigation keeps its highlight and content synchronized through a
   single page property.
6. Settings delegates compile with `pragma ComponentBehavior: Bound`, and all
   settings `Text` items select plain-text rendering explicitly.
7. Focused settings tests, QML lint/type checks, and the project test suite pass
   apart from documented pre-existing failures.
8. Manual verification confirms Appearance, Bar, and placeholder-page
   navigation, editing, discard, and save behavior remain unchanged.

## Non-goals

- Redesigning the settings UI or adding completed content for placeholder pages.
- Refreshing an existing model in response to fonts installed during its
  lifetime.
- Changing theme catalogs, persisted configuration formats, or numeric ranges.
- Addressing U-07, U-10, or other non-U-11 backlog findings.

## Backlog Accounting

Phase 41 left 15 queued Low-severity candidates. Phase 42 repaired a newly
reported functional defect and did not change that count. Acceptance of this
seven-item tranche reduces the queued Phase 7 backlog from 15 to 8.
