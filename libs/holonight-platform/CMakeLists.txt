add_library(holonight_platform STATIC
    src/LayerShell.h
    src/LayerSurface.h
    src/LayerSurface.cpp
    src/HyprlandIpc.h
    src/HyprlandIpc.cpp
    src/HyprlandIpcClient.h
    src/HyprlandIpcClient.cpp
    src/DbusPropertyClient.h
    src/DbusPropertyClient.cpp
)

qt6_generate_wayland_protocol_client_sources(holonight_platform
    FILES
        ${PROJECT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml
        ${PROJECT_SOURCE_DIR}/protocols/ext-workspace-v1.xml
        ${PROJECT_SOURCE_DIR}/protocols/ext-idle-notify-v1.xml
        ${XDG_SHELL_PROTOCOL}
)

target_link_libraries(holonight_platform PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::DBus
    holonight_qt_wayland_client
    ${WAYLAND_CLIENT_LIBRARIES}
)

target_compile_features(holonight_platform PUBLIC cxx_std_23)

target_include_directories(holonight_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)
