# Shell Overlay Focus Policy

HoloNight treats interactive shell overlays as transient, modal UI. When one is open, it owns user interaction until it is dismissed or the user's focus moves somewhere else.

## Surface Classes

Persistent shell surfaces stay mapped and are not dismissed by focus changes:

- Top bar
- Wallpaper/background
- Desktop widgets
- Notification toasts

Transient shell surfaces close when attention moves away:

- Right sidebar
- Status popups
- Session popup
- Tray menus
- Launcher
- Tooltips

## Dismissal Rules

Transient shell surfaces should close when any of these happen:

- The visible workspace changes on any monitor.
- Another mutually exclusive shell overlay opens.
- The user clicks outside the overlay, where the overlay provides a dismiss surface.
- The user presses Escape, where the overlay provides keyboard handling.

Mouse movement alone should not dismiss overlays. In multi-monitor setups, use Hyprland's visible-workspace changes rather than raw focused-monitor changes, so focus-follows-mouse setups do not dismiss overlays when the pointer merely enters another monitor.

## Keyboard Ownership

Interactive overlays that handle keyboard input should request exclusive layer-shell keyboard interactivity while open. This prevents key presses from leaking into application windows behind the overlay and makes Escape dismissal reliable.

Non-interactive surfaces should not request keyboard focus.
