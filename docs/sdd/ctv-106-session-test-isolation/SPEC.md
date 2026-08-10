# CTV-106 Session Test Isolation

## Problem

The session-script test for an unavailable appearance adapter removes only its fixture executable while retaining
`/usr/bin` in `PATH`. Once the production adapter is installed there, the test discovers the host binary and no
longer exercises the unavailable-command branch.

## Contract

- The unavailable-adapter scenario must not resolve any host-installed adapter.
- Required fixture commands remain available through the isolated fake-bin directory.
- Production session behavior is unchanged.

## Verification

Run the focused `test_session_scripts` CTest entry, the full Shell suite, and the existing format, tidy, QML,
architecture, package, and desktop-integration checks.
