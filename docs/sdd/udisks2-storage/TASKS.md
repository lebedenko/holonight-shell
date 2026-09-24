# Tasks

| Task | Requirements | State |
|---|---|---|
| C++ model and filtering | R1–R3 | Done |
| Actions and popup presentation | R4, R6 | Done |
| Automated acceptance | R1–R4, R6 | Done |
| Manual hardware and compositor acceptance | R1–R4, R6 | In Progress |

- `task test`: 1179/1179 passed outside the sandbox (private D-Bus sockets require host access).
- Clean Debug build in `/tmp/holonight-shell-storage-acceptance`: passed.
- `task architecture-check`, `scripts/check-qmltypes.sh build`: passed.
- QML lint: passed with existing imported Audio type metadata warnings; no Storage warnings.
- Scoped clang-tidy, changed C++ formatting, `git diff --check`, and REUSE lint: passed.
- `task compositor-smoke-check`: checklist produced; manual completion remains pending.
- Storage coverage includes removable/fixed/empty/locked filtering, operation dispatch, scope confirmation,
  and loading the populated Storage widget/popup through the QML smoke harness.

Verified on 2026-09-24 against published provider
`5f2ecda7eea653995f4c860bfb7f3a3f53beb279`, with Qt 6.11.2 and GCC 16.2.1.

Manual USB/optical/polkit and cross-application acceptance remains pending. The first reported USB insertion
showed nothing in either application. Read-only host inspection found only internal NVMe devices in both
`lsblk` and `udisksctl status`, the installed `/usr/bin/holonight-shell` running, and no `hn-files` process.
The user was asked to leave the stick attached, try another port, and run the local builds. This is not a
successful hardware acceptance result. No system installation was performed.

Manual follow-up: the user reports that connecting the stick directly to the laptop works as expected in both
applications. Host inspection confirms the Transcend JetFlash and its mounted ARCH_202609 filesystem.
Connection through the USB hub remains under investigation: neither the initial host snapshot nor the boot
kernel log showed an external hub. Separate unmount/recovery, optical and polkit scenarios remain unconfirmed.

Hub investigation resolved: the user found the unpowered hub was physically disconnected. After reconnecting
it and inserting the USB stick through it, both applications worked as expected. USB detection/display is
manually confirmed for direct and hub connections; no hub-specific code correction is needed. Detailed
unmount/removal recovery, optical-media and polkit checks are not inferred from this report.
