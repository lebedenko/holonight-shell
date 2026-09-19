# Verification: Consistent key hints

Date: 2026-09-20. Provider: `8fe24ff83f8108631c2b7b0351994f7e513acfcd`
(published and pinned before consumer implementation). Host Qt: 6.11.2.

## Local checks

- Fresh Release acceptance: `task build:release RELEASE_BUILD_DIR=/tmp/key-hints-shell-clean NPROC=2`: passed. Build logs contain no
  compiler warnings; Shell emits the expected Qt private-module ABI notices.

- `task build:dependencies NPROC=6`, `task configure-tests NPROC=6` and
  `cmake --build build -j6`: passed against the accepted provider, including
  the Qt Wayland adapter.
- Focused launcher, audio and authentication QML regressions: passed.
- `task test NPROC=6`: **1,175/1,175 CTest entries passed**. The suite uses
  its private D-Bus session and filesystem/socket isolation; sandbox restrictions
  required an elevated retry. No native pointer/focus automation was used.
- `task qml-lint NPROC=6`: passed with existing unresolved AudioService tooling
  warnings in unchanged audio components. No changed-file warnings.
- `task qmltypes-check NPROC=6`: QML metadata, module packaging and
  authentication metadata checks passed.
- `task format-check`, `task license-check`: passed. REUSE required permission
  for its multiprocessing socket. Installation/authentication package tests
  are included in the full CTest run.
- Authentication real-model capture: **7/7 passed**, Holonight style, software
  renderer, scale 1.25 with the accepted provider explicitly on QML_IMPORT_PATH.
  Reviewed [capture](authentication-1.25.png): baseline alignment, badge border
  and button bounds are intact.

## Review and handoff

The shared provider's verification covers all symbol mappings, punctuation, legacy
text, wrapping, accessibility, disabled state and the 8/12/18 pt multi-font matrix.
Consumer changes preserve their authoritative shortcut bindings.

Native ecosystem interaction remains a user-performed integration check. This
repository has not been published or pinned by this consumer work.
