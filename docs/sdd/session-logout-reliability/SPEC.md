# Session Logout Reliability Specification

- UWSM-managed HoloNight sessions shall log out through `uwsm stop`.
- Direct Hyprland sessions shall log out through `hyprctl dispatch exit`.
- Backend selection shall work when the shell starts before Hyprland publishes its instance signature.
- Command launch failures shall continue to surface through `commandFailed`.
