add_library(holonight_compositor STATIC
    src/CompositorSelection.h
    src/CompositorSelection.cpp
    src/CompositorSnapshot.h
    src/CompositorWorkspaceModel.h
    src/CompositorWorkspaceModel.cpp
    src/CompositorService.h
    src/CompositorService.cpp
    src/SwayIpc.h
    src/SwayIpc.cpp
)

target_link_libraries(holonight_compositor PUBLIC Qt6::Core Qt6::Qml Qt6::Network)
target_include_directories(holonight_compositor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(holonight_compositor PUBLIC cxx_std_23)
