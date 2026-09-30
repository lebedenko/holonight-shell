file(GLOB sources CONFIGURE_DEPENDS src/*.h src/*.cpp)
# The object target is private to this integration and its tests.
add_library(holonight_sway_implementation OBJECT ${sources})
set_target_properties(holonight_sway_implementation PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(holonight_sway_implementation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(holonight_sway_implementation PUBLIC holonight_integration Qt6::Network Qt6::Qml)
add_library(holonight_backend_sway MODULE Plugin.cpp)
target_link_libraries(holonight_backend_sway PRIVATE holonight_sway_implementation)
configure_file(metadata.json.in metadata.json @ONLY)
target_include_directories(holonight_backend_sway PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(holonight_backend_sway PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/holonight/backends"
  INSTALL_RPATH "$ORIGIN/../..")
install(TARGETS holonight_backend_sway LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/holonight/backends)

configure_file(metadata.json.in ${PROJECT_BINARY_DIR}/holonight/backends/sway.json @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/holonight/backends/sway.json DESTINATION ${CMAKE_INSTALL_LIBDIR}/holonight/backends)
