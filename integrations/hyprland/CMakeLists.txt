file(GLOB sources CONFIGURE_DEPENDS src/*.h src/*.cpp)
# The object target is private to this integration and its tests.
add_library(holonight_hyprland_implementation OBJECT ${sources})
set_target_properties(holonight_hyprland_implementation PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(holonight_hyprland_implementation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(holonight_hyprland_implementation PUBLIC holonight_integration Qt6::Network Qt6::Qml)
add_library(holonight_backend_hyprland MODULE Plugin.cpp)
target_link_libraries(holonight_backend_hyprland PRIVATE holonight_hyprland_implementation)
configure_file(metadata.json.in metadata.json @ONLY)
target_include_directories(holonight_backend_hyprland PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(holonight_backend_hyprland PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/holonight/backends"
  INSTALL_RPATH "$ORIGIN/../..")
install(TARGETS holonight_backend_hyprland LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/holonight/backends)

set_source_files_properties(qml/SpecialWorkspaces.qml PROPERTIES QT_RESOURCE_ALIAS SpecialWorkspaces.qml)
set_source_files_properties(qml/SpecialWorkspaceDot.qml PROPERTIES QT_RESOURCE_ALIAS SpecialWorkspaceDot.qml)
set_source_files_properties(qml/qmldir PROPERTIES QT_RESOURCE_ALIAS qmldir)
set_source_files_properties(qml/special-ws.svg PROPERTIES QT_RESOURCE_ALIAS special-ws.svg)
qt6_add_resources(holonight_backend_hyprland "hyprland_widgets"
  PREFIX /HolonightShell/Integrations/Hyprland
  FILES qml/SpecialWorkspaces.qml qml/SpecialWorkspaceDot.qml qml/qmldir qml/special-ws.svg)

configure_file(metadata.json.in ${PROJECT_BINARY_DIR}/holonight/backends/hyprland.json @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/holonight/backends/hyprland.json DESTINATION ${CMAKE_INSTALL_LIBDIR}/holonight/backends)
