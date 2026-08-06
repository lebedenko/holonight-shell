# Phase 27 — Tray Pixmap Decode Efficiency

**Status**: Complete — implementation, automated validation, and user verification passed.

## Objective

Remediate U-02 I-06: remove avoidable work from StatusNotifierItem pixmap
decoding while preserving the decoded image and selection behavior.

| Source | Phase 27 item | Impact |
|---|---|---|
| U-02 I-06 | Efficient tray pixmap decode | Tray icon updates avoid per-pixel setter overhead and disabled-log string construction. |

## Functional Requirements

### REQ-F-01 — Decode pixmaps without avoidable per-pixel API overhead

`decodePixmapList()` shall preserve the existing big-endian ARGB input contract,
best-fit candidate selection, validation, and scaling behavior while writing
decoded pixels directly to the allocated image rows.

- Pixel coordinates and channel values shall be preserved for multi-row images.
- Invalid inputs shall retain the existing safe rejection behavior.
- Candidate diagnostic strings shall be built only when the existing info log
  category is enabled.

**Acceptance**: focused tests verify exact pixels including row-major ordering,
and the existing invalid-input, selection, and scaling tests remain green.

## Constraints and Verification

- Keep the change local to tray pixmap decoding and its unit tests.
- Do not alter D-Bus wire format, icon-choice policy, validation limits, or log
  messages when info logging is enabled.
- Run focused surfaces tests, `task test`, `task format-check`, and
  `git diff --check`.

## Out of Scope

- Icon cache policy, asynchronous image-provider behavior, and tray watcher
  collection choices.
- The remaining queued Phase 7 Low-severity candidates.
