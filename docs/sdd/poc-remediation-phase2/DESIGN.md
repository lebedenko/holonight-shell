# POC Remediation Phase 2 — Design

| Field | Value |
|---|---|
| Document ID | poc-remediation-phase2/DESIGN.md |
| Cycle | Remediation Phase 2 of the POC Readiness Review |
| Input | `docs/sdd/poc-remediation-phase2/SPEC.md` (11 EARS requirements, 3 items, U-01 foundational-core), live repo at commit `0444cca` |
| Status | Ready for implementation |

---

## How to read this document

Three self-contained items, no shared files, no ordering dependency. Item D is the only one with an
actual code-structure decision (§1); Items E and F are mechanical logging additions and are each
covered in a few lines (§2, §3) — a code-path/interfaces/alternatives treatment for a single added
`qCWarning` line would be padding, not design. §4 covers the cross-cutting test strategy (REQ-C-3),
§5 is a decisions summary, §6 is known risks.

---

## 1. Item D — `parseWidgetEntry` restructure

### Current code path

`libs/holonight-config/src/ConfigParsers.cpp:412-463`. Today, `parseWidgetEntry` reads `type` and
`enabled`, then — **before looking at `type` at all** — short-circuits disabled entries:

```cpp
const bool enabled = entry["enabled"].value<bool>().value_or(true);
if (!enabled) {
  WidgetDefinition def;
  def.enabled = false;
  if (type == QLatin1String("clock")) {
    def.type = WidgetType::Clock;
  }
  return def;  // title/deadline/position/monitors/clock fields never parsed
}
```

Only when `enabled == true` does execution reach the `type == "time-to-event"` / `type == "clock"`
branches, each of which calls the four field-parser helpers and rejects the whole entry
(`return std::nullopt`) if `parseTimeToEventFields` or `parseWidgetPositionField` fails. Since
`ConfigWriter::writeWidgetDefinition` (`libs/holonight-config/src/ConfigWriter.cpp:53-86`)
unconditionally serializes `title`/`deadline`/`position`/`monitors` from whatever `WidgetDefinition`
it's given, a disabled entry's minimal `def` (default-constructed `time_to_event`/`position`/
`monitors`) gets written back as blank fields on the very next save — this is the data-loss bug.

The four field-parser helpers, and which ones can currently reject an entry:

| Helper | Can return "invalid"? | Behavior on invalid input today |
|---|---|---|
| `parseTimeToEventFields` (line 332) | Yes — `std::optional<TimeToEventConfig>`, `nullopt` on empty/missing title or missing/unparseable deadline | Rejects (propagates to `nullopt` in `parseWidgetEntry`) |
| `parseWidgetPositionField` (line 380) | Yes — `std::optional<WidgetPosition>`, `nullopt` only when an explicit `position` string fails to parse | Rejects |
| `parseClockFields` (line 366) | **No** — returns a plain `ClockConfig`, every field optional with an in-place default | Never rejects |
| `parseWidgetMonitors` (line 395) | **No** — returns `QStringList`, skips individual bad array entries with a warning, never rejects the whole list | Never rejects |

This matters for the design: only two of the four helpers need new behavior. The other two already
do exactly what REQ-F-D.2 wants (best-effort field-level defaulting) — they just were never being
*called* for disabled entries, because `parseWidgetEntry` returned before reaching them.

### Proposed change

**Mechanism: thread a `bool strict` parameter through the two helpers that can reject, and always
call all four helpers regardless of `enabled`.** `strict == enabled` at the call site — the helper's
existing "log a warning, then reject" branches become "log the warning, then reject **only if
`strict`**"; when not strict they fall through and leave the field at whatever safe value was already
computed in-place (which is precisely the safe default REQ-F-D.2 §2 asks for — see below).

`parseTimeToEventFields`:

