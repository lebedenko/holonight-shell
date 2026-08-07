cmake_minimum_required(VERSION 3.25)
project(holonight-shell-config-consumer LANGUAGES CXX)

find_package(HoloNightShellConfig CONFIG REQUIRED)

add_executable(holonight-shell-config-consumer main.cpp)
target_link_libraries(holonight-shell-config-consumer PRIVATE HoloNightShellConfig::Config)
target_compile_features(holonight-shell-config-consumer PRIVATE cxx_std_23)
