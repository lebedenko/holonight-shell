# Theme Variant Catalog Design

## Resolution Order

Consumers resolve appearance in this order:

1. Use a valid `appearance/scheme`.
2. If missing or invalid, map legacy `appearance/mode=light` to `holonight-light`.
3. Otherwise use `holonight-dark`.

The resolved scheme determines whether the system-facing appearance is dark or light. Raw `mode` is not a user-facing source of truth.

## Stable Defaults

For this pass, radius, metrics, typography, and font defaults stay identical across all schemes. Only color tokens and active accent slots are scheme/accent dependent.

## Shell Responsibilities

The shell settings writer stores `scheme`, `accent`, and derived `mode`. The shell portal backend uses the same scheme IDs to publish system appearance.