```cpp
std::optional<TimeToEventConfig> parseTimeToEventFields(const toml::table& entry, bool strict) {
  TimeToEventConfig tte;

  const auto title_opt = entry["title"].value<std::string>();
  tte.title = title_opt ? QString::fromStdString(*title_opt) : QString{};
  if (tte.title.isEmpty()) {
    qCWarning(lcConfigParsers) << "Config: time-to-event widget requires a non-empty title; skipping";
    if (strict) {
      return std::nullopt;
    }
  }

  const auto deadline_opt = entry["deadline"].value<std::string>();
  if (deadline_opt) {
    const QString deadline_str = QString::fromStdString(*deadline_opt);
    tte.has_time = deadline_str.contains(QLatin1Char('T'));
    if (tte.has_time) {
      tte.deadline = QDateTime::fromString(deadline_str, Qt::ISODate);
    } else if (const QDate date = QDate::fromString(deadline_str, Qt::ISODate); date.isValid()) {
      tte.deadline = date.startOfDay();
    }
    if (!tte.deadline.isValid()) {
      qCWarning(lcConfigParsers) << "Config: time-to-event widget" << tte.title << "has unparseable deadline"
                                 << deadline_str << "— skipping";
      if (strict) {
        return std::nullopt;
      }
    }
  } else {
    qCWarning(lcConfigParsers) << "Config: time-to-event widget" << tte.title << "requires a deadline; skipping";
    if (strict) {
      return std::nullopt;
    }
  }

  tte.show_seconds = entry["show_seconds"].value<bool>().value_or(false);
  return tte;
}
```

Note the missing-deadline branch had to move from an early `return` to an `if (deadline_opt) {...}
else {...}` — the original code dereferenced `*deadline_opt` unconditionally after the early-return
guard, which is no longer safe once that guard becomes conditional. This is the one place the
restructure is more than "add an `if (strict)`"; every other early-return site keeps its original
shape. When `deadline_opt` is absent and not strict, `tte.deadline` is simply never assigned, so it
stays default-constructed (`QDateTime{}`, invalid) — exactly the "null `QDateTime{}`" default
REQ-F-D.2 specifies, with zero extra code to produce it.

`parseWidgetPositionField`:

```cpp
std::optional<WidgetPosition> parseWidgetPositionField(const toml::table& entry, const QString& label, bool strict) {
  const auto pos_opt = entry["position"].value<std::string>();
  if (!pos_opt) {
    return WidgetPosition::CenterCenter;
  }
  const QString pos_str = QString::fromStdString(*pos_opt);
  const auto position = widgetPositionFromString(pos_str);
  if (!position) {
    qCWarning(lcConfigParsers) << "Config: widget" << label << "has invalid position" << pos_str << "— skipping";
    if (strict) {
      return std::nullopt;
    }
    return WidgetPosition::CenterCenter;
  }
  return position;
}
```

One line added at the existing rejection site. `parseClockFields` and `parseWidgetMonitors` are
**unchanged** — no new parameter, no new branch — they already return best-effort values
unconditionally; the fix for them is purely "get called" (see below).

`parseWidgetEntry` restructured:

```cpp
std::optional<WidgetDefinition> parseWidgetEntry(const toml::table& entry) {
  const auto type_opt = entry["type"].value<std::string>();
  const QString type = type_opt ? QString::fromStdString(*type_opt) : QString{};
  const bool enabled = entry["enabled"].value<bool>().value_or(true);

  if (type == QLatin1String("time-to-event")) {
    auto tte = parseTimeToEventFields(entry, /*strict=*/enabled);
    if (!tte) {                              // only reachable when enabled == true
      return std::nullopt;
    }
    const auto position = parseWidgetPositionField(entry, tte->title, /*strict=*/enabled);
    if (!position) {                         // only reachable when enabled == true
      return std::nullopt;
    }

    WidgetDefinition def;
    def.type = WidgetType::TimeToEvent;
    def.enabled = enabled;
    def.time_to_event = *tte;
    def.position = *position;
    def.monitors = parseWidgetMonitors(entry, tte->title);
    return def;
  }

  if (type == QLatin1String("clock")) {
    const QString label = QStringLiteral("clock");
    const auto position = parseWidgetPositionField(entry, label, /*strict=*/enabled);
    if (!position) {                         // only reachable when enabled == true
      return std::nullopt;
    }

    WidgetDefinition def;
    def.type = WidgetType::Clock;
    def.enabled = enabled;
    def.clock = parseClockFields(entry);
    def.position = *position;
    def.monitors = parseWidgetMonitors(entry, label);
    return def;
  }

  // Unknown/missing type — non-goal (SPEC.md "Non-Goals" F-01): preserved byte-for-byte, including
  // its pre-existing enabled/disabled asymmetry. Do not fold this into the branches above.
  if (!enabled) {
    WidgetDefinition def;
    def.enabled = false;
    return def;
  }
  qCWarning(lcConfigParsers) << "Config: [[widget]] has unknown or missing type" << type << "— skipping";
  return std::nullopt;
}
```

