# Topbar Logo Section – Architecture / Design

**Feature Name:** Themed Distro Logo Rendering with Config Overrides
**Date:** 2026-07-22
**Status:** IMPLEMENTED — VERIFIED against SPEC.md
**Source of truth for behavior:** `docs/sdd/topbar-logo-section/SPEC.md` (this document does not
restate acceptance criteria; it explains how each requirement is realized in code, why, and what
was rejected)

---

## 1. Component Diagram / Data Flow

```
┌─────────────────────┐   parseConfigTable()   ┌──────────────────────┐
│  ~/.config/holonight │ ─────────────────────► │ ParsedConfig.logo    │
│  /config.toml        │   parseLogo() (new)    │ (LogoConfig)         │
│  [logo] section       │                        └──────────┬───────────┘
└─────────────────────┘                                    │ applyParsedConfig()
                                                             ▼
                                                  ┌──────────────────────┐
                                                  │   ConfigService       │
                                                  │   logo_ (LogoConfig)  │
                                                  │   logo() accessor     │
                                                  └──────────┬───────────┘
                                                             │ constructor injection
                                                             │ (ShellApplication::ShellApplication)
                                                             ▼
┌──────────────────────┐  readOsRelease()  ┌──────────────────────────────┐
│ /etc/os-release        │ ────────────────► │  SystemInfoService            │
│ (existing parseOsRelease)│                  │  ctor(ConfigService*, parent) │
└──────────────────────┘                     │  applies 5-step precedence:   │
                                              │   1. config_->logo().file      │
┌──────────────────────┐  mapDistroIdToLogoName() │ 2. config_->logo().generic  │
│ SystemInfo.cpp         │ ◄──────────────────── │ 3. distro alias table         │
│ kDistroLogoAliases     │                        │ 4. findSystemLogoPath()       │
│ (new static table)     │                        │    (unchanged, REQ-C-001)     │
└──────────────────────┘                        │ 5. resolveThemeLogoIconName() │
                                                  │    (unchanged, REQ-C-002)     │
                                                  └───────────┬────────────────────┘
                                                               │ Q_PROPERTY (CONSTANT)
                                                               ▼
                                                  logoSource : QString
                                                  logoTinted : bool   (new)
                                                               │
                                                               ▼
                                        apps/shell/qml/Topbar/LogoSection.qml
                                        HnIcon { source: SystemInfoService.logoSource
                                                 tinted: SystemInfoService.logoTinted }
```

Key property of this flow: everything above the `Q_PROPERTY` line executes exactly once, inside
`SystemInfoService`'s constructor call chain, before the QML engine ever reads `logoSource` —
there is no async step and no signal crossing between `ConfigService` and `SystemInfoService`
(see §4.2, no live reload).

---

## 2. New / Changed C++ Interfaces

### 2.1 `LogoConfig` struct — `libs/holonight-config/include/holonight_config/config_structs.h`

Modeled directly on `WeatherConfig`/`BackgroundConfig` style (plain aggregate, defaulted
`operator==`, doc comment explaining the empty-string sentinel):

```cpp
struct LogoConfig {
  QString file;          // tilde-expanded absolute path; empty = no file override
  bool generic{false};   // true = force the bundled generic tux logo

  bool operator==(const LogoConfig&) const = default;
};
```

