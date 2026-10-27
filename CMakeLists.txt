cmake_minimum_required(VERSION 3.25)
project(holonight-shell VERSION 0.2.0 LANGUAGES C CXX)

# Installed QML providers follow the standard CMake prefix search path.
set(_qml_prefixes ${CMAKE_PREFIX_PATH})
if(DEFINED ENV{CMAKE_PREFIX_PATH})
  string(REPLACE ":" ";" _prefix_environment "$ENV{CMAKE_PREFIX_PATH}")
  list(APPEND _qml_prefixes ${_prefix_environment})
endif()
foreach(_prefix IN LISTS _qml_prefixes)
  if(IS_DIRECTORY "${_prefix}/lib/qt6/qml")
    list(APPEND QML_IMPORT_PATH "${_prefix}/lib/qt6/qml")
  endif()
endforeach()
if(DEFINED ENV{QML_IMPORT_PATH})
  string(REPLACE ":" ";" _qml_environment "$ENV{QML_IMPORT_PATH}")
  list(APPEND QML_IMPORT_PATH ${_qml_environment})
endif()
list(REMOVE_DUPLICATES QML_IMPORT_PATH)
set(HOLONIGHT_POLKIT_CONFLICT_EXIT_STATUS 78 CACHE STRING "Non-retriable Polkit registration conflict status")
configure_file(cmake/AuthenticationConfig.h.in ${CMAKE_BINARY_DIR}/generated/AuthenticationConfig.h @ONLY)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)
option(BUILD_TESTS "Build unit tests using installed prerequisites" OFF)
option(ENABLE_COVERAGE "Instrument for gcov/gcovr coverage reporting" OFF)

install(
  FILES LICENSE THIRD_PARTY_NOTICES.md
        LICENSES/GPL-3.0-or-later.txt
        LICENSES/MIT.txt
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME}"
)

# The shell loads QML modules dynamically from the runtime Qt installation. Some
# distro packages provide QML runtime plugins without matching CMake plugin
# targets, so avoid auto-linking QML plugins during executable finalization.
set(QT_SKIP_AUTO_QML_PLUGIN_INCLUSION ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Quick Qml DBus Network Concurrent Sql QuickControls2 WaylandClient)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(HolonightQt REQUIRED COMPONENTS Wayland)
find_package(HoloNightSystemServices REQUIRED COMPONENTS Audio Storage Bluetooth Compositor)
include(cmake/QuickControlsRuntime.cmake)
find_package(Qt6 ${Qt6_VERSION} EXACT REQUIRED COMPONENTS WaylandClientPrivate)
find_package(Qt6WaylandScannerTools REQUIRED)
if(NOT COMMAND qt6_generate_wayland_protocol_client_sources)
  include("${Qt6WaylandScannerTools_DIR}/Qt6WaylandClientMacros.cmake" OPTIONAL)
endif()
find_package(WaylandScanner REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QT6_WAYLAND_CLIENT REQUIRED Qt6WaylandClient)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_check_modules(TOMLPLUSPLUS REQUIRED IMPORTED_TARGET tomlplusplus)
pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
pkg_check_modules(POLKIT_AGENT REQUIRED IMPORTED_TARGET polkit-agent-1)
pkg_check_modules(POLKIT_QT_AGENT REQUIRED IMPORTED_TARGET polkit-qt6-agent-1>=0.200.0)
pkg_check_modules(POLKIT_QT_CORE REQUIRED IMPORTED_TARGET polkit-qt6-core-1>=0.200.0)

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

add_library(holonight_qt_wayland_client INTERFACE)
target_link_libraries(holonight_qt_wayland_client INTERFACE Qt6::WaylandClient)

