# Phase 46 — Phase 7 Backlog Reconciliation: Design

## Evidence-first closeout

Use the Phase 12 implementation commit, SDD, current source, and focused QML
tests as four independent checks. The current code must show each `MultiEffect`
before its source rectangle, a repeating 100 ms brightness timer that stops on
commit, and a clipped vertical weather `Flickable` whose content height follows
the existing stack.

## Accounting correction

Phase 41 correctly addressed the other three U-09 findings, but its numeric
handoff started from a count that still included the Phase 12 items. Subtract
those three from Phase 41 and propagate the corrected count through Phases 43,
44, and 45. Do not rewrite the scope, acceptance evidence, or commits of those
phases.

## Verification Strategy

Run the Phase 12 QML regression cases, QML lint, QML type metadata checks, and
architecture checks. Since reconciliation changes documentation only, the
recent Phase 45 full-suite result remains the production-code baseline.