Added to `ParsedConfig` (`config_parsers.h`) as a new member `LogoConfig logo;`, alongside the
existing section members. No `MissingDefaults` bits are added for `logo.*` — see §4 rationale
("no missing-default write-back for `[logo]`"), matching the precedent already set by
`WeatherConfig::latitude`/`longitude`/`city` (optional fields commented as having "no missing
default write-back" in `ConfigParsers.cpp`).

### 2.2 `parseLogo()` — `libs/holonight-config/src/ConfigParsers.cpp`

Follows the exact shape of `parseWeather()`: reads a `toml::node_view` for the section, uses the
existing `readStr`/`expandTilde` helpers, and does not write missing-default lines back (see
§4.1 below for why).

```cpp
LogoConfig parseLogo(const toml::table& tbl) {
  LogoConfig cfg;
  const auto sec = tbl["logo"];

  bool file_missing_ignored = false;
  cfg.file = readStr(sec["file"], cfg.file, "logo.file", file_missing_ignored);
  if (!cfg.file.isEmpty()) {
    cfg.file = expandTilde(cfg.file);
  }

  cfg.generic = sec["generic"].value<bool>().value_or(false);

  return cfg;
}
```

Wired into `parseConfigTable()`:

```cpp
parsed.logo = parseLogo(table);
```

Note this function takes `const toml::table&` only (no `MissingDefaults&` out-param), matching
`parseCalendar()`'s signature — both are sections whose fields are optional-by-absence and are
never round-tripped into the generated default-config comments. `parseWeather`/`parseBackground`
take `MissingDefaults&` only because their *presence* in the written-out default config file is
part of onboarding UX (a fresh user gets a `[weather]` stub with commented instructions); `[logo]`
has no such onboarding value — an absent `[logo]` section is a fully valid, common, silent
no-op (precedence step 1 and 2 both fall through immediately), so nothing needs to be written back.

### 2.3 `ConfigService::logo()` accessor — `libs/holonight-core/src/ConfigService.h`/`.cpp`

Mirrors every other section accessor exactly:

```cpp
// .h
[[nodiscard]] const LogoConfig& logo() const { return logo_; }
...
LogoConfig logo_;
```

`applyParsedConfig()` gains `logo_ = parsed.logo;`. **No `logoChanged()` signal is added** — see
§4.2. This is a deliberate asymmetry versus `appearanceChanged()`/`weatherChanged()`/etc.: those
sections drive `NOTIFY`-based QML properties in their respective services; `logo()` is consulted
exactly once, synchronously, by `SystemInfoService`'s constructor, and never again.

### 2.4 `SystemInfoService` constructor + precedence chain

**Header change** (`SystemInfoService.h`):

```cpp
class ConfigService;  // forward-declared, same pattern as AppearanceService.h

...
Q_PROPERTY(QString logoSource READ logoSource CONSTANT)
Q_PROPERTY(bool logoTinted READ logoTinted CONSTANT)   // new
...
explicit SystemInfoService(ConfigService* config = nullptr, QObject* parent = nullptr);
...
[[nodiscard]] bool logoTinted() const { return logo_tinted_; }
...
private:
 void readOsRelease();
 // New: applies the [logo] config precedence (steps 1-3). Returns true if it resolved logo_source_/
 // logo_tinted_ itself, in which case readOsRelease() skips straight to distro name/display-name
 // derivation and does NOT overwrite logo_source_/logo_tinted_ with the pixmaps/icon-theme chain.
 [[nodiscard]] bool applyLogoConfigOverride(const QHash<QString, QString>& os_release);
 ...
 ConfigService* config_{nullptr};
 bool logo_tinted_{false};
```

`config` defaults to `nullptr` — see §4 rationale ("why the ConfigService* parameter is defaulted,
not mandatory like AppearanceService's").

**`.cpp` — constructor:**

```cpp
SystemInfoService::SystemInfoService(ConfigService* config, QObject* parent)
    : QObject(parent), config_(config) {
  readOsRelease();
  readAccountsService();
}
```

**`.cpp` — `readOsRelease()` restructure.** The existing function already branches on whether
`/etc/os-release` opened; both branches independently derive `name_`, `display_name_`,
`logo_icon_name_`, and `logo_source_`. The config-precedence steps (1-3) are prepended as an early
return via the new private helper `applyLogoConfigOverride()`, called with the same
`os_release` hash the existing code already parses — the two failure/success branches of the
existing `QFile::open` are otherwise untouched (REQ-C-001/REQ-C-002 constraints: `findSystemLogoPath()`
and `resolveThemeLogoIconName()` keep their exact call sites and signatures):

```cpp
void SystemInfoService::readOsRelease() {
  QFile file(QStringLiteral("/etc/os-release"));
  QHash<QString, QString> os_release;
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    os_release = parseOsRelease(stream.readAll());
  } else {
    qCWarning(lcSystemInfo) << "SystemInfoService: failed to read /etc/os-release:" << file.errorString();
  }

  const SystemInfoSnapshot snapshot = systemInfoFromOsRelease(os_release);
  name_ = snapshot.name;
  display_name_ = snapshot.display_name;
  logo_icon_name_ = resolveThemeLogoIconName(snapshot.logo_icon_name);

  // Steps 1-3 of the resolution precedence (Appendix A). Only reachable when a valid file
  // override, generic flag, or distro-table hit exists; falls through otherwise.
  if (applyLogoConfigOverride(os_release)) {
    qCInfo(lcSystemInfo) << "SystemInfoService: detected system" << name_ << "display" << display_name_
                         << "logo" << logo_source_ << "(config/alias override)";
    return;
  }

  // Steps 4-5, unchanged (REQ-C-001 / REQ-C-002).
  const QString logo_path = findSystemLogoPath(os_release, {QStringLiteral("/usr/share/pixmaps")});
  logo_source_ = logo_path.isEmpty() ? QStringLiteral("image://icon/%1").arg(logo_icon_name_)
                                     : QUrl::fromLocalFile(logo_path).toString();
  logo_tinted_ = false;

  qCInfo(lcSystemInfo) << "SystemInfoService: detected system" << name_ << "display" << display_name_ << "logo"
                       << logo_source_;
}
```

**`.cpp` — `applyLogoConfigOverride()` (new private method), implements the 3-step chain:**

```cpp
bool SystemInfoService::applyLogoConfigOverride(const QHash<QString, QString>& os_release) {
  if (config_ != nullptr) {
    const LogoConfig& logo_cfg = config_->logo();

    // Step 1: file override.
    if (!logo_cfg.file.isEmpty()) {
      const QFileInfo file_info(logo_cfg.file);
      if (file_info.exists() && file_info.isReadable()) {
        logo_source_ = QUrl::fromLocalFile(logo_cfg.file).toString();
        logo_tinted_ = false;
        return true;
      }
      qCWarning(lcConfigParsers) << "Logo file override not readable:" << logo_cfg.file
                                 << "— falling back to next resolution step";
      // fall through to step 2/3, NOT the invalid path (REQ-NF-001)
    }

    // Step 2: generic flag.
    if (logo_cfg.generic) {
      logo_source_ = QStringLiteral("qrc:/HolonightShell/linux-logo/linux.svg");
      logo_tinted_ = true;
      return true;
    }
  }

  // Step 3: distro alias table.
  const QString dist_id = os_release.value(QStringLiteral("ID")).trimmed();
  QString mapped = mapDistroIdToLogoName(dist_id);
  if (mapped.isEmpty()) {
    const QStringList id_like = os_release.value(QStringLiteral("ID_LIKE")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& token : id_like) {
      mapped = mapDistroIdToLogoName(token.trimmed());
      if (!mapped.isEmpty()) {
        break;
      }
    }
  }
  if (!mapped.isEmpty()) {
    logo_source_ = QStringLiteral("qrc:/HolonightShell/linux-logo/%1.svg").arg(mapped);
    logo_tinted_ = true;
    return true;
  }

  return false;
}
```

Notes on this design:
- `applyLogoConfigOverride()` reuses `os_release` (already parsed by the caller) rather than
  re-reading `/etc/os-release`, so the distro-table step costs nothing extra even when
  `/etc/os-release` was unreadable (`os_release` is then just an empty hash and `dist_id` is
  empty — `mapDistroIdToLogoName("")` returns `""`, falling through cleanly to step 4/5).
- `lcConfigParsers` (not `lcSystemInfo`) is used for the invalid-file warning per REQ-NF-001,
  which explicitly requires that logging category — this is a deliberate cross-module logging
  call; `SystemInfoService.cpp` already includes `<QLoggingCategory>` and needs one new
  `Q_DECLARE_LOGGING_CATEGORY(lcConfigParsers)` (or including the header that declares it) since
  the category is defined in `libs/holonight-config/src/ConfigParsers.cpp`. Exposing
  `Q_DECLARE_LOGGING_CATEGORY(lcConfigParsers)` in a small shared header (or simply forward
  declaring `Q_DECLARE_LOGGING_CATEGORY(lcConfigParsers)` locally, since `QLoggingCategory` object
  identity is by category *name string*, not by C++ symbol) is sufficient — logging categories in
  Qt are resolved by their string name at the backend, so a `Q_DECLARE_LOGGING_CATEGORY` forward
  declaration + `Q_LOGGING_CATEGORY` defined once in `ConfigParsers.cpp` is the correct, minimal
  wiring (matching how other cross-TU category reuse already works nowhere else in this codebase —
  this is the first case of it, and is called out explicitly in §6 risks below).

### 2.5 Distro alias table — `libs/holonight-core/src/SystemInfo.h`/`.cpp`

**Header addition:**

```cpp
// Maps an os-release ID (or ID_LIKE token) to a bundled assets/linux-logo/<basename>.svg name.
// Returns an empty string if id is not in the table. Distinct from findSystemLogoPath()'s
// pixmaps fuzzy matcher (REQ-C-004) — this is an explicit, exact-match static table because the
// fuzzy matcher's startsWith/contains scoring assumes the search *candidate* is no longer than the
// file basename it's compared against, which breaks for IDs like "opensuse-leap" (14 chars) being
// mapped to the asset basename "opensuse" (8 chars) — see DESIGN.md §4.1.
[[nodiscard]] QString mapDistroIdToLogoName(const QString& id);
```

**`.cpp` addition** (anonymous-namespace static table + the function, placed near the other
lookup helpers):

```cpp
namespace {
const QHash<QString, QString>& distroLogoAliasTable() {
  static const QHash<QString, QString> table{
      // 1:1 matches
      {QStringLiteral("arch"), QStringLiteral("archlinux")},
      {QStringLiteral("ubuntu"), QStringLiteral("ubuntu")},
      {QStringLiteral("debian"), QStringLiteral("debian")},
      {QStringLiteral("fedora"), QStringLiteral("fedora")},
      {QStringLiteral("centos"), QStringLiteral("centos")},
      {QStringLiteral("almalinux"), QStringLiteral("almalinux")},
      {QStringLiteral("gentoo"), QStringLiteral("gentoo")},
      {QStringLiteral("manjaro"), QStringLiteral("manjaro")},
      {QStringLiteral("endeavouros"), QStringLiteral("endeavouros")},
      {QStringLiteral("cachyos"), QStringLiteral("cachyos")},
      {QStringLiteral("linuxmint"), QStringLiteral("linuxmint")},
      {QStringLiteral("zorin"), QStringLiteral("zorin")},
      {QStringLiteral("solus"), QStringLiteral("solus")},
      {QStringLiteral("slackware"), QStringLiteral("slackware")},
      {QStringLiteral("deepin"), QStringLiteral("deepin")},
      {QStringLiteral("devuan"), QStringLiteral("devuan")},
      {QStringLiteral("elementary"), QStringLiteral("elementary")},
      {QStringLiteral("openwrt"), QStringLiteral("openwrt")},
      {QStringLiteral("tails"), QStringLiteral("tails")},
      // renamed / longer-ID aliases (the asymmetry cases the fuzzy matcher gets wrong)
      {QStringLiteral("opensuse-leap"), QStringLiteral("opensuse")},
      {QStringLiteral("opensuse-tumbleweed"), QStringLiteral("opensuse")},
      {QStringLiteral("sles"), QStringLiteral("opensuse")},        // follow-up from spec review: sles gap
      {QStringLiteral("suse"), QStringLiteral("opensuse")},        // ID_LIKE token on SLES/openSUSE derivatives
      {QStringLiteral("fedora-asahi-remix"), QStringLiteral("asahilinux")},
      {QStringLiteral("rhel"), QStringLiteral("redhat")},
      {QStringLiteral("pop"), QStringLiteral("popos")},
      {QStringLiteral("mx"), QStringLiteral("mxlinux")},
      {QStringLiteral("neon"), QStringLiteral("kdeneon")},
      {QStringLiteral("void"), QStringLiteral("voidlinux")},
      {QStringLiteral("alpine"), QStringLiteral("alpinelinux")},
      {QStringLiteral("kali"), QStringLiteral("kalilinux")},
      {QStringLiteral("rocky"), QStringLiteral("rockylinux")},
      {QStringLiteral("garuda"), QStringLiteral("garudalinux")},
      {QStringLiteral("artix"), QStringLiteral("artixlinux")},
      {QStringLiteral("parrot"), QStringLiteral("parrotsecurity")},
      {QStringLiteral("qubes"), QStringLiteral("qubesos")},
      {QStringLiteral("nobara"), QStringLiteral("nobaralinux")},
  };
  return table;
}
}  // namespace

QString mapDistroIdToLogoName(const QString& id) {
  if (id.isEmpty()) {
    return {};
  }
  return distroLogoAliasTable().value(id.trimmed().toLower());
}
```

**`sles`→`suse` gap (spec review follow-up), resolved:** the SPEC review flagged that SLES
(`ID=sles`) has no direct table row, and its `ID_LIKE` on some SUSE derivatives is `suse` (not
`opensuse`). Both `sles` and `suse` are added as direct table entries mapping to `opensuse` (the
only openSUSE-family asset bundled). This means: `ID=sles` matches immediately at step-3's first
lookup (no `ID_LIKE` walk needed); and any future distro whose `ID_LIKE` contains the bare token
`suse` (a documented real-world SUSE-family `ID_LIKE` value) also resolves correctly once the
`ID_LIKE` walk reaches it.

**`ID_LIKE` fallback strategy — iterate to the first mapped token:** `os-release(5)` defines
`ID_LIKE` as a space-separated, *ordered-by-relevance* list (per systemd's spec, "these values are
listed in order of how closely the local operating system relates to the listed ones, starting
with the closest"). `mapDistroIdToLogoName()` is called once per token in order, returning on the
first hit. This implements SPEC.md's REQ-F-003 rule to use the first mapped token. Continuing past
unmapped tokens is important because the alias table is intentionally finite: an unknown, more
specific family token may precede a known parent family. The declared ordering is still preserved,
because lookup stops at the first table hit. See §5 for the first-token-only alternative that was
rejected.

### 2.6 `SystemInfoService.logoTinted` property (new, design decision)

**Decision:** add a second `CONSTANT` `Q_PROPERTY(bool logoTinted READ logoTinted CONSTANT)`,
populated alongside `logoSource` at every resolution step (see the `logo_tinted_ = ...` assignment
next to every `logo_source_ = ...` assignment in §2.4).

This is called out explicitly as a **design decision**, not a spec gap: SPEC.md's REQ-F-006/
REQ-F-007/REQ-C-003 specify the *tinting behavior per precedence tier* but do not name a QML
property. The property name `logoTinted` and its placement on `SystemInfoService` (rather than,
say, computing tint-vs-not in QML by string-matching `logoSource`) are new interface surface
introduced by this design. Rationale in §4.3.

**Post-implementation addendum (color token):** neither SPEC.md nor this document originally
pinned a specific tint color — `HnIcon`'s default `normalColor` (`HoloniightPalette.textSecondary`)
was the implicit choice when `tinted: true`. This was later overridden explicitly in
`LogoSection.qml` to `normalColor: HoloniightPalette.accentBlue`, so the logo tints with the
palette's accent color rather than the neutral secondary-text color. This only affects the
`tinted: true` branches (bundled/distro-mapped sources); `[logo] file` overrides remain untinted
and unaffected. No REQ-F/REQ-NF changes — this is a styling refinement within REQ-F-006's existing
"palette-driven recoloring" requirement, not a new requirement.

---

## 3. CMake Asset Bundling — `apps/shell/CMakeLists.txt`

New block, inserted alongside the existing `BAR_ICON_FILES`/`bar_icons` and
`WEATHER_PNG_FILES`/`weather_png_icons` blocks (same `file(GLOB ... CONFIGURE_DEPENDS)` +
`qt6_add_resources` pairing), producing QRC paths `qrc:/HolonightShell/linux-logo/<basename>.svg`:

```cmake
file(GLOB LINUX_LOGO_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/linux-logo/*.svg"
)
qt6_add_resources(holonight-shell "linux_logo_icons"
    PREFIX "/HolonightShell"
    BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES ${LINUX_LOGO_FILES}
)
```

Placed after the existing `common_icons` block and before the pre-existing, unrelated
`logo_resource` block (`assets/logo.png` — the shell's own single application icon, used
elsewhere, e.g. window/taskbar icon; not to be confused with this feature's per-distro
`assets/linux-logo/*.svg` set). `BASE` strips `${PROJECT_SOURCE_DIR}/assets` from each glob match,
so `assets/linux-logo/fedora.svg` → alias `linux-logo/fedora.svg` → resource path
`qrc:/HolonightShell/linux-logo/fedora.svg`, matching REQ-F-013 exactly and the project's
documented CMake Asset Bundling convention (CLAUDE.md).

---

## 4. Key Decisions with Rationale

### 4.1 Explicit static alias table instead of extending the pixmaps fuzzy matcher

`findFuzzyLogoPath()`/`computeFileCandidateScore()` (`SystemInfo.cpp`) score a candidate against a
file's basename using `base_name == candidate`, `base_name.startsWith(candidate)`, and
`base_name.contains(candidate)` — all three checks are only meaningful when the **candidate is a
substring shape that can plausibly appear inside a longer basename**, i.e. they implicitly assume
`candidate.length() <= base_name.length()` for the `startsWith`/`contains` branches to have any
chance of matching a filename that also encodes extra qualifiers (`-text`, `-dark`, etc.) beyond
the bare distro name.

The QRC-bundled asset set is inverted relative to that assumption: the *config-known token* is the
long, versioned string (`opensuse-leap`, `opensuse-tumbleweed`, `fedora-asahi-remix`) and the
*file basename* is the short, canonical one (`opensuse`, `asahilinux`). `"opensuse-leap".startsWith("opensuse")`
is true in the *wrong* direction for how the matcher is invoked (it matches `base_name.startsWith(candidate)`,
i.e. it needs the **file's** basename to start with the **candidate**, not the reverse) — so
`candidate="opensuse-leap"` never matches a file literally named `opensuse.svg`, because
`"opensuse".startsWith("opensuse-leap")` is false and `"opensuse".contains("opensuse-leap")` is
false. Retrofitting the fuzzy matcher to also try the reverse containment direction would silently
change its behavior for the *existing*, working pixmaps-fallback callers (REQ-C-001 explicitly
forbids modifying that algorithm), and would still be a heuristic (subject to false positives on
unrelated files with pixmaps installed) where the new mapping needs to be exact and enumerable
(REQ-F-012's acceptance criteria list literal ID→basename pairs). A separate static
`QHash<QString, QString>` sidesteps both problems: it is O(1), exact, independently testable
(REQ-F-012's own acceptance criteria are literally "table contains these N mappings"), and leaves
the untouched pixmaps algorithm free to remain step 4 exactly as before.

### 4.2 `SystemInfoService` properties stay `CONSTANT` — no live reload

`name`, `displayName`, `avatarPath` are already `CONSTANT` in the pre-existing implementation —
this service's design precedent is "read system identity once at startup, never again," which
matches the actual dynamism of the underlying facts (a running shell process does not change which
Linux distro it's on, and a user-configured `[logo] file`/`generic` override is not expected to be
hot-swapped without a restart — NFR-003 makes this an explicit non-goal). Adding
`ConfigService::logoChanged()` + a `NOTIFY`-based `logoSource`/`logoTinted` pair would require:
(a) a new signal on `ConfigService`, (b) a `connect()` in `SystemInfoService`'s constructor, (c)
re-running the entire 5-step precedence chain (including the blocking `findSystemLogoPath()`
filesystem walk and `QIcon::hasThemeIcon()` theme lookups) on every config-file save — real cost
for a property whose value is, by design, supposed to be static per shell session. `CONSTANT`
keeps `SystemInfoService` consistent with its own existing properties and avoids that plumbing for
zero product value.

### 4.3 File override renders untinted; bundled/distro-mapped SVGs render tinted

The bundled `assets/linux-logo/*.svg` set is explicitly monochrome (`fill="currentColor"`, "KDE
ColorScheme-Text style" per SPEC.md's Feature Summary) — these are glyphs designed to be recolored,
exactly like the icon set `WeatherWidget.qml` and `ProfileButton.qml` already tint via
`HnIcon { tinted: true }`. A `[logo] file` override, by contrast, is an arbitrary user-supplied
image — could be a brand-colored PNG, a multi-color SVG, a photo. Force-tinting it via
`HnIconProvider.sourceUrl(...)`'s colorization pass (see `HnIcon.qml`'s `_renderSource` binding)
would corrupt any image that is not already a `currentColor` monochrome glyph. `tinted` is
therefore keyed to *provenance* (did this path come from the curated bundled set, or from the
user's filesystem?), not to file extension or any content inspection — provenance is exactly what
`logoTinted` (§2.6) encodes, computed at the same point in the precedence chain that already knows
which branch resolved the source.

### 4.4 qrc-bundling over reading from the source tree at runtime

Every other themed/bundled asset set in this codebase (`bar-icons`, `weather-png`, `weather-ui`,
`common`) is qrc-bundled via `qt6_add_resources`, not read from an absolute filesystem path at
runtime. Reading `assets/linux-logo/*.svg` from the source tree at runtime would only work in a
developer's build directory (relative to `PROJECT_SOURCE_DIR`, which does not exist after `make
install`); qrc-bundling makes the asset part of the compiled binary, correct after packaging/
installation, and consistent with how `LogoSection.qml`'s current `Image { source:
SystemInfoService.logoSource }` already consumes `image://icon/` and `file://` (pixmaps) sources —
adding a *third* sourcing mechanism (read-from-source-tree) alongside qrc and filesystem would be
a new, install-breaking pattern with no counterpart anywhere else in the project.

---

## 5. Alternatives Considered

| Decision | Alternative considered | Why rejected |
|---|---|---|
| Static alias table (§4.1) | Extend `computeFileCandidateScore()` to also try `candidate.startsWith(base_name)` (reverse containment) | Would change existing pixmaps-fallback matching behavior (REQ-C-001 forbids touching it byte-for-byte); still heuristic/fuzzy where an exact enumerable table is required and independently testable per REQ-F-012 |
| Static alias table (§4.1) | Rename/symlink the bundled SVG files to match every known os-release ID variant (e.g. ship both `opensuse-leap.svg` and `opensuse-tumbleweed.svg` as copies of `opensuse.svg`) | Multiplies the ~39-file asset set for no visual difference; a table row is one line vs. a duplicated binary asset, and still needs *some* code to pick the right filename in the first place |
| `CONSTANT` properties, no live reload (§4.2) | `NOTIFY`-based `logoSource`/`logoTinted`, re-resolved on `ConfigService::logoChanged()` | Re-running `findSystemLogoPath()`'s filesystem walk and `QIcon::hasThemeIcon()` calls on every config save is wasted work for a value with zero expected runtime dynamism; breaks the property-per-concern `CONSTANT` precedent already set by `name`/`displayName`/`avatarPath` on the same class |
| Tint keyed to provenance (§4.3) | Tint keyed to file extension (`.svg` bundled/distro ⇒ tint; anything else ⇒ don't) | A user could legitimately override with a monochrome `currentColor` SVG they want tinted, or a pixmaps-fallback hit could technically be `.svg`; extension does not reliably signal "was this asset designed to be recolored," provenance (which precedence step resolved it) does |
| Tint keyed to provenance (§4.3) | Content-sniff the SVG for `fill="currentColor"` and decide tinting dynamically | Adds XML parsing to a hot construction-time path for a property whose correct value is already statically known the moment the precedence chain picks a branch; more code, more failure modes, no behavior SPEC.md asks for |
| qrc-bundling (§4.4) | Read `assets/linux-logo/*.svg` from `PROJECT_SOURCE_DIR` at runtime via an absolute path baked in at configure time | Breaks after `make install` / packaging (the source tree is not guaranteed to exist on the deployed system); inconsistent with every other bundled asset set in the project |
| `ID_LIKE` — iterate to the first mapped token (§2.5) | Only check `ID_LIKE`'s first token | `ID_LIKE` is ordered by closeness, but its first token may be absent from this feature's finite alias table. Continuing in order preserves upstream priority while allowing the first known family to resolve. |
| Config parameter default (§2.4) | Require `ConfigService*` as non-optional (mirroring `AppearanceService`'s non-defaulted signature exactly) | Would force every existing `SystemInfoService service;` call site in `tests/test_system_info_service.cpp` (D-Bus timeout tests, unrelated to logo resolution) to be updated to pass a `ConfigService*`, for no behavioral benefit in those tests; defaulting to `nullptr` combined with the required null-check (REQ-F-010's own acceptance criterion) keeps those tests compiling unchanged while `ShellApplication` always passes the real instance |

---

## 6. Known Risks / Edge Cases

1. **Alias table staleness.** As new distro SVGs are added to `assets/linux-logo/`, nothing
   forces a corresponding `distroLogoAliasTable()` entry to be added — the table and the asset
   directory are two independently-maintained artifacts. Mitigation: this is not a crash risk —
   an unmapped `ID`/`ID_LIKE` simply falls through to step 4 (`findSystemLogoPath()`, unchanged)
   and then step 5 (`resolveThemeLogoIconName()`, unchanged), both of which already handle
   "distro not specifically known" gracefully (that is their entire pre-existing purpose). No
   compile-time or runtime check currently enforces table/asset-directory parity; a future test
   *could* glob `assets/linux-logo/*.svg` and assert every alias-table value has a corresponding
   file, but that is not in SPEC.md's acceptance criteria and is left as a follow-up, not part of
   this feature.

2. **`ID_LIKE` ambiguity.** A distro whose `ID_LIKE` lists multiple tokens that map to *different*
   bundled basenames (e.g. hypothetically `ID_LIKE="ubuntu debian"` where both aliased to
   different assets) resolves to whichever token is listed *first* in the distro's own
   `/etc/os-release` — which `mapDistroIdToLogoName()` treats as authoritative per systemd's
   "closest first" ordering convention (§2.5). This is inherently a best-effort heuristic on
   third-party distro metadata the project does not control; no distro in Appendix C's table
   currently exhibits this ambiguity, so it is not exercised by any acceptance criterion, but a
   new distro `ID_LIKE` value in the future could hit it. No code change mitigates this further —
   it is the correct, minimal-risk interpretation of already-ambiguous upstream data.

3. **File override checked only once, at construction.** `[logo] file` is validated
   (`QFileInfo::exists()`/`isReadable()`) exactly once, inside `SystemInfoService`'s constructor.
   A symlink that later dangles, or a file that becomes unreadable (permissions change, USB drive
   unmounted, etc.) *after* startup, is never re-checked — `logoSource` keeps pointing at a path
   that Qt's `Image`/`HnIcon` machinery will then simply fail to load (blank/broken image in the
   topbar), with no warning re-logged. This is a direct, accepted consequence of the no-live-reload
   decision (§4.2, REQ-NF-003) — re-validating on every frame or on a timer would reintroduce the
   live-reload complexity this feature deliberately opts out of. A shell restart re-runs the check
   and picks up the current filesystem state.

4. **qmllint and `HnIcon`'s cross-repo import.** `HnIcon` is a QML type provided by the sibling
   `holonight-qt` repo's `Holonight` module, not by anything in this repo's own QML import graph
   qmllint can statically resolve at lint time. Every existing `HnIcon` usage in this codebase
   (`WeatherWidget.qml`, `ProfileButton.qml`, and four other files found via
   `grep -rl "disable import unresolved-type"`) wraps its `HnIcon { ... }` block in a
   `// qmllint disable import unresolved-type` / `// qmllint enable import unresolved-type`
   comment pair to suppress the resulting false-positive lint error. `LogoSection.qml`'s new
   `HnIcon` block requires the identical disable/enable comment pair — REQ-F-004's acceptance
   criterion ("`qml-lint` reports no warnings in the logo-rendering section") is only satisfiable
   with this suppression in place, exactly matching the established pattern; omitting it would
   regress `task qml-lint`.

5. **Cross-module logging category reuse (`lcConfigParsers` invoked from `SystemInfoService.cpp`).**
   REQ-NF-001 mandates the invalid-file-override warning use the `lcConfigParsers` category
   specifically (for consistency with all other config-validation warnings), but that category is
   `Q_LOGGING_CATEGORY`-defined in `libs/holonight-config/src/ConfigParsers.cpp`, a different
   library target than `libs/holonight-services/src/SystemInfoService.cpp`. This requires either
   (a) a `Q_DECLARE_LOGGING_CATEGORY(lcConfigParsers)` forward declaration reachable from
   `SystemInfoService.cpp` (Qt logging categories resolve by their string name at the backend, so
   a forward declaration without a matching `Q_LOGGING_CATEGORY` definition in the same TU is
   valid and links fine as long as exactly one TU across the whole binary defines it — which
   `ConfigParsers.cpp` already does), or (b) exposing a small free function in
   `holonight_config` that performs the warning log itself. Option (a) is simpler and has no
   precedent-breaking cost; it is flagged here as a risk only because it is a *new pattern* in
   this codebase (no other file currently logs through a category defined in a different static
   library target) — a build-boundary edge case worth double-checking against
   `scripts/check-architecture-boundaries.sh` during implementation, since `holonight-services`
   already depends on `holonight_config` (via `ConfigService`) so this is not a *new* library
   dependency, just a new *cross-TU logging-category* usage.

---

## 7. Testing Implications

No tests are written as part of this design document; this section only maps SPEC.md's
acceptance criteria to the existing test files that will gain new cases.

### `tests/test_system_info.cpp` (pure `SystemInfo.h`/`.cpp` free-function tests, no Qt object
lifecycle needed)
- `mapDistroIdToLogoName()`: one case per Appendix C row (1:1 matches and renamed aliases), plus
  the `sles`/`suse` follow-up rows from §2.5, plus an unmapped-ID case asserting an empty string
  return (REQ-F-012).
- Confirms `mapDistroIdToLogoName()` does **not** touch `findSystemLogoPath()`/`logoCandidates()`
  behavior — i.e. existing tests for the fuzzy matcher in this file must continue passing
  unmodified (REQ-C-004).

### `tests/test_config_service.cpp`
- `parseLogo()` / round-trip through `ConfigService::logo()`: default-empty-section case (REQ-F-009
  acceptance: no `[logo]` section ⇒ `LogoConfig{}` defaults), `file` present with tilde
  (`~/my-logo.svg` expands via `expandTilde()`), `generic = true`, and both set simultaneously
  (parsing itself does not adjudicate precedence — that is `SystemInfoService`'s job — so this test
  only asserts both fields land in `LogoConfig` as given).
- `LogoConfig::operator==` sanity (defaulted, per REQ-F-008) — likely folded into the existing
  `ParsedConfig` equality-comparison tests already present in this file for other sections.

### `tests/test_system_info_service.cpp`
- New cases constructing `SystemInfoService` with a real or fake `ConfigService*` (this file
  currently only constructs `SystemInfoService service;` with the implicit `nullptr` default —
  the null-default keeps those existing D-Bus-timeout tests unaffected, per §5's rejected-
  alternative note) to cover:
  - REQ-F-001/REQ-F-002/REQ-F-003 precedence order end-to-end: valid file wins over `generic`
    (Appendix B test 5); `generic` wins over distro table when no valid file; distro table used
    when neither is set.
  - REQ-NF-001/REQ-NF-002: invalid `[logo] file` path — the constructor does not crash/hang, and
    (if this test file already has an existing pattern for asserting on `qCWarning` output — check
    for `QTest::ignoreMessage` or a log-capturing seam already used elsewhere in this file/repo
    before introducing a new one) the log content contains the invalid path and an
    "unreadable"/"invalid" phrase.
  - `logoTinted` value per branch (REQ-F-006/REQ-F-007/REQ-C-003): `true` for generic and
    distro-mapped resolutions, `false` for file override and for the pixmaps/icon-theme fallback
    paths (steps 4-5) — this is the one new assertion this feature adds to every existing
    precedence-path test, since `logoTinted` didn't exist before.
  - REQ-F-010/REQ-F-011 null-`ConfigService*` case: constructing with `nullptr` (matching the
    existing test file's current calls) must still resolve to the distro-table/pixmaps/icon-theme
    chain (steps 3-5) without touching `config_`, proving the null-guard in
    `applyLogoConfigOverride()` (§2.4) doesn't skip step 3 (distro table) — only steps 1-2 are
    inside the `config_ != nullptr` guard.

### QML smoke test (per SPEC.md Appendix D checklist item "Add QML smoke test (HnIcon rendering,
tinting behavior)")
- Given the project MEMORY notes a pre-existing "QML smoke-test singleton-registration gap" around
  `WeatherIconCompositor`, verify whatever QML test harness is used for `LogoSection.qml` (likely
  under `tests/qml/tst_*.qml`, using the `FakeQmlServices`-registered `SystemInfoService` mock) has
  `logoSource`/`logoTinted` faked with representative values so the test can assert
  `LogoSection.qml`'s `HnIcon.tinted` binding actually reflects `SystemInfoService.logoTinted`
  (REQ-F-006/REQ-F-007 rendering-level acceptance, distinct from the C++-level assertions above).
- Per this repo's `feedback_manual_testing_protocol.md` precedent, this QML smoke test is not a
  substitute for live verification of Appendix B's manual test plan — bundled logo rendering,
  visible theme-color tinting, and file-override rendering must additionally be eyeballed in a
  live Hyprland session (screenshot via `grim`, per CLAUDE.md's driving-the-shell notes) before
  this feature is considered done, matching how `HnIcon`/pointer-handler swaps have needed manual
  confirmation in this codebase before (`feedback_manual_testing_protocol.md`,
  `project_poc_remediation_phase6.md`).

---

**End of Design Document**
