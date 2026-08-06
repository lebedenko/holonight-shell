add_library(holonight_app STATIC
    app/ControlCommandBuffer.h
    app/ControlServer.h
    app/ControlServer.cpp
    app/ShellApplication.h
    app/ShellApplication.cpp
)

target_link_libraries(holonight_app PUBLIC
    holonight_core
    holonight_platform
    holonight_services
    holonight_surfaces
    Qt6::Core
    Qt6::Gui
    Qt6::Network
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

file(GLOB_RECURSE HOLONIGHT_QML_FILES
    LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS
    qml/*.qml
)
list(SORT HOLONIGHT_QML_FILES)

set(HOLONIGHT_QML_TEST_ENTRIES "")
foreach(qml_file IN LISTS HOLONIGHT_QML_FILES)
  get_filename_component(qml_type "${qml_file}" NAME_WE)
  string(APPEND HOLONIGHT_QML_TEST_ENTRIES
      "    QStringLiteral(\"${qml_type} 1.0 %1\").arg(sourceUrl(QStringLiteral(\"apps/shell/${qml_file}\"))),\n")
endforeach()
configure_file(
  ${PROJECT_SOURCE_DIR}/tests/GeneratedQmlFiles.h.in
  ${PROJECT_BINARY_DIR}/GeneratedQmlFiles.h
  @ONLY
)

foreach(qml_file IN LISTS HOLONIGHT_QML_FILES)
  cmake_path(ABSOLUTE_PATH qml_file BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE qml_absolute)
  cmake_path(RELATIVE_PATH qml_absolute BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/qml" OUTPUT_VARIABLE qml_alias)
  set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS "${qml_alias}")
endforeach()

qt6_add_qml_module(holonight-shell
    URI HolonightShell
    VERSION 1.0
    RESOURCE_PREFIX "/"
    TYPEINFO holonight-shell.qmltypes
    NO_GENERATE_QMLTYPES
    NO_IMPORT_SCAN
    QML_FILES ${HOLONIGHT_QML_FILES}
)

file(GLOB WEATHER_SVG_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/weather/*.svg"
)
qt6_add_resources(holonight-shell "weather_icons"
    PREFIX "/HolonightShell"
    BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES ${WEATHER_SVG_FILES}
)

file(GLOB WEATHER_PNG_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/weather-png/512x512/*.png"
)
qt6_add_resources(holonight-shell "weather_png_icons"
    PREFIX "/HolonightShell"
    BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES ${WEATHER_PNG_FILES}
)

file(GLOB WEATHER_UI_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/weather-ui/*.svg"
)
qt6_add_resources(holonight-shell "weather_ui_assets"
    PREFIX "/HolonightShell"
    BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES ${WEATHER_UI_FILES}
)

file(GLOB BAR_ICON_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/bar-icons/*.svg"
)
qt6_add_resources(holonight-shell "bar_icons"
    PREFIX "/HolonightShell"
    BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES ${BAR_ICON_FILES}
)

file(GLOB COMMON_ICON_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/common/*.svg"
)
qt6_add_resources(holonight-shell "common_icons"
    PREFIX "/HolonightShell"
    BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES ${COMMON_ICON_FILES}
)

file(GLOB LINUX_LOGO_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/linux-logo/*.svg"
)
qt6_add_resources(holonight-shell "linux_logo_icons"
    PREFIX "/HolonightShell"
    BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES ${LINUX_LOGO_FILES}
)

qt6_add_resources(holonight-shell "logo_resource"
    PREFIX "/HolonightShell"
    BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES
        "${PROJECT_SOURCE_DIR}/assets/holonight-logo.svg"
        "${PROJECT_SOURCE_DIR}/assets/holonight-shell.svg"
        "${PROJECT_SOURCE_DIR}/assets/logo.png"
)

qt6_add_resources(holonight-shell "media_assets"
    PREFIX "/HolonightShell"
    BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES
        "${PROJECT_SOURCE_DIR}/assets/media/artwork-fallback.svg"
        "${PROJECT_SOURCE_DIR}/assets/media/artwork-frame.svg"
        "${PROJECT_SOURCE_DIR}/assets/media/media-placeholder.svg"
        "${PROJECT_SOURCE_DIR}/assets/media/now-playing-glyph.svg"
)

target_link_libraries(holonight-shell PRIVATE
    holonight_app
    holonight_componentsplugin
)

set_property(TARGET holonight_core holonight_services holonight_surfaces PROPERTY INTERFACE_SOURCES "")

qt6_extract_metatypes(holonight_core OUTPUT_FILES _HOLONIGHT_CORE_METATYPES)
qt6_extract_metatypes(holonight_services OUTPUT_FILES _HOLONIGHT_SERVICES_METATYPES)
qt6_extract_metatypes(holonight_surfaces OUTPUT_FILES _HOLONIGHT_SURFACES_METATYPES)
# The shell registrar uses the merged library metatypes as its primary input.
# Do not also feed those linked libraries back through --foreign-types.
set_property(TARGET holonight_core holonight_services holonight_surfaces PROPERTY INTERFACE_SOURCES "")
set(_HOLONIGHT_LIBRARY_METATYPE_DEPS
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
        "-DINPUT_FILES=${_HOLONIGHT_CORE_METATYPES}|${_HOLONIGHT_SERVICES_METATYPES}|${_HOLONIGHT_SURFACES_METATYPES}"
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

install(TARGETS holonight-shell DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS ${PROJECT_SOURCE_DIR}/scripts/holonight-hyprland-session DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS ${PROJECT_SOURCE_DIR}/scripts/holonight-shell-systemd DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
    FILES ${PROJECT_SOURCE_DIR}/data/wayland-sessions/holonight-hyprland.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions
)
install(
    FILES ${PROJECT_SOURCE_DIR}/data/systemd/user/holonight-shell.service
    DESTINATION ${CMAKE_INSTALL_DATADIR}/systemd/user
)
install(
    FILES ${PROJECT_SOURCE_DIR}/data/xdg-desktop-portal/portals/holonight.portal
    DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal/portals
)
install(
    FILES ${PROJECT_SOURCE_DIR}/data/xdg-desktop-portal/holonight-portals.conf
    DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal
)
