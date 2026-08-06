# Phase 39 — Notification Rule Model Hardening

**Status**: Complete — automated checks and user verification passed.

## Objective

Bundle the two remaining small U-06 notification-model findings into one
cohesive remediation tranche.

| Source | Phase 39 item | Impact |
|---|---|---|
| U-06 I-05 | Bound automatically discovered notification rules | Untrusted session-bus app names cannot grow the in-memory and persisted rule list indefinitely. |
| U-06 I-06 | Give notification role enums explicit storage types | QML model-role contracts follow the repository's explicit ABI convention. |

## Functional Requirements

### REQ-F-01 — Bound notification rules

`NotificationRuleModel` shall retain at most 256 rules.

- New, previously unseen application names are ignored once the bound is
  reached; existing rules continue to refresh their metadata.
- An oversized persisted file is reduced to the 256 most recently seen rules
  and the reduced snapshot is persisted.
- Relative order among retained rules remains unchanged.
- Filtering, user edits, desktop-entry pruning, and notification delivery are
  unchanged.

### REQ-F-02 — Make role enum storage explicit

`NotificationRuleModel::Roles` and `NotificationService::Roles` shall use the
same explicit 16-bit unsigned underlying type convention as sibling Qt models.
Role values, names, QML visibility, and `QAbstractItemModel` behavior remain
unchanged.

## Verification

- Add deterministic tests for the insertion bound, oversized persistence
  recovery, and retained-rule ordering.
- Run focused notification-rule and notification-service tests, `task test`,
  `task architecture-check`, `task format-check`, changed-file clang-format,
  and `git diff --check`.

## Out of Scope

- Authentication or rate limiting of the freedesktop notification D-Bus API.
- A rule-deletion UI, LRU replacement during live discovery, or persistence
  schema changes.
- Any launcher, weather, portal, tray, or QML backlog candidate.
