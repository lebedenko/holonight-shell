# Phase 42 — NetworkManager Wi-Fi Activation Contracts: Design

## 1. Root Cause

NetworkManager represents connection settings as `a{sa{sv}}`: a map from
setting-group names to property maps. The backend currently requests saved
settings as a flat `QVariantMap` and constructs new settings with the same flat
container type. The nested values look plausible in C++, but their D-Bus type
does not match the NetworkManager methods.

When saved-profile decoding fails, a connected access point still appears
connected through active-connection data. After disconnect that fallback
disappears, `known` becomes false, and the QML correctly asks for a password.
Password activation then fails because `AddAndActivateConnection2` rejects the
flat settings argument.

## 2. Design

Use `QMap<QString, QVariantMap>` as the single `ConnectionSettings` type for
both directions and register it with Qt D-Bus in the concrete backend. Read
saved profiles through `QDBusReply<ConnectionSettings>` and construct new
activation settings with the same type.

No QML or service branching changes are required: restoring `known` and
`connection_path` makes the existing delegate call `connectNetwork()`, which
already routes saved profiles to `activateKnown()`.

## 3. Tests

Extend the fake NetworkManager integration boundary to:

- assert the initial saved profile marks its matching access point known and
  carries the settings object path;
- expose `AddAndActivateConnection2` with a nested-map argument and assert the
  received connection and security fields.

These tests validate the D-Bus signature rather than only the service's method
routing.

## 4. Risk

The change is isolated to settings serialization. Existing activation,
disconnect, scanning, and state-refresh paths remain intact. The primary risk is
Qt D-Bus metatype registration, covered by constructing the production backend
against the fake bus in every integration test.
