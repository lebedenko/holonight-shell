qt6_add_library(holonight_authentication_qml STATIC)
set(AUTHENTICATION_QML_FILES AuthenticationDialog.qml AuthenticationIcon.qml AuthenticationPrompt.qml IdentitySelector.qml MessageList.qml)
foreach(qml_file IN LISTS AUTHENTICATION_QML_FILES)
  set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_file}")
endforeach()
qt6_add_qml_module(holonight_authentication_qml
  URI Holonight.Authentication
  VERSION 1.0
  RESOURCE_PREFIX "/qt-project.org/imports"
  OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Holonight/Authentication"
  SOURCES AuthenticationTypes.h
  QML_FILES ${AUTHENTICATION_QML_FILES}
)
target_link_libraries(holonight_authentication_qml PUBLIC Qt6::Quick Qt6::Qml holonight_authentication_core)
install(TARGETS holonight_authentication_qml holonight_authentication_qmlplugin
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/qt6/qml/Holonight/Authentication"
)
install(DIRECTORY "${CMAKE_BINARY_DIR}/Holonight/Authentication/"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/qt6/qml/Holonight/Authentication"
  FILES_MATCHING PATTERN "qmldir" PATTERN "*.qmltypes"
)
