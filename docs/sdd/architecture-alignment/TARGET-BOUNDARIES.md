# Shell target boundaries

This is the allowed source and target direction at shell baseline
`f0fb0574beae8401970806c25e8242d964e70774`, with the architecture checks added by this
implementation. `scripts/check-architecture-boundaries.sh` enforces internal links, selected
service includes from surfaces, and authentication independence. It does not attempt to infer
every transitive CMake usage requirement.

| Owner | Allowed internal targets | External public contract and reason |
|---|---|---|
| `holonight_platform` | `holonight_qt_wayland_client` adapter | Qt Core/Gui/Network/DBus and Wayland client; public headers expose Qt and generated Wayland types. |
| `holonight_shell_config` | None | Qt Core and tomlplusplus; this is the installed package contract. |
| `holonight_core` | Shell config, platform | Config and Qt types occur in public headers. |
| `holonight_compositor` | Platform | Qt Core/Qml/Network types occur in public headers. |
| `holonight_services` | Core, compositor, platform | Config, appearance, theme, audio, storage, Qt, and SQL/Concurrent types occur in public headers. Libsecret is implementation-only and now private. |
| `holonight_surfaces` | Core, compositor, platform, services | Wayland and Qt Quick/Qml/DBus types occur in public headers. The services edge is for notification and MPRIS presentation orchestration. |
| `holonight_app` / shell executable | All shell libraries | Composes and registers lower-layer services and surfaces. |
| Authentication core/QML/frontends | Authentication targets only | Qt and declared authentication providers; no shell service, surface, compositor, or app dependency. |

Surfaces may directly include `NotificationService.h`, `MprisService.h`, and
`MprisArtworkCache.h` for reviewed presentation orchestration. New service includes require a
reviewed exception or a move to application composition. Lower layers may not link upward to
services, surfaces, or the application. `holonight_qt_wayland_client` is a platform adapter,
not a shell application dependency.

`libs/holonight-services/src` currently exports several feature directories because public
headers include peers from those directories by basename. The same applies to the surfaces
source directory. Narrowing those paths without first giving public headers stable include
paths would break consumers and generated metadata. No library split is justified by the
current evidence; splitting should follow a measured ownership, testability, or build-cost
problem.
