# Architecture alignment implementation record

Status: In progress, 2026-09-24. Shell baseline: `f0fb0574beae8401970806c25e8242d964e70774`.
Provider revisions used for local preparation:
`holonight-config` `fe69a59e6b73167fd5349223a4d265d75386c139`, `holonight-qt`
`863af4183bdf09ce05199b37e8f5dfb46a311ba1`, and `holonight-system-services`
`5f2ecda7eea653995f4c860bfb7f3a3f53beb279`. The compiler is GCC 16.2.1,
Qt is 6.11.2, and CMake is 4.4.3.

## Implemented locally

- Debug, test, and coverage use `build/`, `build-tests/`, and `build-coverage/`. The root
  `compile_commands.json` follows the debug build; test and coverage databases stay in their own
  directories. Release and stage paths are separate.
- `tidy-container` now builds pinned config, Qt, and system-services providers inside the
  container into a container-specific prefix before configuring the shell. Fresh CMake detection
  avoids reusing a host compiler/Qt cache.
- Coverage instruments the shell, compositor, platform, core, services, surfaces, shell config,
  authentication core and frontends, and local QML C++ modules. The report uses isolated CTest,
  includes shell-owned code in those targets, and retains the 45% line and 30% branch gates.
  Generated build output and dependency provider sources are outside the source filters. The
  shell entry point remains explicitly excluded because it is exercised by runtime launch checks
  rather than unit assertions.
- Generated QML metadata now includes all 33 runtime singleton registrations, including
  `SettingsNavigationService.openPage(QString)`, `RecentAppsTracker`, and `SidebarManager`.
  The checker verifies module/version, singleton semantics, and the navigation method signature.
  Runtime factories and object ownership are unchanged.
- The network switch and profile selector use standard Controls with explicit accessible roles,
  names, and checked state. Focus treatment and keyboard-visible profile captions were added.
  Ordinary text changed in the network toggle and battery panel uses shared typography roles.
  The battery percentage and percent sign retain deliberate display proportions but now scale
  with the shared display size (1.4375 and 0.6875 times its configured value respectively).
- The remaining network popup labels now use shared typography roles in the popup content,
  connection card, Wi-Fi list/delegate, and password dialog. Existing elision, alignment, color,
  and emphasis bindings remain on those labels. This removes 14 numeric font assignments from
  five components without changing the network service requests.
- The audio popup's application, device, stream, master-volume, and unavailable-state text now
  uses shared typography roles. Percentage fields grow to their text's implicit width, and the
  default-device pill grows with its label. Fifteen numeric font assignments were removed from
  eight components. The stream options ellipsis is an icon glyph and retains its 14-point size;
  it is recorded in the exception ledger below.
- The weather details grid and wind summary now use shared roles for labels, values, units,
  direction, and gusts. Their rows and enclosing popup section grow with text metrics. The wind
  speed keeps its display-font proportion at 1.3125 times the configured display size. This
  removes seven numeric font assignments from two components.
- The five daily forecast card labels now use shared typography roles. Their temperature slot,
  card row, and enclosing forecast section grow with the configured UI font size and label
  metrics. Card labels stay within their columns and elide when needed. This removes five
  numeric font assignments from the daily cards.
- The remaining weather popup headings, current-condition labels, hourly cards, detail rows,
  and footer now use shared typography roles. The hero temperature and unit retain their
  original display proportions through the configured display size; the AQI number remains
  sized to fit its gauge. The current, hourly, forecast, graph, and footer sections grow with
  text metrics, while constrained labels elide within their columns. This removes the final
  17 fixed numeric font assignments from the weather popup.
- The authentication title, subtitle, request text, identity names, metadata, messages, and
  action captions now use shared typography roles. The response field uses the shared
  subheading size; identity rows, the field, and action buttons grow with font metrics. This
  removes 15 numeric font assignments from the four authentication components. The
  constrained identity-popup test now waits for its resized window content before measuring
  popup bounds, as the other authentication resize test already does.
- Notification toast text, toast action labels, and notification sidebar headings/status/help
  now use shared roles. The toast action height grows with its label; the sidebar DND text
  wraps within its row. The sidebar's bell and warning glyphs follow theme sizes, while the
  toast body retains styled notification markup. The relative-time label remains bound to a
  replaced notification and refreshes each minute. This removes 11 fixed numeric font
  assignments from three components.
