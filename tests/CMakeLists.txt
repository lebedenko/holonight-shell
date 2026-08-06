include(GoogleTest)

# Helper: define a test executable with common settings.
function(holonight_add_test_exe name)
  add_executable(${name} ${ARGN})
  set_target_properties(${name} PROPERTIES AUTOMOC ON)
  target_compile_features(${name} PRIVATE cxx_std_23)
  target_include_directories(${name} PRIVATE ${CMAKE_BINARY_DIR})
  target_compile_definitions(${name} PRIVATE
    TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    HOLONIGHT_TESTS
  )
  gtest_discover_tests(${name}
    DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=/tmp/holonight-qt-prefix/lib/qt6/qml;QML2_IMPORT_PATH=/tmp/holonight-qt-prefix/lib/qt6/qml"
  )
endfunction()

# --- platform tests (links holonight_platform) ---
holonight_add_test_exe(test_holonight_platform
  main.cpp
  test_hyprland_ipc.cpp
  test_hyprland_ipc_client.cpp
  test_dbus_property_client.cpp
)
target_link_libraries(test_holonight_platform PRIVATE
  holonight_platform
  GTest::gtest
  GTest::gmock
  Qt6::Test
)

# --- core tests (links holonight_core) ---
holonight_add_test_exe(test_holonight_core
  main.cpp
  test_config_service.cpp
  test_integration_workspace_config.cpp
  test_workspace_model.cpp
  test_hyprland_workspace_service.cpp
  test_ext_workspace_manager.cpp
  test_keyboard_layout_service.cpp
  test_audio_state.cpp
  test_battery_state.cpp
  test_system_info.cpp
)
target_link_libraries(test_holonight_core PRIVATE
  holonight_core
  GTest::gtest
  GTest::gmock
  Qt6::Test
)

# --- services tests (links holonight_services) ---
holonight_add_test_exe(test_holonight_services
  main.cpp
  test_ai_chat_service.cpp
  test_active_window_service.cpp
  test_integration_theme_config.cpp
  test_launcher_service.cpp
  test_audio_device_model.cpp
  test_audio_service.cpp
  test_audio_stream_model.cpp
  test_battery_service.cpp
  test_calendar_integration.cpp
  test_http_sync_client.cpp
  test_monitor_occupancy_service.cpp
  test_network_service.cpp
  test_power_profiles_service.cpp
  test_session_service.cpp
  test_session_integration_service.cpp
  test_pulse_audio_backend.cpp
  test_notification_service.cpp
  test_notification_payload_bounds.cpp
  test_notification_filter.cpp
  test_notification_history.cpp
  test_notification_policy.cpp
  test_notification_rules.cpp
  test_notification_timeout.cpp
  test_osd_controller.cpp
  test_weather_service.cpp
  test_weather_icon_mapper.cpp
  test_moon_phase_calculator.cpp
  test_idle_service.cpp
  test_brightness_service.cpp
  test_sysfs_value_reader.cpp
  test_portal_service.cpp
  test_mime_service_file_watch.cpp
  test_guarded_process_runner.cpp
  test_process_mime_resolver.cpp
  test_system_info_service.cpp
  test_libsecret_credential_storage.cpp
  test_lid_state_monitor.cpp
  test_low_battery_monitor.cpp
  test_suspend_inhibitor_service.cpp
  test_theme_config_path.cpp
  test_logind_session_resolver.cpp
  test_mpris_metadata.cpp
  test_mpris_selection.cpp
  test_mpris_service.cpp
  test_mpris_artwork_cache.cpp
)
target_link_libraries(test_holonight_services PRIVATE
  holonight_services
  HolonightQt::Config
  HolonightQt::Theme
  GTest::gtest
  GTest::gmock
  Qt6::Test
)

# --- surfaces tests (links holonight_surfaces) ---
holonight_add_test_exe(test_holonight_surfaces
  main.cpp
  test_launcher_surface_lifecycle.cpp
  test_sidebar_surface_policy.cpp
  test_sidebar_manager.cpp
  test_widget_surface_policy.cpp
  test_widget_countdown.cpp
  test_widget_clock.cpp
  test_mpris_widget_manager.cpp
  test_status_popup_geometry.cpp
  test_tray_item.cpp
  test_tray_item_properties.cpp
  test_tray_pixmap_validation.cpp
  test_tray_model.cpp
  test_integration_tray_config.cpp
  test_tray_watcher.cpp
  test_dbusmenu_client.cpp
)
target_link_libraries(test_holonight_surfaces PRIVATE
  holonight_surfaces
  GTest::gtest
  GTest::gmock
  Qt6::Test
)

