# SPEC: Consistent key hints

Approved scope: user-supplied implementation plan, 2026-09-20.
Work package KH-002; upstream baseline `ab780d5d477400cdc70195d37783e9e080ee1d05`.

## Requirements

- When semantic key groups are supplied, hints shall use the shared renderer and preserve shortcut activation.
- When displaying symbols, the provider shall scale and align vector ink with resolved-font capital metrics.
- When space is constrained, shared wrapping shall prefer alternatives, then keys.
- When assistive technology reads hints, it shall receive translated semantic names without decorative duplicates.
- When disabled, hints shall retain the shared disabled palette.
- Literal text shall remain supported without parsing.

## Owned scope

Launcher, authentication and audio hints.

## Acceptance

Focused regressions, repository acceptance and QML checks; provider gallery startup and install tests; visual review at 8/12/18 pt, multiple fonts and fractional scales. Manual ecosystem checks remain required. Publication and umbrella pins need explicit authorization.