### Non-goal: the unknown/missing-`type` branch is unchanged, and *why* the redesign doesn't touch it

This is the subtlety worth calling out explicitly, because a naive restructure gets it wrong. Today,
the `if (!enabled) { ...; return def; }` short-circuit runs **before any `type` check**, so a
disabled entry with an unrecognized (or missing) `type` currently returns a minimal, never-rejected
`WidgetDefinition` (type defaults to `WidgetType::TimeToEvent` via its member initializer, unless
`type == "clock"`) — it is *never* subject to the "unknown type — skipping" rejection that an
*enabled* entry with the same bad `type` hits. SPEC.md's Non-Goals section requires this asymmetry
to survive Phase 2 unchanged, "in both enabled and disabled cases."

The naive move — delete the early `if (!enabled)` block and let every entry fall through to the
type-dispatch chain — would make a disabled+unknown-type entry hit the final `qCWarning(...) <<
"unknown or missing type"... ; return std::nullopt;` branch for the first time, rejecting it where it
previously wasn't rejected. That is an unrequested behavior change on an explicit non-goal.

The design above avoids this by checking `type == "time-to-event"` and `type == "clock"` **first,
unconditionally** (this is where the actual fix — unconditional field parsing — lives), and only
*after* both of those fail to match does it re-derive the old `!enabled` short-circuit as the first
half of the fallback branch, ahead of the "unknown type" warning+reject. The fallback branch is
reached only when `type` is neither known value, so it is exactly the old early-return's audience,
reproducing its output (including "no warning when disabled") verbatim.

### Alternatives considered

**Separate lenient wrapper functions** (e.g. `parseTimeToEventFieldsLenient`) instead of a `strict`
parameter, called only from the disabled path. Rejected: this would duplicate ~25 lines of TOML
field-extraction logic per wrapper, with the two copies diverging silently over time as the TOML
schema evolves (e.g. a future new field added to one copy and forgotten in the other) — exactly the
kind of unnecessary abstraction/duplication CLAUDE.md's clang-tidy conventions steer away from, for
no benefit over a one-line-per-site conditional.

