file(GLOB sources CONFIGURE_DEPENDS src/*.h src/*.cpp)
# The object target is private to this integration and its tests.
add_library(holonight_labwc_implementation OBJECT ${sources})
set_target_properties(holonight_labwc_implementation PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(holonight_labwc_implementation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(holonight_labwc_implementation PUBLIC holonight_integration holonight_ext_workspace)
add_library(holonight_backend_labwc MODULE Plugin.cpp)
target_link_libraries(holonight_backend_labwc PRIVATE holonight_labwc_implementation holonight_ext_workspace)
configure_file(metadata.json.in metadata.json @ONLY)
target_include_directories(holonight_backend_labwc PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(holonight_backend_labwc PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/holonight/backends"
  INSTALL_RPATH "$ORIGIN/../..")
install(TARGETS holonight_backend_labwc LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/holonight/backends)

configure_file(metadata.json.in ${PROJECT_BINARY_DIR}/holonight/backends/labwc.json @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/holonight/backends/labwc.json DESTINATION ${CMAKE_INSTALL_LIBDIR}/holonight/backends)

qt6_generate_wayland_protocol_client_sources(holonight_labwc_implementation
  FILES ${PROJECT_SOURCE_DIR}/protocols/wlr-foreign-toplevel-management-unstable-v1.xml)
target_include_directories(holonight_labwc_implementation PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
