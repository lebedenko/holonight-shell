# CTV-104 design

Qt activation extends the existing session-environment pipeline. Shell defaults are assigned in the bootstrap before
the existing D-Bus/systemd import. The service wrapper allowlist recovers the same two values. This keeps direct,
UWSM, D-Bus-activated, and systemd-service applications on one contract without adding configuration files or a
second source of truth.

Diagnostics compare the process values with the supported defaults and compare the systemd values with the process
values. The latter verifies that preserved overrides are imported as faithfully as defaults.