# Preserve spaces in the pkg-config data directory.
execute_process(COMMAND "${PKG_CONFIG_EXECUTABLE}" --variable=pkgdatadir wayland-protocols
  OUTPUT_VARIABLE WAYLAND_PROTOCOLS_PKGDATADIR OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
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
add_subdirectory(libs/holonight-integration)
add_subdirectory(libs/holonight-compositor)
add_subdirectory(qml/Presentation)
add_subdirectory(integrations)
add_subdirectory(libs/holonight-shell-config)
add_subdirectory(libs/holonight-core)
add_subdirectory(libs/holonight-services)
add_subdirectory(libs/holonight-surfaces)
add_subdirectory(libs/holonight-authentication)
add_subdirectory(qml/HoloNight)
add_subdirectory(qml/Authentication)
add_subdirectory(apps/shell)
add_subdirectory(apps/authentication)

set(HOLONIGHT_INSTALL_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
configure_file(cmake/InstallIntegration.cmake.in ${PROJECT_BINARY_DIR}/InstallIntegration.cmake @ONLY)
install(SCRIPT ${PROJECT_BINARY_DIR}/InstallIntegration.cmake)

# ============================================================
# Tests
# ============================================================
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

  find_package(Qt6 6.11 REQUIRED COMPONENTS Test QuickTest)

  find_package(GTest 1.14 CONFIG REQUIRED)
  foreach(target GTest::gtest GTest::gmock)
    if(NOT TARGET ${target})
      message(FATAL_ERROR "Installed GTest must provide ${target}; install GTest >=1.14 with GMock. No sources are downloaded.")
    endif()
  endforeach()

  if(ENABLE_COVERAGE)
    find_program(GCOVR gcovr REQUIRED)
    add_custom_target(clean-coverage-counters
      COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcda" -delete
      COMMENT "Cleaning stale gcov counters"
      VERBATIM
    )
    foreach(lib IN ITEMS holonight_platform holonight_shell_config holonight_authentication_core
                         holonight_core holonight_compositor holonight_services holonight_surfaces holonight_app)
      target_compile_options(${lib} PRIVATE --coverage)
      target_link_options(${lib} PRIVATE --coverage)
    endforeach()
    foreach(target IN ITEMS holonight-shell holonight-polkit-agent holonight-askpass-bin
                            holonight-askpass-test holonight_authentication_qml holonight_components)
      target_compile_options(${target} PRIVATE --coverage)
      target_link_options(${target} PRIVATE --coverage)
    endforeach()
  endif()

  add_subdirectory(tests)
endif()

# ============================================================
# Format / Tidy / qmllint targets
# ============================================================
find_program(CLANG_FORMAT clang-format)

if(NOT QMLLINT)
  get_target_property(QMLLINT Qt6::qmllint IMPORTED_LOCATION)
endif()

file(GLOB_RECURSE ALL_SOURCE_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    apps/*.cpp apps/*.h libs/*.cpp libs/*.h integrations/*.cpp integrations/*.h tests/*.cpp tests/*.h
)

# The installed-package consumer has a separate compiler database and CI tidy check.

file(GLOB_RECURSE ALL_QML_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    apps/shell/qml/*.qml
    qml/HoloNight/*.qml
    qml/Authentication/*.qml
    qml/Presentation/*.qml
    integrations/*/qml/*.qml
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

# Use the same direct compilation-database check as task tidy.
add_custom_target(tidy
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/scripts/quality.py" tidy --build "${CMAKE_BINARY_DIR}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  VERBATIM
)

if(QMLLINT)
  cmake_path(ABSOLUTE_PATH QT6_INSTALL_QML BASE_DIRECTORY "${QT6_INSTALL_PREFIX}"
             OUTPUT_VARIABLE HOLONIGHT_QT_QML_DIR)
  add_custom_target(qml-lint
        COMMAND ${QMLLINT}
            --bare
            --unqualified disable
            --unused-imports disable
            -I "${HOLONIGHT_RUNTIME_QML_PATH}"
            -I "${CMAKE_CURRENT_SOURCE_DIR}/tests/qmllint"
            -I "${CMAKE_CURRENT_BINARY_DIR}/apps/shell"
            -I "${CMAKE_BINARY_DIR}"
            -I "${HOLONIGHT_QT_QML_DIR}"
            ${ALL_QML_FILES}
        DEPENDS
            holonight-shell_qmltyperegistration
            holonight_authentication_qml_qmltyperegistration
            holonight_presentation_qmltyperegistration
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
