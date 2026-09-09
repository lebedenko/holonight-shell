include(GoogleTest)
add_test(NAME test_installation
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_installation.py
)
add_test(NAME test_uninstall
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_uninstall.py
)
add_test(NAME test_wayland_session_environment
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_wayland_session_environment.py
)

add_test(NAME test_session_scripts
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_session_scripts.sh ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_polkit_session_wrapper
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_polkit_session_wrapper.sh ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_authentication_install
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_authentication_install.sh ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)

# Helper: define a test executable with common settings.
function(holonight_add_test_exe name)
  add_executable(${name} ${ARGN})
  holonight_quick_controls_runtime(${name} "${CMAKE_INSTALL_BINDIR}")
  set_target_properties(${name} PROPERTIES AUTOMOC ON)
  target_compile_features(${name} PRIVATE cxx_std_23)
  target_include_directories(${name} PRIVATE ${CMAKE_BINARY_DIR})
  target_compile_definitions(${name} PRIVATE
    TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    HOLONIGHT_TESTS
  )
  gtest_discover_tests(${name}
    DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${HOLONIGHT_RUNTIME_QML_PATH};QML2_IMPORT_PATH=${HOLONIGHT_RUNTIME_QML_PATH}"
  )
endfunction()

holonight_add_test_exe(test_holonight_authentication
  main.cpp
  test_authentication_core.cpp
  test_account_profile_resolver.cpp
  ${PROJECT_SOURCE_DIR}/apps/authentication/polkit/AccountProfileResolver.cpp
  test_askpass_process.cpp
  test_polkit_bridge_integration.cpp
  test_polkit_agent_process.cpp
  ${PROJECT_SOURCE_DIR}/apps/authentication/askpass/AskpassMode.cpp
  ${PROJECT_SOURCE_DIR}/apps/authentication/polkit/PolkitListenerBridge.cpp
)
target_include_directories(test_holonight_authentication PRIVATE
  ${PROJECT_SOURCE_DIR}/apps/authentication/askpass
  ${PROJECT_SOURCE_DIR}/apps/authentication/polkit
)
target_compile_definitions(test_holonight_authentication PRIVATE
  TEST_ASKPASS_PATH="$<TARGET_FILE:holonight-askpass-test>"
  TEST_PRODUCTION_ASKPASS_PATH="$<TARGET_FILE:holonight-askpass-bin>"
  TEST_POLKIT_AGENT_PATH="$<TARGET_FILE:holonight-polkit-agent>"
  POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
)
add_dependencies(test_holonight_authentication holonight-askpass-test holonight-askpass-bin holonight-polkit-agent)
target_link_libraries(test_holonight_authentication PRIVATE
  holonight_authentication_core PkgConfig::POLKIT_AGENT GTest::gtest GTest::gmock Qt6::Gui Qt6::Test Qt6::DBus
)

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
  Qt6::Gui
  Qt6::Test
)

