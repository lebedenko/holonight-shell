# Phase 31 — Portal D-Bus Adapter Naming: Design

**Input**: `poc-remediation-phase31/SPEC.md`
**Baseline**: Phase 30 accepted in `54133c3`.
**Status**: Complete — automated checks and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | Rename `portal/NullPortalBackend.{h,cpp}` to `portal/PortalDbus.{h,cpp}` and update service/test includes | Existing `test_portal_service.cpp` coverage |

## 2. Design Decisions

### 2.1 Rename the misleading file pair, not the established types

`IPortalDBus`, `SystemPortalDBus`, and `NullPortalDBus` already express their
client-side broker role. Rename their containing file pair to `PortalDbus` so
an include communicates the same role. Keeping type names stable avoids a
mechanical API rename with no additional clarity.

### 2.2 Preserve the server-side backend name

`SettingsPortalBackend` is a distinct, server-side D-Bus implementation owned
by `ThemeService`. Its name matches the class and its role, so it remains
unchanged. The resulting filenames make the direction visible:

| Client-side broker adapter | Server-side implementation |
|---|---|
| `PortalDbus` | `SettingsPortalBackend` |

### 2.3 Make the rename atomic

Update every live include in the same change and verify CMake's existing
recursive source discovery picks up the renamed pair. Completed SDD documents
intentionally retain historical filenames, so they are not part of the
mechanical rename.

## 3. Risks and Boundaries

- A missed source/test include would fail at compile time; the focused portal
  tests and project suite cover this directly.
- No method, class, or runtime D-Bus behavior changes, so no live compositor
  verification is expected for this documentation-oriented cleanup.