- Quick Settings inhibitor names/reasons, section heading, charge-limit value, and Keep Awake
  caption now use shared typography roles. The existing elision and layout constraints remain,
  and the caption is translatable. This removes all five fixed numeric font assignments from
  the Quick Settings tab.
- The remaining right-sidebar profile, menu, overview calendar, event, notification, system,
  and placeholder text now uses shared typography roles. The calendar cells, navigation
  targets, notification badge/overflow row, profile, event time column, status messages,
  and system buttons respond to text metrics. This removes 34 fixed numeric font assignments
  from seven components; no right-sidebar component retains a fixed numeric font assignment.
- Launcher headings, selection details, actions, and shortcut hints; topbar date and weather
  condition; tray labels; OSD readouts; and popup title/description now use shared typography
  roles. The launcher search prompt and audio/tray glyphs follow theme sizes, while the action
  row, tray menu row, and audio glyph slot grow with their content. This removes the final 24
  fixed numeric shell QML font assignments from 12 components.
- The `MicroHeader` role defaults to the title font. Headings that previously used the UI font
  now explicitly retain that family in the launcher, audio/network/battery/weather popups, and
  authentication subtitle. The system diagnostic command uses the caption role to retain its
  original UI family. Headings previously bound to the title font keep that family.
- The authentication overflow test now waits for the window content item to receive a requested
  resize before checking action positions. At 1.5 scale, the test previously read the dialog's
  new 400-pixel width while its content item still had the prior 485-pixel width. No production
  authentication layout changed.
- The unused `ThemeService.paletteReloadRequested` signal was removed. The service still projects
  portal settings, and `AppearanceReloadBridge` still handles per-engine palette reloads.
- Appearance integration tests now verify invalid-file recovery and deletion followed by atomic
  replacement. The pinned provider preserves the last good appearance for invalid input, but
  projects defaults when the file is missing; both paths advance to the restored configuration.
- `AudioState` had no production caller. Its obsolete source and test were removed after checking
  the pinned system-services provider's controller/backend implementations and tests for percent
  clamping and rounding, channel counts, and mute state. Shell `AudioService` still derives from
  the provider's `AudioController`.
- The architecture check now enforces allowed target edges and runs negative fixtures for target
  edges, surface service includes, and authentication-to-shell dependencies. The reviewed surface
  notification/MPRIS exceptions and platform Wayland client adapter remain allowed. Libsecret
  linkage and headers are private to services because no service header includes libsecret.

## Checks completed

