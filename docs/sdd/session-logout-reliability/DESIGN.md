# Session Logout Reliability Design

The HoloNight launcher exports `XDG_CURRENT_DESKTOP=Hyprland` before starting the shell service, so backend selection
uses that stable session declaration in addition to the later Hyprland instance signature. The Hyprland backend
checks the UWSM compositor user unit at logout time: active UWSM owns shutdown; otherwise Hyprland handles it directly.
