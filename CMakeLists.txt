cmake_minimum_required(VERSION 3.25)
project(holonight-shell VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

# The shell loads QML modules dynamically from the runtime Qt installation. Some
# distro packages provide QML runtime plugins without matching CMake plugin
# targets, so avoid auto-linking QML plugins during executable finalization.
set(QT_SKIP_AUTO_QML_PLUGIN_INCLUSION ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml DBus Network Concurrent Sql QuickControls2)
find_package(HolonightQt REQUIRED COMPONENTS Wayland)
find_package(HoloNightSystemServices REQUIRED)
find_package(Qt6 COMPONENTS GuiPrivate WaylandClient WaylandClientPrivate QUIET)
find_package(Qt6WaylandScannerTools REQUIRED)
if(NOT COMMAND qt6_generate_wayland_protocol_client_sources)
  include("${Qt6WaylandScannerTools_DIR}/Qt6WaylandClientMacros.cmake" OPTIONAL)
endif()
find_package(WaylandScanner REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QT6_WAYLAND_CLIENT REQUIRED Qt6WaylandClient)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_check_modules(TOMLPLUSPLUS REQUIRED IMPORTED_TARGET tomlplusplus)
pkg_check_modules(LIBSECRET REQUIRED libsecret-1)

if(NOT COMMAND qt6_generate_wayland_protocol_client_sources)
  function(qt6_generate_wayland_protocol_client_sources target)
    cmake_parse_arguments(arg
        "NO_INCLUDE_CORE_ONLY;PRIVATE_CODE;PUBLIC_CODE"
        "__QT_INTERNAL_WAYLAND_INCLUDE_DIR"
        "FILES"
        ${ARGN}
    )

    if(DEFINED arg_UNPARSED_ARGUMENTS)
      message(FATAL_ERROR "Unknown arguments were passed to qt6_generate_wayland_protocol_client_sources: (${arg_UNPARSED_ARGUMENTS}).")
    endif()

    if(NOT TARGET Wayland::Scanner)
      message(FATAL_ERROR "Wayland::Scanner target not found. You might be missing the WaylandScanner CMake package.")
    endif()

    if(NOT TARGET Qt6::qtwaylandscanner)
      message(FATAL_ERROR "Qt6::qtwaylandscanner target not found. You might be missing the Qt Wayland scanner tools package.")
    endif()

    get_target_property(target_binary_dir ${target} BINARY_DIR)
    string(TOUPPER "${target}" module_define_infix)
    string(REPLACE "-" "_" module_define_infix "${module_define_infix}")
    string(REPLACE "." "_" module_define_infix "${module_define_infix}")
    set(build_macro "QT_BUILD_${module_define_infix}_LIB")

    foreach(protocol_file IN LISTS arg_FILES)
      get_filename_component(protocol_name "${protocol_file}" NAME_WLE)

      set(waylandscanner_header_output "${target_binary_dir}/wayland-${protocol_name}-client-protocol.h")
      set(waylandscanner_code_output "${target_binary_dir}/wayland-${protocol_name}-protocol.c")
      set(qtwaylandscanner_header_output "${target_binary_dir}/qwayland-${protocol_name}.h")
      set(qtwaylandscanner_code_output "${target_binary_dir}/qwayland-${protocol_name}.cpp")

      if(NOT arg_NO_INCLUDE_CORE_ONLY)
        set(waylandscanner_extra_args "--include-core-only")
      endif()

      if(arg_PRIVATE_CODE)
        set(wayland_scanner_code_option "private-code")
      else()
        set(wayland_scanner_code_option "public-code")
      endif()

      add_custom_command(
          OUTPUT "${waylandscanner_header_output}"
          COMMAND Wayland::Scanner ${waylandscanner_extra_args} client-header < "${protocol_file}" > "${waylandscanner_header_output}"
          DEPENDS "${protocol_file}" Wayland::Scanner
      )

      add_custom_command(
          OUTPUT "${waylandscanner_code_output}"
          COMMAND Wayland::Scanner ${waylandscanner_extra_args} ${wayland_scanner_code_option} < "${protocol_file}" > "${waylandscanner_code_output}"
          DEPENDS "${protocol_file}" Wayland::Scanner
      )

      set(wayland_include_dir "")
      if(arg___QT_INTERNAL_WAYLAND_INCLUDE_DIR)
        set(wayland_include_dir "${arg___QT_INTERNAL_WAYLAND_INCLUDE_DIR}")
      endif()

      add_custom_command(
          OUTPUT "${qtwaylandscanner_header_output}"
          COMMAND Qt6::qtwaylandscanner client-header
              "${protocol_file}"
              "--build-macro=${build_macro}"
              "--header-path=${wayland_include_dir}"
              > "${qtwaylandscanner_header_output}"
          DEPENDS "${protocol_file}" Qt6::qtwaylandscanner
      )

      add_custom_command(
          OUTPUT "${qtwaylandscanner_code_output}"
          COMMAND Qt6::qtwaylandscanner client-code
              "${protocol_file}"
              "--build-macro=${build_macro}"
              "--header-path=${wayland_include_dir}"
              --add-include=
              > "${qtwaylandscanner_code_output}"
          DEPENDS "${protocol_file}" Qt6::qtwaylandscanner
      )

      set(sources
          "${waylandscanner_header_output}"
          "${waylandscanner_code_output}"
          "${qtwaylandscanner_header_output}"
          "${qtwaylandscanner_code_output}"
      )

      target_sources(${target} PRIVATE ${sources})
      set_source_files_properties(${sources} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    endforeach()

    target_include_directories(${target} PRIVATE "${target_binary_dir}")
  endfunction()
endif()

add_library(holonight_qt_gui_private INTERFACE)
if(TARGET Qt6::GuiPrivate)
  target_link_libraries(holonight_qt_gui_private INTERFACE Qt6::GuiPrivate)
else()
  get_target_property(_qt_gui_include_dirs Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
  get_target_property(_qt_core_include_dirs Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)
  foreach(_qt_gui_include_dir IN LISTS _qt_gui_include_dirs)
    if(EXISTS "${_qt_gui_include_dir}/${Qt6_VERSION}/QtGui/qpa/qplatformwindow_p.h")
      target_include_directories(holonight_qt_gui_private INTERFACE
          "${_qt_gui_include_dir}/${Qt6_VERSION}"
          "${_qt_gui_include_dir}/${Qt6_VERSION}/QtGui"
      )
      set(_holonight_found_qt_gui_private_headers TRUE)
      break()
    endif()
  endforeach()
  foreach(_qt_core_include_dir IN LISTS _qt_core_include_dirs)
    if(EXISTS "${_qt_core_include_dir}/${Qt6_VERSION}/QtCore/private/qglobal_p.h")
      target_include_directories(holonight_qt_gui_private INTERFACE
          "${_qt_core_include_dir}/${Qt6_VERSION}"
          "${_qt_core_include_dir}/${Qt6_VERSION}/QtCore"
      )
      set(_holonight_found_qt_core_private_headers TRUE)
      break()
    endif()
  endforeach()
  if(NOT _holonight_found_qt_gui_private_headers OR NOT _holonight_found_qt_core_private_headers)
    message(FATAL_ERROR "Qt Gui private headers were not found. Install the Qt private development package.")
  endif()
endif()

add_library(holonight_qt_wayland_client INTERFACE)
if(TARGET Qt6::WaylandClient)
  target_link_libraries(holonight_qt_wayland_client INTERFACE Qt6::WaylandClient)
else()
  target_include_directories(holonight_qt_wayland_client INTERFACE ${QT6_WAYLAND_CLIENT_INCLUDE_DIRS})
  target_compile_options(holonight_qt_wayland_client INTERFACE ${QT6_WAYLAND_CLIENT_CFLAGS_OTHER})
  target_link_directories(holonight_qt_wayland_client INTERFACE ${QT6_WAYLAND_CLIENT_LIBRARY_DIRS})
  target_link_libraries(holonight_qt_wayland_client INTERFACE ${QT6_WAYLAND_CLIENT_LIBRARIES})
endif()

pkg_get_variable(WAYLAND_PROTOCOLS_PKGDATADIR wayland-protocols pkgdatadir)
cmake_path(SET WAYLAND_PROTOCOLS_PKGDATADIR NORMALIZE "${WAYLAND_PROTOCOLS_PKGDATADIR}")
set(XDG_SHELL_PROTOCOL "${WAYLAND_PROTOCOLS_PKGDATADIR}/stable/xdg-shell/xdg-shell.xml")
if(NOT EXISTS "${XDG_SHELL_PROTOCOL}")
  message(FATAL_ERROR "xdg-shell protocol XML not found: ${XDG_SHELL_PROTOCOL}")
endif()

# ============================================================
# Libraries and apps
# ============================================================
# Generate qmldir redirectors for QML files in module subdirectories.
qt_policy(SET QTP0004 NEW)

add_subdirectory(libs/holonight-platform)
add_subdirectory(libs/holonight-compositor)
add_subdirectory(libs/holonight-shell-config)
add_subdirectory(libs/holonight-core)
add_subdirectory(libs/holonight-services)
add_subdirectory(libs/holonight-surfaces)
add_subdirectory(qml/HoloNight)
add_subdirectory(apps/shell)

# ============================================================
# Tests
# ============================================================
option(BUILD_TESTS "Build unit tests (requires network access at configure time)" OFF)
option(ENABLE_COVERAGE "Instrument for gcov/gcovr coverage reporting" OFF)

if(DEFINED BUILD_TESTS AND
   NOT "${BUILD_TESTS}" STREQUAL "ON" AND
   NOT "${BUILD_TESTS}" STREQUAL "OFF" AND
   NOT "${BUILD_TESTS}" STREQUAL "1" AND
   NOT "${BUILD_TESTS}" STREQUAL "0" AND
   NOT "${BUILD_TESTS}" STREQUAL "TRUE" AND
   NOT "${BUILD_TESTS}" STREQUAL "FALSE" AND
   NOT "${BUILD_TESTS}" STREQUAL "YES" AND
   NOT "${BUILD_TESTS}" STREQUAL "NO")
  message(FATAL_ERROR "BUILD_TESTS must be ON or OFF, got: '${BUILD_TESTS}'")
endif()

if(BUILD_TESTS)
  enable_testing()

  find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)

  find_package(GTest CONFIG QUIET)
  if(NOT GTest_FOUND OR NOT TARGET GTest::gtest OR NOT TARGET GTest::gmock)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL      https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
      URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  if(ENABLE_COVERAGE)
    find_program(GCOVR gcovr REQUIRED)
    add_custom_target(clean-coverage-counters
      COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcda" -delete
      COMMENT "Cleaning stale gcov counters"
      VERBATIM
    )
    foreach(lib IN ITEMS holonight_platform holonight_core holonight_services holonight_surfaces holonight_app)
      target_compile_options(${lib} PRIVATE --coverage)
      target_link_options(${lib} PRIVATE --coverage)
    endforeach()
    target_compile_options(holonight-shell PRIVATE --coverage)
    target_link_options(holonight-shell PRIVATE --coverage)
  endif()

  add_subdirectory(tests)
endif()

# ============================================================
# Format / Tidy / qmllint targets
# ============================================================
find_program(CLANG_FORMAT clang-format)
find_program(CLANG_TIDY clang-tidy)

find_program(RUN_CLANG_TIDY run-clang-tidy)
find_program(QMLLINT qmllint
    HINTS
        "${Qt6_DIR}/../../../lib/qt6/bin"
        "/usr/lib/qt6/bin"
)

file(GLOB_RECURSE ALL_SOURCE_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    apps/*.cpp apps/*.h libs/*.cpp libs/*.h tests/*.cpp tests/*.h
)

file(GLOB_RECURSE ALL_CXX_SRC_IMPL_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    apps/*.cpp libs/*.cpp
)

file(GLOB_RECURSE ALL_CXX_TEST_IMPL_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    tests/*.cpp
)
list(REMOVE_ITEM ALL_CXX_TEST_IMPL_FILES "${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp")

set(ALL_CXX_IMPL_FILES ${ALL_CXX_SRC_IMPL_FILES} ${ALL_CXX_TEST_IMPL_FILES})

file(GLOB_RECURSE ALL_QML_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    apps/shell/qml/*.qml
    qml/HoloNight/*.qml
)

if(CLANG_FORMAT)
  add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Running clang-format"
        VERBATIM
    )
  add_custom_target(format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${ALL_SOURCE_FILES}
        COMMENT "Checking clang-format"
        VERBATIM
    )
endif()

if(CLANG_TIDY)
  include(ProcessorCount)
  ProcessorCount(TIDY_JOBS)
  if(TIDY_JOBS EQUAL 0)
    set(TIDY_JOBS 1)
  endif()

  # run-clang-tidy gained -hide-progress in LLVM 20. On older toolchains
  # (e.g. Debian's clang-tidy) the flag is unknown, and run-clang-tidy's
  # argparse latches onto the leading "-h", prints --help, and exits 0 —
  # silently skipping every file while the build target reports success.
  # Only pass the flag when the installed run-clang-tidy advertises it.
  set(TIDY_HIDE_PROGRESS_FLAG "")
  if(RUN_CLANG_TIDY)
    execute_process(
      COMMAND ${RUN_CLANG_TIDY} --help
      OUTPUT_VARIABLE RUN_CLANG_TIDY_HELP
      ERROR_VARIABLE RUN_CLANG_TIDY_HELP
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(RUN_CLANG_TIDY_HELP MATCHES "-hide-progress")
      set(TIDY_HIDE_PROGRESS_FLAG "-hide-progress")
    endif()
  endif()

  set(TIDY_DB_DIR ${CMAKE_BINARY_DIR}/tidy)
  set(STRIP_TIDY_FLAGS_SCRIPT ${CMAKE_BINARY_DIR}/strip_tidy_flags.cmake)
  set(TIDY_GENERATED_FILES
      ${CMAKE_CURRENT_BINARY_DIR}/libs/holonight-platform/qwayland-wlr-layer-shell-unstable-v1.h
      ${CMAKE_CURRENT_BINARY_DIR}/libs/holonight-platform/wayland-wlr-layer-shell-unstable-v1-client-protocol.h
      ${CMAKE_CURRENT_BINARY_DIR}/libs/holonight-platform/qwayland-ext-workspace-v1.h
      ${CMAKE_CURRENT_BINARY_DIR}/libs/holonight-platform/wayland-ext-workspace-v1-client-protocol.h
      ${CMAKE_CURRENT_BINARY_DIR}/libs/holonight-platform/qwayland-xdg-shell.h
      ${CMAKE_CURRENT_BINARY_DIR}/libs/holonight-platform/wayland-xdg-shell-client-protocol.h
      ${CMAKE_CURRENT_BINARY_DIR}/libs/holonight-platform/qwayland-ext-idle-notify-v1.h
      ${CMAKE_CURRENT_BINARY_DIR}/libs/holonight-platform/wayland-ext-idle-notify-v1-client-protocol.h
      ${CMAKE_CURRENT_BINARY_DIR}/libs/holonight-surfaces/holonight_surfaces_autogen/timestamp
  )
  if(BUILD_TESTS)
    list(APPEND TIDY_GENERATED_FILES
        ${CMAKE_CURRENT_BINARY_DIR}/tests/test_holonight_platform_autogen/timestamp
        ${CMAKE_CURRENT_BINARY_DIR}/tests/test_holonight_core_autogen/timestamp
        ${CMAKE_CURRENT_BINARY_DIR}/tests/test_holonight_services_autogen/timestamp
        ${CMAKE_CURRENT_BINARY_DIR}/tests/test_holonight_surfaces_autogen/timestamp
        ${CMAKE_CURRENT_BINARY_DIR}/tests/test_holonight_app_autogen/timestamp
        ${CMAKE_CURRENT_BINARY_DIR}/tests/test_holonight_qml_autogen/timestamp
        ${CMAKE_CURRENT_BINARY_DIR}/tests/test_holonight_qml_harness_autogen/timestamp
    )
  endif()
  file(MAKE_DIRECTORY ${TIDY_DB_DIR})
  file(WRITE ${STRIP_TIDY_FLAGS_SCRIPT}
        "file(READ \"${CMAKE_BINARY_DIR}/compile_commands.json\" db)\n"
        "string(REPLACE \"-mno-direct-extern-access\" \"\" db \"\${db}\")\n"
        "file(WRITE \"${TIDY_DB_DIR}/compile_commands.json\" \"\${db}\")\n"
    )

  function(add_holonight_tidy_target target_name target_sources)
    if(RUN_CLANG_TIDY)
      add_custom_target(${target_name}
            COMMAND ${CMAKE_COMMAND} -P ${STRIP_TIDY_FLAGS_SCRIPT}
            COMMAND ${RUN_CLANG_TIDY}
                -quiet
                ${TIDY_HIDE_PROGRESS_FLAG}
                -j ${TIDY_JOBS}
                -p ${TIDY_DB_DIR}
                ${${target_sources}}
            DEPENDS ${TIDY_GENERATED_FILES}
            COMMENT "Running ${target_name}"
            VERBATIM
        )
    else()
      add_custom_target(${target_name}
            COMMAND ${CMAKE_COMMAND} -P ${STRIP_TIDY_FLAGS_SCRIPT}
            COMMAND ${CLANG_TIDY} --quiet -p ${TIDY_DB_DIR} ${${target_sources}}
            DEPENDS ${TIDY_GENERATED_FILES}
            COMMENT "Running ${target_name}"
            VERBATIM
        )
    endif()
  endfunction()

  add_holonight_tidy_target(tidy-src ALL_CXX_SRC_IMPL_FILES)
  add_holonight_tidy_target(tidy-tests ALL_CXX_TEST_IMPL_FILES)
  add_holonight_tidy_target(tidy ALL_CXX_IMPL_FILES)

  if(NOT RUN_CLANG_TIDY)
    message(STATUS "run-clang-tidy was not found; tidy targets will use clang-tidy sequentially")
  endif()
endif()

if(QMLLINT)
  add_custom_target(qml-lint
        COMMAND ${QMLLINT}
            --unqualified disable
            --unused-imports disable
            -I "${CMAKE_CURRENT_SOURCE_DIR}/tests/qmllint"
            -I "${CMAKE_CURRENT_BINARY_DIR}/apps/shell"
            -I "${CMAKE_BINARY_DIR}"
            -I "/tmp/holonight-qt-prefix/lib/qt6/qml"
            -I "$ENV{HOME}/.local/lib/qt6/qml"
            ${ALL_QML_FILES}
        DEPENDS
            holonight-shell_qmltyperegistration
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running qmllint"
        VERBATIM
    )
else()
  add_custom_target(qml-lint
        COMMAND ${CMAKE_COMMAND} -E echo "qmllint was not found. Install qt6-declarative-dev-tools."
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
endif(QMLLINT)
