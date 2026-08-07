# Shell Credential Storage

**Initiative:** SCS (planned)
**Owner:** `holonight-shell`
**Status:** Planned
**Predecessor:** [Appearance Configuration Foundation](../appearance-configuration-foundation/SPEC.md)

## Purpose

Move Shell credentials and private endpoint material out of `config.toml` without reopening the appearance
configuration boundary established by ACF-006.

The initiative must define secret-store ownership, provisioning, migration and rollback, unavailable-store behavior,
service access, and Settings UX before implementation begins. Its initial inventory includes weather API keys, CalDAV
identity/keyring references, and ICS URLs that may embed authentication material. Documentation and tests must use
redacted values and reserved example domains only.

## ACF boundary

SCS is intentionally independent of ACF-006. Global appearance remains a separate read-only Shell domain, and neither
appearance reads nor appearance reloads may parse, expose, or write product configuration or secret storage.
