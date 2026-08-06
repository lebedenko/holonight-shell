# Phase 41 — QML Delegate Contract Hardening: Design

## 1. Current State

`WifiNetworkDelegate` consumes five Wi-Fi roles plus `index` through implicit
delegate context injection. `SidebarOverviewNotifications` similarly wraps an
implicit `modelData` value in a mutable property and reads an implicit index.

Four sidebar files contain delegates that reach outer component ids without
`pragma ComponentBehavior: Bound`. Calendar and upcoming-event delegates also
consume their model values implicitly, while `SidebarTabBar` already declares
its two model inputs and needs only the lexical binding mode.

## 2. Design

Add the bound-component pragma to each affected file. Declare required
properties directly on delegate roots:

- Wi-Fi: `index`, `ssid`, `strength`, `secured`, `known`, and `connected`.
- Notification preview: `modelData` and `index`.
- Calendar header/day cells: `modelData` and `index`.
- Upcoming events: `startTime`, `endTime`, `isAllDay`, `title`, `location`, and
  `index`, matching `CalendarEventModel::roleNames()`.

Bindings inside the upcoming-event delegate use its explicit root properties
instead of the legacy `model.roleName` object. This makes the ownership visible
and avoids relying on the dynamic delegate context. No service or model changes
are needed.

## 3. Tests

QML compilation and linting are the primary regression checks because missing
required roles or illegal outer-context captures fail component compilation.
Existing component-instantiation tests cover the sidebar tab bar and calendar;
the full QML harness exercises the network popup and Overview surfaces with fake
services. Existing interaction tests continue to cover their observable paths.

## 4. Risk

The runtime expressions and values are unchanged. The main risk is declaring a
role with the wrong QML type or omitting an implicit input; focused compilation,
lint, and harness execution expose either mistake deterministically.
