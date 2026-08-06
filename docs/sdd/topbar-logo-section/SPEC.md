# Topbar Logo Section – EARS Specification

**Feature Name:** Themed Distro Logo Rendering with Config Overrides
**Date:** 2026-07-22
**Status:** IMPLEMENTED — VERIFIED

---

## Feature Summary

The topbar's leftmost section (`LogoSection.qml`) currently renders the detected Linux distro logo via a plain `Image` component bound to `SystemInfoService.logoSource`. This feature:

1. Adds a curated set of ~39 monochrome (`fill="currentColor"`, KDE ColorScheme-Text style) distro-logo SVGs to `assets/linux-logo/*.svg` (one per distro: archlinux, ubuntu, fedora, debian, opensuse, popos, manjaro, etc., plus a generic `linux.svg` tux glyph).
2. Introduces a new `[logo]` configuration section in the shell config file, allowing users to override the logo via a custom file path or force the generic tux logo.
3. Replaces the plain `Image` component with `HnIcon` (from the sibling `holonight-qt` module), enabling palette-driven recoloring of bundled and distro-mapped logos.
4. Implements a static distro-ID-to-SVG-basename alias table to map real-world os-release `ID` values (and select `ID_LIKE` fallbacks) to bundled assets.

---

## Requirements

### Resolution Precedence

**REQ-F-001: File Override Support**
- **Template:** Ubiquitous (Feature-specific)
- **Requirement:** The system shall resolve the logo source to a `file:` URL created from the file specified in `[logo] file = "..."` if that path (after tilde-expansion via the existing `expandTilde()` helper) is readable and exists.
- **Acceptance Criteria:**
  - Given a config with `[logo] file = "/usr/local/share/my-logo.svg"` and that file exists and is readable, when `SystemInfoService` is constructed, then `logoSource` equals the local-file URL `file:///usr/local/share/my-logo.svg`.
  - Given a config with `[logo] file = "~/Desktop/logo.png"` and `~/Desktop/logo.png` exists and is readable, when `SystemInfoService` is constructed, then `logoSource` equals the `file:` URL produced from the tilde-expanded absolute path.

**REQ-F-002: Generic Tux Logo Fallback**
- **Template:** Conditional (If-then feature)
- **Requirement:** If `[logo] generic = true` is set in the config AND the `[logo] file` option is absent or invalid, then the system shall resolve the logo source to the bundled generic tux logo `qrc:/HolonightShell/linux-logo/linux.svg`.
- **Acceptance Criteria:**
  - Given a config with `[logo] generic = true` and no `[logo] file`, when `SystemInfoService` is constructed, then `logoSource` equals `qrc:/HolonightShell/linux-logo/linux.svg`.
  - Given a config with both `[logo] file = "/invalid/path"` (unreadable) and `[logo] generic = true`, when `SystemInfoService` is constructed, then `logoSource` equals `qrc:/HolonightShell/linux-logo/linux.svg` (not the invalid file path).

**REQ-F-003: Distro Alias Mapping**
- **Template:** Conditional (Where feature)
- **Requirement:** Where the distro alias table is consulted, the system shall first map the detected os-release `ID`; if that is not found, it shall inspect the space-separated `ID_LIKE` tokens in their declared order and use the first token present in the alias table. The mapped logo source shall be `qrc:/HolonightShell/linux-logo/<basename>.svg`.
- **Acceptance Criteria:**
  - Given os-release `ID=opensuse-leap`, when `SystemInfoService` is constructed with no `[logo]` config override, then the distro alias table maps it to `opensuse` and `logoSource` resolves to `qrc:/HolonightShell/linux-logo/opensuse.svg`.
  - Given os-release `ID=pop`, when `SystemInfoService` is constructed with no `[logo]` config override, then the distro alias table maps it to `popos` and `logoSource` resolves to `qrc:/HolonightShell/linux-logo/popos.svg`.
  - Given os-release `ID=unknown-distro` (not in the alias table) and no matching `ID_LIKE` token, when `SystemInfoService` is constructed, then control falls through to the existing pixmaps fallback (REQ-C-001).