| Command | Result |
|---|---|
| `task configure-tests` | Passed; configured `build-tests/`. |
| `cmake --build build-tests --target qml-lint -j8` | Passed with existing external Components metadata warnings. |
| `cmake --build build-tests --target holonight-shell -j8` | Passed. |
| `scripts/check-qmltypes.sh build-tests` | Passed, including 33 runtime singleton checks. |
| Focused `tst_ArchitectureControls.qml` under isolated HoloNight and Fusion | Four cases passed in each style. |
| `task architecture-check` | Passed with six negative/positive fixtures. |
| `task test` (prior implementation pass) | Passed: 1,169 of 1,169 CTest cases. |
| `task qml-lint` | Passed; inherited provider `AudioService` properties remain unresolved warnings. |
| `task qmltypes-check` | Passed in the independent debug build. |
| `task compositor-smoke-check` | Printed the live Hyprland checklist; environment detected, but the monitor query warned and no manual visual steps were performed. |
| Provider `holonight_system_audio_tests --gtest_brief=1` | Passed: 84 of 84 at the pinned provider revision. |
| `task tidy-container` | Failed at the explicit image preflight: `polkit-qt6-agent-1` and `polkit-qt6-core-1` development packages are missing. Provider builds were not attempted. |
| `task coverage` | Passed: 1,169 isolated CTest cases; 12,657/17,876 lines (70.8%) and 13,434/33,566 branches (40.0%). The existing 45%/30% gates pass. |
| `task test` after network typography conversion | Passed: 1,169 of 1,169 CTest cases. A final one-line role correction in the password dialog was then rebuilt and checked with the focused QML tests below. |
| Focused CTest QML harness and compiled controls after final role correction | Passed: 5 of 5, including HoloNight and Fusion compiled controls at scales 1.0 and 1.25. |
| `cmake --build build --target qml-lint` after final role correction | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| `scripts/check-qmltypes.sh build` after rebuilding shell | Passed: 33 runtime singletons and module packaging. |
| `ctest --test-dir build-tests -R 'AppearanceIntegrationTest' --output-on-failure` | Passed: 9 of 9, including invalid and missing file recovery. |
| `ctest --test-dir build-tests -R '^test_holonight_qml_harness$' --output-on-failure` after audio typography conversion | Passed: 1 of 1. |
| `task qml-lint` after audio typography conversion | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| `task qmltypes-check` after audio typography conversion | Passed: 33 runtime singletons, module packaging, and authentication metadata. |
| `task compositor-smoke-check` after audio typography conversion | Printed the live checklist; Hyprland and the shell socket were detected, monitor query warned, and no manual visual steps were performed. |
| `task test` after audio typography conversion | Passed: 1,170 of 1,170 CTest cases, including the QML harness and launch smoke test. |
| Focused `test_holonight_qml_harness` after weather details typography conversion | Passed: 1 of 1. |
| `task qml-lint` after weather details typography conversion | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| `task qmltypes-check` after weather details typography conversion | Passed: 33 runtime singletons, module packaging, and authentication metadata. |
| `task compositor-smoke-check` after weather details typography conversion | Printed the live checklist; Hyprland and the shell socket were detected, monitor query warned, and no manual visual steps were performed. |
| `task test` after weather details typography conversion | Passed: 1,170 of 1,170 CTest cases. |
| `QT_SCALE_FACTOR=1.25` focused QML harness | Passed: 1 of 1. |
| `QT_SCALE_FACTOR=1.5` focused QML harness | Failed: 297 passed, 1 failed in `AuthenticationDialog.test_overflowKeepsActionsAccessible` at `tests/qml/tst_AuthenticationDialog.qml:524`; this is outside the edited weather components. |
| `QT_SCALE_FACTOR=1.5` QML harness with `-input tests/qml/tst_Phase12PopupResilience.qml` | Passed: 6 of 6 weather and brightness popup cases. |
| Authentication test with `QT_SCALE_FACTOR=1.5` after resize wait | Passed: 26 of 26 cases. |
| Full QML harness after authentication test correction at scales 1.0, 1.25, and 1.5 | Passed: 1 of 1 CTest test at each scale. |
| `task qml-lint` after authentication test correction | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| Focused `test_holonight_qml_harness` after daily forecast typography conversion | Passed: 1 of 1. |
| `task qml-lint` after daily forecast typography conversion | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| `task qmltypes-check` after daily forecast typography conversion | Passed: 33 runtime singletons, module packaging, and authentication metadata. |
| `QT_SCALE_FACTOR=1.5` focused QML harness after daily forecast typography conversion | Passed: 1 of 1. |
| `task compositor-smoke-check` after daily forecast typography conversion | Printed the live checklist; Hyprland and the shell socket were detected, monitor query warned, and no manual visual steps were performed. |
| `task test` after final daily forecast label bounds | Passed: 1,170 of 1,170 CTest cases, including the QML harness and launch smoke test. |
| Focused `test_holonight_qml_harness` after the complete weather popup typography pass | Passed at scales 1.0, 1.25, and 1.5: 1 of 1 CTest test each. |
| `task qml-lint` after the complete weather popup typography pass | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| `task qmltypes-check` after the complete weather popup typography pass | Passed: 33 runtime singletons, module packaging, and authentication metadata. |
| `task compositor-smoke-check` after the complete weather popup typography pass | Printed the live checklist; Hyprland and the shell socket were detected, monitor query warned, and no manual visual steps were performed. |
| `task test` after the complete weather popup typography pass | Passed: 1,170 of 1,170 CTest cases, including the QML harness and launch smoke test. |
| User weather popup visual review (2026-09-25) | No issues reported; exact font settings and display scales were not recorded. |
| Focused compiled authentication tests after typography conversion | Passed: 4 of 4 across HoloNight and Fusion at scales 1.0 and 1.25. |
| Authentication QML cases at scale 1.5 after resize wait | Passed: 26 of 26. |
| Full QML harness at scale 1.5 after authentication test correction | Passed: 1 of 1 CTest test. |
| `task qml-lint` after authentication typography conversion | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| `task qmltypes-check` after authentication typography conversion | Passed: 33 runtime singletons, module packaging, and authentication metadata. |
| `task test` after authentication typography conversion and resize-test correction | Passed: 1,170 of 1,170 CTest cases, including the QML harness and launch smoke test. |
| Focused notification toast tests | Passed at scales 1.0 and 1.5: supported bold markup remains, inline images are removed, and relative time tracks a replaced notification. |
| Full QML harness after notification typography conversion | Passed at scales 1.0, 1.25, and 1.5: 1 of 1 CTest test each. |
| `task qml-lint` after notification typography conversion | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| `task qmltypes-check` after notification typography conversion | Passed: 33 runtime singletons, module packaging, and authentication metadata. |
| `task compositor-smoke-check` after notification typography conversion | Printed the live checklist; Hyprland and the shell socket were detected, monitor query warned, and no manual visual steps were performed. |
| `task test` after final notification typography and time-binding changes | Passed: 1,170 of 1,170 CTest cases, including the QML harness and launch smoke test. |
| Focused right-sidebar QML harness after Quick Settings typography conversion | Passed: 6 of 6 cases. |
| Full QML harness after Quick Settings typography conversion at scales 1.25 and 1.5 | Passed: 1 of 1 CTest test at each scale. |
| `task qml-lint` after Quick Settings typography conversion | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| `task qmltypes-check` after Quick Settings typography conversion | Passed: 33 runtime singletons, module packaging, and authentication metadata. |
| `task compositor-smoke-check` after Quick Settings typography conversion | Printed the live checklist; Hyprland and the shell socket were detected, monitor query warned, and no manual visual steps were performed. |
| `task test` after Quick Settings typography conversion | Passed: 1,170 of 1,170 CTest cases, including the QML harness and launch smoke test. |
| Focused sidebar component-instantiation and shared-controls QML cases after completing the sidebar | Passed: 14 of 14 and 6 of 6. |
| Compiled sidebar controls after completing the sidebar | Passed under HoloNight and Fusion at the 1.25-scale CTest configuration. |
| Full QML harness after completing the sidebar at scale 1.5 | Passed: 1 of 1 CTest test. |
| `task qml-lint` after completing the sidebar | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| `task qmltypes-check` after completing the sidebar | Passed: 33 runtime singletons, module packaging, and authentication metadata. |
| `task compositor-smoke-check` after completing the sidebar | Printed the live checklist; Hyprland and the shell socket were detected, monitor query warned, and no manual visual steps were performed. |
| `task test` after completing the sidebar | Passed: 1,170 of 1,170 CTest cases, including the QML harness and launch smoke test. |
| Focused launcher, OSD, and tooltip QML cases after the final shell typography pass | Passed: 7 of 7, 16 of 16, and 5 of 5. |
| Compiled controls after the final shell typography pass | Passed under HoloNight and Fusion at scale 1.25. |
| Full QML harness after the final shell typography pass at scales 1.25 and 1.5 | Passed: 1 of 1 CTest test at each scale. |
| `task qml-lint` after the final shell typography pass | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| `task qmltypes-check` after the final shell typography pass | Passed: 33 runtime singletons, module packaging, and authentication metadata. |
| `task compositor-smoke-check` after the final shell typography pass | Printed the live checklist; Hyprland and the shell socket were detected, monitor query warned, and no manual visual steps were performed. |
| `task test` after the final shell typography pass | Passed: 1,170 of 1,170 CTest cases, including the QML harness and launch smoke test. |
| User manual visual checks (2026-09-25) | Passed as reported; exact font settings, scales, and engine coverage were not recorded. |
| Focused launcher font-family regression cases | Passed: 7 of 7, including UI-family checks for the browse and search headings. |
| Full QML harness after font-family correction at scale 1.5 | Passed: 1 of 1 CTest test. |
| `task qml-lint` after font-family correction | Passed with the previously recorded inherited `AudioService` metadata warnings. |
| `task qmltypes-check` after font-family correction | Passed: 33 runtime singletons, module packaging, and authentication metadata. |
| `task compositor-smoke-check` after font-family correction | Printed the live checklist; Hyprland and the shell socket were detected, monitor query warned, and no manual visual steps were performed by the script. |
| `task test` after font-family correction | Passed: 1,170 of 1,170 CTest cases, including the QML harness and launch smoke test. |

