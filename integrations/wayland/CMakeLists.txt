add_library(holonight_backend_wayland MODULE Plugin.cpp)
target_link_libraries(holonight_backend_wayland PRIVATE holonight_ext_workspace)
configure_file(metadata.json.in metadata.json @ONLY)
target_include_directories(holonight_backend_wayland PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(holonight_backend_wayland PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/holonight/backends"
  INSTALL_RPATH "$ORIGIN/../..")
install(TARGETS holonight_backend_wayland LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/holonight/backends)

configure_file(metadata.json.in ${PROJECT_BINARY_DIR}/holonight/backends/wayland.json @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/holonight/backends/wayland.json DESTINATION ${CMAKE_INSTALL_LIBDIR}/holonight/backends)
