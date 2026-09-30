set_source_files_properties(BarFrame.qml PROPERTIES QT_RESOURCE_ALIAS BarFrame.qml)
qt6_add_qml_module(holonight_presentation STATIC
  URI HolonightShell.Presentation VERSION 1.0
  RESOURCE_PREFIX /
  OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/HolonightShell/Presentation
  QML_FILES BarFrame.qml)
target_link_libraries(holonight_presentation PUBLIC Qt6::Quick)

# Pure QML types remain embedded; install discovery metadata for relocated engines.
install(FILES
  ${PROJECT_BINARY_DIR}/HolonightShell/Presentation/qmldir
  ${PROJECT_BINARY_DIR}/HolonightShell/Presentation/holonight_presentation.qmltypes
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/qt6/qml/HolonightShell/Presentation)