The coverage compile database contains `--coverage` for compositor, shell-config, and
authentication production objects. The HTML report lists source entries for all three, plus
shell application and services. Its denominator now includes the newly covered targets, so it
should not be compared directly with older percentages without this target inventory.

## Remaining acceptance work

- Verify appearance reload and typography geometry in two live engines plus a later engine,
  including invalid/missing configuration recovery at the palette, typography, and shape
  bindings, and scales 1.0, 1.25, and 1.5. The service-level recovery checks above do not
  establish cross-engine QML propagation. The shell QML fixed-size inventory now has no
  literal numeric `font.pointSize` or `font.pixelSize` assignments. The authentication frontend
  also has no fixed numeric font assignments. The user reports that manual checks passed;
  exact font settings, fractional scales, engine coverage, and long-name scenarios were not
  recorded, so the specified cross-engine acceptance evidence remains open.

  Reviewed retained shell exceptions so far:

  | Component | Numeric size | Reason / follow-up |
  |---|---|---|
  | `Popups/Weather/WeatherAqiGauge.qml` | Gauge-derived size, 9 pt input floor | The single AQI numeral is fitted to the 48 px gauge interior. Its font family follows the configured monospace family; review legibility at fractional scales. |

  The disabled audio-stream ellipsis and tray submenu arrow now use theme sizes; review their
  alignment at fractional scales. The launcher prompt glyph retains its display-font proportion.
  The battery percentage and percent sign, and the weather wind speed, hero temperature,
  and hero unit use proportional `AppearanceService.displayFontSize` bindings rather than
  fixed literals. The user checked the weather popup and reported no issues; its exact font
  settings and display scales were not recorded.