**A result-type wrapper** (e.g. `struct FieldResult<T> { T value; bool valid; }`) returned
unconditionally by both helpers, letting the caller itself decide reject-vs-default based on
`enabled`. Rejected: this changes both helpers' return type away from the already-idiomatic
`std::optional<T>` this file uses everywhere else (see `parseNotificationHistory`,
`parseWidgetPositionField` itself), touches every call site more invasively than adding one bool
parameter, and buys nothing — the caller-side logic ("if enabled, `nullopt` means reject; if
disabled, `nullopt` can't happen") is already fully expressed by `strict` threaded straight through.
It is the helper-proliferation alternative REQ-C-2 ("no architectural refactoring") argues against.

**Only change `parseWidgetEntry`'s dispatch order, keep helpers strict-only, and post-process the
result** (call the strict helpers, catch `nullopt`, and re-run a hand-rolled "grab title from `entry`
raw" fallback in `parseWidgetEntry` itself when disabled). Rejected: this re-implements each helper's
extraction logic a second time at the call site instead of once inside the helper, and risks the
fallback and the helper's "what counts as invalid" logic drifting apart — the `strict` parameter
keeps a single source of truth for what each field's valid range and default is.

### Interfaces

```cpp
// Signature changes (both file-local to ConfigParsers.cpp, no header declarations — these are
// static helpers in an anonymous namespace / TU-local, called only from parseWidgetEntry).
std::optional<TimeToEventConfig> parseTimeToEventFields(const toml::table& entry, bool strict);
std::optional<WidgetPosition> parseWidgetPositionField(const toml::table& entry, const QString& label, bool strict);

// Unchanged signatures:
ClockConfig parseClockFields(const toml::table& entry);
QStringList parseWidgetMonitors(const toml::table& entry, const QString& label);
std::optional<WidgetDefinition> parseWidgetEntry(const toml::table& entry);  // unchanged signature, changed body
```

No `config_structs.h` changes — `WidgetDefinition`, `TimeToEventConfig`, `ClockConfig`,
`WidgetPosition` are all unchanged. This is purely a parser-logic fix; the data model already had
room for every field on a disabled widget (`enabled` is just one member of `WidgetDefinition`
alongside the rest, not a discriminator that hides them).

### Known risks (Item D specific)

See §6 for the cross-cutting write-back behavior-change risk — it is Item D's most consequential
side effect and is called out there since it interacts with the manual acceptance criterion
(REQ-F-D.4).

---

## 2. Item E — `HyprlandIpc.cpp` logging parity

Purely mechanical: add one `qCWarning(lcHyprlandIpc) << "<FunctionName>: expected JSON object";`
line to each of 3 silent parsers, matching the exact convention already used in 4 sibling parsers in
the same file (`parseHyprlandMonitorsJson`, `parseHyprlandFocusedMonitorNameJson`,
`parseHyprlandClientsJson`, `workspaceIdForHyprlandClientAddressJson` — all at
`libs/holonight-platform/src/HyprlandIpc.cpp`, each logging
`qCWarning(lcHyprlandIpc) << "<FunctionName>: expected JSON <array|object>";` immediately before
their own `return std::nullopt;` on the shape-mismatch branch).

Convention to mirror, verified from the sibling at line 258:

```cpp
qCWarning(lcHyprlandIpc) << "parseHyprlandMonitorsJson: expected JSON array";
```

Three insertion points (each: add the line immediately before the existing `return std::nullopt;`
inside the `!doc.isObject()` branch — no other line in each function changes):

| Function | Line (current) | Message to add |
|---|---|---|
| `parseHyprlandActiveWindowJson` | `HyprlandIpc.cpp:35-37` | `qCWarning(lcHyprlandIpc) << "parseHyprlandActiveWindowJson: expected JSON object";` |
| `parseHyprlandKeyboardLayoutDevicesJson` | `HyprlandIpc.cpp:66-68` | `qCWarning(lcHyprlandIpc) << "parseHyprlandKeyboardLayoutDevicesJson: expected JSON object";` |
| `parseHyprlandActiveWorkspaceJson` | `HyprlandIpc.cpp:188-190` | `qCWarning(lcHyprlandIpc) << "parseHyprlandActiveWorkspaceJson: expected JSON object";` |

`parseHyprlandActiveWorkspaceJson`'s second rejection branch (`workspace_id <= 0`) is **not** touched
— REQ-F-E.1's acceptance criterion only asks for the `!doc.isObject()` branch, and that also matches
every sibling's convention: none of the 4 already-logged parsers warn on value-level validation
either (e.g. `parseHyprlandMonitorsJson` silently skips entries with an empty `name` or `ws_id <= 0`
without logging) — only on the JSON-shape mismatch. Extending logging to value-level rejects would be
new scope beyond what SPEC.md asks and beyond the existing convention it's matching.

---

## 3. Item F — `runCommand` logging parity

Equally mechanical: one `qCWarning(lcHyprlandIpcClient) << ...` line in
`HyprlandIpcClient::runCommand` (`libs/holonight-platform/src/HyprlandIpcClient.cpp:65-68`), mirroring
`connectEventStream`'s existing pattern at line 31:

```cpp
qCWarning(lcHyprlandIpcClient) << service_name_ << "HYPRLAND_INSTANCE_SIGNATURE not set";  // connectEventStream, line 31
```

Insertion point — before the existing `return false;`:

```cpp
const QString path = resolvedCommandSocketPath();
if (path.isEmpty()) {
  qCWarning(lcHyprlandIpcClient) << service_name_ << "HYPRLAND_INSTANCE_SIGNATURE not set; dropping command"
                                 << command;
  return false;
}
```

This extends `connectEventStream`'s exact phrasing ("HYPRLAND_INSTANCE_SIGNATURE not set") with
`"; dropping command"` plus the `command` payload, per REQ-F-F.1's four required elements (service
name, reason, dropped command, `lcHyprlandIpcClient` category) — `connectEventStream` doesn't need
the "dropping command" suffix or a command argument since it has no command to name, so the two
messages are siblings, not identical text. No other line in `runCommand` changes; `return false;`
and all downstream behavior are untouched (REQ-F-F.2 — logging only, no retry/queue).

---

## 4. Test strategy (REQ-C-3)

SPEC.md's acceptance criteria name three new files (`tests/unit_widget_disabled_field_preservation.cpp`,
`tests/unit_hyprland_ipc_logging.cpp`, `tests/unit_hyprland_ipc_client_logging.cpp`). Checking the
actual repo against that assumption: **all three components already have a live, populated test file
exercising them**, in the naming convention this repo actually uses (`test_*.cpp`, not `unit_*.cpp` —
already flagged as a pre-existing SPEC/repo naming mismatch by the Phase 0 design doc, §0):

