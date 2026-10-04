# Local CI tasks

- [x] Inspect baseline, provider and isolated runtime contracts.
- [x] Add shared launcher, environments, lanes, workflows and task.
- [x] Fix owned host diagnostics and review changes.
- [x] Verify required lanes, launcher failures, source/build isolation and evidence.
- [x] Record acceptance and local handoff.
- [x] Verify the developer tidy entry point with explicit fresh compiler contexts.

2026-10-04 publication merge acceptance: incoming `a979784` is retained alongside
local CI and host-analysis corrections. Conflict resolutions preserve positioned
desktop/window menus, workspace occupancy/visibility and stable model identities
during title updates. `task ci`'s clean build/test and licensing lanes pass in
`20261004T171842Z-vuimyzez`; obsolete static snapshots were deliberately stopped
and their nonzero results are retained. The final fresh static lane is recorded
separately after completion. Host GCC/Qt build and all 1,216 CTest cases pass;
24 affected final retests, format/QML lint/types, architecture/import policy,
launcher/tooling regressions, REUSE and seven affected clang-tidy 23 translation
units (including shared test headers) pass. Logs are under `build/ci/publication-*`.
Publication and umbrella pinning are authorized by the user.

The final clean pinned static lane passes in `20261004T174222Z-o21rl18i`
(`publication-static-accepted.log`): full build, format-check, full tidy and
installed-package consumer analysis. Complete logs were reviewed; no actionable
compiler/analysis warnings remain. Combined current acceptance is build/test and
licensing from the initial clean merge snapshot plus corrected native affected
retests and this final fresh static snapshot. The stopped obsolete static lanes
remain accurately recorded as exit 143, not passing runs.
