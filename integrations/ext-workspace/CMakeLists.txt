file(GLOB sources CONFIGURE_DEPENDS src/*.h src/*.cpp)
# Neutral ext-workspace protocol implementation shared by runtime plugins.
add_library(holonight_ext_workspace OBJECT ${sources})
set_target_properties(holonight_ext_workspace PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(holonight_ext_workspace PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(holonight_ext_workspace PUBLIC holonight_integration Qt6::Network Qt6::Qml Qt6::Gui holonight_qt_wayland_client ${WAYLAND_CLIENT_LIBRARIES})
qt6_generate_wayland_protocol_client_sources(holonight_ext_workspace
  FILES ${PROJECT_SOURCE_DIR}/protocols/ext-workspace-v1.xml)
target_include_directories(holonight_ext_workspace PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