**REQ-C-001: Preserve Existing Pixmaps Fallback**
- **Template:** Constraint
- **Requirement:** The system shall NOT modify the existing `/usr/share/pixmaps` fuzzy-matching algorithm (`findSystemLogoPath()`/`logoCandidates()` in `libs/holonight-core/src/SystemInfo.cpp`). This algorithm remains the fourth step in the resolution precedence chain, invoked only if the file override is invalid, generic is not set, and the distro ID is not in the alias table.
- **Acceptance Criteria:**
  - When `SystemInfoService` falls through past the alias table (distro unmapped), then `findSystemLogoPath()` is invoked with the distro ID.
  - The call sequence, signature, and candidate-file logic of `findSystemLogoPath()` are unchanged from the pre-feature baseline.

**REQ-C-002: Preserve Existing Icon Theme Fallback**
- **Template:** Constraint
- **Requirement:** The system shall NOT modify the existing `image://icon/...` theme-icon fallback logic (`resolveThemeLogoIconName()` in `libs/holonight-services/src/SystemInfoService.cpp`). This fallback remains the fifth and final step in the resolution precedence chain, invoked only if all prior steps fail, ultimately falling back to `computer-symbolic`.
- **Acceptance Criteria:**
  - When `SystemInfoService` exhausts all prior resolution steps, then `resolveThemeLogoIconName()` is invoked.
  - The call sequence, signature, and icon-name resolution logic of `resolveThemeLogoIconName()` are unchanged from the pre-feature baseline.

---

### Invalid File Handling

**REQ-NF-001: Graceful Invalid File Path Handling**
- **Template:** Unwanted behavior (If-then safeguard)
- **Requirement:** If `[logo] file` is set but the path (after tilde-expansion) is not readable or does not exist, then the system shall NOT crash, freeze, or produce a hard error. Instead, the system shall log a warning via the `lcConfigParsers` logging category (matching the style of `libs/holonight-config/src/ConfigParsers.cpp` validation) and silently fall through to the next resolution step.
- **Acceptance Criteria:**
  - Given a config with `[logo] file = "/nonexistent/file.svg"`, when `SystemInfoService` is constructed, then the shell does not crash.
  - A warning message is logged to `lcConfigParsers` containing the phrase "invalid" or "unreadable" and the file path.
  - Control proceeds to check `[logo] generic` (if set) or the alias table, not the invalid file path.

**REQ-NF-002: Log Message Content**
- **Template:** Ubiquitous (Logging requirement)
- **Requirement:** The system shall log a warning message that includes the invalid file path and a human-readable explanation (e.g., "Logo file override not readable: /invalid/path").
- **Acceptance Criteria:**
  - Given a config with `[logo] file = "/badpath"`, when the file is not readable, then log output contains the substring `"/badpath"` and a phrase indicating the file is not readable.

---

### Rendering Component

**REQ-F-004: Replace Image with HnIcon**
- **Template:** Ubiquitous (Component requirement)
- **Requirement:** The system shall render the logo in `LogoSection.qml` (`apps/shell/qml/Topbar/LogoSection.qml`) via an `HnIcon` component (imported from `Holonight`) instead of a plain `Image { source: SystemInfoService.logoSource }` block.
- **Acceptance Criteria:**
  - The file `apps/shell/qml/Topbar/LogoSection.qml` contains no `Image` component in the logo-rendering section; instead, an `HnIcon` component is used to display the logo.
  - The QML parses without errors and `qml-lint` reports no warnings in the logo-rendering section.

**REQ-F-005: Preserve Layout and Sizing**
- **Template:** Ubiquitous (Non-functional UI requirement)
- **Requirement:** The system shall maintain the existing size (32px), label text, tooltip, and layout of the logo section in the topbar. Only the rendering mechanism (from `Image` to `HnIcon`) changes; visual positioning and text behavior remain unchanged.
- **Acceptance Criteria:**
  - The logo dimensions remain 32×32 pixels (or the existing size value, if different).
  - The label text below the logo (if present) is unaffected.
  - The tooltip text (if any) is unaffected.
  - The layout container and spacing of the logo section are unchanged.

---

### Tinting and Color Handling

**REQ-F-006: Tint Bundled and Distro-Mapped Logos**
- **Template:** Conditional (Where feature)
- **Requirement:** Where the resolved logo source is a bundled SVG (either the generic tux logo `qrc:/HolonightShell/linux-logo/linux.svg` OR a distro-mapped `qrc:/HolonightShell/linux-logo/<basename>.svg`), the system shall render the logo via `HnIcon { tinted: true }`. These SVGs are monochrome (`fill="currentColor"`) and designed for palette-driven recoloring via the current HoloNight theme palette (`HoloniightPalette`).
- **Acceptance Criteria:**
  - When the resolved `logoSource` is `qrc:/HolonightShell/linux-logo/linux.svg`, the `HnIcon` component has `tinted: true`.
  - When the resolved `logoSource` is `qrc:/HolonightShell/linux-logo/archlinux.svg`, the `HnIcon` component has `tinted: true`.
  - The logo color responds to theme changes, matching the palette-driven rendering already used by `WeatherWidget.qml` and `ProfileButton.qml`.

