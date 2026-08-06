# Binary Size Baseline — arch-restructure-roadmap

Used by T-000 and T-015 (M2 acceptance) to validate ≤ 5% binary-size regression.

## Measurement

| Field              | Value                                      |
|--------------------|--------------------------------------------|
| **Byte count**     | 5,715,352 bytes (5.7 MB)                   |
| **Date**           | 2026-05-28                                 |
| **Commit SHA**     | ce9b0851ac4838e736fc38dea481b775527ca7ef   |
| **Build type**     | Debug (`-DCMAKE_BUILD_TYPE=Debug`)         |
| **Strip command**  | `strip --strip-debug`                      |
| **Host CPU**       | Intel Core i9-14900HX (x86_64)            |
| **Compiler**       | GCC 16.1.1 (g++ 16.1.1 20260430)          |

## Procedure

```bash
task clean -y
task configure   # -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=OFF
task build
cp build/holonight-shell build/holonight-shell.sizecheck
strip --strip-debug build/holonight-shell.sizecheck
stat --format="%s" build/holonight-shell.sizecheck
```

## Re-measurement at M2 (T-015)

Use identical build flags and the same `copy-and-strip` invocation on the same host.
Acceptance threshold: new stripped size ≤ **6,001,119 bytes** (baseline × 1.05).

Do **not** strip `build/holonight-shell` in place.
