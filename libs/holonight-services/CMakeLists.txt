file(GLOB_RECURSE HOLONIGHT_SERVICE_SOURCES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    src/*.h
    src/*.cpp
)

add_library(holonight_services STATIC ${HOLONIGHT_SERVICE_SOURCES})

target_link_libraries(holonight_services PUBLIC
    holonight_core
    holonight_platform
    HoloNight::Config
    HolonightQt::Appearance
    HolonightQt::Theme
    HoloNightSystem::Audio
    Qt6::Core
    Qt6::Qml
    Qt6::DBus
    Qt6::Network
    Qt6::Concurrent
    Qt6::Sql
    ${LIBSECRET_LIBRARIES}
)

target_include_directories(holonight_services PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/calendar
    ${CMAKE_CURRENT_SOURCE_DIR}/src/brightness
    ${CMAKE_CURRENT_SOURCE_DIR}/src/idle
    ${CMAKE_CURRENT_SOURCE_DIR}/src/portal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mime
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mpris
    ${CMAKE_CURRENT_SOURCE_DIR}/src/session-integration
    ${CMAKE_CURRENT_SOURCE_DIR}/src/launcher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/session
    ${CMAKE_CURRENT_SOURCE_DIR}/src/weather
    ${CMAKE_CURRENT_SOURCE_DIR}/src/weather-icon
    ${CMAKE_CURRENT_SOURCE_DIR}/src/notifications
    ${CMAKE_CURRENT_SOURCE_DIR}/src/osd
    ${LIBSECRET_INCLUDE_DIRS}
)

target_compile_features(holonight_services PUBLIC cxx_std_23)
