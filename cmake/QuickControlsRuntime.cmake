# Private executable startup support; keep build discovery out of installed launches.
get_filename_component(HOLONIGHT_PROVIDER_PREFIX "${HolonightQt_DIR}/../../.." ABSOLUTE)
find_path(HOLONIGHT_RUNTIME_QML_PATH NAMES Holonight/qmldir
  PATHS "${HOLONIGHT_PROVIDER_PREFIX}/lib/qt6/qml" "${HOLONIGHT_PROVIDER_PREFIX}/lib64/qt6/qml"
  NO_DEFAULT_PATH REQUIRED)

function(holonight_quick_controls_runtime target install_directory)
  file(RELATIVE_PATH runtime_qml "${CMAKE_INSTALL_PREFIX}/${install_directory}"
    "${CMAKE_INSTALL_FULL_LIBDIR}/qt6/qml")
  file(RELATIVE_PATH runtime_lib "${CMAKE_INSTALL_PREFIX}/${install_directory}"
    "${CMAKE_INSTALL_FULL_LIBDIR}")
  target_sources(${target} PRIVATE "${PROJECT_SOURCE_DIR}/libs/holonight-core/src/QuickControlsRuntime.cpp")
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/libs/holonight-core/src")
  # The dynamically imported provider needs Config even when the executable has
  # no direct C++ calls into it. Keep a direct ELF dependency so executable-relative
  # native discovery also works for authentication frontends.
  target_link_libraries(${target} PRIVATE Qt6::QuickControls2
    "-Wl,--no-as-needed" HoloNight::Config "-Wl,--as-needed")
  target_compile_definitions(${target} PRIVATE
    HOLONIGHT_RUNTIME_BUILD_EXECUTABLE="$<TARGET_FILE:${target}>"
    HOLONIGHT_RUNTIME_QML_PATH="${HOLONIGHT_RUNTIME_QML_PATH}"
    HOLONIGHT_RUNTIME_INSTALL_QML="${runtime_qml}")
  set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN/${runtime_lib}")
  qt6_add_resources(${target} controls_config PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES "${PROJECT_SOURCE_DIR}/resources/qtquickcontrols2.conf")
endfunction()
