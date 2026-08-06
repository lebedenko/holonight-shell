# Phase 27 — Tray Pixmap Decode Efficiency: Design

**Input**: `poc-remediation-phase27/SPEC.md`
**Baseline**: Phase 26 accepted in `3d2c9fd`.
**Status**: Complete — implementation, automated validation, and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `libs/holonight-surfaces/src/TrayItem.cpp` | `tests/test_tray_item.cpp` |

## 2. Design Decisions

### 2.1 Write directly to image scanlines

The source bytes are already validated as exactly four bytes per pixel. Writing
`QRgb` values through `QImage::scanLine()` avoids `QImage::setPixel()`'s
coordinate and format-dispatch work while retaining the existing explicit
big-endian ARGB-to-`qRgba()` conversion.

### 2.2 Gate diagnostic assembly at the logging boundary

Candidate descriptions are useful only in an enabled info-level diagnostic.
The existing logging category's `isInfoEnabled()` check therefore encloses both
the string-list assembly and its corresponding log statement, preserving
enabled-log output without allocating strings in the usual disabled case.

### 2.3 Preserve observable behavior with a two-row fixture

Existing tests cover empty, invalid, exact-size, scaling, and candidate-choice
cases. A small two-row pixel fixture protects the row stride and input order
that direct scanline writes introduce.

## 3. Risks and Boundaries

- `QImage::Format_ARGB32` accepts `QRgb` scanline values; the conversion remains
  explicit rather than depending on host byte order.
- No public API or image-provider behavior changes.
