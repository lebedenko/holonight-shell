function(holonight_add_askpass target)
  add_executable(${target} askpass/main.cpp askpass/AskpassMode.cpp)
  target_link_libraries(${target} PRIVATE
    holonight_authentication_core "$<LINK_LIBRARY:WHOLE_ARCHIVE,holonight_authentication_qml>"
    holonight_authentication_qmlplugin Qt6::Gui Qt6::Qml Qt6::Quick
  )
endfunction()

holonight_add_askpass(holonight-askpass-bin)
set_target_properties(holonight-askpass-bin PROPERTIES OUTPUT_NAME holonight-askpass)
if(BUILD_TESTS)
  # The automation channel must never be part of an installed executable.
  holonight_add_askpass(holonight-askpass-test)
  target_compile_definitions(holonight-askpass-test PRIVATE HOLONIGHT_ASKPASS_TEST_CONTROL)
endif()
install(TARGETS holonight-askpass-bin DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
add_executable(holonight-polkit-agent
  polkit/main.cpp
  polkit/AccountProfileResolver.cpp
  polkit/PolkitListenerBridge.cpp
  polkit/PolkitSessionAdapter.cpp
)
target_include_directories(holonight-polkit-agent PRIVATE ${CMAKE_BINARY_DIR})
target_compile_definitions(holonight-polkit-agent PRIVATE POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE)
target_link_libraries(holonight-polkit-agent PRIVATE
  holonight_authentication_core "$<LINK_LIBRARY:WHOLE_ARCHIVE,holonight_authentication_qml>" holonight_authentication_qmlplugin
  PkgConfig::POLKIT_AGENT PkgConfig::POLKIT_QT_AGENT PkgConfig::POLKIT_QT_CORE Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick
)
install(TARGETS holonight-polkit-agent DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