# --- app tests (links holonight_app) ---
holonight_add_test_exe(test_holonight_app
  main.cpp
  test_control_server.cpp
  test_shell_application.cpp
)
target_link_libraries(test_holonight_app PRIVATE
  holonight_app
  GTest::gtest
  GTest::gmock
  Qt6::Test
)

# --- qml smoke test (needs full app type set via holonight_app) ---
holonight_add_test_exe(test_holonight_qml
  main.cpp
  test_media_artwork_assets.cpp
  test_qml_smoke.cpp
  FakeQmlServices.h
)
target_link_libraries(test_holonight_qml PRIVATE
  holonight_app
  GTest::gtest
  GTest::gmock
  Qt6::Test
)

# --- qml unit tests harness (uses QtQuickTest/TestCase) ---
add_executable(test_holonight_qml_harness
  test_qml_harness.cpp
  FakeQmlServices.h
)
set_target_properties(test_holonight_qml_harness PROPERTIES AUTOMOC ON)
target_compile_features(test_holonight_qml_harness PRIVATE cxx_std_23)
target_include_directories(test_holonight_qml_harness PRIVATE ${CMAKE_BINARY_DIR})
target_compile_definitions(test_holonight_qml_harness PRIVATE
  TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
  HOLONIGHT_TESTS
)
target_link_libraries(test_holonight_qml_harness PRIVATE
  holonight_app
  holonight_componentsplugin
  Qt6::QuickTest
  Qt6::Qml
)
qt6_add_resources(test_holonight_qml_harness "test_media_assets"
  PREFIX "/HolonightShell"
  BASE "${PROJECT_SOURCE_DIR}/assets"
  FILES
    "${PROJECT_SOURCE_DIR}/assets/media/artwork-fallback.svg"
    "${PROJECT_SOURCE_DIR}/assets/media/artwork-frame.svg"
    "${PROJECT_SOURCE_DIR}/assets/media/now-playing-glyph.svg"
)
add_test(NAME test_holonight_qml_harness COMMAND test_holonight_qml_harness)
set_tests_properties(test_holonight_qml_harness PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=/tmp/holonight-qt-prefix/lib/qt6/qml;QML2_IMPORT_PATH=/tmp/holonight-qt-prefix/lib/qt6/qml:${CMAKE_BINARY_DIR}"
)

# --- coverage instrumentation and report target ---
if(ENABLE_COVERAGE)
  foreach(test_exe IN ITEMS
      test_holonight_platform
      test_holonight_core
      test_holonight_services
      test_holonight_surfaces
      test_holonight_app
      test_holonight_qml
      test_holonight_qml_harness)
    target_compile_options(${test_exe} PRIVATE --coverage)
    target_link_options(${test_exe} PRIVATE --coverage)
  endforeach()

  add_custom_target(coverage
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcda" -delete
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/coverage
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
    COMMAND $<TARGET_FILE:test_holonight_platform>
    COMMAND $<TARGET_FILE:test_holonight_core>
    COMMAND $<TARGET_FILE:test_holonight_services>
    COMMAND $<TARGET_FILE:test_holonight_surfaces>
    COMMAND $<TARGET_FILE:test_holonight_app>
    COMMAND $<TARGET_FILE:test_holonight_qml>
    COMMAND $<TARGET_FILE:test_holonight_qml_harness>
    COMMAND ${GCOVR}
        --root ${CMAKE_SOURCE_DIR}
        --filter "${CMAKE_SOURCE_DIR}/apps/shell/"
        --filter "${CMAKE_SOURCE_DIR}/libs/holonight-core/"
        --filter "${CMAKE_SOURCE_DIR}/libs/holonight-platform/"
        --filter "${CMAKE_SOURCE_DIR}/libs/holonight-services/"
        --filter "${CMAKE_SOURCE_DIR}/libs/holonight-surfaces/"
        --exclude "${CMAKE_SOURCE_DIR}/apps/shell/main\\.cpp"
        --html-details ${CMAKE_BINARY_DIR}/coverage/index.html
        --exclude-unreachable-branches
        --fail-under-line 45
        --fail-under-branch 30
        --print-summary
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS
      test_holonight_platform
      test_holonight_core
      test_holonight_services
      test_holonight_surfaces
      test_holonight_app
      test_holonight_qml
      test_holonight_qml_harness
      holonight-shell
    COMMENT "Collecting coverage → build/coverage/index.html"
  )
endif()
