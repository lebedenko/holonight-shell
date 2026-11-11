# Package updates topbar widget

One ShellApplication-owned PackageUpdatesService serves every monitor. Startup requests
session D-Bus activation once for org.holonight.Packages1, then asynchronously reads
Updates and optional UpdateCheck properties. No polling, Refresh or CheckNow is used.
Owner changes clear cached data; generation and per-interface request counters discard
obsolete replies. Property changes and invalidations trigger a fresh GetAll.

The widget precedes Network and collapses when absent or ready with zero updates.
Loading/checking takes visual priority; errors retain the last successful count and
mark results as potentially stale. Counts above 99 display 99+; tooltips retain exact
counts, ignored packages, download bytes converted to MiB and known timestamps.
Missing check metadata is distinct from a failed completed check.

Left click dismisses the monitor-routed tooltip and launches holonight-packages through
ApplicationLaunchService. Concurrent launches are suppressed; failures remain visible
until a successful retry. No settings, package backend or checking schedule changes.

Acceptance uses a private D-Bus provider and offscreen widget tests, QML lint and
metadata checks. Live compositor review covers appearance, tooltip routing, clicking
and multiple monitor bars sharing the same results.