**REQ-F-007: Do Not Tint User-Supplied File Overrides**
- **Template:** Conditional (Where feature)
- **Requirement:** Where the resolved logo source is a user-supplied `[logo] file` override, the system shall render the logo via `HnIcon { tinted: false }`. User-supplied images are shown as-is and must NOT be forcibly recolored, as they are likely brand-colored logos or arbitrary images not designed for palette-driven tinting.
- **Acceptance Criteria:**
  - When the resolved `logoSource` is a user file path (e.g., `/usr/local/share/logo.png`), the `HnIcon` component has `tinted: false`.
  - The user-supplied image is displayed without color modification from the theme palette.

**REQ-C-003: Preserve Existing Fallback Rendering**
- **Template:** Constraint
- **Requirement:** The system shall NOT alter the rendering treatment of the existing `/usr/share/pixmaps` and `image://icon/...` fallback paths. These sources continue to be rendered without forced tinting, matching their existing behavior (the `HnIcon` component already special-cases `image://icon/` sources to skip tinting internally).
- **Acceptance Criteria:**
  - When the resolved `logoSource` is an `image://icon/` URL, the `HnIcon` component does not apply tinting.
  - When the resolved `logoSource` is a pixmaps file path, the `HnIcon` component rendering remains visually identical to the pre-feature `Image` component rendering.

---

### Configuration Schema and Parsing

**REQ-F-008: Define LogoConfig Structure**
- **Template:** Ubiquitous (Data structure requirement)
- **Requirement:** The system shall define a new `LogoConfig` struct in `libs/holonight-config/include/holonight_config/config_structs.h` with the following fields:
  - `QString file` (empty string = no override)
  - `bool generic` (default `false`)
  - `operator==` defaulted for equality comparison
- **Acceptance Criteria:**
  - The file `libs/holonight-config/include/holonight_config/config_structs.h` contains a struct named `LogoConfig` with fields `file` and `generic`.
  - The struct compiles without errors.
  - The `operator==` is defaulted (using `= default`), not manually implemented.

**REQ-F-009: Parse Logo Configuration**
- **Template:** Ubiquitous (Config parsing requirement)
- **Requirement:** The system shall implement a new `parseLogo()` free function in `libs/holonight-config/src/ConfigParsers.cpp`, following the exact style of `parseBackground()` and `parseWeather()` functions (using `readStr`/`readBool`-style helpers, reusing the existing `expandTilde()` helper for the `file` field, and tracking optional fields). This function shall parse the `[logo]` section from the config file and populate a `LogoConfig` struct. The function shall be called from `parseConfigTable()` and the resulting `LogoConfig` shall be added to the `ParsedConfig` struct.
- **Acceptance Criteria:**
  - A `parseLogo()` function exists in `libs/holonight-config/src/ConfigParsers.cpp`.
  - The function signature and style match `parseBackground()` and `parseWeather()`.
  - `ConfigService` (`libs/holonight-core/src/ConfigService.h`/`.cpp`) exposes a const-access method `[[nodiscard]] const LogoConfig& logo() const`.
  - A config with `[logo] file = "~/my-logo.svg"` correctly parses with `file` set to the tilde-expanded path.
  - A config with `[logo] generic = true` correctly parses with `generic` set to `true`.
  - A config with no `[logo]` section results in a `LogoConfig` with default values (`file` empty, `generic` false).

---

### SystemInfoService Integration

**REQ-F-010: Accept ConfigService Parameter**
- **Template:** Ubiquitous (Constructor requirement)
- **Requirement:** The system shall modify `SystemInfoService` (`libs/holonight-services/src/SystemInfoService.h`/`.cpp`) constructor to accept a `ConfigService*` parameter, mirroring the pattern used in `AppearanceService::AppearanceService(ConfigService* config, QObject* parent)`. This parameter is consulted during `SystemInfoService` construction to apply config-based logo resolution.
- **Acceptance Criteria:**
  - The `SystemInfoService` constructor signature includes a `ConfigService*` parameter.
  - The constructor implementation stores or uses this pointer to access logo configuration.
  - The constructor does not crash if the pointer is null (handles null gracefully, possibly by skipping config checks).

