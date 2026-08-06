# Phase 38 — Desktop Entry Field Mapping Consistency: Design

**Status**: Complete — automated checks and user verification passed.

**Input**: `poc-remediation-phase38/SPEC.md`
**Baseline**: Phase 37 accepted in `8e1d735`; implementation baseline is
`d793747`.
**Implementation**: `a4ed9b9` (`refactor: consolidate desktop entry fields`).

## 1. Revalidated Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | A compile-time scalar-field descriptor associated with `DesktopEntry` | Direct serializer round-trip coverage, plus scanner and cache paths |
| F-02 | `DesktopEntryScanner.cpp` and `DesktopEntrySerializer.h` consume the descriptor | Existing scanner/cache regressions extended to assert all mapped values |

At current `main`, `applyDesktopEntryField()` has explicit branches for eight
desktop-file string keys, `DesktopEntrySerializer::toJson()` writes nine
scalar text members explicitly, and `fromJson()` assigns the same nine members
explicitly. `desktop_file` is intentionally absent from scanner key handling
because `parseDesktopEntryFile()` derives it from the path.

## 2. Design

### 2.1 Descriptor-based scalar mapping

Introduce a small compile-time descriptor list beside `DesktopEntry`. Each
descriptor contains:

- the JSON key;
- an optional desktop-file key; and
- a pointer to the corresponding `QString DesktopEntry` member.

The list is the sole registry for the nine scalar text fields. This keeps the
data representation and the maintenance contract co-located, permits ordinary
type checking of member pointers, and avoids Qt reflection or runtime lookup.

The entry for `desktop_file` has no desktop-file key. The scanner retains its
explicit path assignment; JSON serialization still includes it through the
descriptor list.

### 2.2 Consumers retain their current responsibilities

`applyDesktopEntryField()` first resolves a matching descriptor with a
desktop-file key and assigns through its member pointer. Its existing branches
continue to own fields that are not scalar text: `Type`, booleans, action
sections, and `MimeType` splitting.

The serializer iterates the same descriptors to write and restore scalar text
values. Mandatory-field validation remains explicit and before construction:
it is a validity rule for three fields, not a field-mapping concern. Action
and MIME arrays remain explicit because they have filtering and collection
semantics unlike a direct `QString` assignment.

### 2.3 Compatibility and cache behavior

The descriptor preserves all existing JSON key strings and assigns the same
members, so the on-disk JSON payload shape does not change. `kCurrentVersion`
therefore remains `2`; Phase 37's current-version schema check and prepared
query lifecycle are deliberately untouched.

## 3. Risks and Mitigations

| Risk | Mitigation |
|---|---|
| A key typo could make a parsed field silently empty. | Use `QLatin1StringView`/`QStringLiteral` constants in one descriptor list and assert every scalar field through scanner and serializer tests. |
| Treating `desktop_file` as an XDG key could overwrite its source path. | Model its desktop-file key as absent and keep the existing explicit path assignment. |
| The registry could absorb non-equivalent collection or boolean semantics. | Limit it to direct scalar `QString` members; actions, MIME lists, and booleans stay in their existing specialized code. |
| A needless schema bump would discard a valid cache. | Preserve every JSON key/value and leave the cache version/schema untouched. |

## 4. Alternatives Rejected

- **Qt meta-object reflection**: `DesktopEntry` is a lightweight value struct,
  not a QObject; adding reflection would enlarge the design solely to replace
  a small static mapping.
- **A generic JSON object copied directly into the model**: this weakens the
  typed `DesktopEntry` contract and makes mandatory validation less clear.
- **Leaving independent lists with a comment**: comments do not make a new
  field omission mechanically visible; a shared descriptor removes the
  duplication instead.
