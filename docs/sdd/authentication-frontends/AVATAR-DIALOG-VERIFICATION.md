# Shared avatars and authentication dialog verification

## Compositor configuration

The dialog draws its own frame. Floating and centering belong to the compositor;
disable additional borders, shadows, blur, and corner clipping for these windows.
Match the reported app ID/class (`holonight-polkit-agent`, `holonight-askpass`,
`holonight-sudo-askpass`, or `holonight-ssh-askpass`, depending on the launcher).
Inspect `hyprctl clients -j` or the compositor's equivalent to confirm the actual ID.
Do not force a fixed window size: that would override work-area clamping.

Hyprland 0.55+ Lua example (the inspected session runs 0.56.2):

```lua
hl.window_rule({
    match = { class = "^holonight-(polkit-agent|askpass|sudo-askpass|ssh-askpass)$" },
    float = true,
    center = true,
    border_size = 0,
    no_blur = true,
    no_shadow = true,
    rounding = 0,
})
```

For installations still using Hyprland's 0.53/0.54 hyprlang syntax:

```ini
windowrule {
    name = holonight-authentication
    match:class = ^holonight-(polkit-agent|askpass|sudo-askpass|ssh-askpass)$
    float = on
    center = on
    border_size = 0
    no_blur = on
    no_shadow = on
    rounding = 0
}
```