- Execute `task tidy-container` against an image with all shell build dependencies. The local
  image has Qt 6.11.1 while the host uses 6.11.2, confirming that the host-built provider prefix
  cannot be reused. It also lacks `polkit-qt6-agent-1`; the task now fails early with that
  requirement, pending the shared CI image owner.
- Complete the public Qt registration API feasibility prototype and a clean acceptance build.
  Perform the compositor and assistive-technology visual checks in an appropriate live session.
- Complete inherited provider metadata for `AudioService`: `qml-lint` still reports unresolved
  `AudioController` properties despite the shell singleton export and passing runtime tests.
  A local Qt 6.11.2 prototype generated the provider header's moc JSON and appended it to
  qmltyperegistrar's `--foreign-types` input. Registration succeeded, but the generated
  `AudioService` prototype remained unresolved and no `AudioController` component appeared.
  Raw, unannotated provider moc JSON alone does not close this metadata gap; the provider
  contract or a deliberate shell-side foreign-type wrapper needs a separate validated change.

## Public Qt registration feasibility

Qt 6.11.2 provides `qt_generate_foreign_qml_types(source_target, destination_qml_target)` as a
public CMake command. It can collect annotated types from the shell libraries into a QML module,
but its documented behavior generates QML integration structs. The same Qt documentation says
custom wrappers are needed for singleton instances with application-owned lifetimes; simply
using generated registration for the current `ShellApplication` objects would risk duplicate
registration and different engine identities. The shell currently declares no minimum Qt
version that guarantees this API. The public migration is therefore deferred until a separate
prototype covers the supported Qt versions, runtime factories, multiple engines, resource
layout, and provider-derived interfaces. The current registration machinery remains in place.

Reference: [Qt `qt_generate_foreign_qml_types` documentation](https://doc.qt.io/qt-6/qt-generate-foreign-qml-types.html).

## Deferred MPRIS decoding investigation

At `holonight-images` revision `3da5f4e51fe2eed9a1bd9f72c0ab523aa57a5ceb`,
`HolonightImages::decode` accepts an open seekable `QIODevice`, explicit byte/pixel/output limits,
a 512-pixel bound, an orientation policy, and cooperative cancellation; it returns an image and
structured outcome. That overlaps the worker-thread `QImageReader` portion of
`MprisArtworkCache`, while the shell still owns URL handling, 5 MiB input cap, PNG persistence,
cache budget, scheduling, and callbacks. The provider defaults to applying orientation, which
would need a deliberate compatibility choice because the current cache uses `QImageReader`
directly. Adoption is deferred until package/version, format coverage, exact limits, threading,
error mapping, and output behavior are settled under the shared image initiative. No dependency
was added here.
