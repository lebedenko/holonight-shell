# Shared Shape Appearance Settings Design

**Status:** Approved for implementation
**Date:** 2026-07-27

## Architecture

`holonight-qt` remains the schema authority. `Holonight::AppearanceConfig` gains public corner-style
name conversion, normalization limits, and an atomic `save()` operation. This keeps JSON keys,
enum spelling, ranges, defaults, and serialization in the same library as parsing.

HoloNight Settings links `HolonightQt::Theme`. `ConfigFileService` loads the optional shared value
with `AppearanceConfig::load(AppearanceConfig::configFilePath())` and passes it to
`SettingsEditModel`. Saving asks the same value type to atomically persist itself.

## Edit Model

`SettingsEditModel` stores current and snapshot `AppearanceConfig` values. QML sees:

- `shapeCornerStyle` as the canonical string name;
- `shapeScale` as a real value;
- independent `baseRadiusEnabled` / `baseChamferEnabled` flags;
- `baseRadius` / `baseChamfer` numeric values.

NaN remains the C++ representation for an unset override, matching `AppearanceConfig`. The QML
enable flags prevent QML from having to interpret NaN as an optional value. Disabling an override
sets its stored value to NaN; enabling one restores the semantic default value of zero.

All setters normalize through limits exposed by `AppearanceConfig`. Shape state participates in the
existing current-versus-snapshot dirty comparison.

## Persistence and Failure Policy

Each file uses its existing atomic writer. The save order is `config.toml`, `theme.conf`, then
`appearance.json`. Cross-file atomic transactions are not available, so a later failure may leave
earlier files committed. Settings reports failure, names the failed path, and keeps the edit model
dirty so retry is safe. This is explicit best-effort consistency rather than pretending rollback is
possible after filesystem replacement.

An all-default shape value is written as an explicit version-1 file on Save & Apply. Settings never
deletes an existing file and never creates one during load.

## UI

The Appearance page adds:

- a four-choice global corner-style selector;
- a `0.25`–`4.0` shape-scale slider;
- an Advanced Shape section with independent switches and `0`–`128` sliders for radius and
  chamfer overrides.

Controls bind directly to `SettingsEditModel`, so Discard and subsequent loads update visible state.
Text identifies these settings as global to HoloNight applications.

## Verification

C++ tests cover missing and valid loads, dirty transitions, range normalization, independent
optional overrides, successful round-trip through the shared parser, and write failure. QML tests
or smoke loading cover the new bindings and interactions. Relevant formatting, QML lint,
qmltypes, Settings tests, and `holonight-qt` appearance tests complete verification.