These settings follow the [current Hyprland window rules](https://wiki.hypr.land/0.56.0/Configuring/Basics/Window-Rules/)
and [0.54 syntax](https://wiki.hypr.land/0.54.0/Configuring/Window-Rules/).
No personal compositor configuration was changed.

Other compositors are documentation-only until their acceptance rows are tested:

| Compositor | Decoration and placement guidance |
| --- | --- |
| Sway | `for_window [app_id="^holonight-(polkit-agent\|askpass\|sudo-askpass\|ssh-askpass)$"] floating enable, border none, move position center`. Standard Sway does not add blur or shadows. See [Sway's commands](https://github.com/swaywm/sway/blob/master/sway/sway.5.scd). |
| niri | Match `app-id` in a `window-rule`; use `open-floating true`, `default-floating-position x=0 y=0 relative-to="center"`, `border { off; }`, `focus-ring { off; }`, `shadow { off; }`, `geometry-corner-radius 0`, and `clip-to-geometry false`. Avoid background-filled decorations showing through transparent frame corners. See [niri window rules](https://niri-wm.github.io/niri/Configuration:-Window-Rules.html). |
| labwc | Add app-specific `windowRule` entries with `serverDecoration="no"`; use a `MoveTo` action with centered x/y placement if desired. Check theme shadow settings when using server decorations. See [labwc configuration](https://labwc.github.io/labwc-config.5.html) and [actions](https://labwc.github.io/labwc-actions.5.html). |
| river | Configure floating, centering, and decorations in the selected window manager. Current river delegates these policies to a separate manager, so no universal `riverctl` rule is assumed. See [river's architecture](https://github.com/riverwm/river). |

## Automated checks

The new shared-avatar tests cover asynchronous loading, empty/error sources,
fallback failure, account switching, and pixel checks for square/wide/tall and
translucent images at 56 and 132 logical pixels. The shader test explicitly skips
Qt's software renderer. Run it with a shader-capable platform, for example:

```sh
QT_QPA_PLATFORM=wayland QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=opengl \
QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE= \
../holonight-qt/build/tests/holonight_qml_smoke_tests --gtest_filter='QmlAvatar.*'
```

The focused `AuthenticationDialog` QML suite covers Continue and both Enter keys,
popup confirmation, retry and cancellation, direct account roles, large dropdowns,
local-only avatar URLs, stable geometry, request-size reset, constrained layout,
the bottom frame, body scrolling, and focused input visibility. Existing coordinator
tests continue to require explicit confirmation for multiple eligible identities.

Build/stage `holonight-qt` before either consumer. The development installation is
`/tmp/holonight-qt-prefix`; both consumers must use its CMake package and QML modules.
The greeter lint override is:

```sh
task lint QML_IMPORT_DIR=/tmp/holonight-qt-prefix/lib/qt6/qml
```

## Results recorded on 2026-09-05

| Check | Result |
| --- | --- |
| `holonight-qt`: `task test` | All 28 CTest entries passed. Software rendering skips the dedicated masking test; GPU results are recorded separately below. |
| Shared controls gallery | Built with the new 56/132-pixel avatar examples. |
| Shader pixel tests | Both avatar tests passed on Wayland/OpenGL, including fractional-scale runs. Covered loading/failure/switching, both sizes, square/wide/tall crops, ring, antialiased edges, and translucent images. |
| Shell: focused `AuthenticationDialog` suite | 23 checks passed, including setup/cleanup. |
| Shell: `dbus-run-session -- task test` | All 1,139 tests passed on an isolated session bus. |
| Shell: `task qml-lint`, `task qmltypes-check` | Passed. Final direct checks also passed; lint retains existing unresolved AudioService warnings in unrelated shell QML. Authentication-only lint is clean. |
| Greeter: `task test`, `task lint QML_IMPORT_DIR=/tmp/holonight-qt-prefix/lib/qt6/qml` | Passed, including existing controller and demo-scenario tests. |
| Staged installation | Both consumers use `/tmp/holonight-qt-prefix/lib/cmake/HolonightQt`. Greeter's runtime import trace confirms the staged Controls module; Shell's QML tests also use that prefix. |
| Live synthetic presentation | Captured selection, popup, password, and askpass at effective window ratios 1.25 and 1.5 on Hyprland 0.56.2. Repeated mock state cycles retained their configured height and new requests reapplied sizing. These are simulated requests, not real PAM attempts. |
| Greeter demo appearance | Captured and visually inspected the migrated circular avatar, ring, inset, and surrounding layout. No real login was attempted. |
| `task compositor-smoke-check` | Prerequisites/checklist ran in the live session. Shell control socket was absent; the printed shell interaction checklist was not executed. |
| Formatting and whitespace | Changed C++ passes clang-format; all three repository diffs pass `git diff --check`. |

Temporary logs and visual captures are under `/tmp/holonight-avatar-work/`.
The fractional runs use `QT_SCALE_FACTOR` relative to the active Wayland
output; effective window ratios were checked separately from `Screen.devicePixelRatio`,
which reports a rounded ratio on this session. Compositor-assigned sizes can differ
from the preferred dimensions; the captures intentionally preserve those sizes.

## Manual acceptance checklist

The interactive checks below remain outstanding.

- [ ] Repeat Polkit selection → Continue → PAM prompt → busy → failure → Retry
  at 125% and 150% Hyprland output scales. Confirm constant height within each
  request, explicit confirmation after retry, and no password field before PAM.
- [ ] Test a single identity, alternate identity, pointer activation, keyboard
  navigation, cancellation, and a fresh request following a compositor resize.
- [ ] Confirm 740 × 720 Polkit and 740 × 480 askpass preferences where space allows.
  On smaller work areas, verify 24-pixel margins, visible actions/bottom frame,
  scrolling body, focus visibility, and popup placement above/below the selector.
- [ ] Compare names, usernames, long labels, selected/hovered rows, and dropdown
  avatars. Inspect circular edges, ring, inset, transparency, and fallback imagery.
- [ ] Run Greeter **demo mode only**, switch users, and inspect missing avatars
  and unchanged 132-pixel avatar size, four-pixel inset, colors, and one-pixel ring.
  Exercise the existing wrong-password, OTP, and fingerprint demo scenarios.
- [ ] Repeat decoration/placement checks on Sway, niri, labwc, and the selected
  river window manager before claiming support verified on those compositors.

Real login sessions are excluded. The repository's UI automation guidance prohibits
automating live pointer/focus interaction; interactive acceptance requires a person.
Synthetic rendering checks and the printed compositor checklist do not substitute
for these manual steps.
