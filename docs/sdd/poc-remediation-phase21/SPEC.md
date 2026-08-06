# Phase 21 — Shared Desktop File Discovery

**Status**: Complete — implementation and user verification passed.

## Objective

Remediate U-05 I-03: consolidate the identical recursive `.desktop`-file
discovery logic used by the application-cache rebuilder and session
integration diagnostics.

| Source | Phase 21 item | Impact |
|---|---|---|
| U-05 I-03 | Share `.desktop` file discovery | Rebuild eligibility and MIME-cache diagnostics use one defined traversal policy. |

## Functional Requirements

### REQ-F-01 — One desktop-file discovery policy

`ApplicationCacheRebuilder` and `SessionIntegrationService` shall call one
shared helper to determine whether an application directory contains a
`.desktop` file.

- The helper shall recursively search regular files using the existing
  `*.desktop` name filter.
- Both existing call sites shall preserve their surrounding eligibility rules:
  cache rebuilding still additionally requires a writable directory; MIME-cache
  diagnostics still report any existing directory containing desktop files.
- Empty directories and directories without matching files shall remain
  excluded.
- The change shall not alter command execution, diagnostic row contents, or
  public service APIs.

**Acceptance**: focused tests demonstrate that the shared helper finds a
nested `.desktop` file and ignores a similarly named non-matching file, while
the existing rebuild and diagnostics tests retain their current observable
behavior.

## Constraints and Verification

- Keep this phase scoped to U-05 I-03; do not cache filesystem results or
  change recursive traversal, permissions, or diagnostic policy.
- Keep the utility internal to the session-integration implementation area;
  do not add a new framework-level abstraction.
- Run focused session-integration tests, `task test`, `task format-check`, and
  `git diff --check`.

## Out of Scope

- The other 49 queued Low-severity candidates after this planned tranche.
- U-05 portal naming, MIME-cache copy-on-write, portal timeout, and portal
  color-scheme cleanup items.
- Any user-facing change to application discovery, MIME cache rebuilding, or
  session diagnostics.