**REQ-F-011: Apply Config Precedence at Construction**
- **Template:** Event-driven (Construction-time precedence)
- **Requirement:** When `SystemInfoService` is constructed, the system shall apply the config-based logo resolution (file override → generic flag → distro mapping) BEFORE falling into the existing distro-detection, pixmaps fuzzy-matching, and theme-icon fallback logic. This resolution shall occur inside `readOsRelease()` or an early initialization method.
- **Acceptance Criteria:**
  - Given a config with `[logo] file = "/custom/logo.svg"` (valid file) and os-release `ID=ubuntu`, when `SystemInfoService` is constructed, then `logoSource` is the custom file, not the ubuntu distro-mapped logo.
  - Given a config with no `[logo]` overrides and os-release `ID=fedora`, when `SystemInfoService` is constructed, then the distro mapping is consulted and `logoSource` resolves to `qrc:/HolonightShell/linux-logo/fedora.svg`.

**REQ-NF-003: No Live Reload**
- **Template:** Constraint (Non-goal enforcement)
- **Requirement:** Logo resolution occurs only once during `SystemInfoService` construction. All `logoSource`-related Q_PROPERTYs remain `CONSTANT`. Changes to the `[logo]` config file or the system's distro do NOT trigger a live reload; a shell restart is required.
- **Acceptance Criteria:**
  - The `logoSource` property is declared as `CONSTANT` (or the existing property declaration includes no NOTIFY signal).
  - Editing the config file while the shell is running and calling a hypothetical "reload" method does not change `logoSource`.
  - Only restarting the shell picks up `[logo]` config changes.

---

### Distro Alias Table

**REQ-F-012: Implement Distro Alias Lookup**
- **Template:** Ubiquitous (Data structure and lookup requirement)
- **Requirement:** The system shall implement a static distro-ID-to-SVG-basename alias table in `libs/holonight-core/src/SystemInfo.cpp`/`.h`. This table maps known os-release `ID` values (and select `ID_LIKE` fallbacks) to one of the ~39 bundled SVG basenames. The table shall handle cases where the os-release ID is LONGER than or different from the asset basename, cases that the existing pixmaps fuzzy-matching algorithm does NOT handle correctly.
- **Acceptance Criteria:**
  - A static lookup table (e.g., `QHash<QString, QString>` or equivalent) exists in `SystemInfo.cpp`.
  - The table contains mappings for at least the following IDs: `opensuse-leap`→`opensuse`, `opensuse-tumbleweed`→`opensuse`, `fedora-asahi-remix`→`asahilinux`, `rhel`→`redhat`, `pop`→`popos`, `mx`→`mxlinux`, `neon`→`kdeneon`, `void`→`voidlinux`, `alpine`→`alpinelinux`, `kali`→`kalilinux`, `rocky`→`rockylinux`, `garuda`→`garudalinux`, `artix`→`artixlinux`, `parrot`→`parrotsecurity`, `qubes`→`qubesos`, `nobara`→`nobaralinux`, and 1:1 matches (`arch`→`archlinux`, `ubuntu`→`ubuntu`, `debian`→`debian`, `fedora`→`fedora`, `centos`→`centos`, `almalinux`→`almalinux`, `gentoo`→`gentoo`, `manjaro`→`manjaro`, `endeavouros`→`endeavouros`, `cachyos`→`cachyos`, `linuxmint`→`linuxmint`, `zorin`→`zorin`, `solus`→`solus`, `slackware`→`slackware`, `deepin`→`deepin`, `devuan`→`devuan`, `elementary`→`elementary`, `openwrt`→`openwrt`, `tails`→`tails`).
  - A function (e.g., `QString mapDistroIdToLogoName(const QString& id)`) is provided to query the table and return the mapped basename or an empty string if not found.

**REQ-C-004: Do Not Reuse Pixmaps Fuzzy-Matching Algorithm**
- **Template:** Constraint
- **Requirement:** The system shall NOT reuse the existing pixmaps fuzzy-matching algorithm for distro-ID-to-SVG-basename mapping. The alias table is a separate, explicit static lookup; the pixmaps algorithm remains unchanged and is invoked only as a fallback AFTER the alias table lookup fails.
- **Acceptance Criteria:**
  - The implementation of `mapDistroIdToLogoName()` (or equivalent) uses the static alias table, not the `findSystemLogoPath()`/`logoCandidates()` logic.
  - The pixmaps algorithm is NOT called during distro alias mapping.

