add_library(holonight_app STATIC
    app/ControlCommandBuffer.h
    app/ControlServer.h
    app/ControlServer.cpp
    app/ShellApplication.h
    app/ShellApplication.cpp
    app/SettingsNavigationService.h
    app/SettingsNavigationService.cpp
)

target_link_libraries(holonight_app PUBLIC
    holonight_core
    holonight_platform
    holonight_services
    holonight_surfaces
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::DBus
    Qt6::Qml
)

target_include_directories(holonight_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
)

target_compile_features(holonight_app PUBLIC cxx_std_23)

qt6_add_executable(holonight-shell
    main.cpp
)

holonight_quick_controls_runtime(holonight-shell "${CMAKE_INSTALL_BINDIR}")

set_target_properties(holonight-shell PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
    @ONLY
)

target_include_directories(holonight-shell PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

include(${PROJECT_SOURCE_DIR}/cmake/ShellQml.cmake)
holonight_shell_qml(holonight-shell)

target_link_libraries(holonight-shell PRIVATE
    holonight_app
    holonight_componentsplugin
)

set_property(TARGET holonight_compositor holonight_core holonight_services holonight_surfaces PROPERTY INTERFACE_SOURCES
                                                                                              "")

qt6_extract_metatypes(holonight_compositor OUTPUT_FILES _HOLONIGHT_COMPOSITOR_METATYPES)
qt6_extract_metatypes(holonight_core OUTPUT_FILES _HOLONIGHT_CORE_METATYPES)
qt6_extract_metatypes(holonight_services OUTPUT_FILES _HOLONIGHT_SERVICES_METATYPES)
qt6_extract_metatypes(holonight_surfaces OUTPUT_FILES _HOLONIGHT_SURFACES_METATYPES)
# The shell registrar uses the merged library metatypes as its primary input.
# Do not also feed those linked libraries back through --foreign-types.
set_property(TARGET holonight_compositor holonight_core holonight_services holonight_surfaces PROPERTY INTERFACE_SOURCES
                                                                                              "")
set(_HOLONIGHT_LIBRARY_METATYPE_DEPS
    ${_HOLONIGHT_COMPOSITOR_METATYPES}
    ${_HOLONIGHT_CORE_METATYPES}
    ${_HOLONIGHT_SERVICES_METATYPES}
    ${_HOLONIGHT_SURFACES_METATYPES}
)
list(TRANSFORM _HOLONIGHT_LIBRARY_METATYPE_DEPS APPEND ".gen")

set(_HOLONIGHT_SHELL_METATYPES "${CMAKE_CURRENT_BINARY_DIR}/meta_types/qt6holonight-shell_metatypes.json")
add_custom_command(
    OUTPUT "${_HOLONIGHT_SHELL_METATYPES}"
    DEPENDS
        ${_HOLONIGHT_LIBRARY_METATYPE_DEPS}
        "${PROJECT_SOURCE_DIR}/scripts/collect-moc-metatypes.cmake"
    COMMAND ${CMAKE_COMMAND}
        "-DINPUT_FILES=${_HOLONIGHT_COMPOSITOR_METATYPES}|${_HOLONIGHT_CORE_METATYPES}|${_HOLONIGHT_SERVICES_METATYPES}|${_HOLONIGHT_SURFACES_METATYPES}"
        "-DOUTPUT=${_HOLONIGHT_SHELL_METATYPES}"
        -P "${PROJECT_SOURCE_DIR}/scripts/collect-moc-metatypes.cmake"
    COMMENT "Collecting HolonightShell C++ metatypes"
    VERBATIM
)
_qt_internal_assign_build_metatypes_files_and_properties(holonight-shell
    METATYPES_FILE_NAME "qt6holonight-shell_metatypes.json"
    METATYPES_FILE_PATH "${_HOLONIGHT_SHELL_METATYPES}"
)
set_target_properties(holonight-shell PROPERTIES _qt_internal_has_qmltypes TRUE)
_qt_internal_qml_type_registration(holonight-shell)

# Find runtime providers installed alongside a local-prefix shell. Never retain
# development dependency paths such as /tmp/holonight-config-prefix in the ELF.
file(RELATIVE_PATH HOLONIGHT_BIN_TO_LIB "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
set_target_properties(holonight-shell PROPERTIES INSTALL_RPATH "$ORIGIN/${HOLONIGHT_BIN_TO_LIB}")
install(TARGETS holonight-shell DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
    FILES ${PROJECT_SOURCE_DIR}/data/xdg-desktop-portal/portals/holonight.portal
    DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal/portals
)
install(
    FILES ${PROJECT_SOURCE_DIR}/data/xdg-desktop-portal/holonight-portals.conf
    DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal
)

install(
    FILES ${PROJECT_SOURCE_DIR}/data/xdg/xdg-desktop-portal/hyprland-portals.conf
    DESTINATION ${CMAKE_INSTALL_DATADIR}/holonight/xdg/xdg-desktop-portal
)
install(
    FILES ${PROJECT_SOURCE_DIR}/data/xdg/xdg-desktop-portal/sway-portals.conf
    DESTINATION ${CMAKE_INSTALL_DATADIR}/holonight/xdg/xdg-desktop-portal
)
