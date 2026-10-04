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
    holonight_presentationplugin
)

set_property(TARGET holonight_compositor holonight_core holonight_services holonight_surfaces PROPERTY INTERFACE_SOURCES
                                                                                              "")

# Provider metadata is absent from the installed static Audio target. Extract only
# metadata from its public headers; never compile another copy of provider code.
get_target_property(_HOLONIGHT_AUDIO_INCLUDE_DIRS HoloNightSystem::Audio INTERFACE_INCLUDE_DIRECTORIES)
find_path(_HOLONIGHT_AUDIO_HEADER_DIR AudioController.h
    PATHS ${_HOLONIGHT_AUDIO_INCLUDE_DIRS} NO_DEFAULT_PATH NO_CACHE REQUIRED)
set(_HOLONIGHT_AUDIO_MOC_JSON)
foreach(_audio_type IN ITEMS AudioController AudioDeviceModel AudioStreamModel)
  set(_audio_moc "${CMAKE_CURRENT_BINARY_DIR}/audio-meta/moc_${_audio_type}.cpp")
  add_custom_command(
      OUTPUT "${_audio_moc}.json"
      BYPRODUCTS "${_audio_moc}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/audio-meta"
      COMMAND Qt6::moc --output-json -o "${_audio_moc}" "${_HOLONIGHT_AUDIO_HEADER_DIR}/${_audio_type}.h"
      DEPENDS "${_HOLONIGHT_AUDIO_HEADER_DIR}/${_audio_type}.h" Qt6::moc
      VERBATIM
  )
  list(APPEND _HOLONIGHT_AUDIO_MOC_JSON "${_audio_moc}.json")
endforeach()
set(_HOLONIGHT_AUDIO_METATYPES "${CMAKE_CURRENT_BINARY_DIR}/audio-meta/metatypes.json")
add_custom_command(
    OUTPUT "${_HOLONIGHT_AUDIO_METATYPES}"
    COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/scripts/collect-audio-metatypes.py"
        "${_HOLONIGHT_AUDIO_METATYPES}" ${_HOLONIGHT_AUDIO_MOC_JSON}
    DEPENDS ${_HOLONIGHT_AUDIO_MOC_JSON} "${PROJECT_SOURCE_DIR}/scripts/collect-audio-metatypes.py"
    VERBATIM
)

qt6_extract_metatypes(holonight_compositor OUTPUT_FILES _HOLONIGHT_COMPOSITOR_METATYPES)
qt6_extract_metatypes(holonight_core OUTPUT_FILES _HOLONIGHT_CORE_METATYPES)
qt6_extract_metatypes(holonight_services OUTPUT_FILES _HOLONIGHT_SERVICES_METATYPES)
qt6_extract_metatypes(holonight_surfaces OUTPUT_FILES _HOLONIGHT_SURFACES_METATYPES)
qt6_extract_metatypes(holonight_app OUTPUT_FILES _HOLONIGHT_APP_METATYPES)
# The shell registrar uses the merged library metatypes as its primary input.
# Do not also feed those linked libraries back through --foreign-types.
set_property(TARGET holonight_compositor holonight_core holonight_services holonight_surfaces PROPERTY INTERFACE_SOURCES
                                                                                              "")
set(_HOLONIGHT_LIBRARY_METATYPE_DEPS
    ${_HOLONIGHT_COMPOSITOR_METATYPES}
    ${_HOLONIGHT_CORE_METATYPES}
    ${_HOLONIGHT_SERVICES_METATYPES}
    ${_HOLONIGHT_SURFACES_METATYPES}
    ${_HOLONIGHT_APP_METATYPES}
)
list(TRANSFORM _HOLONIGHT_LIBRARY_METATYPE_DEPS APPEND ".gen")
list(APPEND _HOLONIGHT_LIBRARY_METATYPE_DEPS "${_HOLONIGHT_AUDIO_METATYPES}")

set(_HOLONIGHT_SHELL_METATYPES "${CMAKE_CURRENT_BINARY_DIR}/meta_types/qt6holonight-shell_metatypes.json")
add_custom_command(
    OUTPUT "${_HOLONIGHT_SHELL_METATYPES}"
    DEPENDS
        ${_HOLONIGHT_LIBRARY_METATYPE_DEPS}
        "${PROJECT_SOURCE_DIR}/scripts/collect-moc-metatypes.cmake"
    COMMAND ${CMAKE_COMMAND}
        "-DINPUT_FILES=${_HOLONIGHT_COMPOSITOR_METATYPES}|${_HOLONIGHT_CORE_METATYPES}|${_HOLONIGHT_SERVICES_METATYPES}|${_HOLONIGHT_SURFACES_METATYPES}|${_HOLONIGHT_APP_METATYPES}|${_HOLONIGHT_AUDIO_METATYPES}"
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
          ${PROJECT_SOURCE_DIR}/data/xdg/xdg-desktop-portal/labwc-portals.conf
    DESTINATION ${CMAKE_INSTALL_DATADIR}/holonight/xdg/xdg-desktop-portal
)