---

### Asset Bundling

**REQ-F-013: Bundle Logo SVGs into QRC**
- **Template:** Ubiquitous (Build requirement)
- **Requirement:** The system shall bundle all ~39 distro-logo SVG files from `assets/linux-logo/*.svg` into the shell's Qt Resource Collection (QRC) file via `qt6_add_resources` in `apps/shell/CMakeLists.txt`, using `PREFIX "/HolonightShell"` and `BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"`. This produces QRC paths of the form `qrc:/HolonightShell/linux-logo/<basename>.svg`.
- **Acceptance Criteria:**
  - `apps/shell/CMakeLists.txt` contains a `qt6_add_resources` call for logo SVGs with `PREFIX "/HolonightShell"` and `BASE` set to the assets directory.
  - After building, the QRC path `qrc:/HolonightShell/linux-logo/linux.svg` resolves to the generic tux logo.
  - After building, the QRC path `qrc:/HolonightShell/linux-logo/ubuntu.svg` resolves to the Ubuntu logo SVG.
  - The bundling follows the same pattern as existing `bar-icons` and `weather-png` bundling in the same CMakeLists.txt file.

---

## Non-Goals

The following are explicitly **NOT** part of this feature and shall NOT be implemented:

1. **No Settings UI for Logo Selection**: Users may only configure the logo via the TOML config file (`[logo]` section); no graphical settings dialog or preferences panel is provided for this feature.

2. **No Live Config Reload**: Logo configuration (file path or generic flag) is read only once during `SystemInfoService` construction. Changes to the config file require a shell restart to take effect. This is consistent with `SystemInfoService`'s existing `CONSTANT`-property design, where `name`, `displayName`, and `avatarPath` are also constant and require a restart to update.

3. **No Modification to Existing Pixmaps Fallback**: The existing `/usr/share/pixmaps` fuzzy-matching algorithm (`findSystemLogoPath()`/`logoCandidates()`) is preserved byte-for-byte. Only the distro-ID-to-SVG-basename mapping uses the new alias table; the pixmaps algorithm remains the fourth resolution step.

4. **No Modification to Existing Icon Theme Fallback**: The existing `image://icon/...` theme-icon fallback logic (`resolveThemeLogoIconName()`) is preserved unchanged and remains the fifth resolution step.

5. **No Changes to LogoSection Layout or Sizing**: The logo section's size (32px), label text, tooltip, and overall layout remain unchanged. Only the rendering mechanism (from `Image` to `HnIcon`) is updated.

6. **No Light/Dark Logo Variants**: The feature does not support providing separate logo SVGs for light and dark themes. One SVG per distro is used, and tinting via `HnIcon` adapts it to the current HoloNight theme palette. Users who desire light/dark variants must provide custom file overrides via `[logo] file`.

---

## Appendix A: Resolution Precedence Flowchart

```
1. Is [logo] file set AND valid (readable + exists)?
   YES → Use [logo] file, render with tinted: false
   NO → Go to step 2

2. Is [logo] generic = true?
   YES → Use qrc:/HolonightShell/linux-logo/linux.svg, render with tinted: true
   NO → Go to step 3

3. Is os-release ID in the distro alias table, or does ID_LIKE contain a mapped token?
   YES → Use qrc:/HolonightShell/linux-logo/<mapped>.svg, render with tinted: true
   NO → Go to step 4

4. Search /usr/share/pixmaps with findSystemLogoPath() (existing algorithm)
   FOUND → Use pixmaps file, render as-is (no tinting)
   NOT FOUND → Go to step 5

5. Resolve via resolveThemeLogoIconName() (existing algorithm, fallback to computer-symbolic)
   → Use image://icon/<name>, render as-is (no tinting)
```

---

## Appendix B: Acceptance Test Plan

1. **File Override Test**: Create a custom PNG logo, set `[logo] file = "~/my-logo.png"` in config, start shell, verify logo appears as custom image without tinting.

2. **Generic Logo Test**: Set `[logo] generic = true` in config, start shell, verify generic tux logo appears (tinted per theme).

