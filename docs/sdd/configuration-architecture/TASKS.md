# CA-003 implementation

- [x] Inspect existing implementation and preserve public contracts.
- [x] Implement repository-owned scope from README.
- [x] Add focused behavioral regression coverage.
- [x] Run focused checks, clean acceptance and installed consumer checks.
- [ ] Review final diff, commit, publish and hand off exact revision.

2026-10-06 local: clean `task ci` passed 1153 registrations, QML lint, full source/test/package-consumer static analysis, formatting and licensing (`build/ci/20261005T220206Z-38vdcygp/`). Four private-bus cases skipped in the general suite passed in the dedicated private-bus registration. All 6429 log lines reviewed. Clean providers: Config `733781607124fc9bec0820c880e7467d08b34a50`, Qt `98803bca05e16ae0d0784a6cb43b0ace561385de`, System `398804a7cce5a57f9f6870c4e7ec99e9b1f3ddaa`. Subsequently installed Config `d6a392b41991f70a004d58f7694c7b6115cb7280` and verified focused consumers; its rollback-only fix does not change Shell reader behavior. Separate baseline-analysis cleanup is included in the same publication.

## CA-003a: preserve the existing Settings Kelvin choice

Settings consumer inspection found that strict temperature metadata omitted its existing Kelvin choice. The schema now accepts `kelvin`; decoding and metadata retain its meaning. On 2026-10-06 local, all 10 Shell configuration package tests passed; both affected translation units passed focused analysis and formatting. A fresh Release build/install of the standalone package and its installed consumer passed (`build/configuration-kelvin-clean.log`), using accepted Config `d6a392b41991f70a004d58f7694c7b6115cb7280`. The earlier full Shell acceptance remains applicable to unaffected application code; no application implementation changed in this correction.
