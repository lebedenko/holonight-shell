add_library(holonight_compositor STATIC
    src/CompositorWorkspaceModel.h src/CompositorWorkspaceModel.cpp
    src/CompositorService.h src/CompositorService.cpp
    src/WorkspacePresentation.h src/WorkspacePresentation.cpp
    src/IntegrationLoader.h src/IntegrationLoader.cpp
)
target_link_libraries(holonight_compositor PUBLIC holonight_integration Qt6::Core Qt6::Qml)
target_include_directories(holonight_compositor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(holonight_compositor PUBLIC cxx_std_23)
file(RELATIVE_PATH BIN_TO_BACKENDS "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}/holonight/backends")
target_compile_definitions(holonight_compositor PRIVATE HOLONIGHT_BACKEND_RELATIVE_PATH="${BIN_TO_BACKENDS}")