3. **Distro Mapping Test**: On a Fedora system with no config overrides, start shell, verify Fedora logo appears (tinted per theme).

4. **Invalid File Fallthrough Test**: Set `[logo] file = "/nonexistent"`, start shell, verify a warning is logged, generic or distro-mapped logo appears as fallback.

5. **Precedence Test**: Set both `[logo] file = "/valid"` and `[logo] generic = true`, verify file override wins.

6. **Complex Distro Mapping Test**: On openSUSE Leap (ID=opensuse-leap), verify distro table maps to `opensuse` and logo resolves correctly.

7. **Unmapped Distro Fallback Test**: On a distro not in the alias table, verify fallback to pixmaps/icon-theme works without error.

8. **HnIcon Rendering Test**: Verify bundled logos render with correct theme colors, user-supplied logos render without theme tinting.

9. **No Live Reload Test**: Edit config file, verify logo does NOT change without restarting shell.

10. **Config Parsing Test**: Verify `parseLogo()` correctly parses `[logo]` section with `file`, `generic`, both, or neither set.

---

## Appendix C: Distro Alias Table Reference

| OS-Release ID | SVG Basename | Notes |
|---------------|--------------|-------|
| arch | archlinux | Direct match |
| alpine | alpinelinux | Alpine Linux |
| almalinux | almalinux | Direct match |
| artix | artixlinux | Artix Linux |
| cachyos | cachyos | Direct match |
| centos | centos | Direct match |
| deepin | deepin | Direct match |
| debian | debian | Direct match |
| devuan | devuan | Direct match |
| elementary | elementary | Direct match |
| endeavouros | endeavouros | Direct match |
| fedora | fedora | Direct match |
| fedora-asahi-remix | asahilinux | Fedora Asahi Remix |
| garuda | garudalinux | Garuda Linux |
| gentoo | gentoo | Direct match |
| kali | kalilinux | Kali Linux |
| kdeneon | kdeneon | KDE Neon (neon from ID_LIKE) |
| linuxmint | linuxmint | Linux Mint |
| manjaro | manjaro | Direct match |
| mx | mxlinux | MX Linux |
| nobara | nobaralinux | Nobara Linux |
| neon | kdeneon | KDE Neon |
| opensuse-leap | opensuse | openSUSE Leap |
| opensuse-tumbleweed | opensuse | openSUSE Tumbleweed |
| openwrt | openwrt | OpenWrt |
| parrot | parrotsecurity | Parrot Security OS |
| pop | popos | Pop!_OS |
| qubes | qubesos | Qubes OS |
| rhel | redhat | Red Hat Enterprise Linux |
| rocky | rockylinux | Rocky Linux |
| slackware | slackware | Direct match |
| solus | solus | Direct match |
| tails | tails | Direct match |
| ubuntu | ubuntu | Direct match |
| void | voidlinux | Void Linux |
| zorin | zorin | Direct match |

---

## Appendix D: Implementation Checklist

- [x] Create `assets/linux-logo/` directory with ~39 SVG files
- [x] Add `LogoConfig` struct to `libs/holonight-config/include/holonight_config/config_structs.h`
- [x] Implement `parseLogo()` in `libs/holonight-config/src/ConfigParsers.cpp`
- [x] Add `logo()` accessor to `ConfigService` (`.h` and `.cpp`)
- [x] Implement distro alias table in `libs/holonight-core/src/SystemInfo.cpp`/`.h`
- [x] Add `mapDistroIdToLogoName()` function to `SystemInfo`
- [x] Modify `SystemInfoService` constructor to accept `ConfigService*`
- [x] Update `readOsRelease()` to apply config precedence
- [x] Update `ShellApplication` to pass `config_service_` to `SystemInfoService`
- [x] Replace `Image` with `HnIcon` in `apps/shell/qml/Topbar/LogoSection.qml`
- [x] Add `qt6_add_resources` for logo SVGs in `apps/shell/CMakeLists.txt`
- [x] Add config validation test (invalid file path logging)
- [x] Add QML smoke test (HnIcon rendering, tinting behavior)
- [x] Verify `task build` succeeds
- [x] Verify `task test` passes (including new tests)
- [x] Verify `task qml-lint` passes
- [x] Verify `task qmltypes-check` passes
- [x] Run manual test plan (Appendix B)

Automated gates and live Hyprland behavior were verified on 2026-07-22. See `TASKS.md` T-016 and
T-017 for the recorded results.

---

**End of Specification**