| Component | Existing file | Currently in | Already covers |
|---|---|---|---|
| `parseWidgetEntry` / widgets | `tests/test_config_parsers.cpp` | `test_holonight_core` | `WidgetsParserKeepsValidDefinitionsAndSkipsInvalidOnes` — parses a 4-entry `[[widget]]` TOML fixture including one `enabled = false` clock entry; asserts `.enabled == false` and `.type == Clock` for it (does **not** currently assert on field preservation, so it won't need changing — it will keep passing once fields become populated) |
| `HyprlandIpc.cpp` parsers | `tests/test_hyprland_ipc.cpp` | `test_holonight_platform` | Direct-call unit tests per parser, e.g. `ParsesActiveWindowJson` / `IgnoresMalformedActiveWindowJson` for `parseHyprlandActiveWindowJson` |
| `HyprlandIpcClient::runCommand` | `tests/test_hyprland_ipc_client.cpp` | `test_holonight_platform` | `ScopedHyprlandSocketEnv` RAII fixture (sets `XDG_RUNTIME_DIR`/`HYPRLAND_INSTANCE_SIGNATURE`, creates the socket dir) plus a `TestSocket` double already used by `EmitsCompleteEventLinesFromPartialChunks` et al. |

**Decision: add new `TEST`/`TEST_F` cases to these three existing files, do not create the three
files SPEC.md names.** This is a deliberate deviation from SPEC.md's literal acceptance-criteria
wording, called out here per REQ-C-3's own instruction to document a test's purpose and per this
project's stated preference (CLAUDE.md, testing conventions) against inventing parallel
infrastructure — all the fixtures the new tests need (TOML-string-to-`ParsedConfig` parsing,
`QTest::ignoreMessage`-based log capture, `ScopedHyprlandSocketEnv`) already exist in these files.
Three new files would either duplicate that infrastructure or `#include` across test TUs, both worse
than adding cases to the file that already owns the fixture. `tests/CMakeLists.txt` needs **no
changes** — all three files are already listed against their respective executables.

### Log-capture mechanism

SPEC.md's Item E/F acceptance criteria suggest "`QTest::ignoreMessage()` or a custom log sink."
`QTest::ignoreMessage(QtWarningMsg, "<exact message>")` is the established pattern in this repo
(`tests/test_ext_workspace_manager.cpp:17`, `tests/test_shell_application.cpp:16`, both from Phase 1)
— no custom sink exists or is needed. New tests use the same mechanism, matching the exact message
text designed in §2/§3.

### Item D test cases (added to `tests/test_config_parsers.cpp`)

New `TEST`s alongside the existing `WidgetsParserKeepsValidDefinitionsAndSkipsInvalidOnes`:

- `DisabledTimeToEventPreservesValidFields` — `enabled = false`, valid `title`/`deadline`, asserts
  both are populated (not default) in the returned definition (REQ-F-D.1).
- `DisabledClockPreservesFields` — `enabled = false`, `type = "clock"` with `date_format`/`locale`
  set, asserts they're populated (REQ-F-D.1).
- `DisabledEntryPreservesPositionAndMonitors` — `enabled = false` with explicit `position` and
  `monitors`, asserts both are present rather than defaulted (REQ-F-D.1).
- `DisabledEntryWithMissingTitleDefaultsToEmptyString` — `enabled = false`, no `title` key, asserts
  `time_to_event.title == QString{}` and no rejection (REQ-F-D.2).
- `DisabledEntryWithInvalidDeadlineDefaultsAndWarns` — `enabled = false`, `deadline = "not-a-date"`,
  `QTest::ignoreMessage` for the existing unparseable-deadline warning, asserts
  `!time_to_event.deadline.isValid()` and no rejection (REQ-F-D.2).
- `DisabledEntryWithInvalidPositionDefaultsToCenterCenter` — `enabled = false`,
  `position = "invalid-position-name"`, asserts `position == WidgetPosition::CenterCenter` and no
  rejection (REQ-F-D.2).
- `DisabledEntryMonitorsSkipsNonStringEntriesOnly` — reuses the existing fixture's mixed
  `["eDP-1", 99, "HDMI-A-1"]` `monitors` array but on a disabled entry, asserts only the two strings
  survive (regression check that `parseWidgetMonitors`'s existing per-entry skip is unaffected).
- `EnabledEntryWithInvalidFieldStillRejectsWholeEntry` — `enabled = true` (or omitted, defaulting
  true) with an invalid field (e.g. bad `position`), asserts the surrounding `parseConfig()` /
  `parseWidgetEntry()` result **excludes** that widget entirely — regression proof that REQ-F-D.3's
  strict path is unchanged.
- `DisabledEntryWithUnknownTypeStaysUnchanged` — `enabled = false`, `type = "bogus"` (or `type`
  omitted), asserts a minimal definition is still returned (type defaults to `TimeToEvent`, no
  fields populated, no warning) — regression proof the non-goal branch in §1 wasn't altered.

REQ-F-D.4 (Settings-app round-trip survives a save) stays a **manual** acceptance criterion exactly
as SPEC.md scopes it — no automated test is added for it; §6 discusses why it's still worth a human
pass given the write-back behavior change.

### Item E test cases (added to `tests/test_hyprland_ipc.cpp`)

- `WarnsOnMalformedActiveWindowJson`, `WarnsOnMalformedKeyboardLayoutDevicesJson`,
  `WarnsOnMalformedActiveWorkspaceJson` — each feeds a non-object payload (e.g. `"[]"` or
  `"not json"`) to the respective parser wrapped in `QTest::ignoreMessage(QtWarningMsg, "<exact
  message from §2>")`, asserting `has_value() == false` (REQ-F-E.1).
- A regression pass re-running the same malformed-input pattern against the 4 already-logged
  siblings (`parseHyprlandMonitorsJson` et al.), each also under `QTest::ignoreMessage`, confirming
  their pre-existing logging wasn't disturbed (REQ-F-E.2). Several of these parsers likely already
  have malformed-input tests in this file predating Phase 2 — extend them with `ignoreMessage` rather
  than adding a parallel test if so, rather than duplicating a `TEST`.

### Item F test cases (added to `tests/test_hyprland_ipc_client.cpp`)

- `RunCommandWarnsAndDropsWhenSignatureUnset` — constructs a bare `HyprlandIpcClient` (no explicit
  socket paths) with `HYPRLAND_INSTANCE_SIGNATURE` unset (`qunsetenv`, no `ScopedHyprlandSocketEnv`),
  wraps a `runCommand("set_workspace 3")` call in `QTest::ignoreMessage(QtWarningMsg, "...")` matching
  §3's message, and asserts the return value is `false` (REQ-F-F.1, REQ-F-F.2).
- `RunCommandSucceedsWithoutWarningWhenSignatureSet` — reuses `ScopedHyprlandSocketEnv` (already
  proven able to stand up a resolvable socket path) and asserts `runCommand()` returns `true` with no
  `qCWarning` emitted — the non-regression half of REQ-F-F.2's acceptance criterion.

---

## 5. Key decisions summary

| # | Question | Decision |
|---|---|---|
| 1 | Item D mechanism | Add a `bool strict` parameter to the two helpers that can reject (`parseTimeToEventFields`, `parseWidgetPositionField`); leave the two that already can't reject (`parseClockFields`, `parseWidgetMonitors`) untouched. `parseWidgetEntry` calls all four unconditionally, threading `strict = enabled`. |
| 2 | Non-goal preservation | Type-dispatch on `"time-to-event"`/`"clock"` moves ahead of the `enabled` check (that's the actual fix); the old `!enabled` short-circuit is kept, verbatim, as the first half of the fallback branch reached only when `type` matches neither known value — so the unknown/missing-type asymmetry between enabled and disabled entries is untouched. |
| 3 | Items E/F | Mechanical `qCWarning` additions, message text and insertion point derived directly from each function's already-logged sibling in the same file. |
| 4 | Test file strategy | Extend the three existing test files (`test_config_parsers.cpp`, `test_hyprland_ipc.cpp`, `test_hyprland_ipc_client.cpp`) instead of creating SPEC.md's three literally-named new files — deliberate deviation, documented in §4, matching the fixtures those files already own. `tests/CMakeLists.txt` needs no edits. |
| 5 | Log capture | `QTest::ignoreMessage`, the pattern already established in Phase 1's test files — no new log-sink infrastructure. |

---

## 6. Known risks

- **Write-back behavior change for existing users (Item D)**: this is a bug fix, but it's worth
  naming plainly — after this ships, the *first* Settings-app save of a config that has an existing
  disabled widget will write back that widget's real `title`/`deadline`/`position`/`monitors` instead
  of blanking them, because `parseWidgetEntry` now actually parses those fields on load. For a user
  who disabled a widget and saved *before* this fix (title/deadline already overwritten to empty on
  disk), the fix does not un-erase already-lost data — there's nothing left to preserve by the time
  they load with the fixed parser. The fix only stops the loss going forward. Worth a one-line mention
  in release notes if this project does them, but no code-level mitigation is possible or in scope
  (REQ-C-2 rules out speculative recovery/migration logic).
- **`parseTimeToEventFields` control-flow shape change**: the missing-deadline branch changes from an
  early `return std::nullopt;` to an `if (deadline_opt) {...} else {...}` wrapping the rest of the
  deadline logic (§1) — this is the one helper where the diff is not a pure one-line addition. Worth
  extra review attention / a dedicated read-through against the pseudocode in §1 during implementation,
  since a mis-placed brace here would silently change which fields get parsed under which `strict`
  value.
- **`parseWidgetPositionField`'s label parameter now flows from a possibly-invalid `TimeToEventConfig`
  under non-strict mode**: `parseWidgetEntry`'s time-to-event branch calls
  `parseWidgetPositionField(entry, tte->title, ...)` — when disabled and the title was invalid/missing,
  `tte->title` is now an empty string (not `nullopt`, since `strict=false` never rejects), so the
  position-parser's warning message reads `Config: widget "" has invalid position ...` instead of
  naming the widget. This is a pre-existing message-quality characteristic of using the title as a
  human-readable label (the same thing happens today if a valid *enabled* widget somehow reached this
  line with an empty title, which currently can't happen because the strict title check rejects first)
  — not a new defect, but worth knowing the log line degrades gracefully rather than crashing.
- **Test-file growth**: adding ~11 new `TEST`s across three already-populated files (§4) grows them
  further rather than giving each item its own small file; acceptable here since REQ-C-3 permits
  either interpretation and every new test is a handful of lines reusing existing fixtures, but flagged
  in case a future reviewer expected the SPEC.md filenames literally.
