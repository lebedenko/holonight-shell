# System Appearance Portal Tasks

- [x] Implement in-process `org.freedesktop.impl.portal.Settings` backend.
- [x] Map theme scheme IDs to portal `color-scheme`.
- [x] Map theme accent IDs to portal `accent-color`.
- [x] Emit `SettingChanged` only for changed system-facing values.
- [x] Install HoloNight portal routing files.
- [x] Export `XDG_CURRENT_DESKTOP=HoloNight:Hyprland` from the session bootstrap.
- [x] Extend desktop integration smoke check to verify HoloNight portal files and bus owner.
- [ ] Restart/start `xdg-desktop-portal` after installing portal files during manual validation.