# --- core tests (links holonight_core) ---
holonight_add_test_exe(test_holonight_core
  main.cpp
  test_config_service.cpp
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

holonight_add_test_exe(test_holonight_compositor
  main.cpp
  test_compositor.cpp
  test_window_activation.cpp
  test_hyprland_backend.cpp
  test_sway_backend.cpp
  test_sway_ipc.cpp
)
target_link_libraries(test_holonight_compositor PRIVATE
  holonight_compositor
  GTest::gtest
  GTest::gmock
  Qt6::Gui
  Qt6::Test
)

holonight_add_test_exe(test_holonight_shell_config
  main.cpp
  test_shell_config_package.cpp
)
target_link_libraries(test_holonight_shell_config PRIVATE
  HoloNightShellConfig::Config
  GTest::gtest
  GTest::gmock
  Qt6::Gui
  Qt6::Test
)

# --- services tests (links holonight_services) ---
holonight_add_test_exe(test_holonight_services
  main.cpp
  test_ai_chat_service.cpp
  test_integration_theme_config.cpp
  test_launcher_service.cpp
  test_battery_service.cpp
  test_calendar_integration.cpp
  test_http_sync_client.cpp
  test_network_service.cpp
  test_power_profiles_service.cpp
  test_session_service.cpp
  test_session_integration_service.cpp
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
  test_window_activation_server.cpp
  test_suspend_inhibitor_service.cpp
  test_logind_session_resolver.cpp
  test_mpris_metadata.cpp
  test_mpris_selection.cpp
  test_mpris_service.cpp
  test_mpris_artwork_cache.cpp
)
target_link_libraries(test_holonight_services PRIVATE
  holonight_services
  HolonightQt::Appearance
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
    test_persistent_surface_policy.cpp
    test_transient_surface_policy.cpp
    test_paired_transient_surface.cpp
    test_paired_surface_policy.cpp
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
  test_settings_navigation_service.cpp
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
  "$<LINK_LIBRARY:WHOLE_ARCHIVE,holonight_authentication_qml>"
  holonight_authentication_qmlplugin
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
holonight_quick_controls_runtime(test_holonight_qml_harness "${CMAKE_INSTALL_BINDIR}")
add_test(NAME test_holonight_qml_harness COMMAND test_holonight_qml_harness)
set_tests_properties(test_holonight_qml_harness PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${HOLONIGHT_RUNTIME_QML_PATH};QML2_IMPORT_PATH=${HOLONIGHT_RUNTIME_QML_PATH}:${CMAKE_BINARY_DIR}"
)

# --- coverage instrumentation and report target ---
if(ENABLE_COVERAGE)
  foreach(test_exe IN ITEMS
      test_holonight_platform
      test_holonight_core
      test_holonight_compositor
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
    COMMAND $<TARGET_FILE:test_holonight_compositor>
    COMMAND $<TARGET_FILE:test_holonight_services>
    COMMAND $<TARGET_FILE:test_holonight_surfaces>
    COMMAND $<TARGET_FILE:test_holonight_app>
    COMMAND $<TARGET_FILE:test_holonight_qml>
    COMMAND $<TARGET_FILE:test_holonight_qml_harness>
    COMMAND ${GCOVR}
        --root ${CMAKE_SOURCE_DIR}
        --filter "${CMAKE_SOURCE_DIR}/apps/shell/"
        --filter "${CMAKE_SOURCE_DIR}/libs/holonight-core/"
        --filter "${CMAKE_SOURCE_DIR}/libs/holonight-compositor/"
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
      test_holonight_compositor
      test_holonight_services
      test_holonight_surfaces
      test_holonight_app
      test_holonight_qml
      test_holonight_qml_harness
      holonight-shell
    COMMENT "Collecting coverage → build/coverage/index.html"
  )
endif()

add_test(NAME uqc_policy COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/check-qml-import-policy.py)
add_test(NAME uqc_policy_fixtures COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/test_qml_import_policy.py)

add_executable(test_holonight_compiled_controls test_qml_harness.cpp FakeQmlServices.h)
target_include_directories(test_holonight_compiled_controls PRIVATE ${CMAKE_BINARY_DIR})
target_compile_definitions(test_holonight_compiled_controls PRIVATE
  TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/uqc_qml"
  HOLONIGHT_TESTS HOLONIGHT_COMPILED_ACCEPTANCE)
target_link_libraries(test_holonight_compiled_controls PRIVATE holonight_app holonight_componentsplugin
  "$<LINK_LIBRARY:WHOLE_ARCHIVE,holonight_authentication_qml>" holonight_authentication_qmlplugin Qt6::QuickTest)
holonight_quick_controls_runtime(test_holonight_compiled_controls "${CMAKE_INSTALL_BINDIR}")
holonight_shell_qml(test_holonight_compiled_controls)
foreach(style IN ITEMS Holonight Fusion)
  foreach(scale IN ITEMS 1 1.25)
    add_test(NAME uqc_compiled_${style}_${scale} COMMAND test_holonight_compiled_controls)
    set_tests_properties(uqc_compiled_${style}_${scale} PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=${style};QT_SCALE_FACTOR=${scale};QT_QUICK_BACKEND=software")
  endforeach()
  add_test(NAME uqc_source_${style} COMMAND test_holonight_qml_harness)
  set_tests_properties(uqc_source_${style} PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=${style};QT_QUICK_BACKEND=software")
endforeach()

foreach(style IN ITEMS Holonight Fusion)
  foreach(scale IN ITEMS 1 1.25)
    add_test(NAME uqc_authentication_${style}_${scale} COMMAND test_holonight_compiled_controls
      -input ${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_AuthenticationDialog.qml)
    set_tests_properties(uqc_authentication_${style}_${scale} PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=${style};QT_SCALE_FACTOR=${scale};QT_QUICK_BACKEND=software")
  endforeach()
endforeach()

foreach(mode IN ITEMS default environment cli precedence)
  set(runtime_style "")
  set(runtime_args "")
  set(expected_style Fusion)
  if(mode STREQUAL "default")
    set(expected_style Holonight)
  elseif(mode STREQUAL "environment")
    set(runtime_style Fusion)
  else()
    set(runtime_args -style Fusion)
    if(mode STREQUAL "precedence")
      set(runtime_style Holonight)
    endif()
  endif()
  add_test(NAME uqc_runtime_${mode} COMMAND test_holonight_compiled_controls ${runtime_args})
  set_tests_properties(uqc_runtime_${mode} PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=${runtime_style};QT_QUICK_CONTROLS_CONF=;UQC_EXPECTED_STYLE=${expected_style}")
endforeach()

add_test(NAME uqc_launch COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/run-isolated-test.py
  --hide-host ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/test_quick_controls_launch.py
  --build ${PROJECT_BINARY_DIR} --prefix ${HOLONIGHT_PROVIDER_PREFIX}
  --logs ${PROJECT_BINARY_DIR}/uqc-launch-logs)
set_tests_properties(uqc_launch PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
