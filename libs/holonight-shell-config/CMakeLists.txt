if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(HoloNightShellConfig VERSION 0.1.0 LANGUAGES CXX)

    include(GNUInstallDirs)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(TOMLPLUSPLUS REQUIRED IMPORTED_TARGET tomlplusplus)
endif()

add_library(holonight_shell_config STATIC
    src/ConfigParsers.cpp
    src/ConfigWriter.cpp
)

add_library(HoloNightShellConfig::Config ALIAS holonight_shell_config)
set_target_properties(holonight_shell_config PROPERTIES EXPORT_NAME Config)

target_include_directories(holonight_shell_config
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(holonight_shell_config PUBLIC
    Qt6::Core
    PkgConfig::TOMLPLUSPLUS
)
target_compile_features(holonight_shell_config PUBLIC cxx_std_23)

include(CMakePackageConfigHelpers)

install(TARGETS holonight_shell_config
    EXPORT HoloNightShellConfigTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT HoloNightShellConfigTargets
    FILE HoloNightShellConfigTargets.cmake
    NAMESPACE HoloNightShellConfig::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HoloNightShellConfig
)
configure_package_config_file(
    cmake/HoloNightShellConfigConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/HoloNightShellConfigConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HoloNightShellConfig
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/HoloNightShellConfigConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/HoloNightShellConfigConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/HoloNightShellConfigConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HoloNightShellConfig
)
