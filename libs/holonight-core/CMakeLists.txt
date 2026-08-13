add_library(holonight_core STATIC
    src/ConfigService.h
    src/ConfigService.cpp
    src/KeyboardLayoutService.h
    src/KeyboardLayoutService.cpp
    src/BatteryState.h
    src/BatteryState.cpp
    src/AudioState.h
    src/AudioState.cpp
    src/SystemInfo.h
    src/SystemInfo.cpp
    src/Logger.h
    src/Logger.cpp
)

target_link_libraries(holonight_core PUBLIC
    HoloNightShellConfig::Config
    holonight_platform
    Qt6::Core
    Qt6::Qml
    Qt6::DBus
)

target_include_directories(holonight_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(holonight_core PUBLIC cxx_std_23)
