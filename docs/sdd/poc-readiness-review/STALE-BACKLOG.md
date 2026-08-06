# Stale Backlog Context

**These items are pre-existing incomplete work from prior SDD cycles and are NOT addressed by this review cycle.**

This section catalogs unchecked `TASKS.md` items from prior SDD feature cycles, as of the start of the poc-readiness-review cycle. It exists for transparency and context only — per REQ-C-3, no stale task listed here was analyzed, resumed, or executed as part of this review. Where a unit review's prior-context reading touched on one of these features (e.g., U-03 touching `idle-management`, U-05 touching `portal-service`), that review assessed the *current code*, not the outstanding stale tasks.

| Feature Directory | Unchecked Count | Sample Task Titles |
|---|---|---|
| portal-service | 12 | T-059: Verify PortalService.available is true on live session with xdg-desktop-portal; T-060: Verify interfaces QStringList contains expected portal interface names |
| idle-management | 7 | T-032: Verify ext-idle-notify-v1 backend activation and idle threshold crossing; T-035: Verify sidebar Keep Awake toggle holds logind inhibitor |
| launcher-desktop-cache | 4 | T-010: Verify cache load latency performance; T-011: Verify validator does not block UI during filesystem scan |
| holonight-settings | 2 | T-059: Verify shell ConfigService detects written changes; T-065: Final acceptance smoke test |
| sidebar-upcoming-calendar | 2 | T-039: Implement CalendarSyncManager::onConfigReloaded() to rebuild providers and re-run testConnection(); T-061: End-to-end verification: build, run shell, open sidebar, verify calendar widget displays cached events |
| theme-variant-catalog | 2 | Apply scheme-first resolution in `holonight-qt` when that repository is edited; Add resolver/platform-theme token coverage in `holonight-qt` |
| workspace-indicator-redesign | 2 | T-023: Live verification of special workspace protocol support (manual); T-024: Live verification of per-monitor active workspace distinction (manual) |
| launcher-redesign | 1 | T-024: Manual smoke test — Browse mode, Search mode, Recent tracking, action launching |
| right-sidebar | 1 | T-017: Add keyboard navigation |
| system-appearance-portal | 1 | Restart/start `xdg-desktop-portal` after installing portal files during manual validation |

**Total unchecked**: 12 + 7 + 4 + 2 + 2 + 2 + 2 + 1 + 1 + 1 = **34**

**Note on count discrepancy**: TASKS.md's T-013 acceptance check states the ten per-feature counts "sum = 32." All ten individual per-feature figures above match SPEC.md §12 exactly (re-verified 2026-07-05 via `grep -c '^- \[ \]'` against each feature's current `TASKS.md`, with zero drift from the SPEC.md-recorded values). However, 12 + 7 + 4 + 2 + 1 + 1 + 2 + 1 + 2 + 2 arithmetically sums to **34**, not 32 — the "32" figure in the TASKS.md check text is an addition error in that check's own wording, not a discrepancy in the underlying data. This is flagged here rather than silently corrected in TASKS.md, so REPORT.md's remediation-context reader has visibility into it.

**Explicit scope statement**: None of the above stale tasks were resumed, executed, validated, or otherwise acted upon during the poc-readiness-review cycle. They are listed here solely so a future remediation effort has visibility into pre-existing incomplete work alongside the findings produced by this review.
