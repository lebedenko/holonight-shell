file(GLOB sources CONFIGURE_DEPENDS src/*.h src/*.cpp)
# The object target is private to this integration and its tests.
add_library(holonight_wayland_implementation OBJECT ${sources})
set_target_properties(holonight_wayland_implementation PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(holonight_wayland_implementation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(holonight_wayland_implementation PUBLIC holonight_integration Qt6::Network Qt6::Qml Qt6::Gui holonight_qt_wayland_client ${WAYLAND_CLIENT_LIBRARIES})
add_library(holonight_backend_wayland MODULE Plugin.cpp)
target_link_libraries(holonight_backend_wayland PRIVATE holonight_wayland_implementation)
configure_file(metadata.json.in metadata.json @ONLY)
target_include_directories(holonight_backend_wayland PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(holonight_backend_wayland PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/holonight/backends"
  INSTALL_RPATH "$ORIGIN/../..")
install(TARGETS holonight_backend_wayland LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/holonight/backends)

qt6_generate_wayland_protocol_client_sources(holonight_wayland_implementation
  FILES ${PROJECT_SOURCE_DIR}/protocols/ext-workspace-v1.xml)
target_include_directories(holonight_wayland_implementation PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

configure_file(metadata.json.in ${PROJECT_BINARY_DIR}/holonight/backends/wayland.json @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/holonight/backends/wayland.json DESTINATION ${CMAKE_INSTALL_LIBDIR}/holonight/backends)
